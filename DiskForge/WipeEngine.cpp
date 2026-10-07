// WipeEngine.cpp
#include "pch.h"
#include "WipeEngine.h"
#include "SystemDiskGuard.h"
#include "VolumeLockHelper.h"
#include "PartitionParser.h" // reuse CPartitionParser::FormatSize for status text

namespace
{
    const DWORD kTargetChunkBytes = 4 * 1024 * 1024;

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

CString ValidateWipeJob(const WipeJob& job)
{
    if (job.devicePath.IsEmpty())
        return _T("No drive selected.");
    if (job.sectorCount == 0)
        return _T("Nothing to wipe: the selected range is empty.");
    if (CSystemDiskGuard::IsProtectedDrive(job.driveIndex))
        return _T("The selected drive is the one Windows is running from. Refusing to wipe it.");
    return CString();
}

int CWipeEngine::PassCountFor(WipePattern p)
{
    switch (p)
    {
        case WipePattern::Zeros:            return 1;
        case WipePattern::Random:           return 1;
        case WipePattern::ZerosThenRandom:  return 2;
        case WipePattern::RandomThreePass:  return 3;
    }
    return 1;
}

bool CWipeEngine::IsZeroPass(WipePattern p, int passIndex)
{
    switch (p)
    {
        case WipePattern::Zeros:            return true;
        case WipePattern::Random:           return false;
        case WipePattern::ZerosThenRandom:  return passIndex == 0; // pass 0 = zeros, pass 1 = random
        case WipePattern::RandomThreePass:  return false;          // all three passes random
    }
    return true;
}

CWipeEngine::CWipeEngine()
    : m_cancelRequested(false)
{
    InitializeCriticalSection(&m_progressLock);
}

CWipeEngine::~CWipeEngine()
{
    DeleteCriticalSection(&m_progressLock);
}

void CWipeEngine::SetProgress(ULONGLONG written, ULONGLONG total, int pass, int totalPasses, const CString& status)
{
    EnterCriticalSection(&m_progressLock);
    m_progress.bytesWritten = written;
    m_progress.bytesTotal   = total;
    m_progress.currentPass  = pass;
    m_progress.totalPasses  = totalPasses;
    m_progress.statusText   = status;
    LeaveCriticalSection(&m_progressLock);
}

void CWipeEngine::SetFinished(bool success, bool canceled, const CString& err)
{
    EnterCriticalSection(&m_progressLock);
    m_progress.finished     = true;
    m_progress.success      = success;
    m_progress.canceled     = canceled;
    m_progress.errorMessage = err;
    LeaveCriticalSection(&m_progressLock);
}

WipeProgress CWipeEngine::GetProgressSnapshot() const
{
    EnterCriticalSection(&m_progressLock);
    WipeProgress copy = m_progress;
    LeaveCriticalSection(&m_progressLock);
    return copy;
}

bool CWipeEngine::Run(const WipeJob& job)
{
    m_cancelRequested = false;

    int totalPasses = PassCountFor(job.pattern);
    ULONGLONG bytesPerPass = job.sectorCount * (ULONGLONG)job.sectorSize;
    ULONGLONG totalBytesAllPasses = bytesPerPass * (ULONGLONG)totalPasses;

    EnterCriticalSection(&m_progressLock);
    m_progress = WipeProgress();
    m_progress.bytesTotal  = totalBytesAllPasses;
    m_progress.totalPasses = totalPasses;
    m_progress.statusText  = _T("Starting...");
    LeaveCriticalSection(&m_progressLock);

    CString validationError = ValidateWipeJob(job);
    if (!validationError.IsEmpty())
    {
        SetFinished(false, false, validationError);
        return false;
    }

    bool locksOk = true;
    std::vector<HANDLE> lockedVolumes = CVolumeLockHelper::LockAndDismountVolumesOnDisk(job.driveIndex, locksOk);
    if (!locksOk)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false,
            _T("Could not lock all volumes on this disk for exclusive access. Close any windows, apps, ")
            _T("or drive letters using it (including this app's own sector view of it) and try again."));
        return false;
    }

    HANDLE hDrive = CreateFile(
        job.devicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false,
            _T("Failed to open the drive for writing. Make sure you're running as Administrator and ")
            _T("that no other program has it open."));
        return false;
    }

    DWORD align = job.sectorSize;
    DWORD chunkBytes = ComputeChunkBytes(align);
    LPVOID buf = AllocAlignedBuffer(chunkBytes);
    if (!buf)
    {
        CloseHandle(hDrive);
        CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);
        SetFinished(false, false, _T("Out of memory allocating the wipe buffer."));
        return false;
    }

    srand(GetTickCount()); // Not cryptographically secure - see header note

    bool ok = true;
    bool canceled = false;
    CString err;
    ULONGLONG bytesWrittenAllPasses = 0;

    for (int pass = 0; pass < totalPasses && ok; ++pass)
    {
        bool zeroPass = IsZeroPass(job.pattern, pass);

        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(job.startLBA) * job.sectorSize;
        SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN);

        ULONGLONG passWritten = 0;
        while (passWritten < bytesPerPass)
        {
            if (m_cancelRequested)
            {
                canceled = true;
                ok = false;
                break;
            }

            DWORD thisChunk = static_cast<DWORD>(std::min<ULONGLONG>(chunkBytes, bytesPerPass - passWritten));

            if (zeroPass)
            {
                ZeroMemory(buf, thisChunk);
            }
            else
            {
                BYTE* p = static_cast<BYTE*>(buf);
                for (DWORD i = 0; i < thisChunk; ++i)
                    p[i] = static_cast<BYTE>(rand() & 0xFF);
            }

            DWORD bytesWritten = 0;
            if (!WriteFile(hDrive, buf, thisChunk, &bytesWritten, nullptr) || bytesWritten != thisChunk)
            {
                err.Format(_T("Write error at offset %llu on pass %d (Win32 error %u)."),
                            passWritten, pass + 1, ::GetLastError());
                ok = false;
                break;
            }

            passWritten += thisChunk;
            bytesWrittenAllPasses += thisChunk;

            CString status;
            status.Format(_T("Pass %d of %d - %s of %s"),
                           pass + 1, totalPasses,
                           CPartitionParser::FormatSize(bytesWrittenAllPasses).GetString(),
                           CPartitionParser::FormatSize(totalBytesAllPasses).GetString());
            SetProgress(bytesWrittenAllPasses, totalBytesAllPasses, pass + 1, totalPasses, status);
        }
    }

    FlushFileBuffers(hDrive);
    VirtualFree(buf, 0, MEM_RELEASE);

    if (ok)
    {
        DWORD dummy = 0;
        DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, nullptr, 0, nullptr, 0, &dummy, nullptr);
    }

    CloseHandle(hDrive);
    CVolumeLockHelper::ReleaseLockedVolumes(lockedVolumes);

    if (ok && bytesWrittenAllPasses >= totalBytesAllPasses)
    {
        SetFinished(true, false, CString());
        return true;
    }

    SetFinished(false, canceled, canceled ? CString() : err);
    return false;
}
