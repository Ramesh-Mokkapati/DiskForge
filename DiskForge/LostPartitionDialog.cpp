// LostPartitionDialog.cpp
#include "pch.h"
#include "LostPartitionDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CLostPartitionDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_LP_COMBO_DRIVE, &CLostPartitionDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_LP_BTN_START, &CLostPartitionDialog::OnBtnStart)
    ON_BN_CLICKED(IDC_LP_BTN_STOP,  &CLostPartitionDialog::OnBtnStop)
    ON_BN_CLICKED(IDC_LP_BTN_CLOSE, &CLostPartitionDialog::OnBtnClose)
END_MESSAGE_MAP()

CLostPartitionDialog::CLostPartitionDialog(CWnd* pParent)
    : CDialogEx(IDD_LOSTPARTITION, pParent)
    , m_scanRunning(false)
    , m_pThread(nullptr)
    , m_lastCandidateCount(0)
{
}

CLostPartitionDialog::~CLostPartitionDialog()
{
    if (m_scanRunning)
        m_scanner.RequestCancel();
}

void CLostPartitionDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LP_COMBO_DRIVE,       m_comboDrive);
    DDX_Control(pDX, IDC_LP_STATIC_FREESPACE,  m_staticFreeSpace);
    DDX_Control(pDX, IDC_LP_LIST_CANDIDATES,   m_listCandidates);
    DDX_Control(pDX, IDC_LP_PROGRESS,          m_progress);
    DDX_Control(pDX, IDC_LP_STATIC_STATUS,     m_staticStatus);
    DDX_Control(pDX, IDC_LP_BTN_START,         m_btnStart);
    DDX_Control(pDX, IDC_LP_BTN_STOP,          m_btnStop);
    DDX_Control(pDX, IDC_LP_BTN_CLOSE,         m_btnClose);
}

BOOL CLostPartitionDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Lost Partition Search (read-only)"));
    CenterWindow(GetParent());

    m_listCandidates.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listCandidates.InsertColumn(0, _T("Start LBA"),       LVCFMT_RIGHT, 100);
    m_listCandidates.InsertColumn(1, _T("Type"),            LVCFMT_LEFT,   80);
    m_listCandidates.InsertColumn(2, _T("Size"),            LVCFMT_RIGHT, 100);
    m_listCandidates.InsertColumn(3, _T("Bytes/Sector"),    LVCFMT_RIGHT,  90);
    m_listCandidates.InsertColumn(4, _T("Sectors/Cluster"), LVCFMT_RIGHT, 100);
    m_listCandidates.InsertColumn(5, _T("Volume Label"),    LVCFMT_LEFT,  140);

    m_progress.SetRange(0, 1000);
    m_btnStop.EnableWindow(FALSE);

    CDiskManager dm;
    dm.EnumerateDrives(m_drives);
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);
    if (m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    UpdateFreeSpacePreview();

    return TRUE;
}

void CLostPartitionDialog::OnComboDriveChange()
{
    UpdateFreeSpacePreview();
}

void CLostPartitionDialog::UpdateFreeSpacePreview()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        m_staticFreeSpace.SetWindowText(_T("Select a drive."));
        return;
    }

    const DriveInfo& d = m_drives[sel];
    CDiskManager dm;
    if (!dm.OpenDrive(d.devicePath))
    {
        m_staticFreeSpace.SetWindowText(_T("Could not open the drive: ") + dm.GetLastError());
        return;
    }

    std::vector<PartitionEntry> parts;
    CString scheme, diskGuid, err;
    CPartitionParser::Parse(dm, parts, scheme, diskGuid, err);
    dm.CloseDrive();

    std::vector<FreeRange> used;
    for (const auto& p : parts)
        if (!p.isLogical) // Logical partitions live inside their extended container, already excluded via it
            used.push_back({ p.startLBA, p.sectorCount });

    std::vector<FreeRange> free = CLostPartitionScanner::ComputeFreeRanges(d.totalSectors, used);
    ULONGLONG freeSectors = 0;
    for (const auto& r : free) freeSectors += r.sectorCount;

    CString msg;
    msg.Format(_T("Current scheme: %s. %s of unallocated space will be scanned (in %d range(s)) - ")
               _T("space already covered by an existing partition is skipped, both because it isn't ")
               _T("\"lost\" and to keep the scan faster."),
               scheme.GetString(), CPartitionParser::FormatSize(freeSectors * (ULONGLONG)d.bytesPerSector).GetString(),
               (int)free.size());
    m_staticFreeSpace.SetWindowText(msg);
}

void CLostPartitionDialog::SetUiEnabled(bool enabled)
{
    m_comboDrive.EnableWindow(enabled);
    m_btnStart.EnableWindow(enabled);
    m_btnClose.EnableWindow(enabled);
    m_btnStop.EnableWindow(!enabled);
}

UINT __cdecl CLostPartitionDialog::ScanThreadProc(LPVOID pParam)
{
    auto* jobPtr = static_cast<std::pair<CLostPartitionScanner*, LostPartitionScanJob*>*>(pParam);
    jobPtr->first->Run(*jobPtr->second);
    delete jobPtr->second;
    delete jobPtr;
    return 0;
}

void CLostPartitionDialog::OnBtnStart()
{
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a drive first."), MB_ICONWARNING);
        return;
    }
    const DriveInfo& d = m_drives[sel];

    CDiskManager dm;
    if (!dm.OpenDrive(d.devicePath))
    {
        AfxMessageBox(_T("Could not open the drive:\r\n") + dm.GetLastError(), MB_ICONERROR);
        return;
    }
    std::vector<PartitionEntry> parts;
    CString scheme, diskGuid, err;
    CPartitionParser::Parse(dm, parts, scheme, diskGuid, err);
    dm.CloseDrive();

    LostPartitionScanJob job;
    job.devicePath = d.devicePath;
    job.sectorSize = d.bytesPerSector;
    job.driveIndex = d.driveIndex;
    job.totalSectors = d.totalSectors;
    for (const auto& p : parts)
        if (!p.isLogical)
            job.existingPartitions.push_back({ p.startLBA, p.sectorCount });

    m_listCandidates.DeleteAllItems();
    m_lastCandidateCount = 0;
    m_staticStatus.SetWindowText(_T("Starting scan..."));
    m_progress.SetPos(0);

    SetUiEnabled(false);
    m_scanRunning = true;

    auto* jobHeap = new LostPartitionScanJob(job);
    auto* threadParam = new std::pair<CLostPartitionScanner*, LostPartitionScanJob*>(&m_scanner, jobHeap);
    m_pThread = AfxBeginThread(&CLostPartitionDialog::ScanThreadProc, threadParam);

    SetTimer(TIMER_ID, 400, nullptr);
}

void CLostPartitionDialog::OnBtnStop()
{
    m_btnStop.EnableWindow(FALSE);
    m_staticStatus.SetWindowText(_T("Stopping..."));
    m_scanner.RequestCancel();
}

void CLostPartitionDialog::PopulateCandidateList()
{
    std::vector<LostPartitionCandidate> cands = m_scanner.GetCandidatesSnapshot();
    if (cands.size() == m_lastCandidateCount)
        return;

    m_listCandidates.DeleteAllItems();
    for (size_t i = 0; i < cands.size(); ++i)
    {
        const LostPartitionCandidate& c = cands[i];

        CString startStr, sizeStr, bpsStr, spcStr;
        startStr.Format(_T("%llu"), c.startLBA);
        sizeStr = (c.totalSectorsInVolume > 0)
            ? CPartitionParser::FormatSize(c.totalSectorsInVolume * (ULONGLONG)c.bytesPerSector)
            : CString(_T("(unknown)"));
        bpsStr.Format(_T("%u"), c.bytesPerSector);
        spcStr.Format(_T("%u"), c.sectorsPerCluster);

        int row = m_listCandidates.InsertItem((int)i, startStr);
        m_listCandidates.SetItemText(row, 1, c.fsTypeName);
        m_listCandidates.SetItemText(row, 2, sizeStr);
        m_listCandidates.SetItemText(row, 3, bpsStr);
        m_listCandidates.SetItemText(row, 4, spcStr);
        m_listCandidates.SetItemText(row, 5, c.volumeLabel);
    }
    m_lastCandidateCount = cands.size();
}

void CLostPartitionDialog::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        LostPartitionScanProgress p = m_scanner.GetProgressSnapshot();

        if (p.sectorsToScan > 0)
        {
            int permille = static_cast<int>((p.sectorsScanned * 1000ULL) / p.sectorsToScan);
            m_progress.SetPos(permille);
        }
        m_staticStatus.SetWindowText(p.statusText);
        PopulateCandidateList();

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
                m_progress.SetPos(1000);
                CString msg;
                msg.Format(_T("Scan complete. %d candidate(s) found."), (int)m_lastCandidateCount);
                m_staticStatus.SetWindowText(msg);

                if (m_lastCandidateCount == 0)
                {
                    AfxMessageBox(_T("Scan complete. No filesystem signatures found in unallocated space."), MB_ICONINFORMATION);
                }
                else
                {
                    AfxMessageBox(
                        _T("Scan complete. See the list for candidates.\r\n\r\n")
                        _T("Note: this tool only finds and reports candidates - it does not add them back to the ")
                        _T("live partition table. Also, an NTFS volume's own backup boot sector (stored at its last ")
                        _T("sector) can appear as a second hit for the same volume - that's expected, not a bug."),
                        MB_ICONINFORMATION);
                }
            }
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CLostPartitionDialog::OnBtnClose()
{
    if (m_scanRunning)
    {
        AfxMessageBox(_T("A scan is still running. Stop it first."), MB_ICONWARNING);
        return;
    }
    EndDialog(IDCANCEL);
}

void CLostPartitionDialog::OnCancel()
{
    if (m_scanRunning)
    {
        AfxMessageBox(_T("A scan is still running. Stop it first."), MB_ICONWARNING);
        return;
    }
    CDialogEx::OnCancel();
}

void CLostPartitionDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_listCandidates.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    m_comboDrive.MoveWindow(L + 55, 10, R - (L + 55), 190, TRUE);
    m_staticFreeSpace.MoveWindow(L, 40, R - L, 32, TRUE);
    m_listCandidates.MoveWindow(L, 78, R - L, cy - 78 - 100, TRUE);
    m_progress.MoveWindow(L, cy - 92, R - L, 18, TRUE);
    m_staticStatus.MoveWindow(L, cy - 68, R - L, 28, TRUE);
    m_btnStart.MoveWindow(L, cy - 34, 110, 26, TRUE);
    m_btnStop.MoveWindow(L + 120, cy - 34, 110, 26, TRUE);
    m_btnClose.MoveWindow(R - 90, cy - 34, 90, 26, TRUE);
}

void CLostPartitionDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
