// PartitionParser.h - Read-only MBR / GPT partition table parser
//
// This module only ever calls CDiskManager::ReadSector() (which issues plain
// ReadFile calls). It never writes to the drive.
#pragma once
#include "pch.h"
#include "DiskManager.h"

struct PartitionEntry
{
    int         index = 0;          // 1-based display index
    CString     scheme;             // "MBR" or "GPT"
    bool        bootable = false;   // MBR boot flag (0x80) / GPT ESP heuristic
    bool        isLogical = false;  // true for MBR logical (inside extended) partitions
    BYTE        mbrType = 0;        // Raw MBR type byte (0 for GPT entries)
    CString     typeId;             // "0x07" for MBR, or GUID string for GPT
    CString     typeName;           // Human-readable description, e.g. "NTFS / exFAT"
    ULONGLONG   startLBA = 0;
    ULONGLONG   sectorCount = 0;
    ULONGLONG   sizeBytes = 0;
    CString     name;               // GPT partition name (empty for MBR)

    // Raw fields, needed by editing features (CPartitionEditor) so an entry
    // can be rewritten without losing data the display-only fields above
    // don't capture. Zero/unused for MBR entries.
    GUID        typeGuidRaw = {};
    GUID        uniqueGuidRaw = {};
    ULONGLONG   attributesRaw = 0;

    // 0-based position within the on-disk table (MBR: 0-3 primary slot; GPT:
    // 0-127 array index). -1 for entries with no simple fixed slot (MBR
    // logical/extended partitions) - those can't be safely rewritten by
    // CPartitionEditor and it will refuse.
    int         rawSlotIndex = -1;
};

class CPartitionParser
{
public:
    // Reads sector 0 (and additional sectors as needed) from the currently
    // open drive in dm and fills 'out' with whatever partitions are found.
    // schemeOut is set to "MBR", "GPT", "Unpartitioned/Unknown", or similar.
    // Returns false only on a hard read error (see errorOut); an empty-but-
    // valid table (e.g. an unpartitioned disk) still returns true.
    static bool Parse(
        CDiskManager& dm,
        std::vector<PartitionEntry>& out,
        CString& schemeOut,
        CString& diskGuidOut,
        CString& errorOut);

    // Human-readable helpers exposed for reuse by the UI layer.
    static CString FormatSize(ULONGLONG bytes);

private:
    static bool ParseGPT(
        CDiskManager& dm,
        ULONGLONG gptHeaderLBA,
        std::vector<PartitionEntry>& out,
        CString& diskGuidOut,
        CString& errorOut);

    static void ParseMbrPrimary(
        CDiskManager& dm,
        const std::vector<BYTE>& mbrSector,
        std::vector<PartitionEntry>& out,
        bool& sawProtectiveGpt);

    static void ParseExtendedChain(
        CDiskManager& dm,
        ULONGLONG extendedPartitionStartLBA,
        std::vector<PartitionEntry>& out);

    static CString MbrTypeName(BYTE type);
    static CString GptTypeName(const GUID& g);
    static CString GuidToString(const GUID& g);
    static bool    IsZeroGuid(const GUID& g);
};
