// VolumeLockHelper.cpp
#include "pch.h"
#include "VolumeLockHelper.h"

std::vector<HANDLE> CVolumeLockHelper::LockAndDismountVolumesOnDisk(UINT diskIndex, bool& allSucceeded)
{
    std::vector<HANDLE> lockedHandles;
    allSucceeded = true;

    TCHAR volumeName[MAX_PATH] = {};
    HANDLE hFind = FindFirstVolume(volumeName, MAX_PATH);
    if (hFind == INVALID_HANDLE_VALUE)
        return lockedHandles; // No volumes at all is not an error (e.g. blank disk)

    do
    {
        CString vol(volumeName);
        if (vol.GetLength() > 0 && vol[vol.GetLength() - 1] == _T('\\'))
            vol = vol.Left(vol.GetLength() - 1); // CreateFile wants no trailing backslash here

        HANDLE hVol = CreateFile(
            vol, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVol == INVALID_HANDLE_VALUE)
            continue; // Not every volume string is openable (e.g. some system volumes); skip

        BYTE extentBuf[sizeof(VOLUME_DISK_EXTENTS) + 8 * sizeof(DISK_EXTENT)] = {};
        DWORD bytesReturned = 0;
        BOOL gotExtents = DeviceIoControl(
            hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            nullptr, 0, extentBuf, sizeof(extentBuf), &bytesReturned, nullptr);

        bool belongsToTarget = false;
        if (gotExtents)
        {
            auto* extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(extentBuf);
            for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i)
            {
                if (extents->Extents[i].DiskNumber == diskIndex)
                {
                    belongsToTarget = true;
                    break;
                }
            }
        }

        if (!belongsToTarget)
        {
            CloseHandle(hVol);
            continue;
        }

        DWORD dummy = 0;
        BOOL lockOk = DeviceIoControl(hVol, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &dummy, nullptr);
        if (!lockOk)
        {
            // Something else has this volume open. Don't proceed with the raw write.
            allSucceeded = false;
            CloseHandle(hVol);
            continue;
        }

        DeviceIoControl(hVol, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &dummy, nullptr);

        lockedHandles.push_back(hVol); // Keep open (and therefore locked) until the write finishes.

    } while (FindNextVolume(hFind, volumeName, MAX_PATH));

    FindVolumeClose(hFind);
    return lockedHandles;
}

void CVolumeLockHelper::ReleaseLockedVolumes(std::vector<HANDLE>& handles)
{
    for (HANDLE h : handles)
    {
        DWORD dummy = 0;
        DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &dummy, nullptr);
        CloseHandle(h);
    }
    handles.clear();
}
