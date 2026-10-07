// ScanMapView.cpp
#include "pch.h"
#include "ScanMapView.h"

static const TCHAR SCAN_MAP_CLASS[] = _T("DiskSectorScanMap");

static const COLORREF CLR_UNSCANNED = RGB(225, 225, 225);
static const COLORREF CLR_OK        = RGB(70, 170, 90);
static const COLORREF CLR_SLOW      = RGB(230, 170, 40);
static const COLORREF CLR_BAD       = RGB(210, 55, 55);
static const COLORREF CLR_GRID      = RGB(180, 180, 180);

BEGIN_MESSAGE_MAP(CScanMapView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CScanMapView::CScanMapView()
    : m_rangeStart(0)
    , m_rangeTotal(0)
{
}

CScanMapView::~CScanMapView()
{
}

// static
bool CScanMapView::RegisterClass()
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = ::DefWindowProc;
    wc.hInstance     = AfxGetInstanceHandle();
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = SCAN_MAP_CLASS;

    ATOM a = ::RegisterClassEx(&wc);
    return (a != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
}

bool CScanMapView::Create(CWnd* pParent, const RECT& rect, UINT nID)
{
    RegisterClass();
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER;
    return CWnd::Create(SCAN_MAP_CLASS, nullptr, style, rect, pParent, nID);
}

void CScanMapView::SetRange(ULONGLONG startLBA, ULONGLONG totalSectors)
{
    m_rangeStart = startLBA;
    m_rangeTotal = totalSectors;
    m_blocks.clear();
    Invalidate();
}

void CScanMapView::SetBlocks(const std::vector<ScanMapBlock>& blocks)
{
    m_blocks = blocks;
    Invalidate();
}

void CScanMapView::Reset()
{
    m_blocks.clear();
    Invalidate();
}

BOOL CScanMapView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // We paint the whole client area ourselves in OnPaint
}

void CScanMapView::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    memDC.FillSolidRect(&rc, RGB(255, 255, 255));

    if (m_rangeTotal == 0)
    {
        memDC.SelectObject(pOldBmp);
        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
        return;
    }

    int rowHeight = rc.Height() / kRows;
    ULONGLONG sectorsPerRow = (m_rangeTotal + kRows - 1) / kRows;
    ULONGLONG rangeEnd = m_rangeStart + m_rangeTotal;

    // Background: mark every row fully "unscanned" first.
    for (int row = 0; row < kRows; ++row)
    {
        CRect rowRect(rc.left, rc.top + row * rowHeight, rc.right,
                      (row == kRows - 1) ? rc.bottom : rc.top + (row + 1) * rowHeight);
        memDC.FillSolidRect(&rowRect, CLR_UNSCANNED);
    }

    // Paint each completed block into whichever row(s) it overlaps.
    for (const auto& block : m_blocks)
    {
        ULONGLONG blockStart = block.startLBA;
        ULONGLONG blockEnd   = block.startLBA + block.sectorCount;

        COLORREF color = CLR_OK;
        if (block.status == 1) color = CLR_SLOW;
        else if (block.status == 2) color = CLR_BAD;

        for (int row = 0; row < kRows; ++row)
        {
            ULONGLONG rowStart = m_rangeStart + (ULONGLONG)row * sectorsPerRow;
            ULONGLONG rowEnd   = std::min<ULONGLONG>(rangeEnd, rowStart + sectorsPerRow);
            if (rowStart >= rowEnd) continue;

            ULONGLONG ovStart = std::max<ULONGLONG>(blockStart, rowStart);
            ULONGLONG ovEnd   = std::min<ULONGLONG>(blockEnd, rowEnd);
            if (ovStart >= ovEnd) continue; // No overlap with this row

            double rowWidth = static_cast<double>(rc.Width());
            double rowSectors = static_cast<double>(rowEnd - rowStart);
            int xStart = rc.left + static_cast<int>((ovStart - rowStart) / rowSectors * rowWidth);
            int xEnd   = rc.left + static_cast<int>((ovEnd   - rowStart) / rowSectors * rowWidth);
            if (xEnd <= xStart) xEnd = xStart + 1; // Ensure even tiny blocks are visible

            CRect cellRect(xStart, rc.top + row * rowHeight, xEnd,
                            (row == kRows - 1) ? rc.bottom : rc.top + (row + 1) * rowHeight);
            memDC.FillSolidRect(&cellRect, color);
        }
    }

    // Row separators for readability.
    CPen pen(PS_SOLID, 1, CLR_GRID);
    CPen* pOldPen = memDC.SelectObject(&pen);
    for (int row = 1; row < kRows; ++row)
    {
        int y = rc.top + row * rowHeight;
        memDC.MoveTo(rc.left, y);
        memDC.LineTo(rc.right, y);
    }
    memDC.SelectObject(pOldPen);

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}
