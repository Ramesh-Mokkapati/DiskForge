// OutputWnd.h - Output docking pane (from the NewTest framework): a tabbed
// log where the System Browser, recovery and tools report what they did.
#pragma once

class COutputList : public CListBox
{
public:
    COutputList() noexcept = default;

protected:
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnEditCopy();
    afx_msg void OnEditClear();
    afx_msg void OnViewOutput();

    DECLARE_MESSAGE_MAP()
};

class COutputWnd : public CDockablePane
{
public:
    COutputWnd() noexcept = default;

    void UpdateFonts();
    void AddLine(const CString& text);

protected:
    CMFCTabCtrl m_wndTabs;
    COutputList m_wndOutputLog;

    void AdjustHorzScroll(CListBox& wndListBox);

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()
};
