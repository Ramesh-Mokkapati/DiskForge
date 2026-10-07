// SmartHealthDialog.cpp
#include "pch.h"
#include "SmartHealthDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmartHealthDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_CBN_SELCHANGE(IDC_SH_COMBO_DRIVE, &CSmartHealthDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_SH_BTN_REFRESH, &CSmartHealthDialog::OnBtnRefresh)
    ON_BN_CLICKED(IDC_SH_BTN_CHECK,   &CSmartHealthDialog::OnBtnCheck)
    ON_BN_CLICKED(IDC_SH_BTN_CLOSE,   &CSmartHealthDialog::OnBtnClose)
END_MESSAGE_MAP()

CSmartHealthDialog::CSmartHealthDialog(CWnd* pParent)
    : CDialogEx(IDD_SMARTHEALTH, pParent)
{
}

void CSmartHealthDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SH_COMBO_DRIVE,      m_comboDrive);
    DDX_Control(pDX, IDC_SH_BTN_REFRESH,      m_btnRefresh);
    DDX_Control(pDX, IDC_SH_BTN_CHECK,        m_btnCheck);
    DDX_Control(pDX, IDC_SH_STATIC_OVERALL,   m_staticOverall);
    DDX_Control(pDX, IDC_SH_LIST_ATTRIBUTES,  m_listAttributes);
    DDX_Control(pDX, IDC_SH_STATIC_CAVEAT,    m_staticCaveat);
    DDX_Control(pDX, IDC_SH_BTN_CLOSE,        m_btnClose);
}

BOOL CSmartHealthDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Disk Health / S.M.A.R.T. (read-only)"));
    CenterWindow(GetParent());

    m_listAttributes.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listAttributes.InsertColumn(0, _T("ID"),        LVCFMT_RIGHT, 40);
    m_listAttributes.InsertColumn(1, _T("Attribute"), LVCFMT_LEFT,  220);
    m_listAttributes.InsertColumn(2, _T("Current"),   LVCFMT_RIGHT, 70);
    m_listAttributes.InsertColumn(3, _T("Worst"),     LVCFMT_RIGHT, 70);
    m_listAttributes.InsertColumn(4, _T("Raw Value"), LVCFMT_RIGHT, 110);
    m_listAttributes.InsertColumn(5, _T("Flag"),      LVCFMT_LEFT,  90);

    RefreshDriveList();
    return TRUE;
}

void CSmartHealthDialog::RefreshDriveList()
{
    CDiskManager dm;
    m_drives.clear();
    dm.EnumerateDrives(m_drives);

    m_comboDrive.ResetContent();
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);
    if (m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    m_listAttributes.DeleteAllItems();
    m_staticOverall.SetWindowText(_T("Select a drive and click \"Check Health\"."));
}

void CSmartHealthDialog::OnBtnRefresh()
{
    RefreshDriveList();
}

void CSmartHealthDialog::OnComboDriveChange()
{
    m_listAttributes.DeleteAllItems();
    m_staticOverall.SetWindowText(_T("Click \"Check Health\" to query this drive."));
}

void CSmartHealthDialog::OnBtnCheck()
{
    RunHealthCheck();
}

void CSmartHealthDialog::RunHealthCheck()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a drive first."), MB_ICONWARNING);
        return;
    }

    m_listAttributes.DeleteAllItems();
    m_staticOverall.SetWindowText(_T("Querying..."));
    UpdateWindow(); // Let the "Querying..." text actually paint before the (usually near-instant) IOCTLs run

    SmartHealthResult result = CSmartDataReader::ReadHealth(m_drives[sel].devicePath);

    if (!result.querySucceeded)
    {
        CString msg = _T("Health data not available for this drive.\r\n") + result.unsupportedReason;
        m_staticOverall.SetWindowText(msg);
        return;
    }

    CString overall;
    if (result.hasPredictFailureFlag)
    {
        overall = result.failurePredicted
            ? _T("\u26A0 WARNING: this drive is reporting a predicted failure. Back up its data now.")
            : _T("Drive reports no predicted failure.");
    }
    else
    {
        overall = _T("Drive did not respond to the failure-prediction query; showing attribute table only.");
    }

    if (result.temperatureCelsius >= 0)
    {
        CString t;
        t.Format(_T(" Temperature: %d\u00B0C."), result.temperatureCelsius);
        overall += t;
    }

    bool anyCriticalAttr = false;
    for (const auto& a : result.attributes)
        if (a.isCriticalWarning) anyCriticalAttr = true;
    if (anyCriticalAttr)
        overall += _T(" \u26A0 One or more error-count attributes are nonzero - see the flagged rows below.");

    if (!result.hasAttributeTable)
        overall += _T(" (Detailed SMART attribute table not supported on this drive/controller.)");

    m_staticOverall.SetWindowText(overall);

    for (size_t i = 0; i < result.attributes.size(); ++i)
    {
        const SmartAttribute& a = result.attributes[i];

        CString idStr, curStr, worstStr, rawStr;
        idStr.Format(_T("%u"), a.id);
        curStr.Format(_T("%u"), a.currentValue);
        worstStr.Format(_T("%u"), a.worstValue);
        rawStr.Format(_T("%llu"), a.rawValue);

        int row = m_listAttributes.InsertItem((int)i, idStr);
        m_listAttributes.SetItemText(row, 1, a.name);
        m_listAttributes.SetItemText(row, 2, curStr);
        m_listAttributes.SetItemText(row, 3, worstStr);
        m_listAttributes.SetItemText(row, 4, rawStr);
        m_listAttributes.SetItemText(row, 5, a.isCriticalWarning ? _T("WARNING") : _T(""));
    }
}

void CSmartHealthDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}

void CSmartHealthDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_listAttributes.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    m_comboDrive.MoveWindow(L + 60, 10, (R - 220) - (L + 60), 190, TRUE);
    m_btnRefresh.MoveWindow(R - 210, 10, 105, 22, TRUE);
    m_btnCheck.MoveWindow(R - 100, 10, 100, 22, TRUE);
    m_staticOverall.MoveWindow(L, 42, R - L, 36, TRUE);
    m_listAttributes.MoveWindow(L, 84, R - L, cy - 84 - 66, TRUE);
    m_staticCaveat.MoveWindow(L, cy - 58, R - L, 24, TRUE);
    m_btnClose.MoveWindow(R - 90, cy - 30, 90, 24, TRUE);
}

void CSmartHealthDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
