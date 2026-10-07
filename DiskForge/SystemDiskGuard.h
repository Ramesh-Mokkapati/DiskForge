// SystemDiskGuard.h - Identifies which physical drive(s) must never be
// treated as a write destination (namely, whatever disk the running copy
// of Windows lives on).
#pragma once
#include "pch.h"

class CSystemDiskGuard
{
public:
    // Returns the physical drive index(es) that host the volume Windows is
    // currently running from (normally just one entry, but a spanned volume
    // could span more than one physical disk). Best-effort: if detection
    // fails for any reason, it errs toward caution.
    static std::vector<UINT> GetProtectedDriveIndices();

    // Convenience check used right before allowing a destination selection
    // and again right before a copy actually starts.
    static bool IsProtectedDrive(UINT physicalDriveIndex);

private:
    static bool GetDiskExtentsForVolume(const CString& volumePath, std::vector<UINT>& diskIndicesOut);
};
