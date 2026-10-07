// PartitionEditor.cpp
#include "pch.h"
#include "PartitionEditor.h"
#include "SystemDiskGuard.h"
#include "VolumeLockHelper.h"

namespace
{
    const DWORD kGptHeaderSize = 92;
    const DWORD kNumPartitionEntries = 128;
    const DWORD kPartitionEntrySize  = 128;

    struct HiddenPair { BYTE visible; BYTE hidden; };
    const HiddenPair kHiddenPairs[] = {
        { 0x01, 0x11 }, // FAT12
        { 0x04, 0x14 }, // FAT16 <32MB
        { 0x06, 0x16 }, // FAT16
        { 0x07, 0x17 }, // NTFS/exFAT
        { 0x0B, 0x1B }, // FAT32 (CHS)
        { 0x0C, 0x1C }, // FAT32 (LBA)
        { 0x0E, 0x1E }, // FAT16 (LBA)
    };

    inline void WriteU16(BYTE* p, WORD v)  { memcpy(p, &v, sizeof(v)); }
    inline void WriteU32(BYTE* p, DWORD v) { memcpy(p, &v, sizeof(v)); }
    inline void WriteU64(BYTE* p, ULONGLONG v) { memcpy(p, &v, sizeof(v)); }
    inline WORD  ReadU16(const BYTE* p) { WORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline DWORD ReadU32(const BYTE* p) { DWORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline ULONGLONG ReadU64(const BYTE* p) { ULONGLONG v; memcpy(&v, p, sizeof(v)); return v; }

    inline void WriteGuid(BYTE* p, const GUID& g)
    {
        WriteU32(p, g.Data1);
        WriteU16(p + 4, g.Data2);
        WriteU16(p + 6, g.Data3);
        memcpy(p + 8, g.Data4, 8);
    }
}

DWORD CPartitionEditor::Crc32(const void* data, size_t length)
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

PartitionEditFields CPartitionEditor::ReadFields(const PartitionEntry& entry)
{
    PartitionEditFields f;
    f.scheme = entry.scheme;

    if (entry.scheme == _T("MBR"))
    {
        f.mbrActive   = entry.bootable;
        f.mbrTypeByte = entry.mbrType;
    }
    else
    {
        f.gptTypeGuid           = entry.typeGuidRaw;
        f.gptName               = entry.name;
        f.gptLegacyBiosBootable = (entry.attributesRaw & (1ULL << 2)) != 0;
        f.gptNoDriveLetter      = (entry.attributesRaw & (1ULL << 63)) != 0;
    }
    return f;
}

bool CPartitionEditor::IsMbrTypeHideable(BYTE type)
{
    for (const auto& p : kHiddenPairs)
        if (p.visible == type || p.hidden == type)
            return true;
    return false;
}

bool CPartitionEditor::IsMbrTypeHidden(BYTE type)
{
    for (const auto& p : kHiddenPairs)
        if (p.hidden == type)
            return true;
    return false;
}

BYTE CPartitionEditor::ToggleMbrHidden(BYTE type, bool makeHidden)
{
    for (const auto& p : kHiddenPairs)
    {
        if (makeHidden && p.visible == type) return p.hidden;
        if (!makeHidden && p.hidden == type) return p.visible;
    }
    return type; // No known counterpart - leave unchanged
}

CString CPartitionEditor::WriteMbrEntry(const CString& devicePath, UINT driveIndex, int slot,
                                         const PartitionEditFields& fields,
                                         ULONGLONG startLBA, ULONGLONG sectorCount)
{
    if (slot < 0 || slot > 3)
        return _T("Internal error: invalid MBR partition slot.");

    if (CSystemDiskGuard::IsProtectedDrive(driveIndex))
        return _T("Refusing to write: this is the drive Windows is running from.");

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Could not lock all volumes on this disk for exclusive access. Close any windows, ")
               _T("apps, or drive letters using it and try again.");
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
    }

    // Determine the sector size so we allocate a correctly-sized, aligned buffer.
    DISK_GEOMETRY geom = {};
    DWORD bytesReturned = 0;
    DWORD sectorSize = 512;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr))
        sectorSize = geom.BytesPerSector;

    LPVOID buf = VirtualAlloc(nullptr, sectorSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CString err;
    if (!buf)
    {
        err = _T("Out of memory.");
    }
    else
    {
        LARGE_INTEGER zero = {};
        SetFilePointerEx(hDrive, zero, nullptr, FILE_BEGIN);
        DWORD readBytes = 0;
        if (!ReadFile(hDrive, buf, sectorSize, &readBytes, nullptr) || readBytes != sectorSize)
        {
            err = _T("Failed to read sector 0 before editing it.");
        }
        else
        {
            BYTE* mbr = static_cast<BYTE*>(buf);
            BYTE* entry = mbr + 446 + slot * 16;
            entry[0] = fields.mbrActive ? 0x80 : 0x00;
            entry[4] = fields.mbrTypeByte;
            // Bytes 1-3, 5-7 (CHS) and 8-15 (LBA start/count) are left as-is -
            // this feature only edits the active flag and type byte.

            SetFilePointerEx(hDrive, zero, nullptr, FILE_BEGIN);
            DWORD written = 0;
            if (!WriteFile(hDrive, buf, sectorSize, &written, nullptr) || written != sectorSize)
                err.Format(_T("Write error (Win32 error %u)."), ::GetLastError());
        }
        VirtualFree(buf, 0, MEM_RELEASE);
    }

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

CString CPartitionEditor::WriteGptEntry(const CString& devicePath, UINT driveIndex, int entryIndex,
                                         const PartitionEditFields& fields,
                                         ULONGLONG startLBA, ULONGLONG sectorCount)
{
    if (entryIndex < 0 || entryIndex >= (int)kNumPartitionEntries)
        return _T("Internal error: invalid GPT partition entry index.");

    if (CSystemDiskGuard::IsProtectedDrive(driveIndex))
        return _T("Refusing to write: this is the drive Windows is running from.");

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Could not lock all volumes on this disk for exclusive access. Close any windows, ")
               _T("apps, or drive letters using it and try again.");
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
    }

    DISK_GEOMETRY geom = {};
    DWORD bytesReturned = 0;
    DWORD sectorSize = 512;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr))
        sectorSize = geom.BytesPerSector;

    GET_LENGTH_INFORMATION lenInfo = {};
    ULONGLONG totalSectors = 0;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lenInfo, sizeof(lenInfo), &bytesReturned, nullptr))
        totalSectors = lenInfo.Length.QuadPart / sectorSize;

    auto readSectors = [&](ULONGLONG lba, DWORD count, std::vector<BYTE>& out) -> bool
    {
        out.resize((size_t)count * sectorSize);
        LARGE_INTEGER off; off.QuadPart = static_cast<LONGLONG>(lba) * sectorSize;
        SetFilePointerEx(hDrive, off, nullptr, FILE_BEGIN);
        DWORD readBytes = 0;
        return ReadFile(hDrive, out.data(), (DWORD)out.size(), &readBytes, nullptr) && readBytes == out.size();
    };
    auto writeSectors = [&](ULONGLONG lba, const std::vector<BYTE>& data) -> bool
    {
        LARGE_INTEGER off; off.QuadPart = static_cast<LONGLONG>(lba) * sectorSize;
        SetFilePointerEx(hDrive, off, nullptr, FILE_BEGIN);
        DWORD written = 0;
        return WriteFile(hDrive, data.data(), (DWORD)data.size(), &written, nullptr) && written == data.size();
    };

    CString err;
    if (totalSectors == 0)
    {
        err = _T("Could not determine the disk's size.");
    }
    else
    {
        // Read the primary header (LBA1) to find where the primary array and
        // its size live, rather than assuming the standard layout.
        std::vector<BYTE> hdrSec;
        if (!readSectors(1, 1, hdrSec) || memcmp(hdrSec.data(), "EFI PART", 8) != 0)
        {
            err = _T("Could not read a valid primary GPT header.");
        }
        else
        {
            ULONGLONG primaryEntriesLBA = ReadU64(&hdrSec[72]);
            DWORD     numEntries        = ReadU32(&hdrSec[80]);
            DWORD     entrySize         = ReadU32(&hdrSec[84]);
            ULONGLONG backupHeaderLBA   = ReadU64(&hdrSec[32]); // AlternateLBA

            if (numEntries != kNumPartitionEntries || entrySize != kPartitionEntrySize || entryIndex >= (int)numEntries)
            {
                err = _T("This disk's GPT layout is non-standard; refusing to edit it to avoid corrupting the table.");
            }
            else
            {
                DWORD arrayBytes = numEntries * entrySize;
                DWORD arraySectors = arrayBytes / sectorSize;

                std::vector<BYTE> hdrBackupSec;
                std::vector<BYTE> primaryArray, backupArray;
                ULONGLONG backupEntriesLBA = backupHeaderLBA - arraySectors;

                if (!readSectors(backupHeaderLBA, 1, hdrBackupSec) || memcmp(hdrBackupSec.data(), "EFI PART", 8) != 0)
                    err = _T("Could not read a valid backup GPT header.");
                else if (!readSectors(primaryEntriesLBA, arraySectors, primaryArray))
                    err = _T("Could not read the primary partition entry array.");
                else if (!readSectors(backupEntriesLBA, arraySectors, backupArray))
                    err = _T("Could not read the backup partition entry array.");
                else
                {
                    // ---- Apply the edit to entry `entryIndex` in both arrays ----
                    std::vector<BYTE>* arrays[2] = { &primaryArray, &backupArray };
                    for (int a = 0; a < 2; ++a)
                    {
                        std::vector<BYTE>& arr = *arrays[a];
                        BYTE* e = &arr[(size_t)entryIndex * entrySize];
                        WriteGuid(e + 0, fields.gptTypeGuid);
                        // UniquePartitionGUID (offset 16), StartingLBA/EndingLBA (32/40) are left as-is.
                        ULONGLONG attrs = ReadU64(e + 48);
                        attrs = fields.gptLegacyBiosBootable ? (attrs | (1ULL << 2))  : (attrs & ~(1ULL << 2));
                        attrs = fields.gptNoDriveLetter      ? (attrs | (1ULL << 63)) : (attrs & ~(1ULL << 63));
                        WriteU64(e + 48, attrs);

                        WCHAR nameBuf[36] = {};
                        wcsncpy_s(nameBuf, fields.gptName.GetString(), 35);
                        memcpy(e + 56, nameBuf, sizeof(nameBuf));
                    }

                    DWORD primaryCrc = Crc32(primaryArray.data(), primaryArray.size());
                    DWORD backupCrc  = Crc32(backupArray.data(), backupArray.size());

                    // ---- Recompute both header CRCs ----
                    WriteU32(&hdrSec[88], primaryCrc);
                    WriteU32(&hdrSec[16], 0);
                    DWORD primaryHeaderCrc = Crc32(hdrSec.data(), kGptHeaderSize);
                    WriteU32(&hdrSec[16], primaryHeaderCrc);

                    WriteU32(&hdrBackupSec[88], backupCrc);
                    WriteU32(&hdrBackupSec[16], 0);
                    DWORD backupHeaderCrc = Crc32(hdrBackupSec.data(), kGptHeaderSize);
                    WriteU32(&hdrBackupSec[16], backupHeaderCrc);

                    // ---- Write everything back: backup first, primary last ----
                    if (!writeSectors(backupEntriesLBA, backupArray))
                        err = _T("Failed writing the backup partition entry array.");
                    else if (!writeSectors(backupHeaderLBA, hdrBackupSec))
                        err = _T("Failed writing the backup GPT header.");
                    else if (!writeSectors(primaryEntriesLBA, primaryArray))
                        err = _T("Failed writing the primary partition entry array.");
                    else if (!writeSectors(1, hdrSec))
                        err = _T("Failed writing the primary GPT header.");
                }
            }
        }
    }

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

CPartitionEditor::VolumeMatch CPartitionEditor::FindVolumeForPartition(UINT diskIndex, ULONGLONG startLBA)
{
    VolumeMatch result;

    TCHAR volumeName[MAX_PATH] = {};
    HANDLE hFind = FindFirstVolume(volumeName, MAX_PATH);
    if (hFind == INVALID_HANDLE_VALUE)
        return result;

    do
    {
        CString volNoSlash(volumeName);
        if (volNoSlash.GetLength() > 0 && volNoSlash[volNoSlash.GetLength() - 1] == _T('\\'))
            volNoSlash = volNoSlash.Left(volNoSlash.GetLength() - 1);

        HANDLE hVol = CreateFile(volNoSlash, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVol == INVALID_HANDLE_VALUE)
            continue;

        BYTE extentBuf[sizeof(VOLUME_DISK_EXTENTS) + 8 * sizeof(DISK_EXTENT)] = {};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                    nullptr, 0, extentBuf, sizeof(extentBuf), &bytesReturned, nullptr);
        CloseHandle(hVol);

        if (!ok) continue;
        auto* extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(extentBuf);
        if (extents->NumberOfDiskExtents < 1) continue;
        if (extents->Extents[0].DiskNumber != diskIndex) continue;

        // Match by byte offset against the partition's start LBA. We don't
        // know this volume's exact sector size at this point, so check the
        // two overwhelmingly common values (512 and 4096).
        ULONGLONG extentStartBytes = extents->Extents[0].StartingOffset.QuadPart;
        bool matches = (extentStartBytes == startLBA * 512) || (extentStartBytes == startLBA * 4096);
        if (!matches)
            continue;

        result.found = true;
        result.volumeGuidPath = volumeName; // keep trailing backslash form for SetVolumeMountPoint/DeleteVolumeMountPoint

        TCHAR pathNames[MAX_PATH] = {};
        DWORD returnLen = 0;
        if (GetVolumePathNamesForVolumeName(volumeName, pathNames, MAX_PATH, &returnLen))
        {
            CString first(pathNames);
            if (first.GetLength() >= 2 && first[1] == _T(':'))
                result.currentDriveLetter = first[0];
        }
        break;

    } while (FindNextVolume(hFind, volumeName, MAX_PATH));

    FindVolumeClose(hFind);
    return result;
}

CString CPartitionEditor::SetDriveLetter(const VolumeMatch& vol, TCHAR newLetter)
{
    if (!vol.found)
        return _T("No mounted volume was found for this partition (it may be unformatted or unrecognized).");

    // Remove the existing assignment first, if any.
    if (vol.currentDriveLetter != 0)
    {
        CString oldPath;
        oldPath.Format(_T("%c:\\"), vol.currentDriveLetter);
        DeleteVolumeMountPoint(oldPath);
    }

    if (newLetter == 0)
        return CString(); // Only removal was requested.

    CString newPath;
    newPath.Format(_T("%c:\\"), newLetter);
    if (!SetVolumeMountPoint(newPath, vol.volumeGuidPath))
    {
        CString err;
        err.Format(_T("Failed to assign drive letter %c: (Win32 error %u)."), newLetter, ::GetLastError());
        return err;
    }
    return CString();
}

std::vector<TCHAR> CPartitionEditor::GetAvailableDriveLetters()
{
    std::vector<TCHAR> result;
    DWORD used = GetLogicalDrives();
    for (int i = 0; i < 26; ++i)
    {
        if (!(used & (1 << i)))
            result.push_back(static_cast<TCHAR>('A' + i));
    }
    return result;
}

CString CPartitionEditor::DeleteMbrEntry(const CString& devicePath, UINT driveIndex, int slot)
{
    if (slot < 0 || slot > 3)
        return _T("Internal error: invalid MBR partition slot.");

    if (CSystemDiskGuard::IsProtectedDrive(driveIndex))
        return _T("Refusing to delete: this is the drive Windows is running from.");

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Could not lock all volumes on this disk for exclusive access.");
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
    }

    DISK_GEOMETRY geom = {};
    DWORD bytesReturned = 0;
    DWORD sectorSize = 512;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr))
        sectorSize = geom.BytesPerSector;

    LPVOID buf = VirtualAlloc(nullptr, sectorSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CString err;
    if (!buf)
    {
        err = _T("Out of memory.");
    }
    else
    {
        LARGE_INTEGER zero = {};
        SetFilePointerEx(hDrive, zero, nullptr, FILE_BEGIN);
        DWORD readBytes = 0;
        if (!ReadFile(hDrive, buf, sectorSize, &readBytes, nullptr) || readBytes != sectorSize)
        {
            err = _T("Failed to read sector 0.");
        }
        else
        {
            BYTE* mbr = static_cast<BYTE*>(buf);
            memset(mbr + 446 + slot * 16, 0, 16); // zero the entire 16-byte entry
            SetFilePointerEx(hDrive, zero, nullptr, FILE_BEGIN);
            DWORD written = 0;
            if (!WriteFile(hDrive, buf, sectorSize, &written, nullptr) || written != sectorSize)
                err.Format(_T("Write error (Win32 error %u)."), ::GetLastError());
        }
        VirtualFree(buf, 0, MEM_RELEASE);
    }

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

CString CPartitionEditor::DeleteGptEntry(const CString& devicePath, UINT driveIndex, int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= (int)kNumPartitionEntries)
        return _T("Internal error: invalid GPT partition entry index.");

    if (CSystemDiskGuard::IsProtectedDrive(driveIndex))
        return _T("Refusing to delete: this is the drive Windows is running from.");

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Could not lock all volumes on this disk for exclusive access.");
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
    }

    DISK_GEOMETRY geom = {};
    DWORD bytesReturned = 0;
    DWORD sectorSize = 512;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr))
        sectorSize = geom.BytesPerSector;

    auto readSectors = [&](ULONGLONG lba, DWORD count, std::vector<BYTE>& out) -> bool
    {
        out.resize((size_t)count * sectorSize);
        LARGE_INTEGER off; off.QuadPart = static_cast<LONGLONG>(lba) * sectorSize;
        SetFilePointerEx(hDrive, off, nullptr, FILE_BEGIN);
        DWORD readBytes = 0;
        return ReadFile(hDrive, out.data(), (DWORD)out.size(), &readBytes, nullptr) && readBytes == out.size();
    };
    auto writeSectors = [&](ULONGLONG lba, const std::vector<BYTE>& data) -> bool
    {
        LARGE_INTEGER off; off.QuadPart = static_cast<LONGLONG>(lba) * sectorSize;
        SetFilePointerEx(hDrive, off, nullptr, FILE_BEGIN);
        DWORD written = 0;
        return WriteFile(hDrive, data.data(), (DWORD)data.size(), &written, nullptr) && written == data.size();
    };

    CString err;
    std::vector<BYTE> hdrSec;
    if (!readSectors(1, 1, hdrSec) || memcmp(hdrSec.data(), "EFI PART", 8) != 0)
    {
        err = _T("Could not read a valid primary GPT header.");
    }
    else
    {
        ULONGLONG primaryEntriesLBA = ReadU64(&hdrSec[72]);
        DWORD     numEntries        = ReadU32(&hdrSec[80]);
        DWORD     entrySize         = ReadU32(&hdrSec[84]);
        ULONGLONG backupHeaderLBA   = ReadU64(&hdrSec[32]);

        if (numEntries != kNumPartitionEntries || entrySize != kPartitionEntrySize || entryIndex >= (int)numEntries)
        {
            err = _T("This disk's GPT layout is non-standard; refusing to modify it.");
        }
        else
        {
            DWORD arrayBytes   = numEntries * entrySize;
            DWORD arraySectors = arrayBytes / sectorSize;
            ULONGLONG backupEntriesLBA = backupHeaderLBA - arraySectors;

            std::vector<BYTE> hdrBackupSec, primaryArray, backupArray;
            if (!readSectors(backupHeaderLBA, 1, hdrBackupSec) || memcmp(hdrBackupSec.data(), "EFI PART", 8) != 0)
                err = _T("Could not read a valid backup GPT header.");
            else if (!readSectors(primaryEntriesLBA, arraySectors, primaryArray))
                err = _T("Could not read the primary partition entry array.");
            else if (!readSectors(backupEntriesLBA, arraySectors, backupArray))
                err = _T("Could not read the backup partition entry array.");
            else
            {
                // Zero the entry in both arrays
                std::vector<BYTE>* arrays[2] = { &primaryArray, &backupArray };
                for (int a = 0; a < 2; ++a)
                    memset(&(*arrays[a])[(size_t)entryIndex * entrySize], 0, entrySize);

                DWORD primaryCrc = Crc32(primaryArray.data(), primaryArray.size());
                DWORD backupCrc  = Crc32(backupArray.data(), backupArray.size());

                WriteU32(&hdrSec[88], primaryCrc);
                WriteU32(&hdrSec[16], 0);
                WriteU32(&hdrSec[16], Crc32(hdrSec.data(), kGptHeaderSize));

                WriteU32(&hdrBackupSec[88], backupCrc);
                WriteU32(&hdrBackupSec[16], 0);
                WriteU32(&hdrBackupSec[16], Crc32(hdrBackupSec.data(), kGptHeaderSize));

                if (!writeSectors(backupEntriesLBA, backupArray))
                    err = _T("Failed writing the backup partition entry array.");
                else if (!writeSectors(backupHeaderLBA, hdrBackupSec))
                    err = _T("Failed writing the backup GPT header.");
                else if (!writeSectors(primaryEntriesLBA, primaryArray))
                    err = _T("Failed writing the primary partition entry array.");
                else if (!writeSectors(1, hdrSec))
                    err = _T("Failed writing the primary GPT header.");
            }
        }
    }

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

CPartitionEditor::FreeSpaceAfter CPartitionEditor::QueryFreeSpaceAfter(
    const CString& devicePath, UINT driveIndex, const PartitionEntry& entry)
{
    (void)driveIndex;
    FreeSpaceAfter result;

    HANDLE hDrive = CreateFile(devicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
        return result;

    DISK_GEOMETRY geom = {};
    DWORD bytesReturned = 0;
    DWORD sectorSize = 512;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr))
        sectorSize = geom.BytesPerSector;

    GET_LENGTH_INFORMATION lenInfo = {};
    ULONGLONG diskBytes = 0;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lenInfo, sizeof(lenInfo), &bytesReturned, nullptr))
        diskBytes = static_cast<ULONGLONG>(lenInfo.Length.QuadPart);

    const DWORD kMaxLayoutParts = 256;
    DWORD layoutBufSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) +
                          (kMaxLayoutParts - 1) * sizeof(PARTITION_INFORMATION_EX);
    std::vector<BYTE> layoutBuf(layoutBufSize, 0);

    BOOL layoutOk = DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0,
                                     layoutBuf.data(), layoutBufSize, &bytesReturned, nullptr);
    CloseHandle(hDrive);

    if (!layoutOk || diskBytes == 0)
        return result; // valid remains false

    result.valid = true;

    auto* layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(layoutBuf.data());
    ULONGLONG ourEnd = entry.startLBA * sectorSize + entry.sectorCount * sectorSize;

    // Find the start of the closest partition that begins after our partition's end.
    ULONGLONG nextPartStart = diskBytes;
    for (DWORD i = 0; i < layout->PartitionCount && i < kMaxLayoutParts; ++i)
    {
        const PARTITION_INFORMATION_EX& p = layout->PartitionEntry[i];
        if (p.PartitionLength.QuadPart <= 0)
            continue;

        // Skip empty MBR slots (type 0) and empty GPT entries (zero partition number)
        if (layout->PartitionStyle == PARTITION_STYLE_MBR && p.Mbr.PartitionType == 0)
            continue;
        if (layout->PartitionStyle == PARTITION_STYLE_GPT && p.PartitionNumber == 0)
            continue;

        ULONGLONG pStart = static_cast<ULONGLONG>(p.StartingOffset.QuadPart);
        if (pStart >= ourEnd && pStart < nextPartStart)
            nextPartStart = pStart;
    }

    if (nextPartStart > ourEnd)
        result.freeSizeBytes = nextPartStart - ourEnd;

    return result;
}

CString CPartitionEditor::GrowPartition(const CString& devicePath, UINT driveIndex,
                                         const PartitionEntry& entry, ULONGLONG extendByBytes)
{
    if (CSystemDiskGuard::IsProtectedDrive(driveIndex))
        return _T("Refusing to extend: this is the drive Windows is running from.");

    // Resolve the OS partition number and sector size via IOCTL_DISK_GET_DRIVE_LAYOUT_EX.
    HANDLE hDriveRo = CreateFile(devicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDriveRo == INVALID_HANDLE_VALUE)
        return _T("Could not open drive to query partition layout.");

    DISK_GEOMETRY geom = {};
    DWORD bytesReturned = 0;
    DWORD sectorSize = 512;
    if (DeviceIoControl(hDriveRo, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &geom, sizeof(geom), &bytesReturned, nullptr))
        sectorSize = geom.BytesPerSector;

    const DWORD kMaxLayoutParts = 256;
    DWORD layoutBufSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) +
                          (kMaxLayoutParts - 1) * sizeof(PARTITION_INFORMATION_EX);
    std::vector<BYTE> layoutBuf(layoutBufSize, 0);

    ULONG partNumber = 0;
    if (DeviceIoControl(hDriveRo, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0,
                          layoutBuf.data(), layoutBufSize, &bytesReturned, nullptr))
    {
        auto* layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(layoutBuf.data());
        ULONGLONG ourStart = entry.startLBA * sectorSize;
        for (DWORD i = 0; i < layout->PartitionCount && i < kMaxLayoutParts; ++i)
        {
            if (static_cast<ULONGLONG>(layout->PartitionEntry[i].StartingOffset.QuadPart) == ourStart)
            {
                partNumber = layout->PartitionEntry[i].PartitionNumber;
                break;
            }
        }
    }
    CloseHandle(hDriveRo);

    if (partNumber == 0)
        return _T("Could not find this partition in the Windows driver's layout table. "
                   "Try reloading the partition list and try again.");

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Could not lock all volumes on this disk for exclusive access. "
                   "Close any open files or Explorer windows and try again.");
    }

    HANDLE hDrive = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        return _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
    }

    DISK_GROW_PARTITION grow = {};
    grow.PartitionNumber       = partNumber;
    grow.BytesToGrow.QuadPart  = static_cast<LONGLONG>(extendByBytes);

    CString err;
    if (!DeviceIoControl(hDrive, IOCTL_DISK_GROW_PARTITION, &grow, sizeof(grow),
                          nullptr, 0, &bytesReturned, nullptr))
    {
        err.Format(_T("IOCTL_DISK_GROW_PARTITION failed (Win32 error %u). "
                       "Ensure there is enough contiguous free space after this partition."),
                   ::GetLastError());
    }
    else
    {
        DWORD dummy = 0;
        DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, nullptr, 0, nullptr, 0, &dummy, nullptr);
    }
    CloseHandle(hDrive);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes); // unlocks; volume remounts

    if (!err.IsEmpty())
        return err;

    // Try to extend the filesystem on any mounted NTFS volume. Non-fatal if this
    // fails (e.g. FAT32/exFAT don't support live extension via this FSCTL).
    VolumeMatch vol = FindVolumeForPartition(driveIndex, entry.startLBA);
    if (vol.found)
    {
        CString volPath(vol.volumeGuidPath);
        if (volPath.GetLength() > 0 && volPath[volPath.GetLength() - 1] == _T('\\'))
            volPath = volPath.Left(volPath.GetLength() - 1);

        HANDLE hVol = CreateFile(volPath, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVol != INVALID_HANDLE_VALUE)
        {
            LONGLONG newSectors = static_cast<LONGLONG>(
                (entry.sectorCount * (ULONGLONG)sectorSize + extendByBytes) / sectorSize);
            DWORD dummy = 0;
            DeviceIoControl(hVol, FSCTL_EXTEND_VOLUME, &newSectors, sizeof(newSectors),
                             nullptr, 0, &dummy, nullptr);
            CloseHandle(hVol);
        }
    }

    return CString();
}
