// WipeDiskDialog.cpp
#include "pch.h"
#include "WipeDiskDialog.h"
#include "PartitionDialog.h"
#include "ConfirmTextDialog.h"
#include "SystemDiskGuard.h"

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

BEGIN_MESSAGE_MAP(CWipeDiskDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_WD_COMBO_DRIVE, &CWipeDiskDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_WD_RADIO_WHOLE, &CWipeDiskDialog::OnRadioMode)
    ON_BN_CLICKED(IDC_WD_RADIO_PART,  &CWipeDiskDialog::OnRadioMode)
    ON_BN_CLICKED(IDC_WD_BTN_CHOOSE_PART, &CWipeDiskDialog::OnBtnChoosePartition)
    ON_BN_CLICKED(IDC_WD_BTN_REFRESH, &CWipeDiskDialog::OnBtnRefresh)
    ON_BN_CLICKED(IDC_WD_BTN_START,   &CWipeDiskDialog::OnBtnStart)
    ON_BN_CLICKED(IDC_WD_BTN_CANCEL,  &CWipeDiskDialog::OnBtnCancelWipe)
    ON_BN_CLICKED(IDC_WD_BTN_CLOSE,   &CWipeDiskDialog::OnBtnClose)
END_MESSAGE_MAP()

CWipeDiskDialog::CWipeDiskDialog(CWnd* pParent)
    : CDialogEx(IDD_WIPEDISK, pParent)
    , m_hasChosenPartition(false)
    , m_wipeRunning(false)
    , m_pThread(nullptr)
{
}

CWipeDiskDialog::~CWipeDiskDialog()
{
    if (m_wipeRunning)
        m_engine.RequestCancel();
}

void CWipeDiskDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_WD_COMBO_DRIVE,    m_comboDrive);
    DDX_Control(pDX, IDC_WD_RADIO_WHOLE,    m_radioWhole);
    DDX_Control(pDX, IDC_WD_RADIO_PART,     m_radioPart);
    DDX_Control(pDX, IDC_WD_BTN_CHOOSE_PART,m_btnChoosePart);
    DDX_Control(pDX, IDC_WD_STATIC_PART,    m_staticPart);
    DDX_Control(pDX, IDC_WD_COMBO_PATTERN,  m_comboPattern);
    DDX_Control(pDX, IDC_WD_BTN_REFRESH,    m_btnRefresh);
    DDX_Control(pDX, IDC_WD_STATIC_SUMMARY, m_staticSummary);
    DDX_Control(pDX, IDC_WD_PROGRESS,       m_progress);
    DDX_Control(pDX, IDC_WD_STATIC_STATUS,  m_staticStatus);
    DDX_Control(pDX, IDC_WD_BTN_START,      m_btnStart);
    DDX_Control(pDX, IDC_WD_BTN_CANCEL,     m_btnCancelWipe);
    DDX_Control(pDX, IDC_WD_BTN_CLOSE,      m_btnClose);
}

BOOL CWipeDiskDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Wipe Disk / Partition"));
    CenterWindow(GetParent());

    m_radioWhole.SetCheck(BST_CHECKED);
    m_btnChoosePart.EnableWindow(FALSE);

    m_comboPattern.AddString(_T("Zeros (1 pass, fast)"));
    m_comboPattern.AddString(_T("Random data (1 pass)"));
    m_comboPattern.AddString(_T("Zeros then Random (2 passes)"));
    m_comboPattern.AddString(_T("Random x3 (3 passes, slowest)"));
    m_comboPattern.SetCurSel(0);

    m_progress.SetRange(0, 1000);
    m_btnCancelWipe.EnableWindow(FALSE);

    RefreshDriveList();
    UpdateModeEnablement();
    UpdateSummary();

    return TRUE;
}

void CWipeDiskDialog::RefreshDriveList()
{
    CDiskManager dm;
    m_drives.clear();
    dm.EnumerateDrives(m_drives);

    CString prevSel;
    if (m_comboDrive.GetCurSel() >= 0) m_comboDrive.GetLBText(m_comboDrive.GetCurSel(), prevSel);

    m_comboDrive.ResetContent();
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);

    if (m_comboDrive.SelectString(-1, prevSel) == CB_ERR && m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    m_hasChosenPartition = false;
    UpdateChosenPartitionLabel();
}

void CWipeDiskDialog::UpdateChosenPartitionLabel()
{
    if (!m_hasChosenPartition)
    {
        m_staticPart.SetWindowText(
            (m_radioPart.GetCheck() == BST_CHECKED)
            ? _T("No partition chosen yet - click \"Choose Partition...\"")
            : _T("(whole disk selected)"));
        return;
    }
    CString s;
    s.Format(_T("Partition #%d: %s, start LBA %llu, %s"),
              m_chosenPartition.index, m_chosenPartition.typeName.GetString(),
              m_chosenPartition.startLBA, CPartitionParser::FormatSize(m_chosenPartition.sizeBytes).GetString());
    m_staticPart.SetWindowText(s);
}

void CWipeDiskDialog::UpdateModeEnablement()
{
    bool part = (m_radioPart.GetCheck() == BST_CHECKED);
    m_btnChoosePart.ShowWindow(part ? SW_SHOW : SW_HIDE);
    m_btnChoosePart.EnableWindow(part);
}

void CWipeDiskDialog::OnRadioMode()
{
    if (m_radioWhole.GetCheck() == BST_CHECKED)
        m_hasChosenPartition = false;
    UpdateModeEnablement();
    UpdateChosenPartitionLabel();
    UpdateSummary();
}

void CWipeDiskDialog::OnComboDriveChange()
{
    m_hasChosenPartition = false;
    UpdateChosenPartitionLabel();
    UpdateSummary();
}

void CWipeDiskDialog::OnBtnChoosePartition()
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
        AfxMessageBox(_T("Could not open the drive to read its partition table:\r\n") + dm.GetLastError(),
                      MB_ICONERROR);
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
        }
    }
    dm.CloseDrive();

    UpdateChosenPartitionLabel();
    UpdateSummary();
}

void CWipeDiskDialog::OnBtnRefresh()
{
    RefreshDriveList();
    UpdateSummary();
}

void CWipeDiskDialog::UpdateSummary()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        m_staticSummary.SetWindowText(_T("Select a drive."));
        return;
    }

    const DriveInfo& d = m_drives[sel];
    bool part = (m_radioPart.GetCheck() == BST_CHECKED);
    ULONGLONG bytes = (part && m_hasChosenPartition) ? m_chosenPartition.sizeBytes : d.totalBytes;

    CString summary;
    summary.Format(_T("This will overwrite %s "), CPartitionParser::FormatSize(bytes).GetString());
    summary += part ? _T("(selected partition).") : _T("(entire disk).");

    if (part && !m_hasChosenPartition)
    {
        summary += _T("\r\nChoose a partition.");
    }
    else if (CSystemDiskGuard::IsProtectedDrive(d.driveIndex))
    {
        summary += _T("\r\n\u26A0 This is the drive Windows is running from - it cannot be wiped.");
    }
    else
    {
        summary += part
            ? _T("\r\n\u26A0 EVERYTHING in that partition will be permanently destroyed. Other partitions are not touched.")
            : _T("\r\n\u26A0 EVERYTHING on the entire disk will be permanently destroyed.");
        summary += _T("\r\n\r\nNote: this is a straightforward overwrite, not a certified secure-erase - see the ")
                   _T("README for details on what it does and doesn't guarantee.");
    }

    m_staticSummary.SetWindowText(summary);
}

bool CWipeDiskDialog::BuildJob(WipeJob& job, CString& errorOut)
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        errorOut = _T("Select a drive.");
        return false;
    }
    const DriveInfo& d = m_drives[sel];

    job.devicePath = d.devicePath;
    job.driveIndex = d.driveIndex;
    job.sectorSize = d.bytesPerSector;

    if (m_radioPart.GetCheck() == BST_CHECKED)
    {
        if (!m_hasChosenPartition)
        {
            errorOut = _T("Choose a partition to wipe, or switch to \"Whole disk\".");
            return false;
        }
        job.startLBA    = m_chosenPartition.startLBA;
        job.sectorCount = m_chosenPartition.sectorCount;
    }
    else
    {
        job.startLBA    = 0;
        job.sectorCount = d.totalSectors;
    }

    switch (m_comboPattern.GetCurSel())
    {
        case 0: job.pattern = WipePattern::Zeros; break;
        case 1: job.pattern = WipePattern::Random; break;
        case 2: job.pattern = WipePattern::ZerosThenRandom; break;
        case 3: job.pattern = WipePattern::RandomThreePass; break;
        default: job.pattern = WipePattern::Zeros; break;
    }

    return true;
}

void CWipeDiskDialog::SetUiEnabled(bool enabled)
{
    m_comboDrive.EnableWindow(enabled);
    m_radioWhole.EnableWindow(enabled);
    m_radioPart.EnableWindow(enabled);
    m_btnChoosePart.EnableWindow(enabled && m_radioPart.GetCheck() == BST_CHECKED);
    m_comboPattern.EnableWindow(enabled);
    m_btnRefresh.EnableWindow(enabled);
    m_btnStart.EnableWindow(enabled);
    m_btnClose.EnableWindow(enabled);
    m_btnCancelWipe.EnableWindow(!enabled);
}

UINT __cdecl CWipeDiskDialog::WipeThreadProc(LPVOID pParam)
{
    auto* jobPtr = static_cast<std::pair<CWipeEngine*, WipeJob*>*>(pParam);
    jobPtr->first->Run(*jobPtr->second);
    delete jobPtr->second;
    delete jobPtr;
    return 0;
}

void CWipeDiskDialog::OnBtnStart()
{
    WipeJob job;
    CString buildError;
    if (!BuildJob(job, buildError))
    {
        AfxMessageBox(buildError, MB_ICONWARNING);
        return;
    }

    CString validationError = ValidateWipeJob(job);
    if (!validationError.IsEmpty())
    {
        AfxMessageBox(validationError, MB_ICONERROR);
        return;
    }

    CString driveName = ShortDriveName(job.devicePath);
    CString scopeDesc = (job.startLBA == 0)
        ? CString(_T("the ENTIRE disk"))
        : CString(_T("the selected PARTITION"));

    CString message;
    message.Format(
        _T("You are about to wipe %s on %s.\r\n\r\n")
        _T("ALL data there will be permanently destroyed and CANNOT be recovered by this app or, in most ")
        _T("cases, by any other tool.\r\n\r\nThis writes directly to the disk now."),
        scopeDesc.GetString(), driveName.GetString());

    CString requiredPhrase;
    if (job.startLBA == 0)
    {
        requiredPhrase = CString(_T("ERASE ")) + driveName;
    }
    else
    {
        CString partNum;
        partNum.Format(_T("%d"), m_chosenPartition.index);
        requiredPhrase = CString(_T("WIPE ")) + driveName + _T(" PART ") + partNum;
    }

    CConfirmTextDialog confirmDlg(message, requiredPhrase, this);
    if (confirmDlg.DoModal() != IDOK)
        return;

    validationError = ValidateWipeJob(job);
    if (!validationError.IsEmpty())
    {
        AfxMessageBox(validationError, MB_ICONERROR);
        return;
    }

    m_progress.SetPos(0);
    m_staticStatus.SetWindowText(_T("Starting..."));
    SetUiEnabled(false);
    m_wipeRunning = true;

    auto* jobHeap = new WipeJob(job);
    auto* threadParam = new std::pair<CWipeEngine*, WipeJob*>(&m_engine, jobHeap);
    m_pThread = AfxBeginThread(&CWipeDiskDialog::WipeThreadProc, threadParam);

    SetTimer(TIMER_ID, 250, nullptr);
}

void CWipeDiskDialog::OnBtnCancelWipe()
{
    m_btnCancelWipe.EnableWindow(FALSE);
    m_staticStatus.SetWindowText(_T("Cancelling..."));
    m_engine.RequestCancel();
}

void CWipeDiskDialog::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        WipeProgress p = m_engine.GetProgressSnapshot();

        if (p.bytesTotal > 0)
        {
            int permille = static_cast<int>((p.bytesWritten * 1000ULL) / p.bytesTotal);
            m_progress.SetPos(permille);
        }
        m_staticStatus.SetWindowText(p.statusText);

        if (p.finished)
        {
            KillTimer(TIMER_ID);
            m_wipeRunning = false;
            SetUiEnabled(true);

            if (p.success)
            {
                m_progress.SetPos(1000);
                m_staticStatus.SetWindowText(_T("Done."));
                AfxMessageBox(_T("Wipe completed successfully."), MB_ICONINFORMATION);
            }
            else if (p.canceled)
            {
                m_staticStatus.SetWindowText(_T("Cancelled."));
                AfxMessageBox(_T("Wipe was cancelled. The region written so far has already been overwritten ")
                              _T("and cannot be recovered; the rest was left untouched."), MB_ICONWARNING);
            }
            else
            {
                m_staticStatus.SetWindowText(_T("Failed."));
                AfxMessageBox(_T("Wipe failed:\r\n") + p.errorMessage, MB_ICONERROR);
            }
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CWipeDiskDialog::OnBtnClose()
{
    if (m_wipeRunning)
    {
        AfxMessageBox(_T("A wipe is still running. Cancel it first."), MB_ICONWARNING);
        return;
    }
    EndDialog(IDCANCEL);
}

void CWipeDiskDialog::OnCancel()
{
    if (m_wipeRunning)
    {
        AfxMessageBox(_T("A wipe is still running. Cancel it first."), MB_ICONWARNING);
        return;
    }
    CDialogEx::OnCancel();
}

void CWipeDiskDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_staticSummary.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    CRect rc;

    // Drive combo: stretch to fill cx; radio buttons, choose-part, pattern combo, and
    // refresh button stay at their template (DPI-correct) positions.
    m_comboDrive.GetWindowRect(&rc); ScreenToClient(&rc);
    m_comboDrive.MoveWindow(rc.left, rc.top, R - rc.left, rc.Height(), TRUE);

    // Part label: full width, template y.
    m_staticPart.GetWindowRect(&rc); ScreenToClient(&rc);
    m_staticPart.MoveWindow(L, rc.top, R - L, rc.Height(), TRUE);

    // Compute DPI-correct heights for the bottom section.
    CRect rBh(0, 0, 0, 17); MapDialogRect(&rBh);
    const int bh = rBh.bottom;
    m_progress.GetWindowRect(&rc); ScreenToClient(&rc);
    const int barH = rc.Height();
    m_staticStatus.GetWindowRect(&rc); ScreenToClient(&rc);
    const int statH = rc.Height();

    // Bottom rows (from bottom up): buttons, status text, progress bar.
    const int btnBy  = cy - bh - 6;
    const int statBy = btnBy - statH - 4;
    const int progBy = statBy - barH - 4;

    m_progress.MoveWindow(L, progBy, R - L, barH, TRUE);
    m_staticStatus.MoveWindow(L, statBy, R - L, statH, TRUE);

    auto anchorBottom = [&](CWnd& w) {
        w.GetWindowRect(&rc); ScreenToClient(&rc);
        w.MoveWindow(rc.left, btnBy, rc.Width(), bh, TRUE);
    };
    anchorBottom(m_btnStart);
    anchorBottom(m_btnCancelWipe);
    m_btnClose.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnClose.MoveWindow(R - rc.Width(), btnBy, rc.Width(), bh, TRUE);

    // Summary fills from its template top to the progress bar.
    m_staticSummary.GetWindowRect(&rc); ScreenToClient(&rc);
    int sumH = progBy - rc.top - 6;
    if (sumH < 20) sumH = 20;
    m_staticSummary.MoveWindow(L, rc.top, R - L, sumH, TRUE);
}

void CWipeDiskDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
