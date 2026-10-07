// BadSectorScanner.h - Read-only surface scan.
//
// Sequentially reads a drive (or a chosen range of it) in chunks, timing
// each read. A chunk that fails to read is recursively bisected down to
// individual sectors so the exact failing sector(s) can be pinpointed,
// without aborting the rest of the scan.
//
// IMPORTANT CAVEAT (surfaced in the UI too): a drive's firmware transparently
// remaps sectors it has already found to be bad, so a plain read-based scan
// like this one can only ever report sectors that are *currently* failing to
// read - it cannot see sectors the drive has already reallocated, and it is
// not a substitute for the drive's own SMART self-test data.
#pragma once
#include "pch.h"

enum class SectorBlockStatus { Ok, Slow, Bad };

struct ScanBlockResult
{
    ULONGLONG          startLBA = 0;
    ULONGLONG          sectorCount = 0;
    SectorBlockStatus  status = SectorBlockStatus::Ok;
    DWORD              readTimeMs = 0; // meaningful for Ok/Slow only
};

struct BadSectorRange
{
    ULONGLONG startLBA;
    ULONGLONG sectorCount;
};

struct ScanJob
{
    CString    devicePath;
    DWORD      sectorSize = 512;
    ULONGLONG  startLBA = 0;
    ULONGLONG  sectorCount = 0; // sectors to scan, starting at startLBA
};

struct ScanProgress
{
    ULONGLONG bytesScanned = 0;
    ULONGLONG bytesTotal   = 0;
    ULONGLONG badSectorsFound = 0;
    bool      finished = false;
    bool      canceled  = false;
    bool      success   = false;
    CString   statusText;
    CString   errorMessage;
};

class CBadSectorScanner
{
public:
    CBadSectorScanner();
    ~CBadSectorScanner();

    // Blocking - call from a worker thread only.
    bool Run(const ScanJob& job);

    // Safe from any thread.
    void RequestCancel() { m_cancelRequested = true; }

    // Safe from any thread while Run() executes on another one.
    ScanProgress GetProgressSnapshot() const;
    std::vector<ScanBlockResult> GetBlocksSnapshot() const;
    std::vector<BadSectorRange>  GetBadRangesSnapshot() const;

    // Exposed so the UI can pre-compute how many display chunks a scan of
    // this size will (at minimum) involve, before Run() has produced any.
    static DWORD ComputeChunkSectors(ULONGLONG totalSectors, DWORD sectorSize);

private:
    volatile bool              m_cancelRequested;
    mutable CRITICAL_SECTION   m_lock;
    ScanProgress               m_progress;
    std::vector<ScanBlockResult> m_blocks;
    std::vector<BadSectorRange>  m_badRanges;

    void AppendBlock(const ScanBlockResult& block);
    void AdvanceProgress(ULONGLONG sectors, DWORD sectorSize);
    void SetFinished(bool success, bool canceled, const CString& err);
    void RecordBadRange(ULONGLONG startLBA, ULONGLONG count);

    bool TryReadRange(HANDLE hDrive, DWORD sectorSize, ULONGLONG startLBA,
                       ULONGLONG count, LPVOID buf, DWORD bufCapacityBytes, DWORD& elapsedMsOut);

    // Recursively narrows down a failing range; appends completed blocks as it goes.
    void ScanRange(HANDLE hDrive, DWORD sectorSize, ULONGLONG startLBA,
                    ULONGLONG count, LPVOID buf, DWORD bufCapacityBytes);
};
