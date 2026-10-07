// StartupManagerDialog.cpp - Windows startup registry manager
#include "pch.h"
#include "StartupManagerDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Registry paths used throughout
static LPCTSTR kRunKey     = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
static LPCTSTR kRunOnceKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce");
static LPCTSTR kApprovedRunHkcu = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run");
static LPCTSTR kApprovedRunHklm = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run");

// ============================================================
//  CAddStartupEntryDialog
// ============================================================

BEGIN_MESSAGE_MAP(CAddStartupEntryDialog, CDialogEx)
    ON_BN_CLICKED(IDC_AS_BTN_BROWSE,  &CAddStartupEntryDialog::OnBtnBrowse)
    ON_BN_CLICKED(IDC_AS_BTN_ADD,     &CAddStartupEntryDialog::OnBtnAdd)
    ON_BN_CLICKED(IDC_AS_BTN_CANCEL,  &CAddStartupEntryDialog::OnBtnCancel)
END_MESSAGE_MAP()

CAddStartupEntryDialog::CAddStartupEntryDialog(CWnd* pParent)
    : CDialogEx(IDD_ADDSTARTUPENTRY, pParent)
    , m_hive(HKEY_CURRENT_USER)
    , m_runOnce(false)
{}

void CAddStartupEntryDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_AS_STATIC_NAME, m_staticName);
    DDX_Control(pDX, IDC_AS_STATIC_CMD,  m_staticCmd);
    DDX_Control(pDX, IDC_AS_EDIT_NAME,   m_editName);
    DDX_Control(pDX, IDC_AS_EDIT_CMD,    m_editCmd);
    DDX_Control(pDX, IDC_AS_BTN_BROWSE,  m_btnBrowse);
    DDX_Control(pDX, IDC_AS_RADIO_HKCU,  m_radioHkcu);
    DDX_Control(pDX, IDC_AS_RADIO_HKLM,  m_radioHklm);
    DDX_Control(pDX, IDC_AS_CHK_RUNONCE, m_chkRunOnce);
    DDX_Control(pDX, IDC_AS_BTN_ADD,     m_btnAdd);
    DDX_Control(pDX, IDC_AS_BTN_CANCEL,  m_btnCancel);
}

BOOL CAddStartupEntryDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_radioHkcu.SetCheck(BST_CHECKED);

    return TRUE;
}

void CAddStartupEntryDialog::OnBtnBrowse()
{
    CFileDialog dlg(TRUE, nullptr, nullptr,
        OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("Executable files (*.exe;*.bat;*.cmd)|*.exe;*.bat;*.cmd|All files (*.*)|*.*||"),
        this);
    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        if (path.Find(_T(' ')) >= 0)
            path = _T("\"") + path + _T("\"");
        m_editCmd.SetWindowText(path);
    }
}

void CAddStartupEntryDialog::OnBtnAdd()
{
    m_editName.GetWindowText(m_name);
    m_editCmd.GetWindowText(m_command);
    m_name.Trim();
    m_command.Trim();

    if (m_name.IsEmpty())
    {
        AfxMessageBox(_T("Please enter a name for this startup entry."), MB_ICONWARNING);
        m_editName.SetFocus();
        return;
    }
    if (m_command.IsEmpty())
    {
        AfxMessageBox(_T("Please enter a command or click Browse to select an executable."), MB_ICONWARNING);
        m_editCmd.SetFocus();
        return;
    }

    m_hive    = (m_radioHklm.GetCheck() == BST_CHECKED) ? HKEY_LOCAL_MACHINE
                                                         : HKEY_CURRENT_USER;
    m_runOnce = (m_chkRunOnce.GetCheck() == BST_CHECKED);
    EndDialog(IDOK);
}

void CAddStartupEntryDialog::OnBtnCancel()
{
    EndDialog(IDCANCEL);
}

// ============================================================
//  CStartupManagerDialog
// ============================================================

BEGIN_MESSAGE_MAP(CStartupManagerDialog, CDialogEx)
    ON_BN_CLICKED(IDC_SM_BTN_ADD,     &CStartupManagerDialog::OnBtnAdd)
    ON_BN_CLICKED(IDC_SM_BTN_DELETE,  &CStartupManagerDialog::OnBtnDelete)
    ON_BN_CLICKED(IDC_SM_BTN_TOGGLE,  &CStartupManagerDialog::OnBtnToggle)
    ON_BN_CLICKED(IDC_SM_BTN_REFRESH, &CStartupManagerDialog::OnBtnRefresh)
    ON_BN_CLICKED(IDC_SM_BTN_CLOSE,   &CStartupManagerDialog::OnBtnClose)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_SM_LIST, &CStartupManagerDialog::OnListItemChanged)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

CStartupManagerDialog::CStartupManagerDialog(CWnd* pParent)
    : CDialogEx(IDD_STARTUPMANAGER, pParent)
{}

void CStartupManagerDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SM_LIST,          m_list);
    DDX_Control(pDX, IDC_SM_BTN_ADD,       m_btnAdd);
    DDX_Control(pDX, IDC_SM_BTN_DELETE,    m_btnDelete);
    DDX_Control(pDX, IDC_SM_BTN_TOGGLE,    m_btnToggle);
    DDX_Control(pDX, IDC_SM_BTN_REFRESH,   m_btnRefresh);
    DDX_Control(pDX, IDC_SM_BTN_CLOSE,     m_btnClose);
    DDX_Control(pDX, IDC_SM_STATIC_STATUS, m_staticStatus);
}

BOOL CStartupManagerDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Startup Manager"));
    CenterWindow(GetParent());

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_LABELTIP);
    m_list.InsertColumn(0, _T("Name"),    LVCFMT_LEFT, 200);
    m_list.InsertColumn(1, _T("Command"), LVCFMT_LEFT, 280);
    m_list.InsertColumn(2, _T("Status"),  LVCFMT_LEFT,  80);
    m_list.InsertColumn(3, _T("Source"),  LVCFMT_LEFT,  60);
    m_list.InsertColumn(4, _T("Type"),    LVCFMT_LEFT,  72);

    LoadEntries();
    PopulateList();
    UpdateButtonState();
    return TRUE;
}

void CStartupManagerDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_W;
    lpMMI->ptMinTrackSize.y = MIN_H;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CStartupManagerDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0) LayoutControls(cx, cy);
}

void CStartupManagerDialog::LayoutControls(int cx, int cy)
{
    if (!m_list.GetSafeHwnd()) return;
    const int BH = 28, BY = cy - 46;
    m_list.MoveWindow(8, 8, cx-16, cy-60, TRUE);
    m_btnAdd.MoveWindow(8, BY, 100, BH, TRUE);
    m_btnDelete.MoveWindow(116, BY, 100, BH, TRUE);
    m_btnToggle.MoveWindow(224, BY, 130, BH, TRUE);
    m_btnRefresh.MoveWindow(362, BY, 100, BH, TRUE);
    m_btnClose.MoveWindow(cx-108, BY, 100, BH, TRUE);
    m_staticStatus.MoveWindow(8, cy-18, cx-16, 16, TRUE);
}

// ---- Registry helpers ----

void CStartupManagerDialog::LoadEntries()
{
    m_entries.clear();

    struct Src { HKEY hive; LPCTSTR subkey; bool runOnce; LPCTSTR label; };
    static const Src sources[] = {
        { HKEY_CURRENT_USER,  kRunKey,     false, _T("HKCU") },
        { HKEY_LOCAL_MACHINE, kRunKey,     false, _T("HKLM") },
        { HKEY_CURRENT_USER,  kRunOnceKey, true,  _T("HKCU") },
        { HKEY_LOCAL_MACHINE, kRunOnceKey, true,  _T("HKLM") },
    };

    for (const auto& src : sources)
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyEx(src.hive, src.subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        for (DWORD idx = 0; ; ++idx)
        {
            TCHAR  valueName[512] = {};
            BYTE   valueData[4096] = {};
            DWORD  nameLen = _countof(valueName);
            DWORD  dataLen = sizeof(valueData);
            DWORD  type    = 0;

            LONG r = RegEnumValue(hKey, idx, valueName, &nameLen,
                                  nullptr, &type, valueData, &dataLen);
            if (r == ERROR_NO_MORE_ITEMS) break;
            if (r != ERROR_SUCCESS) continue;
            if (type != REG_SZ && type != REG_EXPAND_SZ) continue;

            StartupEntry e;
            e.name      = valueName;
            e.command   = reinterpret_cast<LPCTSTR>(valueData);
            e.hive      = src.hive;
            e.isRunOnce = src.runOnce;
            e.hiveLabel = src.label;
            e.keyLabel  = src.runOnce ? _T("RunOnce") : _T("Run");
            e.enabled   = true;

            if (!src.runOnce)
            {
                bool en = true;
                GetStartupApprovedStatus(src.hive, e.name, en);
                e.enabled = en;
            }

            m_entries.push_back(std::move(e));
        }
        RegCloseKey(hKey);
    }
}

void CStartupManagerDialog::PopulateList()
{
    m_list.DeleteAllItems();
    for (int i = 0; i < (int)m_entries.size(); ++i)
    {
        const auto& e = m_entries[i];
        int row = m_list.InsertItem(i, e.name);
        m_list.SetItemText(row, 1, e.command);
        if (e.isRunOnce)
            m_list.SetItemText(row, 2, _T("—"));
        else
            m_list.SetItemText(row, 2, e.enabled ? _T("Enabled") : _T("Disabled"));
        m_list.SetItemText(row, 3, e.hiveLabel);
        m_list.SetItemText(row, 4, e.keyLabel);
    }
    CString msg;
    msg.Format(_T("%d startup entr%s found across HKCU/HKLM Run and RunOnce."),
               (int)m_entries.size(),
               m_entries.size() == 1 ? _T("y") : _T("ies"));
    SetStatus(msg);
}

void CStartupManagerDialog::SetStatus(const CString& msg)
{
    if (m_staticStatus.GetSafeHwnd())
        m_staticStatus.SetWindowText(msg);
}

int CStartupManagerDialog::GetSelectedIndex()
{
    return m_list.GetNextItem(-1, LVNI_SELECTED);
}

void CStartupManagerDialog::UpdateButtonState()
{
    int sel = GetSelectedIndex();
    bool hasSel = (sel >= 0 && sel < (int)m_entries.size());

    m_btnDelete.EnableWindow(hasSel);

    if (hasSel)
    {
        bool canToggle = !m_entries[sel].isRunOnce;
        m_btnToggle.EnableWindow(canToggle);
        if (canToggle)
            m_btnToggle.SetWindowText(m_entries[sel].enabled ? _T("Disable") : _T("Enable"));
        else
            m_btnToggle.SetWindowText(_T("Enable/Disable"));
    }
    else
    {
        m_btnToggle.EnableWindow(FALSE);
        m_btnToggle.SetWindowText(_T("Enable/Disable"));
    }
}

void CStartupManagerDialog::OnListItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    UpdateButtonState();
    *pResult = 0;
}

bool CStartupManagerDialog::GetStartupApprovedStatus(HKEY hive, const CString& name, bool& enabled)
{
    enabled = true;
    LPCTSTR approvedPath = (hive == HKEY_LOCAL_MACHINE) ? kApprovedRunHklm : kApprovedRunHkcu;

    HKEY hKey = nullptr;
    if (RegOpenKeyEx(hive, approvedPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    BYTE  data[12] = {};
    DWORD dataLen  = sizeof(data);
    DWORD type     = 0;
    LONG  r = RegQueryValueEx(hKey, name, nullptr, &type, data, &dataLen);
    RegCloseKey(hKey);

    if (r != ERROR_SUCCESS) return false;
    // First byte: 0x02 = enabled, 0x03 = disabled by user
    enabled = (data[0] == 0x02);
    return true;
}

bool CStartupManagerDialog::SetStartupApprovedStatus(HKEY hive, const CString& name, bool enable)
{
    LPCTSTR approvedPath = (hive == HKEY_LOCAL_MACHINE) ? kApprovedRunHklm : kApprovedRunHkcu;

    HKEY hKey = nullptr;
    LONG r = RegCreateKeyEx(hive, approvedPath, 0, nullptr, 0,
                            KEY_WRITE, nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS) return false;

    BYTE data[12] = {};
    data[0] = enable ? 0x02 : 0x03;
    r = RegSetValueEx(hKey, name, 0, REG_BINARY, data, sizeof(data));
    RegCloseKey(hKey);
    return (r == ERROR_SUCCESS);
}

bool CStartupManagerDialog::DeleteFromRegistry(const StartupEntry& e)
{
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyEx(e.hive,
                          e.isRunOnce ? kRunOnceKey : kRunKey,
                          0, KEY_WRITE, &hKey);
    if (r != ERROR_SUCCESS) return false;

    r = RegDeleteValue(hKey, e.name);
    RegCloseKey(hKey);

    if (!e.isRunOnce)
    {
        // Remove the StartupApproved entry too, so the status is clean if the
        // entry is re-added later.
        LPCTSTR approvedPath = (e.hive == HKEY_LOCAL_MACHINE) ? kApprovedRunHklm
                                                               : kApprovedRunHkcu;
        HKEY hApproved = nullptr;
        if (RegOpenKeyEx(e.hive, approvedPath, 0, KEY_WRITE, &hApproved) == ERROR_SUCCESS)
        {
            RegDeleteValue(hApproved, e.name);
            RegCloseKey(hApproved);
        }
    }

    return (r == ERROR_SUCCESS);
}

bool CStartupManagerDialog::AddToRegistry(const CString& name, const CString& command,
                                           HKEY hive, bool runOnce)
{
    HKEY hKey = nullptr;
    LONG r = RegCreateKeyEx(hive, runOnce ? kRunOnceKey : kRunKey,
                            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS) return false;

    r = RegSetValueEx(hKey, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.GetString()),
        (command.GetLength() + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);
    return (r == ERROR_SUCCESS);
}

// ---- Button handlers ----

void CStartupManagerDialog::OnBtnAdd()
{
    CAddStartupEntryDialog dlg(this);
    if (dlg.DoModal() != IDOK) return;

    // Warn on name collision
    for (const auto& e : m_entries)
    {
        if (e.hive == dlg.m_hive && e.isRunOnce == dlg.m_runOnce &&
            e.name.CompareNoCase(dlg.m_name) == 0)
        {
            CString msg;
            msg.Format(
                _T("An entry named \"%s\" already exists in %s\\%s.\r\nOverwrite it?"),
                dlg.m_name.GetString(),
                dlg.m_hive == HKEY_LOCAL_MACHINE ? _T("HKLM") : _T("HKCU"),
                dlg.m_runOnce ? _T("RunOnce") : _T("Run"));
            if (AfxMessageBox(msg, MB_ICONQUESTION|MB_YESNO) != IDYES) return;
            break;
        }
    }

    if (!AddToRegistry(dlg.m_name, dlg.m_command, dlg.m_hive, dlg.m_runOnce))
    {
        AfxMessageBox(
            _T("Failed to write to the registry.\r\n")
            _T("Make sure the application is running as Administrator."),
            MB_ICONERROR);
        return;
    }

    LoadEntries();
    PopulateList();
    SetStatus(_T("Entry added successfully."));
}

void CStartupManagerDialog::OnBtnDelete()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= (int)m_entries.size()) return;

    const StartupEntry& e = m_entries[sel];
    CString msg;
    msg.Format(_T("Delete startup entry \"%s\" from %s\\%s?\r\nThis cannot be undone."),
               e.name.GetString(), e.hiveLabel.GetString(), e.keyLabel.GetString());
    if (AfxMessageBox(msg, MB_ICONQUESTION|MB_YESNO) != IDYES) return;

    if (!DeleteFromRegistry(e))
    {
        AfxMessageBox(
            _T("Failed to delete the registry value.\r\n")
            _T("Make sure the application is running as Administrator."),
            MB_ICONERROR);
        return;
    }

    LoadEntries();
    PopulateList();
    UpdateButtonState();
    SetStatus(_T("Entry deleted."));
}

void CStartupManagerDialog::OnBtnToggle()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= (int)m_entries.size()) return;

    const StartupEntry& e = m_entries[sel];
    if (e.isRunOnce) return;

    bool newState = !e.enabled;
    if (!SetStartupApprovedStatus(e.hive, e.name, newState))
    {
        AfxMessageBox(_T("Failed to update the startup approval status in the registry."),
                      MB_ICONERROR);
        return;
    }

    LoadEntries();
    PopulateList();
    // Reselect the same row
    if (sel < m_list.GetItemCount())
    {
        m_list.SetItemState(sel, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
        m_list.EnsureVisible(sel, FALSE);
    }
    UpdateButtonState();
    SetStatus(newState ? _T("Entry enabled.") : _T("Entry disabled."));
}

void CStartupManagerDialog::OnBtnRefresh()
{
    int sel = GetSelectedIndex();
    LoadEntries();
    PopulateList();
    if (sel >= 0 && sel < m_list.GetItemCount())
    {
        m_list.SetItemState(sel, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
        m_list.EnsureVisible(sel, FALSE);
    }
    UpdateButtonState();
    SetStatus(_T("Startup entries refreshed."));
}

void CStartupManagerDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}
