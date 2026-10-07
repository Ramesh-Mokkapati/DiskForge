// FileUnlocker.cpp
#include "pch.h"
#include <restartmanager.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>
#include "FileUnlocker.h"

#pragma comment(lib, "Rstrtmgr.lib")

namespace
{
    std::wstring GetProcessName(DWORD pid)
    {
        std::wstring name = L"Unknown Process";
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnapshot, &pe))
            {
                do
                {
                    if (pe.th32ProcessID == pid)
                    {
                        name = pe.szExeFile;
                        break;
                    }
                } while (Process32NextW(hSnapshot, &pe));
            }
            CloseHandle(hSnapshot);
        }
        return name;
    }

    std::wstring ToLower(std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
        return s;
    }
}

bool FileUnlocker::IsCriticalSystemProcess(const std::wstring& processName)
{
    static const wchar_t* kCritical[] = {
        L"csrss.exe", L"wininit.exe", L"winlogon.exe", L"services.exe",
        L"lsass.exe", L"smss.exe", L"system", L"registry"
    };

    std::wstring lower = ToLower(processName);
    for (const wchar_t* c : kCritical)
        if (lower == c)
            return true;
    return false;
}

std::vector<LockingProcess> FileUnlocker::FindLockingProcesses(const std::wstring& filePath)
{
    std::vector<LockingProcess> lockingProcesses;
    DWORD dwSession = 0;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError != ERROR_SUCCESS)
        return lockingProcesses;

    PCWSTR pszFile = filePath.c_str();
    dwError = RmRegisterResources(dwSession, 1, &pszFile, 0, nullptr, 0, nullptr);
    if (dwError != ERROR_SUCCESS)
    {
        RmEndSession(dwSession);
        return lockingProcesses;
    }

    UINT dwProcInfoNeeded = 0;
    UINT dwProcInfo = 0;
    DWORD dwReason = 0;

    dwError = RmGetList(dwSession, &dwProcInfoNeeded, &dwProcInfo, nullptr, &dwReason);
    if (dwError == ERROR_MORE_DATA && dwProcInfoNeeded > 0)
    {
        std::vector<RM_PROCESS_INFO> rgAffectedApps(dwProcInfoNeeded);
        dwProcInfo = dwProcInfoNeeded;

        dwError = RmGetList(dwSession, &dwProcInfoNeeded, &dwProcInfo, rgAffectedApps.data(), &dwReason);
        if (dwError == ERROR_SUCCESS)
        {
            for (UINT i = 0; i < dwProcInfo; ++i)
            {
                LockingProcess lp;
                lp.processId = rgAffectedApps[i].Process.dwProcessId;

                if (wcslen(rgAffectedApps[i].strAppName) > 0)
                    lp.processName = rgAffectedApps[i].strAppName;
                else
                    lp.processName = GetProcessName(lp.processId);

                lockingProcesses.push_back(lp);
            }
        }
    }

    RmEndSession(dwSession);
    return lockingProcesses;
}

bool FileUnlocker::TerminateLockingProcess(DWORD processId, const std::wstring& processName, std::wstring& errorOut)
{
    if (processId == GetCurrentProcessId())
    {
        errorOut = L"Refusing to terminate this application's own process.";
        return false;
    }
    if (IsCriticalSystemProcess(processName))
    {
        errorOut = L"Refusing to terminate " + processName + L" - it's a core Windows process; " +
            L"forcing it closed can crash or destabilize the system.";
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess == nullptr || hProcess == INVALID_HANDLE_VALUE)
    {
        errorOut = L"Could not open the process (it may already have exited, or access was denied).";
        return false;
    }

    BOOL ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);

    if (!ok)
    {
        errorOut = L"TerminateProcess failed.";
        return false;
    }
    return true;
}

bool FileUnlocker::UnlockFile(const std::wstring& filePath, bool forceTerminate)
{
    std::vector<LockingProcess> locks = FindLockingProcesses(filePath);
    if (locks.empty())
        return true;

    bool allUnlocked = true;
    for (const auto& proc : locks)
    {
        if (!forceTerminate)
        {
            allUnlocked = false;
            continue;
        }

        std::wstring err;
        if (!TerminateLockingProcess(proc.processId, proc.processName, err))
            allUnlocked = false;
    }

    return allUnlocked;
}
