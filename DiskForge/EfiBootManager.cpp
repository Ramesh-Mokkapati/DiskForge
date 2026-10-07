// EfiBootManager.cpp
#include "pch.h"
#include "EfiBootManager.h"

#pragma comment(lib, "advapi32.lib")

namespace
{
    const TCHAR* kEfiGlobalGuid = _T("{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}");
    const DWORD  kVarBufSize = 8192; // Comfortably larger than any real load option

    inline DWORD ReadU32(const BYTE* p) { DWORD v; memcpy(&v, p, sizeof(v)); return v; }
    inline WORD  ReadU16(const BYTE* p) { WORD v; memcpy(&v, p, sizeof(v)); return v; }
}

CString CEfiBootManager::FormatVarName(const TCHAR* prefix, WORD id)
{
    CString s;
    s.Format(_T("%s%04X"), prefix, id);
    return s;
}

bool CEfiBootManager::IsUefiSystem()
{
    FIRMWARE_TYPE type = FirmwareTypeUnknown;
    if (!GetFirmwareType(&type))
        return false;
    return type == FirmwareTypeUefi;
}

bool CEfiBootManager::EnableRequiredPrivilege(CString& errorOut)
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        errorOut = _T("Could not open the process token.");
        return false;
    }

    LUID luid = {};
    if (!LookupPrivilegeValue(nullptr, SE_SYSTEM_ENVIRONMENT_NAME, &luid))
    {
        CloseHandle(hToken);
        errorOut = _T("Could not look up the required privilege.");
        return false;
    }

    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD lastErr = ::GetLastError();
    CloseHandle(hToken);

    if (!ok || lastErr == ERROR_NOT_ALL_ASSIGNED)
    {
        errorOut = _T("Could not enable the system-environment privilege. Make sure this app is running as Administrator.");
        return false;
    }
    return true;
}

bool CEfiBootManager::ReadVariable(const CString& name, std::vector<BYTE>& out)
{
    out.resize(kVarBufSize);
    DWORD n = GetFirmwareEnvironmentVariable(name, kEfiGlobalGuid, out.data(), kVarBufSize);
    if (n == 0)
    {
        out.clear();
        return false;
    }
    out.resize(n);
    return true;
}

bool CEfiBootManager::WriteVariable(const CString& name, const void* data, DWORD size, CString& errorOut)
{
    if (!SetFirmwareEnvironmentVariable(name, kEfiGlobalGuid, const_cast<void*>(data), size))
    {
        errorOut.Format(_T("Failed to write firmware variable %s (Win32 error %u)."), name.GetString(), ::GetLastError());
        return false;
    }
    return true;
}

bool CEfiBootManager::DeleteVariable(const CString& name, CString& errorOut)
{
    // Per documentation, writing a zero-length value deletes the variable.
    if (!SetFirmwareEnvironmentVariable(name, kEfiGlobalGuid, nullptr, 0))
    {
        errorOut.Format(_T("Failed to delete firmware variable %s (Win32 error %u)."), name.GetString(), ::GetLastError());
        return false;
    }
    return true;
}

EfiBootInfo CEfiBootManager::ReadBootInfo()
{
    EfiBootInfo info;

    if (!IsUefiSystem())
    {
        info.unsupportedReason = _T("This system reports BIOS/legacy boot, not UEFI - there are no NVRAM boot variables to manage.");
        return info;
    }

    CString privErr;
    if (!EnableRequiredPrivilege(privErr))
    {
        info.unsupportedReason = privErr;
        return info;
    }

    std::vector<BYTE> orderBuf;
    if (!ReadVariable(_T("BootOrder"), orderBuf) || orderBuf.size() < 2)
    {
        info.unsupportedReason = _T("Could not read the BootOrder firmware variable.");
        return info;
    }

    size_t count = orderBuf.size() / 2;
    for (size_t i = 0; i < count; ++i)
        info.bootOrder.push_back(ReadU16(&orderBuf[i * 2]));

    for (WORD id : info.bootOrder)
    {
        std::vector<BYTE> entryBuf;
        CString varName = FormatVarName(_T("Boot"), id);
        if (!ReadVariable(varName, entryBuf) || entryBuf.size() < 6)
            continue; // Referenced in BootOrder but unreadable/malformed - skip rather than guess

        EfiBootEntry entry;
        entry.id = id;

        DWORD attrs = ReadU32(&entryBuf[0]);
        entry.active = (attrs & 0x00000001) != 0; // LOAD_OPTION_ACTIVE

        // Description is a null-terminated UTF-16 string starting at offset 6.
        const WCHAR* descStart = reinterpret_cast<const WCHAR*>(&entryBuf[6]);
        size_t maxChars = (entryBuf.size() - 6) / sizeof(WCHAR);
        size_t len = 0;
        while (len < maxChars && descStart[len] != 0)
            ++len;
        entry.description = CString(descStart, (int)len);

        info.entries.push_back(entry);
    }

    std::vector<BYTE> nextBuf;
    if (ReadVariable(_T("BootNext"), nextBuf) && nextBuf.size() >= 2)
    {
        info.hasBootNext = true;
        info.bootNext = ReadU16(nextBuf.data());
    }

    info.supported = true;
    return info;
}

CString CEfiBootManager::SetBootOrder(const std::vector<WORD>& newOrder)
{
    if (!IsUefiSystem())
        return _T("This system is not UEFI.");

    CString privErr;
    if (!EnableRequiredPrivilege(privErr))
        return privErr;

    std::vector<BYTE> buf(newOrder.size() * 2);
    for (size_t i = 0; i < newOrder.size(); ++i)
        memcpy(&buf[i * 2], &newOrder[i], 2);

    CString err;
    WriteVariable(_T("BootOrder"), buf.data(), (DWORD)buf.size(), err);
    return err;
}

CString CEfiBootManager::SetBootNext(WORD entryId)
{
    if (!IsUefiSystem())
        return _T("This system is not UEFI.");

    CString privErr;
    if (!EnableRequiredPrivilege(privErr))
        return privErr;

    CString err;
    WriteVariable(_T("BootNext"), &entryId, sizeof(entryId), err);
    return err;
}

CString CEfiBootManager::ClearBootNext()
{
    if (!IsUefiSystem())
        return _T("This system is not UEFI.");

    CString privErr;
    if (!EnableRequiredPrivilege(privErr))
        return privErr;

    CString err;
    DeleteVariable(_T("BootNext"), err);
    return err; // Deleting a variable that doesn't exist is not treated as an error by the caller
}

CString CEfiBootManager::DeleteBootEntry(WORD entryId)
{
    if (!IsUefiSystem())
        return _T("This system is not UEFI.");

    CString privErr;
    if (!EnableRequiredPrivilege(privErr))
        return privErr;

    // Remove it from BootOrder first, so nothing ever points at a deleted entry.
    EfiBootInfo info = ReadBootInfo();
    if (!info.supported)
        return info.unsupportedReason;

    std::vector<WORD> newOrder;
    for (WORD id : info.bootOrder)
        if (id != entryId)
            newOrder.push_back(id);

    CString err = SetBootOrder(newOrder);
    if (!err.IsEmpty())
        return _T("Failed updating BootOrder before delete: ") + err;

    CString varName = FormatVarName(_T("Boot"), entryId);
    DeleteVariable(varName, err);
    return err;
}
