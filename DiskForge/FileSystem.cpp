// FileSystem.cpp - Detection and shared helpers for the System Browser's
// filesystem parsers.
#include "pch.h"
#include "FileSystem.h"
#include "FatFileSystem.h"
#include "ExFatFileSystem.h"
#include "NtfsFileSystem.h"

namespace
{
    // Returns the FAT variant implied by a BPB (by cluster count, which is how
    // the FAT specification defines it - the type string is only a label), or
    // FsKind::Unknown when the BPB isn't plausible.
    FsKind FatKindFromBpb(const std::vector<BYTE>& s)
    {
        if (s.size() < 512 || s[510] != 0x55 || s[511] != 0xAA)
            return FsKind::Unknown;
        if (!(s[0] == 0xEB || s[0] == 0xE9))
            return FsKind::Unknown; // FAT boot sectors start with a jump instruction

        WORD  bps      = ReadLE16(&s[11]);
        BYTE  spc      = s[13];
        WORD  reserved = ReadLE16(&s[14]);
        BYTE  nFats    = s[16];
        WORD  rootEnt  = ReadLE16(&s[17]);
        WORD  tot16    = ReadLE16(&s[19]);
        WORD  fat16sz  = ReadLE16(&s[22]);
        DWORD tot32    = ReadLE32(&s[32]);
        DWORD fat32sz  = ReadLE32(&s[36]);

        if (bps < 512 || bps > 4096 || (bps & (bps - 1)) != 0) return FsKind::Unknown;
        if (spc == 0 || (spc & (spc - 1)) != 0)                 return FsKind::Unknown;
        if (reserved == 0 || nFats == 0 || nFats > 4)           return FsKind::Unknown;

        DWORD fatSz  = fat16sz ? fat16sz : fat32sz;
        DWORD totSec = tot16 ? tot16 : tot32;
        if (fatSz == 0 || totSec == 0) return FsKind::Unknown;

        DWORD rootDirSectors = ((DWORD)rootEnt * 32 + (bps - 1)) / bps;
        ULONGLONG meta = (ULONGLONG)reserved + (ULONGLONG)nFats * fatSz + rootDirSectors;
        if (meta >= totSec) return FsKind::Unknown;

        ULONGLONG clusters = (totSec - meta) / spc;
        if (clusters < 4085)  return FsKind::FAT12;
        if (clusters < 65525) return FsKind::FAT16;
        return FsKind::FAT32;
    }
}

CString IFileSystem::Identify(const std::vector<BYTE>& s)
{
    if (s.size() < 512)
        return CString();
    if (memcmp(&s[3], "NTFS    ", 8) == 0)
        return _T("NTFS");
    if (memcmp(&s[3], "EXFAT   ", 8) == 0)
        return _T("exFAT");
    switch (FatKindFromBpb(s))
    {
    case FsKind::FAT12: return _T("FAT12");
    case FsKind::FAT16: return _T("FAT16");
    case FsKind::FAT32: return _T("FAT32");
    default:            return CString();
    }
}

std::shared_ptr<IFileSystem> IFileSystem::Detect(const CString& devicePath, ULONGLONG volumeStartLBA,
                                                 ULONGLONG volumeSectors, CString& errorOut)
{
    CDiskManager dm;
    if (!dm.OpenDrive(devicePath))
    {
        errorOut = dm.GetLastError();
        return nullptr;
    }
    std::vector<BYTE> boot;
    if (!dm.ReadSector(volumeStartLBA, boot))
    {
        errorOut = _T("Could not read the boot record: ") + dm.GetLastError();
        return nullptr;
    }
    dm.CloseDrive();

    std::shared_ptr<IFileSystem> fs;
    CString kind = Identify(boot);
    if (kind == _T("NTFS"))
        fs = std::make_shared<CNtfsFileSystem>();
    else if (kind == _T("exFAT"))
        fs = std::make_shared<CExFatFileSystem>();
    else if (!kind.IsEmpty())
        fs = std::make_shared<CFatFileSystem>();
    else
    {
        errorOut = _T("No FAT12/16/32, exFAT or NTFS boot record was found at the start of this partition ")
                   _T("(it may be unformatted, encrypted such as BitLocker, or use another filesystem).");
        return nullptr;
    }

    if (!fs->Init(devicePath, volumeStartLBA, volumeSectors, errorOut))
        return nullptr;
    return fs;
}

bool IFileSystem::Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors,
                       CString& errorOut)
{
    if (!m_dm.OpenDrive(devicePath))
    {
        errorOut = m_dm.GetLastError();
        return false;
    }
    m_volumeStartLBA = volumeStartLBA;
    m_volumeSectors = volumeSectors;
    return true;
}

CString IFileSystem::KindName() const
{
    switch (Kind())
    {
    case FsKind::FAT12: return _T("FAT12");
    case FsKind::FAT16: return _T("FAT16");
    case FsKind::FAT32: return _T("FAT32");
    case FsKind::ExFAT: return _T("exFAT");
    case FsKind::NTFS:  return _T("NTFS");
    default:            return _T("Unknown");
    }
}

bool IFileSystem::ReadVolumeSectors(ULONGLONG relSector, DWORD count, std::vector<BYTE>& out)
{
    if (m_volumeSectors && (relSector >= m_volumeSectors || count > m_volumeSectors - relSector))
        return false;
    return m_dm.ReadSectors(m_volumeStartLBA + relSector, count, out);
}

bool IFileSystem::ReadRuns(const std::vector<ClusterRun>& runs, ULONGLONG clusterBase, ULONGLONG clusterStartLBA,
                           ULONGLONG fileSize, HANDLE hOut, ULONGLONG& written, CString& err)
{
    const DWORD secSize = m_dm.GetBytesPerSector();
    const DWORD spc = (DWORD)(m_clusterBytes / secSize);
    const DWORD kChunkClusters = std::max<DWORD>(1, (4u * 1024 * 1024) / (DWORD)m_clusterBytes); // ~4 MB reads

    ULONGLONG remaining = fileSize;
    std::vector<BYTE> zeros;

    for (const ClusterRun& run : runs)
    {
        for (ULONGLONG done = 0; done < run.length && remaining > 0; )
        {
            DWORD n = (DWORD)std::min<ULONGLONG>(kChunkClusters, run.length - done);
            const BYTE* data = nullptr;
            std::vector<BYTE> buf;
            size_t avail = (size_t)n * (size_t)m_clusterBytes;

            if (run.lcn < 0)
            {
                zeros.assign(avail, 0); // sparse run - reads as zeros
                data = zeros.data();
            }
            else
            {
                ULONGLONG cluster = (ULONGLONG)run.lcn + done;
                if (cluster < clusterBase)
                {
                    err = _T("The file's cluster map points outside the data area.");
                    return false;
                }
                ULONGLONG lba = clusterStartLBA + (cluster - clusterBase) * spc;
                if (lba + (ULONGLONG)n * spc > m_volumeStartLBA + m_volumeSectors)
                {
                    err = _T("The file's cluster map runs past the end of the volume.");
                    return false;
                }
                if (!m_dm.ReadSectors(lba, n * spc, buf))
                {
                    err = _T("Read error while recovering: ") + m_dm.GetLastError();
                    return false;
                }
                data = buf.data();
            }

            DWORD toWrite = (DWORD)std::min<ULONGLONG>(remaining, avail);
            DWORD w = 0;
            if (!WriteFile(hOut, data, toWrite, &w, nullptr) || w != toWrite)
            {
                err = _T("Write error while saving the recovered file.");
                return false;
            }
            written += w;
            remaining -= w;
            done += n;
        }
        if (remaining == 0)
            break;
    }
    if (remaining > 0)
        err = _T("The recorded clusters ended before the file's full size was reached - the output is truncated.");
    return true;
}

void IFileSystem::AddField(std::vector<FieldRow>& out, LPCTSTR group, LPCTSTR name, const CString& value,
                           int offset, LPCTSTR description)
{
    FieldRow r;
    r.group = group;
    r.name = name;
    r.value = value;
    if (offset >= 0)
        r.offset.Format(_T("0x%03X"), offset);
    if (description)
        r.description = description;
    out.push_back(r);
}

void IFileSystem::EntryFields(const FsEntry& e, std::vector<FieldRow>& out) const
{
    LPCTSTR g = _T("Entry");
    AddField(out, g, _T("Name"), e.name);
    AddField(out, g, _T("Type"), e.isDirectory ? _T("Folder") : _T("File"));
    AddField(out, g, _T("State"), e.isDeleted ? _T("Deleted") : _T("Live"));
    CString v;
    v.Format(_T("%llu bytes (%s)"), e.size, FormatSize(e.size).GetString());
    AddField(out, g, _T("Size"), v);
    AddField(out, g, _T("Created"), FormatFileTime(e.created));
    AddField(out, g, _T("Modified"), FormatFileTime(e.modified));
    CString attrs;
    if (e.attributes & FILE_ATTRIBUTE_READONLY)  attrs += _T("R");
    if (e.attributes & FILE_ATTRIBUTE_HIDDEN)    attrs += _T("H");
    if (e.attributes & FILE_ATTRIBUTE_SYSTEM)    attrs += _T("S");
    if (e.attributes & FILE_ATTRIBUTE_ARCHIVE)   attrs += _T("A");
    if (e.attributes & FILE_ATTRIBUTE_COMPRESSED) attrs += _T("C");
    if (e.attributes & FILE_ATTRIBUTE_ENCRYPTED) attrs += _T("E");
    if (e.attributes & FILE_ATTRIBUTE_SPARSE_FILE) attrs += _T("P");
    AddField(out, g, _T("Attributes"), attrs.IsEmpty() ? CString(_T("-")) : attrs);

    LPCTSTR loc = _T("Location");
    v.Format(_T("%llu (0x%llX)"), e.id, e.id);
    AddField(out, loc, Kind() == FsKind::NTFS ? _T("MFT record") : _T("First cluster"), v);
    if (e.metaLBA)
    {
        v.Format(_T("%llu"), e.metaLBA);
        AddField(out, loc, Kind() == FsKind::NTFS ? _T("MFT record LBA") : _T("Directory entry LBA"), v);
    }
    if (e.firstLBA)
    {
        v.Format(_T("%llu"), e.firstLBA);
        AddField(out, loc, _T("First data LBA"), v);
    }
    if (e.resident)
        AddField(out, loc, _T("Data"), _T("Resident (stored inside the MFT record)"));
    else if (!e.runs.empty())
    {
        v.Format(_T("%u run(s), %llu cluster(s)"), (UINT)e.runs.size(), e.allocatedClusters);
        AddField(out, loc, _T("Data runs"), v);
    }

    if (e.isDeleted)
    {
        AddField(out, _T("Recovery"), _T("Recoverable"), RecoverabilityText(e.recover));
        AddField(out, _T("Recovery"), _T("Notes"), e.recoverNote);
    }
}

CString IFileSystem::RecoverabilityText(Recoverability r)
{
    switch (r)
    {
    case Recoverability::Excellent: return _T("Yes - excellent");
    case Recoverability::Good:      return _T("Yes - good");
    case Recoverability::Partial:   return _T("Partially");
    case Recoverability::Poor:      return _T("Unlikely");
    case Recoverability::None:      return _T("No");
    default:                        return CString();
    }
}

CString IFileSystem::FormatFileTime(const FILETIME& ft)
{
    if (ft.dwHighDateTime == 0 && ft.dwLowDateTime == 0)
        return CString();
    FILETIME local;
    SYSTEMTIME st;
    if (!FileTimeToLocalFileTime(&ft, &local) || !FileTimeToSystemTime(&local, &st))
        return CString();
    CString s;
    s.Format(_T("%04u-%02u-%02u %02u:%02u:%02u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return s;
}

CString IFileSystem::FormatSize(ULONGLONG bytes)
{
    CString s;
    if (bytes >= (1ULL << 40))      s.Format(_T("%.2f TB"), (double)bytes / (1ULL << 40));
    else if (bytes >= (1ULL << 30)) s.Format(_T("%.2f GB"), (double)bytes / (1ULL << 30));
    else if (bytes >= (1ULL << 20)) s.Format(_T("%.2f MB"), (double)bytes / (1ULL << 20));
    else if (bytes >= 1024)         s.Format(_T("%.1f KB"), (double)bytes / 1024);
    else                            s.Format(_T("%llu B"), bytes);
    return s;
}

CString IFileSystem::Hex(ULONGLONG v, int digits)
{
    CString s;
    if (digits > 0)
        s.Format(_T("0x%0*llX"), digits, v);
    else
        s.Format(_T("0x%llX"), v);
    return s;
}
