// MbrGptConvertDialog.cpp
#include "pch.h"
#include "MbrGptConvertDialog.h"
#include "ConfirmTextDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    CString ShortDriveName(const CString& devicePath)
    {
        int pos = devicePath.ReverseFind(_T('\\'));
        return (pos >= 0) ? devicePath.Mid(pos + 1) : devicePath;
    }
}

BEGIN_MESSAGE_MAP(CMbrGptConvertDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_CBN_SELCHANGE(IDC_MG_COMBO_DRIVE, &CMbrGptConvertDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_MG_BTN_ANALYZE,     &CMbrGptConvertDialog::OnBtnAnalyze)
    ON_BN_CLICKED(IDC_MG_BTN_SAVE_BACKUP, &CMbrGptConvertDialog::OnBtnSaveBackup)
    ON_BN_CLICKED(IDC_MG_BTN_CONVERT,     &CMbrGptConvertDialog::OnBtnConvert)
    ON_BN_CLICKED(IDC_MG_BTN_RESTORE,     &CMbrGptConvertDialog::OnBtnRestore)
    ON_BN_CLICKED(IDC_MG_BTN_CLOSE,       &CMbrGptConvertDialog::OnBtnClose)
END_MESSAGE_MAP()

CMbrGptConvertDialog::CMbrGptConvertDialog(CWnd* pParent)
    : CDialogEx(IDD_MBRGPTCONVERT, pParent)
    , m_backupSaved(false)
{
}

void CMbrGptConvertDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MG_COMBO_DRIVE,       m_comboDrive);
    DDX_Control(pDX, IDC_MG_BTN_ANALYZE,       m_btnAnalyze);
    DDX_Control(pDX, IDC_MG_LIST_PARTITIONS,   m_listPartitions);
    DDX_Control(pDX, IDC_MG_STATIC_RESULT,     m_staticResult);
    DDX_Control(pDX, IDC_MG_BTN_SAVE_BACKUP,   m_btnSaveBackup);
    DDX_Control(pDX, IDC_MG_STATIC_BACKUP,     m_staticBackup);
    DDX_Control(pDX, IDC_MG_BTN_CONVERT,       m_btnConvert);
    DDX_Control(pDX, IDC_MG_BTN_RESTORE,       m_btnRestore);
    DDX_Control(pDX, IDC_MG_BTN_CLOSE,         m_btnClose);
}

BOOL CMbrGptConvertDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Convert MBR to GPT"));
    CenterWindow(GetParent());

    m_listPartitions.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listPartitions.InsertColumn(0, _T("#"),                 LVCFMT_RIGHT, 30);
    m_listPartitions.InsertColumn(1, _T("Current MBR Type"), LVCFMT_LEFT,  160);
    m_listPartitions.InsertColumn(2, _T("Will Become (GPT)"),LVCFMT_LEFT,  200);
    m_listPartitions.InsertColumn(3, _T("Start LBA"),        LVCFMT_RIGHT, 100);
    m_listPartitions.InsertColumn(4, _T("Size"),             LVCFMT_RIGHT, 90);

    m_btnSaveBackup.EnableWindow(FALSE);
    m_btnConvert.EnableWindow(FALSE);

    CDiskManager dm;
    dm.EnumerateDrives(m_drives);
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);
    if (m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    return TRUE;
}

void CMbrGptConvertDialog::ResetAnalysisState()
{
    m_lastAnalysis = ConvertPreflightResult();
    m_backupSaved = false;
    m_backupFilePath.Empty();
    m_listPartitions.DeleteAllItems();
    m_staticResult.SetWindowText(_T("Click \"Analyze Disk\" to check this drive."));
    m_staticBackup.SetWindowText(_T("No backup saved yet."));
    m_btnSaveBackup.EnableWindow(FALSE);
    m_btnConvert.EnableWindow(FALSE);
}

void CMbrGptConvertDialog::OnComboDriveChange()
{
    ResetAnalysisState();
}

void CMbrGptConvertDialog::PopulatePartitionList()
{
    m_listPartitions.DeleteAllItems();
    for (size_t i = 0; i < m_lastAnalysis.mbrPartitions.size(); ++i)
    {
        const PartitionEntry& e = m_lastAnalysis.mbrPartitions[i];

        CString idx, willBecome, startStr, sizeStr;
        idx.Format(_T("%d"), e.index);

        // Mirrors CMbrGptConverter::MbrTypeToGptTypeGuid's mapping, for display purposes.
        switch (e.mbrType)
        {
            case 0xEF: willBecome = _T("EFI System Partition"); break;
            case 0x82: willBecome = _T("Linux swap"); break;
            case 0x83: willBecome = _T("Linux filesystem"); break;
            default:   willBecome = _T("Microsoft Basic Data"); break;
        }

        startStr.Format(_T("%llu"), e.startLBA);
        sizeStr = CPartitionParser::FormatSize(e.sizeBytes);

        int row = m_listPartitions.InsertItem((int)i, idx);
        m_listPartitions.SetItemText(row, 1, e.typeName);
        m_listPartitions.SetItemText(row, 2, willBecome);
        m_listPartitions.SetItemText(row, 3, startStr);
        m_listPartitions.SetItemText(row, 4, sizeStr);
    }
}

void CMbrGptConvertDialog::OnBtnAnalyze()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a drive first."), MB_ICONWARNING);
        return;
    }
    const DriveInfo& drive = m_drives[sel];

    ResetAnalysisState();

    CDiskManager dm;
    if (!dm.OpenDrive(drive.devicePath))
    {
        m_staticResult.SetWindowText(_T("Could not open the drive: ") + dm.GetLastError());
        return;
    }

    m_lastAnalysis = CMbrGptConverter::Analyze(dm);
    m_lastAnalysis.driveIndex = drive.driveIndex;
    m_lastAnalysis.devicePath = drive.devicePath;
    dm.CloseDrive();

    if (!m_lastAnalysis.canConvert)
    {
        m_staticResult.SetWindowText(_T("\u26A0 Cannot convert this disk: ") + m_lastAnalysis.blockingReason);
        return;
    }

    PopulatePartitionList();

    CString msg;
    msg.Format(_T("This disk CAN be converted: %d partition(s) found, and there is enough free space at both ")
               _T("ends of the disk for the new GPT structures. No partition will be moved, resized, or have ")
               _T("its data touched - only the partition table itself changes.\r\n")
               _T("Next: click \"Save Backup...\" (required before Convert is enabled)."),
               (int)m_lastAnalysis.mbrPartitions.size());
    m_staticResult.SetWindowText(msg);
    m_btnSaveBackup.EnableWindow(TRUE);
}

void CMbrGptConvertDialog::OnBtnSaveBackup()
{
    if (!m_lastAnalysis.canConvert)
    {
        AfxMessageBox(_T("Analyze a convertible disk first."), MB_ICONWARNING);
        return;
    }

    CDiskManager dm;
    if (!dm.OpenDrive(m_lastAnalysis.devicePath))
    {
        AfxMessageBox(_T("Could not reopen the drive to capture a backup:\r\n") + dm.GetLastError(), MB_ICONERROR);
        return;
    }
    std::vector<BackupRegion> regions = CMbrGptConverter::CaptureAffectedRegions(dm, m_lastAnalysis);
    dm.CloseDrive();

    if (regions.empty())
    {
        AfxMessageBox(_T("Could not read the regions that need to be backed up. Aborting - Convert will stay disabled."), MB_ICONERROR);
        return;
    }

    CString defaultName;
    defaultName.Format(_T("gpt_convert_backup_%s.bak"), ShortDriveName(m_lastAnalysis.devicePath).GetString());

    CFileDialog dlg(FALSE, _T("bak"), defaultName,
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        _T("Partition table backups (*.bak)|*.bak|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
        return;

    CString err;
    if (!CMbrGptConverter::SaveBackup(dlg.GetPathName(), m_lastAnalysis.devicePath, m_lastAnalysis.sectorSize, regions, err))
    {
        AfxMessageBox(_T("Failed to save the backup:\r\n") + err + _T("\r\n\r\nConvert will stay disabled."), MB_ICONERROR);
        return;
    }

    m_backupSaved = true;
    m_backupFilePath = dlg.GetPathName();
    m_staticBackup.SetWindowText(_T("Backup saved: ") + m_backupFilePath);
    m_btnConvert.EnableWindow(TRUE);

    AfxMessageBox(
        _T("Backup saved. Keep this file safe - if anything goes wrong, use \"Restore From Backup...\" ")
        _T("with this exact file to put the original partition table back."),
        MB_ICONINFORMATION);
}

void CMbrGptConvertDialog::OnBtnConvert()
{
    if (!m_lastAnalysis.canConvert || !m_backupSaved)
    {
        AfxMessageBox(_T("Analyze the disk and save a backup first."), MB_ICONWARNING);
        return;
    }

    CString driveName = ShortDriveName(m_lastAnalysis.devicePath);

    CString message;
    message.Format(
        _T("You are about to convert %s from MBR to GPT.\r\n\r\n")
        _T("%d partition(s) will be preserved in place - their data is NOT touched - but the partition ")
        _T("table itself will be rewritten. A backup was saved to:\r\n%s\r\n\r\n")
        _T("Any operating system currently installed on this disk that boots via legacy BIOS/MBR will ")
        _T("very likely NOT boot afterward without also reconfiguring boot loading for UEFI/GPT - that is ")
        _T("outside what this tool does.\r\n\r\nThis writes directly to the disk now."),
        driveName.GetString(), (int)m_lastAnalysis.mbrPartitions.size(), m_backupFilePath.GetString());

    CString requiredPhrase = _T("CONVERT ") + driveName;
    CConfirmTextDialog confirmDlg(message, requiredPhrase, this);
    if (confirmDlg.DoModal() != IDOK)
        return;

    m_staticResult.SetWindowText(_T("Converting..."));
    UpdateWindow();

    CString err = CMbrGptConverter::ConvertMbrToGpt(m_lastAnalysis.devicePath, m_lastAnalysis);

    if (err.IsEmpty())
    {
        m_staticResult.SetWindowText(_T("Conversion complete. The disk now uses GPT."));
        AfxMessageBox(_T("Conversion complete. If Windows doesn't immediately show the new layout, ")
                      _T("re-open Disk Management or restart."), MB_ICONINFORMATION);
        m_btnConvert.EnableWindow(FALSE); // Done - re-analyze if further action is needed
    }
    else
    {
        m_staticResult.SetWindowText(_T("\u26A0 Conversion failed: ") + err +
            _T("\r\nUse \"Restore From Backup...\" with the saved backup file if the disk is now in an inconsistent state."));
        AfxMessageBox(_T("Conversion failed:\r\n") + err +
            _T("\r\n\r\nIf you're unsure of the disk's state, use \"Restore From Backup...\" now with the backup file saved earlier."),
            MB_ICONERROR);
    }
}

void CMbrGptConvertDialog::OnBtnRestore()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select the drive to restore onto first."), MB_ICONWARNING);
        return;
    }
    const DriveInfo& drive = m_drives[sel];

    CFileDialog dlg(TRUE, _T("bak"), nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        _T("Partition table backups (*.bak)|*.bak|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
        return;

    CString message;
    message.Format(
        _T("This will write the sectors stored in:\r\n%s\r\nback onto %s, at their original locations.\r\n\r\n")
        _T("Only use this to undo a conversion attempt on this exact drive. This writes directly to the disk now."),
        dlg.GetPathName().GetString(), ShortDriveName(drive.devicePath).GetString());

    CString requiredPhrase = _T("RESTORE ") + ShortDriveName(drive.devicePath);
    CConfirmTextDialog confirmDlg(message, requiredPhrase, this);
    if (confirmDlg.DoModal() != IDOK)
        return;

    CString err;
    bool ok = CMbrGptConverter::LoadBackupAndRestore(dlg.GetPathName(), drive.devicePath, err);

    if (ok)
    {
        AfxMessageBox(_T("Restore complete."), MB_ICONINFORMATION);
        ResetAnalysisState();
    }
    else
    {
        AfxMessageBox(_T("Restore failed:\r\n") + err, MB_ICONERROR);
    }
}

void CMbrGptConvertDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}

void CMbrGptConvertDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_listPartitions.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    CRect rc, rcBtn;

    // Drive combo stretches; Analyze button is right-anchored — both use template y/height.
    m_comboDrive.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnAnalyze.GetWindowRect(&rcBtn); ScreenToClient(&rcBtn);
    m_comboDrive.MoveWindow(rc.left, rc.top, R - rcBtn.Width() - 8 - rc.left, rc.Height(), TRUE);
    m_btnAnalyze.MoveWindow(R - rcBtn.Width(), rcBtn.top, rcBtn.Width(), rcBtn.Height(), TRUE);

    // Partition list and result area: full width, template y/height (fixed-height in this dialog).
    // Save-backup, Convert, Restore buttons stay at their template positions.
    auto stretchFull = [&](CWnd& w) {
        w.GetWindowRect(&rc); ScreenToClient(&rc);
        w.MoveWindow(L, rc.top, R - L, rc.Height(), TRUE);
    };
    stretchFull(m_listPartitions);
    stretchFull(m_staticResult);

    // Backup path label: stretches from its template left edge to the right margin.
    m_staticBackup.GetWindowRect(&rc); ScreenToClient(&rc);
    m_staticBackup.MoveWindow(rc.left, rc.top, R - rc.left, rc.Height(), TRUE);

    // Close: bottom-right anchored.
    CRect rBh(0, 0, 0, 17); MapDialogRect(&rBh);
    const int bh = rBh.bottom;
    m_btnClose.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnClose.MoveWindow(R - rc.Width(), cy - bh - 6, rc.Width(), bh, TRUE);
}

void CMbrGptConvertDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
