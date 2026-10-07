// Fat32Reader.h - Read-only FAT32 filesystem parser: boot sector, FAT chain
// walking, directory listing (including deleted entries), and file recovery.
//
// SCOPE, deliberately narrow:
//   - FAT32 only. Not FAT12, FAT16, exFAT, or NTFS - each has a different
//     enough on-disk structure that supporting them well needs its own pass;
//     bolting them onto this reader would risk getting all of them wrong.
//   - Reads raw LBAs directly, independent of whether Windows has mounted
//     the volume or shows it as "RAW"/unformatted - as long as the FAT32
//     metadata is still physically present, this can read it. That also
//     means it does NOT help after a full reformat with a different
//     filesystem (which overwrites this metadata) - recovering data after
//     that requires file-signature carving through raw unallocated space,
//     a different technique this module does not implement.
//   - Deleted-file recovery is inherently best-effort: FAT clears a
//     deleted file's cluster-chain links in the FAT table itself (only the
//     starting cluster survives in the directory entry), so recovery here
//     assumes the file's clusters are CONTIGUOUS starting from that first
//     cluster. This is correct for the common case (especially on flash
//     media, and for files that were never rewritten in place) but will
//     produce corrupted output for a deleted file that was fragmented -
//     there is no way to know that from FAT metadata alone. Live
//     (non-deleted) files instead properly follow their intact FAT chain.
//   - A deleted short (8.3) directory entry has its first character
//     overwritten by the OS (0xE5), so that character is permanently lost;
//     recovered deleted-file names show it as "_". Preceding long-filename
//     entries are used when still present and their checksum still matches.
#pragma once
#include "pch.h"
#include "DiskManager.h"

struct FatVolumeInfo
{
    bool      valid = false;
    CString   errorMessage;

    ULONGLONG volumeStartLBA = 0; // absolute LBA on the physical disk
    DWORD     bytesPerSector = 512;
    DWORD     sectorsPerCluster = 8;
    DWORD     numFats = 2;
    ULONGLONG fatStartLBA = 0;    // absolute LBA of FAT #1
    DWORD     sectorsPerFat = 0;
    DWORD     rootCluster = 2;
    ULONGLONG dataStartLBA = 0;   // absolute LBA where cluster #2 begins
    ULONGLONG totalClusters = 0;
    CString   volumeLabel;
};

struct FatDirEntry
{
    CString   name;
    bool      isDirectory = false;
    bool      isDeleted = false;
    DWORD     firstCluster = 0;
    ULONGLONG fileSize = 0;
};

class CFat32Reader
{
public:
    // Parses the boot sector at volumeStartLBA. Rejects anything that
    // doesn't look like FAT32 (checked via the "FAT32   " filesystem-type
    // string and BPB sanity bounds, same checks LostPartitionScanner uses).
    static FatVolumeInfo OpenVolume(CDiskManager& dm, ULONGLONG volumeStartLBA, DWORD sectorSize);

    // Lists one directory's entries (both live and deleted), given its
    // first cluster (pass vol.rootCluster for the root directory).
    static std::vector<FatDirEntry> ReadDirectory(CDiskManager& dm, const FatVolumeInfo& vol, DWORD dirCluster);

    // Follows the FAT chain from startCluster until an end-of-chain marker,
    // a free/bad cluster, or maxClusters is reached (loop guard). Intended
    // for LIVE files/directories, whose chain should still be intact.
    static std::vector<DWORD> GetClusterChain(CDiskManager& dm, const FatVolumeInfo& vol,
                                               DWORD startCluster, ULONGLONG maxClusters = 1000000);

    // Recovers a file to outputPath.
    //   assumeContiguous = true  -> reads clusters sequentially from
    //     startCluster (the only option for deleted files - see header note).
    //   assumeContiguous = false -> follows the FAT chain properly (for live files).
    // Returns an empty CString on success (bytesWrittenOut receives the
    // actual byte count, which may be less than fileSize if the volume
    // ran out of clusters before reaching it), or an error message.
    static CString RecoverFile(CDiskManager& dm, const FatVolumeInfo& vol, DWORD startCluster,
                                ULONGLONG fileSize, bool assumeContiguous, const CString& outputPath,
                                ULONGLONG& bytesWrittenOut);

private:
    static bool ReadCluster(CDiskManager& dm, const FatVolumeInfo& vol, DWORD clusterNum, std::vector<BYTE>& out);
    static bool ReadFatEntry(CDiskManager& dm, const FatVolumeInfo& vol, DWORD clusterNum, DWORD& nextOut);
};
