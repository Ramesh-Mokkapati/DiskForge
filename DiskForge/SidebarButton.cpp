// SidebarButton.cpp
#include "pch.h"
#include "SidebarButton.h"

namespace
{
    const COLORREF kBg        = RGB(30, 41, 59);   // base sidebar navy
    const COLORREF kBgHover   = RGB(42, 55, 78);
    const COLORREF kBgPressed = RGB(51, 65, 90);
    const COLORREF kAccent    = RGB(74, 144, 226);
    const COLORREF kText      = RGB(226, 232, 240);
    const COLORREF kTextDim   = RGB(148, 163, 184); // used when disabled
}

BEGIN_MESSAGE_MAP(CSidebarButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

CSidebarButton::CSidebarButton()
    : m_hovered(false)
    , m_tracking(false)
{
}

void CSidebarButton::DrawItem(LPDRAWITEMSTRUCT di)
{
    CDC dc;
    dc.Attach(di->hDC);
    CRect rc(di->rcItem);

    bool pressed  = (di->itemState & ODS_SELECTED) != 0;
    bool disabled = (di->itemState & ODS_DISABLED) != 0;

    COLORREF bg = kBg;
    if (pressed)      bg = kBgPressed;
    else if (m_hovered && !disabled) bg = kBgHover;

    dc.FillSolidRect(&rc, bg);

    if ((m_hovered || pressed) && !disabled)
    {
        CRect accent(rc.left, rc.top, rc.left + 3, rc.bottom);
        dc.FillSolidRect(&accent, kAccent);
    }

    CString text;
    GetWindowText(text);

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(disabled ? kTextDim : kText);

    CRect textRect(rc.left + 20, rc.top, rc.right - 8, rc.bottom);
    CFont* pFont = GetFont();
    CFont* pOld = pFont ? dc.SelectObject(pFont) : nullptr;
    dc.DrawText(text, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    if (pOld) dc.SelectObject(pOld);

    dc.Detach();
}

void CSidebarButton::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        _TrackMouseEvent(&tme);

        m_tracking = true;
        m_hovered = true;
        Invalidate();
    }
    CButton::OnMouseMove(nFlags, point);
}

void CSidebarButton::OnMouseLeave()
{
    m_tracking = false;
    m_hovered = false;
    Invalidate();
}
