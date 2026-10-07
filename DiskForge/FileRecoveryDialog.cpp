// FileRecoveryDialog.cpp
#include "pch.h"
#include "FileRecoveryDialog.h"
#include "PartitionDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CFileRecoveryDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
    ON_CBN_SELCHANGE(IDC_FR_COMBO_DRIVE, &CFileRecoveryDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_FR_BTN_CHOOSE_PART, &CFileRecoveryDialog::OnBtnChoosePartition)
    ON_BN_CLICKED(IDC_FR_BTN_OPEN, &CFileRecoveryDialog::OnBtnOpenVolume)
    ON_NOTIFY(TVN_ITEMEXPANDING, IDC_FR_TREE, &CFileRecoveryDialog::OnTreeItemExpanding)
    ON_NOTIFY(TVN_SELCHANGED, IDC_FR_TREE, &CFileRecoveryDialog::OnTreeSelChanged)
    ON_BN_CLICKED(IDC_FR_BTN_RECOVER, &CFileRecoveryDialog::OnBtnRecover)
    ON_BN_CLICKED(IDC_FR_BTN_CLOSE, &CFileRecoveryDialog::OnBtnClose)
END_MESSAGE_MAP()

CFileRecoveryDialog::CFileRecoveryDialog(CWnd* pParent)
    : CDialogEx(IDD_FILERECOVERY, pParent)
    , m_hasChosenPartition(false)
    , m_volumeOpen(false)
{
}

CFileRecoveryDialog::~CFileRecoveryDialog()
{
    if (m_dm.IsOpen())
        m_dm.CloseDrive();
}

void CFileRecoveryDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_FR_COMBO_DRIVE,    m_comboDrive);
    DDX_Control(pDX, IDC_FR_BTN_CHOOSE_PART,m_btnChoosePart);
    DDX_Control(pDX, IDC_FR_STATIC_PART,    m_staticPart);
    DDX_Control(pDX, IDC_FR_EDIT_CUSTOM_LBA,m_editCustomLba);
    DDX_Control(pDX, IDC_FR_BTN_OPEN,       m_btnOpen);
    DDX_Control(pDX, IDC_FR_TREE,           m_tree);
    DDX_Control(pDX, IDC_FR_STATIC_INFO,    m_staticInfo);
    DDX_Control(pDX, IDC_FR_BTN_RECOVER,    m_btnRecover);
    DDX_Control(pDX, IDC_FR_STATIC_STATUS,  m_staticStatus);
    DDX_Control(pDX, IDC_FR_BTN_CLOSE,      m_btnClose);
}

BOOL CFileRecoveryDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Recover Deleted Files (FAT32 only, read-only against the source disk)"));
    CenterWindow(GetParent());

    m_btnRecover.EnableWindow(FALSE);

    CDiskManager tmp;
    tmp.EnumerateDrives(m_drives);
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);
    if (m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    return TRUE;
}

void CFileRecoveryDialog::OnComboDriveChange()
{
    m_hasChosenPartition = false;
    UpdateChosenPartitionLabel();
}

void CFileRecoveryDialog::UpdateChosenPartitionLabel()
{
    if (m_hasChosenPartition)
    {
        CString s;
        s.Format(_T("Partition #%d: %s, start LBA %llu"), m_chosenPartition.index,
                  m_chosenPartition.typeName.GetString(), m_chosenPartition.startLBA);
        m_staticPart.SetWindowText(s);
    }
    else
    {
        m_staticPart.SetWindowText(_T("No partition chosen - or type a start LBA below (e.g. from Lost Partition Search)."));
    }
}

void CFileRecoveryDialog::OnBtnChoosePartition()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a drive first."), MB_ICONWARNING);
        return;
    }

    CDiskManager dm;
    if (!dm.OpenDrive(m_drives[sel].devicePath))
    {
        AfxMessageBox(_T("Could not open the drive to read its partition table:\r\n") + dm.GetLastError(), MB_ICONERROR);
        return;
    }

    CPartitionDialog dlg(dm, this);
    if (dlg.DoModal() == IDOK)
    {
        PartitionEntry pe;
        if (dlg.GetSelectedPartitionEntry(pe))
        {
            m_chosenPartition = pe;
            m_hasChosenPartition = true;
            m_editCustomLba.SetWindowText(_T(""));
        }
    }
    dm.CloseDrive();

    UpdateChosenPartitionLabel();
}

void CFileRecoveryDialog::OnBtnOpenVolume()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a drive first."), MB_ICONWARNING);
        return;
    }
    const DriveInfo& drive = m_drives[sel];

    ULONGLONG startLBA = 0;
    CString lbaText;
    m_editCustomLba.GetWindowText(lbaText);
    lbaText.TrimLeft(); lbaText.TrimRight();

    if (!lbaText.IsEmpty())
    {
        startLBA = _tcstoui64(lbaText, nullptr, 10);
    }
    else if (m_hasChosenPartition)
    {
        startLBA = m_chosenPartition.startLBA;
    }
    else
    {
        AfxMessageBox(_T("Choose a partition, or type a start LBA."), MB_ICONWARNING);
        return;
    }

    if (m_dm.IsOpen())
        m_dm.CloseDrive();
    m_volumeOpen = false;
    m_tree.DeleteAllItems();
    m_btnRecover.EnableWindow(FALSE);

    if (!m_dm.OpenDrive(drive.devicePath))
    {
        AfxMessageBox(_T("Could not open the drive:\r\n") + m_dm.GetLastError(), MB_ICONERROR);
        return;
    }

    m_volume = CFat32Reader::OpenVolume(m_dm, startLBA, drive.bytesPerSector);
    if (!m_volume.valid)
    {
        AfxMessageBox(_T("Could not open a FAT32 volume there:\r\n") + m_volume.errorMessage, MB_ICONERROR);
        m_dm.CloseDrive();
        m_staticStatus.SetWindowText(_T("Not a FAT32 volume at that location."));
        return;
    }

    m_volumeOpen = true;

    CString status;
    status.Format(_T("FAT32 volume opened (label: \"%s\"). Browse the tree below - deleted entries are marked."),
                   m_volume.volumeLabel.IsEmpty() ? _T("(none)") : m_volume.volumeLabel.GetString());
    m_staticStatus.SetWindowText(status);

    PopulateChildren(TVI_ROOT, m_volume.rootCluster);
}

void CFileRecoveryDialog::PopulateChildren(HTREEITEM parent, DWORD dirCluster)
{
    std::vector<FatDirEntry> entries = CFat32Reader::ReadDirectory(m_dm, m_volume, dirCluster);

    for (const auto& e : entries)
    {
        CString text = e.name;
        if (e.isDeleted)
            text = _T("[DELETED] ") + text;
        if (!e.isDirectory)
        {
            CString sizeStr;
            sizeStr.Format(_T(" (%s)"), CPartitionParser::FormatSize(e.fileSize).GetString());
            text += sizeStr;
        }

        HTREEITEM item = m_tree.InsertItem(text, parent, TVI_LAST);

        FrTreeItemData* data = new FrTreeItemData();
        data->isDirectory = e.isDirectory;
        data->isDeleted = e.isDeleted;
        data->firstCluster = e.firstCluster;
        data->fileSize = e.fileSize;
        data->childrenLoaded = false;
        m_tree.SetItemData(item, (DWORD_PTR)data);

        if (e.isDirectory && e.firstCluster >= 2)
        {
            // Insert a placeholder child so the "+" expand box shows; the
            // real children are loaded lazily on first expand.
            m_tree.InsertItem(_T("Loading..."), item, TVI_LAST);
        }
    }
}

void CFileRecoveryDialog::OnTreeItemExpanding(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTREEVIEW pNM = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    *pResult = 0;

    if (pNM->action != TVE_EXPAND)
        return;

    HTREEITEM item = pNM->itemNew.hItem;
    FrTreeItemData* data = reinterpret_cast<FrTreeItemData*>(m_tree.GetItemData(item));
    if (!data || data->childrenLoaded || !data->isDirectory)
        return;

    // Remove the "Loading..." placeholder.
    HTREEITEM child = m_tree.GetChildItem(item);
    while (child)
    {
        HTREEITEM next = m_tree.GetNextSiblingItem(child);
        m_tree.DeleteItem(child);
        child = next;
    }

    data->childrenLoaded = true;
    PopulateChildren(item, data->firstCluster);
}

void CFileRecoveryDialog::OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTREEVIEW pNM = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    *pResult = 0;

    FrTreeItemData* data = reinterpret_cast<FrTreeItemData*>(m_tree.GetItemData(pNM->itemNew.hItem));
    if (!data)
    {
        m_btnRecover.EnableWindow(FALSE);
        m_staticInfo.SetWindowText(_T(""));
        return;
    }

    if (data->isDirectory)
    {
        m_btnRecover.EnableWindow(FALSE);
        m_staticInfo.SetWindowText(_T("Folder selected - recovering a whole folder isn't supported yet; open it and recover files individually."));
        return;
    }

    CString info;
    info.Format(_T("%s file, %s, first cluster %u.%s"),
                 data->isDeleted ? _T("Deleted") : _T("Live"),
                 CPartitionParser::FormatSize(data->fileSize).GetString(),
                 data->firstCluster,
                 data->isDeleted ? _T(" Recovery assumes contiguous clusters (see README) - verify the result.") : _T(""));
    m_staticInfo.SetWindowText(info);
    m_btnRecover.EnableWindow(data->firstCluster >= 2);
}

void CFileRecoveryDialog::OnBtnRecover()
{
    HTREEITEM sel = m_tree.GetSelectedItem();
    if (!sel)
        return;
    FrTreeItemData* data = reinterpret_cast<FrTreeItemData*>(m_tree.GetItemData(sel));
    if (!data || data->isDirectory)
        return;

    CString name = m_tree.GetItemText(sel);
    // Strip the "[DELETED] " prefix and " (size)" suffix we added for display.
    if (name.Left(10) == _T("[DELETED] "))
        name = name.Mid(10);
    int parenPos = name.ReverseFind(_T('('));
    if (parenPos > 0)
        name = name.Left(parenPos).TrimRight();

    CFileDialog dlg(FALSE, nullptr, name, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        _T("All files (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
        return;

    ULONGLONG bytesWritten = 0;
    CString err = CFat32Reader::RecoverFile(
        m_dm, m_volume, data->firstCluster, data->fileSize,
        /*assumeContiguous=*/data->isDeleted, dlg.GetPathName(), bytesWritten);

    CString resultMsg;
    resultMsg.Format(_T("Recovered %s of %s expected."),
                      CPartitionParser::FormatSize(bytesWritten).GetString(),
                      CPartitionParser::FormatSize(data->fileSize).GetString());

    if (err.IsEmpty())
        AfxMessageBox(resultMsg, MB_ICONINFORMATION);
    else
        AfxMessageBox(resultMsg + _T("\r\n\r\n") + err, MB_ICONWARNING);
}

void CFileRecoveryDialog::CleanupTreeItemData(HTREEITEM item)
{
    while (item)
    {
        HTREEITEM child = m_tree.GetChildItem(item);
        if (child)
            CleanupTreeItemData(child);

        FrTreeItemData* data = reinterpret_cast<FrTreeItemData*>(m_tree.GetItemData(item));
        delete data;

        item = m_tree.GetNextSiblingItem(item);
    }
}

void CFileRecoveryDialog::OnDestroy()
{
    if (m_tree.GetSafeHwnd())
        CleanupTreeItemData(m_tree.GetRootItem());
    CDialogEx::OnDestroy();
}

void CFileRecoveryDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}

void CFileRecoveryDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_tree.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    CRect rc, rcBtn;

    // Drive combo stretches; Choose Part is right-anchored — both keep template y/height.
    // editCustomLba and btnOpen stay at their template (DPI-correct) positions.
    m_comboDrive.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnChoosePart.GetWindowRect(&rcBtn); ScreenToClient(&rcBtn);
    m_comboDrive.MoveWindow(rc.left, rc.top, R - rcBtn.Width() - 8 - rc.left, rc.Height(), TRUE);
    m_btnChoosePart.MoveWindow(R - rcBtn.Width(), rcBtn.top, rcBtn.Width(), rcBtn.Height(), TRUE);

    // Part label: full width, template y.
    m_staticPart.GetWindowRect(&rc); ScreenToClient(&rc);
    m_staticPart.MoveWindow(L, rc.top, R - L, rc.Height(), TRUE);

    // Read template heights for the bottom three rows.
    CRect rBh(0, 0, 0, 17); MapDialogRect(&rBh);
    const int bh = rBh.bottom;
    m_staticInfo.GetWindowRect(&rc);  ScreenToClient(&rc);
    const int infoH = rc.Height();
    m_btnRecover.GetWindowRect(&rc);  ScreenToClient(&rc);
    const int recW = rc.Width();
    m_btnClose.GetWindowRect(&rc);    ScreenToClient(&rc);
    const int closeW = rc.Width();
    m_staticStatus.GetWindowRect(&rc); ScreenToClient(&rc);
    const int statusX = L + recW + 8;

    // Bottom rows (from bottom up): close, recover+status, info bar.
    const int closeBy   = cy - bh - 6;
    const int recoverBy = closeBy - bh - 4;
    const int infoBy    = recoverBy - infoH - 4;

    m_staticInfo.MoveWindow(L, infoBy, R - L, infoH, TRUE);
    m_btnRecover.MoveWindow(L, recoverBy, recW, bh, TRUE);
    m_staticStatus.GetWindowRect(&rc); ScreenToClient(&rc);
    m_staticStatus.MoveWindow(statusX, recoverBy, R - statusX, rc.Height(), TRUE);
    m_btnClose.MoveWindow(R - closeW, closeBy, closeW, bh, TRUE);

    // Tree fills from its template top to above the info bar.
    m_tree.GetWindowRect(&rc); ScreenToClient(&rc);
    int treeH = infoBy - rc.top - 8;
    if (treeH < 40) treeH = 40;
    m_tree.MoveWindow(L, rc.top, R - L, treeH, TRUE);
}

void CFileRecoveryDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
