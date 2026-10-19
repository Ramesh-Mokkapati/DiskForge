// ViewTree.h - Tree control used on the Tools bar pages (from the NewTest
// framework). Each item's data is a command ID; clicking an item sends it to
// the main frame.
#pragma once

class CViewTree : public CTreeCtrl
{
public:
    CViewTree() noexcept = default;

    HTREEITEM AddCommand(LPCTSTR text, UINT commandId, HTREEITEM parent = TVI_ROOT);

protected:
    virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

    void RunCommand(HTREEITEM item);

    DECLARE_MESSAGE_MAP()
};
