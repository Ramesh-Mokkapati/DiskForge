// BadSectorScanner.cpp
#include "pch.h"
#include "BadSectorScanner.h"

namespace
{
    // Heuristic only: a chunk read this much slower than a healthy chunk of
    // the same size suggests the drive had to retry/recover, which is worth
    // flagging even if the read ultimately succeeded. Expressed as a minimum
    // throughput in MB/s below which a block is marked "Slow".
    const double kSlowThroughputMBps = 5.0;
}

DWORD CBadSectorScanner::ComputeChunkSectors(ULONGLONG totalSectors, DWORD sectorSize)
{
    if (sectorSize == 0) sectorSize = 512;

    const ULONGLONG desiredBlocks = 1000;
    ULONGLONG chunk = (totalSectors > 0) ? (totalSectors / desiredBlocks) : 256;

    const ULONGLONG minChunkSectors = 256; // floor, so tiny disks still get a sensible chunk size
    if (chunk < minChunkSectors) chunk = minChunkSectors;

    const ULONGLONG maxChunkBytes = 64ULL * 1024 * 1024; // cap so progress updates stay frequent
    ULONGLONG maxChunkSectors = maxChunkBytes / sectorSize;
    if (maxChunkSectors == 0) maxChunkSectors = 1;
    if (chunk > maxChunkSectors) chunk = maxChunkSectors;

    return static_cast<DWORD>(chunk);
}

CBadSectorScanner::CBadSectorScanner()
    : m_cancelRequested(false)
{
    InitializeCriticalSection(&m_lock);
}

CBadSectorScanner::~CBadSectorScanner()
{
    DeleteCriticalSection(&m_lock);
}

void CBadSectorScanner::AppendBlock(const ScanBlockResult& block)
{
    EnterCriticalSection(&m_lock);
    m_blocks.push_back(block);
    LeaveCriticalSection(&m_lock);
}

void CBadSectorScanner::RecordBadRange(ULONGLONG startLBA, ULONGLONG count)
{
    EnterCriticalSection(&m_lock);
    if (!m_badRanges.empty())
    {
        BadSectorRange& last = m_badRanges.back();
        if (last.startLBA + last.sectorCount == startLBA)
        {
            last.sectorCount += count;
            LeaveCriticalSection(&m_lock);
            return;
        }
    }
    m_badRanges.push_back({ startLBA, count });
    LeaveCriticalSection(&m_lock);
}

void CBadSectorScanner::AdvanceProgress(ULONGLONG sectors, DWORD sectorSize)
{
    EnterCriticalSection(&m_lock);
    m_progress.bytesScanned += sectors * (ULONGLONG)sectorSize;
    m_progress.badSectorsFound = 0;
    for (const auto& r : m_badRanges)
        m_progress.badSectorsFound += r.sectorCount;

    CString status;
    double pct = (m_progress.bytesTotal > 0)
        ? (100.0 * (double)m_progress.bytesScanned / (double)m_progress.bytesTotal)
        : 0.0;
    status.Format(_T("Scanned %.1f%% - %llu bad sector(s) found so far"),
                  pct, m_progress.badSectorsFound);
    m_progress.statusText = status;
    LeaveCriticalSection(&m_lock);
}

void CBadSectorScanner::SetFinished(bool success, bool canceled, const CString& err)
{
    EnterCriticalSection(&m_lock);
    m_progress.finished = true;
    m_progress.success  = success;
    m_progress.canceled = canceled;
    m_progress.errorMessage = err;
    LeaveCriticalSection(&m_lock);
}

ScanProgress CBadSectorScanner::GetProgressSnapshot() const
{
    EnterCriticalSection(&m_lock);
    ScanProgress copy = m_progress;
    LeaveCriticalSection(&m_lock);
    return copy;
}

std::vector<ScanBlockResult> CBadSectorScanner::GetBlocksSnapshot() const
{
    EnterCriticalSection(&m_lock);
    std::vector<ScanBlockResult> copy = m_blocks;
    LeaveCriticalSection(&m_lock);
    return copy;
}

std::vector<BadSectorRange> CBadSectorScanner::GetBadRangesSnapshot() const
{
    EnterCriticalSection(&m_lock);
    std::vector<BadSectorRange> copy = m_badRanges;
    LeaveCriticalSection(&m_lock);
    return copy;
}

bool CBadSectorScanner::TryReadRange(HANDLE hDrive, DWORD sectorSize, ULONGLONG startLBA,
                                      ULONGLONG count, LPVOID buf, DWORD bufCapacityBytes, DWORD& elapsedMsOut)
{
    elapsedMsOut = 0;

    ULONGLONG bytesToReadU64 = count * (ULONGLONG)sectorSize;
    if (bytesToReadU64 > bufCapacityBytes)
        return false; // Shouldn't happen given how ScanRange is driven

    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(startLBA) * sectorSize;
    if (!SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN))
        return false;

    DWORD bytesToRead = static_cast<DWORD>(bytesToReadU64);
    DWORD t0 = GetTickCount();
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hDrive, buf, bytesToRead, &bytesRead, nullptr);
    DWORD t1 = GetTickCount();
    elapsedMsOut = t1 - t0;

    return ok && bytesRead == bytesToRead;
}

void CBadSectorScanner::ScanRange(HANDLE hDrive, DWORD sectorSize, ULONGLONG startLBA,
                                   ULONGLONG count, LPVOID buf, DWORD bufCapacityBytes)
{
    if (m_cancelRequested || count == 0)
        return;

    DWORD elapsedMs = 0;
    bool ok = TryReadRange(hDrive, sectorSize, startLBA, count, buf, bufCapacityBytes, elapsedMs);

    if (ok)
    {
        ScanBlockResult block;
        block.startLBA    = startLBA;
        block.sectorCount = count;
        block.readTimeMs  = elapsedMs;

        double seconds = elapsedMs / 1000.0;
        double mb = (count * (double)sectorSize) / (1024.0 * 1024.0);
        double mbps = (seconds > 0.0) ? (mb / seconds) : 1e9;

        block.status = (mbps < kSlowThroughputMBps) ? SectorBlockStatus::Slow : SectorBlockStatus::Ok;

        AppendBlock(block);
        AdvanceProgress(count, sectorSize);
        return;
    }

    if (count == 1)
    {
        ScanBlockResult block;
        block.startLBA    = startLBA;
        block.sectorCount = 1;
        block.status      = SectorBlockStatus::Bad;
        block.readTimeMs  = elapsedMs;

        AppendBlock(block);
        RecordBadRange(startLBA, 1);
        AdvanceProgress(1, sectorSize);
        return;
    }

    // Bisect and retry each half separately, to narrow down the exact bad sector(s).
    ULONGLONG half = count / 2;
    ScanRange(hDrive, sectorSize, startLBA, half, buf, bufCapacityBytes);
    if (m_cancelRequested) return;
    ScanRange(hDrive, sectorSize, startLBA + half, count - half, buf, bufCapacityBytes);
}

bool CBadSectorScanner::Run(const ScanJob& job)
{
    m_cancelRequested = false;

    EnterCriticalSection(&m_lock);
    m_progress = ScanProgress();
    m_progress.bytesTotal = job.sectorCount * (ULONGLONG)job.sectorSize;
    m_progress.statusText = _T("Starting scan...");
    m_blocks.clear();
    m_badRanges.clear();
    LeaveCriticalSection(&m_lock);

    if (job.sectorCount == 0)
    {
        SetFinished(true, false, CString());
        return true;
    }

    HANDLE hDrive = CreateFile(
        job.devicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        SetFinished(false, false, _T("Failed to open the drive for reading."));
        return false;
    }

    DWORD chunkSectors = ComputeChunkSectors(job.sectorCount, job.sectorSize);
    DWORD chunkBytes   = chunkSectors * job.sectorSize;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD allocSize = chunkBytes;
    if (allocSize < si.dwAllocationGranularity)
        allocSize = si.dwAllocationGranularity;

    LPVOID buf = VirtualAlloc(nullptr, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf)
    {
        CloseHandle(hDrive);
        SetFinished(false, false, _T("Out of memory allocating the scan buffer."));
        return false;
    }

    ULONGLONG pos    = job.startLBA;
    ULONGLONG endLBA = job.startLBA + job.sectorCount;

    while (pos < endLBA)
    {
        if (m_cancelRequested)
            break;

        ULONGLONG remaining = endLBA - pos;
        ULONGLONG thisCount = std::min<ULONGLONG>(chunkSectors, remaining);

        ScanRange(hDrive, job.sectorSize, pos, thisCount, buf, chunkBytes);
        pos += thisCount;
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hDrive);

    bool canceled = m_cancelRequested;
    SetFinished(!canceled, canceled, CString());
    return !canceled;
}
