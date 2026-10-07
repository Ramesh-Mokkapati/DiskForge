// FileUnlocker.h - Finds which process(es) currently have a file open/locked
// (via the Windows Restart Manager API - the same mechanism behind Explorer's
// "This file is open in another program" dialog) and can terminate them to
// release the lock.
//
// This module never touches disk sectors or partition tables at all - it
// works entirely at the process/file-handle level, a different subsystem
// from everything else in this app. The only "write" it ever performs is
// terminating another process, which is inherently destructive to whatever
// unsaved work that process was holding - callers must warn accordingly
// before calling UnlockFile()/TerminateLockingProcess() with force enabled.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct LockingProcess
{
    DWORD        processId = 0;
    std::wstring processName;
};

class FileUnlocker
{
public:
    // Read-only: asks Restart Manager which processes have filePath open.
    static std::vector<LockingProcess> FindLockingProcesses(const std::wstring& filePath);

    // Terminates every process currently locking filePath (skipping this
    // process itself and any name in IsCriticalSystemProcess()). Returns
    // true only if every locker was successfully terminated.
    static bool UnlockFile(const std::wstring& filePath, bool forceTerminate = false);

    // Terminates a single process by ID. Refuses (returns false, with a
    // reason) for this process itself or a known-critical system process.
    static bool TerminateLockingProcess(DWORD processId, const std::wstring& processName, std::wstring& errorOut);

    // Case-insensitive check against a short list of core OS processes
    // (csrss.exe, wininit.exe, winlogon.exe, services.exe, lsass.exe,
    // smss.exe, System, Registry) that can crash or destabilize Windows if
    // force-terminated. This is a safety floor, not a complete list of
    // "important" processes - killing any process can lose unsaved work in
    // it, which is why callers should still confirm with the user regardless.
    static bool IsCriticalSystemProcess(const std::wstring& processName);
};
