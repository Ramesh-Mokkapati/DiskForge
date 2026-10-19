// FatFileSystem.cpp
#include "pch.h"
#include "FatFileSystem.h"

namespace
{
    BYTE ShortNameChecksum(const BYTE* name11)
    {
        BYTE sum = 0;
        for (int k = 0; k < 11; ++k)
            sum = (BYTE)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + name11[k]);
        return sum;
    }

    FILETIME FatTimeToFileTime(WORD date, WORD time)
    {
        FILETIME local = {}, utc = {};
        if (date == 0 || !DosDateTimeToFileTime(date, time, &local))
            return utc;
        LocalFileTimeToFileTime(&local, &utc); // FAT stores local time
        return utc;
    }

    // Appends the 13 UTF-16 characters held by one LFN directory entry.
    void LfnChars(const BYTE* e, WCHAR out[13])
    {
        static const int pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
        for (int k = 0; k < 13; ++k)
            out[k] = (WCHAR)ReadLE16(e + pos[k]);
    }

    CString TrimLfn(const std::wstring& s)
    {
        size_t len = 0;
        while (len < s.size() && s[len] != 0 && s[len] != 0xFFFF)
            ++len;
        return CString(s.c_str(), (int)len);
    }

    CString ShortName(const BYTE* e, BYTE firstChar)
    {
        char base[9] = {}, ext[4] = {};
        memcpy(base, e, 8);
        memcpy(ext, e + 8, 3);
        base[0] = (char)firstChar;
        if ((BYTE)base[0] == 0x05)
            base[0] = (char)0xE5; // 0x05 stands in for a real 0xE5 first byte
        CString b(base), x(ext);
        b.TrimRight(_T(' '));
        x.TrimRight(_T(' '));
        // Windows stores lower-case flags in byte 12 (NT reserved).
        if (e[12] & 0x08) b.MakeLower();
        if (e[12] & 0x10) x.MakeLower();
        return x.IsEmpty() ? b : b + _T(".") + x;
    }
}

bool CFatFileSystem::Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors,
                          CString& errorOut)
{
    if (!IFileSystem::Init(devicePath, volumeStartLBA, volumeSectors, errorOut))
        return false;
    if (!m_dm.ReadSector(volumeStartLBA, m_boot) || m_boot.size() < 512)
    {
        errorOut = _T("Could not read the boot record.");
        return false;
    }

    CString id = Identify(m_boot);
    m_kind = id == _T("FAT12") ? FsKind::FAT12 : id == _T("FAT16") ? FsKind::FAT16 :
             id == _T("FAT32") ? FsKind::FAT32 : FsKind::Unknown;
    if (m_kind == FsKind::Unknown)
    {
        errorOut = _T("The boot record is not a valid FAT BIOS Parameter Block.");
        return false;
    }

    const BYTE* s = m_boot.data();
    m_bps         = ReadLE16(s + 11);
    m_spc         = s[13];
    m_reserved    = ReadLE16(s + 14);
    m_numFats     = s[16];
    m_rootEntries = ReadLE16(s + 17);
    m_totalSectors = ReadLE16(s + 19) ? ReadLE16(s + 19) : ReadLE32(s + 32);
    m_fatSectors  = ReadLE16(s + 22) ? ReadLE16(s + 22) : ReadLE32(s + 36);

    if (m_bps != SectorSize())
    {
        errorOut.Format(_T("The volume uses %u-byte sectors but the disk reports %u-byte sectors; ")
                        _T("this layout is not supported."), m_bps, SectorSize());
        return false;
    }

    m_rootDirSectors = (m_rootEntries * 32 + (m_bps - 1)) / m_bps;
    m_fatStartRel    = m_reserved;
    m_rootDirRel     = m_fatStartRel + (ULONGLONG)m_numFats * m_fatSectors;
    m_dataStartRel   = m_rootDirRel + m_rootDirSectors;
    m_clusterCount   = (DWORD)((m_totalSectors - m_dataStartRel) / m_spc);
    m_rootCluster    = (m_kind == FsKind::FAT32) ? ReadLE32(s + 44) : 0;
    m_clusterBytes   = (ULONGLONG)m_spc * m_bps;

    if (m_volumeSectors == 0 || m_volumeSectors > m_totalSectors)
        m_volumeSectors = m_totalSectors;
    return true;
}

void CFatFileSystem::BootRecordFields(std::vector<FieldRow>& out) const
{
    const BYTE* s = m_boot.data();
    CString v;
    LPCTSTR g = _T("Boot Sector");

    v.Format(_T("%02X %02X %02X"), s[0], s[1], s[2]);
    AddField(out, g, _T("Jump instruction"), v, 0x00);
    char oem[9] = {}; memcpy(oem, s + 3, 8);
    AddField(out, g, _T("OEM name"), CString(oem), 0x03);

    g = _T("BIOS Parameter Block");
    v.Format(_T("%u"), m_bps);               AddField(out, g, _T("Bytes per sector"), v, 0x0B);
    v.Format(_T("%u (%s per cluster)"), m_spc, FormatSize(m_clusterBytes).GetString());
                                             AddField(out, g, _T("Sectors per cluster"), v, 0x0D);
    v.Format(_T("%u"), m_reserved);          AddField(out, g, _T("Reserved sectors"), v, 0x0E);
    v.Format(_T("%u"), m_numFats);           AddField(out, g, _T("Number of FATs"), v, 0x10);
    v.Format(_T("%u"), m_rootEntries);       AddField(out, g, _T("Root directory entries"), v, 0x11,
                                                      _T("0 on FAT32, whose root is an ordinary cluster chain"));
    v.Format(_T("%u"), ReadLE16(s + 19));    AddField(out, g, _T("Total sectors (16-bit)"), v, 0x13);
    v.Format(_T("0x%02X"), s[21]);           AddField(out, g, _T("Media descriptor"), v, 0x15);
    v.Format(_T("%u"), ReadLE16(s + 22));    AddField(out, g, _T("Sectors per FAT (16-bit)"), v, 0x16);
    v.Format(_T("%u"), ReadLE16(s + 24));    AddField(out, g, _T("Sectors per track"), v, 0x18);
    v.Format(_T("%u"), ReadLE16(s + 26));    AddField(out, g, _T("Number of heads"), v, 0x1A);
    v.Format(_T("%u"), ReadLE32(s + 28));    AddField(out, g, _T("Hidden sectors"), v, 0x1C,
                                                      _T("Sectors before this volume on the disk"));
    v.Format(_T("%u"), ReadLE32(s + 32));    AddField(out, g, _T("Total sectors (32-bit)"), v, 0x20);

    int ext = 36; // extended BPB offset for FAT12/16
    if (m_kind == FsKind::FAT32)
    {
        g = _T("FAT32 Extended BPB");
        v.Format(_T("%u"), ReadLE32(s + 36));    AddField(out, g, _T("Sectors per FAT"), v, 0x24);
        v.Format(_T("0x%04X"), ReadLE16(s + 40)); AddField(out, g, _T("Mirror flags"), v, 0x28,
                                                         _T("Bit 7 set: only one FAT is active (bits 0-3)"));
        v.Format(_T("%u.%u"), s[43], s[42]);      AddField(out, g, _T("Version"), v, 0x2A);
        v.Format(_T("%u"), m_rootCluster);        AddField(out, g, _T("Root directory cluster"), v, 0x2C);
        v.Format(_T("%u"), ReadLE16(s + 48));     AddField(out, g, _T("FSInfo sector"), v, 0x30);
        v.Format(_T("%u"), ReadLE16(s + 50));     AddField(out, g, _T("Backup boot sector"), v, 0x32);
        ext = 64;
    }
    else
        g = _T("Extended BPB");

    v.Format(_T("0x%02X"), s[ext]);              AddField(out, g, _T("Drive number"), v, ext);
    v.Format(_T("0x%02X"), s[ext + 2]);          AddField(out, g, _T("Extended boot signature"), v, ext + 2);
    v.Format(_T("%04X-%04X"), ReadLE16(s + ext + 5), ReadLE16(s + ext + 3));
                                                 AddField(out, g, _T("Volume serial number"), v, ext + 3);
    char label[12] = {}; memcpy(label, s + ext + 7, 11);
    CString lbl(label); lbl.TrimRight();         AddField(out, g, _T("Volume label"), lbl, ext + 7);
    char type[9] = {}; memcpy(type, s + ext + 18, 8);
    CString t(type); t.TrimRight();              AddField(out, g, _T("Filesystem type label"), t, ext + 18);
    v.Format(_T("%02X %02X"), s[510], s[511]);   AddField(out, g, _T("Boot signature"), v, 0x1FE);

    g = _T("Derived Layout");
    v.Format(_T("%s (by cluster count)"), KindName().GetString()); AddField(out, g, _T("FAT type"), v);
    v.Format(_T("%llu"), m_volumeStartLBA + m_fatStartRel);   AddField(out, g, _T("FAT #1 start LBA"), v);
    if (m_rootDirSectors)
    {
        v.Format(_T("%llu (%u sectors)"), m_volumeStartLBA + m_rootDirRel, m_rootDirSectors);
        AddField(out, g, _T("Root directory LBA"), v);
    }
    v.Format(_T("%llu"), m_volumeStartLBA + m_dataStartRel);  AddField(out, g, _T("Data area (cluster 2) LBA"), v);
    v.Format(_T("%u"), m_clusterCount);                       AddField(out, g, _T("Data clusters"), v);
    v.Format(_T("%s"), FormatSize(m_totalSectors * m_bps).GetString()); AddField(out, g, _T("Volume size"), v);
}

bool CFatFileSystem::GetFatEntry(DWORD cluster, DWORD& value)
{
    ULONGLONG byteOff;
    switch (m_kind)
    {
    case FsKind::FAT12: byteOff = cluster + cluster / 2; break;
    case FsKind::FAT16: byteOff = (ULONGLONG)cluster * 2; break;
    default:            byteOff = (ULONGLONG)cluster * 4; break;
    }

    const ULONGLONG blockBytes = (ULONGLONG)kFatBlockSectors * m_bps;
    auto readByte = [&](ULONGLONG off, BYTE& b) -> bool
    {
        ULONGLONG block = off / blockBytes;
        auto it = m_fatBlocks.find(block);
        if (it == m_fatBlocks.end())
        {
            ULONGLONG firstSec = block * kFatBlockSectors;
            if (firstSec >= m_fatSectors)
                return false;
            DWORD n = (DWORD)std::min<ULONGLONG>(kFatBlockSectors, m_fatSectors - firstSec);
            std::vector<BYTE> data;
            if (!ReadVolumeSectors(m_fatStartRel + firstSec, n, data))
                return false;
            it = m_fatBlocks.emplace(block, std::move(data)).first;
        }
        size_t idx = (size_t)(off % blockBytes);
        if (idx >= it->second.size())
            return false;
        b = it->second[idx];
        return true;
    };

    BYTE b[4] = {};
    int width = (m_kind == FsKind::FAT32) ? 4 : 2;
    for (int i = 0; i < width; ++i)
        if (!readByte(byteOff + i, b[i]))
            return false;

    if (m_kind == FsKind::FAT12)
    {
        WORD w = (WORD)(b[0] | (b[1] << 8));
        value = (cluster & 1) ? (w >> 4) : (w & 0x0FFF);
    }
    else if (m_kind == FsKind::FAT16)
        value = (DWORD)(b[0] | (b[1] << 8));
    else
        value = ReadLE32(b) & 0x0FFFFFFF;
    return true;
}

bool CFatFileSystem::IsEndOfChain(DWORD v) const
{
    switch (m_kind)
    {
    case FsKind::FAT12: return v >= 0xFF8;
    case FsKind::FAT16: return v >= 0xFFF8;
    default:            return v >= 0x0FFFFFF8;
    }
}

std::vector<DWORD> CFatFileSystem::Chain(DWORD start, DWORD maxLen)
{
    std::vector<DWORD> chain;
    DWORD cur = start;
    while (cur >= 2 && cur <= MaxCluster() && chain.size() < maxLen)
    {
        chain.push_back(cur);
        DWORD next = 0;
        if (!GetFatEntry(cur, next) || IsEndOfChain(next) || next < 2 || next > MaxCluster() || next == cur)
            break;
        cur = next;
    }
    return chain;
}

FsEntry CFatFileSystem::Root() const
{
    FsEntry root;
    root.name = _T("\\");
    root.isDirectory = true;
    root.id = m_rootCluster; // 0 = FAT12/16 fixed root region
    root.firstLBA = (m_kind == FsKind::FAT32) ? ClusterLBA(m_rootCluster) : m_volumeStartLBA + m_rootDirRel;
    return root;
}

bool CFatFileSystem::ReadDirectoryBytes(const FsEntry& dir, std::vector<BYTE>& data, std::vector<ULONGLONG>& lbas)
{
    data.clear();
    lbas.clear();

    if (dir.id == 0 && m_kind != FsKind::FAT32)
    {
        if (!ReadVolumeSectors(m_rootDirRel, m_rootDirSectors, data))
            return false;
        for (DWORD i = 0; i < m_rootDirSectors; ++i)
            lbas.push_back(m_volumeStartLBA + m_rootDirRel + i);
        return true;
    }

    std::vector<DWORD> clusters;
    if (!dir.isDeleted)
        clusters = Chain((DWORD)dir.id, 65536);
    else
    {
        // The chain of a deleted folder is gone: read forward from its first
        // cluster while the clusters are still free (cap: 64 clusters).
        for (DWORD c = (DWORD)dir.id; c <= MaxCluster() && clusters.size() < 64; ++c)
        {
            DWORD v = 1;
            if (!GetFatEntry(c, v) || (v != 0 && c != dir.id))
                break;
            clusters.push_back(c);
        }
    }

    for (DWORD c : clusters)
    {
        std::vector<BYTE> buf;
        if (!m_dm.ReadSectors(ClusterLBA(c), m_spc, buf))
            return false;
        bool endMarker = false;
        for (size_t off = 0; off + 32 <= buf.size(); off += 32)
            endMarker = endMarker || buf[off] == 0x00;
        data.insert(data.end(), buf.begin(), buf.end());
        for (DWORD i = 0; i < m_spc; ++i)
            lbas.push_back(ClusterLBA(c) + i);
        if (dir.isDeleted && endMarker)
            break;
    }
    return !clusters.empty();
}

bool CFatFileSystem::ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut)
{
    std::vector<BYTE> data;
    std::vector<ULONGLONG> lbas;
    if (!ReadDirectoryBytes(dir, data, lbas))
    {
        errorOut = _T("Could not read this directory's clusters: ") + m_dm.GetLastError();
        return false;
    }

    if (dir.isDeleted && (data.size() < 64 || data[0] != '.' || data[32] != '.' || data[33] != '.'))
    {
        errorOut = _T("This deleted folder's first cluster no longer holds a directory - it has been ")
                   _T("overwritten, so its contents cannot be listed.");
        return false;
    }

    // Long-file-name fragments preceding the current short entry. For live
    // entries they arrive last-part-first with sequence numbers; deleted LFN
    // entries lose their sequence byte (0xE5), so they're kept in on-disk order.
    std::vector<std::pair<BYTE, std::wstring>> lfnParts; // (checksum, 13 chars)
    bool lfnDeleted = false;

    const size_t count = data.size() / 32;
    for (size_t i = 0; i < count; ++i)
    {
        const BYTE* e = &data[i * 32];
        if (e[0] == 0x00)
            break; // end-of-directory marker

        const BYTE attr = e[11];
        const bool deleted = (e[0] == 0xE5);

        if ((attr & 0x3F) == 0x0F)
        {
            if (lfnParts.empty())
                lfnDeleted = deleted;
            else if (deleted != lfnDeleted)
                lfnParts.clear(), lfnDeleted = deleted;
            if (!deleted && (e[0] & 0x40))
                lfnParts.clear(); // first (= last-part) entry of a new live name
            WCHAR chars[13];
            LfnChars(e, chars);
            lfnParts.emplace_back(e[13], std::wstring(chars, 13));
            continue;
        }

        if (attr & 0x08)
        {
            lfnParts.clear(); // volume label
            continue;
        }

        FsEntry fe;
        fe.isDeleted = deleted;
        fe.isDirectory = (attr & 0x10) != 0;
        fe.attributes = attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                                FILE_ATTRIBUTE_ARCHIVE);
        if (fe.isDirectory)
            fe.attributes |= FILE_ATTRIBUTE_DIRECTORY;
        DWORD hi = (m_kind == FsKind::FAT32) ? ReadLE16(e + 20) : 0;
        fe.id = (hi << 16) | ReadLE16(e + 26);
        fe.size = fe.isDirectory ? 0 : ReadLE32(e + 28);
        fe.created = FatTimeToFileTime(ReadLE16(e + 16), ReadLE16(e + 14));
        fe.modified = FatTimeToFileTime(ReadLE16(e + 24), ReadLE16(e + 22));
        fe.metaLBA = lbas[(i * 32) / m_bps];
        if (fe.id >= 2 && fe.id <= MaxCluster())
            fe.firstLBA = ClusterLBA((DWORD)fe.id);

        // Pick the name: matching LFN if present, else the 8.3 name. For a
        // deleted entry the first byte was overwritten with 0xE5; the LFN
        // checksum covers that byte, so try every value to recover it.
        BYTE firstChar = e[0];
        bool lfnOk = false;
        if (!lfnParts.empty() && lfnDeleted == deleted)
        {
            BYTE sum = lfnParts.front().first;
            bool same = true;
            for (const auto& part : lfnParts)
                same = same && part.first == sum;
            if (same)
            {
                if (!deleted)
                    lfnOk = ShortNameChecksum(e) == sum;
                else
                {
                    BYTE name11[11];
                    memcpy(name11, e, 11);
                    for (int c = 0x20; c < 0x100 && !lfnOk; ++c)
                    {
                        name11[0] = (BYTE)c;
                        if (ShortNameChecksum(name11) == sum)
                            lfnOk = true, firstChar = (BYTE)c;
                    }
                }
            }
        }

        if (lfnOk)
        {
            std::wstring full;
            for (auto it = lfnParts.rbegin(); it != lfnParts.rend(); ++it)
                full += it->second; // stored last part first
            fe.name = TrimLfn(full);
        }
        else
            fe.name = ShortName(e, deleted ? (BYTE)'_' : firstChar);
        lfnParts.clear();

        if (fe.name == _T(".") || fe.name == _T(".."))
            continue;

        if (deleted)
            AssessDeleted(fe);
        out.push_back(fe);
    }
    return true;
}

void CFatFileSystem::AssessDeleted(FsEntry& e)
{
    e.contiguous = true;
    if (e.id < 2 || e.id > MaxCluster())
    {
        e.recover = Recoverability::None;
        e.recoverNote = (e.size == 0 && !e.isDirectory) ? _T("Empty file - there is no data to recover.")
                                                        : _T("No starting cluster was recorded for this entry.");
        return;
    }

    ULONGLONG needed = e.isDirectory ? 1 : std::max<ULONGLONG>(1, (e.size + m_clusterBytes - 1) / m_clusterBytes);
    ULONGLONG free = 0, checked = 0;
    for (ULONGLONG k = 0; k < needed; ++k)
    {
        DWORD c = (DWORD)(e.id + k);
        if (c > MaxCluster())
            break;
        DWORD v = 1;
        if (!GetFatEntry(c, v))
            break;
        ++checked;
        if (v == 0)
            ++free;
    }
    e.allocatedClusters = needed;
    e.runs.push_back({ (LONGLONG)e.id, needed });

    CString note;
    if (checked < needed)
    {
        e.recover = free ? Recoverability::Partial : Recoverability::None;
        note.Format(_T("The file would run past the end of the volume (%llu of %llu clusters exist) - it was ")
                    _T("fragmented, so only the start can be recovered."), checked, needed);
    }
    else if (free == needed)
    {
        e.recover = Recoverability::Good;
        note.Format(_T("All %llu cluster(s) are still free. Recovery assumes the file was stored contiguously ")
                    _T("(usually true); a fragmented file may come back partly corrupted."), needed);
    }
    else if (free == 0)
    {
        e.recover = Recoverability::None;
        note.Format(_T("All %llu cluster(s) have been reused by other files - the data has been overwritten."), needed);
    }
    else
    {
        e.recover = (free * 2 >= needed) ? Recoverability::Partial : Recoverability::Poor;
        note.Format(_T("%llu of %llu cluster(s) have been reused by other files - parts of the output will ")
                    _T("contain other data."), needed - free, needed);
    }
    e.recoverNote = note;
}

void CFatFileSystem::ComputeStats()
{
    if (m_statsDone)
        return;
    m_statsDone = true;
    DWORD bad = (m_kind == FsKind::FAT12) ? 0xFF7 : (m_kind == FsKind::FAT16) ? 0xFFF7 : 0x0FFFFFF7;
    for (DWORD c = 2; c <= MaxCluster(); ++c)
    {
        DWORD v = 0;
        if (!GetFatEntry(c, v))
            break;
        if (v == 0) ++m_freeClusters;
        else if (v == bad) ++m_badClusters;
    }
}

void CFatFileSystem::AllocationFields(std::vector<FieldRow>& out)
{
    ComputeStats();
    CString v;
    LPCTSTR g = _T("File Allocation Table");
    AddField(out, g, _T("Type"), KindName());
    v.Format(_T("%u"), m_numFats);                              AddField(out, g, _T("Copies"), v);
    v.Format(_T("%u sectors (%s)"), m_fatSectors, FormatSize((ULONGLONG)m_fatSectors * m_bps).GetString());
                                                                AddField(out, g, _T("Size of each copy"), v);
    for (DWORD i = 0; i < m_numFats; ++i)
    {
        CString n; n.Format(_T("FAT #%u start LBA"), i + 1);
        v.Format(_T("%llu"), m_volumeStartLBA + m_fatStartRel + (ULONGLONG)i * m_fatSectors);
        AddField(out, g, n, v);
    }
    v.Format(_T("%d bits"), m_kind == FsKind::FAT12 ? 12 : m_kind == FsKind::FAT16 ? 16 : 28);
                                                                AddField(out, g, _T("Entry width"), v);

    g = _T("Cluster Usage");
    DWORD used = m_clusterCount - m_freeClusters - m_badClusters;
    v.Format(_T("%u"), m_clusterCount);                         AddField(out, g, _T("Data clusters"), v);
    v.Format(_T("%s"), FormatSize(m_clusterBytes).GetString()); AddField(out, g, _T("Cluster size"), v);
    v.Format(_T("%u (%s)"), used, FormatSize((ULONGLONG)used * m_clusterBytes).GetString());
                                                                AddField(out, g, _T("Used clusters"), v);
    v.Format(_T("%u (%s)"), m_freeClusters, FormatSize((ULONGLONG)m_freeClusters * m_clusterBytes).GetString());
    AddField(out, g, _T("Free clusters"), v, -1,
             _T("Deleted files can only be recovered from clusters that are still free"));
    v.Format(_T("%u"), m_badClusters);                          AddField(out, g, _T("Bad clusters"), v);

    if (m_kind == FsKind::FAT32)
    {
        std::vector<BYTE> fsInfo;
        WORD fsInfoSec = ReadLE16(&m_boot[48]);
        if (fsInfoSec && ReadVolumeSectors(fsInfoSec, 1, fsInfo) && ReadLE32(&fsInfo[0]) == 0x41615252)
        {
            g = _T("FSInfo");
            DWORD f = ReadLE32(&fsInfo[488]), n = ReadLE32(&fsInfo[492]);
            v.Format(f == 0xFFFFFFFF ? _T("unknown") : _T("%u"), f); AddField(out, g, _T("Free cluster hint"), v, 488);
            v.Format(n == 0xFFFFFFFF ? _T("unknown") : _T("%u"), n); AddField(out, g, _T("Next free cluster hint"), v, 492);
        }
    }
}

CString CFatFileSystem::Recover(const FsEntry& e, const CString& outputPath, ULONGLONG& written)
{
    written = 0;
    if (e.isDirectory)
        return _T("Folders can't be recovered as a whole - open the folder and recover the files inside it.");
    if (e.id < 2 || e.id > MaxCluster())
        return _T("This entry has no valid starting cluster - there is nothing to read.");

    std::vector<ClusterRun> runs;
    if (e.isDeleted)
        runs = e.runs; // contiguous assumption (see header)
    else
    {
        for (DWORD c : Chain((DWORD)e.id))
        {
            if (!runs.empty() && (ULONGLONG)runs.back().lcn + runs.back().length == c)
                ++runs.back().length;
            else
                runs.push_back({ (LONGLONG)c, 1 });
        }
    }

    HANDLE hOut = CreateFile(outputPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE)
        return _T("Could not create the output file.");
    CString err;
    ReadRuns(runs, 2, m_volumeStartLBA + m_dataStartRel, e.size, hOut, written, err);
    CloseHandle(hOut);
    return err;
}
