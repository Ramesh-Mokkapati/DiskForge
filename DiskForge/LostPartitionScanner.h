// LostPartitionScanner.h - Read-only scan for filesystem boot-sector
// signatures (NTFS/FAT32/FAT16/FAT12/exFAT) sitting in space the CURRENT
// partition table doesn't claim, to find partitions that used to exist but
// whose table entry is now missing or corrupted (the classic "lost
// partition" scenario after an accidental delete or table corruption).
//
// Scope, deliberately conservative:
//   - Only scans space not already covered by an existing partition entry -
//     both for correctness (an existing partition's own boot sector isn't
//     "lost") and for speed (skipping known-allocated regions on an
//     otherwise-full disk meaningfully shortens the scan).
//   - Only reads and parses each candidate's boot sector - never mounts or
//     walks into the filesystem itself. Volume labels are reported when
//     the filesystem stores them directly in the boot sector (FAT12/16/32);
//     NTFS and exFAT store labels elsewhere (MFT / a directory entry) which
//     would need actual filesystem parsing, so those are left blank here.
//   - READ-ONLY. This module never writes anything. Turning a found
//     candidate back into a live partition table entry (i.e. creating a new
//     partition) is a separate capability this scanner does not perform.
#pragma once
#include "pch.h"
#include "DiskManager.h"

enum class DetectedFsType { NTFS, FAT32, FAT16, FAT12, ExFAT };

struct LostPartitionCandidate
{
    ULONGLONG      startLBA = 0;
    DetectedFsType fsType = DetectedFsType::NTFS;
    CString        fsTypeName;
    ULONGLONG      totalSectorsInVolume = 0; // as reported by the volume's own boot sector; 0 if implausible/unknown
    DWORD          bytesPerSector = 0;
    DWORD          sectorsPerCluster = 0;
    CString        volumeLabel; // best-effort; blank for NTFS/exFAT (see header note)
};

struct FreeRange { ULONGLONG startLBA; ULONGLONG sectorCount; };

struct LostPartitionScanJob
{
    CString    devicePath;
    DWORD      sectorSize = 512;
    UINT       driveIndex = 0;
    ULONGLONG  totalSectors = 0; // whole disk size, used together with existingPartitions to compute free space

    // Ranges already claimed by the current partition table - the scan skips these.
    std::vector<FreeRange> existingPartitions;
};

struct LostPartitionScanProgress
{
    ULONGLONG sectorsScanned = 0;
    ULONGLONG sectorsToScan  = 0; // total across only the free (unallocated) ranges being scanned
    int       foundCount = 0;
    bool      finished = false;
    bool      canceled  = false;
    bool      success   = false;
    CString   statusText;
    CString   errorMessage;
};

class CLostPartitionScanner
{
public:
    CLostPartitionScanner();
    ~CLostPartitionScanner();

    // Blocking - call from a worker thread only.
    bool Run(const LostPartitionScanJob& job);

    void RequestCancel() { m_cancelRequested = true; }

    LostPartitionScanProgress GetProgressSnapshot() const;
    std::vector<LostPartitionCandidate> GetCandidatesSnapshot() const;

    // Computes the free (unallocated) ranges within [0, totalSectors) given
    // the partitions already in the table. Exposed for the UI to show how
    // much space will actually be scanned before starting.
    static std::vector<FreeRange> ComputeFreeRanges(ULONGLONG totalSectors, const std::vector<FreeRange>& used);

private:
    volatile bool              m_cancelRequested;
    mutable CRITICAL_SECTION   m_lock;
    LostPartitionScanProgress  m_progress;
    std::vector<LostPartitionCandidate> m_candidates;

    void AdvanceProgress(ULONGLONG sectorsJustScanned, DWORD sectorSize);
    void AppendCandidate(const LostPartitionCandidate& c);
    void SetFinished(bool success, bool canceled, const CString& err);

    static bool TryParseBootSector(const BYTE* sector, ULONGLONG lba, DWORD assumedSectorSize,
                                    LostPartitionCandidate& out);
};
