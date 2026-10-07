// SmartDataReader.h - Read-only S.M.A.R.T./health query.
//
// Uses two independent, purely-informational Windows mechanisms:
//   1. IOCTL_STORAGE_PREDICT_FAILURE - a modern, broadly-supported "is this
//      drive predicting its own failure" flag.
//   2. The classic "Disk Failure Prediction" (legacy SMART) IOCTLs, to pull
//      the raw SMART attribute table when the driver/controller supports it.
//
// Both mechanisms are issued with FILE_ANY_ACCESS IOCTL codes, so the drive
// handle here is opened with GENERIC_READ only - this module never requests
// write access to a drive.
//
// IMPORTANT CAVEAT (surfaced in the UI): SMART passthrough support varies a
// lot by storage stack. Standard SATA/AHCI drives usually work; NVMe drives,
// many USB enclosures, and some RAID controllers commonly do not expose
// either mechanism, in which case this reports "not supported" rather than
// guessing or fabricating data.
#pragma once
#include "pch.h"

struct SmartAttribute
{
    BYTE      id = 0;
    CString   name;
    BYTE      currentValue = 0;
    BYTE      worstValue = 0;
    ULONGLONG rawValue = 0;
    bool      isCriticalWarning = false; // heuristic: nonzero raw value on a known-critical attribute
};

struct SmartHealthResult
{
    bool    querySucceeded = false; // could we get ANY health signal at all?
    CString unsupportedReason;

    bool    hasPredictFailureFlag = false;
    bool    failurePredicted = false;

    bool    hasAttributeTable = false;
    std::vector<SmartAttribute> attributes;

    int     temperatureCelsius = -1; // -1 = unknown
};

class CSmartDataReader
{
public:
    // Opens devicePath itself (GENERIC_READ only) - self-contained and
    // independent of CDiskManager so this module's access pattern stays
    // simple to audit.
    static SmartHealthResult ReadHealth(const CString& devicePath);

private:
    static bool QueryPredictFailure(HANDLE h, bool& failurePredicted);
    static bool ReadSmartAttributes(HANDLE h, std::vector<SmartAttribute>& attributesOut);
    static CString AttributeName(BYTE id);
    static bool IsCriticalAttribute(BYTE id);
};
