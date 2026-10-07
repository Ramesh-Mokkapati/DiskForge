// RegistryScannerDialog.cpp
#include "pch.h"
#include "RegistryScannerDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CRegistryScannerDialog, CDialogEx)
    ON_BN_CLICKED(IDC_RS_BTN_SCAN,       &CRegistryScannerDialog::OnBtnScan)
    ON_BN_CLICKED(IDC_RS_BTN_FIX_SEL,    &CRegistryScannerDialog::OnBtnFixSelected)
    ON_BN_CLICKED(IDC_RS_BTN_FIX_ALL,    &CRegistryScannerDialog::OnBtnFixAll)
    ON_BN_CLICKED(IDC_RS_BTN_BACKUP_FIX, &CRegistryScannerDialog::OnBtnBackupAndFix)
    ON_BN_CLICKED(IDC_RS_BTN_IGNORE,     &CRegistryScannerDialog::OnBtnIgnore)
    ON_BN_CLICKED(IDC_RS_BTN_CLOSE,      &CRegistryScannerDialog::OnBtnClose)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_RS_LIST, &CRegistryScannerDialog::OnListItemChanged)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_MESSAGE(WM_RS_PROGRESS, &CRegistryScannerDialog::OnScanProgress)
    ON_MESSAGE(WM_RS_COMPLETE,  &CRegistryScannerDialog::OnScanComplete)
END_MESSAGE_MAP()

CRegistryScannerDialog::CRegistryScannerDialog(CWnd* pParent)
    : CDialogEx(IDD_REGISTRYSCANNER, pParent)
    , m_scanning(false)
{}

void CRegistryScannerDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_RS_CHK_APPPATH,         m_chkAppPath);
    DDX_Control(pDX, IDC_RS_CHK_BROWSER_HELPER,  m_chkBrowserHelper);
    DDX_Control(pDX, IDC_RS_CHK_FILE_EXT,        m_chkFileExt);
    DDX_Control(pDX, IDC_RS_CHK_FIREWALL,        m_chkFirewall);
    DDX_Control(pDX, IDC_RS_CHK_FONTS,           m_chkFonts);
    DDX_Control(pDX, IDC_RS_CHK_HELP_FILES,      m_chkHelpFiles);
    DDX_Control(pDX, IDC_RS_CHK_INSTALLERS,      m_chkInstallers);
    DDX_Control(pDX, IDC_RS_CHK_INTERFACE,       m_chkInterface);
    DDX_Control(pDX, IDC_RS_CHK_MUI_CACHE,       m_chkMuiCache);
    DDX_Control(pDX, IDC_RS_CHK_SHAREDDLL,       m_chkSharedDll);
    DDX_Control(pDX, IDC_RS_CHK_UNINSTALL,       m_chkUninstall);
    DDX_Control(pDX, IDC_RS_CHK_OPEN_WITH,       m_chkOpenWith);
    DDX_Control(pDX, IDC_RS_CHK_STARTUP,         m_chkStartup);
    DDX_Control(pDX, IDC_RS_CHK_SOUND_EVENTS,    m_chkSoundEvents);
    DDX_Control(pDX, IDC_RS_CHK_SERVICES,        m_chkServices);
    DDX_Control(pDX, IDC_RS_BTN_SCAN,            m_btnScan);
    DDX_Control(pDX, IDC_RS_STATIC_PROGRESS,     m_staticProgress);
    DDX_Control(pDX, IDC_RS_PROGRESS,            m_progress);
    DDX_Control(pDX, IDC_RS_LIST,                m_list);
    DDX_Control(pDX, IDC_RS_BTN_FIX_SEL,        m_btnFixSel);
    DDX_Control(pDX, IDC_RS_BTN_FIX_ALL,        m_btnFixAll);
    DDX_Control(pDX, IDC_RS_BTN_BACKUP_FIX,     m_btnBackupFix);
    DDX_Control(pDX, IDC_RS_BTN_IGNORE,         m_btnIgnore);
    DDX_Control(pDX, IDC_RS_BTN_CLOSE,          m_btnClose);
    DDX_Control(pDX, IDC_RS_STATIC_STATUS,      m_staticStatus);
}

BOOL CRegistryScannerDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Registry Scanner"));
    CenterWindow(GetParent());

    // Check all categories by default
    m_chkAppPath.SetCheck(BST_CHECKED);
    m_chkBrowserHelper.SetCheck(BST_CHECKED);
    m_chkFileExt.SetCheck(BST_CHECKED);
    m_chkFirewall.SetCheck(BST_CHECKED);
    m_chkFonts.SetCheck(BST_CHECKED);
    m_chkHelpFiles.SetCheck(BST_CHECKED);
    m_chkInstallers.SetCheck(BST_CHECKED);
    m_chkInterface.SetCheck(BST_CHECKED);
    m_chkMuiCache.SetCheck(BST_CHECKED);
    m_chkSharedDll.SetCheck(BST_CHECKED);
    m_chkUninstall.SetCheck(BST_CHECKED);
    m_chkOpenWith.SetCheck(BST_CHECKED);
    m_chkStartup.SetCheck(BST_CHECKED);
    m_chkSoundEvents.SetCheck(BST_CHECKED);
    m_chkServices.SetCheck(BST_CHECKED);

    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_LABELTIP|LVS_EX_CHECKBOXES);
    m_list.InsertColumn(0, _T("Type"),       LVCFMT_LEFT, 110);
    m_list.InsertColumn(1, _T("Name / Key"), LVCFMT_LEFT, 220);
    m_list.InsertColumn(2, _T("Problem"),    LVCFMT_LEFT, 300);
    m_list.InsertColumn(3, _T("Source"),     LVCFMT_LEFT,  60);
    m_list.InsertColumn(4, _T("Status"),     LVCFMT_LEFT,  70);

    UpdateButtonState();
    return TRUE;
}

void CRegistryScannerDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_W;
    lpMMI->ptMinTrackSize.y = MIN_H;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CRegistryScannerDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0) LayoutControls(cx, cy);
}

void CRegistryScannerDialog::LayoutControls(int cx, int cy)
{
    if (!m_list.GetSafeHwnd()) return;
    CRect rc;

    // Progress bar: stretches cx, keeps template y/height.
    // Scope label, checkboxes, Scan button, and progress label stay at template positions.
    m_progress.GetWindowRect(&rc); ScreenToClient(&rc);
    m_progress.MoveWindow(rc.left, rc.top, cx - rc.left - 8, rc.Height(), TRUE);

    // Bottom section heights (DPI-correct).
    CRect rBh(0, 0, 0, 17); MapDialogRect(&rBh);
    const int bh = rBh.bottom;
    m_staticStatus.GetWindowRect(&rc); ScreenToClient(&rc);
    const int statH = rc.Height();

    const int statBy = cy - statH - 4;
    const int by     = statBy - bh - 4;

    // List fills from its template top to just above the button row.
    m_list.GetWindowRect(&rc); ScreenToClient(&rc);
    int listH = by - rc.top - 4;
    if (listH < 40) listH = 40;
    m_list.MoveWindow(8, rc.top, cx - 16, listH, TRUE);

    // Bottom buttons: keep template x/width, anchor to bottom.
    auto anchorBottom = [&](CWnd& w) {
        w.GetWindowRect(&rc); ScreenToClient(&rc);
        w.MoveWindow(rc.left, by, rc.Width(), bh, TRUE);
    };
    anchorBottom(m_btnFixSel);
    anchorBottom(m_btnFixAll);
    anchorBottom(m_btnBackupFix);
    anchorBottom(m_btnIgnore);
    m_btnClose.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnClose.MoveWindow(cx - rc.Width() - 8, by, rc.Width(), bh, TRUE);

    // Status label: full width, bottom of dialog.
    m_staticStatus.GetWindowRect(&rc); ScreenToClient(&rc);
    m_staticStatus.MoveWindow(8, statBy, cx - 16, statH, TRUE);
}

// ---- Scan thread ----

UINT CRegistryScannerDialog::ScanThreadProc(LPVOID pParam)
{
    ScanParams* p = reinterpret_cast<ScanParams*>(pParam);
    CRegistryScannerDialog* pDlg = p->pDlg;

    pDlg->m_scanner.Scan(
        p->opts,
        [pDlg](int pct, const CString& phase) {
            CString* pPhase = new CString(phase);
            pDlg->PostMessage(WM_RS_PROGRESS, (WPARAM)pct, (LPARAM)pPhase);
        });

    int count = (int)pDlg->m_scanner.GetIssues().size();
    pDlg->PostMessage(WM_RS_COMPLETE, (WPARAM)count, 0);

    delete p;
    return 0;
}

ScanOptions CRegistryScannerDialog::BuildScanOptions() const
{
    ScanOptions o;
    o.appPaths      = (m_chkAppPath.GetCheck()       == BST_CHECKED);
    o.browserHelper = (m_chkBrowserHelper.GetCheck() == BST_CHECKED);
    o.fileExt       = (m_chkFileExt.GetCheck()       == BST_CHECKED);
    o.firewall      = (m_chkFirewall.GetCheck()      == BST_CHECKED);
    o.fonts         = (m_chkFonts.GetCheck()         == BST_CHECKED);
    o.helpFiles     = (m_chkHelpFiles.GetCheck()     == BST_CHECKED);
    o.installers    = (m_chkInstallers.GetCheck()    == BST_CHECKED);
    o.interfaceCom  = (m_chkInterface.GetCheck()     == BST_CHECKED);
    o.muiCache      = (m_chkMuiCache.GetCheck()      == BST_CHECKED);
    o.sharedDlls    = (m_chkSharedDll.GetCheck()     == BST_CHECKED);
    o.uninstall     = (m_chkUninstall.GetCheck()     == BST_CHECKED);
    o.openWith      = (m_chkOpenWith.GetCheck()      == BST_CHECKED);
    o.startup       = (m_chkStartup.GetCheck()       == BST_CHECKED);
    o.soundEvents   = (m_chkSoundEvents.GetCheck()   == BST_CHECKED);
    o.services      = (m_chkServices.GetCheck()      == BST_CHECKED);
    return o;
}

void CRegistryScannerDialog::OnBtnScan()
{
    if (m_scanning) return;

    ScanOptions opts = BuildScanOptions();
    bool anyChecked = opts.appPaths || opts.browserHelper || opts.fileExt || opts.firewall
                   || opts.fonts || opts.helpFiles || opts.installers || opts.interfaceCom
                   || opts.muiCache || opts.sharedDlls || opts.uninstall || opts.openWith
                   || opts.startup || opts.soundEvents || opts.services;
    if (!anyChecked)
    {
        AfxMessageBox(_T("Please select at least one category to scan."), MB_ICONWARNING);
        return;
    }

    SetScanInProgress(true);
    m_list.DeleteAllItems();
    m_progress.SetPos(0);
    SetStatus(_T("Scanning..."));

    auto* p = new ScanParams{ this, opts };
    AfxBeginThread(ScanThreadProc, p);
}

LRESULT CRegistryScannerDialog::OnScanProgress(WPARAM wParam, LPARAM lParam)
{
    int pct = (int)wParam;
    CString* pPhase = reinterpret_cast<CString*>(lParam);
    m_progress.SetPos(pct);
    if (pPhase)
    {
        m_staticProgress.SetWindowText(*pPhase);
        delete pPhase;
    }
    return 0;
}

LRESULT CRegistryScannerDialog::OnScanComplete(WPARAM wParam, LPARAM /*lParam*/)
{
    int count = (int)wParam;
    m_progress.SetPos(100);
    m_staticProgress.SetWindowText(_T("Done"));
    SetScanInProgress(false);
    PopulateList();
    CString msg;
    msg.Format(_T("Scan complete. %d issue%s found."), count, count == 1 ? _T("") : _T("s"));
    SetStatus(msg);
    return 0;
}

// ---- List population ----

void CRegistryScannerDialog::PopulateList()
{
    m_list.DeleteAllItems();
    auto& issues = m_scanner.GetIssues();
    int row = 0;
    for (int i = 0; i < (int)issues.size(); ++i)
    {
        const auto& e = issues[i];
        if (e.fixed || e.ignored) continue;

        // Show value name or (for Uninstall/AppPath subkeys) the last component of keyPath
        CString nameCol;
        if (!e.valueName.IsEmpty())
        {
            nameCol = e.valueName;
            if (nameCol.GetLength() > 60)
                nameCol = nameCol.Left(57) + _T("...");
        }
        else
        {
            int sep = e.keyPath.ReverseFind(_T('\\'));
            nameCol = (sep >= 0) ? e.keyPath.Mid(sep + 1) : e.keyPath;
        }

        m_list.InsertItem(row, e.typeLabel);
        m_list.SetItemText(row, 1, nameCol);
        m_list.SetItemText(row, 2, e.description);
        m_list.SetItemText(row, 3, e.hiveLabel);
        m_list.SetItemText(row, 4, _T("Pending"));
        m_list.SetItemData(row, (DWORD_PTR)i);  // store original index
        ++row;
    }
    UpdateButtonState();
}

// ---- Button handlers ----

void CRegistryScannerDialog::SetStatus(const CString& msg)
{
    if (m_staticStatus.GetSafeHwnd()) m_staticStatus.SetWindowText(msg);
}

void CRegistryScannerDialog::SetScanInProgress(bool inProgress)
{
    m_scanning = inProgress;
    m_btnScan.EnableWindow(!inProgress);
    m_chkAppPath.EnableWindow(!inProgress);
    m_chkBrowserHelper.EnableWindow(!inProgress);
    m_chkFileExt.EnableWindow(!inProgress);
    m_chkFirewall.EnableWindow(!inProgress);
    m_chkFonts.EnableWindow(!inProgress);
    m_chkHelpFiles.EnableWindow(!inProgress);
    m_chkInstallers.EnableWindow(!inProgress);
    m_chkInterface.EnableWindow(!inProgress);
    m_chkMuiCache.EnableWindow(!inProgress);
    m_chkSharedDll.EnableWindow(!inProgress);
    m_chkUninstall.EnableWindow(!inProgress);
    m_chkOpenWith.EnableWindow(!inProgress);
    m_chkStartup.EnableWindow(!inProgress);
    m_chkSoundEvents.EnableWindow(!inProgress);
    m_chkServices.EnableWindow(!inProgress);
    m_btnClose.EnableWindow(!inProgress);
    UpdateButtonState();
}

void CRegistryScannerDialog::UpdateButtonState()
{
    bool hasIssues = false;
    auto& issues = m_scanner.GetIssues();
    for (const auto& e : issues)
        if (!e.fixed && !e.ignored) { hasIssues = true; break; }

    bool hasSel = (m_list.GetNextItem(-1, LVNI_SELECTED) >= 0);

    m_btnFixSel.EnableWindow(!m_scanning && hasSel);
    m_btnFixAll.EnableWindow(!m_scanning && hasIssues);
    m_btnBackupFix.EnableWindow(!m_scanning && hasIssues);
    m_btnIgnore.EnableWindow(!m_scanning && hasSel);
}

void CRegistryScannerDialog::OnListItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    UpdateButtonState();
    *pResult = 0;
}

// Collect original issue indices for all selected (or checked) list rows
static std::vector<int> GetSelectedIndices(CListCtrl& list)
{
    std::vector<int> out;
    int row = -1;
    while ((row = list.GetNextItem(row, LVNI_SELECTED)) >= 0)
        out.push_back((int)list.GetItemData(row));
    return out;
}

static std::vector<int> GetAllPendingIndices(CListCtrl& list)
{
    std::vector<int> out;
    for (int row = 0; row < list.GetItemCount(); ++row)
        out.push_back((int)list.GetItemData(row));
    return out;
}

bool CRegistryScannerDialog::DoFixIssues(const std::vector<int>& indices)
{
    int fixed = 0, failed = 0;
    auto& issues = m_scanner.GetIssues();
    for (int idx : indices)
    {
        if (idx < 0 || idx >= (int)issues.size()) continue;
        if (issues[idx].fixed || issues[idx].ignored) continue;
        if (CRegistryScanner::FixIssue(issues[idx])) ++fixed;
        else ++failed;
    }
    PopulateList();
    CString msg;
    if (failed == 0)
        msg.Format(_T("%d issue%s fixed successfully."), fixed, fixed == 1 ? _T("") : _T("s"));
    else
        msg.Format(_T("%d fixed, %d failed (check you are running as Administrator)."), fixed, failed);
    SetStatus(msg);
    return (failed == 0);
}

void CRegistryScannerDialog::OnBtnFixSelected()
{
    auto indices = GetSelectedIndices(m_list);
    if (indices.empty()) return;

    CString msg;
    msg.Format(_T("Fix %d selected issue%s?\r\nThis will delete the registry entries listed."),
               (int)indices.size(), indices.size() == 1 ? _T("") : _T("s"));
    if (AfxMessageBox(msg, MB_ICONQUESTION|MB_YESNO) != IDYES) return;
    DoFixIssues(indices);
}

void CRegistryScannerDialog::OnBtnFixAll()
{
    auto indices = GetAllPendingIndices(m_list);
    if (indices.empty()) return;

    CString msg;
    msg.Format(_T("Fix all %d pending issue%s?\r\nThis will delete the registry entries listed."),
               (int)indices.size(), indices.size() == 1 ? _T("") : _T("s"));
    if (AfxMessageBox(msg, MB_ICONQUESTION|MB_YESNO) != IDYES) return;
    DoFixIssues(indices);
}

void CRegistryScannerDialog::OnBtnBackupAndFix()
{
    auto indices = GetAllPendingIndices(m_list);
    if (indices.empty()) return;

    // Prompt for save path
    CFileDialog dlg(FALSE, _T("reg"), _T("RegistryBackup"),
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        _T("Registry files (*.reg)|*.reg|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;

    CString path = dlg.GetPathName();
    if (!m_scanner.BackupIssuesToFile(path, indices))
    {
        AfxMessageBox(_T("Failed to write backup file. Aborting — no changes made."), MB_ICONERROR);
        return;
    }

    CString msg;
    msg.Format(_T("Backup saved to:\r\n%s\r\n\r\nFix all %d issue%s now?"),
               path.GetString(), (int)indices.size(), indices.size() == 1 ? _T("") : _T("s"));
    if (AfxMessageBox(msg, MB_ICONQUESTION|MB_YESNO) != IDYES) return;
    DoFixIssues(indices);
}

void CRegistryScannerDialog::OnBtnIgnore()
{
    auto indices = GetSelectedIndices(m_list);
    auto& issues = m_scanner.GetIssues();
    for (int idx : indices)
        if (idx >= 0 && idx < (int)issues.size())
            issues[idx].ignored = true;
    PopulateList();
    SetStatus(_T("Selected issues marked as ignored."));
}

void CRegistryScannerDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}
