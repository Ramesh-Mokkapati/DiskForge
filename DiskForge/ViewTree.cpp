// ViewTree.cpp
#include "pch.h"
#include "ViewTree.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CViewTree, CTreeCtrl)
    ON_NOTIFY_REFLECT(NM_CLICK, &CViewTree::OnClick)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

BOOL CViewTree::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    BOOL bRes = CTreeCtrl::OnNotify(wParam, lParam, pResult);

    NMHDR* pNMHDR = (NMHDR*)lParam;
    ASSERT(pNMHDR != nullptr);
    if (pNMHDR && pNMHDR->code == TTN_SHOW && GetToolTips() != nullptr)
        GetToolTips()->SetWindowPos(&wndTop, -1, -1, -1, -1, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
    return bRes;
}

HTREEITEM CViewTree::AddCommand(LPCTSTR text, UINT commandId, HTREEITEM parent)
{
    HTREEITEM h = InsertItem(text, parent, TVI_LAST);
    SetItemData(h, commandId);
    return h;
}

void CViewTree::RunCommand(HTREEITEM item)
{
    if (!item)
        return;
    UINT id = (UINT)GetItemData(item);
    if (id != 0)
        AfxGetMainWnd()->PostMessage(WM_COMMAND, MAKEWPARAM(id, 0), 0);
}

void CViewTree::OnClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    CPoint pt;
    GetCursorPos(&pt);
    ScreenToClient(&pt);
    UINT flags = 0;
    HTREEITEM item = HitTest(pt, &flags);
    if (item && (flags & TVHT_ONITEM))
    {
        SelectItem(item);
        RunCommand(item);
    }
}

void CViewTree::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_RETURN || nChar == VK_SPACE)
        RunCommand(GetSelectedItem());
    else
        CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}
