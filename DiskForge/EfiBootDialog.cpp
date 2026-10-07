// EfiBootDialog.cpp
#include "pch.h"
#include "EfiBootDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CEfiBootDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_EB_LIST_ENTRIES, &CEfiBootDialog::OnListSelChange)
    ON_BN_CLICKED(IDC_EB_BTN_MOVE_UP, &CEfiBootDialog::OnBtnMoveUp)
    ON_BN_CLICKED(IDC_EB_BTN_MOVE_DOWN, &CEfiBootDialog::OnBtnMoveDown)
    ON_BN_CLICKED(IDC_EB_BTN_APPLY_ORDER, &CEfiBootDialog::OnBtnApplyOrder)
    ON_BN_CLICKED(IDC_EB_BTN_SET_NEXT, &CEfiBootDialog::OnBtnSetNext)
    ON_BN_CLICKED(IDC_EB_BTN_CLEAR_NEXT, &CEfiBootDialog::OnBtnClearNext)
    ON_BN_CLICKED(IDC_EB_BTN_DELETE, &CEfiBootDialog::OnBtnDelete)
    ON_BN_CLICKED(IDC_EB_BTN_REFRESH, &CEfiBootDialog::OnBtnRefresh)
    ON_BN_CLICKED(IDC_EB_BTN_CLOSE, &CEfiBootDialog::OnBtnClose)
END_MESSAGE_MAP()

CEfiBootDialog::CEfiBootDialog(CWnd* pParent)
    : CDialogEx(IDD_EFIBOOT, pParent)
    , m_orderDirty(false)
    , m_hasBootNext(false)
    , m_bootNext(0)
{
}

void CEfiBootDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EB_LIST_ENTRIES,   m_listEntries);
    DDX_Control(pDX, IDC_EB_BTN_MOVE_UP,    m_btnMoveUp);
    DDX_Control(pDX, IDC_EB_BTN_MOVE_DOWN,  m_btnMoveDown);
    DDX_Control(pDX, IDC_EB_BTN_APPLY_ORDER,m_btnApplyOrder);
    DDX_Control(pDX, IDC_EB_BTN_SET_NEXT,   m_btnSetNext);
    DDX_Control(pDX, IDC_EB_BTN_CLEAR_NEXT, m_btnClearNext);
    DDX_Control(pDX, IDC_EB_BTN_DELETE,     m_btnDelete);
    DDX_Control(pDX, IDC_EB_BTN_REFRESH,    m_btnRefresh);
    DDX_Control(pDX, IDC_EB_STATIC_STATUS,  m_staticStatus);
    DDX_Control(pDX, IDC_EB_BTN_CLOSE,      m_btnClose);
}

BOOL CEfiBootDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("UEFI Boot Entry Manager"));
    CenterWindow(GetParent());

    m_listEntries.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listEntries.InsertColumn(0, _T("Order"),       LVCFMT_RIGHT,  50);
    m_listEntries.InsertColumn(1, _T("ID"),          LVCFMT_LEFT,   70);
    m_listEntries.InsertColumn(2, _T("Description"), LVCFMT_LEFT,  260);
    m_listEntries.InsertColumn(3, _T("Active"),      LVCFMT_CENTER, 60);
    m_listEntries.InsertColumn(4, _T("Next Boot"),   LVCFMT_CENTER, 80);

    LoadFromFirmware();

    return TRUE;
}

void CEfiBootDialog::LoadFromFirmware()
{
    EfiBootInfo info = CEfiBootManager::ReadBootInfo();

    if (!info.supported)
    {
        m_entries.clear();
        m_listEntries.DeleteAllItems();
        m_staticStatus.SetWindowText(_T("Not available: ") + info.unsupportedReason);
        UpdateButtonStates();
        return;
    }

    m_entries = info.entries;
    m_orderDirty = false;
    m_hasBootNext = info.hasBootNext;
    m_bootNext = info.bootNext;

    PopulateList();

    CString status;
    status.Format(_T("%d boot entr%s found (entries not currently listed in BootOrder are not shown - see README)."),
                   (int)m_entries.size(), m_entries.size() == 1 ? _T("y") : _T("ies"));
    m_staticStatus.SetWindowText(status);

    UpdateButtonStates();
}

void CEfiBootDialog::PopulateList()
{
    m_listEntries.DeleteAllItems();
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const EfiBootEntry& e = m_entries[i];

        CString orderStr, idStr;
        orderStr.Format(_T("%d"), (int)i + 1);
        idStr.Format(_T("Boot%04X"), e.id);

        int row = m_listEntries.InsertItem((int)i, orderStr);
        m_listEntries.SetItemText(row, 1, idStr);
        m_listEntries.SetItemText(row, 2, e.description);
        m_listEntries.SetItemText(row, 3, e.active ? _T("Yes") : _T("No"));
        m_listEntries.SetItemText(row, 4, (m_hasBootNext && m_bootNext == e.id) ? _T("\u2192 NEXT") : _T(""));
    }
}

void CEfiBootDialog::UpdateButtonStates()
{
    int sel = m_listEntries.GetNextItem(-1, LVNI_SELECTED);
    bool hasSel = (sel >= 0 && sel < (int)m_entries.size());

    m_btnMoveUp.EnableWindow(hasSel && sel > 0);
    m_btnMoveDown.EnableWindow(hasSel && sel < (int)m_entries.size() - 1);
    m_btnApplyOrder.EnableWindow(m_orderDirty);
    m_btnSetNext.EnableWindow(hasSel);
    m_btnClearNext.EnableWindow(m_hasBootNext);
    m_btnDelete.EnableWindow(hasSel);
}

void CEfiBootDialog::OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
    UpdateButtonStates();
    *pResult = 0;
}

void CEfiBootDialog::OnBtnMoveUp()
{
    int sel = m_listEntries.GetNextItem(-1, LVNI_SELECTED);
    if (sel <= 0 || sel >= (int)m_entries.size())
        return;
    std::swap(m_entries[sel - 1], m_entries[sel]);
    m_orderDirty = true;
    PopulateList();
    m_listEntries.SetItemState(sel - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    UpdateButtonStates();
}

void CEfiBootDialog::OnBtnMoveDown()
{
    int sel = m_listEntries.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_entries.size() - 1)
        return;
    std::swap(m_entries[sel + 1], m_entries[sel]);
    m_orderDirty = true;
    PopulateList();
    m_listEntries.SetItemState(sel + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    UpdateButtonStates();
}

void CEfiBootDialog::OnBtnApplyOrder()
{
    if (!m_orderDirty)
        return;

    CString listDesc;
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        CString line;
        line.Format(_T("%d. Boot%04X - %s\r\n"), (int)i + 1, m_entries[i].id, m_entries[i].description.GetString());
        listDesc += line;
    }

    CString msg = _T("Apply this new boot order?\r\n\r\n") + listDesc;
    if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    std::vector<WORD> order;
    for (const auto& e : m_entries)
        order.push_back(e.id);

    CString err = CEfiBootManager::SetBootOrder(order);
    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Boot order updated."), MB_ICONINFORMATION);
        LoadFromFirmware();
    }
    else
    {
        AfxMessageBox(_T("Failed to update boot order:\r\n") + err, MB_ICONERROR);
    }
}

void CEfiBootDialog::OnBtnSetNext()
{
    int sel = m_listEntries.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_entries.size())
        return;

    const EfiBootEntry& e = m_entries[sel];
    CString msg;
    msg.Format(_T("Boot from \"%s\" (Boot%04X) on the very next restart only? ")
               _T("After that one boot, the normal boot order resumes automatically."),
               e.description.GetString(), e.id);
    if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    CString err = CEfiBootManager::SetBootNext(e.id);
    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Next-boot override set."), MB_ICONINFORMATION);
        LoadFromFirmware();
    }
    else
    {
        AfxMessageBox(_T("Failed to set next-boot override:\r\n") + err, MB_ICONERROR);
    }
}

void CEfiBootDialog::OnBtnClearNext()
{
    CString err = CEfiBootManager::ClearBootNext();
    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Next-boot override cleared."), MB_ICONINFORMATION);
        LoadFromFirmware();
    }
    else
    {
        AfxMessageBox(_T("Failed to clear next-boot override:\r\n") + err, MB_ICONERROR);
    }
}

void CEfiBootDialog::OnBtnDelete()
{
    int sel = m_listEntries.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_entries.size())
        return;

    const EfiBootEntry& e = m_entries[sel];
    CString msg;
    msg.Format(_T("Permanently delete boot entry \"%s\" (Boot%04X)?\r\n\r\n")
               _T("It will also be removed from the boot order. You can usually recreate a missing boot ")
               _T("entry by reinstalling/repairing that OS's boot loader, but this app cannot recreate one."),
               e.description.GetString(), e.id);
    if (AfxMessageBox(msg, MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    CString err = CEfiBootManager::DeleteBootEntry(e.id);
    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Boot entry deleted."), MB_ICONINFORMATION);
        LoadFromFirmware();
    }
    else
    {
        AfxMessageBox(_T("Failed to delete the boot entry:\r\n") + err, MB_ICONERROR);
    }
}

void CEfiBootDialog::OnBtnRefresh()
{
    if (m_orderDirty)
    {
        if (AfxMessageBox(_T("You have an unapplied order change. Discard it and reload from firmware?"),
                           MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }
    LoadFromFirmware();
}

void CEfiBootDialog::OnBtnClose()
{
    if (m_orderDirty)
    {
        if (AfxMessageBox(_T("You have an unapplied order change that will be discarded. Close anyway?"),
                           MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }
    EndDialog(IDCANCEL);
}

void CEfiBootDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_listEntries.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    CRect rc;

    // Right-column button width from template (DPI-correct).
    m_btnMoveUp.GetWindowRect(&rc); ScreenToClient(&rc);
    const int BTN_W = rc.Width();
    const int BTN_COL = R - BTN_W - 4;

    // Move each right-column button to BTN_COL, keeping its template y/height.
    auto rightBtn = [&](CWnd& w) {
        w.GetWindowRect(&rc); ScreenToClient(&rc);
        w.MoveWindow(BTN_COL, rc.top, BTN_W, rc.Height(), TRUE);
    };
    rightBtn(m_btnMoveUp);
    rightBtn(m_btnMoveDown);
    rightBtn(m_btnApplyOrder);
    rightBtn(m_btnSetNext);
    rightBtn(m_btnClearNext);
    rightBtn(m_btnDelete);
    rightBtn(m_btnRefresh);

    // Status bar and Close button are bottom-anchored.
    CRect rBh(0, 0, 0, 17); MapDialogRect(&rBh);
    const int bh = rBh.bottom, by = cy - bh - 6;

    m_staticStatus.GetWindowRect(&rc); ScreenToClient(&rc);
    const int statH = rc.Height();
    const int statBy = by - statH - 4;
    m_staticStatus.MoveWindow(L, statBy, R - L, statH, TRUE);

    m_btnClose.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnClose.MoveWindow(R - rc.Width(), by, rc.Width(), bh, TRUE);

    // List fills left area from its template top to above status bar.
    m_listEntries.GetWindowRect(&rc); ScreenToClient(&rc);
    int listH = statBy - rc.top - 8;
    if (listH < 40) listH = 40;
    m_listEntries.MoveWindow(L, rc.top, BTN_COL - L - 4, listH, TRUE);
}

void CEfiBootDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
