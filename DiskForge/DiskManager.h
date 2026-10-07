// DiskManager.h - Disk I/O abstraction layer
#pragma once
#include "pch.h"

struct DriveInfo
{
    CString     devicePath;     // e.g. \\.\PhysicalDrive0
    CString     displayName;    // Human-readable name
    ULONGLONG   totalSectors;   // Total number of sectors
    DWORD       bytesPerSector; // Bytes per physical sector
    ULONGLONG   totalBytes;     // Total disk size in bytes
    CString     model;          // Device model string
    UINT        driveIndex;     // Physical drive index
};

class CDiskManager
{
public:
    CDiskManager();
    ~CDiskManager();

    // Enumerate all physical drives present on the system
    bool EnumerateDrives(std::vector<DriveInfo>& drives);

    // Open a physical drive for reading (requires elevation)
    bool OpenDrive(const CString& devicePath);

    // Close the currently open drive
    void CloseDrive();

    // Read a specific sector (0-based index) into buffer
    // Buffer must be at least m_bytesPerSector bytes, sector-aligned
    bool ReadSector(ULONGLONG sectorIndex, std::vector<BYTE>& buffer);

    // Getters for currently-open drive properties
    DWORD       GetBytesPerSector() const { return m_bytesPerSector; }
    ULONGLONG   GetTotalSectors()   const { return m_totalSectors; }
    bool        IsOpen()            const { return m_hDrive != INVALID_HANDLE_VALUE; }
    CString     GetLastError()      const { return m_lastError; }

private:
    HANDLE      m_hDrive;
    DWORD       m_bytesPerSector;
    ULONGLONG   m_totalSectors;
    CString     m_lastError;

    void SetError(const CString& msg);
    CString GetDriveModel(const CString& devicePath);
    CString FormatSize(ULONGLONG bytes);
};
