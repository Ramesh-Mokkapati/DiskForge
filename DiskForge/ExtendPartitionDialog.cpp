// ExtendPartitionDialog.cpp
#include "pch.h"
#include "ExtendPartitionDialog.h"
#include "ConfirmTextDialog.h"
#include "PartitionParser.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CExtendPartitionDialog, CDialogEx)
    ON_BN_CLICKED(IDC_EX_BTN_EXTEND, &CExtendPartitionDialog::OnBtnExtend)
    ON_BN_CLICKED(IDC_EX_BTN_CANCEL, &CExtendPartitionDialog::OnBtnCancel)
END_MESSAGE_MAP()

CExtendPartitionDialog::CExtendPartitionDialog(const DriveInfo& drive,
                                                const PartitionEntry& entry,
                                                CWnd* pParent)
    : CDialogEx(IDD_EXTENDPARTITION, pParent)
    , m_drive(drive)
    , m_entry(entry)
    , m_maxExtendBytes(0)
{
}

void CExtendPartitionDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EX_STATIC_INFO,    m_staticInfo);
    DDX_Control(pDX, IDC_EX_STATIC_CUR,     m_staticCur);
    DDX_Control(pDX, IDC_EX_STATIC_AVAIL,   m_staticAvail);
    DDX_Control(pDX, IDC_EX_EDIT_MB,        m_editMb);
    DDX_Control(pDX, IDC_EX_STATIC_MAXHINT, m_staticMaxHint);
    DDX_Control(pDX, IDC_EX_BTN_EXTEND,     m_btnExtend);
    DDX_Control(pDX, IDC_EX_BTN_CANCEL,     m_btnCancel);
    DDX_Control(pDX, IDC_EX_STATIC_STATUS,  m_staticStatus);
}

BOOL CExtendPartitionDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CenterWindow(GetParent());
    SetWindowText(_T("Extend Partition"));

    // Query available free space
    CPartitionEditor::FreeSpaceAfter fsa =
        CPartitionEditor::QueryFreeSpaceAfter(m_drive.devicePath, m_drive.driveIndex, m_entry);
    m_maxExtendBytes = fsa.valid ? fsa.freeSizeBytes : 0;

    // Drive name (short form)
    int pos = m_drive.devicePath.ReverseFind(_T('\\'));
    CString driveName = (pos >= 0) ? m_drive.devicePath.Mid(pos + 1) : m_drive.devicePath;

    // Partition info line
    CString infoText;
    infoText.Format(_T("Partition #%d (%s) on %s"), m_entry.index,
                    m_entry.typeName.GetString(), driveName.GetString());
    if (!m_entry.name.IsEmpty())
        infoText += _T("  \"") + m_entry.name + _T("\"");
    m_staticInfo.SetWindowText(infoText);

    m_staticCur.SetWindowText(CPartitionParser::FormatSize(m_entry.sizeBytes));

    CString availStr;
    if (!fsa.valid)
        availStr = _T("Query failed (could not open drive).");
    else if (m_maxExtendBytes == 0)
        availStr = _T("None -- no free space immediately follows this partition.");
    else
        availStr = CPartitionParser::FormatSize(m_maxExtendBytes) + _T(" available");
    m_staticAvail.SetWindowText(availStr);

    CString maxHint;
    if (m_maxExtendBytes > 0)
        maxHint.Format(_T("(max: %llu MB)"), m_maxExtendBytes / (1024ULL * 1024ULL));
    else
        maxHint = _T("(no free space available)");
    m_staticMaxHint.SetWindowText(maxHint);

    m_btnExtend.EnableWindow(m_maxExtendBytes > 0);

    return TRUE;
}

void CExtendPartitionDialog::OnBtnExtend()
{
    CString mbStr;
    m_editMb.GetWindowText(mbStr);
    mbStr.Trim();
    if (mbStr.IsEmpty())
    {
        m_staticStatus.SetWindowText(_T("Enter the number of MB to add."));
        return;
    }

    ULONGLONG mb = static_cast<ULONGLONG>(_ttoi64(mbStr.GetString()));
    if (mb == 0)
    {
        m_staticStatus.SetWindowText(_T("Enter a value greater than 0."));
        return;
    }

    ULONGLONG extendBytes = mb * 1024ULL * 1024ULL;
    if (extendBytes > m_maxExtendBytes)
    {
        CString msg;
        msg.Format(_T("Maximum is %llu MB. Enter a smaller value."),
                   m_maxExtendBytes / (1024ULL * 1024ULL));
        m_staticStatus.SetWindowText(msg);
        return;
    }

    // Safety gate
    int pos = m_drive.devicePath.ReverseFind(_T('\\'));
    CString driveName = (pos >= 0) ? m_drive.devicePath.Mid(pos + 1) : m_drive.devicePath;

    CString phrase;
    phrase.Format(_T("EXTEND %s PART %d"), driveName.GetString(), m_entry.index);

    CString warnMsg;
    warnMsg.Format(
        _T("You are about to extend partition #%d on %s by %llu MB.\r\n\r\n")
        _T("The partition table will be updated immediately and cannot be undone.\r\n\r\n")
        _T("Type  %s  to confirm."),
        m_entry.index, driveName.GetString(), mb, phrase.GetString());

    CConfirmTextDialog confirmDlg(warnMsg, phrase, this);
    if (confirmDlg.DoModal() != IDOK)
        return;

    m_staticStatus.SetWindowText(_T("Extending... please wait."));
    UpdateWindow();

    CString err = CPartitionEditor::GrowPartition(
        m_drive.devicePath, m_drive.driveIndex, m_entry, extendBytes);

    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Partition extended successfully."), MB_ICONINFORMATION);
        EndDialog(IDOK);
    }
    else
    {
        m_staticStatus.SetWindowText(_T("Failed: ") + err);
        AfxMessageBox(_T("Failed to extend partition:\r\n") + err, MB_ICONERROR);
    }
}

void CExtendPartitionDialog::OnBtnCancel()
{
    EndDialog(IDCANCEL);
}
