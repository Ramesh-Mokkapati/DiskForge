// PartitionEditor.h - Low-risk partition metadata edits: active/boot flag,
// partition type, hidden state, GPT partition name, and GPT attribute bits.
// Also handles drive-letter assignment, which is a completely separate
// Windows Mount Manager operation that involves no raw disk I/O at all.
//
// Scope, deliberately narrow: this only ever rewrites the partition TABLE
// entry for one partition (a few dozen bytes) - it never touches partition
// boundaries (that's Partition Manager's resize/move/split/merge, a
// separate, much higher-risk feature) and never touches filesystem
// structures inside the partition (formatting, labels stored in the
// filesystem itself, etc.).
//
// This is the third module (after DiskCopyEngine and MbrGptConverter) that
// ever opens a drive handle with GENERIC_WRITE.
#pragma once
#include "pch.h"
#include "DiskManager.h"
#include "PartitionParser.h"

struct PartitionEditFields
{
    CString scheme; // "MBR" or "GPT", copied from the source PartitionEntry

    // MBR
    bool    mbrActive = false;
    BYTE    mbrTypeByte = 0;

    // GPT
    GUID    gptTypeGuid = {};
    CString gptName;
    bool    gptLegacyBiosBootable = false; // Attribute bit 2 - informal MBR "active" analogue
    bool    gptNoDriveLetter = false;      // Attribute bit 63 - Windows won't auto-assign a letter
};

class CPartitionEditor
{
public:
    static PartitionEditFields ReadFields(const PartitionEntry& entry);

    // Rewrites one MBR primary partition entry (sector 0, offset 446 + 16*slot).
    // slot is 0-3. Locks/dismounts volumes on the disk first, like every
    // other write-capable module in this app.
    static CString WriteMbrEntry(const CString& devicePath, UINT driveIndex, int slot,
                                  const PartitionEditFields& fields,
                                  ULONGLONG startLBA, ULONGLONG sectorCount);

    // Rewrites one GPT partition entry in BOTH the primary and backup
    // partition arrays, and recomputes both header CRC32s so the disk stays
    // internally consistent (a mismatched backup can make firmware/OS tools
    // report the disk as corrupt). entryIndex is 0-based (0..127).
    static CString WriteGptEntry(const CString& devicePath, UINT driveIndex, int entryIndex,
                                  const PartitionEditFields& fields,
                                  ULONGLONG startLBA, ULONGLONG sectorCount);

    // MBR hidden/visible type-byte pairs (0x07<->0x17, etc.). Returns the
    // input unchanged if there's no known counterpart for that type.
    static BYTE ToggleMbrHidden(BYTE type, bool makeHidden);
    static bool IsMbrTypeHideable(BYTE type);
    static bool IsMbrTypeHidden(BYTE type);

    // ---- Drive letter management (Windows Mount Manager - no raw disk I/O) ----
    struct VolumeMatch
    {
        bool    found = false;
        CString volumeGuidPath;     // "\\?\Volume{GUID}\"
        TCHAR   currentDriveLetter = 0; // 0 if none assigned
    };

    // Finds the volume (if any) that Windows has mounted for the partition
    // starting at startLBA on diskIndex.
    static VolumeMatch FindVolumeForPartition(UINT diskIndex, ULONGLONG startLBA);

    // Assigns newLetter (e.g. 'D') as this volume's drive letter, replacing
    // any letter it currently has. Pass newLetter = 0 to just remove the
    // existing assignment without adding a new one.
    static CString SetDriveLetter(const VolumeMatch& vol, TCHAR newLetter);

    // Letters not currently used by any volume.
    static std::vector<TCHAR> GetAvailableDriveLetters();

    // ---- Delete partition entry (zeroes the slot in the partition table) ----
    // For MBR: slot is 0-3 (the rawSlotIndex from PartitionEntry).
    // For GPT: entryIndex is the 0-based position in both partition arrays.
    static CString DeleteMbrEntry(const CString& devicePath, UINT driveIndex, int slot);
    static CString DeleteGptEntry(const CString& devicePath, UINT driveIndex, int entryIndex);

    // ---- Extend partition into immediately-following free space ----
    struct FreeSpaceAfter
    {
        bool      valid = false;       // false if the query itself failed
        ULONGLONG freeSizeBytes = 0;   // 0 if valid but no free space immediately follows
    };
    // Queries the OS partition layout to find contiguous free space after entry.
    static FreeSpaceAfter QueryFreeSpaceAfter(const CString& devicePath, UINT driveIndex,
                                               const PartitionEntry& entry);
    // Extends the partition by extendByBytes using IOCTL_DISK_GROW_PARTITION, then
    // issues FSCTL_EXTEND_VOLUME on any mounted NTFS volume to grow the filesystem.
    static CString GrowPartition(const CString& devicePath, UINT driveIndex,
                                  const PartitionEntry& entry, ULONGLONG extendByBytes);

private:
    static DWORD Crc32(const void* data, size_t length);
};
