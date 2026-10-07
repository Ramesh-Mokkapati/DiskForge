// HexView.cpp - Custom hex-dump view control implementation
#include "pch.h"
#include "HexView.h"

static const TCHAR HEX_VIEW_CLASS[] = _T("DiskSectorHexView");

// Colours
static const COLORREF CLR_BG         = RGB(30,  30,  40);   // Dark background
static const COLORREF CLR_OFFSET     = RGB(100, 180, 255);  // Blue offset column
static const COLORREF CLR_HEX        = RGB(220, 220, 200);  // Light hex bytes
static const COLORREF CLR_HEX_ALT    = RGB(180, 180, 160);  // Alternating group
static const COLORREF CLR_ASCII      = RGB(160, 220, 130);  // Green ASCII
static const COLORREF CLR_ASCII_DOT  = RGB(100, 100,  90);  // Non-printable dots
static const COLORREF CLR_SEL_BG     = RGB(60,  100, 180);  // Selected byte background
static const COLORREF CLR_SEL_FG     = RGB(255, 255, 255);  // Selected byte text
static const COLORREF CLR_RULER      = RGB(60,   60,  80);  // Ruler / separator
static const COLORREF CLR_RULER_TEXT = RGB(140, 140, 160);

BEGIN_MESSAGE_MAP(CHexView, CWnd)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_VSCROLL()
    ON_WM_HSCROLL()
    ON_WM_KEYDOWN()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CHexView::CHexView()
    : m_baseOffset(0)
    , m_scrollPosV(0)
    , m_scrollPosH(0)
    , m_selectedByte(-1)
    , m_charWidth(8)
    , m_lineHeight(16)
    , m_bytesPerRow(BYTES_PER_ROW)
    , m_totalRows(0)
    , m_visibleRows(0)
    , m_colOffset(0)
    , m_colHex(0)
    , m_colAscii(0)
    , m_totalWidth(800)
{
}

CHexView::~CHexView()
{
}

// static
bool CHexView::RegisterClass()
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = ::DefWindowProc;
    wc.hInstance     = AfxGetInstanceHandle();
    wc.hCursor       = ::LoadCursor(nullptr, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = HEX_VIEW_CLASS;

    // If already registered that's fine
    ATOM a = ::RegisterClassEx(&wc);
    return (a != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
}

bool CHexView::Create(CWnd* pParent, const RECT& rect, UINT nID)
{
    RegisterClass();

    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER |
                  WS_VSCROLL | WS_HSCROLL | WS_TABSTOP;

    return CWnd::Create(HEX_VIEW_CLASS, nullptr, style, rect, pParent, nID);
}

void CHexView::SetData(const std::vector<BYTE>& data, ULONGLONG baseOffset)
{
    m_data        = data;
    m_baseOffset  = baseOffset;
    m_scrollPosV  = 0;
    m_scrollPosH  = 0;
    m_selectedByte = -1;
    RecalcLayout();
    UpdateScrollBars();
    Invalidate();
}

void CHexView::ClearData()
{
    m_data.clear();
    m_baseOffset   = 0;
    m_scrollPosV   = 0;
    m_scrollPosH   = 0;
    m_selectedByte = -1;
    RecalcLayout();
    UpdateScrollBars();
    Invalidate();
}

void CHexView::RecalcLayout()
{
    CClientDC dc(this);

    // Create / select Courier New font
    if (m_font.GetSafeHandle() == nullptr)
    {
        LOGFONT lf = {};
        lf.lfHeight         = -14;
        lf.lfWeight         = FW_NORMAL;
        lf.lfCharSet        = ANSI_CHARSET;
        lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        _tcscpy_s(lf.lfFaceName, _T("Courier New"));
        m_font.CreateFontIndirect(&lf);
    }

    CFont* pOld = dc.SelectObject(&m_font);
    TEXTMETRIC tm;
    dc.GetTextMetrics(&tm);
    m_charWidth  = tm.tmAveCharWidth;
    m_lineHeight = tm.tmHeight + tm.tmExternalLeading + 2;
    dc.SelectObject(pOld);

    // Column positions (in pixels)
    // Format: "0x0000000000000000: " = 20 chars
    // Hex:    "XX XX XX XX XX XX XX XX  XX XX XX XX XX XX XX XX  " = 49 chars
    // ASCII:  "................"  = 16 chars
    m_colOffset = 0;
    int offsetCols = 20; // "0x0000000000000000: "
    m_colHex   = offsetCols * m_charWidth;
    // Each byte: "XX " = 3 chars, plus space every 8 bytes
    int hexCols = (BYTES_PER_ROW * 3) + 2; // 2 extra spaces for the mid-gap
    m_colAscii = m_colHex + hexCols * m_charWidth + m_charWidth;
    m_totalWidth = m_colAscii + (BYTES_PER_ROW + 2) * m_charWidth;

    // Row count
    m_bytesPerRow = BYTES_PER_ROW;
    int dataSize = (int)m_data.size();
    m_totalRows = (dataSize > 0) ? ((dataSize + m_bytesPerRow - 1) / m_bytesPerRow) : 0;

    // Visible rows
    CRect rc;
    GetClientRect(&rc);
    m_visibleRows = (rc.Height() > 0 && m_lineHeight > 0) ? (rc.Height() / m_lineHeight) : 1;
}

void CHexView::UpdateScrollBars()
{
    // Vertical
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_ALL;
    si.nMin   = 0;
    si.nMax   = (m_totalRows > 0) ? m_totalRows - 1 : 0;
    si.nPage  = max(1, m_visibleRows);
    si.nPos   = m_scrollPosV;
    SetScrollInfo(SB_VERT, &si, TRUE);

    // Horizontal
    CRect rc;
    GetClientRect(&rc);
    si.nMin  = 0;
    si.nMax  = max(0, m_totalWidth - 1);
    si.nPage = max(1, (int)rc.Width());
    si.nPos  = m_scrollPosH;
    SetScrollInfo(SB_HORZ, &si, TRUE);
}

BOOL CHexView::OnEraseBkgnd(CDC* pDC)
{
    // We paint the background ourselves in OnPaint
    return TRUE;
}

void CHexView::OnPaint()
{
    CPaintDC dc(this);

    CRect rcClient;
    GetClientRect(&rcClient);

    // Double-buffer
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // Fill background
    memDC.FillSolidRect(&rcClient, CLR_BG);

    if (m_data.empty())
    {
        // Draw placeholder text
        CFont* pOld = memDC.SelectObject(&m_font);
        memDC.SetTextColor(RGB(120, 120, 130));
        memDC.SetBkMode(TRANSPARENT);
        CString msg(_T("No sector data loaded. Select a drive and click 'Open Drive'."));
        memDC.DrawText(msg, &rcClient, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        memDC.SelectObject(pOld);
    }
    else
    {
        CFont* pOld = memDC.SelectObject(&m_font);
        memDC.SetBkMode(OPAQUE);

        int startRow = m_scrollPosV;
        int endRow   = min(m_totalRows, startRow + m_visibleRows + 1);

        for (int row = startRow; row < endRow; ++row)
        {
            int y = (row - startRow) * m_lineHeight;
            DrawRow(memDC, row, y, rcClient);
        }

        memDC.SelectObject(pOld);
    }

    // Blit to screen
    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CHexView::DrawRow(CDC& dc, int rowIndex, int y, const CRect& /*clipRect*/)
{
    int baseIdx = rowIndex * m_bytesPerRow;
    if (baseIdx >= (int)m_data.size())
        return;

    int hOff = m_scrollPosH; // horizontal scroll offset in pixels

    ULONGLONG offset = m_baseOffset + (ULONGLONG)baseIdx;

    // --- Offset column ---
    CString offsetStr;
    offsetStr.Format(_T("0x%016I64X: "), offset);

    int xOff = m_colOffset - hOff;
    dc.SetTextColor(CLR_OFFSET);
    dc.SetBkColor(CLR_BG);
    dc.TextOut(xOff, y, offsetStr);

    // --- Hex bytes ---
    for (int col = 0; col < m_bytesPerRow; ++col)
    {
        int idx = baseIdx + col;
        if (idx >= (int)m_data.size())
            break;

        BYTE b = m_data[idx];

        // Position: each byte is 3 chars wide, with a 1-char gap after byte 7
        int hexCharPos = col * 3 + (col >= 8 ? 1 : 0);
        int xHex = m_colHex + hexCharPos * m_charWidth - hOff;

        CString hexStr;
        hexStr.Format(_T("%02X "), b);

        bool selected = (idx == m_selectedByte);
        COLORREF fg = selected ? CLR_SEL_FG : ((col < 8) ? CLR_HEX : CLR_HEX_ALT);
        COLORREF bg = selected ? CLR_SEL_BG : CLR_BG;

        dc.SetTextColor(fg);
        dc.SetBkColor(bg);
        dc.TextOut(xHex, y, hexStr);
    }

    // --- ASCII column ---
    for (int col = 0; col < m_bytesPerRow; ++col)
    {
        int idx = baseIdx + col;
        if (idx >= (int)m_data.size())
            break;

        BYTE b = m_data[idx];
        int xAsc = m_colAscii + col * m_charWidth - hOff;

        bool selected = (idx == m_selectedByte);
        COLORREF bg   = selected ? CLR_SEL_BG : CLR_BG;
        dc.SetBkColor(bg);

        if (b >= 0x20 && b < 0x7F)
        {
            dc.SetTextColor(selected ? CLR_SEL_FG : CLR_ASCII);
            TCHAR ch[2] = { (TCHAR)b, 0 };
            dc.TextOut(xAsc, y, ch, 1);
        }
        else
        {
            dc.SetTextColor(selected ? CLR_SEL_FG : CLR_ASCII_DOT);
            dc.TextOut(xAsc, y, _T("."), 1);
        }
    }
}

void CHexView::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    RecalcLayout();
    UpdateScrollBars();
    Invalidate();
}

void CHexView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    int maxPos = max(0, m_totalRows - m_visibleRows);

    switch (nSBCode)
    {
    case SB_LINEUP:        m_scrollPosV -= 1; break;
    case SB_LINEDOWN:      m_scrollPosV += 1; break;
    case SB_PAGEUP:        m_scrollPosV -= m_visibleRows; break;
    case SB_PAGEDOWN:      m_scrollPosV += m_visibleRows; break;
    case SB_TOP:           m_scrollPosV = 0; break;
    case SB_BOTTOM:        m_scrollPosV = maxPos; break;
    case SB_THUMBPOSITION: m_scrollPosV = (int)nPos; break;
    case SB_THUMBTRACK:    m_scrollPosV = (int)nPos; break;
    default: return;
    }

    m_scrollPosV = max(0, min(m_scrollPosV, maxPos));
    SetScrollPos(SB_VERT, m_scrollPosV, TRUE);
    Invalidate();
}

void CHexView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CRect rc;
    GetClientRect(&rc);
    int maxPos = max(0, m_totalWidth - rc.Width());

    switch (nSBCode)
    {
    case SB_LINELEFT:      m_scrollPosH -= m_charWidth;  break;
    case SB_LINERIGHT:     m_scrollPosH += m_charWidth;  break;
    case SB_PAGELEFT:      m_scrollPosH -= rc.Width();   break;
    case SB_PAGERIGHT:     m_scrollPosH += rc.Width();   break;
    case SB_LEFT:          m_scrollPosH = 0; break;
    case SB_RIGHT:         m_scrollPosH = maxPos; break;
    case SB_THUMBPOSITION: m_scrollPosH = (int)nPos; break;
    case SB_THUMBTRACK:    m_scrollPosH = (int)nPos; break;
    default: return;
    }

    m_scrollPosH = max(0, min(m_scrollPosH, maxPos));
    SetScrollPos(SB_HORZ, m_scrollPosH, TRUE);
    Invalidate();
}

BOOL CHexView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    int lines = (zDelta > 0) ? -3 : 3;
    m_scrollPosV = max(0, min(m_scrollPosV + lines, max(0, m_totalRows - m_visibleRows)));
    SetScrollPos(SB_VERT, m_scrollPosV, TRUE);
    Invalidate();
    return TRUE;
}

void CHexView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    switch (nChar)
    {
    case VK_UP:    OnVScroll(SB_LINEUP,    0, nullptr); break;
    case VK_DOWN:  OnVScroll(SB_LINEDOWN,  0, nullptr); break;
    case VK_PRIOR: OnVScroll(SB_PAGEUP,    0, nullptr); break;
    case VK_NEXT:  OnVScroll(SB_PAGEDOWN,  0, nullptr); break;
    case VK_HOME:  OnVScroll(SB_TOP,       0, nullptr); break;
    case VK_END:   OnVScroll(SB_BOTTOM,    0, nullptr); break;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

int CHexView::HitTestByte(CPoint pt) const
{
    if (m_data.empty()) return -1;

    int hOff = m_scrollPosH;

    // Check hex region
    int xHexStart = m_colHex - hOff;
    int xAscStart = m_colAscii - hOff;
    int xAscEnd   = xAscStart + BYTES_PER_ROW * m_charWidth;

    int row = m_scrollPosV + pt.y / m_lineHeight;
    if (row < 0 || row >= m_totalRows) return -1;

    int col = -1;

    if (pt.x >= xHexStart && pt.x < xAscStart)
    {
        // In hex region
        int relX = pt.x - xHexStart;
        // Each byte group: 3 chars wide; mid-gap after byte 7
        // Account for gap
        int byteWidth = m_charWidth * 3;
        int halfPoint = m_charWidth * 3 * 8 + m_charWidth; // 8 bytes + 1 gap char

        int localX = relX;
        if (localX >= halfPoint)
            localX -= m_charWidth; // subtract gap

        col = localX / byteWidth;
    }
    else if (pt.x >= xAscStart && pt.x < xAscEnd)
    {
        col = (pt.x - xAscStart) / m_charWidth;
    }

    if (col < 0 || col >= BYTES_PER_ROW) return -1;

    int idx = row * BYTES_PER_ROW + col;
    if (idx < 0 || idx >= (int)m_data.size()) return -1;
    return idx;
}

void CHexView::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();
    int idx = HitTestByte(point);
    if (idx != m_selectedByte)
    {
        m_selectedByte = idx;
        Invalidate();
    }
    CWnd::OnLButtonDown(nFlags, point);
}

void CHexView::OnSetFocus(CWnd* pOldWnd)
{
    CWnd::OnSetFocus(pOldWnd);
    Invalidate();
}

void CHexView::OnKillFocus(CWnd* pNewWnd)
{
    CWnd::OnKillFocus(pNewWnd);
    Invalidate();
}
