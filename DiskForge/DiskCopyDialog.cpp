// DiskCopyDialog.cpp
#include "pch.h"
#include "DiskCopyDialog.h"
#include "PartitionDialog.h"
#include "ConfirmTextDialog.h"
#include "SystemDiskGuard.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    // Extracts "PhysicalDriveN" from "\\.\PhysicalDriveN" for display/typed confirmation.
    CString ShortDriveName(const CString& devicePath)
    {
        int pos = devicePath.ReverseFind(_T('\\'));
        return (pos >= 0) ? devicePath.Mid(pos + 1) : devicePath;
    }
}

BEGIN_MESSAGE_MAP(CDiskCopyDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_DC_COMBO_SRC, &CDiskCopyDialog::OnComboSrcChange)
    ON_CBN_SELCHANGE(IDC_DC_COMBO_DST, &CDiskCopyDialog::OnComboDstChange)
    ON_BN_CLICKED(IDC_DC_RADIO_SRC_WHOLE, &CDiskCopyDialog::OnRadioSrcMode)
    ON_BN_CLICKED(IDC_DC_RADIO_SRC_PART,  &CDiskCopyDialog::OnRadioSrcMode)
    ON_BN_CLICKED(IDC_DC_RADIO_SRC_TYPE_DRIVE, &CDiskCopyDialog::OnRadioSrcTypeMode)
    ON_BN_CLICKED(IDC_DC_RADIO_SRC_TYPE_FILE,  &CDiskCopyDialog::OnRadioSrcTypeMode)
    ON_BN_CLICKED(IDC_DC_RADIO_DST_DISK,  &CDiskCopyDialog::OnRadioDstMode)
    ON_BN_CLICKED(IDC_DC_RADIO_DST_FILE,  &CDiskCopyDialog::OnRadioDstMode)
    ON_BN_CLICKED(IDC_DC_RADIO_DST_WHOLE, &CDiskCopyDialog::OnRadioDstRangeMode)
    ON_BN_CLICKED(IDC_DC_RADIO_DST_PART,  &CDiskCopyDialog::OnRadioDstRangeMode)
    ON_BN_CLICKED(IDC_DC_BTN_CHOOSE_PART, &CDiskCopyDialog::OnBtnChoosePartition)
    ON_BN_CLICKED(IDC_DC_BTN_CHOOSE_DST_PART, &CDiskCopyDialog::OnBtnChooseDstPartition)
    ON_BN_CLICKED(IDC_DC_BTN_BROWSE,      &CDiskCopyDialog::OnBtnBrowseDestFile)
    ON_BN_CLICKED(IDC_DC_BTN_BROWSE_SRC,  &CDiskCopyDialog::OnBtnBrowseSrcFile)
    ON_BN_CLICKED(IDC_DC_BTN_REFRESH,     &CDiskCopyDialog::OnBtnRefresh)
    ON_BN_CLICKED(IDC_DC_BTN_START,       &CDiskCopyDialog::OnBtnStart)
    ON_BN_CLICKED(IDC_DC_BTN_CANCEL,      &CDiskCopyDialog::OnBtnCancelCopy)
    ON_BN_CLICKED(IDC_DC_BTN_CLOSE,       &CDiskCopyDialog::OnBtnClose)
END_MESSAGE_MAP()

CDiskCopyDialog::CDiskCopyDialog(CWnd* pParent)
    : CDialogEx(IDD_DISKCOPY, pParent)
    , m_hasChosenPartition(false)
    , m_hasChosenDstPartition(false)
    , m_copyRunning(false)
    , m_pThread(nullptr)
{
}

CDiskCopyDialog::~CDiskCopyDialog()
{
    if (m_copyRunning)
    {
        // Should not normally happen (OnCancel blocks this), but never leak a
        // running background copy against a dialog that's about to be destroyed.
        m_engine.RequestCancel();
    }
}

void CDiskCopyDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DC_STATIC_SRC_HDR,        m_staticSrcHdr);
    DDX_Control(pDX, IDC_DC_STATIC_DST_HDR,        m_staticDstHdr);
    DDX_Control(pDX, IDC_DC_RADIO_SRC_TYPE_DRIVE, m_radioSrcTypeDrive);
    DDX_Control(pDX, IDC_DC_RADIO_SRC_TYPE_FILE,  m_radioSrcTypeFile);
    DDX_Control(pDX, IDC_DC_COMBO_SRC,            m_comboSrc);
    DDX_Control(pDX, IDC_DC_RADIO_SRC_WHOLE,      m_radioSrcWhole);
    DDX_Control(pDX, IDC_DC_RADIO_SRC_PART,       m_radioSrcPart);
    DDX_Control(pDX, IDC_DC_BTN_CHOOSE_PART,      m_btnChoosePart);
    DDX_Control(pDX, IDC_DC_STATIC_PART,          m_staticPart);
    DDX_Control(pDX, IDC_DC_EDIT_SRC_FILE,        m_editSrcFile);
    DDX_Control(pDX, IDC_DC_BTN_BROWSE_SRC,       m_btnBrowseSrc);
    DDX_Control(pDX, IDC_DC_RADIO_DST_DISK,       m_radioDstDisk);
    DDX_Control(pDX, IDC_DC_RADIO_DST_FILE,       m_radioDstFile);
    DDX_Control(pDX, IDC_DC_COMBO_DST,            m_comboDst);
    DDX_Control(pDX, IDC_DC_EDIT_DST_FILE,        m_editDstFile);
    DDX_Control(pDX, IDC_DC_BTN_BROWSE,           m_btnBrowse);
    DDX_Control(pDX, IDC_DC_RADIO_DST_WHOLE,      m_radioDstWhole);
    DDX_Control(pDX, IDC_DC_RADIO_DST_PART,       m_radioDstPart);
    DDX_Control(pDX, IDC_DC_BTN_CHOOSE_DST_PART,  m_btnChooseDstPart);
    DDX_Control(pDX, IDC_DC_STATIC_DST_PART,      m_staticDstPart);
    DDX_Control(pDX, IDC_DC_BTN_REFRESH,          m_btnRefresh);
    DDX_Control(pDX, IDC_DC_STATIC_SUMMARY,       m_staticSummary);
    DDX_Control(pDX, IDC_DC_PROGRESS,             m_progress);
    DDX_Control(pDX, IDC_DC_STATIC_STATUS,        m_staticStatus);
    DDX_Control(pDX, IDC_DC_BTN_START,            m_btnStart);
    DDX_Control(pDX, IDC_DC_BTN_CANCEL,           m_btnCancelCopy);
    DDX_Control(pDX, IDC_DC_BTN_CLOSE,            m_btnClose);
}

BOOL CDiskCopyDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Disk Copy / Restore Image"));
    CenterWindow(GetParent());

    m_radioSrcTypeDrive.SetCheck(BST_CHECKED);
    m_radioSrcWhole.SetCheck(BST_CHECKED);
    m_radioDstFile.SetCheck(BST_CHECKED); // Default to the safer option
    m_radioDstWhole.SetCheck(BST_CHECKED);
    m_btnChoosePart.EnableWindow(FALSE);
    m_btnChooseDstPart.EnableWindow(FALSE);
    m_progress.SetRange(0, 1000);
    m_btnCancelCopy.EnableWindow(FALSE);

    RefreshDriveLists();
    UpdateModeEnablement();
    UpdateSummary();

    return TRUE;
}

void CDiskCopyDialog::RefreshDriveLists()
{
    CDiskManager dm;
    m_drives.clear();
    dm.EnumerateDrives(m_drives);

    CString prevSrc, prevDst;
    if (m_comboSrc.GetCurSel() >= 0) m_comboSrc.GetLBText(m_comboSrc.GetCurSel(), prevSrc);
    if (m_comboDst.GetCurSel() >= 0) m_comboDst.GetLBText(m_comboDst.GetCurSel(), prevDst);

    m_comboSrc.ResetContent();
    m_comboDst.ResetContent();

    for (const auto& d : m_drives)
    {
        CString label;
        label.Format(_T("%s - %s (%s)"), ShortDriveName(d.devicePath).GetString(),
                      d.model.GetString(), CPartitionParser::FormatSize(d.totalBytes).GetString());
        m_comboSrc.AddString(label);
        m_comboDst.AddString(label);
    }

    if (m_comboSrc.SelectString(-1, prevSrc) == CB_ERR && m_comboSrc.GetCount() > 0)
        m_comboSrc.SetCurSel(0);
    if (m_comboDst.SelectString(-1, prevDst) == CB_ERR && m_comboDst.GetCount() > 0)
        m_comboDst.SetCurSel(m_comboDst.GetCount() > 1 ? 1 : 0); // default to a different drive than source, if possible

    m_hasChosenPartition = false;
    UpdateChosenPartitionLabel();
    m_hasChosenDstPartition = false;
    UpdateChosenDstPartitionLabel();
}

void CDiskCopyDialog::UpdateChosenPartitionLabel()
{
    if (m_radioSrcTypeFile.GetCheck() == BST_CHECKED)
    {
        CString path;
        m_editSrcFile.GetWindowText(path);
        if (path.IsEmpty())
        {
            m_staticPart.SetWindowText(_T("No image file chosen yet - click \"Browse...\""));
        }
        else
        {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            if (GetFileAttributesEx(path, GetFileExInfoStandard, &fad))
            {
                ULARGE_INTEGER size;
                size.LowPart = fad.nFileSizeLow;
                size.HighPart = fad.nFileSizeHigh;
                CString s;
                s.Format(_T("Image file: %s (%s)"), path.GetString(),
                          CPartitionParser::FormatSize(size.QuadPart).GetString());
                m_staticPart.SetWindowText(s);
            }
            else
            {
                m_staticPart.SetWindowText(_T("That file could not be found."));
            }
        }
        return;
    }

    if (!m_hasChosenPartition)
    {
        m_staticPart.SetWindowText(
            (m_radioSrcPart.GetCheck() == BST_CHECKED)
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

void CDiskCopyDialog::UpdateModeEnablement()
{
    bool srcFile = (m_radioSrcTypeFile.GetCheck() == BST_CHECKED);

    m_comboSrc.ShowWindow(srcFile ? SW_HIDE : SW_SHOW);
    m_comboSrc.EnableWindow(!srcFile);
    m_editSrcFile.ShowWindow(srcFile ? SW_SHOW : SW_HIDE);
    m_editSrcFile.EnableWindow(srcFile);
    m_btnBrowseSrc.ShowWindow(srcFile ? SW_SHOW : SW_HIDE);
    m_btnBrowseSrc.EnableWindow(srcFile);

    m_radioSrcWhole.ShowWindow(srcFile ? SW_HIDE : SW_SHOW);
    m_radioSrcPart.ShowWindow(srcFile ? SW_HIDE : SW_SHOW);

    bool srcPart = !srcFile && (m_radioSrcPart.GetCheck() == BST_CHECKED);
    m_btnChoosePart.ShowWindow(srcFile ? SW_HIDE : SW_SHOW);
    m_btnChoosePart.EnableWindow(srcPart);

    bool dstDisk = (m_radioDstDisk.GetCheck() == BST_CHECKED);
    m_comboDst.ShowWindow(dstDisk ? SW_SHOW : SW_HIDE);
    m_comboDst.EnableWindow(dstDisk);
    m_editDstFile.ShowWindow(dstDisk ? SW_HIDE : SW_SHOW);
    m_btnBrowse.ShowWindow(dstDisk ? SW_HIDE : SW_SHOW);
    m_editDstFile.EnableWindow(!dstDisk);
    m_btnBrowse.EnableWindow(!dstDisk);

    m_radioDstWhole.ShowWindow(dstDisk ? SW_SHOW : SW_HIDE);
    m_radioDstPart.ShowWindow(dstDisk ? SW_SHOW : SW_HIDE);
    bool dstPart = dstDisk && (m_radioDstPart.GetCheck() == BST_CHECKED);
    m_btnChooseDstPart.ShowWindow(dstDisk ? SW_SHOW : SW_HIDE);
    m_btnChooseDstPart.EnableWindow(dstPart);
    m_staticDstPart.ShowWindow(dstPart ? SW_SHOW : SW_HIDE);
}

void CDiskCopyDialog::OnRadioSrcTypeMode()
{
    m_hasChosenPartition = false;
    if (m_radioSrcTypeFile.GetCheck() == BST_CHECKED)
    {
        // Restoring an image only makes sense onto a physical disk.
        m_radioDstDisk.SetCheck(BST_CHECKED);
        m_radioDstFile.SetCheck(BST_UNCHECKED);
    }
    UpdateModeEnablement();
    UpdateChosenPartitionLabel();
    UpdateSummary();
}

void CDiskCopyDialog::OnRadioSrcMode()
{
    if (m_radioSrcWhole.GetCheck() == BST_CHECKED)
        m_hasChosenPartition = false;
    UpdateModeEnablement();
    UpdateChosenPartitionLabel();
    UpdateSummary();
}

void CDiskCopyDialog::OnRadioDstMode()
{
    UpdateModeEnablement();
    UpdateSummary();
}

void CDiskCopyDialog::OnRadioDstRangeMode()
{
    if (m_radioDstWhole.GetCheck() == BST_CHECKED)
        m_hasChosenDstPartition = false;
    UpdateModeEnablement();
    UpdateChosenDstPartitionLabel();
    UpdateSummary();
}

void CDiskCopyDialog::UpdateChosenDstPartitionLabel()
{
    if (!m_hasChosenDstPartition)
    {
        m_staticDstPart.SetWindowText(_T("No destination partition chosen yet - click \"Choose Destination Partition...\""));
        return;
    }
    CString s;
    s.Format(_T("Will write onto partition #%d: %s, start LBA %llu, %s"),
              m_chosenDstPartition.index, m_chosenDstPartition.typeName.GetString(),
              m_chosenDstPartition.startLBA, CPartitionParser::FormatSize(m_chosenDstPartition.sizeBytes).GetString());
    m_staticDstPart.SetWindowText(s);
}

void CDiskCopyDialog::OnComboSrcChange()
{
    m_hasChosenPartition = false;
    UpdateChosenPartitionLabel();
    UpdateSummary();
}

void CDiskCopyDialog::OnComboDstChange()
{
    m_hasChosenDstPartition = false;
    UpdateChosenDstPartitionLabel();
    UpdateSummary();
}

void CDiskCopyDialog::OnBtnBrowseSrcFile()
{
    CFileDialog dlg(TRUE, _T("img"), nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        _T("Disk image files (*.img;*.dd)|*.img;*.dd|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() == IDOK)
    {
        m_editSrcFile.SetWindowText(dlg.GetPathName());
        UpdateChosenPartitionLabel();
        UpdateSummary();
    }
}

void CDiskCopyDialog::OnBtnChooseDstPartition()
{
    int sel = m_comboDst.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a destination drive first."), MB_ICONWARNING);
        return;
    }

    CDiskManager dm;
    if (!dm.OpenDrive(m_drives[sel].devicePath))
    {
        AfxMessageBox(_T("Could not open the destination drive to read its partition table:\r\n") + dm.GetLastError(),
                      MB_ICONERROR);
        return;
    }

    CPartitionDialog dlg(dm, this);
    if (dlg.DoModal() == IDOK)
    {
        PartitionEntry pe;
        if (dlg.GetSelectedPartitionEntry(pe))
        {
            m_chosenDstPartition = pe;
            m_hasChosenDstPartition = true;
        }
    }
    dm.CloseDrive();

    UpdateChosenDstPartitionLabel();
    UpdateSummary();
}

void CDiskCopyDialog::OnBtnChoosePartition()
{
    int sel = m_comboSrc.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
    {
        AfxMessageBox(_T("Select a source drive first."), MB_ICONWARNING);
        return;
    }

    CDiskManager dm;
    if (!dm.OpenDrive(m_drives[sel].devicePath))
    {
        AfxMessageBox(_T("Could not open the source drive to read its partition table:\r\n") + dm.GetLastError(),
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

void CDiskCopyDialog::OnBtnBrowseDestFile()
{
    CFileDialog dlg(FALSE, _T("img"), nullptr,
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        _T("Disk image files (*.img;*.dd)|*.img;*.dd|All files (*.*)|*.*||"), this);
    if (dlg.DoModal() == IDOK)
    {
        m_editDstFile.SetWindowText(dlg.GetPathName());
        UpdateSummary();
    }
}

void CDiskCopyDialog::OnBtnRefresh()
{
    RefreshDriveLists();
    UpdateSummary();
}

void CDiskCopyDialog::UpdateSummary()
{
    CString summary;
    bool srcIsFile = (m_radioSrcTypeFile.GetCheck() == BST_CHECKED);

    ULONGLONG requiredBytes = 0;
    CString srcDescForOverlapCheck; // devicePath when source is a drive, empty otherwise

    if (srcIsFile)
    {
        CString path;
        m_editSrcFile.GetWindowText(path);
        if (path.IsEmpty())
        {
            m_staticSummary.SetWindowText(_T("Choose a source image file."));
            return;
        }
        WIN32_FILE_ATTRIBUTE_DATA fad = {};
        if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fad))
        {
            m_staticSummary.SetWindowText(_T("That source file could not be found."));
            return;
        }
        ULARGE_INTEGER size;
        size.LowPart = fad.nFileSizeLow;
        size.HighPart = fad.nFileSizeHigh;
        requiredBytes = size.QuadPart;

        summary.Format(_T("This will restore %s from the image file onto the selected disk."),
                        CPartitionParser::FormatSize(requiredBytes).GetString());
    }
    else
    {
        int srcSel = m_comboSrc.GetCurSel();
        if (srcSel < 0 || srcSel >= (int)m_drives.size())
        {
            m_staticSummary.SetWindowText(_T("Select a source drive."));
            return;
        }

        const DriveInfo& src = m_drives[srcSel];
        srcDescForOverlapCheck = src.devicePath;
        requiredBytes = (m_radioSrcPart.GetCheck() == BST_CHECKED && m_hasChosenPartition)
            ? m_chosenPartition.sizeBytes
            : src.totalBytes;

        summary.Format(_T("This will copy %s "), CPartitionParser::FormatSize(requiredBytes).GetString());
        summary += (m_radioSrcPart.GetCheck() == BST_CHECKED) ? _T("(selected partition)") : _T("(entire disk)");
    }

    if (m_radioDstDisk.GetCheck() == BST_CHECKED)
    {
        int dstSel = m_comboDst.GetCurSel();
        if (dstSel >= 0 && dstSel < (int)m_drives.size())
        {
            const DriveInfo& dst = m_drives[dstSel];
            bool dstIsPart = (m_radioDstPart.GetCheck() == BST_CHECKED);
            ULONGLONG destAvailBytes = (dstIsPart && m_hasChosenDstPartition)
                ? m_chosenDstPartition.sizeBytes
                : dst.totalBytes;

            if (dstIsPart && !m_hasChosenDstPartition)
            {
                summary += _T("\r\nChoose a destination partition.");
            }
            else
            {
                summary.AppendFormat(_T("\r\n%s capacity: %s"),
                                      dstIsPart ? _T("Destination partition") : _T("Destination disk"),
                                      CPartitionParser::FormatSize(destAvailBytes).GetString());

                if (!srcIsFile && dst.devicePath.CompareNoCase(srcDescForOverlapCheck) == 0)
                    summary += _T("\r\n\u26A0 Source and destination are the same drive - not allowed.");
                else if (CSystemDiskGuard::IsProtectedDrive(dst.driveIndex))
                    summary += _T("\r\n\u26A0 This is the drive Windows is running from - it cannot be a destination.");
                else if (requiredBytes > destAvailBytes)
                    summary += _T("\r\n\u26A0 Destination is too small for this copy.");
                else if (!srcIsFile && !dstIsPart)
                {
                    int srcSel = m_comboSrc.GetCurSel();
                    const DriveInfo& src = m_drives[srcSel];
                    if (src.bytesPerSector != dst.bytesPerSector)
                        summary.AppendFormat(_T("\r\n\u26A0 Sector size mismatch (%u vs %u) - disk-to-disk copy needs matching sector sizes."),
                                              src.bytesPerSector, dst.bytesPerSector);
                    else
                        summary += _T("\r\n\u26A0 EVERYTHING on the destination disk will be permanently overwritten.");
                }
                else if (dstIsPart)
                {
                    summary += _T("\r\n\u26A0 Everything on the destination PARTITION will be overwritten. ")
                               _T("Other partitions on this disk are not touched.");
                }
                else
                {
                    summary += _T("\r\n\u26A0 EVERYTHING on the destination disk will be permanently overwritten.");
                }
            }
        }
        else
        {
            summary += _T("\r\nSelect a destination drive.");
        }
    }
    else if (srcIsFile)
    {
        summary += _T("\r\nRestoring an image file requires a physical disk destination.");
    }
    else
    {
        CString path;
        m_editDstFile.GetWindowText(path);
        if (path.IsEmpty())
            summary += _T("\r\nChoose a destination file.");
        else if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
            summary += _T("\r\n\u26A0 That file already exists and will be overwritten.");
    }

    m_staticSummary.SetWindowText(summary);
}

bool CDiskCopyDialog::BuildJob(DiskCopyJob& job, CString& errorOut)
{
    bool srcIsFile = (m_radioSrcTypeFile.GetCheck() == BST_CHECKED);

    if (srcIsFile)
    {
        CString path;
        m_editSrcFile.GetWindowText(path);
        if (path.IsEmpty())
        {
            errorOut = _T("Choose a source image file.");
            return false;
        }

        WIN32_FILE_ATTRIBUTE_DATA fad = {};
        if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fad))
        {
            errorOut = _T("That source image file could not be found.");
            return false;
        }

        ULARGE_INTEGER size;
        size.LowPart = fad.nFileSizeLow;
        size.HighPart = fad.nFileSizeHigh;
        if (size.QuadPart == 0)
        {
            errorOut = _T("That source image file is empty.");
            return false;
        }

        job.sourceKind      = CopySourceKind::ImageFile;
        job.sourceFilePath  = path;
        job.sourceFileBytes = size.QuadPart;
    }
    else
    {
        int srcSel = m_comboSrc.GetCurSel();
        if (srcSel < 0 || srcSel >= (int)m_drives.size())
        {
            errorOut = _T("Select a source drive.");
            return false;
        }
        const DriveInfo& src = m_drives[srcSel];

        job.sourceKind       = CopySourceKind::PhysicalDrive;
        job.sourceDevicePath = src.devicePath;
        job.sourceDriveIndex = src.driveIndex;
        job.sourceSectorSize = src.bytesPerSector;

        if (m_radioSrcPart.GetCheck() == BST_CHECKED)
        {
            if (!m_hasChosenPartition)
            {
                errorOut = _T("Choose a partition to copy, or switch to \"Whole disk\".");
                return false;
            }
            job.sourceStartLBA    = m_chosenPartition.startLBA;
            job.sourceSectorCount = m_chosenPartition.sectorCount;
        }
        else
        {
            job.sourceStartLBA    = 0;
            job.sourceSectorCount = src.totalSectors;
        }
    }

    if (m_radioDstDisk.GetCheck() == BST_CHECKED)
    {
        int dstSel = m_comboDst.GetCurSel();
        if (dstSel < 0 || dstSel >= (int)m_drives.size())
        {
            errorOut = _T("Select a destination drive.");
            return false;
        }
        const DriveInfo& dst = m_drives[dstSel];

        job.destKind        = CopyDestKind::Disk;
        job.destDevicePath  = dst.devicePath;
        job.destDriveIndex  = dst.driveIndex;
        job.destSectorSize  = dst.bytesPerSector;

        if (m_radioDstPart.GetCheck() == BST_CHECKED)
        {
            if (!m_hasChosenDstPartition)
            {
                errorOut = _T("Choose a destination partition, or switch to \"Write to whole disk\".");
                return false;
            }
            job.destStartLBA    = m_chosenDstPartition.startLBA;
            job.destTotalSectors = m_chosenDstPartition.sectorCount;
        }
        else
        {
            job.destStartLBA    = 0;
            job.destTotalSectors = dst.totalSectors;
        }
    }
    else
    {
        if (srcIsFile)
        {
            errorOut = _T("Restoring an image file requires a physical disk destination.");
            return false;
        }

        CString path;
        m_editDstFile.GetWindowText(path);
        if (path.IsEmpty())
        {
            errorOut = _T("Choose a destination file.");
            return false;
        }
        job.destKind     = CopyDestKind::ImageFile;
        job.destFilePath = path;
    }

    return true;
}

void CDiskCopyDialog::SetUiEnabled(bool enabled)
{
    m_radioSrcTypeDrive.EnableWindow(enabled);
    m_radioSrcTypeFile.EnableWindow(enabled);
    bool srcIsFile = (m_radioSrcTypeFile.GetCheck() == BST_CHECKED);
    m_comboSrc.EnableWindow(enabled && !srcIsFile);
    m_editSrcFile.EnableWindow(enabled && srcIsFile);
    m_btnBrowseSrc.EnableWindow(enabled && srcIsFile);
    m_radioSrcWhole.EnableWindow(enabled);
    m_radioSrcPart.EnableWindow(enabled);
    m_btnChoosePart.EnableWindow(enabled && !srcIsFile && m_radioSrcPart.GetCheck() == BST_CHECKED);
    m_radioDstDisk.EnableWindow(enabled);
    m_radioDstFile.EnableWindow(enabled);
    m_comboDst.EnableWindow(enabled && m_radioDstDisk.GetCheck() == BST_CHECKED);
    m_editDstFile.EnableWindow(enabled && m_radioDstDisk.GetCheck() != BST_CHECKED);
    m_btnBrowse.EnableWindow(enabled && m_radioDstDisk.GetCheck() != BST_CHECKED);
    bool dstDiskNow = (m_radioDstDisk.GetCheck() == BST_CHECKED);
    m_radioDstWhole.EnableWindow(enabled && dstDiskNow);
    m_radioDstPart.EnableWindow(enabled && dstDiskNow);
    m_btnChooseDstPart.EnableWindow(enabled && dstDiskNow && m_radioDstPart.GetCheck() == BST_CHECKED);
    m_btnRefresh.EnableWindow(enabled);
    m_btnStart.EnableWindow(enabled);
    m_btnClose.EnableWindow(enabled);
    m_btnCancelCopy.EnableWindow(!enabled);
}

void CDiskCopyDialog::SetStatus(const CString& msg)
{
    m_staticStatus.SetWindowText(msg);
}

UINT __cdecl CDiskCopyDialog::CopyThreadProc(LPVOID pParam)
{
    // The DiskCopyJob is heap-allocated by the caller and owned by this thread.
    auto* jobPtr = static_cast<std::pair<CDiskCopyEngine*, DiskCopyJob*>*>(pParam);
    CDiskCopyEngine* engine = jobPtr->first;
    DiskCopyJob* job = jobPtr->second;

    engine->Run(*job);

    delete job;
    delete jobPtr;
    return 0;
}

void CDiskCopyDialog::OnBtnStart()
{
    DiskCopyJob job;
    CString buildError;
    if (!BuildJob(job, buildError))
    {
        AfxMessageBox(buildError, MB_ICONWARNING);
        return;
    }

    CString validationError = ValidateDiskCopyJob(job);
    if (!validationError.IsEmpty())
    {
        AfxMessageBox(validationError, MB_ICONERROR);
        return;
    }

    // Build the warning + figure out the required typed confirmation phrase.
    ULONGLONG totalBytes = (job.sourceKind == CopySourceKind::ImageFile)
        ? job.sourceFileBytes
        : job.sourceSectorCount * (ULONGLONG)job.sourceSectorSize;

    CString sourceDesc;
    if (job.sourceKind == CopySourceKind::ImageFile)
        sourceDesc.Format(_T("the image file \"%s\""), job.sourceFilePath.GetString());
    else if (m_radioSrcPart.GetCheck() == BST_CHECKED)
        sourceDesc = CString(_T("partition ")) + m_chosenPartition.typeName;
    else
        sourceDesc.Format(_T("the entire disk (%s)"), ShortDriveName(job.sourceDevicePath).GetString());

    CString message;
    CString requiredPhrase;
    bool needsTypedConfirm = true;

    if (job.destKind == CopyDestKind::Disk)
    {
        CString driveName = ShortDriveName(job.destDevicePath);
        if (job.destStartLBA == 0)
        {
            message.Format(
                _T("You are about to restore/copy %s (%s) onto %s.\r\n\r\n")
                _T("ALL existing data on %s will be permanently erased and replaced.\r\n")
                _T("This cannot be undone."),
                sourceDesc.GetString(), CPartitionParser::FormatSize(totalBytes).GetString(),
                driveName.GetString(), driveName.GetString());
            requiredPhrase = _T("ERASE ") + driveName;
        }
        else
        {
            message.Format(
                _T("You are about to restore/copy %s (%s) onto a specific partition of %s ")
                _T("(partition #%d, starting at LBA %llu).\r\n\r\n")
                _T("ALL existing data in that partition will be permanently erased and replaced. ")
                _T("Other partitions on this disk are not touched.\r\nThis cannot be undone."),
                sourceDesc.GetString(), CPartitionParser::FormatSize(totalBytes).GetString(),
                driveName.GetString(), m_chosenDstPartition.index, job.destStartLBA);
            requiredPhrase.Format(_T("OVERWRITE %s PART %d"), driveName.GetString(), m_chosenDstPartition.index);
        }
    }
    else
    {
        bool fileExists = (GetFileAttributes(job.destFilePath) != INVALID_FILE_ATTRIBUTES);
        if (!fileExists)
        {
            // No existing data at risk - a light confirmation is enough.
            CString msg;
            msg.Format(_T("Copy %s (%s) to:\r\n%s\r\n\r\nStart now?"),
                        sourceDesc.GetString(), CPartitionParser::FormatSize(totalBytes).GetString(),
                        job.destFilePath.GetString());
            if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
                return;
            needsTypedConfirm = false;
        }
        else
        {
            message.Format(
                _T("You are about to copy %s (%s) to:\r\n%s\r\n\r\n")
                _T("That file already exists and will be OVERWRITTEN."),
                sourceDesc.GetString(), CPartitionParser::FormatSize(totalBytes).GetString(),
                job.destFilePath.GetString());
            requiredPhrase = _T("OVERWRITE");
        }
    }

    if (needsTypedConfirm)
    {
        CConfirmTextDialog confirmDlg(message, requiredPhrase, this);
        if (confirmDlg.DoModal() != IDOK)
            return;
    }

    // Re-validate right before starting - state may have changed while the
    // confirmation dialog was open (e.g. a drive was unplugged).
    validationError = ValidateDiskCopyJob(job);
    if (!validationError.IsEmpty())
    {
        AfxMessageBox(validationError, MB_ICONERROR);
        return;
    }

    m_progress.SetPos(0);
    SetStatus(_T("Starting..."));
    SetUiEnabled(false);
    m_copyRunning = true;

    auto* jobHeap = new DiskCopyJob(job);
    auto* threadParam = new std::pair<CDiskCopyEngine*, DiskCopyJob*>(&m_engine, jobHeap);
    m_pThread = AfxBeginThread(&CDiskCopyDialog::CopyThreadProc, threadParam);

    SetTimer(TIMER_ID, 250, nullptr);
}

void CDiskCopyDialog::OnBtnCancelCopy()
{
    m_btnCancelCopy.EnableWindow(FALSE);
    SetStatus(_T("Cancelling..."));
    m_engine.RequestCancel();
}

void CDiskCopyDialog::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        DiskCopyProgress p = m_engine.GetProgressSnapshot();

        if (p.bytesTotal > 0)
        {
            int permille = static_cast<int>((p.bytesCopied * 1000ULL) / p.bytesTotal);
            m_progress.SetPos(permille);
        }
        SetStatus(p.statusText);

        if (p.finished)
        {
            KillTimer(TIMER_ID);
            m_copyRunning = false;
            SetUiEnabled(true);

            if (p.success)
            {
                m_progress.SetPos(1000);
                SetStatus(_T("Done."));
                AfxMessageBox(_T("Copy completed successfully."), MB_ICONINFORMATION);
            }
            else if (p.canceled)
            {
                SetStatus(_T("Cancelled."));
                AfxMessageBox(_T("Copy was cancelled. Note: if the destination was a physical disk, ")
                              _T("it may now contain partial/inconsistent data."), MB_ICONWARNING);
            }
            else
            {
                SetStatus(_T("Failed."));
                AfxMessageBox(_T("Copy failed:\r\n") + p.errorMessage, MB_ICONERROR);
            }
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CDiskCopyDialog::OnBtnClose()
{
    if (m_copyRunning)
    {
        AfxMessageBox(_T("A copy is still running. Cancel it first."), MB_ICONWARNING);
        return;
    }
    EndDialog(IDCANCEL);
}

void CDiskCopyDialog::OnCancel()
{
    if (m_copyRunning)
    {
        AfxMessageBox(_T("A copy is still running. Cancel it first."), MB_ICONWARNING);
        return;
    }
    CDialogEx::OnCancel();
}

void CDiskCopyDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_staticSrcHdr.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12, W = R - L;

    // Source header row
    m_staticSrcHdr.MoveWindow(L, 8, 52, 20, TRUE);
    m_radioSrcTypeDrive.MoveWindow(L + 56, 8, 130, 20, TRUE);
    m_radioSrcTypeFile.MoveWindow(L + 192, 8, 64, 20, TRUE);
    m_btnRefresh.MoveWindow(R - 102, 6, 102, 24, TRUE);

    // Source input row
    m_comboSrc.MoveWindow(L, 34, W, 170, TRUE);
    m_editSrcFile.MoveWindow(L, 34, W - 90, 22, TRUE);
    m_btnBrowseSrc.MoveWindow(R - 82, 34, 82, 22, TRUE);

    // Source range row
    m_radioSrcWhole.MoveWindow(L, 64, 112, 20, TRUE);
    m_radioSrcPart.MoveWindow(L + 118, 64, 132, 20, TRUE);
    m_btnChoosePart.MoveWindow(L + 256, 62, 144, 24, TRUE);

    // Partition info
    m_staticPart.MoveWindow(L, 92, W, 18, TRUE);

    // Destination header row
    m_staticDstHdr.MoveWindow(L, 116, 82, 20, TRUE);

    // Destination type row
    m_radioDstDisk.MoveWindow(L, 140, 112, 20, TRUE);
    m_radioDstFile.MoveWindow(L + 118, 140, 80, 20, TRUE);

    // Destination input row
    m_comboDst.MoveWindow(L, 166, W, 170, TRUE);
    m_editDstFile.MoveWindow(L, 166, W - 90, 22, TRUE);
    m_btnBrowse.MoveWindow(R - 82, 166, 82, 22, TRUE);

    // Destination range row
    m_radioDstWhole.MoveWindow(L, 196, 112, 20, TRUE);
    m_radioDstPart.MoveWindow(L + 118, 196, 132, 20, TRUE);
    m_btnChooseDstPart.MoveWindow(L + 256, 194, 182, 24, TRUE);

    // Destination partition info
    m_staticDstPart.MoveWindow(L, 224, W, 18, TRUE);

    // Summary area (fills between fixed rows and bottom section)
    int summaryH = (cy - 248 - 108 > 20) ? (cy - 248 - 108) : 20;
    m_staticSummary.MoveWindow(L, 248, W, summaryH, TRUE);

    // Bottom section
    m_progress.MoveWindow(L, cy - 100, W, 18, TRUE);
    m_staticStatus.MoveWindow(L, cy - 76, W, 18, TRUE);
    m_btnStart.MoveWindow(L, cy - 44, 130, 28, TRUE);
    m_btnCancelCopy.MoveWindow(L + 140, cy - 44, 110, 28, TRUE);
    m_btnClose.MoveWindow(R - 90, cy - 44, 90, 28, TRUE);
}

void CDiskCopyDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
