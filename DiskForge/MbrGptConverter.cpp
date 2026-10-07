// MbrGptConverter.cpp
#include "pch.h"
#include "MbrGptConverter.h"
#include "SystemDiskGuard.h"
#include "VolumeLockHelper.h"

#pragma comment(lib, "ole32.lib") // CoCreateGuid

namespace
{
    const ULONGLONG kGptReservedSectors = 34; // LBA 0 (protective MBR) + LBA 1 (header) + LBA 2-33 (128 x 128-byte entries)
    const DWORD     kNumPartitionEntries = 128;
    const DWORD     kPartitionEntrySize  = 128;
    const DWORD     kGptHeaderSize       = 92;

    // Common GPT partition type GUIDs (same values used for display in
    // PartitionParser.cpp's GptTypeName - repeated here since that function
    // returns strings, not GUIDs, and this module needs the actual GUID bytes).
    const GUID kGuidEfiSystem   = { 0xC12A7328, 0xF81F, 0x11D2, {0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B} };
    const GUID kGuidMsBasicData = { 0xEBD0A0A2, 0xB9E5, 0x4433, {0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7} };
    const GUID kGuidLinuxFs     = { 0x0FC63DAF, 0x8483, 0x4772, {0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4} };
    const GUID kGuidLinuxSwap   = { 0x0657FD6D, 0xA4AB, 0x43C4, {0x84,0xE5,0x09,0x33,0xC8,0x4B,0x4F,0x4F} };

    const BYTE BACKUP_MAGIC[8] = { 'D','S','V','B','A','K','1', 0 };

    inline void WriteU16(BYTE* p, WORD v)  { memcpy(p, &v, sizeof(v)); }
    inline void WriteU32(BYTE* p, DWORD v) { memcpy(p, &v, sizeof(v)); }
    inline void WriteU64(BYTE* p, ULONGLONG v) { memcpy(p, &v, sizeof(v)); }
    inline void WriteGuid(BYTE* p, const GUID& g)
    {
        WriteU32(p, g.Data1);
        WriteU16(p + 4, g.Data2);
        WriteU16(p + 6, g.Data3);
        memcpy(p + 8, g.Data4, 8);
    }
}

DWORD CMbrGptConverter::Crc32(const void* data, size_t length)
{
    static DWORD table[256];
    static bool tableBuilt = false;
    if (!tableBuilt)
    {
        for (DWORD i = 0; i < 256; ++i)
        {
            DWORD c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        tableBuilt = true;
    }

    const BYTE* p = static_cast<const BYTE*>(data);
    DWORD crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

GUID CMbrGptConverter::NewRandomGuid()
{
    GUID g = {};
    CoCreateGuid(&g);
    return g;
}

GUID CMbrGptConverter::MbrTypeToGptTypeGuid(BYTE mbrType)
{
    switch (mbrType)
    {
        case 0xEF: return kGuidEfiSystem;
        case 0x82: return kGuidLinuxSwap;
        case 0x83: return kGuidLinuxFs;
        default:   return kGuidMsBasicData; // Safe generic default for FAT/NTFS/exFAT/unknown data partitions
    }
}

ConvertPreflightResult CMbrGptConverter::Analyze(CDiskManager& dm)
{
    ConvertPreflightResult result;

    if (!dm.IsOpen())
    {
        result.blockingReason = _T("No drive is open.");
        return result;
    }

    result.totalSectors = dm.GetTotalSectors();
    result.sectorSize   = dm.GetBytesPerSector();

    if (result.totalSectors < kGptReservedSectors * 2)
    {
        result.blockingReason = _T("This disk is too small to hold GPT structures.");
        return result;
    }

    std::vector<PartitionEntry> allEntries;
    CString scheme, diskGuid, parseErr;
    if (!CPartitionParser::Parse(dm, allEntries, scheme, diskGuid, parseErr))
    {
        result.blockingReason = _T("Could not read the current partition table: ") + parseErr;
        return result;
    }

    if (scheme.Find(_T("GPT")) >= 0)
    {
        result.blockingReason = _T("This disk already uses GPT.");
        return result;
    }

    for (const auto& e : allEntries)
    {
        if (e.isLogical)
        {
            result.blockingReason =
                _T("This disk has logical partitions inside an extended partition. This converter ")
                _T("only supports disks with up to 4 simple primary partitions.");
            return result;
        }
    }

    if (allEntries.size() > 4)
    {
        result.blockingReason = _T("Unexpected number of primary partitions found.");
        return result;
    }

    ULONGLONG firstUsableLBA = kGptReservedSectors;
    ULONGLONG lastUsableLBA  = result.totalSectors - kGptReservedSectors; // inclusive

    for (const auto& e : allEntries)
    {
        if (e.sectorCount == 0)
            continue;

        ULONGLONG lastSectorOfPart = e.startLBA + e.sectorCount - 1;

        if (e.startLBA < firstUsableLBA)
        {
            result.blockingReason.Format(
                _T("A partition starts at LBA %llu, inside the space GPT needs at the start of the disk ")
                _T("(LBA 1-%llu). This converter does not move partitions, so this disk can't be safely converted."),
                e.startLBA, firstUsableLBA - 1);
            return result;
        }
        if (lastSectorOfPart > lastUsableLBA)
        {
            result.blockingReason.Format(
                _T("A partition extends to LBA %llu, leaving no room for the backup GPT structures needed ")
                _T("at the end of the disk (last %llu sectors / %s). Shrink that partition first and try again."),
                lastSectorOfPart, kGptReservedSectors,
                CPartitionParser::FormatSize(kGptReservedSectors * (ULONGLONG)result.sectorSize).GetString());
            return result;
        }
    }

    result.mbrPartitions = allEntries;
    result.driveIndex    = 0; // filled in by the caller, which knows the DriveInfo
    result.canConvert    = true;
    return result;
}

std::vector<BackupRegion> CMbrGptConverter::CaptureAffectedRegions(CDiskManager& dm, const ConvertPreflightResult& info)
{
    std::vector<BackupRegion> regions;
    if (!info.canConvert)
        return regions;

    // Region 1: sector 0 (the MBR itself, about to become the protective MBR).
    {
        BackupRegion r;
        r.startLBA = 0;
        std::vector<BYTE> sector0;
        if (dm.ReadSector(0, sector0))
        {
            r.data = sector0;
            regions.push_back(r);
        }
    }

    // Region 2: LBA 1..33 (currently whatever was there - usually unused, but
    // captured regardless so the restore is byte-exact either way).
    {
        BackupRegion r;
        r.startLBA = 1;
        for (ULONGLONG lba = 1; lba < kGptReservedSectors; ++lba)
        {
            std::vector<BYTE> sec;
            if (dm.ReadSector(lba, sec))
                r.data.insert(r.data.end(), sec.begin(), sec.end());
        }
        if (!r.data.empty())
            regions.push_back(r);
    }

    // Region 3: the last kGptReservedSectors sectors of the disk (where the
    // backup GPT header + array will be written).
    {
        BackupRegion r;
        ULONGLONG startLBA = info.totalSectors - kGptReservedSectors;
        r.startLBA = startLBA;
        for (ULONGLONG lba = startLBA; lba < info.totalSectors; ++lba)
        {
            std::vector<BYTE> sec;
            if (dm.ReadSector(lba, sec))
                r.data.insert(r.data.end(), sec.begin(), sec.end());
        }
        if (!r.data.empty())
            regions.push_back(r);
    }

    return regions;
}

bool CMbrGptConverter::SaveBackup(const CString& filePath, const CString& devicePath, DWORD sectorSize,
                                   const std::vector<BackupRegion>& regions, CString& errorOut)
{
    HANDLE hFile = CreateFile(filePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        errorOut = _T("Could not create the backup file.");
        return false;
    }

    auto writeAll = [&](const void* p, DWORD len) -> bool
    {
        DWORD written = 0;
        return WriteFile(hFile, p, len, &written, nullptr) && written == len;
    };

    bool ok = true;
    ok = ok && writeAll(BACKUP_MAGIC, sizeof(BACKUP_MAGIC));

    // Device path: length-prefixed UTF-16.
    DWORD pathLenChars = (DWORD)devicePath.GetLength();
    ok = ok && writeAll(&pathLenChars, sizeof(pathLenChars));
    ok = ok && writeAll(devicePath.GetString(), pathLenChars * sizeof(TCHAR));

    ok = ok && writeAll(&sectorSize, sizeof(sectorSize));

    DWORD regionCount = (DWORD)regions.size();
    ok = ok && writeAll(&regionCount, sizeof(regionCount));

    for (const auto& r : regions)
    {
        ULONGLONG startLBA = r.startLBA;
        DWORD dataLen = (DWORD)r.data.size();
        ok = ok && writeAll(&startLBA, sizeof(startLBA));
        ok = ok && writeAll(&dataLen, sizeof(dataLen));
        ok = ok && writeAll(r.data.data(), dataLen);
    }

    CloseHandle(hFile);

    if (!ok)
    {
        errorOut = _T("Failed while writing the backup file.");
        DeleteFile(filePath); // Don't leave a truncated, misleading backup behind
        return false;
    }
    return true;
}

bool CMbrGptConverter::LoadBackupAndRestore(const CString& filePath, const CString& devicePath, CString& errorOut)
{
    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        errorOut = _T("Could not open the backup file.");
        return false;
    }

    auto readAll = [&](void* p, DWORD len) -> bool
    {
        DWORD readBytes = 0;
        return ReadFile(hFile, p, len, &readBytes, nullptr) && readBytes == len;
    };

    BYTE magic[8] = {};
    if (!readAll(magic, sizeof(magic)) || memcmp(magic, BACKUP_MAGIC, sizeof(magic)) != 0)
    {
        CloseHandle(hFile);
        errorOut = _T("This doesn't look like a valid partition-table backup file.");
        return false;
    }

    DWORD pathLenChars = 0;
    if (!readAll(&pathLenChars, sizeof(pathLenChars)) || pathLenChars > 2048)
    {
        CloseHandle(hFile);
        errorOut = _T("Backup file is corrupt (bad device path length).");
        return false;
    }
    std::vector<TCHAR> pathBuf(pathLenChars + 1, 0);
    if (pathLenChars > 0 && !readAll(pathBuf.data(), pathLenChars * sizeof(TCHAR)))
    {
        CloseHandle(hFile);
        errorOut = _T("Backup file is corrupt (truncated device path).");
        return false;
    }
    CString originalDevicePath(pathBuf.data());

    DWORD sectorSize = 0;
    if (!readAll(&sectorSize, sizeof(sectorSize)) || sectorSize == 0 || sectorSize > 65536)
    {
        CloseHandle(hFile);
        errorOut = _T("Backup file is corrupt (bad sector size).");
        return false;
    }

    DWORD regionCount = 0;
    if (!readAll(&regionCount, sizeof(regionCount)) || regionCount > 100)
    {
        CloseHandle(hFile);
        errorOut = _T("Backup file is corrupt (bad region count).");
        return false;
    }

    std::vector<BackupRegion> regions;
    for (DWORD i = 0; i < regionCount; ++i)
    {
        BackupRegion r;
        ULONGLONG startLBA = 0;
        DWORD dataLen = 0;
        if (!readAll(&startLBA, sizeof(startLBA)) || !readAll(&dataLen, sizeof(dataLen)) || dataLen > 64 * 1024 * 1024)
        {
            CloseHandle(hFile);
            errorOut = _T("Backup file is corrupt (bad region header).");
            return false;
        }
        r.startLBA = startLBA;
        r.data.resize(dataLen);
        if (dataLen > 0 && !readAll(r.data.data(), dataLen))
        {
            CloseHandle(hFile);
            errorOut = _T("Backup file is corrupt (truncated region data).");
            return false;
        }
        regions.push_back(std::move(r));
    }
    CloseHandle(hFile);

    if (originalDevicePath.CompareNoCase(devicePath) != 0)
    {
        errorOut.Format(
            _T("This backup was made from %s, not the drive you selected (%s). Restoring it to the wrong ")
            _T("drive would corrupt that drive instead. Select the original drive and try again."),
            originalDevicePath.GetString(), devicePath.GetString());
        return false;
    }

    // Open the destination raw for writing (sector-aligned, unbuffered - same
    // discipline as DiskCopyEngine).
    UINT driveIndex = 0;
    {
        CString tail = devicePath;
        int pos = tail.ReverseFind(_T('\\'));
        if (pos >= 0) tail = tail.Mid(pos + 1);
        // tail looks like "PhysicalDriveN" - pull the trailing digits.
        int i = tail.GetLength();
        while (i > 0 && _istdigit(tail[i - 1])) --i;
        driveIndex = (i < tail.GetLength()) ? (UINT)_ttoi(tail.Mid(i)) : 0;
    }

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        errorOut = _T("Could not lock all volumes on this disk for exclusive access. Close any windows, ")
                   _T("apps, or drive letters using it and try again.");
        return false;
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        errorOut = _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
        return false;
    }

    bool ok = true;
    for (const auto& r : regions)
    {
        if (r.data.empty())
            continue;

        LPVOID buf = VirtualAlloc(nullptr, r.data.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buf)
        {
            errorOut = _T("Out of memory while restoring.");
            ok = false;
            break;
        }
        memcpy(buf, r.data.data(), r.data.size());

        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(r.startLBA) * sectorSize;
        SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN);

        DWORD written = 0;
        BOOL wok = WriteFile(hDrive, buf, (DWORD)r.data.size(), &written, nullptr);
        VirtualFree(buf, 0, MEM_RELEASE);

        if (!wok || written != r.data.size())
        {
            errorOut.Format(_T("Write error while restoring region at LBA %llu (Win32 error %u)."),
                              r.startLBA, ::GetLastError());
            ok = false;
            break;
        }
    }

    FlushFileBuffers(hDrive);
    CloseHandle(hDrive);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);

    if (ok)
    {
        // Ask Windows to re-read the (now-restored) partition table.
        HANDLE hUpdate = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, 0, nullptr);
        if (hUpdate != INVALID_HANDLE_VALUE)
        {
            DWORD dummy = 0;
            DeviceIoControl(hUpdate, IOCTL_DISK_UPDATE_PROPERTIES, nullptr, 0, nullptr, 0, &dummy, nullptr);
            CloseHandle(hUpdate);
        }
    }

    return ok;
}

CString CMbrGptConverter::ConvertMbrToGpt(const CString& devicePath, const ConvertPreflightResult& info)
{
    if (!info.canConvert)
        return _T("Internal error: attempted to convert a disk that Analyze() did not approve.");

    if (CSystemDiskGuard::IsProtectedDrive(info.driveIndex))
        return _T("Refusing to convert: this is the drive Windows is running from.");

    const DWORD sectorSize = info.sectorSize;
    const ULONGLONG totalSectors = info.totalSectors;
    const ULONGLONG lastLBA = totalSectors - 1;
    const ULONGLONG firstUsableLBA = kGptReservedSectors;
    const ULONGLONG lastUsableLBA  = totalSectors - kGptReservedSectors;

    // Layout at the end of the disk: the very last sector (lastLBA) holds the
    // backup GPT header; the 32 sectors immediately before it hold the backup
    // partition entry array.
    const ULONGLONG backupHeaderLBA = lastLBA;
    const ULONGLONG backupEntriesLBA = lastLBA - (kNumPartitionEntries * kPartitionEntrySize / sectorSize);

    // ---- Build the 128 x 128-byte partition entry array (shared by primary & backup) ----
    std::vector<BYTE> entryArray(kNumPartitionEntries * kPartitionEntrySize, 0);
    for (size_t i = 0; i < info.mbrPartitions.size(); ++i)
    {
        const PartitionEntry& src = info.mbrPartitions[i];
        if (src.sectorCount == 0)
            continue;

        BYTE* entry = &entryArray[i * kPartitionEntrySize];
        GUID typeGuid = MbrTypeToGptTypeGuid(src.mbrType);
        GUID uniqueGuid = NewRandomGuid();

        WriteGuid(entry + 0, typeGuid);
        WriteGuid(entry + 16, uniqueGuid);
        WriteU64(entry + 32, src.startLBA);
        WriteU64(entry + 40, src.startLBA + src.sectorCount - 1);
        WriteU64(entry + 48, 0); // Attributes
        // PartitionName (offset 56, 72 bytes / 36 UTF-16 chars) left blank - matches
        // how migrated MBR->GPT conversions commonly leave names empty.
    }
    DWORD entryArrayCrc = Crc32(entryArray.data(), entryArray.size());

    // ---- Disk GUID (shared by both headers) ----
    GUID diskGuid = NewRandomGuid();

    // ---- Build primary GPT header (LBA 1) ----
    std::vector<BYTE> primaryHeader(sectorSize, 0);
    {
        BYTE* h = primaryHeader.data();
        memcpy(h + 0, "EFI PART", 8);
        WriteU32(h + 8, 0x00010000);      // Revision 1.0
        WriteU32(h + 12, kGptHeaderSize); // HeaderSize
        WriteU32(h + 16, 0);              // HeaderCRC32 - filled in after zeroing this field for the calc
        WriteU32(h + 20, 0);              // Reserved
        WriteU64(h + 24, 1);              // MyLBA
        WriteU64(h + 32, lastLBA);        // AlternateLBA
        WriteU64(h + 40, firstUsableLBA); // FirstUsableLBA
        WriteU64(h + 48, lastUsableLBA - 1); // LastUsableLBA (inclusive)
        WriteGuid(h + 56, diskGuid);
        WriteU64(h + 72, 2);              // PartitionEntryLBA
        WriteU32(h + 80, kNumPartitionEntries);
        WriteU32(h + 84, kPartitionEntrySize);
        WriteU32(h + 88, entryArrayCrc);

        DWORD headerCrc = Crc32(h, kGptHeaderSize);
        WriteU32(h + 16, headerCrc);
    }

    // ---- Build backup GPT header (last LBA) ----
    std::vector<BYTE> backupHeader(sectorSize, 0);
    {
        BYTE* h = backupHeader.data();
        memcpy(h + 0, "EFI PART", 8);
        WriteU32(h + 8, 0x00010000);
        WriteU32(h + 12, kGptHeaderSize);
        WriteU32(h + 16, 0);
        WriteU32(h + 20, 0);
        WriteU64(h + 24, lastLBA);         // MyLBA
        WriteU64(h + 32, 1);               // AlternateLBA
        WriteU64(h + 40, firstUsableLBA);
        WriteU64(h + 48, lastUsableLBA - 1);
        WriteGuid(h + 56, diskGuid);
        WriteU64(h + 72, backupEntriesLBA);
        WriteU32(h + 80, kNumPartitionEntries);
        WriteU32(h + 84, kPartitionEntrySize);
        WriteU32(h + 88, entryArrayCrc);

        DWORD headerCrc = Crc32(h, kGptHeaderSize);
        WriteU32(h + 16, headerCrc);
    }

    // ---- Build the protective MBR (sector 0) ----
    std::vector<BYTE> mbrSector(sectorSize, 0);
    {
        // Start from a clean, spec-correct protective MBR rather than trying
        // to preserve unknown existing boot code (Analyze() and Convert() are
        // separate calls, so we don't carry the original sector 0 between them).
        BYTE* entry = &mbrSector[446];
        entry[0] = 0x00;          // Boot indicator
        entry[1] = 0x00; entry[2] = 0x02; entry[3] = 0x00; // Starting CHS (0/0/2, per UEFI spec)
        entry[4] = 0xEE;          // OSType = GPT protective
        entry[5] = 0xFF; entry[6] = 0xFF; entry[7] = 0xFF; // Ending CHS (overflow marker)
        ULONGLONG protectiveSize = std::min<ULONGLONG>(totalSectors - 1, 0xFFFFFFFFULL);
        WriteU32(&mbrSector[446 + 8], 1);                       // StartingLBA
        WriteU32(&mbrSector[446 + 12], (DWORD)protectiveSize);  // SizeInLBA
        mbrSector[510] = 0x55;
        mbrSector[511] = 0xAA;
    }

    // ---- Write everything. Backup structures first (least consequential if
    // interrupted), primary structures last (only once these are written does
    // the disk actually "become" GPT to most software). ----
    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(info.driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Could not lock all volumes on this disk for exclusive access. Close any windows, ")
               _T("apps, or drive letters using it (including this app's own sector view of it) and try again.");
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
    }

    auto writeSectorAligned = [&](ULONGLONG lba, const std::vector<BYTE>& data) -> bool
    {
        LPVOID buf = VirtualAlloc(nullptr, data.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buf) return false;
        memcpy(buf, data.data(), data.size());

        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(lba) * sectorSize;
        SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN);

        DWORD written = 0;
        BOOL ok = WriteFile(hDrive, buf, (DWORD)data.size(), &written, nullptr);
        VirtualFree(buf, 0, MEM_RELEASE);
        return ok && written == data.size();
    };

    CString err;
    if (!writeSectorAligned(backupEntriesLBA, entryArray))
        err = _T("Failed writing the backup partition entry array.");
    else if (!writeSectorAligned(backupHeaderLBA, backupHeader))
        err = _T("Failed writing the backup GPT header.");
    else if (!writeSectorAligned(2, entryArray))
        err = _T("Failed writing the primary partition entry array.");
    else if (!writeSectorAligned(1, primaryHeader))
        err = _T("Failed writing the primary GPT header.");
    else if (!writeSectorAligned(0, mbrSector))
        err = _T("Failed writing the protective MBR.");

    FlushFileBuffers(hDrive);

    if (err.IsEmpty())
    {
        DWORD dummy = 0;
        DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, nullptr, 0, nullptr, 0, &dummy, nullptr);
    }

    CloseHandle(hDrive);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
    return err;
}
