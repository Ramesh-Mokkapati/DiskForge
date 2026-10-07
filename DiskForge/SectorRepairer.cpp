// SectorRepairer.cpp
#include "pch.h"
#include "SectorRepairer.h"
#include "SystemDiskGuard.h"
#include "VolumeLockHelper.h"

SectorRepairResult CSectorRepairer::Repair(const CString& devicePath, UINT driveIndex, DWORD sectorSize,
                                            ULONGLONG startLBA, ULONGLONG sectorCount)
{
    SectorRepairResult result;

    if (sectorCount == 0)
    {
        result.errorMessage = _T("Internal error: empty sector range.");
        return result;
    }

    if (CSystemDiskGuard::IsProtectedDrive(driveIndex))
    {
        result.errorMessage = _T("Refusing to write: this is the drive Windows is running from.");
        return result;
    }

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        result.errorMessage = _T("Could not lock all volumes on this disk for exclusive access. Close any ")
                               _T("windows, apps, or drive letters using it and try again.");
        return result;
    }

    HANDLE hDrive = CreateFile(
        devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        result.errorMessage = _T("Failed to open the drive for writing. Make sure you're running as Administrator.");
        return result;
    }

    DWORD bytes = static_cast<DWORD>(sectorCount * (ULONGLONG)sectorSize);
    LPVOID buf = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf)
    {
        CloseHandle(hDrive);
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        result.errorMessage = _T("Out of memory.");
        return result;
    }
    ZeroMemory(buf, bytes);

    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(startLBA) * sectorSize;
    SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN);

    DWORD written = 0;
    BOOL wok = WriteFile(hDrive, buf, bytes, &written, nullptr);
    result.writeSucceeded = (wok && written == bytes);
    if (!result.writeSucceeded)
        result.errorMessage.Format(_T("Write failed (Win32 error %u) - the sector could not be repaired."), ::GetLastError());

    FlushFileBuffers(hDrive);

    if (result.writeSucceeded)
    {
        // Re-read to see whether it's actually usable now.
        ZeroMemory(buf, bytes);
        SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN);
        DWORD readBytes = 0;
        BOOL rok = ReadFile(hDrive, buf, bytes, &readBytes, nullptr);
        result.nowReadable = (rok && readBytes == bytes);
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hDrive);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
    return result;
}
