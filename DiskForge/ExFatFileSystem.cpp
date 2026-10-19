// ExFatFileSystem.cpp
#include "pch.h"
#include "ExFatFileSystem.h"

namespace
{
    FILETIME ExFatTime(DWORD ts)
    {
        FILETIME local = {}, utc = {};
        if (ts == 0 || !DosDateTimeToFileTime(HIWORD(ts), LOWORD(ts), &local))
            return utc;
        LocalFileTimeToFileTime(&local, &utc);
        return utc;
    }

    const ULONGLONG kMaxDirBytes = 64ULL * 1024 * 1024; // sanity cap for one directory
}

bool CExFatFileSystem::Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors,
                            CString& errorOut)
{
    if (!IFileSystem::Init(devicePath, volumeStartLBA, volumeSectors, errorOut))
        return false;
    if (!m_dm.ReadSector(volumeStartLBA, m_boot) || m_boot.size() < 512)
    {
        errorOut = _T("Could not read the boot record.");
        return false;
    }

    const BYTE* s = m_boot.data();
    BYTE bpsShift = s[0x6C], spcShift = s[0x6D];
    if (bpsShift < 9 || bpsShift > 12 || spcShift > 25 - bpsShift)
    {
        errorOut = _T("The exFAT boot record has implausible sector/cluster sizes.");
        return false;
    }
    if ((1u << bpsShift) != SectorSize())
    {
        errorOut.Format(_T("The volume uses %u-byte sectors but the disk reports %u-byte sectors; ")
                        _T("this layout is not supported."), 1u << bpsShift, SectorSize());
        return false;
    }

    m_spc          = 1u << spcShift;
    m_fatOffset    = ReadLE32(s + 0x50);
    m_fatLength    = ReadLE32(s + 0x54);
    m_heapOffset   = ReadLE32(s + 0x58);
    m_clusterCount = ReadLE32(s + 0x5C);
    m_rootCluster  = ReadLE32(s + 0x60);
    m_clusterBytes = (ULONGLONG)m_spc * SectorSize();

    ULONGLONG volLen = ReadLE64(s + 0x48);
    if (m_volumeSectors == 0 || m_volumeSectors > volLen)
        m_volumeSectors = volLen;
    if (m_rootCluster < 2 || m_rootCluster > m_clusterCount + 1)
    {
        errorOut = _T("The exFAT root directory cluster is out of range.");
        return false;
    }
    return true;
}

ULONGLONG CExFatFileSystem::AllocationLBA() const
{
    return m_volumeStartLBA + m_fatOffset;
}

void CExFatFileSystem::BootRecordFields(std::vector<FieldRow>& out) const
{
    const BYTE* s = m_boot.data();
    CString v;
    LPCTSTR g = _T("Boot Sector");
    v.Format(_T("%02X %02X %02X"), s[0], s[1], s[2]);      AddField(out, g, _T("Jump instruction"), v, 0x00);
    AddField(out, g, _T("Filesystem name"), _T("EXFAT"), 0x03);

    g = _T("exFAT Parameters");
    v.Format(_T("%llu"), ReadLE64(s + 0x40));  AddField(out, g, _T("Partition offset (sectors)"), v, 0x40);
    v.Format(_T("%llu (%s)"), ReadLE64(s + 0x48), FormatSize(ReadLE64(s + 0x48) * SectorSize()).GetString());
                                               AddField(out, g, _T("Volume length (sectors)"), v, 0x48);
    v.Format(_T("%u"), m_fatOffset);           AddField(out, g, _T("FAT offset"), v, 0x50);
    v.Format(_T("%u"), m_fatLength);           AddField(out, g, _T("FAT length (sectors)"), v, 0x54);
    v.Format(_T("%u"), m_heapOffset);          AddField(out, g, _T("Cluster heap offset"), v, 0x58);
    v.Format(_T("%u"), m_clusterCount);        AddField(out, g, _T("Cluster count"), v, 0x5C);
    v.Format(_T("%u"), m_rootCluster);         AddField(out, g, _T("Root directory cluster"), v, 0x60);
    v.Format(_T("%04X-%04X"), ReadLE16(s + 0x66), ReadLE16(s + 0x64));
                                               AddField(out, g, _T("Volume serial number"), v, 0x64);
    v.Format(_T("%u.%02u"), s[0x69], s[0x68]); AddField(out, g, _T("Revision"), v, 0x68);
    WORD flags = ReadLE16(s + 0x6A);
    v.Format(_T("0x%04X%s%s"), flags, (flags & 2) ? _T(" (dirty)") : _T(""), (flags & 4) ? _T(" (media failure)") : _T(""));
                                               AddField(out, g, _T("Volume flags"), v, 0x6A);
    v.Format(_T("2^%u = %u"), s[0x6C], 1u << s[0x6C]); AddField(out, g, _T("Bytes per sector"), v, 0x6C);
    v.Format(_T("2^%u = %u (%s)"), s[0x6D], m_spc, FormatSize(m_clusterBytes).GetString());
                                               AddField(out, g, _T("Sectors per cluster"), v, 0x6D);
    v.Format(_T("%u"), s[0x6E]);               AddField(out, g, _T("Number of FATs"), v, 0x6E);
    v.Format(s[0x70] == 0xFF ? _T("unknown") : _T("%u%%"), s[0x70]); AddField(out, g, _T("Percent in use"), v, 0x70);
    v.Format(_T("%02X %02X"), s[510], s[511]); AddField(out, g, _T("Boot signature"), v, 0x1FE);
}

FsEntry CExFatFileSystem::Root() const
{
    FsEntry root;
    root.name = _T("\\");
    root.isDirectory = true;
    root.id = m_rootCluster;
    root.firstLBA = ClusterLBA(m_rootCluster);
    return root;
}

bool CExFatFileSystem::FatNext(DWORD c, DWORD& next)
{
    ULONGLONG byteOff = (ULONGLONG)c * 4;
    std::vector<BYTE> sec;
    if (!ReadVolumeSectors(m_fatOffset + byteOff / SectorSize(), 1, sec))
        return false;
    next = ReadLE32(&sec[(size_t)(byteOff % SectorSize())]);
    return true;
}

bool CExFatFileSystem::ExtentRuns(DWORD first, ULONGLONG length, bool noFatChain, std::vector<ClusterRun>& runs)
{
    runs.clear();
    if (first < 2 || first > m_clusterCount + 1)
        return false;
    ULONGLONG clusters = length ? (length + m_clusterBytes - 1) / m_clusterBytes : 0;

    if (noFatChain)
    {
        runs.push_back({ (LONGLONG)first, std::max<ULONGLONG>(clusters, 1) });
        return true;
    }

    DWORD cur = first;
    for (ULONGLONG n = 0; n < (clusters ? clusters : 65536); ++n)
    {
        if (!runs.empty() && (ULONGLONG)runs.back().lcn + runs.back().length == cur)
            ++runs.back().length;
        else
            runs.push_back({ (LONGLONG)cur, 1 });
        DWORD next = 0;
        if (!FatNext(cur, next) || next < 2 || next > m_clusterCount + 1 || next == cur)
            break;
        cur = next;
    }
    return true;
}

bool CExFatFileSystem::ReadExtent(const std::vector<ClusterRun>& runs, ULONGLONG maxBytes, std::vector<BYTE>& data,
                                  std::vector<ULONGLONG>& lbas)
{
    data.clear();
    lbas.clear();
    for (const ClusterRun& r : runs)
    {
        for (ULONGLONG k = 0; k < r.length && data.size() < maxBytes; ++k)
        {
            DWORD c = (DWORD)(r.lcn + k);
            if (c > m_clusterCount + 1)
                return !data.empty();
            std::vector<BYTE> buf;
            if (!m_dm.ReadSectors(ClusterLBA(c), m_spc, buf))
                return false;
            data.insert(data.end(), buf.begin(), buf.end());
            for (DWORD i = 0; i < m_spc; ++i)
                lbas.push_back(ClusterLBA(c) + i);
        }
    }
    return true;
}

bool CExFatFileSystem::LoadBitmap()
{
    if (m_bitmapLoaded)
        return !m_bitmap.empty();
    m_bitmapLoaded = true;

    // The bitmap's location is recorded in a 0x81 entry of the root directory.
    std::vector<ClusterRun> runs;
    std::vector<BYTE> root;
    std::vector<ULONGLONG> lbas;
    if (!ExtentRuns(m_rootCluster, 0, false, runs) || !ReadExtent(runs, kMaxDirBytes, root, lbas))
        return false;
    for (size_t off = 0; off + 32 <= root.size(); off += 32)
    {
        if (root[off] == 0x00)
            break;
        if (root[off] == 0x81 && (root[off + 1] & 1) == 0) // first bitmap (TexFAT may have two)
        {
            m_bitmapCluster = ReadLE32(&root[off + 20]);
            m_bitmapLength = ReadLE64(&root[off + 24]);
            break;
        }
    }
    if (m_bitmapCluster < 2 || m_bitmapLength == 0 || m_bitmapLength > 512ULL * 1024 * 1024)
        return false;

    std::vector<ClusterRun> bmRuns;
    if (!ExtentRuns(m_bitmapCluster, m_bitmapLength, false, bmRuns))
        return false;
    // The bitmap is normally contiguous even without NoFatChain; follow the FAT to be safe.
    if (!ReadExtent(bmRuns, m_bitmapLength, m_bitmap, lbas))
    {
        m_bitmap.clear();
        return false;
    }
    m_bitmap.resize((size_t)m_bitmapLength);
    return true;
}

bool CExFatFileSystem::IsClusterInUse(DWORD c)
{
    if (!LoadBitmap() || c < 2)
        return true;
    ULONGLONG bit = c - 2;
    if (bit / 8 >= m_bitmap.size())
        return true;
    return (m_bitmap[(size_t)(bit / 8)] >> (bit % 8)) & 1;
}

bool CExFatFileSystem::ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut)
{
    std::vector<ClusterRun> runs;
    if (!ExtentRuns((DWORD)dir.id, dir.size, dir.contiguous, runs))
    {
        errorOut = _T("This directory's first cluster is out of range.");
        return false;
    }
    std::vector<BYTE> data;
    std::vector<ULONGLONG> lbas;
    if (!ReadExtent(runs, dir.size ? dir.size : kMaxDirBytes, data, lbas))
    {
        errorOut = _T("Could not read this directory: ") + m_dm.GetLastError();
        return false;
    }

    const size_t count = data.size() / 32;
    for (size_t i = 0; i < count; ++i)
    {
        const BYTE* e = &data[i * 32];
        BYTE type = e[0];
        if (type == 0x00)
            break;                      // end of directory
        if ((type & 0x7F) != 0x05)
            continue;                   // only File entries (0x85 live / 0x05 deleted) start a set

        const bool deleted = (type & 0x80) == 0;
        const BYTE secondaries = e[1];
        if (secondaries < 2 || i + secondaries >= count)
            continue;
        const BYTE* stream = &data[(i + 1) * 32];
        if ((stream[0] & 0x7F) != 0x40)
            continue;                   // stream extension must follow
        // A deleted set must be deleted throughout; a live set live throughout.
        if (((stream[0] & 0x80) == 0) != deleted)
            continue;

        FsEntry fe;
        fe.isDeleted = deleted;
        WORD attr = ReadLE16(e + 4);
        fe.attributes = attr;
        fe.isDirectory = (attr & 0x10) != 0;
        fe.created  = ExFatTime(ReadLE32(e + 8));
        fe.modified = ExFatTime(ReadLE32(e + 12));
        fe.metaLBA  = lbas[(i * 32) / SectorSize()];

        BYTE flags     = stream[1];
        BYTE nameLen   = stream[3];
        fe.contiguous  = (flags & 0x02) != 0;   // NoFatChain
        fe.id          = ReadLE32(stream + 20);
        fe.size        = ReadLE64(stream + 24);
        if (fe.id >= 2 && fe.id <= m_clusterCount + 1)
            fe.firstLBA = ClusterLBA((DWORD)fe.id);

        std::wstring name;
        for (BYTE k = 2; k <= secondaries && i + k < count && name.size() < nameLen; ++k)
        {
            const BYTE* n = &data[(i + k) * 32];
            if ((n[0] & 0x7F) != 0x41)
                break;
            for (int c = 0; c < 15 && name.size() < nameLen; ++c)
                name += (wchar_t)ReadLE16(n + 2 + c * 2);
        }
        fe.name = name.c_str();
        if (fe.name.IsEmpty())
            fe.name = _T("(unnamed)");
        if (fe.isDirectory)
            fe.size = ReadLE64(stream + 24); // directories carry their extent length too
        i += secondaries;

        if (deleted)
        {
            ULONGLONG clusters = fe.size ? (fe.size + m_clusterBytes - 1) / m_clusterBytes : 0;
            if (fe.id < 2 || fe.id > m_clusterCount + 1 || clusters == 0)
            {
                fe.recover = Recoverability::None;
                fe.recoverNote = fe.size == 0 ? _T("Empty file - there is no data to recover.")
                                              : _T("No starting cluster was recorded for this entry.");
            }
            else
            {
                ExtentRuns((DWORD)fe.id, fe.size, fe.contiguous, fe.runs);
                ULONGLONG total = 0, free = 0;
                for (const ClusterRun& r : fe.runs)
                    for (ULONGLONG k = 0; k < r.length; ++k, ++total)
                        free += IsClusterInUse((DWORD)(r.lcn + k)) ? 0 : 1;
                fe.allocatedClusters = total;

                if (!fe.contiguous && total < clusters)
                {
                    fe.recover = free ? Recoverability::Poor : Recoverability::None;
                    fe.recoverNote = _T("The file was fragmented and its FAT chain has been cleared - only the ")
                                     _T("first fragment can be located.");
                }
                else if (free == total)
                {
                    fe.recover = Recoverability::Good;
                    fe.recoverNote.Format(fe.contiguous
                        ? _T("Contiguous file (NoFatChain); all %llu cluster(s) are still free.")
                        : _T("Fragmented file; all %llu cluster(s) on its old FAT chain are still free."), total);
                }
                else if (free == 0)
                {
                    fe.recover = Recoverability::None;
                    fe.recoverNote = _T("All of the file's clusters have been reused - the data has been overwritten.");
                }
                else
                {
                    fe.recover = (free * 2 >= total) ? Recoverability::Partial : Recoverability::Poor;
                    fe.recoverNote.Format(_T("%llu of %llu cluster(s) have been reused by other files."),
                                          total - free, total);
                }
            }
        }
        out.push_back(fe);
    }
    return true;
}

void CExFatFileSystem::AllocationFields(std::vector<FieldRow>& out)
{
    CString v;
    LPCTSTR g = _T("FAT");
    v.Format(_T("%llu"), m_volumeStartLBA + m_fatOffset);  AddField(out, g, _T("FAT start LBA"), v);
    v.Format(_T("%u sectors"), m_fatLength);               AddField(out, g, _T("FAT length"), v);
    AddField(out, g, _T("Note"), _T("Only fragmented files use FAT chains; contiguous files are flagged NoFatChain."));

    g = _T("Allocation Bitmap");
    if (!LoadBitmap())
    {
        AddField(out, g, _T("Status"), _T("Not found / unreadable"));
        return;
    }
    v.Format(_T("%u"), m_bitmapCluster);                   AddField(out, g, _T("First cluster"), v);
    v.Format(_T("%llu"), ClusterLBA(m_bitmapCluster));     AddField(out, g, _T("Start LBA"), v);
    v.Format(_T("%llu bytes"), m_bitmapLength);            AddField(out, g, _T("Length"), v);

    ULONGLONG used = 0;
    for (DWORD c = 0; c < m_clusterCount && c / 8 < m_bitmap.size(); ++c)
        used += (m_bitmap[c / 8] >> (c % 8)) & 1;
    g = _T("Cluster Usage");
    v.Format(_T("%u"), m_clusterCount);                    AddField(out, g, _T("Clusters"), v);
    v.Format(_T("%s"), FormatSize(m_clusterBytes).GetString()); AddField(out, g, _T("Cluster size"), v);
    v.Format(_T("%llu (%s)"), used, FormatSize(used * m_clusterBytes).GetString()); AddField(out, g, _T("Used"), v);
    v.Format(_T("%llu (%s)"), m_clusterCount - used, FormatSize((m_clusterCount - used) * m_clusterBytes).GetString());
    AddField(out, g, _T("Free"), v, -1, _T("Deleted files can only be recovered from clusters that are still free"));
}

CString CExFatFileSystem::Recover(const FsEntry& e, const CString& outputPath, ULONGLONG& written)
{
    written = 0;
    if (e.isDirectory)
        return _T("Folders can't be recovered as a whole - open the folder and recover the files inside it.");

    std::vector<ClusterRun> runs = e.runs;
    if (runs.empty() && !ExtentRuns((DWORD)e.id, e.size, e.contiguous, runs))
        return _T("This entry has no valid starting cluster - there is nothing to read.");

    HANDLE hOut = CreateFile(outputPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE)
        return _T("Could not create the output file.");
    CString err;
    ReadRuns(runs, 2, m_volumeStartLBA + m_heapOffset, e.size, hOut, written, err);
    CloseHandle(hOut);
    return err;
}
