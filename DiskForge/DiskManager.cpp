// DiskManager.cpp - Disk I/O implementation
#include "pch.h"
#include "DiskManager.h"

CDiskManager::CDiskManager()
    : m_hDrive(INVALID_HANDLE_VALUE)
    , m_bytesPerSector(512)
    , m_totalSectors(0)
{
}

CDiskManager::~CDiskManager()
{
    CloseDrive();
}

void CDiskManager::SetError(const CString& msg)
{
    m_lastError = msg;
}

CString CDiskManager::FormatSize(ULONGLONG bytes)
{
    CString s;
    if (bytes >= (1ULL << 40))
        s.Format(_T("%.2f TB"), (double)bytes / (1ULL << 40));
    else if (bytes >= (1ULL << 30))
        s.Format(_T("%.2f GB"), (double)bytes / (1ULL << 30));
    else if (bytes >= (1ULL << 20))
        s.Format(_T("%.2f MB"), (double)bytes / (1ULL << 20));
    else
        s.Format(_T("%llu bytes"), bytes);
    return s;
}

CString CDiskManager::GetDriveModel(const CString& devicePath)
{
    // Query STORAGE_DEVICE_DESCRIPTOR to get model string
    HANDLE hDrive = CreateFile(
        devicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hDrive == INVALID_HANDLE_VALUE)
        return _T("Unknown");

    // Allocate a buffer large enough for the descriptor + strings
    const DWORD bufSize = 1024;
    std::vector<BYTE> buf(bufSize, 0);

    STORAGE_PROPERTY_QUERY spq = {};
    spq.PropertyId = StorageDeviceProperty;
    spq.QueryType  = PropertyStandardQuery;

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        hDrive,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &spq, sizeof(spq),
        buf.data(), bufSize,
        &bytesReturned, nullptr);

    CloseHandle(hDrive);

    if (!ok || bytesReturned < sizeof(STORAGE_DEVICE_DESCRIPTOR))
        return _T("Unknown");

    auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buf.data());
    CString model;

    if (desc->ProductIdOffset && desc->ProductIdOffset < bufSize)
    {
        // ProductId is an ANSI string at the given offset
        const char* pid = reinterpret_cast<const char*>(buf.data() + desc->ProductIdOffset);
        model = CString(pid);
        model.Trim();
    }

    if (model.IsEmpty())
        model = _T("Unknown Model");

    return model;
}

bool CDiskManager::EnumerateDrives(std::vector<DriveInfo>& drives)
{
    drives.clear();

    // Try physical drives 0-15; stop on first consecutive 4 failures
    int failures = 0;
    for (UINT i = 0; i < 16 && failures < 4; ++i)
    {
        CString path;
        path.Format(_T("\\\\.\\PhysicalDrive%u"), i);

        HANDLE h = CreateFile(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (h == INVALID_HANDLE_VALUE)
        {
            ++failures;
            continue;
        }

        failures = 0; // reset consecutive-failure counter

        // Get disk geometry
        DISK_GEOMETRY_EX geo = {};
        DWORD bytes = 0;
        BOOL ok = DeviceIoControl(
            h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            nullptr, 0,
            &geo, sizeof(geo),
            &bytes, nullptr);

        CloseHandle(h);

        if (!ok)
            continue;

        DriveInfo info;
        info.driveIndex     = i;
        info.devicePath     = path;
        info.bytesPerSector = (geo.Geometry.BytesPerSector > 0) ? geo.Geometry.BytesPerSector : 512;
        info.totalBytes     = static_cast<ULONGLONG>(geo.DiskSize.QuadPart);
        info.totalSectors   = (info.bytesPerSector > 0) ? (info.totalBytes / info.bytesPerSector) : 0;
        info.model          = GetDriveModel(path);

        info.displayName.Format(
            _T("PhysicalDrive%u - %s (%s, %u B/sector, %llu sectors)"),
            i,
            info.model.GetString(),
            FormatSize(info.totalBytes).GetString(),
            info.bytesPerSector,
            info.totalSectors);

        drives.push_back(info);
    }

    return !drives.empty();
}

bool CDiskManager::OpenDrive(const CString& devicePath)
{
    CloseDrive();

    m_hDrive = CreateFile(
        devicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING, // Required for sector-aligned reads
        nullptr);

    if (m_hDrive == INVALID_HANDLE_VALUE)
    {
        DWORD err = ::GetLastError();
        CString msg;
        msg.Format(_T("Failed to open drive '%s'. Error: %u\n"
                      "Note: This application must be run as Administrator."),
                   devicePath.GetString(), err);
        SetError(msg);
        return false;
    }

    // Re-query geometry to populate member fields
    DISK_GEOMETRY_EX geo = {};
    DWORD bytes = 0;
    if (!DeviceIoControl(m_hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                         nullptr, 0, &geo, sizeof(geo), &bytes, nullptr))
    {
        DWORD err = ::GetLastError();
        CString msg;
        msg.Format(_T("Failed to query drive geometry. Error: %u"), err);
        SetError(msg);
        CloseDrive();
        return false;
    }

    m_bytesPerSector = (geo.Geometry.BytesPerSector > 0) ? geo.Geometry.BytesPerSector : 512;
    ULONGLONG totalBytes = static_cast<ULONGLONG>(geo.DiskSize.QuadPart);
    m_totalSectors = (m_bytesPerSector > 0) ? (totalBytes / m_bytesPerSector) : 0;

    return true;
}

void CDiskManager::CloseDrive()
{
    if (m_hDrive != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hDrive);
        m_hDrive = INVALID_HANDLE_VALUE;
    }
    m_bytesPerSector = 512;
    m_totalSectors   = 0;
}

bool CDiskManager::ReadSector(ULONGLONG sectorIndex, std::vector<BYTE>& buffer)
{
    if (m_hDrive == INVALID_HANDLE_VALUE)
    {
        SetError(_T("No drive is open."));
        return false;
    }

    if (sectorIndex >= m_totalSectors)
    {
        CString msg;
        msg.Format(_T("Sector %llu is beyond the last sector (%llu)."),
                   sectorIndex, m_totalSectors - 1);
        SetError(msg);
        return false;
    }

    // Sector-aligned allocation using VirtualAlloc (requirement for FILE_FLAG_NO_BUFFERING)
    DWORD allocSize = m_bytesPerSector;
    // Round up to system allocation granularity
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD granularity = si.dwAllocationGranularity;
    if (allocSize < granularity)
        allocSize = granularity;

    LPVOID rawBuf = VirtualAlloc(nullptr, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rawBuf)
    {
        SetError(_T("VirtualAlloc failed for sector buffer."));
        return false;
    }

    // Seek to the correct byte offset
    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(sectorIndex) * m_bytesPerSector;

    LARGE_INTEGER newPos;
    if (!SetFilePointerEx(m_hDrive, offset, &newPos, FILE_BEGIN))
    {
        DWORD err = ::GetLastError();
        CString msg;
        msg.Format(_T("SetFilePointerEx failed. Error: %u"), err);
        SetError(msg);
        VirtualFree(rawBuf, 0, MEM_RELEASE);
        return false;
    }

    DWORD bytesRead = 0;
    BOOL ok = ReadFile(m_hDrive, rawBuf, m_bytesPerSector, &bytesRead, nullptr);

    if (!ok || bytesRead != m_bytesPerSector)
    {
        DWORD err = ::GetLastError();
        CString msg;
        msg.Format(_T("ReadFile failed (read %u of %u bytes). Error: %u"),
                   bytesRead, m_bytesPerSector, err);
        SetError(msg);
        VirtualFree(rawBuf, 0, MEM_RELEASE);
        return false;
    }

    buffer.assign(
        static_cast<BYTE*>(rawBuf),
        static_cast<BYTE*>(rawBuf) + m_bytesPerSector);

    VirtualFree(rawBuf, 0, MEM_RELEASE);
    return true;
}
