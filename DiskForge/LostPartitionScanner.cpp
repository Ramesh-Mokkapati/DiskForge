// LostPartitionScanner.cpp
#include "pch.h"
#include "LostPartitionScanner.h"

namespace
{
    inline WORD  ReadU16(const BYTE* p) { WORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline DWORD ReadU32(const BYTE* p) { DWORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline ULONGLONG ReadU64(const BYTE* p) { ULONGLONG v; memcpy(&v, p, sizeof(v)); return v; }

    bool IsPow2InRange(DWORD v, DWORD lo, DWORD hi)
    {
        if (v < lo || v > hi) return false;
        return (v & (v - 1)) == 0;
    }
}

CLostPartitionScanner::CLostPartitionScanner()
    : m_cancelRequested(false)
{
    InitializeCriticalSection(&m_lock);
}

CLostPartitionScanner::~CLostPartitionScanner()
{
    DeleteCriticalSection(&m_lock);
}

std::vector<FreeRange> CLostPartitionScanner::ComputeFreeRanges(ULONGLONG totalSectors, const std::vector<FreeRange>& used)
{
    std::vector<FreeRange> result;

    std::vector<std::pair<ULONGLONG, ULONGLONG>> intervals; // [start, end)
    for (const auto& u : used)
    {
        if (u.sectorCount == 0) continue;
        intervals.push_back({ u.startLBA, u.startLBA + u.sectorCount });
    }
    std::sort(intervals.begin(), intervals.end());

    ULONGLONG cursor = 0;
    for (const auto& iv : intervals)
    {
        ULONGLONG s = std::min<ULONGLONG>(iv.first, totalSectors);
        ULONGLONG e = std::min<ULONGLONG>(iv.second, totalSectors);
        if (s > cursor)
            result.push_back({ cursor, s - cursor });
        cursor = std::max<ULONGLONG>(cursor, e);
    }
    if (cursor < totalSectors)
        result.push_back({ cursor, totalSectors - cursor });

    return result;
}

bool CLostPartitionScanner::TryParseBootSector(const BYTE* s, ULONGLONG lba, DWORD /*assumedSectorSize*/,
                                                LostPartitionCandidate& out)
{
    if (s[510] != 0x55 || s[511] != 0xAA)
        return false;

    // ---- NTFS ----
    if (memcmp(s + 3, "NTFS    ", 8) == 0)
    {
        WORD bps = ReadU16(s + 11);
        BYTE spc = s[13];
        if (!IsPow2InRange(bps, 512, 4096)) return false;
        if (spc == 0 || (spc & (spc - 1)) != 0 || spc > 128) return false;

        out.startLBA = lba;
        out.fsType = DetectedFsType::NTFS;
        out.fsTypeName = _T("NTFS");
        out.bytesPerSector = bps;
        out.sectorsPerCluster = spc;
        out.totalSectorsInVolume = ReadU64(s + 0x28);
        out.volumeLabel.Empty(); // Not stored in the NTFS boot sector itself
        return true;
    }

    // ---- exFAT ----
    if (memcmp(s + 3, "EXFAT   ", 8) == 0)
    {
        BYTE bpsShift = s[108];
        BYTE spcShift = s[109];
        if (bpsShift < 9 || bpsShift > 12) return false; // 512..4096 bytes/sector
        if (spcShift > 25) return false;

        out.startLBA = lba;
        out.fsType = DetectedFsType::ExFAT;
        out.fsTypeName = _T("exFAT");
        out.bytesPerSector = 1u << bpsShift;
        out.sectorsPerCluster = 1u << spcShift;
        out.totalSectorsInVolume = ReadU64(s + 0x48);
        out.volumeLabel.Empty(); // Not stored in the exFAT boot sector itself
        return true;
    }

    // ---- FAT32 ----
    if (memcmp(s + 82, "FAT32   ", 8) == 0)
    {
        WORD bps = ReadU16(s + 11);
        BYTE spc = s[13];
        if (!IsPow2InRange(bps, 512, 4096)) return false;
        if (spc == 0 || (spc & (spc - 1)) != 0 || spc > 128) return false;

        WORD totSec16 = ReadU16(s + 19);
        DWORD totSec32 = ReadU32(s + 32);

        out.startLBA = lba;
        out.fsType = DetectedFsType::FAT32;
        out.fsTypeName = _T("FAT32");
        out.bytesPerSector = bps;
        out.sectorsPerCluster = spc;
        out.totalSectorsInVolume = (totSec16 != 0) ? totSec16 : totSec32;

        char label[12] = {};
        memcpy(label, s + 71, 11);
        CString lbl(label);
        lbl.TrimRight(_T(' '));
        out.volumeLabel = lbl;
        return true;
    }

    // ---- FAT16 / FAT12 ----
    bool isFat16 = memcmp(s + 54, "FAT16   ", 8) == 0;
    bool isFat12 = memcmp(s + 54, "FAT12   ", 8) == 0;
    if (isFat16 || isFat12)
    {
        WORD bps = ReadU16(s + 11);
        BYTE spc = s[13];
        if (!IsPow2InRange(bps, 512, 4096)) return false;
        if (spc == 0 || (spc & (spc - 1)) != 0 || spc > 128) return false;

        WORD totSec16 = ReadU16(s + 19);
        DWORD totSec32 = ReadU32(s + 32);

        out.startLBA = lba;
        out.fsType = isFat12 ? DetectedFsType::FAT12 : DetectedFsType::FAT16;
        out.fsTypeName = isFat12 ? _T("FAT12") : _T("FAT16");
        out.bytesPerSector = bps;
        out.sectorsPerCluster = spc;
        out.totalSectorsInVolume = (totSec16 != 0) ? totSec16 : totSec32;

        char label[12] = {};
        memcpy(label, s + 43, 11);
        CString lbl(label);
        lbl.TrimRight(_T(' '));
        out.volumeLabel = lbl;
        return true;
    }

    return false;
}

void CLostPartitionScanner::AdvanceProgress(ULONGLONG sectorsJustScanned, DWORD /*sectorSize*/)
{
    EnterCriticalSection(&m_lock);
    m_progress.sectorsScanned += sectorsJustScanned;
    m_progress.foundCount = (int)m_candidates.size();

    double pct = (m_progress.sectorsToScan > 0)
        ? (100.0 * (double)m_progress.sectorsScanned / (double)m_progress.sectorsToScan)
        : 100.0;
    CString status;
    status.Format(_T("Scanned %.1f%% of unallocated space - %d candidate(s) found"), pct, m_progress.foundCount);
    m_progress.statusText = status;
    LeaveCriticalSection(&m_lock);
}

void CLostPartitionScanner::AppendCandidate(const LostPartitionCandidate& c)
{
    EnterCriticalSection(&m_lock);
    m_candidates.push_back(c);
    LeaveCriticalSection(&m_lock);
}

void CLostPartitionScanner::SetFinished(bool success, bool canceled, const CString& err)
{
    EnterCriticalSection(&m_lock);
    m_progress.finished = true;
    m_progress.success = success;
    m_progress.canceled = canceled;
    m_progress.errorMessage = err;
    LeaveCriticalSection(&m_lock);
}

LostPartitionScanProgress CLostPartitionScanner::GetProgressSnapshot() const
{
    EnterCriticalSection(&m_lock);
    LostPartitionScanProgress copy = m_progress;
    LeaveCriticalSection(&m_lock);
    return copy;
}

std::vector<LostPartitionCandidate> CLostPartitionScanner::GetCandidatesSnapshot() const
{
    EnterCriticalSection(&m_lock);
    std::vector<LostPartitionCandidate> copy = m_candidates;
    LeaveCriticalSection(&m_lock);
    return copy;
}

bool CLostPartitionScanner::Run(const LostPartitionScanJob& job)
{
    m_cancelRequested = false;

    std::vector<FreeRange> freeRanges = ComputeFreeRanges(job.totalSectors, job.existingPartitions);

    ULONGLONG totalFreeSectors = 0;
    for (const auto& r : freeRanges)
        totalFreeSectors += r.sectorCount;

    EnterCriticalSection(&m_lock);
    m_progress = LostPartitionScanProgress();
    m_progress.sectorsToScan = totalFreeSectors;
    m_progress.statusText = _T("Starting scan...");
    m_candidates.clear();
    LeaveCriticalSection(&m_lock);

    if (totalFreeSectors == 0)
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

    DWORD sectorSize = job.sectorSize;
    const DWORD kTargetChunkBytes = 8 * 1024 * 1024;
    DWORD chunkSectors = kTargetChunkBytes / sectorSize;
    if (chunkSectors == 0) chunkSectors = 1;
    DWORD chunkBytes = chunkSectors * sectorSize;

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

    bool canceled = false;

    for (const auto& range : freeRanges)
    {
        if (m_cancelRequested) { canceled = true; break; }

        ULONGLONG pos = range.startLBA;
        ULONGLONG rangeEnd = range.startLBA + range.sectorCount;

        while (pos < rangeEnd)
        {
            if (m_cancelRequested) { canceled = true; break; }

            ULONGLONG remaining = rangeEnd - pos;
            DWORD thisSectors = static_cast<DWORD>(std::min<ULONGLONG>(chunkSectors, remaining));
            DWORD thisBytes = thisSectors * sectorSize;

            LARGE_INTEGER offset;
            offset.QuadPart = static_cast<LONGLONG>(pos) * sectorSize;
            SetFilePointerEx(hDrive, offset, nullptr, FILE_BEGIN);

            DWORD bytesRead = 0;
            if (!ReadFile(hDrive, buf, thisBytes, &bytesRead, nullptr) || bytesRead != thisBytes)
            {
                // A handful of unreadable sectors shouldn't stop the search
                // for everything else - skip this chunk and keep going.
                pos += thisSectors;
                AdvanceProgress(thisSectors, sectorSize);
                continue;
            }

            const BYTE* base = static_cast<const BYTE*>(buf);
            for (DWORD i = 0; i < thisSectors; ++i)
            {
                const BYTE* sec = base + (size_t)i * sectorSize;
                LostPartitionCandidate cand;
                if (TryParseBootSector(sec, pos + i, sectorSize, cand))
                {
                    // Sanity-guard: if the reported volume size would run past
                    // the physical disk (e.g. this is an NTFS *backup* boot
                    // sector, which lives at a volume's LAST sector and
                    // describes the same volume from there), don't trust the
                    // size - still report the hit, just without a size.
                    if (cand.totalSectorsInVolume == 0 ||
                        cand.startLBA + cand.totalSectorsInVolume > job.totalSectors)
                    {
                        cand.totalSectorsInVolume = 0;
                    }
                    AppendCandidate(cand);
                }
            }

            pos += thisSectors;
            AdvanceProgress(thisSectors, sectorSize);
        }

        if (canceled) break;
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hDrive);

    SetFinished(!canceled, canceled, CString());
    return !canceled;
}
