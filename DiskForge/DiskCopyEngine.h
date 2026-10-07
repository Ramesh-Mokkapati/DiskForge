// DiskCopyEngine.h - Byte-level copy engine for Disk Copy and Restore Image.
//
// Sources:
//   - CopySourceKind::PhysicalDrive - a physical drive (whole disk or one
//     partition's range), read with FILE_FLAG_NO_BUFFERING.
//   - CopySourceKind::ImageFile     - a plain file, read with normal
//     buffered I/O (used by the "Restore Image to Disk" feature).
//
// Destinations:
//   - CopyDestKind::ImageFile - a plain file; written with normal buffered I/O.
//   - CopyDestKind::Disk      - a physical drive; written with
//     FILE_FLAG_NO_BUFFERING, after locking & dismounting any of its volumes.
//
// The only unsupported combination is ImageFile -> ImageFile (plain file
// copy isn't a disk utility operation; ValidateDiskCopyJob rejects it).
//
// This is the ONLY module in the app that opens a handle with GENERIC_WRITE
// to a physical drive. Everything else (DiskManager, PartitionParser) stays
// strictly read-only.
#pragma once
#include "pch.h"

enum class CopySourceKind { PhysicalDrive, ImageFile };
enum class CopyDestKind   { ImageFile, Disk };

struct DiskCopyJob
{
    CopySourceKind sourceKind = CopySourceKind::PhysicalDrive;

    // ---- Source: physical drive (whole disk or one partition's range) ----
    CString     sourceDevicePath;      // e.g. \\.\PhysicalDrive1
    UINT        sourceDriveIndex = 0;
    DWORD       sourceSectorSize = 512;
    ULONGLONG   sourceStartLBA = 0;    // 0 for whole-disk copies
    ULONGLONG   sourceSectorCount = 0; // sectors to copy (whole disk or one partition)

    // ---- Source: image file (when sourceKind == ImageFile) ----
    CString     sourceFilePath;
    ULONGLONG   sourceFileBytes = 0;   // actual file size - populated by the caller before Run()/Validate

    // ---- Destination ----
    CopyDestKind destKind = CopyDestKind::ImageFile;

    // Disk destination
    CString     destDevicePath;        // e.g. \\.\PhysicalDrive2
    UINT        destDriveIndex = 0;
    DWORD       destSectorSize = 512;
    ULONGLONG   destStartLBA = 0;      // 0 = write from the start of the disk (whole-disk overwrite);
                                        // > 0 = write starting at a specific partition's start LBA instead
    ULONGLONG   destTotalSectors = 0;  // sectors AVAILABLE at destStartLBA (whole-disk capacity when
                                        // destStartLBA==0, or the target partition's own sector count otherwise)

    // Image file destination
    CString     destFilePath;
};

struct DiskCopyProgress
{
    ULONGLONG bytesCopied = 0;
    ULONGLONG bytesTotal  = 0;
    bool      finished = false;
    bool      canceled  = false;
    bool      success   = false;
    CString   statusText;
    CString   errorMessage;
};

// Validates a job (capacity checks, sector-size compatibility, source/dest
// overlap, system-drive protection) without touching any disk I/O beyond
// what's needed to check capacity. Call this before Run().
CString ValidateDiskCopyJob(const DiskCopyJob& job);

class CDiskCopyEngine
{
public:
    CDiskCopyEngine();
    ~CDiskCopyEngine();

    // Blocking - intended to be called from a worker thread only.
    bool Run(const DiskCopyJob& job);

    // Safe to call from any thread; cooperative cancellation checked once
    // per chunk (see kChunkBytes below).
    void RequestCancel() { m_cancelRequested = true; }

    // Safe to call from any thread while Run() is executing on another one.
    DiskCopyProgress GetProgressSnapshot() const;

private:
    volatile bool               m_cancelRequested;
    mutable CRITICAL_SECTION    m_progressLock;
    DiskCopyProgress            m_progress;

    void SetProgress(ULONGLONG copied, ULONGLONG total, const CString& status);
    void SetFinished(bool success, bool canceled, const CString& err);

    bool CopyToFile(const DiskCopyJob& job);        // PhysicalDrive -> ImageFile
    bool CopyToDisk(const DiskCopyJob& job);        // PhysicalDrive -> Disk
    bool RestoreFileToDisk(const DiskCopyJob& job); // ImageFile     -> Disk
};

