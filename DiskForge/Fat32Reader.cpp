// Fat32Reader.cpp
#include "pch.h"
#include "Fat32Reader.h"

namespace
{
    inline WORD  ReadU16(const BYTE* p) { WORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline DWORD ReadU32(const BYTE* p) { DWORD v; memcpy(&v, p, sizeof(v)); return v; }

    BYTE ShortNameChecksum(const BYTE* nameBytes11)
    {
        BYTE sum = 0;
        for (int k = 0; k < 11; ++k)
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + nameBytes11[k];
        return sum;
    }
}

FatVolumeInfo CFat32Reader::OpenVolume(CDiskManager& dm, ULONGLONG volumeStartLBA, DWORD sectorSize)
{
    FatVolumeInfo v;
    v.volumeStartLBA = volumeStartLBA;
    v.bytesPerSector = sectorSize;

    std::vector<BYTE> sec;
    if (!dm.ReadSector(volumeStartLBA, sec) || sec.size() < 512)
    {
        v.errorMessage = _T("Could not read the volume's boot sector.");
        return v;
    }

    if (sec[510] != 0x55 || sec[511] != 0xAA)
    {
        v.errorMessage = _T("No valid boot signature (0x55AA) found at that location.");
        return v;
    }
    if (memcmp(&sec[82], "FAT32   ", 8) != 0)
    {
        v.errorMessage = _T("This does not look like a FAT32 volume (only FAT32 is supported by this reader).");
        return v;
    }

    WORD  bps = ReadU16(&sec[11]);
    BYTE  spc = sec[13];
    WORD  reservedSectors = ReadU16(&sec[14]);
    BYTE  numFats = sec[16];
    DWORD sectorsPerFat32 = ReadU32(&sec[36]);
    DWORD rootCluster = ReadU32(&sec[44]);

    if (bps == 0 || (bps & (bps - 1)) != 0 || bps > 4096)
    {
        v.errorMessage = _T("Implausible bytes-per-sector value in the boot sector.");
        return v;
    }
    if (spc == 0 || (spc & (spc - 1)) != 0)
    {
        v.errorMessage = _T("Implausible sectors-per-cluster value in the boot sector.");
        return v;
    }
    if (numFats == 0 || numFats > 4 || sectorsPerFat32 == 0)
    {
        v.errorMessage = _T("Implausible FAT layout in the boot sector.");
        return v;
    }

    v.bytesPerSector    = bps;
    v.sectorsPerCluster = spc;
    v.numFats           = numFats;
    v.sectorsPerFat      = sectorsPerFat32;
    v.rootCluster        = (rootCluster >= 2) ? rootCluster : 2;
    v.fatStartLBA         = volumeStartLBA + reservedSectors;
    v.dataStartLBA         = v.fatStartLBA + (ULONGLONG)numFats * sectorsPerFat32;

    char label[12] = {};
    memcpy(label, &sec[71], 11);
    CString lbl(label);
    lbl.TrimRight(_T(' '));
    v.volumeLabel = lbl;

    DWORD totSec32 = ReadU32(&sec[32]);
    ULONGLONG systemSectors = (ULONGLONG)reservedSectors + (ULONGLONG)numFats * sectorsPerFat32;
    if (totSec32 > systemSectors)
        v.totalClusters = (totSec32 - systemSectors) / spc;

    v.valid = true;
    return v;
}

bool CFat32Reader::ReadFatEntry(CDiskManager& dm, const FatVolumeInfo& vol, DWORD clusterNum, DWORD& nextOut)
{
    ULONGLONG byteOffset = (ULONGLONG)clusterNum * 4;
    ULONGLONG sectorOffset = byteOffset / vol.bytesPerSector;
    DWORD inSectorOffset = (DWORD)(byteOffset % vol.bytesPerSector);

    std::vector<BYTE> sec;
    if (!dm.ReadSector(vol.fatStartLBA + sectorOffset, sec) || sec.size() < (size_t)inSectorOffset + 4)
        return false;

    nextOut = ReadU32(&sec[inSectorOffset]) & 0x0FFFFFFF;
    return true;
}

std::vector<DWORD> CFat32Reader::GetClusterChain(CDiskManager& dm, const FatVolumeInfo& vol,
                                                  DWORD startCluster, ULONGLONG maxClusters)
{
    std::vector<DWORD> chain;
    DWORD cur = startCluster;
    for (ULONGLONG i = 0; i < maxClusters; ++i)
    {
        if (cur < 2) break;
        chain.push_back(cur);

        DWORD next = 0;
        if (!ReadFatEntry(dm, vol, cur, next)) break;
        if (next >= 0x0FFFFFF8) break;      // End of chain
        if (next == 0 || next == 0x0FFFFFF7) break; // Free or bad cluster - stop rather than guess
        if (next == cur) break;             // Corrupt self-referencing loop guard

        cur = next;
    }
    return chain;
}

bool CFat32Reader::ReadCluster(CDiskManager& dm, const FatVolumeInfo& vol, DWORD clusterNum, std::vector<BYTE>& out)
{
    if (clusterNum < 2)
        return false;

    ULONGLONG lba = vol.dataStartLBA + (ULONGLONG)(clusterNum - 2) * vol.sectorsPerCluster;
    out.clear();
    out.reserve((size_t)vol.sectorsPerCluster * vol.bytesPerSector);

    for (DWORD i = 0; i < vol.sectorsPerCluster; ++i)
    {
        std::vector<BYTE> sec;
        if (!dm.ReadSector(lba + i, sec))
            return false;
        out.insert(out.end(), sec.begin(), sec.end());
    }
    return true;
}

std::vector<FatDirEntry> CFat32Reader::ReadDirectory(CDiskManager& dm, const FatVolumeInfo& vol, DWORD dirCluster)
{
    std::vector<FatDirEntry> result;
    if (!vol.valid)
        return result;

    std::vector<DWORD> chain = GetClusterChain(dm, vol, dirCluster);
    if (chain.empty() && dirCluster >= 2)
        chain.push_back(dirCluster);

    std::vector<BYTE> buf;
    for (DWORD c : chain)
    {
        std::vector<BYTE> clusterData;
        if (ReadCluster(dm, vol, c, clusterData))
            buf.insert(buf.end(), clusterData.begin(), clusterData.end());
    }

    static const size_t kMaxNameChars = 260;
    WCHAR lfnBuf[kMaxNameChars + 1] = {};
    bool haveLfn = false;
    BYTE lfnChecksum = 0;

    size_t entryCount = buf.size() / 32;
    for (size_t i = 0; i < entryCount; ++i)
    {
        const BYTE* e = &buf[i * 32];

        if (e[0] == 0x00)
            break; // End of directory marker

        BYTE attr = e[11];

        if (attr == 0x0F)
        {
            // Long File Name (LFN) entry
            if (e[0] == 0xE5)
            {
                haveLfn = false; // A deleted LFN piece - don't trust a partial reconstruction
                continue;
            }

            BYTE seq = e[0] & 0x1F;
            bool isLast = (e[0] & 0x40) != 0;
            if (isLast)
            {
                ZeroMemory(lfnBuf, sizeof(lfnBuf));
                haveLfn = true;
                lfnChecksum = e[13];
            }
            if (seq == 0 || seq > 20 || !haveLfn || e[13] != lfnChecksum)
            {
                haveLfn = false;
                continue;
            }

            int base = (seq - 1) * 13;
            const BYTE* chars1 = e + 1;   // 5 chars
            const BYTE* chars2 = e + 14;  // 6 chars
            const BYTE* chars3 = e + 28;  // 2 chars
            for (int k = 0; k < 5 && (size_t)(base + k) < kMaxNameChars; ++k)      memcpy(&lfnBuf[base + k], chars1 + k * 2, 2);
            for (int k = 0; k < 6 && (size_t)(base + 5 + k) < kMaxNameChars; ++k)  memcpy(&lfnBuf[base + 5 + k], chars2 + k * 2, 2);
            for (int k = 0; k < 2 && (size_t)(base + 11 + k) < kMaxNameChars; ++k) memcpy(&lfnBuf[base + 11 + k], chars3 + k * 2, 2);
            continue;
        }

        if (attr == 0x08)
        {
            haveLfn = false; // Volume label entry - not a navigable file/dir
            continue;
        }

        bool deleted = (e[0] == 0xE5);
        bool isDir = (attr & 0x10) != 0;

        FatDirEntry fd;
        fd.isDeleted = deleted;
        fd.isDirectory = isDir;

        WORD clusHi = ReadU16(e + 20);
        WORD clusLo = ReadU16(e + 26);
        fd.firstCluster = (DWORD(clusHi) << 16) | clusLo;
        fd.fileSize = ReadU32(e + 28);

        // For a deleted entry we can't verify the LFN checksum against the
        // now-corrupted first byte, so just trust adjacency instead - the
        // same practical compromise most single-file undelete tools make.
        bool useLfn = haveLfn && (deleted || ShortNameChecksum(e) == lfnChecksum);

        if (useLfn)
        {
            size_t len = 0;
            while (len < kMaxNameChars && lfnBuf[len] != 0 && lfnBuf[len] != 0xFFFF)
                ++len;
            fd.name = CString(lfnBuf, (int)len);
        }
        else
        {
            char shortName[9] = {}, ext[4] = {};
            memcpy(shortName, e, 8);
            memcpy(ext, e + 8, 3);
            CString baseName(shortName); baseName.TrimRight(_T(' '));
            CString extName(ext); extName.TrimRight(_T(' '));
            if (deleted && baseName.GetLength() > 0)
                baseName.SetAt(0, _T('_')); // First byte was overwritten by the OS on delete - unrecoverable
            fd.name = extName.IsEmpty() ? baseName : (baseName + _T(".") + extName);
        }

        haveLfn = false;

        if (fd.name == _T(".") || fd.name == _T(".."))
            continue; // Self/parent references, not real navigable children

        result.push_back(fd);
    }

    return result;
}

CString CFat32Reader::RecoverFile(CDiskManager& dm, const FatVolumeInfo& vol, DWORD startCluster,
                                   ULONGLONG fileSize, bool assumeContiguous, const CString& outputPath,
                                   ULONGLONG& bytesWrittenOut)
{
    bytesWrittenOut = 0;

    if (!vol.valid)
        return _T("Volume is not open.");
    if (startCluster < 2)
        return _T("This entry has no valid starting cluster - it may already be fully overwritten.");

    HANDLE hOut = CreateFile(outputPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE)
        return _T("Could not create the output file.");

    ULONGLONG clusterBytes = (ULONGLONG)vol.sectorsPerCluster * vol.bytesPerSector;
    ULONGLONG clustersNeeded = (fileSize == 0) ? 0 : ((fileSize + clusterBytes - 1) / clusterBytes);

    CString err;
    ULONGLONG remaining = fileSize;

    auto writeCluster = [&](DWORD cluster) -> bool
    {
        std::vector<BYTE> data;
        if (!ReadCluster(dm, vol, cluster, data))
        {
            err = _T("Read error while recovering.");
            return false;
        }
        DWORD toWrite = (DWORD)std::min<ULONGLONG>(remaining, data.size());
        DWORD written = 0;
        if (!WriteFile(hOut, data.data(), toWrite, &written, nullptr) || written != toWrite)
        {
            err = _T("Write error while saving the recovered file.");
            return false;
        }
        bytesWrittenOut += written;
        remaining -= written;
        return true;
    };

    if (assumeContiguous)
    {
        for (ULONGLONG i = 0; i < clustersNeeded && remaining > 0; ++i)
        {
            DWORD cluster = startCluster + (DWORD)i;
            if (vol.totalClusters > 0 && (ULONGLONG)(cluster - 2) >= vol.totalClusters)
            {
                err = _T("Reached the end of the volume before recovering all the data - the file may be ")
                      _T("larger than the remaining space, or (more likely) fragmented, which this contiguous-")
                      _T("assumption recovery can't follow.");
                break;
            }
            if (!writeCluster(cluster))
                break;
        }
    }
    else
    {
        std::vector<DWORD> chain = GetClusterChain(dm, vol, startCluster);
        for (size_t i = 0; i < chain.size() && remaining > 0; ++i)
        {
            if (!writeCluster(chain[i]))
                break;
        }
        if (err.IsEmpty() && remaining > 0)
            err = _T("The FAT chain ended before recovering all the data - the file may be truncated on disk.");
    }

    CloseHandle(hOut);
    return err;
}
