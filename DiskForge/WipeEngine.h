// WipeEngine.h - Overwrites a whole disk or a single partition with a chosen
// pattern (zeros and/or pseudo-random data), 1-3 passes.
//
// IMPORTANT HONESTY NOTE (also surfaced in the UI): this is a straightforward
// overwrite tool, not a certified secure-erase implementation. It does not
// claim to meet any particular data-sanitization standard (e.g. NIST 800-88,
// DoD 5220.22-M), does not attempt to reach HPA/DCO-hidden areas or
// SSD-remapped-but-not-yet-erased flash blocks, and its "random" pass uses
// the standard C library PRNG, not a cryptographic one. For drives holding
// genuinely sensitive data, the drive's own ATA/NVMe Secure Erase or
// Sanitize command (not implemented here) is the more thorough option.
//
// Same write discipline as DiskCopyEngine: system-drive protection, and
// volume locking/dismounting (via the shared CVolumeLockHelper) before any
// raw write.
#pragma once
#include "pch.h"

enum class WipePattern { Zeros, Random, ZerosThenRandom, RandomThreePass };

struct WipeJob
{
    CString     devicePath;
    UINT        driveIndex = 0;
    DWORD       sectorSize = 512;
    ULONGLONG   startLBA = 0;      // 0 for whole-disk wipes
    ULONGLONG   sectorCount = 0;   // sectors to wipe (whole disk or one partition)
    WipePattern pattern = WipePattern::Zeros;
};

struct WipeProgress
{
    ULONGLONG bytesWritten = 0;
    ULONGLONG bytesTotal   = 0;  // total across ALL passes
    int       currentPass = 0;
    int       totalPasses = 1;
    bool      finished = false;
    bool      canceled  = false;
    bool      success   = false;
    CString   statusText;
    CString   errorMessage;
};

CString ValidateWipeJob(const WipeJob& job);

class CWipeEngine
{
public:
    CWipeEngine();
    ~CWipeEngine();

    // Blocking - intended to be called from a worker thread only.
    bool Run(const WipeJob& job);

    void RequestCancel() { m_cancelRequested = true; }
    WipeProgress GetProgressSnapshot() const;

private:
    volatile bool            m_cancelRequested;
    mutable CRITICAL_SECTION m_progressLock;
    WipeProgress             m_progress;

    void SetProgress(ULONGLONG written, ULONGLONG total, int pass, int totalPasses, const CString& status);
    void SetFinished(bool success, bool canceled, const CString& err);

    static int PassCountFor(WipePattern p);
    // Returns true if pass index `passIndex` (0-based) should be filled with
    // zeros; false means fill with pseudo-random bytes.
    static bool IsZeroPass(WipePattern p, int passIndex);
};
