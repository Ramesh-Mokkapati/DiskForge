// PartitionParser.cpp - Read-only MBR / GPT partition table parser
#include "pch.h"
#include "PartitionParser.h"

namespace
{
    // ---- Little-endian field readers (safe on the x86/x64 targets this app builds for) ----
    inline WORD  ReadU16(const BYTE* p) { WORD  v; memcpy(&v, p, sizeof(v)); return v; }
    inline DWORD ReadU32(const BYTE* p) { DWORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline ULONGLONG ReadU64(const BYTE* p) { ULONGLONG v; memcpy(&v, p, sizeof(v)); return v; }

    GUID ReadGuid(const BYTE* p)
    {
        GUID g;
        g.Data1 = ReadU32(p);
        g.Data2 = ReadU16(p + 4);
        g.Data3 = ReadU16(p + 6);
        memcpy(g.Data4, p + 8, 8);
        return g;
    }

    // Reads 'sectorsNeeded' consecutive sectors starting at startLBA into one
    // contiguous buffer. Returns false on the first read failure.
    bool ReadSectors(CDiskManager& dm, ULONGLONG startLBA, DWORD sectorsNeeded,
                      std::vector<BYTE>& out, CString& errorOut)
    {
        out.clear();
        out.reserve(static_cast<size_t>(sectorsNeeded) * dm.GetBytesPerSector());

        for (DWORD i = 0; i < sectorsNeeded; ++i)
        {
            std::vector<BYTE> sec;
            if (!dm.ReadSector(startLBA + i, sec))
            {
                errorOut = dm.GetLastError();
                return false;
            }
            out.insert(out.end(), sec.begin(), sec.end());
        }
        return true;
    }
}

CString CPartitionParser::FormatSize(ULONGLONG bytes)
{
    CString s;
    if (bytes >= (1ULL << 40))
        s.Format(_T("%.2f TB"), (double)bytes / (1ULL << 40));
    else if (bytes >= (1ULL << 30))
        s.Format(_T("%.2f GB"), (double)bytes / (1ULL << 30));
    else if (bytes >= (1ULL << 20))
        s.Format(_T("%.2f MB"), (double)bytes / (1ULL << 20));
    else if (bytes >= (1ULL << 10))
        s.Format(_T("%.2f KB"), (double)bytes / (1ULL << 10));
    else
        s.Format(_T("%llu bytes"), bytes);
    return s;
}

CString CPartitionParser::MbrTypeName(BYTE type)
{
    switch (type)
    {
        case 0x00: return _T("Empty");
        case 0x01: return _T("FAT12");
        case 0x04: return _T("FAT16 (<32MB)");
        case 0x05: return _T("Extended (CHS)");
        case 0x06: return _T("FAT16");
        case 0x07: return _T("NTFS / exFAT");
        case 0x0B: return _T("FAT32 (CHS)");
        case 0x0C: return _T("FAT32 (LBA)");
        case 0x0E: return _T("FAT16 (LBA)");
        case 0x0F: return _T("Extended (LBA)");
        case 0x11: return _T("Hidden FAT12");
        case 0x12: return _T("Compaq diagnostics");
        case 0x14: return _T("Hidden FAT16 (<32MB)");
        case 0x16: return _T("Hidden FAT16");
        case 0x17: return _T("Hidden NTFS/exFAT");
        case 0x1B: return _T("Hidden FAT32 (CHS)");
        case 0x1C: return _T("Hidden FAT32 (LBA)");
        case 0x42: return _T("Windows dynamic disk (LDM)");
        case 0x82: return _T("Linux swap");
        case 0x83: return _T("Linux filesystem");
        case 0x85: return _T("Linux extended");
        case 0x8E: return _T("Linux LVM");
        case 0xA5: return _T("FreeBSD");
        case 0xA8: return _T("Mac OS X (UFS)");
        case 0xA9: return _T("NetBSD");
        case 0xAF: return _T("Mac OS X (HFS+)");
        case 0xEE: return _T("GPT protective");
        case 0xEF: return _T("EFI System (FAT)");
        case 0xFB: return _T("VMware VMFS");
        case 0xFC: return _T("VMware swap");
        default:
        {
            CString s;
            s.Format(_T("Unknown (0x%02X)"), type);
            return s;
        }
    }
}

bool CPartitionParser::IsZeroGuid(const GUID& g)
{
    static const GUID zero = {};
    return memcmp(&g, &zero, sizeof(GUID)) == 0;
}

CString CPartitionParser::GuidToString(const GUID& g)
{
    CString s;
    s.Format(_T("%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
              g.Data1, g.Data2, g.Data3,
              g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
              g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return s;
}

CString CPartitionParser::GptTypeName(const GUID& g)
{
    struct Entry { const GUID* guid; const TCHAR* name; };

    // clang-format off
    static const GUID EfiSystem       = { 0xC12A7328, 0xF81F, 0x11D2, {0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B} };
    static const GUID MsReserved      = { 0xE3C9E316, 0x0B5C, 0x4DB8, {0x81,0x7D,0xF9,0x2D,0xF0,0x02,0x15,0xAE} };
    static const GUID MsBasicData     = { 0xEBD0A0A2, 0xB9E5, 0x4433, {0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7} };
    static const GUID MsRecovery      = { 0xDE94BBA4, 0x06D1, 0x4D40, {0xA1,0x6A,0xBF,0xD5,0x01,0x79,0xD6,0xAC} };
    static const GUID MsLdmMeta       = { 0x5808C8AA, 0x7E8F, 0x42E0, {0x85,0xD2,0xE1,0xE9,0x04,0x34,0xCF,0xB3} };
    static const GUID MsLdmData       = { 0xAF9B60A0, 0x1431, 0x4F62, {0xBC,0x68,0x33,0x11,0x71,0x4A,0x69,0xAD} };
    static const GUID MsStorageSpaces = { 0xE75CAF8F, 0xF680, 0x4CEE, {0xAF,0xA3,0xB0,0x01,0xE5,0x6E,0xFC,0x2D} };
    static const GUID LinuxFs         = { 0x0FC63DAF, 0x8483, 0x4772, {0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4} };
    static const GUID LinuxSwap       = { 0x0657FD6D, 0xA4AB, 0x43C4, {0x84,0xE5,0x09,0x33,0xC8,0x4B,0x4F,0x4F} };
    static const GUID LinuxLvm        = { 0xE6D6D379, 0xF507, 0x44C2, {0xA2,0x3C,0x23,0x8F,0x2A,0x3D,0xF9,0x28} };
    static const GUID LinuxRaid       = { 0xA19D880F, 0x05FC, 0x4D3B, {0xA0,0x06,0x74,0x3F,0x0F,0x84,0x91,0x1E} };
    static const GUID LinuxRoot_x86_64= { 0x4F68BCE3, 0xE8CD, 0x4DB1, {0x96,0xE7,0xFB,0xCA,0xF9,0x84,0xB7,0x09} };
    static const GUID AppleHfsPlus    = { 0x48465300, 0x0000, 0x11AA, {0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC} };
    static const GUID AppleApfs       = { 0x7C3457EF, 0x0000, 0x11AA, {0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC} };
    static const GUID FreeBsdUfs      = { 0x516E7CB6, 0x6ECF, 0x11D6, {0x8F,0xF8,0x00,0x02,0x2D,0x09,0x71,0x2B} };
    static const GUID VmwareVmfs      = { 0xAA31E02A, 0x400F, 0x11DB, {0x9D,0x95,0x00,0x0C,0x29,0x11,0xD1,0xB8} };
    // clang-format on

    static const Entry table[] = {
        { &EfiSystem,        _T("EFI System Partition") },
        { &MsReserved,       _T("Microsoft Reserved") },
        { &MsBasicData,      _T("Microsoft Basic Data (NTFS/FAT)") },
        { &MsRecovery,       _T("Windows Recovery") },
        { &MsLdmMeta,        _T("Windows LDM Metadata") },
        { &MsLdmData,        _T("Windows LDM Data") },
        { &MsStorageSpaces,  _T("Windows Storage Spaces") },
        { &LinuxFs,          _T("Linux filesystem") },
        { &LinuxSwap,        _T("Linux swap") },
        { &LinuxLvm,         _T("Linux LVM") },
        { &LinuxRaid,        _T("Linux RAID") },
        { &LinuxRoot_x86_64, _T("Linux root (x86-64)") },
        { &AppleHfsPlus,     _T("Apple HFS+") },
        { &AppleApfs,        _T("Apple APFS") },
        { &FreeBsdUfs,       _T("FreeBSD UFS") },
        { &VmwareVmfs,       _T("VMware VMFS") },
    };

    for (const auto& e : table)
    {
        if (memcmp(e.guid, &g, sizeof(GUID)) == 0)
            return e.name;
    }
    return _T("Unknown / vendor-specific");
}

void CPartitionParser::ParseMbrPrimary(
    CDiskManager& dm,
    const std::vector<BYTE>& mbrSector,
    std::vector<PartitionEntry>& out,
    bool& sawProtectiveGpt)
{
    sawProtectiveGpt = false;

    if (mbrSector.size() < 512)
        return;

    // Boot signature check (0x55AA at offset 510-511 of the first 512 bytes).
    if (mbrSector[510] != 0x55 || mbrSector[511] != 0xAA)
        return; // Not a valid MBR - likely an unpartitioned / raw disk.

    for (int i = 0; i < 4; ++i)
    {
        const BYTE* e = &mbrSector[446 + i * 16];
        BYTE  bootFlag = e[0];
        BYTE  type     = e[4];
        DWORD startLBA = ReadU32(e + 8);
        DWORD numSect  = ReadU32(e + 12);

        if (type == 0x00 || (startLBA == 0 && numSect == 0))
            continue; // Empty slot

        if (type == 0xEE)
        {
            // GPT protective MBR - the real partition table lives in the GPT header/entries.
            sawProtectiveGpt = true;
            continue;
        }

        PartitionEntry pe;
        pe.scheme      = _T("MBR");
        pe.bootable    = (bootFlag == 0x80);
        pe.isLogical   = false;
        pe.mbrType     = type;
        pe.typeId.Format(_T("0x%02X"), type);
        pe.typeName    = MbrTypeName(type);
        pe.rawSlotIndex = i;
        pe.startLBA    = startLBA;
        pe.sectorCount = numSect;
        pe.sizeBytes   = static_cast<ULONGLONG>(numSect) * dm.GetBytesPerSector();
        out.push_back(pe);

        if (type == 0x05 || type == 0x0F || type == 0x85)
        {
            // Extended partition - walk its EBR chain for logical drives.
            ParseExtendedChain(dm, startLBA, out);
        }
    }
}

void CPartitionParser::ParseExtendedChain(
    CDiskManager& dm,
    ULONGLONG extendedPartitionStartLBA,
    std::vector<PartitionEntry>& out)
{
    ULONGLONG ebrLBA = extendedPartitionStartLBA;
    const int kMaxLogicalPartitions = 128; // Guard against a corrupt/looping chain.

    for (int guard = 0; guard < kMaxLogicalPartitions; ++guard)
    {
        std::vector<BYTE> ebr;
        if (!dm.ReadSector(ebrLBA, ebr) || ebr.size() < 512)
            break;

        if (ebr[510] != 0x55 || ebr[511] != 0xAA)
            break;

        const BYTE* e0 = &ebr[446];      // Describes this logical partition
        const BYTE* e1 = &ebr[446 + 16]; // Points to the next EBR, if any

        BYTE  type0     = e0[4];
        DWORD startRel0 = ReadU32(e0 + 8);
        DWORD numSect0  = ReadU32(e0 + 12);

        BYTE  type1     = e1[4];
        DWORD startRel1 = ReadU32(e1 + 8);

        if (type0 != 0x00 && numSect0 != 0)
        {
            PartitionEntry pe;
            pe.scheme      = _T("MBR");
            pe.bootable    = (e0[0] == 0x80);
            pe.isLogical   = true;
            pe.mbrType     = type0;
            pe.typeId.Format(_T("0x%02X"), type0);
            pe.typeName    = MbrTypeName(type0);
            pe.startLBA    = ebrLBA + startRel0;
            pe.sectorCount = numSect0;
            pe.sizeBytes   = static_cast<ULONGLONG>(numSect0) * dm.GetBytesPerSector();
            out.push_back(pe);
        }

        bool hasNext = (type1 == 0x05 || type1 == 0x0F || type1 == 0x85) && startRel1 != 0;
        if (!hasNext)
            break;

        ebrLBA = extendedPartitionStartLBA + startRel1;
    }
}

bool CPartitionParser::ParseGPT(
    CDiskManager& dm,
    ULONGLONG gptHeaderLBA,
    std::vector<PartitionEntry>& out,
    CString& diskGuidOut,
    CString& errorOut)
{
    std::vector<BYTE> hdr;
    if (!dm.ReadSector(gptHeaderLBA, hdr))
    {
        errorOut = dm.GetLastError();
        return false;
    }

    static const char kSig[8] = { 'E','F','I',' ','P','A','R','T' };
    if (hdr.size() < 92 || memcmp(hdr.data(), kSig, 8) != 0)
    {
        errorOut = _T("GPT header signature not found.");
        return false;
    }

    ULONGLONG partEntryLBA   = ReadU64(&hdr[72]);
    DWORD     numEntries     = ReadU32(&hdr[80]);
    DWORD     entrySize      = ReadU32(&hdr[84]);
    GUID      diskGuid       = ReadGuid(&hdr[56]);

    diskGuidOut = GuidToString(diskGuid);

    if (entrySize == 0 || numEntries == 0 || entrySize > 4096 || numEntries > 4096)
    {
        errorOut = _T("GPT header reports an implausible partition entry table size.");
        return false;
    }

    DWORD bytesPerSector = dm.GetBytesPerSector();
    ULONGLONG totalArrayBytes = static_cast<ULONGLONG>(numEntries) * entrySize;
    DWORD sectorsNeeded = static_cast<DWORD>(
        (totalArrayBytes + bytesPerSector - 1) / bytesPerSector);

    std::vector<BYTE> entries;
    if (!ReadSectors(dm, partEntryLBA, sectorsNeeded, entries, errorOut))
        return false;

    int idx = 0;
    for (DWORD i = 0; i < numEntries; ++i)
    {
        size_t off = static_cast<size_t>(i) * entrySize;
        if (off + 128 > entries.size())
            break;

        GUID typeGuid = ReadGuid(&entries[off]);
        if (IsZeroGuid(typeGuid))
            continue; // Unused slot

        ULONGLONG startLBA = ReadU64(&entries[off + 32]);
        ULONGLONG endLBA   = ReadU64(&entries[off + 40]);
        ULONGLONG sectors  = (endLBA >= startLBA) ? (endLBA - startLBA + 1) : 0;
        ULONGLONG attrs    = ReadU64(&entries[off + 48]);
        GUID uniqueGuid    = ReadGuid(&entries[off + 16]);

        // PartitionName: 36 UTF-16LE code units starting at offset 56.
        CString name;
        {
            const WCHAR* wname = reinterpret_cast<const WCHAR*>(&entries[off + 56]);
            WCHAR buf[37] = {};
            wcsncpy_s(buf, wname, 36);
            name = buf;
        }

        PartitionEntry pe;
        pe.index       = ++idx;
        pe.scheme      = _T("GPT");
        pe.bootable    = (GptTypeName(typeGuid) == _T("EFI System Partition"));
        pe.isLogical   = false;
        pe.mbrType     = 0;
        pe.typeId      = GuidToString(typeGuid);
        pe.typeName    = GptTypeName(typeGuid);
        pe.startLBA    = startLBA;
        pe.sectorCount = sectors;
        pe.sizeBytes   = sectors * bytesPerSector;
        pe.name        = name;
        pe.typeGuidRaw    = typeGuid;
        pe.uniqueGuidRaw  = uniqueGuid;
        pe.attributesRaw  = attrs;
        pe.rawSlotIndex   = static_cast<int>(i);
        out.push_back(pe);
    }

    return true;
}

bool CPartitionParser::Parse(
    CDiskManager& dm,
    std::vector<PartitionEntry>& out,
    CString& schemeOut,
    CString& diskGuidOut,
    CString& errorOut)
{
    out.clear();
    schemeOut.Empty();
    diskGuidOut.Empty();
    errorOut.Empty();

    if (!dm.IsOpen())
    {
        errorOut = _T("No drive is open.");
        return false;
    }

    std::vector<BYTE> sector0;
    if (!dm.ReadSector(0, sector0))
    {
        errorOut = dm.GetLastError();
        return false;
    }

    bool sawProtectiveGpt = false;
    std::vector<PartitionEntry> mbrEntries;
    ParseMbrPrimary(dm, sector0, mbrEntries, sawProtectiveGpt);

    if (sawProtectiveGpt)
    {
        std::vector<PartitionEntry> gptEntries;
        CString gptError;
        if (ParseGPT(dm, 1, gptEntries, diskGuidOut, gptError))
        {
            schemeOut = _T("GPT");
            out = std::move(gptEntries);
            return true;
        }
        // Fall through: protective MBR present but GPT header unreadable/corrupt.
        errorOut = _T("Protective MBR found but GPT header could not be parsed: ") + gptError;
        schemeOut = _T("GPT (header unreadable)");
        return true; // Not a hard failure - report what we know via errorOut.
    }

    if (!mbrEntries.empty())
    {
        schemeOut = _T("MBR");
        int idx = 0;
        for (auto& pe : mbrEntries)
            pe.index = ++idx;
        out = std::move(mbrEntries);
        return true;
    }

    if (sector0.size() >= 512 && sector0[510] == 0x55 && sector0[511] == 0xAA)
        schemeOut = _T("MBR (valid signature, no partitions defined)");
    else
        schemeOut = _T("Unpartitioned / unrecognized (no 0x55AA boot signature)");

    return true;
}
