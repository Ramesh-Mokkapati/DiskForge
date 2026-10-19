// NtfsFileSystem.cpp
#include "pch.h"
#include "NtfsFileSystem.h"

namespace
{
    const DWORD kAttrStandardInfo = 0x10;
    const DWORD kAttrList         = 0x20;
    const DWORD kAttrFileName     = 0x30;
    const DWORD kAttrData         = 0x80;
    const DWORD kAttrEnd          = 0xFFFFFFFF;

    const ULONGLONG kRootRecord   = 5;
    const ULONGLONG kBitmapRecord = 6;

    FILETIME ToFileTime(ULONGLONG v)
    {
        FILETIME ft;
        ft.dwLowDateTime = (DWORD)v;
        ft.dwHighDateTime = (DWORD)(v >> 32);
        return ft;
    }

    // Calls fn(attrPtr, attrLen) for every attribute in a fixed-up record.
    template <typename Fn>
    void ForEachAttribute(const BYTE* rec, DWORD recSize, Fn fn)
    {
        DWORD off = ReadLE16(rec + 0x14);
        DWORD used = std::min<DWORD>(ReadLE32(rec + 0x18), recSize);
        while (off + 16 <= used)
        {
            DWORD type = ReadLE32(rec + off);
            DWORD len = ReadLE32(rec + off + 4);
            if (type == kAttrEnd || len < 16 || off + len > used)
                break;
            if (!fn(rec + off, len))
                break;
            off += len;
        }
    }

    bool ResidentValue(const BYTE* a, DWORD len, const BYTE*& value, DWORD& valueLen)
    {
        if (a[8] != 0)
            return false;
        valueLen = ReadLE32(a + 0x10);
        WORD valueOff = ReadLE16(a + 0x14);
        if ((ULONGLONG)valueOff + valueLen > len)
            return false;
        value = a + valueOff;
        return true;
    }
}

bool CNtfsFileSystem::Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors,
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
    WORD bps = ReadLE16(s + 0x0B);
    BYTE spcRaw = s[0x0D];
    m_spc = (spcRaw <= 0x80) ? spcRaw : (1ULL << (256 - spcRaw));
    if (bps != SectorSize() || m_spc == 0)
    {
        errorOut.Format(_T("Unsupported NTFS geometry (%u bytes/sector on disk with %u-byte sectors)."),
                        bps, SectorSize());
        return false;
    }
    m_clusterBytes = m_spc * bps;
    ULONGLONG totalSectors = ReadLE64(s + 0x28);
    m_totalClusters = totalSectors / m_spc;
    m_mftLcn = ReadLE64(s + 0x30);
    m_mftMirrLcn = ReadLE64(s + 0x38);
    signed char cpr = (signed char)s[0x40];
    m_recordSize = (cpr > 0) ? (DWORD)(cpr * m_clusterBytes) : (1u << (-cpr));
    if (m_recordSize < 512 || m_recordSize > 65536 || m_mftLcn >= m_totalClusters)
    {
        errorOut = _T("The NTFS boot record has an implausible MFT location or record size.");
        return false;
    }
    if (m_volumeSectors == 0 || m_volumeSectors > totalSectors + 1)
        m_volumeSectors = totalSectors + 1; // +1: the backup boot sector sits just past "total sectors"

    // Bootstrap: read record 0 ($MFT) straight from its LCN to learn where
    // the rest of the MFT lives.
    std::vector<BYTE> rec0;
    DWORD secs = (m_recordSize + bps - 1) / bps;
    if (!m_dm.ReadSectors(volumeStartLBA + m_mftLcn * m_spc, secs, rec0) || memcmp(rec0.data(), "FILE", 4) != 0 ||
        !ApplyFixup(rec0.data(), m_recordSize))
    {
        errorOut = _T("Could not read a valid $MFT record at the location given by the boot record.");
        return false;
    }
    ForEachAttribute(rec0.data(), m_recordSize, [&](const BYTE* a, DWORD len)
    {
        if (ReadLE32(a) == kAttrData && a[8] == 1 && a[9] == 0 && ReadLE64(a + 0x10) == 0)
        {
            ParseRuns(a, len, m_mftRuns);
            m_mftBytes = ReadLE64(a + 0x30);
            return false;
        }
        return true;
    });
    if (m_mftRuns.empty() || m_mftBytes < m_recordSize * 16ULL)
    {
        errorOut = _T("The $MFT record has no usable data runs.");
        return false;
    }
    auto rebuildSegments = [&]
    {
        m_mftSegments.clear();
        for (const ClusterRun& r : m_mftRuns)
            if (r.lcn >= 0)
                m_mftSegments.emplace_back(m_volumeStartLBA + (ULONGLONG)r.lcn * m_spc, r.length * m_spc);
    };
    rebuildSegments();

    // A very fragmented MFT spills its run list into extension records via
    // an $ATTRIBUTE_LIST; now that the first runs are known, follow it.
    DataInfo full;
    if (ReadData(0, full) && !full.resident && full.runs.size() > m_mftRuns.size())
    {
        m_mftRuns = full.runs;
        m_mftBytes = full.size;
        rebuildSegments();
    }
    return true;
}

bool CNtfsFileSystem::ApplyFixup(BYTE* rec, DWORD size) const
{
    WORD usaOff = ReadLE16(rec + 4);
    WORD usaCount = ReadLE16(rec + 6);
    if (usaCount < 2 || (DWORD)usaOff + usaCount * 2 > size)
        return false;
    DWORD stride = size / (usaCount - 1);
    if (stride < 256 || stride * (usaCount - 1) != size)
        return false;
    WORD usn = ReadLE16(rec + usaOff);
    for (WORD i = 1; i < usaCount; ++i)
    {
        BYTE* tail = rec + i * stride - 2;
        if (ReadLE16(tail) != usn)
            return false; // torn / corrupt record
        memcpy(tail, rec + usaOff + i * 2, 2);
    }
    return true;
}

bool CNtfsFileSystem::ParseRuns(const BYTE* a, DWORD attrLen, std::vector<ClusterRun>& runs) const
{
    WORD off = ReadLE16(a + 0x20);
    LONGLONG lcn = 0;
    while (off < attrLen)
    {
        BYTE hdr = a[off];
        if (hdr == 0)
            break;
        int lenBytes = hdr & 0x0F, offBytes = hdr >> 4;
        if (lenBytes == 0 || lenBytes > 8 || offBytes > 8 || (DWORD)(off + 1 + lenBytes + offBytes) > attrLen)
            return false;
        ULONGLONG length = 0;
        for (int i = 0; i < lenBytes; ++i)
            length |= (ULONGLONG)a[off + 1 + i] << (8 * i);
        ClusterRun run;
        run.length = length;
        if (offBytes == 0)
            run.lcn = -1; // sparse
        else
        {
            LONGLONG delta = 0;
            for (int i = 0; i < offBytes; ++i)
                delta |= (LONGLONG)a[off + 1 + lenBytes + i] << (8 * i);
            if (a[off + lenBytes + offBytes] & 0x80) // sign-extend
                delta |= -1LL << (8 * offBytes);
            lcn += delta;
            if (lcn < 0 || (ULONGLONG)lcn >= m_totalClusters)
                return false;
            run.lcn = lcn;
        }
        runs.push_back(run);
        off += (WORD)(1 + lenBytes + offBytes);
    }
    return true;
}

bool CNtfsFileSystem::ReadMftBytes(ULONGLONG offset, size_t len, std::vector<BYTE>& out)
{
    out.clear();
    out.reserve(len);
    const DWORD bps = SectorSize();
    ULONGLONG segStartByte = 0;
    for (const auto& seg : m_mftSegments)
    {
        ULONGLONG segBytes = seg.second * bps;
        ULONGLONG want = offset + out.size();
        if (out.size() < len && want < segStartByte + segBytes && want >= segStartByte)
        {
            ULONGLONG inSeg = want - segStartByte;
            ULONGLONG take = std::min<ULONGLONG>(len - out.size(), segBytes - inSeg);
            ULONGLONG firstSec = inSeg / bps;
            ULONGLONG lastSec = (inSeg + take + bps - 1) / bps;
            std::vector<BYTE> buf;
            if (!m_dm.ReadSectors(seg.first + firstSec, (DWORD)(lastSec - firstSec), buf))
                return false;
            size_t skip = (size_t)(inSeg - firstSec * bps);
            out.insert(out.end(), buf.begin() + skip, buf.begin() + skip + (size_t)take);
        }
        segStartByte += segBytes;
        if (out.size() >= len)
            return true;
    }
    return out.size() == len;
}

bool CNtfsFileSystem::ReadRecord(ULONGLONG recNo, std::vector<BYTE>& rec)
{
    if (!ReadMftBytes(recNo * m_recordSize, m_recordSize, rec))
        return false;
    return memcmp(rec.data(), "FILE", 4) == 0 && ApplyFixup(rec.data(), m_recordSize);
}

bool CNtfsFileSystem::ReadData(ULONGLONG recNo, DataInfo& info)
{
    info = DataInfo();
    std::vector<BYTE> rec;
    if (!ReadRecord(recNo, rec))
        return false;

    struct Piece { ULONGLONG vcn; std::vector<ClusterRun> runs; };
    std::vector<Piece> pieces;
    std::vector<ULONGLONG> extRecords;

    auto takeData = [&](const BYTE* a, DWORD len)
    {
        if (ReadLE32(a) != kAttrData || a[9] != 0)
            return;                     // only the unnamed (default) stream
        WORD flags = ReadLE16(a + 0x0C);
        info.found = true;
        if (a[8] == 0)
        {
            const BYTE* v; DWORD vl;
            if (ResidentValue(a, len, v, vl))
            {
                info.resident = true;
                info.size = vl;
                info.residentData.assign(v, v + vl);
            }
            return;
        }
        ULONGLONG vcn = ReadLE64(a + 0x10);
        if (vcn == 0)
        {
            info.size = ReadLE64(a + 0x30);
            info.compressed = (flags & 0x0001) != 0;
            info.encrypted = (flags & 0x4000) != 0;
        }
        Piece p;
        p.vcn = vcn;
        if (ParseRuns(a, len, p.runs))
            pieces.push_back(std::move(p));
    };

    ForEachAttribute(rec.data(), m_recordSize, [&](const BYTE* a, DWORD len)
    {
        DWORD type = ReadLE32(a);
        if (type == kAttrData)
            takeData(a, len);
        else if (type == kAttrList)
        {
            std::vector<BYTE> list;
            const BYTE* v; DWORD vl;
            if (ResidentValue(a, len, v, vl))
                list.assign(v, v + vl);
            else
            {
                std::vector<ClusterRun> runs;
                if (ParseRuns(a, len, runs))
                    for (const ClusterRun& r : runs)
                        if (r.lcn >= 0 && list.size() < 1024 * 1024)
                        {
                            std::vector<BYTE> buf;
                            if (m_dm.ReadSectors(m_volumeStartLBA + (ULONGLONG)r.lcn * m_spc, (DWORD)(r.length * m_spc), buf))
                                list.insert(list.end(), buf.begin(), buf.end());
                        }
                list.resize((size_t)std::min<ULONGLONG>(list.size(), ReadLE64(a + 0x30)));
            }
            for (size_t off = 0; off + 26 <= list.size(); )
            {
                WORD entLen = ReadLE16(&list[off + 4]);
                if (entLen < 26)
                    break;
                if (ReadLE32(&list[off]) == kAttrData && list[off + 6] == 0)
                {
                    ULONGLONG ref = ReadLE64(&list[off + 16]) & 0x0000FFFFFFFFFFFFULL;
                    if (ref != recNo)
                        extRecords.push_back(ref);
                }
                off += entLen;
            }
        }
        return true;
    });

    for (ULONGLONG ext : extRecords)
    {
        std::vector<BYTE> er;
        if (ReadRecord(ext, er))
            ForEachAttribute(er.data(), m_recordSize, [&](const BYTE* a, DWORD len) { takeData(a, len); return true; });
    }

    std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) { return a.vcn < b.vcn; });
    for (Piece& p : pieces)
        info.runs.insert(info.runs.end(), p.runs.begin(), p.runs.end());
    return true;
}

void CNtfsFileSystem::ParseRecord(const BYTE* rec, Rec& r) const
{
    WORD recFlags = ReadLE16(rec + 0x16);
    r.seq = ReadLE16(rec + 0x10);
    r.flags = 0;
    if (recFlags & 0x01) r.flags |= kInUse;
    if (recFlags & 0x02) r.flags |= kDir;
    if (ReadLE64(rec + 0x20) != 0)
        return;                         // extension record - its base record carries the name

    int bestNs = -1;
    bool sawData = false;
    ULONGLONG fnSize = 0;
    ForEachAttribute(rec, m_recordSize, [&](const BYTE* a, DWORD len)
    {
        DWORD type = ReadLE32(a);
        const BYTE* v; DWORD vl;
        if (type == kAttrStandardInfo && ResidentValue(a, len, v, vl) && vl >= 36)
        {
            r.created = ToFileTime(ReadLE64(v));
            r.modified = ToFileTime(ReadLE64(v + 8));
            r.attributes = ReadLE32(v + 32);
        }
        else if (type == kAttrFileName && ResidentValue(a, len, v, vl) && vl >= 66)
        {
            BYTE nameLen = v[64], ns = v[65];
            if (66u + nameLen * 2u > vl)
                return true;
            // Preference: Win32 / Win32+DOS, then POSIX, then the 8.3 DOS name.
            int rank = (ns == 1 || ns == 3) ? 3 : (ns == 0) ? 2 : 1;
            if (rank > bestNs)
            {
                bestNs = rank;
                ULONGLONG ref = ReadLE64(v);
                r.parent = ref & 0x0000FFFFFFFFFFFFULL;
                r.parentSeq = (WORD)(ref >> 48);
                r.name.assign(reinterpret_cast<const wchar_t*>(v + 66), nameLen);
                fnSize = ReadLE64(v + 48);
                r.flags |= kHasName;
            }
        }
        else if (type == kAttrData && a[9] == 0)
        {
            sawData = true;
            if (a[8] == 0)
            {
                r.flags |= kResident;
                r.size = ReadLE32(a + 0x10);
            }
            else if (ReadLE64(a + 0x10) == 0)
            {
                r.size = ReadLE64(a + 0x30);
                WORD f = ReadLE16(a + 0x0C);
                if (f & 0x0001) r.flags |= kCompressed;
                if (f & 0x4000) r.flags |= kEncrypted;
            }
        }
        return true;
    });
    if (!sawData)
    {
        r.flags |= kNoData;
        r.size = (r.flags & kDir) ? 0 : fnSize; // data described by an attribute list elsewhere
    }
    if (r.flags & kDir)
        r.attributes |= FILE_ATTRIBUTE_DIRECTORY;
}

bool CNtfsFileSystem::BuildIndex(const std::function<bool(int)>& progress, CString& errorOut)
{
    const ULONGLONG total = m_mftBytes / m_recordSize;
    if (total == 0 || total > 200000000ULL)
    {
        errorOut = _T("The $MFT size is implausible.");
        return false;
    }
    m_recs.assign((size_t)total, Rec());
    m_inUse = m_deleted = m_dirs = 0;

    const ULONGLONG kChunk = std::max<ULONGLONG>(1, (4ULL * 1024 * 1024) / m_recordSize);
    int lastPct = -1;
    for (ULONGLONG first = 0; first < total; first += kChunk)
    {
        ULONGLONG n = std::min<ULONGLONG>(kChunk, total - first);
        std::vector<BYTE> buf;
        if (!ReadMftBytes(first * m_recordSize, (size_t)(n * m_recordSize), buf))
            continue; // unreadable chunk (bad sectors) - skip those records
        for (ULONGLONG i = 0; i < n; ++i)
        {
            BYTE* rec = &buf[(size_t)(i * m_recordSize)];
            if (memcmp(rec, "FILE", 4) != 0 || !ApplyFixup(rec, m_recordSize))
                continue;
            Rec& r = m_recs[(size_t)(first + i)];
            ParseRecord(rec, r);
            if (!(r.flags & kHasName))
                continue;
            if (r.flags & kInUse) ++m_inUse; else ++m_deleted;
            if (r.flags & kDir) ++m_dirs;
        }
        int pct = (int)((first + n) * 95 / total);
        if (pct != lastPct)
        {
            lastPct = pct;
            if (!progress(pct))
            {
                errorOut = _T("Cancelled.");
                return false;
            }
        }
    }

    // Rebuild the tree from parent references. A deleted entry only belongs
    // to its old folder if that folder record hasn't since been reused
    // (sequence number unchanged, or bumped once by the folder's own deletion).
    m_children.clear();
    m_orphans.clear();
    for (ULONGLONG i = 0; i < total; ++i)
    {
        const Rec& r = m_recs[(size_t)i];
        if (!(r.flags & kHasName) || i == kRootRecord)
            continue;
        bool ok = r.parent < total && r.parent != i;
        if (ok)
        {
            const Rec& p = m_recs[(size_t)r.parent];
            ok = (p.flags & kDir) && (p.flags & kHasName || r.parent == kRootRecord) &&
                 (r.parentSeq == p.seq || (WORD)(r.parentSeq + 1) == p.seq || r.parentSeq == 0);
            if (ok && (r.flags & kInUse) && !(p.flags & kInUse))
                ok = false;
        }
        if (ok)
            m_children[r.parent].push_back(i);
        else
            m_orphans.push_back(i);
    }

    // Volume $Bitmap - which clusters are allocated right now.
    DataInfo bm;
    m_volBitmap.clear();
    if (ReadData(kBitmapRecord, bm) && !bm.resident && bm.size <= 512ULL * 1024 * 1024)
    {
        for (const ClusterRun& r : bm.runs)
        {
            if (r.lcn < 0)
            {
                m_volBitmap.insert(m_volBitmap.end(), (size_t)(r.length * m_clusterBytes), 0);
                continue;
            }
            for (ULONGLONG done = 0; done < r.length; )
            {
                ULONGLONG n = std::min<ULONGLONG>(r.length - done, std::max<ULONGLONG>(1, (4ULL << 20) / m_clusterBytes));
                std::vector<BYTE> buf;
                if (!m_dm.ReadSectors(m_volumeStartLBA + ((ULONGLONG)r.lcn + done) * m_spc, (DWORD)(n * m_spc), buf))
                {
                    m_volBitmap.clear();
                    break;
                }
                m_volBitmap.insert(m_volBitmap.end(), buf.begin(), buf.end());
                done += n;
            }
        }
        if (m_volBitmap.size() > bm.size)
            m_volBitmap.resize((size_t)bm.size);
    }
    progress(100);
    m_indexed = true;
    return true;
}

bool CNtfsFileSystem::IsClusterInUse(ULONGLONG lcn) const
{
    if (lcn / 8 >= m_volBitmap.size())
        return true; // unknown - assume the worst
    return (m_volBitmap[(size_t)(lcn / 8)] >> (lcn % 8)) & 1;
}

FsEntry CNtfsFileSystem::MakeEntry(ULONGLONG recNo) const
{
    const Rec& r = m_recs[(size_t)recNo];
    FsEntry e;
    e.name = r.name.c_str();
    e.isDirectory = (r.flags & kDir) != 0;
    e.isDeleted = (r.flags & kInUse) == 0;
    e.size = e.isDirectory ? 0 : r.size;
    e.created = r.created;
    e.modified = r.modified;
    e.attributes = r.attributes;
    e.id = recNo;
    e.resident = (r.flags & kResident) != 0;

    ULONGLONG off = recNo * m_recordSize, start = 0;
    for (const auto& seg : m_mftSegments)
    {
        ULONGLONG bytes = seg.second * SectorSize();
        if (off < start + bytes)
        {
            e.metaLBA = seg.first + (off - start) / SectorSize();
            break;
        }
        start += bytes;
    }
    return e;
}

void CNtfsFileSystem::Assess(ULONGLONG recNo, FsEntry& e)
{
    const Rec& r = m_recs[(size_t)recNo];
    if (e.isDirectory)
    {
        e.recover = Recoverability::NotApplicable;
        e.recoverNote = _T("Deleted folder - open it to see (and recover) the files that were inside.");
        return;
    }
    if (r.flags & kEncrypted)
    {
        e.recover = Recoverability::None;
        e.recoverNote = _T("EFS-encrypted file - the raw clusters can't be decrypted outside Windows.");
        return;
    }
    if (r.flags & kCompressed)
    {
        e.recover = Recoverability::Poor;
        e.recoverNote = _T("NTFS-compressed file - its clusters hold compressed data, which this tool does not decompress.");
        return;
    }

    DataInfo d;
    if (!ReadData(recNo, d) || !d.found)
    {
        e.recover = Recoverability::None;
        e.recoverNote = _T("The record no longer describes where the file's data was stored.");
        return;
    }
    if (d.resident)
    {
        e.resident = true;
        e.residentData = d.residentData;
        e.size = d.size;
        e.recover = Recoverability::Excellent;
        e.recoverNote = _T("Small file stored entirely inside its MFT record - it can be recovered exactly.");
        return;
    }
    if (d.size == 0)
    {
        e.recover = Recoverability::None;
        e.recoverNote = _T("Empty file - there is no data to recover.");
        return;
    }

    e.runs = d.runs;
    ULONGLONG total = 0, free = 0;
    for (const ClusterRun& run : d.runs)
    {
        if (run.lcn < 0)
            continue;
        if (!e.firstLBA)
            e.firstLBA = m_volumeStartLBA + (ULONGLONG)run.lcn * m_spc;
        for (ULONGLONG k = 0; k < run.length; ++k, ++total)
            free += IsClusterInUse((ULONGLONG)run.lcn + k) ? 0 : 1;
    }
    e.allocatedClusters = total;

    if (m_volBitmap.empty())
    {
        e.recover = Recoverability::Partial;
        e.recoverNote = _T("The volume $Bitmap could not be read, so cluster reuse can't be checked.");
    }
    else if (total == 0)
    {
        e.recover = Recoverability::None;
        e.recoverNote = _T("No data runs were recorded for this file.");
    }
    else if (free == total)
    {
        e.recover = Recoverability::Good;
        e.recoverNote.Format(_T("All %llu cluster(s) in %u data run(s) are still free - the data should be intact."),
                             total, (UINT)d.runs.size());
    }
    else if (free == 0)
    {
        e.recover = Recoverability::None;
        e.recoverNote = _T("All of the file's clusters have been reused by other files - the data has been overwritten.");
    }
    else
    {
        e.recover = (free * 2 >= total) ? Recoverability::Partial : Recoverability::Poor;
        e.recoverNote.Format(_T("%llu of %llu cluster(s) have been reused by other files - parts of the output ")
                             _T("will contain other data."), total - free, total);
    }
}

FsEntry CNtfsFileSystem::Root() const
{
    FsEntry root;
    root.name = _T("\\");
    root.isDirectory = true;
    root.id = kRootRecord;
    if (m_indexed)
        root = MakeEntry(kRootRecord), root.name = _T("\\");
    return root;
}

bool CNtfsFileSystem::ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut)
{
    if (!m_indexed)
    {
        errorOut = _T("The MFT has not been scanned yet.");
        return false;
    }
    auto it = m_children.find(dir.id);
    if (it == m_children.end())
        return true;
    for (ULONGLONG rec : it->second)
    {
        FsEntry e = MakeEntry(rec);
        if (e.isDeleted)
            Assess(rec, e);
        out.push_back(e);
    }
    return true;
}

void CNtfsFileSystem::ListOrphans(std::vector<FsEntry>& out)
{
    for (ULONGLONG rec : m_orphans)
    {
        FsEntry e = MakeEntry(rec);
        if (e.isDeleted)
            Assess(rec, e);
        out.push_back(e);
    }
}

CString CNtfsFileSystem::Recover(const FsEntry& e, const CString& outputPath, ULONGLONG& written)
{
    written = 0;
    if (e.isDirectory)
        return _T("Folders can't be recovered as a whole - open the folder and recover the files inside it.");

    DataInfo d;
    if (!ReadData(e.id, d) || !d.found)
        return _T("The record no longer describes where the file's data was stored.");
    if (d.encrypted)
        return _T("EFS-encrypted files can't be recovered from raw clusters.");
    if (d.compressed)
        return _T("NTFS-compressed files can't be recovered by this tool (the clusters hold compressed data).");

    HANDLE hOut = CreateFile(outputPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE)
        return _T("Could not create the output file.");

    CString err;
    if (d.resident)
    {
        DWORD w = 0;
        if (!WriteFile(hOut, d.residentData.data(), (DWORD)d.residentData.size(), &w, nullptr))
            err = _T("Write error while saving the recovered file.");
        written = w;
    }
    else
        ReadRuns(d.runs, 0, m_volumeStartLBA, d.size, hOut, written, err);
    CloseHandle(hOut);
    return err;
}

void CNtfsFileSystem::BootRecordFields(std::vector<FieldRow>& out) const
{
    const BYTE* s = m_boot.data();
    CString v;
    LPCTSTR g = _T("Boot Sector");
    v.Format(_T("%02X %02X %02X"), s[0], s[1], s[2]);  AddField(out, g, _T("Jump instruction"), v, 0x00);
    AddField(out, g, _T("OEM ID"), _T("NTFS"), 0x03);

    g = _T("BIOS Parameter Block");
    v.Format(_T("%u"), ReadLE16(s + 0x0B));             AddField(out, g, _T("Bytes per sector"), v, 0x0B);
    v.Format(_T("%llu (%s per cluster)"), m_spc, FormatSize(m_clusterBytes).GetString());
                                                        AddField(out, g, _T("Sectors per cluster"), v, 0x0D);
    v.Format(_T("0x%02X"), s[0x15]);                    AddField(out, g, _T("Media descriptor"), v, 0x15);
    v.Format(_T("%u"), ReadLE16(s + 0x18));             AddField(out, g, _T("Sectors per track"), v, 0x18);
    v.Format(_T("%u"), ReadLE16(s + 0x1A));             AddField(out, g, _T("Number of heads"), v, 0x1A);
    v.Format(_T("%u"), ReadLE32(s + 0x1C));             AddField(out, g, _T("Hidden sectors"), v, 0x1C,
                                                                 _T("Sectors before this volume on the disk"));

    g = _T("NTFS Extended BPB");
    v.Format(_T("%llu (%s)"), ReadLE64(s + 0x28), FormatSize(ReadLE64(s + 0x28) * SectorSize()).GetString());
                                                        AddField(out, g, _T("Total sectors"), v, 0x28);
    v.Format(_T("%llu (LBA %llu)"), m_mftLcn, m_volumeStartLBA + m_mftLcn * m_spc);
                                                        AddField(out, g, _T("$MFT cluster"), v, 0x30);
    v.Format(_T("%llu (LBA %llu)"), m_mftMirrLcn, m_volumeStartLBA + m_mftMirrLcn * m_spc);
                                                        AddField(out, g, _T("$MFTMirr cluster"), v, 0x38);
    v.Format(_T("%d -> %u bytes"), (int)(signed char)s[0x40], m_recordSize);
                                                        AddField(out, g, _T("Clusters per MFT record"), v, 0x40,
                                                                 _T("Negative n means 2^-n bytes"));
    signed char cpi = (signed char)s[0x44];
    v.Format(_T("%d -> %llu bytes"), (int)cpi, cpi > 0 ? cpi * m_clusterBytes : (1ULL << (-cpi)));
                                                        AddField(out, g, _T("Clusters per index record"), v, 0x44);
    v.Format(_T("%016llX"), ReadLE64(s + 0x48));        AddField(out, g, _T("Volume serial number"), v, 0x48);
    v.Format(_T("%02X %02X"), s[510], s[511]);          AddField(out, g, _T("Boot signature"), v, 0x1FE);
}

void CNtfsFileSystem::AllocationFields(std::vector<FieldRow>& out)
{
    CString v;
    LPCTSTR g = _T("Master File Table");
    v.Format(_T("%llu"), m_volumeStartLBA + m_mftLcn * m_spc); AddField(out, g, _T("$MFT start LBA"), v);
    v.Format(_T("%llu"), m_volumeStartLBA + m_mftMirrLcn * m_spc); AddField(out, g, _T("$MFTMirr start LBA"), v);
    v.Format(_T("%u bytes"), m_recordSize);                   AddField(out, g, _T("Record size"), v);
    v.Format(_T("%s"), FormatSize(m_mftBytes).GetString());   AddField(out, g, _T("$MFT size"), v);
    v.Format(_T("%llu"), m_mftBytes / m_recordSize);          AddField(out, g, _T("Records"), v);
    v.Format(_T("%u"), (UINT)m_mftRuns.size());               AddField(out, g, _T("Fragments (data runs)"), v);

    g = _T("Scan Results");
    if (!m_indexed)
    {
        AddField(out, g, _T("Status"), _T("Not scanned yet - expand File System to scan the MFT."));
        return;
    }
    v.Format(_T("%llu"), m_inUse);                            AddField(out, g, _T("Records in use"), v);
    v.Format(_T("%llu"), m_deleted);                          AddField(out, g, _T("Deleted records with a name"), v, -1,
                                                                       _T("Candidates for recovery"));
    v.Format(_T("%llu"), m_dirs);                             AddField(out, g, _T("Folders (live + deleted)"), v);
    v.Format(_T("%u"), (UINT)m_orphans.size());               AddField(out, g, _T("Orphaned entries"), v, -1,
                                                                       _T("Entries whose parent folder no longer exists"));

    g = _T("Cluster Usage ($Bitmap)");
    if (m_volBitmap.empty())
        AddField(out, g, _T("Status"), _T("Could not read $Bitmap"));
    else
    {
        ULONGLONG used = 0;
        for (ULONGLONG c = 0; c < m_totalClusters && c / 8 < m_volBitmap.size(); ++c)
            used += (m_volBitmap[(size_t)(c / 8)] >> (c % 8)) & 1;
        v.Format(_T("%llu"), m_totalClusters);                AddField(out, g, _T("Clusters"), v);
        v.Format(_T("%s"), FormatSize(m_clusterBytes).GetString()); AddField(out, g, _T("Cluster size"), v);
        v.Format(_T("%llu (%s)"), used, FormatSize(used * m_clusterBytes).GetString()); AddField(out, g, _T("Used"), v);
        v.Format(_T("%llu (%s)"), m_totalClusters - used, FormatSize((m_totalClusters - used) * m_clusterBytes).GetString());
        AddField(out, g, _T("Free"), v, -1, _T("Deleted files can only be recovered from clusters that are still free"));
    }
}

void CNtfsFileSystem::EntryFields(const FsEntry& e, std::vector<FieldRow>& out) const
{
    IFileSystem::EntryFields(e, out);
    if (e.id < m_recs.size())
    {
        const Rec& r = m_recs[(size_t)e.id];
        CString v;
        v.Format(_T("%llu (sequence %u)"), r.parent, r.parentSeq);
        AddField(out, _T("NTFS"), _T("Parent record"), v);
        v.Format(_T("%u"), r.seq);
        AddField(out, _T("NTFS"), _T("Record sequence"), v);
        if (r.flags & kCompressed) AddField(out, _T("NTFS"), _T("Compressed"), _T("Yes"));
        if (r.flags & kEncrypted)  AddField(out, _T("NTFS"), _T("Encrypted (EFS)"), _T("Yes"));
    }
}
