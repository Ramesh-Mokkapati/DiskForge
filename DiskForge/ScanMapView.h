// ScanMapView.h - Small custom control that draws the bad-sector scan
// results as a proportional, multi-row strip (green/yellow/red = ok/slow/bad,
// light gray = not yet scanned). Decoupled from CBadSectorScanner's types so
// it can be reused/tested independently; the dialog translates.
#pragma once
#include "pch.h"

struct ScanMapBlock
{
    ULONGLONG startLBA;
    ULONGLONG sectorCount;
    int       status; // 0 = Ok, 1 = Slow, 2 = Bad
};

class CScanMapView : public CWnd
{
public:
    CScanMapView();
    virtual ~CScanMapView();

    static bool RegisterClass();
    bool Create(CWnd* pParent, const RECT& rect, UINT nID);

    // Call once when a scan starts (or the selection changes).
    void SetRange(ULONGLONG startLBA, ULONGLONG totalSectors);

    // Call periodically with the latest full set of completed blocks.
    void SetBlocks(const std::vector<ScanMapBlock>& blocks);

    void Reset();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()

private:
    ULONGLONG                 m_rangeStart;
    ULONGLONG                 m_rangeTotal;
    std::vector<ScanMapBlock> m_blocks;

    static const int kRows = 10;
};
