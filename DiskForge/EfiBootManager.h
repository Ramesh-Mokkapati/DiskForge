// EfiBootManager.h - Reads and manages UEFI NVRAM boot entries via the
// standard Get/SetFirmwareEnvironmentVariable Win32 APIs.
//
// Scope, deliberately conservative:
//   - Only works on UEFI systems (checked via GetFirmwareType) - BIOS/legacy
//     systems have no NVRAM boot variables to manage.
//   - Only discovers entries that are currently listed in the BootOrder
//     variable. There is no reliable, documented Win32 API to enumerate ALL
//     firmware variables (only undocumented native APIs can do that), so an
//     orphaned BootXXXX entry that isn't in BootOrder simply won't appear
//     here. This is a real, stated limitation, not a bug.
//   - Supports: reordering BootOrder, setting/clearing a one-time BootNext
//     override, and deleting an existing entry (which also removes it from
//     BootOrder for consistency).
//   - Deliberately does NOT support creating a brand-new boot entry. That
//     requires constructing a correct EFI_LOAD_OPTION, including an EFI
//     Device Path Protocol structure pointing at a specific boot file -
//     intricate, firmware-implementation-sensitive binary data that really
//     needs validation against real hardware/firmware, which isn't available
//     in this development environment. Getting it wrong produces a boot
//     entry that silently fails to boot, not disk damage, but it's still not
//     something to ship unvalidated.
//
// Every write operation here requires the SE_SYSTEM_ENVIRONMENT_NAME
// privilege (enabled on demand) and administrator rights. This module never
// touches disk sectors at all - it's entirely NVRAM variable access.
#pragma once
#include "pch.h"

struct EfiBootEntry
{
    WORD    id = 0;          // e.g. 0x0001 for variable "Boot0001"
    CString description;
    bool    active = false;  // LOAD_OPTION_ACTIVE bit in the entry's Attributes
};

struct EfiBootInfo
{
    bool    supported = false;
    CString unsupportedReason;

    std::vector<EfiBootEntry> entries;   // only entries currently listed in BootOrder - see header note
    std::vector<WORD>         bootOrder; // current BootOrder, in order

    bool    hasBootNext = false;
    WORD    bootNext = 0;
};

class CEfiBootManager
{
public:
    static bool IsUefiSystem();

    // Enables SE_SYSTEM_ENVIRONMENT_NAME in the current process token.
    // Required before any read or write here will succeed.
    static bool EnableRequiredPrivilege(CString& errorOut);

    static EfiBootInfo ReadBootInfo();

    static CString SetBootOrder(const std::vector<WORD>& newOrder);
    static CString SetBootNext(WORD entryId);
    static CString ClearBootNext();

    // Deletes the BootXXXX variable and removes it from BootOrder.
    static CString DeleteBootEntry(WORD entryId);

private:
    static CString FormatVarName(const TCHAR* prefix, WORD id);
    static bool ReadVariable(const CString& name, std::vector<BYTE>& out);
    static bool WriteVariable(const CString& name, const void* data, DWORD size, CString& errorOut);
    static bool DeleteVariable(const CString& name, CString& errorOut);
};
