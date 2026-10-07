// MbrGptConverter.h - Converts a disk's partition table from MBR to GPT
// in place, WITHOUT moving, resizing, or touching any partition's data.
//
// Scope, deliberately conservative:
//   - MBR -> GPT only (GPT -> MBR is lossy - different partition types,
//     4-partition/2TB limits - and is not implemented here).
//   - Only simple primary-partition MBR disks are supported. Disks with an
//     extended partition / logical drives are refused outright, since there
//     is no direct GPT equivalent to linearize them into without judgment
//     calls this tool isn't in a position to make safely.
//   - Only disks that already have enough free space at the very start
//     (LBA 1-33) and very end (last 33 sectors) for the new GPT structures
//     are convertible. This tool never shrinks or moves an existing
//     partition to make room - if there isn't room, it refuses and explains
//     exactly why, rather than attempting anything clever.
//
// Because this constructs and writes brand-new on-disk metadata (as opposed
// to Disk Copy, which only ever moves opaque byte ranges), CaptureAffectedRegions()
// + SaveBackup() exist so the caller can make a byte-exact backup of every
// sector about to be overwritten before writing anything, and
// LoadBackupAndRestore() exists to put it back if something goes wrong.
//
// This is the second module (after DiskCopyEngine) that ever opens a drive
// handle with GENERIC_WRITE.
#pragma once
#include "pch.h"
#include "DiskManager.h"
#include "PartitionParser.h"

struct ConvertPreflightResult
{
    bool    canConvert = false;
    CString blockingReason; // populated when canConvert is false

    std::vector<PartitionEntry> mbrPartitions; // primary partitions found (already parsed, read-only)
    ULONGLONG totalSectors = 0;
    DWORD     sectorSize = 0;
    UINT      driveIndex = 0;
    CString   devicePath;
};

struct BackupRegion
{
    ULONGLONG          startLBA = 0;
    std::vector<BYTE>  data; // size is always a multiple of the disk's sector size
};

class CMbrGptConverter
{
public:
    // Read-only analysis - parses the MBR and decides whether a safe,
    // non-destructive conversion is possible. Never writes anything.
    static ConvertPreflightResult Analyze(CDiskManager& dm);

    // Reads exactly the sector regions ConvertMbrToGpt() would overwrite, so
    // the caller can save them before making any change.
    static std::vector<BackupRegion> CaptureAffectedRegions(CDiskManager& dm, const ConvertPreflightResult& info);

    // Performs the conversion. Only call this after Analyze() returned
    // canConvert == true, and only after the caller has successfully saved a
    // backup via SaveBackup(). Opens devicePath with GENERIC_WRITE directly.
    // Returns an empty CString on success, or an error message.
    static CString ConvertMbrToGpt(const CString& devicePath, const ConvertPreflightResult& info);

    // Backup file I/O - simple custom binary format documented in the .cpp.
    static bool SaveBackup(const CString& filePath, const CString& devicePath, DWORD sectorSize,
                            const std::vector<BackupRegion>& regions, CString& errorOut);
    static bool LoadBackupAndRestore(const CString& filePath, const CString& devicePath, CString& errorOut);

private:
    static DWORD Crc32(const void* data, size_t length);
    static GUID  MbrTypeToGptTypeGuid(BYTE mbrType);
    static GUID  NewRandomGuid();
};
