// BadSectorScanDialog.cpp
#include "pch.h"
#include "BadSectorScanDialog.h"
#include "PartitionDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CBadSectorScanDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_BSS_COMBO_DRIVE, &CBadSectorScanDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_BSS_RADIO_WHOLE, &CBadSectorScanDialog::OnRadioMode)
    ON_BN_CLICKED(IDC_BSS_RADIO_PART, &CBadSectorScanDialog::OnRadioMode)
    ON_BN_CLICKED(IDC_BSS_RADIO_RANGE, &CBadSectorScanDialog::OnRadioMode)
    ON_BN_CLICKED(IDC_BSS_BTN_CHOOSE_PART, &CBadSectorScanDialog::OnBtnChoosePartition)
    ON_BN_CLICKED(IDC_BSS_BTN_START, &CBadSectorScanDialog::OnBtnStart)
    ON_BN_CLICKED(IDC_BSS_BTN_STOP, &CBadSectorScanDialog::OnBtnStop)
    ON_BN_CLICKED(IDC_BSS_BTN_REPAIR, &CBadSectorScanDialog::OnBtnRepair)
    ON_BN_CLICKED(IDC_BSS_BTN_CLOSE, &CBadSectorScanDialog::OnBtnClose)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_BSS_LIST_BAD, &CBadSectorScanDialog::OnListSelChange)
END_MESSAGE_MAP()

CBadSectorScanDialog::CBadSectorScanDialog(CWnd* pParent)
    : CDialogEx(IDD_BADSECTORSCAN, pParent)
    , m_hasChosenPartition(false)
    , m_scanRunning(false)
    , m_pThread(nullptr)
    , m_lastBadRangeCount(0)
    , m_lastSectorSize(512)
    , m_scannedDriveIndex(0)
{
}

CBadSectorScanDialog::~CBadSectorScanDialog()
{
    if (m_scanRunning)
        m_scanner.RequestCancel();
}

void CBadSectorScanDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_BSS_COMBO_DRIVE,            m_comboDrive);
    DDX_Control(pDX, IDC_BSS_RADIO_WHOLE,            m_radioWhole);
    DDX_Control(pDX, IDC_BSS_RADIO_PART,             m_radioPart);
    DDX_Control(pDX, IDC_BSS_RADIO_RANGE,            m_radioRange);
    DDX_Control(pDX, IDC_BSS_BTN_CHOOSE_PART,        m_btnChoosePart);
    DDX_Control(pDX, IDC_BSS_STATIC_RANGE_START_LBL, m_staticRangeStartLbl);
    DDX_Control(pDX, IDC_BSS_EDIT_RANGE_START,       m_editRangeStart);
    DDX_Control(pDX, IDC_BSS_STATIC_RANGE_COUNT_LBL, m_staticRangeCountLbl);
    DDX_Control(pDX, IDC_BSS_EDIT_RANGE_COUNT,       m_editRangeCount);
    DDX_Control(pDX, IDC_BSS_STATIC_PART,            m_staticPart);
    DDX_Control(pDX, IDC_BSS_STATIC_STATUS,          m_staticStatus);
    DDX_Control(pDX, IDC_BSS_LIST_BAD,               m_listBad);
    DDX_Control(pDX, IDC_BSS_BTN_START,              m_btnStart);
    DDX_Control(pDX, IDC_BSS_BTN_STOP,               m_btnStop);
    DDX_Control(pDX, IDC_BSS_BTN_REPAIR,             m_btnRepair);
    DDX_Control(pDX, IDC_BSS_BTN_CLOSE,              m_btnClose);
    // IDC_BSS_STATIC_MAP is a placeholder LTEXT; destroyed and replaced below
}

BOOL CBadSectorScanDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Bad Sector Scan (read-only surface test)"));
    CenterWindow(GetParent());

    // Replace the IDC_BSS_STATIC_MAP placeholder LTEXT with the real CScanMapView
    CRect rcMap;
    if (CWnd* pPh = GetDlgItem(IDC_BSS_STATIC_MAP))
    {
        pPh->GetWindowRect(&rcMap);
        ScreenToClient(&rcMap);
        pPh->DestroyWindow();
    }
    else
    {
        rcMap = CRect(12, 156, 688, 256);
    }
    m_mapView.Create(this, rcMap, IDC_BSS_STATIC_MAP);

    m_radioWhole.SetCheck(BST_CHECKED);
    m_editRangeStart.SetWindowText(_T("0"));

    m_listBad.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listBad.InsertColumn(0, _T("Start LBA"),    LVCFMT_RIGHT, 140);
    m_listBad.InsertColumn(1, _T("Sector Count"), LVCFMT_RIGHT, 140);
    m_listBad.InsertColumn(2, _T("Byte Offset"),  LVCFMT_RIGHT, 160);

    m_btnStop.EnableWindow(FALSE);
    m_btnRepair.EnableWindow(FALSE);

    CDiskManager dm;
    dm.EnumerateDrives(m_drives);
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);
    if (m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    m_mapView.SetRange(0, 0);
    UpdateModeEnablement();
    UpdateChosenPartitionLabel();

    return TRUE;
}

void CBadSectorScanDialog::UpdateModeEnablement()
{
    bool part = (m_radioPart.GetCheck() == BST_CHECKED);
    bool range = (m_radioRange.GetCheck() == BST_CHECKED);

    m_btnChoosePart.ShowWindow(part ? SW_SHOW : SW_HIDE);
    m_btnChoosePart.EnableWindow(part);

    m_editRangeStart.ShowWindow(range ? SW_SHOW : SW_HIDE);
    m_editRangeStart.EnableWindow(range);
    m_editRangeCount.ShowWindow(range ? SW_SHOW : SW_HIDE);
    m_editRangeCount.EnableWindow(range);
    m_staticRangeStartLbl.ShowWindow(range ? SW_SHOW : SW_HIDE);
    m_staticRangeCountLbl.ShowWindow(range ? SW_SHOW : SW_HIDE);
}

void CBadSectorScanDialog::UpdateChosenPartitionLabel()
{
    if (m_radioPart.GetCheck() == BST_CHECKED)
    {
        if (m_hasChosenPartition)
        {
            CString s;
            s.Format(_T("Partition #%d: %s, start LBA %llu, %s"),
                m_chosenPartition.index, m_chosenPartition.typeName.GetString(),
                m_chosenPartition.startLBA, CPartitionParser::FormatSize(m_chosenPartition.sizeBytes).GetString());
            m_staticPart.SetWindowText(s);
        }
        else
        {
            m_staticPart.SetWindowText(_T("No partition chosen yet - click \"Choose Partition...\""));
        }
    }
    else if (m_radioRange.GetCheck() == BST_CHECKED)
    {
        m_staticPart.SetWindowText(_T("Custom range - enter start sector and sector count above."));
    }
    else
    {
        m_staticPart.SetWindowText(_T("(entire disk will be scanned)"));
    }
}

void CBadSectorScanDialog::OnComboDriveChange()
{
    m_hasChosenPartition = false;
    UpdateChosenPartitionLabel();
}

void CBadSectorScanDialog::OnRadioMode()
{
    if (m_radioWhole.GetCheck() == BST_CHECKED)
        m_hasChosenPartition = false;
    UpdateModeEnablement();
    UpdateChosenPartitionLabel();
}

void CBadSectorScanDialog::OnBtnChoosePartition()
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
}

bool CBadSectorScanDialog::BuildJob(ScanJob& job, CString& errorOut)
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        errorOut = _T("Select a drive to scan.");
        return false;
    }
    const DriveInfo& d = m_drives[sel];

    job.devicePath = d.devicePath;
    job.sectorSize = d.bytesPerSector;

    if (m_radioPart.GetCheck() == BST_CHECKED)
    {
        if (!m_hasChosenPartition)
        {
            errorOut = _T("Choose a partition to scan, or switch to \"Whole disk\".");
            return false;
        }
        job.startLBA = m_chosenPartition.startLBA;
        job.sectorCount = m_chosenPartition.sectorCount;
    }
    else if (m_radioRange.GetCheck() == BST_CHECKED)
    {
        CString startStr, countStr;
        m_editRangeStart.GetWindowText(startStr);
        m_editRangeCount.GetWindowText(countStr);
        ULONGLONG start = _tcstoui64(startStr, nullptr, 10);
        ULONGLONG count = _tcstoui64(countStr, nullptr, 10);
        if (count == 0)
        {
            errorOut = _T("Enter a sector count greater than zero.");
            return false;
        }
        if (start + count > d.totalSectors)
        {
            errorOut = _T("The requested range extends past the end of the drive.");
            return false;
        }
        job.startLBA = start;
        job.sectorCount = count;
    }
    else
    {
        job.startLBA = 0;
        job.sectorCount = d.totalSectors;
    }

    return true;
}

void CBadSectorScanDialog::SetUiEnabled(bool enabled)
{
    m_comboDrive.EnableWindow(enabled);
    m_radioWhole.EnableWindow(enabled);
    m_radioPart.EnableWindow(enabled);
    m_radioRange.EnableWindow(enabled);
    m_btnChoosePart.EnableWindow(enabled && m_radioPart.GetCheck() == BST_CHECKED);
    m_editRangeStart.EnableWindow(enabled && m_radioRange.GetCheck() == BST_CHECKED);
    m_editRangeCount.EnableWindow(enabled && m_radioRange.GetCheck() == BST_CHECKED);
    m_btnStart.EnableWindow(enabled);
    m_btnClose.EnableWindow(enabled);
    m_btnStop.EnableWindow(!enabled);
    if (!enabled)
        m_btnRepair.EnableWindow(FALSE); // Re-enabled by OnListSelChange once a row is selected after the scan finishes
}

UINT __cdecl CBadSectorScanDialog::ScanThreadProc(LPVOID pParam)
{
    auto* jobPtr = static_cast<std::pair<CBadSectorScanner*, ScanJob*>*>(pParam);
    jobPtr->first->Run(*jobPtr->second);
    delete jobPtr->second;
    delete jobPtr;
    return 0;
}

void CBadSectorScanDialog::OnBtnStart()
{
    ScanJob job;
    CString err;
    if (!BuildJob(job, err))
    {
        AfxMessageBox(err, MB_ICONWARNING);
        return;
    }

    if (job.sectorCount * (ULONGLONG)job.sectorSize > (50ULL * 1024 * 1024 * 1024))
    {
        if (AfxMessageBox(
            _T("This range is larger than 50 GB. A full surface scan can take a long time ")
            _T("(potentially hours for large hard drives). You can stop it early at any point.\r\n\r\n")
            _T("Continue?"), MB_YESNO | MB_ICONINFORMATION) != IDYES)
            return;
    }

    m_listBad.DeleteAllItems();
    m_lastBadRangeCount = 0;
    m_lastSectorSize = job.sectorSize;
    m_scannedDevicePath = job.devicePath;
    m_scannedDriveIndex = 0;
    {
        int sel = m_comboDrive.GetCurSel();
        if (sel >= 0 && sel < (int)m_drives.size())
            m_scannedDriveIndex = m_drives[sel].driveIndex;
    }
    m_badRanges.clear();
    m_btnRepair.EnableWindow(FALSE);
    m_mapView.SetRange(job.startLBA, job.sectorCount);
    m_staticStatus.SetWindowText(_T("Starting scan..."));

    SetUiEnabled(false);
    m_scanRunning = true;

    auto* jobHeap = new ScanJob(job);
    auto* threadParam = new std::pair<CBadSectorScanner*, ScanJob*>(&m_scanner, jobHeap);
    m_pThread = AfxBeginThread(&CBadSectorScanDialog::ScanThreadProc, threadParam);

    SetTimer(TIMER_ID, 400, nullptr);
}

void CBadSectorScanDialog::OnBtnStop()
{
    m_btnStop.EnableWindow(FALSE);
    m_staticStatus.SetWindowText(_T("Stopping..."));
    m_scanner.RequestCancel();
}

void CBadSectorScanDialog::OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNM = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if ((pNM->uChanged & LVIF_STATE) && (pNM->uNewState & LVIS_SELECTED))
        m_btnRepair.EnableWindow(!m_scanRunning);
    *pResult = 0;
}

void CBadSectorScanDialog::OnBtnRepair()
{
    int sel = m_listBad.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_badRanges.size())
    {
        AfxMessageBox(_T("Select a bad-sector row first."), MB_ICONWARNING);
        return;
    }
    if (m_scannedDevicePath.IsEmpty())
    {
        AfxMessageBox(_T("Run a scan first."), MB_ICONWARNING);
        return;
    }

    const BadSectorRange& range = m_badRanges[sel];

    CString driveName = m_scannedDevicePath;
    int pos = driveName.ReverseFind(_T('\\'));
    if (pos >= 0) driveName = driveName.Mid(pos + 1);

    CString msg;
    msg.Format(
        _T("Attempt to repair %llu sector(s) starting at LBA %llu on %s?\r\n\r\n")
        _T("This writes zeros to that location and gives the drive a chance to fix or reallocate it. ")
        _T("Whatever data was there is already unreadable and is NOT recoverable by this or any other ")
        _T("action - this only tries to make the sector usable again, it does not recover data.\r\n\r\n")
        _T("This writes directly to the disk now."),
        range.sectorCount, range.startLBA, driveName.GetString());

    if (AfxMessageBox(msg, MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    SectorRepairResult result = CSectorRepairer::Repair(
        m_scannedDevicePath, m_scannedDriveIndex, m_lastSectorSize, range.startLBA, range.sectorCount);

    if (!result.writeSucceeded)
    {
        AfxMessageBox(_T("Repair attempt failed:\r\n") + result.errorMessage, MB_ICONERROR);
        return;
    }

    if (result.nowReadable)
    {
        AfxMessageBox(_T("The write succeeded and the sector now reads back successfully. ")
            _T("Removing it from the bad-sector list below."), MB_ICONINFORMATION);
        m_badRanges.erase(m_badRanges.begin() + sel);
        m_listBad.DeleteItem(sel);
        // Keep m_lastBadRangeCount in sync so a subsequent scan tick doesn't
        // think nothing changed and skip repopulating.
        m_lastBadRangeCount = m_badRanges.size();
    }
    else
    {
        AfxMessageBox(_T("The write succeeded, but the sector still fails to read back. ")
            _T("This usually means the underlying defect is more serious - consider backing up ")
            _T("this drive and planning to replace it."), MB_ICONWARNING);
    }
}

void CBadSectorScanDialog::PopulateBadList()
{
    std::vector<BadSectorRange> ranges = m_scanner.GetBadRangesSnapshot();
    if (ranges.size() == m_lastBadRangeCount)
        return; // Nothing new

    m_badRanges = ranges;
    m_listBad.DeleteAllItems();
    for (size_t i = 0; i < ranges.size(); ++i)
    {
        CString startStr, countStr, offsetStr;
        startStr.Format(_T("%llu"), ranges[i].startLBA);
        countStr.Format(_T("%llu"), ranges[i].sectorCount);

        int row = m_listBad.InsertItem((int)i, startStr);
        m_listBad.SetItemText(row, 1, countStr);

        ULONGLONG byteOffset = ranges[i].startLBA * (ULONGLONG)m_lastSectorSize;
        offsetStr.Format(_T("%llu"), byteOffset);
        m_listBad.SetItemText(row, 2, offsetStr);
    }
    m_lastBadRangeCount = ranges.size();
}

void CBadSectorScanDialog::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        ScanProgress p = m_scanner.GetProgressSnapshot();
        std::vector<ScanBlockResult> blocks = m_scanner.GetBlocksSnapshot();

        std::vector<ScanMapBlock> mapBlocks;
        mapBlocks.reserve(blocks.size());
        for (const auto& b : blocks)
        {
            ScanMapBlock mb;
            mb.startLBA = b.startLBA;
            mb.sectorCount = b.sectorCount;
            mb.status = (b.status == SectorBlockStatus::Bad) ? 2 : (b.status == SectorBlockStatus::Slow ? 1 : 0);
            mapBlocks.push_back(mb);
        }
        m_mapView.SetBlocks(mapBlocks);

        m_staticStatus.SetWindowText(p.statusText);
        PopulateBadList();

        if (p.finished)
        {
            KillTimer(TIMER_ID);
            m_scanRunning = false;
            SetUiEnabled(true);

            if (p.canceled)
            {
                m_staticStatus.SetWindowText(_T("Stopped by user."));
            }
            else if (!p.success)
            {
                m_staticStatus.SetWindowText(_T("Scan failed."));
                AfxMessageBox(_T("Scan failed:\r\n") + p.errorMessage, MB_ICONERROR);
            }
            else
            {
                ULONGLONG badCount = 0;
                for (const auto& r : m_scanner.GetBadRangesSnapshot())
                    badCount += r.sectorCount;

                CString msg;
                if (badCount == 0)
                    msg = _T("Scan complete. No bad sectors found.");
                else
                    msg.Format(_T("Scan complete. %llu bad sector(s) found - see the list below."), badCount);
                m_staticStatus.SetWindowText(msg);
            }
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CBadSectorScanDialog::OnBtnClose()
{
    if (m_scanRunning)
    {
        AfxMessageBox(_T("A scan is still running. Stop it first."), MB_ICONWARNING);
        return;
    }
    EndDialog(IDCANCEL);
}

void CBadSectorScanDialog::OnCancel()
{
    if (m_scanRunning)
    {
        AfxMessageBox(_T("A scan is still running. Stop it first."), MB_ICONWARNING);
        return;
    }
    CDialogEx::OnCancel();
}

void CBadSectorScanDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_mapView.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    CRect rc;

    // Full-width controls: stretch to fit cx, keep template y/height (DPI-correct).
    // Radio buttons, range controls, and choose-part button stay at template positions.
    auto stretchFull = [&](CWnd& w) {
        w.GetWindowRect(&rc); ScreenToClient(&rc);
        w.MoveWindow(L, rc.top, R - L, rc.Height(), TRUE);
    };
    m_comboDrive.GetWindowRect(&rc); ScreenToClient(&rc);
    m_comboDrive.MoveWindow(rc.left, rc.top, R - rc.left, rc.Height(), TRUE);
    stretchFull(m_staticPart);
    stretchFull(m_mapView);
    stretchFull(m_staticStatus);

    // Bad-sector list: full width, grows with cy
    m_listBad.GetWindowRect(&rc); ScreenToClient(&rc);
    CRect rBh(0, 0, 0, 17); MapDialogRect(&rBh);
    const int bh = rBh.bottom, by = cy - bh - 6;
    m_listBad.MoveWindow(L, rc.top, R - L, by - rc.top - 8, TRUE);

    // Bottom buttons: keep template x/width, anchor to bottom
    auto anchorBottom = [&](CWnd& w) {
        w.GetWindowRect(&rc); ScreenToClient(&rc);
        w.MoveWindow(rc.left, by, rc.Width(), bh, TRUE);
    };
    anchorBottom(m_btnStart);
    anchorBottom(m_btnStop);
    anchorBottom(m_btnRepair);
    m_btnClose.GetWindowRect(&rc); ScreenToClient(&rc);
    m_btnClose.MoveWindow(R - rc.Width(), by, rc.Width(), bh, TRUE);
}

void CBadSectorScanDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
