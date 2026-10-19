// VolumeMap.cpp
#include "pch.h"
#include "VolumeMap.h"

namespace
{
    // Extents of one volume, opened by a "\\.\X:" style device name.
    void QueryExtents(const CString& device, const CString& root, std::vector<VolumeExtent>& out)
    {
        HANDLE h = CreateFile(device, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return;

        std::vector<BYTE> buf(sizeof(VOLUME_DISK_EXTENTS) + 32 * sizeof(DISK_EXTENT));
        DWORD bytes = 0;
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, buf.data(), (DWORD)buf.size(),
                            &bytes, nullptr))
        {
            const VOLUME_DISK_EXTENTS* ext = reinterpret_cast<const VOLUME_DISK_EXTENTS*>(buf.data());
            for (DWORD i = 0; i < ext->NumberOfDiskExtents && i < 32; ++i)
            {
                VolumeExtent v;
                v.root = root;
                v.diskNumber = ext->Extents[i].DiskNumber;
                v.startByte = (ULONGLONG)ext->Extents[i].StartingOffset.QuadPart;
                v.lengthBytes = (ULONGLONG)ext->Extents[i].ExtentLength.QuadPart;
                out.push_back(v);
            }
        }
        CloseHandle(h);
    }
}

std::vector<VolumeExtent> CVolumeMap::Enumerate()
{
    std::vector<VolumeExtent> out;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i)
    {
        if (!(mask & (1u << i)))
            continue;
        CString root;
        root.Format(_T("%c:\\"), _T('A') + i);
        UINT type = GetDriveType(root);
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
            continue;
        CString device;
        device.Format(_T("\\\\.\\%c:"), _T('A') + i);
        QueryExtents(device, root, out);
    }
    return out;
}

CString CVolumeMap::LettersFor(const std::vector<VolumeExtent>& map, DWORD diskNumber, ULONGLONG startLBA,
                               DWORD sectorSize)
{
    CString letters;
    for (const VolumeExtent& v : map)
    {
        if (v.diskNumber == diskNumber && v.startByte == startLBA * sectorSize)
        {
            if (!letters.IsEmpty())
                letters += _T(" ");
            letters += v.root.Left(2);
        }
    }
    return letters;
}

std::vector<VolumeExtent> CVolumeMap::ExtentsForPath(const CString& path)
{
    std::vector<VolumeExtent> out;
    TCHAR volPath[MAX_PATH] = {};
    if (!GetVolumePathName(path, volPath, MAX_PATH))
        return out;
    TCHAR volName[MAX_PATH] = {};
    if (!GetVolumeNameForVolumeMountPoint(volPath, volName, MAX_PATH))
        return out;
    CString device(volName);                // \\?\Volume{guid}\  -> strip the trailing slash
    device.TrimRight(_T('\\'));
    QueryExtents(device, volPath, out);
    return out;
}
