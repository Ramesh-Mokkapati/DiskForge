// SectorRepairer.h - Attempts to make a previously-bad sector usable again
// by writing zeros to it, then re-reading it to check.
//
// IMPORTANT: this does NOT recover whatever data used to be in the sector -
// that data is already unreadable (that's what "bad sector" means here) and
// this operation overwrites it with zeros regardless of the outcome. It only
// tries to make the LBA usable again, the same way tools like `chkdsk /r`
// or a drive's own reallocation-on-write behavior work: writing to a sector
// that's currently failing to read gives the drive's firmware a chance to
// either fix it in place or transparently reallocate it from its spare pool.
// If the underlying defect is severe, the write itself may also fail, in
// which case nothing has improved (and nothing further has been lost,
// beyond the write attempt).
//
// Same write discipline as every other write-capable module: system-drive
// protection and volume locking/dismounting before the write.
#pragma once
#include "pch.h"

struct SectorRepairResult
{
    bool    writeSucceeded = false;
    bool    nowReadable = false; // re-read after writing
    CString errorMessage;
};

class CSectorRepairer
{
public:
    static SectorRepairResult Repair(const CString& devicePath, UINT driveIndex, DWORD sectorSize,
                                      ULONGLONG startLBA, ULONGLONG sectorCount);
};
