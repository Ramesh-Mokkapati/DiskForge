// SystemDiskGuard.cpp
#include "pch.h"
#include "SystemDiskGuard.h"

bool CSystemDiskGuard::GetDiskExtentsForVolume(const CString& volumePath, std::vector<UINT>& diskIndicesOut)
{
    diskIndicesOut.clear();

    HANDLE h = CreateFile(
        volumePath,
        0, // No access needed, just querying
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    BYTE buf[sizeof(VOLUME_DISK_EXTENTS) + 8 * sizeof(DISK_EXTENT)] = {};
    DWORD bytesReturned = 0;

    BOOL ok = DeviceIoControl(
        h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        nullptr, 0,
        buf, sizeof(buf),
        &bytesReturned, nullptr);

    CloseHandle(h);

    if (!ok)
        return false;

    auto* extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(buf);
    for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i)
        diskIndicesOut.push_back(static_cast<UINT>(extents->Extents[i].DiskNumber));

    return !diskIndicesOut.empty();
}

std::vector<UINT> CSystemDiskGuard::GetProtectedDriveIndices()
{
    std::vector<UINT> protectedDrives;

    // 1) The volume Windows itself is running from (%SystemRoot%, e.g. C:\Windows).
    TCHAR winDir[MAX_PATH] = {};
    if (GetWindowsDirectory(winDir, MAX_PATH) > 0 && _tcslen(winDir) >= 2)
    {
        CString volPath;
        volPath.Format(_T("\\\\.\\%c:"), winDir[0]);

        std::vector<UINT> disks;
        if (GetDiskExtentsForVolume(volPath, disks))
        {
            for (UINT d : disks)
                protectedDrives.push_back(d);
        }
    }

    // 2) The volume holding the currently running executable, as a second
    //    line of defense in case %SystemRoot% detection above fails for some
    //    reason (e.g. exotic boot configurations).
    TCHAR exePath[MAX_PATH] = {};
    if (GetModuleFileName(nullptr, exePath, MAX_PATH) > 0 && _tcslen(exePath) >= 2)
    {
        CString volPath;
        volPath.Format(_T("\\\\.\\%c:"), exePath[0]);

        std::vector<UINT> disks;
        if (GetDiskExtentsForVolume(volPath, disks))
        {
            for (UINT d : disks)
                protectedDrives.push_back(d);
        }
    }

    return protectedDrives;
}

bool CSystemDiskGuard::IsProtectedDrive(UINT physicalDriveIndex)
{
    std::vector<UINT> protectedDrives = GetProtectedDriveIndices();

    // Fail safe: if we couldn't determine anything at all, don't silently
    // allow drive 0 - most systems boot from PhysicalDrive0, so treat an
    // empty/uncertain result as "drive 0 is protected" rather than "nothing is".
    if (protectedDrives.empty())
        return physicalDriveIndex == 0;

    for (UINT d : protectedDrives)
    {
        if (d == physicalDriveIndex)
            return true;
    }
    return false;
}
