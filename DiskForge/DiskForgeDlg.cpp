// DiskForgeDlg.cpp - Main dialog implementation
#include "pch.h"
#include "DiskForge.h"
#include "DiskForgeDlg.h"
#include "PartitionDialog.h"
#include "DiskCopyDialog.h"
#include "BadSectorScanDialog.h"
#include "SmartHealthDialog.h"
#include "MbrGptConvertDialog.h"
#include "PartitionEditDialog.h"
#include "WipeDiskDialog.h"
#include "EfiBootDialog.h"
#include "LostPartitionDialog.h"
#include "FileRecoveryDialog.h"
#include "FileUnlockerDialog.h"
#include "StartupManagerDialog.h"
#include "RegistryScannerDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ---- Dialog template resource IDs ----
#define IDD_MAIN 100

IMPLEMENT_DYNCREATE(CDiskForgeDlg, CFormView)

BEGIN_MESSAGE_MAP(CDiskForgeDlg, CFormView)
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BTN_REFRESH, &CDiskForgeDlg::OnBtnRefresh)
    ON_BN_CLICKED(IDC_BTN_OPEN,    &CDiskForgeDlg::OnBtnOpen)
    ON_BN_CLICKED(IDC_BTN_GOTO,    &CDiskForgeDlg::OnBtnGoto)
    ON_BN_CLICKED(IDC_BTN_PREV,    &CDiskForgeDlg::OnBtnPrev)
    ON_BN_CLICKED(IDC_BTN_NEXT,    &CDiskForgeDlg::OnBtnNext)
    ON_BN_CLICKED(IDC_BTN_FIRST,   &CDiskForgeDlg::OnBtnFirst)
    ON_BN_CLICKED(IDC_BTN_LAST,    &CDiskForgeDlg::OnBtnLast)
    ON_BN_CLICKED(IDC_BTN_PARTITIONS, &CDiskForgeDlg::OnBtnPartitions)
    ON_BN_CLICKED(IDC_BTN_DISKCOPY,   &CDiskForgeDlg::OnBtnDiskCopy)
    ON_BN_CLICKED(IDC_BTN_BADSECTORSCAN, &CDiskForgeDlg::OnBtnBadSectorScan)
    ON_BN_CLICKED(IDC_BTN_SMARTHEALTH, &CDiskForgeDlg::OnBtnSmartHealth)
    ON_BN_CLICKED(IDC_BTN_MBRGPT, &CDiskForgeDlg::OnBtnMbrGptConvert)
    ON_BN_CLICKED(IDC_BTN_PARTEDIT, &CDiskForgeDlg::OnBtnPartitionEdit)
    ON_BN_CLICKED(IDC_BTN_WIPEDISK, &CDiskForgeDlg::OnBtnWipeDisk)
    ON_BN_CLICKED(IDC_BTN_EFIBOOT, &CDiskForgeDlg::OnBtnEfiBoot)
    ON_BN_CLICKED(IDC_BTN_LOSTPART, &CDiskForgeDlg::OnBtnLostPartition)
    ON_BN_CLICKED(IDC_BTN_FILERECOVERY, &CDiskForgeDlg::OnBtnFileRecovery)
    ON_BN_CLICKED(IDC_BTN_FILEUNLOCKER, &CDiskForgeDlg::OnBtnFileUnlocker)
    ON_BN_CLICKED(IDC_BTN_STARTUP,      &CDiskForgeDlg::OnBtnStartup)
    ON_BN_CLICKED(IDC_BTN_REGSCAN,     &CDiskForgeDlg::OnBtnRegScan)
    ON_CBN_SELCHANGE(IDC_COMBO_DRIVES, &CDiskForgeDlg::OnComboSelChange)
    ON_EN_CHANGE(IDC_EDIT_SECTOR,  &CDiskForgeDlg::OnEditSectorChange)
END_MESSAGE_MAP()

CDiskForgeDlg::CDiskForgeDlg()
    : CFormView(IDD)
    , m_currentSector(0)
    , m_driveOpen(false)
{
}

void CDiskForgeDlg::DoDataExchange(CDataExchange* pDX)
{
    CFormView::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_COMBO_DRIVES,         m_comboDrives);
    DDX_Control(pDX, IDC_BTN_OPEN,             m_btnOpen);
    DDX_Control(pDX, IDC_STATIC_DRIVEINFO,     m_staticDriveInfo);
    DDX_Control(pDX, IDC_EDIT_SECTOR,          m_editSector);
    DDX_Control(pDX, IDC_BTN_GOTO,             m_btnGoto);
    DDX_Control(pDX, IDC_BTN_PREV,             m_btnPrev);
    DDX_Control(pDX, IDC_BTN_NEXT,             m_btnNext);
    DDX_Control(pDX, IDC_BTN_FIRST,            m_btnFirst);
    DDX_Control(pDX, IDC_BTN_LAST,             m_btnLast);
    DDX_Control(pDX, IDC_STATIC_SECTOR,        m_staticSector);
    DDX_Control(pDX, IDC_BTN_REFRESH,          m_btnRefresh);
    DDX_Control(pDX, IDC_STATIC_STATUS,        m_staticStatus);
}

void CDiskForgeDlg::OnInitialUpdate()
{
    CFormView::OnInitialUpdate();

    CRect rcHex;
    if (CWnd* pPh = GetDlgItem(IDC_HEXVIEW))
    {
        pPh->GetWindowRect(&rcHex);
        ScreenToClient(&rcHex);
        pPh->DestroyWindow();
    }
    else
    {
        CRect rcClient;
        GetClientRect(&rcClient);
        rcHex = CRect(8, 140, rcClient.right - 8, rcClient.bottom - 30);
    }
    m_hexView.Create(this, rcHex, IDC_HEXVIEW);

    m_editSector.SetWindowText(_T("0"));

    UpdateNavigationState();

    // Enumerate drives immediately on startup
    RefreshDriveList();
}

void CDiskForgeDlg::OnDestroy()
{
    m_diskManager.CloseDrive();
    CFormView::OnDestroy();
}

void CDiskForgeDlg::OnSize(UINT nType, int cx, int cy)
{
    CFormView::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0)
        LayoutControls(cx, cy);
}

void CDiskForgeDlg::LayoutControls(int cx, int cy)
{
    if (!m_comboDrives.GetSafeHwnd()) return;

    const int L = 8;
    const int R = cx - 8;

    m_comboDrives.MoveWindow(L, 8, R - L - 210, 26, TRUE);
    m_btnRefresh.MoveWindow(R - 200, 8, 90, 26, TRUE);
    m_btnOpen.MoveWindow(R - 100, 8, 100, 26, TRUE);

    m_staticDriveInfo.MoveWindow(L, 44, R - L, 48, TRUE);
    m_staticSector.MoveWindow(R - 200, 104, 200, 24, TRUE);

    m_hexView.MoveWindow(L, 140, R - L, cy - 140 - 30, TRUE);

    m_staticStatus.MoveWindow(L, cy - 26, R - L, 22, TRUE);
}

void CDiskForgeDlg::RefreshDriveList()
{
    SetStatus(_T("Enumerating physical drives..."), RGB(0, 0, 180));
    m_comboDrives.ResetContent();
    m_drives.clear();

    if (m_diskManager.IsOpen())
    {
        m_diskManager.CloseDrive();
        m_driveOpen = false;
    }

    bool found = m_diskManager.EnumerateDrives(m_drives);

    if (!found)
    {
        m_comboDrives.AddString(_T("No drives found (run as Administrator)"));
        SetStatus(_T("No physical drives found. Make sure you are running as Administrator."),
                  RGB(180, 0, 0));
    }
    else
    {
        for (auto& d : m_drives)
            m_comboDrives.AddString(d.displayName);

        m_comboDrives.SetCurSel(0);

        CString msg;
        msg.Format(_T("Found %d physical drive(s). Select one and click 'Open Drive'."),
                   (int)m_drives.size());
        SetStatus(msg);
    }

    m_staticDriveInfo.SetWindowText(_T("Select a drive above and click 'Open Drive'."));
    UpdateNavigationState();
    m_hexView.ClearData();
}

void CDiskForgeDlg::OnBtnRefresh()
{
    RefreshDriveList();
}

void CDiskForgeDlg::OnComboSelChange()
{
    // If a different drive is chosen, close current
    if (m_driveOpen)
    {
        m_diskManager.CloseDrive();
        m_driveOpen = false;
        m_hexView.ClearData();
        UpdateNavigationState();
    }
    UpdateDriveInfo();
}

void CDiskForgeDlg::UpdateDriveInfo()
{
    int sel = m_comboDrives.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        m_staticDriveInfo.SetWindowText(_T("No drive selected."));
        return;
    }

    const DriveInfo& di = m_drives[sel];
    CString info;
    info.Format(
        _T("Device: %s\r\n")
        _T("Model:  %s\r\n")
        _T("Capacity: %llu bytes  |  Sector size: %u bytes  |  Total sectors: %llu"),
        di.devicePath.GetString(),
        di.model.GetString(),
        di.totalBytes,
        di.bytesPerSector,
        di.totalSectors);
    m_staticDriveInfo.SetWindowText(info);
}

void CDiskForgeDlg::OnBtnOpen()
{
    int sel = m_comboDrives.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Please select a drive first."), MB_ICONWARNING);
        return;
    }

    const DriveInfo& di = m_drives[sel];

    if (m_driveOpen)
    {
        m_diskManager.CloseDrive();
        m_driveOpen = false;
    }

    SetStatus(_T("Opening drive..."), RGB(0, 0, 180));

    if (!m_diskManager.OpenDrive(di.devicePath))
    {
        CString err = m_diskManager.GetLastError();
        AfxMessageBox(err, MB_ICONERROR);
        SetStatus(_T("Failed to open drive. ") + err, RGB(180, 0, 0));
        return;
    }

    m_driveOpen = true;
    m_currentSector = 0;
    m_editSector.SetWindowText(_T("0"));

    UpdateDriveInfo();
    UpdateNavigationState();

    CString msg;
    msg.Format(_T("Drive opened successfully. %llu sectors, %u bytes/sector."),
               m_diskManager.GetTotalSectors(),
               m_diskManager.GetBytesPerSector());
    SetStatus(msg);

    // Load sector 0 immediately
    LoadSector(0);
}

void CDiskForgeDlg::LoadSector(ULONGLONG sectorNum)
{
    if (!m_driveOpen)
    {
        SetStatus(_T("No drive open."), RGB(180, 0, 0));
        return;
    }

    if (sectorNum >= m_diskManager.GetTotalSectors())
    {
        CString msg;
        msg.Format(_T("Sector %llu is out of range (max %llu)."),
                   sectorNum, m_diskManager.GetTotalSectors() - 1);
        SetStatus(msg, RGB(180, 0, 0));
        return;
    }

    SetStatus(_T("Reading sector..."), RGB(0, 0, 180));

    std::vector<BYTE> buf;
    if (!m_diskManager.ReadSector(sectorNum, buf))
    {
        CString err = m_diskManager.GetLastError();
        SetStatus(_T("Read error: ") + err, RGB(180, 0, 0));
        m_hexView.ClearData();
        return;
    }

    m_currentSector = sectorNum;

    // Compute byte offset of this sector
    ULONGLONG byteOffset = sectorNum * m_diskManager.GetBytesPerSector();
    m_hexView.SetData(buf, byteOffset);

    // Update edit box
    CString sectorStr;
    sectorStr.Format(_T("%llu"), sectorNum);
    m_editSector.SetWindowText(sectorStr);

    // Update sector label
    CString label;
    label.Format(_T("Sector: %llu / %llu"),
                 sectorNum, m_diskManager.GetTotalSectors() - 1);
    m_staticSector.SetWindowText(label);

    // Status
    CString status;
    status.Format(_T("Sector %llu loaded  (byte offset 0x%016I64X, size %u bytes)"),
                  sectorNum, byteOffset, (UINT)buf.size());
    SetStatus(status);

    UpdateNavigationState();
}

void CDiskForgeDlg::OnBtnGoto()
{
    CString text;
    m_editSector.GetWindowText(text);

    ULONGLONG sector = 0;
    // Support decimal or 0x hex input
    if (text.Left(2).CompareNoCase(_T("0x")) == 0)
        sector = _tcstoui64(text.GetString() + 2, nullptr, 16);
    else
        sector = _tcstoui64(text.GetString(), nullptr, 10);

    LoadSector(sector);
}

void CDiskForgeDlg::OnEditSectorChange()
{
    // Allow pressing Enter in edit box to trigger goto
}

void CDiskForgeDlg::OnBtnPrev()
{
    if (m_currentSector > 0)
        LoadSector(m_currentSector - 1);
}

void CDiskForgeDlg::OnBtnNext()
{
    if (m_driveOpen && m_currentSector + 1 < m_diskManager.GetTotalSectors())
        LoadSector(m_currentSector + 1);
}

void CDiskForgeDlg::OnBtnFirst()
{
    LoadSector(0);
}

void CDiskForgeDlg::OnBtnLast()
{
    if (m_driveOpen && m_diskManager.GetTotalSectors() > 0)
        LoadSector(m_diskManager.GetTotalSectors() - 1);
}

void CDiskForgeDlg::OnBtnPartitions()
{
    // Independent of the sector-viewer's currently open drive - like every
    // other sidebar feature, this opens its own drive selection rather than
    // requiring "Open Drive" to have been clicked first. (Previously this was
    // the one feature still tied to m_diskManager's shared open state, which
    // left it looking permanently disabled in the new sidebar layout - this
    // makes it consistent with the other ten.)
    int sel = m_comboDrives.GetCurSel();
    if (sel < 0)
    {
        AfxMessageBox(_T("Select a drive from the list above first."), MB_ICONWARNING);
        return;
    }

    std::vector<DriveInfo> drives;
    CDiskManager enumDm;
    enumDm.EnumerateDrives(drives);
    if (sel >= (int)drives.size())
    {
        AfxMessageBox(_T("Drive list is out of date - click Refresh and try again."), MB_ICONWARNING);
        return;
    }

    CDiskManager dm;
    if (!dm.OpenDrive(drives[sel].devicePath))
    {
        AfxMessageBox(_T("Could not open the drive:\r\n") + dm.GetLastError(), MB_ICONERROR);
        return;
    }

    // Read-only: the dialog only ever reads sectors via dm.
    CPartitionDialog dlg(dm, this);
    bool gotSelection = (dlg.DoModal() == IDOK && dlg.HasSelection());
    ULONGLONG selectedSector = gotSelection ? dlg.GetSelectedSector() : 0;
    dm.CloseDrive();

    // If the sector viewer already has this same drive open, jump it there
    // too, since that's a nice bonus - but it's not required for Partitions to work.
    if (gotSelection && m_driveOpen)
        LoadSector(selectedSector);
}

void CDiskForgeDlg::OnBtnDiskCopy()
{
    // Independent of the sector-viewer's currently open drive - this dialog
    // enumerates and opens drives on its own.
    CDiskCopyDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnBadSectorScan()
{
    // Also independent of the sector-viewer's currently open drive, and
    // purely read-only - no confirmation gate needed to open it.
    CBadSectorScanDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnSmartHealth()
{
    // Purely read-only (IOCTL queries only, GENERIC_READ handle) - no
    // confirmation gate needed.
    CSmartHealthDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnMbrGptConvert()
{
    // Write-capable (rewrites the partition table). The dialog itself
    // enforces Analyze -> mandatory backup -> type-to-confirm before any
    // write happens - independent of the sector-viewer's currently open drive.
    CMbrGptConvertDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnPartitionEdit()
{
    // Write-capable (rewrites one partition table entry per Apply, plus
    // drive-letter changes via the Mount Manager). Confirms each change
    // individually before writing - independent of the sector-viewer's
    // currently open drive.
    CPartitionEditDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnWipeDisk()
{
    // Write-capable (overwrites an entire disk or partition). Enforces a
    // type-to-confirm gate before any write - independent of the sector-
    // viewer's currently open drive.
    CWipeDiskDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnEfiBoot()
{
    // Write-capable, but touches NVRAM boot variables only - no disk sector
    // I/O at all. Independent of the sector-viewer's currently open drive.
    CEfiBootDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnLostPartition()
{
    // Purely read-only (signature scan of unallocated space only) - no
    // confirmation gate needed. Independent of the sector-viewer's
    // currently open drive.
    CLostPartitionDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnFileRecovery()
{
    // Read-only against the source disk (FAT32 only) - it only ever writes
    // to a NEW output file the user chooses via Save As, never back to the
    // source drive. Independent of the sector-viewer's currently open drive.
    CFileRecoveryDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnFileUnlocker()
{
    // A completely different subsystem from the rest of this app - works at
    // the process/file-handle level via Restart Manager, never touches disk
    // sectors or partition tables. Its only "write" is terminating another
    // process, which the dialog itself confirms before doing.
    CFileUnlockerDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnStartup()
{
    CStartupManagerDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::OnBtnRegScan()
{
    CRegistryScannerDialog dlg(this);
    dlg.DoModal();
}

void CDiskForgeDlg::UpdateNavigationState()
{
    bool open = m_driveOpen;

    // Note: m_btnPartitions is deliberately NOT gated on `open` - like every
    // other sidebar feature, it manages its own drive selection independently
    // (see OnBtnPartitions) and should always be available.
    m_btnGoto.EnableWindow(open);
    m_btnFirst.EnableWindow(open);
    m_btnLast.EnableWindow(open);
    m_btnPrev.EnableWindow(open && m_currentSector > 0);
    m_btnNext.EnableWindow(open && m_diskManager.IsOpen() &&
                            m_currentSector + 1 < m_diskManager.GetTotalSectors());
    m_editSector.EnableWindow(open);
}

void CDiskForgeDlg::SetStatus(const CString& msg, COLORREF /*color*/)
{
    if (m_staticStatus.GetSafeHwnd())
        m_staticStatus.SetWindowText(msg);
}
