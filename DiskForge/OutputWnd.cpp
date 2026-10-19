// OutputWnd.cpp - Output docking pane
#include "pch.h"
#include "OutputWnd.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(COutputWnd, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
END_MESSAGE_MAP()

int COutputWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CDockablePane::OnCreate(lpCreateStruct) == -1)
        return -1;

    CRect rectDummy;
    rectDummy.SetRectEmpty();

    if (!m_wndTabs.Create(CMFCTabCtrl::STYLE_FLAT, rectDummy, this, 1))
        return -1;

    const DWORD dwStyle = LBS_NOINTEGRALHEIGHT | LBS_EXTENDEDSEL | WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL;
    if (!m_wndOutputLog.Create(dwStyle, rectDummy, &m_wndTabs, 2))
        return -1;

    UpdateFonts();

    CString strTabName;
    VERIFY(strTabName.LoadString(IDS_LOG_TAB));
    m_wndTabs.AddTab(&m_wndOutputLog, strTabName, (UINT)0);
    return 0;
}

void COutputWnd::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    m_wndTabs.SetWindowPos(nullptr, -1, -1, cx, cy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void COutputWnd::AdjustHorzScroll(CListBox& wndListBox)
{
    CClientDC dc(this);
    CFont* pOldFont = dc.SelectObject(&afxGlobalData.fontRegular);

    int cxExtentMax = 0;
    for (int i = 0; i < wndListBox.GetCount(); i++)
    {
        CString strItem;
        wndListBox.GetText(i, strItem);
        cxExtentMax = std::max<int>(cxExtentMax, (int)dc.GetTextExtent(strItem).cx);
    }

    wndListBox.SetHorizontalExtent(cxExtentMax);
    dc.SelectObject(pOldFont);
}

void COutputWnd::AddLine(const CString& text)
{
    if (!m_wndOutputLog.GetSafeHwnd())
        return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    CString line;
    line.Format(_T("%02u:%02u:%02u  %s"), st.wHour, st.wMinute, st.wSecond, text.GetString());
    int idx = m_wndOutputLog.AddString(line);
    m_wndOutputLog.SetTopIndex(idx);
    AdjustHorzScroll(m_wndOutputLog);
}

void COutputWnd::UpdateFonts()
{
    m_wndOutputLog.SetFont(&afxGlobalData.fontRegular);
}

/////////////////////////////////////////////////////////////////////////////
// COutputList

BEGIN_MESSAGE_MAP(COutputList, CListBox)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
    ON_COMMAND(ID_EDIT_CLEAR, OnEditClear)
    ON_COMMAND(ID_VIEW_OUTPUTWND, OnViewOutput)
END_MESSAGE_MAP()

void COutputList::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
    CMenu menu;
    menu.LoadMenu(IDR_OUTPUT_POPUP);
    CMenu* pSubMenu = menu.GetSubMenu(0);

    CMFCPopupMenu* pPopupMenu = new CMFCPopupMenu;
    if (!pPopupMenu->Create(this, point.x, point.y, (HMENU)pSubMenu->m_hMenu, FALSE, TRUE))
        return;
    SetFocus();
}

void COutputList::OnEditCopy()
{
    CString all;
    int sel = GetSelCount();
    for (int i = 0; i < GetCount(); ++i)
    {
        if (sel > 0 && GetSel(i) <= 0)
            continue;
        CString line;
        GetText(i, line);
        all += line + _T("\r\n");
    }
    if (all.IsEmpty() || !OpenClipboard())
        return;
    EmptyClipboard();
    size_t bytes = (all.GetLength() + 1) * sizeof(TCHAR);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h)
    {
        memcpy(GlobalLock(h), all.GetString(), bytes);
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
    }
    CloseClipboard();
}

void COutputList::OnEditClear()
{
    ResetContent();
}

void COutputList::OnViewOutput()
{
    CDockablePane* pParentBar = DYNAMIC_DOWNCAST(CDockablePane, GetOwner());
    CFrameWndEx* pMainFrame = DYNAMIC_DOWNCAST(CFrameWndEx, GetTopLevelFrame());
    if (pMainFrame != nullptr && pParentBar != nullptr)
    {
        pMainFrame->SetFocus();
        pMainFrame->ShowPane(pParentBar, FALSE, FALSE, FALSE);
        pMainFrame->RecalcLayout();
    }
}
