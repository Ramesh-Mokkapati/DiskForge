// DiskCopyEngine.cpp
#include "pch.h"
#include "DiskCopyEngine.h"
#include "SystemDiskGuard.h"
#include "VolumeLockHelper.h"
#include "PartitionParser.h" // reuse CPartitionParser::FormatSize for status text

namespace
{
    const DWORD kTargetChunkBytes = 4 * 1024 * 1024; // 4 MiB, evenly divisible by 512 and 4096

    DWORD ComputeChunkBytes(DWORD align)
    {
        if (align == 0) align = 512;
        DWORD chunk = (kTargetChunkBytes / align) * align;
        return (chunk == 0) ? align : chunk;
    }

    LPVOID AllocAlignedBuffer(DWORD chunkBytes)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD allocSize = chunkBytes;
        if (allocSize < si.dwAllocationGranularity)
            allocSize = si.dwAllocationGranularity;
        return VirtualAlloc(nullptr, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
}

// ---------------------------------------------------------------------------
CString ValidateDiskCopyJob(const DiskCopyJob& job)
{
    if (job.sourceKind == CopySourceKind::ImageFile && job.destKind == CopyDestKind::ImageFile)
        return _T("Copying one file to another isn't a disk operation - choose a physical disk as the destination.");

    ULONGLONG requiredBytes = 0;

    if (job.sourceKind == CopySourceKind::ImageFile)
    {
        if (job.sourceFilePath.IsEmpty())
            return _T("No source image file selected.");
        if (GetFileAttributes(job.sourceFilePath) == INVALID_FILE_ATTRIBUTES)
            return _T("The source image file could not be found.");
        if (job.sourceFileBytes == 0)
            return _T("The source image file is empty.");
        requiredBytes = job.sourceFileBytes;
    }
    else
    {
        if (job.sourceDevicePath.IsEmpty())
            return _T("No source drive selected.");
        if (job.sourceSectorCount == 0)
            return _T("Nothing to copy: the selected source range is empty.");
        requiredBytes = job.sourceSectorCount * (ULONGLONG)job.sourceSectorSize;
    }

    if (job.destKind == CopyDestKind::Disk)
    {
        if (job.destDevicePath.IsEmpty())
            return _T("No destination drive selected.");

        if (job.sourceKind == CopySourceKind::PhysicalDrive &&
            job.destDevicePath.CompareNoCase(job.sourceDevicePath) == 0)
            return _T("Source and destination cannot be the same physical drive.");

        if (CSystemDiskGuard::IsProtectedDrive(job.destDriveIndex))
            return _T("The selected destination is the drive Windows is currently running from. Refusing to overwrite it.");

        if (job.sourceKind == CopySourceKind::PhysicalDrive && job.sourceSectorSize != job.destSectorSize)
        {
            CString msg;
            msg.Format(
                _T("Source sector size (%u bytes) does not match destination sector size (%u bytes). ")
                _T("Raw disk-to-disk copy requires matching sector sizes - copy to an image file instead."),
                job.sourceSectorSize, job.destSectorSize);
            return msg;
        }

        ULONGLONG destBytes = job.destTotalSectors * (ULONGLONG)job.destSectorSize;
        if (requiredBytes > destBytes)
        {
            CString msg;
            msg.Format(_T("Destination is too small: this copy needs %s but the destination only has %s."),
                CPartitionParser::FormatSize(requiredBytes).GetString(),
                CPartitionParser::FormatSize(destBytes).GetString());
            return msg;
        }
    }
    else
    {
        if (job.destFilePath.IsEmpty())
            return _T("No destination file selected.");
    }

    return CString(); // empty CString == valid
}

// ---------------------------------------------------------------------------
CDiskCopyEngine::CDiskCopyEngine()
    : m_cancelRequested(false)
{
    InitializeCriticalSection(&m_progressLock);
}

CDiskCopyEngine::~CDiskCopyEngine()
{
    DeleteCriticalSection(&m_progressLock);
}

void CDiskCopyEngine::SetProgress(ULONGLONG copied, ULONGLONG total, const CString& status)
{
    EnterCriticalSection(&m_progressLock);
    m_progress.bytesCopied = copied;
    m_progress.bytesTotal = total;
    m_progress.statusText = status;
    LeaveCriticalSection(&m_progressLock);
}

void CDiskCopyEngine::SetFinished(bool success, bool canceled, const CString& err)
{
    EnterCriticalSection(&m_progressLock);
    m_progress.finished = true;
    m_progress.success = success;
    m_progress.canceled = canceled;
    m_progress.errorMessage = err;
    LeaveCriticalSection(&m_progressLock);
}

DiskCopyProgress CDiskCopyEngine::GetProgressSnapshot() const
{
    EnterCriticalSection(&m_progressLock);
    DiskCopyProgress snapshot = m_progress;
    LeaveCriticalSection(&m_progressLock);
    return snapshot;
}

bool CDiskCopyEngine::Run(const DiskCopyJob& job)
{
    m_cancelRequested = false;

    ULONGLONG totalBytes = (job.sourceKind == CopySourceKind::ImageFile)
        ? job.sourceFileBytes
        : job.sourceSectorCount * (ULONGLONG)job.sourceSectorSize;

    // Full reset - important if this engine instance is reused for a second
    // copy in the same session; otherwise a stale "finished" from the
    // previous run would be visible to the poller for an instant.
    EnterCriticalSection(&m_progressLock);
    m_progress = DiskCopyProgress();
    m_progress.bytesTotal = totalBytes;
    m_progress.statusText = _T("Starting...");
    LeaveCriticalSection(&m_progressLock);

    CString validationError = ValidateDiskCopyJob(job);
    if (!validationError.IsEmpty())
    {
        SetFinished(false, false, validationError);
        return false;
    }

    if (job.sourceKind == CopySourceKind::ImageFile)
        return RestoreFileToDisk(job);
    if (job.destKind == CopyDestKind::ImageFile)
        return CopyToFile(job);
    return CopyToDisk(job);
}

// ---------------------------------------------------------------------------
bool CDiskCopyEngine::CopyToFile(const DiskCopyJob& job)
{
    HANDLE hSrc = CreateFile(
        job.sourceDevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hSrc == INVALID_HANDLE_VALUE)
    {
        SetFinished(false, false, _T("Failed to open the source drive for reading."));
        return false;
    }

    HANDLE hDst = CreateFile(
        job.destFilePath, GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hDst == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSrc);
        SetFinished(false, false, _T("Failed to create the destination image file."));
        return false;
    }

    DWORD align = job.sourceSectorSize;
    DWORD chunkBytes = ComputeChunkBytes(align);
    LPVOID buf = AllocAlignedBuffer(chunkBytes);
    if (!buf)
    {
        CloseHandle(hSrc);
        CloseHandle(hDst);
        SetFinished(false, false, _T("Out of memory allocating the copy buffer."));
        return false;
    }

    LARGE_INTEGER srcOffset;
    srcOffset.QuadPart = static_cast<LONGLONG>(job.sourceStartLBA) * job.sourceSectorSize;
    SetFilePointerEx(hSrc, srcOffset, nullptr, FILE_BEGIN);

    ULONGLONG totalBytes = job.sourceSectorCount * (ULONGLONG)job.sourceSectorSize;
    ULONGLONG copied = 0;
    bool ok = true;
    CString err;
    bool canceled = false;

    SetProgress(0, totalBytes, _T("Copying..."));

    while (copied < totalBytes)
    {
        if (m_cancelRequested)
        {
            canceled = true;
            ok = false;
            break;
        }

        DWORD thisChunk = static_cast<DWORD>(std::min<ULONGLONG>(chunkBytes, totalBytes - copied));

        DWORD bytesRead = 0;
        if (!ReadFile(hSrc, buf, thisChunk, &bytesRead, nullptr) || bytesRead != thisChunk)
        {
            err.Format(_T("Read error at source offset %llu (Win32 error %u)."), copied, ::GetLastError());
            ok = false;
            break;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(hDst, buf, bytesRead, &bytesWritten, nullptr) || bytesWritten != bytesRead)
        {
            err.Format(_T("Write error at destination offset %llu (Win32 error %u)."), copied, ::GetLastError());
            ok = false;
            break;
        }

        copied += bytesWritten;

        CString status;
        status.Format(_T("Copied %s of %s"),
            CPartitionParser::FormatSize(copied).GetString(),
            CPartitionParser::FormatSize(totalBytes).GetString());
        SetProgress(copied, totalBytes, status);
    }

    FlushFileBuffers(hDst);
    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hSrc);
    CloseHandle(hDst);

    if (ok && copied >= totalBytes)
    {
        SetFinished(true, false, CString());
        return true;
    }

    // Don't leave a half-written, misleading image file behind.
    DeleteFile(job.destFilePath);

    SetFinished(false, canceled, canceled ? CString() : err);
    return false;
}

// ---------------------------------------------------------------------------
bool CDiskCopyEngine::CopyToDisk(const DiskCopyJob& job)
{
    // Defense in depth - the UI already validated this, but never trust that alone.
    if (CSystemDiskGuard::IsProtectedDrive(job.destDriveIndex))
    {
        SetFinished(false, false, _T("Refusing to write: destination is the drive Windows is running from."));
        return false;
    }

    HANDLE hSrc = CreateFile(
        job.sourceDevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hSrc == INVALID_HANDLE_VALUE)
    {
        SetFinished(false, false, _T("Failed to open the source drive for reading."));
        return false;
    }

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(job.destDriveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        CloseHandle(hSrc);
        SetFinished(false, false,
            _T("Could not lock all volumes on the destination disk for exclusive access. ")
            _T("Close any windows, apps, or drive letters using it (including this app's own ")
            _T("sector view of it) and try again."));
        return false;
    }

    HANDLE hDst = CreateFile(
        job.destDevicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDst == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSrc);
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false,
            _T("Failed to open the destination drive for writing. Make sure you are running as ")
            _T("Administrator and that no other program has it open."));
        return false;
    }

    DWORD align = (std::max)(job.sourceSectorSize, job.destSectorSize);
    DWORD chunkBytes = ComputeChunkBytes(align);
    LPVOID buf = AllocAlignedBuffer(chunkBytes);
    if (!buf)
    {
        CloseHandle(hSrc);
        CloseHandle(hDst);
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false, _T("Out of memory allocating the copy buffer."));
        return false;
    }

    LARGE_INTEGER srcOffset;
    srcOffset.QuadPart = static_cast<LONGLONG>(job.sourceStartLBA) * job.sourceSectorSize;
    SetFilePointerEx(hSrc, srcOffset, nullptr, FILE_BEGIN);

    LARGE_INTEGER dstOffset;
    dstOffset.QuadPart = static_cast<LONGLONG>(job.destStartLBA) * job.destSectorSize;
    SetFilePointerEx(hDst, dstOffset, nullptr, FILE_BEGIN);

    ULONGLONG totalBytes = job.sourceSectorCount * (ULONGLONG)job.sourceSectorSize;
    ULONGLONG copied = 0;
    bool ok = true;
    bool canceled = false;
    CString err;

    SetProgress(0, totalBytes, _T("Copying..."));

    while (copied < totalBytes)
    {
        if (m_cancelRequested)
        {
            canceled = true;
            ok = false;
            break;
        }

        DWORD thisChunk = static_cast<DWORD>(std::min<ULONGLONG>(chunkBytes, totalBytes - copied));

        DWORD bytesRead = 0;
        if (!ReadFile(hSrc, buf, thisChunk, &bytesRead, nullptr) || bytesRead != thisChunk)
        {
            err.Format(_T("Read error at source offset %llu (Win32 error %u)."), copied, ::GetLastError());
            ok = false;
            break;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(hDst, buf, thisChunk, &bytesWritten, nullptr) || bytesWritten != thisChunk)
        {
            err.Format(_T("Write error at destination offset %llu (Win32 error %u)."), copied, ::GetLastError());
            ok = false;
            break;
        }

        copied += thisChunk;

        CString status;
        status.Format(_T("Copied %s of %s"),
            CPartitionParser::FormatSize(copied).GetString(),
            CPartitionParser::FormatSize(totalBytes).GetString());
        SetProgress(copied, totalBytes, status);
    }

    FlushFileBuffers(hDst);
    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hSrc);
    CloseHandle(hDst);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);

    if (ok && copied >= totalBytes)
    {
        SetFinished(true, false, CString());
        return true;
    }

    SetFinished(false, canceled, canceled ? CString() : err);
    return false;
}

// ---------------------------------------------------------------------------
bool CDiskCopyEngine::RestoreFileToDisk(const DiskCopyJob& job)
{
    // Defense in depth - the UI already validated this, but never trust that alone.
    if (CSystemDiskGuard::IsProtectedDrive(job.destDriveIndex))
    {
        SetFinished(false, false, _T("Refusing to write: destination is the drive Windows is running from."));
        return false;
    }

    HANDLE hSrc = CreateFile(
        job.sourceFilePath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hSrc == INVALID_HANDLE_VALUE)
    {
        SetFinished(false, false, _T("Failed to open the source image file for reading."));
        return false;
    }

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(job.destDriveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        CloseHandle(hSrc);
        SetFinished(false, false,
            _T("Could not lock all volumes on the destination disk for exclusive access. ")
            _T("Close any windows, apps, or drive letters using it and try again."));
        return false;
    }

    HANDLE hDst = CreateFile(
        job.destDevicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDst == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSrc);
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false,
            _T("Failed to open the destination drive for writing. Make sure you are running as ")
            _T("Administrator and that no other program has it open."));
        return false;
    }

    // The image file itself has no alignment requirement, but every raw write
    // to the destination disk handle does - align (and, for the final
    // possibly-short chunk, zero-pad) everything to the destination's sector size.
    DWORD align = job.destSectorSize;
    DWORD chunkBytes = ComputeChunkBytes(align);
    LPVOID buf = AllocAlignedBuffer(chunkBytes);
    if (!buf)
    {
        CloseHandle(hSrc);
        CloseHandle(hDst);
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false, _T("Out of memory allocating the copy buffer."));
        return false;
    }

    LARGE_INTEGER dstOffset;
    dstOffset.QuadPart = static_cast<LONGLONG>(job.destStartLBA) * job.destSectorSize;
    SetFilePointerEx(hDst, dstOffset, nullptr, FILE_BEGIN);

    ULONGLONG totalBytes = job.sourceFileBytes;
    ULONGLONG copied = 0;
    bool ok = true;
    bool canceled = false;
    CString err;

    SetProgress(0, totalBytes, _T("Restoring..."));

    while (copied < totalBytes)
    {
        if (m_cancelRequested)
        {
            canceled = true;
            ok = false;
            break;
        }

        ULONGLONG remaining = totalBytes - copied;
        DWORD wantBytes = static_cast<DWORD>(std::min<ULONGLONG>(chunkBytes, remaining));

        ZeroMemory(buf, chunkBytes); // so any end-of-file padding below is zero, not stale buffer contents
        DWORD bytesRead = 0;
        if (!ReadFile(hSrc, buf, wantBytes, &bytesRead, nullptr))
        {
            err.Format(_T("Read error at source offset %llu (Win32 error %u)."), copied, ::GetLastError());
            ok = false;
            break;
        }

        // Round the write up to a full destination sector; the raw disk
        // handle requires sector-aligned write lengths even on the last,
        // possibly-short chunk.
        DWORD writeBytes = ((bytesRead + align - 1) / align) * align;
        if (writeBytes == 0)
            writeBytes = align;

        DWORD bytesWritten = 0;
        if (!WriteFile(hDst, buf, writeBytes, &bytesWritten, nullptr) || bytesWritten != writeBytes)
        {
            err.Format(_T("Write error at destination offset %llu (Win32 error %u)."), copied, ::GetLastError());
            ok = false;
            break;
        }

        copied += bytesRead;

        CString status;
        status.Format(_T("Restored %s of %s"),
            CPartitionParser::FormatSize(copied).GetString(),
            CPartitionParser::FormatSize(totalBytes).GetString());
        SetProgress(copied, totalBytes, status);

        if (bytesRead < wantBytes)
            break; // Hit EOF earlier than expected - stop rather than loop forever
    }

    FlushFileBuffers(hDst);
    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hSrc);
    CloseHandle(hDst);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);

    if (ok && copied >= totalBytes)
    {
        SetFinished(true, false, CString());
        return true;
    }

    SetFinished(false, canceled, canceled ? CString() : err);
    return false;
}

