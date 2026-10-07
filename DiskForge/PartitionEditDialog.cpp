// PartitionEditDialog.cpp
#include "pch.h"
#include "PartitionEditDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    struct GptTypeChoice { const TCHAR* label; GUID guid; };

    // clang-format off
    const GUID kGuidEfiSystem   = { 0xC12A7328, 0xF81F, 0x11D2, {0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B} };
    const GUID kGuidMsReserved  = { 0xE3C9E316, 0x0B5C, 0x4DB8, {0x81,0x7D,0xF9,0x2D,0xF0,0x02,0x15,0xAE} };
    const GUID kGuidMsBasicData = { 0xEBD0A0A2, 0xB9E5, 0x4433, {0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7} };
    const GUID kGuidMsRecovery  = { 0xDE94BBA4, 0x06D1, 0x4D40, {0xA1,0x6A,0xBF,0xD5,0x01,0x79,0xD6,0xAC} };
    const GUID kGuidLinuxFs     = { 0x0FC63DAF, 0x8483, 0x4772, {0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4} };
    const GUID kGuidLinuxSwap   = { 0x0657FD6D, 0xA4AB, 0x43C4, {0x84,0xE5,0x09,0x33,0xC8,0x4B,0x4F,0x4F} };
    // clang-format on

    const GptTypeChoice kGptTypeChoices[] = {
        { _T("Microsoft Basic Data"),  kGuidMsBasicData },
        { _T("EFI System Partition"),  kGuidEfiSystem },
        { _T("Microsoft Reserved"),    kGuidMsReserved },
        { _T("Windows Recovery"),      kGuidMsRecovery },
        { _T("Linux filesystem"),      kGuidLinuxFs },
        { _T("Linux swap"),            kGuidLinuxSwap },
    };
    const int kNumGptTypeChoices = sizeof(kGptTypeChoices) / sizeof(kGptTypeChoices[0]);

    struct MbrTypeChoice { const TCHAR* label; BYTE type; };
    const MbrTypeChoice kMbrTypeChoices[] = {
        { _T("0x07 - NTFS / exFAT"),      0x07 },
        { _T("0x0B - FAT32 (CHS)"),       0x0B },
        { _T("0x0C - FAT32 (LBA)"),       0x0C },
        { _T("0x0E - FAT16 (LBA)"),       0x0E },
        { _T("0x83 - Linux filesystem"),  0x83 },
        { _T("0x82 - Linux swap"),        0x82 },
        { _T("0xEF - EFI System (FAT)"),  0xEF },
    };
    const int kNumMbrTypeChoices = sizeof(kMbrTypeChoices) / sizeof(kMbrTypeChoices[0]);

    CString ShortDriveName(const CString& devicePath)
    {
        int pos = devicePath.ReverseFind(_T('\\'));
        return (pos >= 0) ? devicePath.Mid(pos + 1) : devicePath;
    }

    // Parses a leading "0xNN" (case-insensitive) from combo text like
    // "0x07 - NTFS / exFAT" or a bare "0x07" the user typed themselves.
    bool ParseLeadingHexByte(const CString& text, BYTE& out)
    {
        CString trimmed = text;
        trimmed.TrimLeft();
        if (trimmed.GetLength() < 2 || trimmed[0] != _T('0') || (trimmed[1] != _T('x') && trimmed[1] != _T('X')))
            return false;
        TCHAR* endPtr = nullptr;
        long v = _tcstol(trimmed.GetString() + 2, &endPtr, 16);
        if (endPtr == trimmed.GetString() + 2 || v < 0 || v > 0xFF)
            return false;
        out = static_cast<BYTE>(v);
        return true;
    }
}

BEGIN_MESSAGE_MAP(CPartitionEditDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_CBN_SELCHANGE(IDC_PE_COMBO_DRIVE, &CPartitionEditDialog::OnComboDriveChange)
    ON_BN_CLICKED(IDC_PE_BTN_LOAD, &CPartitionEditDialog::OnBtnLoad)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_PE_LIST_PARTITIONS, &CPartitionEditDialog::OnListSelChange)
    ON_CBN_SELCHANGE(IDC_PE_COMBO_MBR_TYPE, &CPartitionEditDialog::OnComboMbrTypeChange)
    ON_CBN_EDITCHANGE(IDC_PE_COMBO_MBR_TYPE, &CPartitionEditDialog::OnComboMbrTypeChange)
    ON_BN_CLICKED(IDC_PE_BTN_APPLY, &CPartitionEditDialog::OnBtnApply)
    ON_BN_CLICKED(IDC_PE_BTN_ASSIGN_LETTER, &CPartitionEditDialog::OnBtnAssignLetter)
    ON_BN_CLICKED(IDC_PE_BTN_REMOVE_LETTER, &CPartitionEditDialog::OnBtnRemoveLetter)
    ON_BN_CLICKED(IDC_PE_BTN_DELETE, &CPartitionEditDialog::OnBtnDelete)
    ON_BN_CLICKED(IDC_PE_BTN_EXTEND, &CPartitionEditDialog::OnBtnExtend)
    ON_BN_CLICKED(IDC_PE_BTN_CLOSE, &CPartitionEditDialog::OnBtnClose)
    ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

CPartitionEditDialog::CPartitionEditDialog(CWnd* pParent)
    : CDialogEx(IDD_PARTITIONEDIT, pParent)
    , m_selectedIndex(-1)
{
}

void CPartitionEditDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PE_COMBO_DRIVE,         m_comboDrive);
    DDX_Control(pDX, IDC_PE_BTN_LOAD,            m_btnLoad);
    DDX_Control(pDX, IDC_PE_LIST_PARTITIONS,     m_listPartitions);
    DDX_Control(pDX, IDC_PE_CHK_MBR_ACTIVE,      m_chkMbrActive);
    DDX_Control(pDX, IDC_PE_COMBO_MBR_TYPE,      m_comboMbrType);
    DDX_Control(pDX, IDC_PE_CHK_MBR_HIDDEN,      m_chkMbrHidden);
    DDX_Control(pDX, IDC_PE_EDIT_GPT_NAME,       m_editGptName);
    DDX_Control(pDX, IDC_PE_COMBO_GPT_TYPE,      m_comboGptType);
    DDX_Control(pDX, IDC_PE_CHK_GPT_LEGACY_BOOT, m_chkGptLegacyBoot);
    DDX_Control(pDX, IDC_PE_CHK_GPT_NO_LETTER,   m_chkGptNoLetter);
    DDX_Control(pDX, IDC_PE_BTN_APPLY,           m_btnApply);
    DDX_Control(pDX, IDC_PE_STATIC_STATUS,       m_staticStatus);
    DDX_Control(pDX, IDC_PE_STATIC_CUR_LETTER,   m_staticCurLetter);
    DDX_Control(pDX, IDC_PE_COMBO_NEW_LETTER,    m_comboNewLetter);
    DDX_Control(pDX, IDC_PE_BTN_ASSIGN_LETTER,   m_btnAssignLetter);
    DDX_Control(pDX, IDC_PE_BTN_REMOVE_LETTER,   m_btnRemoveLetter);
    DDX_Control(pDX, IDC_PE_BTN_DELETE,          m_btnDelete);
    DDX_Control(pDX, IDC_PE_BTN_EXTEND,          m_btnExtend);
    DDX_Control(pDX, IDC_PE_BTN_CLOSE,           m_btnClose);
}

BOOL CPartitionEditDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Partition Editor (labels, type, active/hidden flags, drive letter)"));
    CenterWindow(GetParent());

    m_listPartitions.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listPartitions.InsertColumn(0, _T("#"),          LVCFMT_RIGHT, 30);
    m_listPartitions.InsertColumn(1, _T("Scheme"),     LVCFMT_LEFT,  60);
    m_listPartitions.InsertColumn(2, _T("Type"),       LVCFMT_LEFT,  170);
    m_listPartitions.InsertColumn(3, _T("Active/Boot"),LVCFMT_CENTER,80);
    m_listPartitions.InsertColumn(4, _T("Name"),       LVCFMT_LEFT,  130);
    m_listPartitions.InsertColumn(5, _T("Start LBA"),  LVCFMT_RIGHT, 90);
    m_listPartitions.InsertColumn(6, _T("Size"),       LVCFMT_RIGHT, 80);

    for (int i = 0; i < kNumMbrTypeChoices; ++i)
        m_comboMbrType.AddString(kMbrTypeChoices[i].label);

    m_comboGptType.AddString(_T("(keep current type)"));
    for (int i = 0; i < kNumGptTypeChoices; ++i)
        m_comboGptType.AddString(kGptTypeChoices[i].label);

    m_btnApply.EnableWindow(FALSE);
    m_btnDelete.EnableWindow(FALSE);
    m_btnExtend.EnableWindow(FALSE);
    m_btnAssignLetter.EnableWindow(FALSE);
    m_btnRemoveLetter.EnableWindow(FALSE);

    CDiskManager dm;
    dm.EnumerateDrives(m_drives);
    for (const auto& d : m_drives)
        m_comboDrive.AddString(d.displayName);
    if (m_comboDrive.GetCount() > 0)
        m_comboDrive.SetCurSel(0);

    UpdateModeVisibility();
    return TRUE;
}

void CPartitionEditDialog::OnComboDriveChange()
{
    m_partitions.clear();
    m_listPartitions.DeleteAllItems();
    m_selectedIndex = -1;
    UpdateModeVisibility();
    m_staticStatus.SetWindowText(_T("Click \"Load Partitions\"."));
}

void CPartitionEditDialog::PopulateList()
{
    m_listPartitions.DeleteAllItems();
    for (size_t i = 0; i < m_partitions.size(); ++i)
    {
        const PartitionEntry& e = m_partitions[i];
        CString idx, activeStr, startStr, sizeStr;
        idx.Format(_T("%d"), e.index);
        activeStr = e.bootable ? _T("Yes") : _T("");
        startStr.Format(_T("%llu"), e.startLBA);
        sizeStr = CPartitionParser::FormatSize(e.sizeBytes);

        int row = m_listPartitions.InsertItem((int)i, idx);
        m_listPartitions.SetItemText(row, 1, e.scheme);
        m_listPartitions.SetItemText(row, 2, e.typeName);
        m_listPartitions.SetItemText(row, 3, activeStr);
        m_listPartitions.SetItemText(row, 4, e.name);
        m_listPartitions.SetItemText(row, 5, startStr);
        m_listPartitions.SetItemText(row, 6, sizeStr);
    }
}

void CPartitionEditDialog::OnBtnLoad()
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
        AfxMessageBox(_T("Could not open the drive:\r\n") + dm.GetLastError(), MB_ICONERROR);
        return;
    }

    CString scheme, diskGuid, err;
    m_partitions.clear();
    bool ok = CPartitionParser::Parse(dm, m_partitions, scheme, diskGuid, err);
    dm.CloseDrive();

    if (!ok)
    {
        AfxMessageBox(_T("Could not read the partition table:\r\n") + err, MB_ICONERROR);
        return;
    }

    PopulateList();
    m_selectedIndex = -1;
    ShowFieldsForSelection();

    CString msg;
    msg.Format(_T("%s: %d partition(s) loaded. Select one to edit."), scheme.GetString(), (int)m_partitions.size());
    m_staticStatus.SetWindowText(msg);
}

void CPartitionEditDialog::OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNM = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if ((pNM->uChanged & LVIF_STATE) && (pNM->uNewState & LVIS_SELECTED))
    {
        m_selectedIndex = pNM->iItem;
        ShowFieldsForSelection();
        RefreshDriveLetterPanel();
    }
    *pResult = 0;
}

void CPartitionEditDialog::UpdateModeVisibility()
{
    bool haveSelection = (m_selectedIndex >= 0 && m_selectedIndex < (int)m_partitions.size());
    bool isMbr = haveSelection && m_partitions[m_selectedIndex].scheme == _T("MBR");
    bool isGpt = haveSelection && m_partitions[m_selectedIndex].scheme == _T("GPT");

    m_chkMbrActive.ShowWindow(isMbr ? SW_SHOW : SW_HIDE);
    m_comboMbrType.ShowWindow(isMbr ? SW_SHOW : SW_HIDE);
    m_chkMbrHidden.ShowWindow(isMbr ? SW_SHOW : SW_HIDE);

    m_editGptName.ShowWindow(isGpt ? SW_SHOW : SW_HIDE);
    m_comboGptType.ShowWindow(isGpt ? SW_SHOW : SW_HIDE);
    m_chkGptLegacyBoot.ShowWindow(isGpt ? SW_SHOW : SW_HIDE);
    m_chkGptNoLetter.ShowWindow(isGpt ? SW_SHOW : SW_HIDE);

    if (!haveSelection)
    {
        if (m_btnDelete.GetSafeHwnd()) m_btnDelete.EnableWindow(FALSE);
        if (m_btnExtend.GetSafeHwnd()) m_btnExtend.EnableWindow(FALSE);
    }
}

void CPartitionEditDialog::ShowFieldsForSelection()
{
    UpdateModeVisibility();

    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
    {
        m_btnApply.EnableWindow(FALSE);
        m_btnDelete.EnableWindow(FALSE);
        m_btnExtend.EnableWindow(FALSE);
        return;
    }

    const PartitionEntry& e = m_partitions[m_selectedIndex];

    if (e.isLogical || e.rawSlotIndex < 0)
    {
        m_btnApply.EnableWindow(FALSE);
        m_btnDelete.EnableWindow(FALSE);
        m_btnExtend.EnableWindow(FALSE);
        m_staticStatus.SetWindowText(_T("This entry (logical/extended partition) can't be edited by this tool."));
        return;
    }

    PartitionEditFields f = CPartitionEditor::ReadFields(e);

    if (e.scheme == _T("MBR"))
    {
        m_chkMbrActive.SetCheck(f.mbrActive ? BST_CHECKED : BST_UNCHECKED);

        CString current;
        current.Format(_T("0x%02X - %s"), f.mbrTypeByte, e.typeName.GetString());
        m_comboMbrType.SetWindowText(current);

        m_chkMbrHidden.SetCheck(CPartitionEditor::IsMbrTypeHidden(f.mbrTypeByte) ? BST_CHECKED : BST_UNCHECKED);
        m_chkMbrHidden.EnableWindow(CPartitionEditor::IsMbrTypeHideable(f.mbrTypeByte));
    }
    else
    {
        m_editGptName.SetWindowText(f.gptName);
        m_comboGptType.SetCurSel(0); // "(keep current type)" - safe default, see header note
        m_chkGptLegacyBoot.SetCheck(f.gptLegacyBiosBootable ? BST_CHECKED : BST_UNCHECKED);
        m_chkGptNoLetter.SetCheck(f.gptNoDriveLetter ? BST_CHECKED : BST_UNCHECKED);
    }

    m_btnApply.EnableWindow(TRUE);
    m_btnDelete.EnableWindow(TRUE);
    m_btnExtend.EnableWindow(TRUE);
    m_staticStatus.SetWindowText(_T("Ready to edit. Click \"Apply Changes\" when done."));
}

void CPartitionEditDialog::OnComboMbrTypeChange()
{
    CString text;
    m_comboMbrType.GetWindowText(text);
    BYTE type = 0;
    if (ParseLeadingHexByte(text, type))
        m_chkMbrHidden.EnableWindow(CPartitionEditor::IsMbrTypeHideable(type));
}

void CPartitionEditDialog::OnBtnApply()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
        return;

    const PartitionEntry& e = m_partitions[m_selectedIndex];
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
        return;
    const DriveInfo& drive = m_drives[sel];

    CString driveName = ShortDriveName(drive.devicePath);
    CString err;

    if (e.scheme == _T("MBR"))
    {
        CString typeText;
        m_comboMbrType.GetWindowText(typeText);
        BYTE typeByte = 0;
        if (!ParseLeadingHexByte(typeText, typeByte))
        {
            AfxMessageBox(_T("Enter the type as \"0xNN\" (e.g. 0x07), or pick one from the list."), MB_ICONWARNING);
            return;
        }

        bool wantHidden = (m_chkMbrHidden.GetCheck() == BST_CHECKED);
        typeByte = CPartitionEditor::ToggleMbrHidden(typeByte, wantHidden);

        PartitionEditFields fields;
        fields.scheme = _T("MBR");
        fields.mbrActive = (m_chkMbrActive.GetCheck() == BST_CHECKED);
        fields.mbrTypeByte = typeByte;

        CString msg;
        msg.Format(_T("Apply these changes to partition #%d on %s?\r\n\r\nActive: %s\r\nType: 0x%02X"),
                    e.index, driveName.GetString(), fields.mbrActive ? _T("Yes") : _T("No"), typeByte);
        if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;

        err = CPartitionEditor::WriteMbrEntry(drive.devicePath, drive.driveIndex, e.rawSlotIndex,
                                                fields, e.startLBA, e.sectorCount);
    }
    else
    {
        PartitionEditFields fields = CPartitionEditor::ReadFields(e); // start from current, so untouched fields stay untouched
        fields.scheme = _T("GPT");

        m_editGptName.GetWindowText(fields.gptName);

        int typeSel = m_comboGptType.GetCurSel();
        if (typeSel > 0 && typeSel <= kNumGptTypeChoices)
            fields.gptTypeGuid = kGptTypeChoices[typeSel - 1].guid;
        // else: index 0 ("keep current type") - fields.gptTypeGuid already holds the current GUID from ReadFields()

        fields.gptLegacyBiosBootable = (m_chkGptLegacyBoot.GetCheck() == BST_CHECKED);
        fields.gptNoDriveLetter      = (m_chkGptNoLetter.GetCheck() == BST_CHECKED);

        CString msg;
        msg.Format(_T("Apply these changes to partition #%d on %s?\r\n\r\nName: %s\r\nLegacy BIOS bootable: %s\r\nHidden (no default drive letter): %s"),
                    e.index, driveName.GetString(), fields.gptName.GetString(),
                    fields.gptLegacyBiosBootable ? _T("Yes") : _T("No"),
                    fields.gptNoDriveLetter ? _T("Yes") : _T("No"));
        if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;

        err = CPartitionEditor::WriteGptEntry(drive.devicePath, drive.driveIndex, e.rawSlotIndex,
                                                fields, e.startLBA, e.sectorCount);
    }

    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Changes applied."), MB_ICONINFORMATION);
        OnBtnLoad(); // Reload so the list/fields reflect the new on-disk state
    }
    else
    {
        AfxMessageBox(_T("Failed to apply changes:\r\n") + err, MB_ICONERROR);
    }
}

void CPartitionEditDialog::RefreshDriveLetterPanel()
{
    m_comboNewLetter.ResetContent();

    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
    {
        m_staticCurLetter.SetWindowText(_T("(select a partition)"));
        m_btnAssignLetter.EnableWindow(FALSE);
        m_btnRemoveLetter.EnableWindow(FALSE);
        return;
    }

    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
        return;

    const PartitionEntry& e = m_partitions[m_selectedIndex];
    CPartitionEditor::VolumeMatch vol = CPartitionEditor::FindVolumeForPartition(m_drives[sel].driveIndex, e.startLBA);

    if (!vol.found)
    {
        m_staticCurLetter.SetWindowText(_T("No mounted volume found (unformatted or unrecognized)."));
        m_btnAssignLetter.EnableWindow(FALSE);
        m_btnRemoveLetter.EnableWindow(FALSE);
        return;
    }

    CString cur = (vol.currentDriveLetter != 0)
        ? CString(vol.currentDriveLetter) + _T(":")
        : CString(_T("(none)"));
    m_staticCurLetter.SetWindowText(cur);

    std::vector<TCHAR> avail = CPartitionEditor::GetAvailableDriveLetters();
    for (TCHAR c : avail)
    {
        CString s;
        s.Format(_T("%c:"), c);
        m_comboNewLetter.AddString(s);
    }
    if (m_comboNewLetter.GetCount() > 0)
        m_comboNewLetter.SetCurSel(0);

    m_btnAssignLetter.EnableWindow(m_comboNewLetter.GetCount() > 0);
    m_btnRemoveLetter.EnableWindow(vol.currentDriveLetter != 0);
}

void CPartitionEditDialog::OnBtnAssignLetter()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
        return;
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
        return;

    CString letterStr;
    m_comboNewLetter.GetWindowText(letterStr);
    if (letterStr.IsEmpty())
        return;
    TCHAR newLetter = letterStr[0];

    const PartitionEntry& e = m_partitions[m_selectedIndex];
    CPartitionEditor::VolumeMatch vol = CPartitionEditor::FindVolumeForPartition(m_drives[sel].driveIndex, e.startLBA);

    CString err = CPartitionEditor::SetDriveLetter(vol, newLetter);
    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Drive letter assigned."), MB_ICONINFORMATION);
        RefreshDriveLetterPanel();
    }
    else
    {
        AfxMessageBox(_T("Failed:\r\n") + err, MB_ICONERROR);
    }
}

void CPartitionEditDialog::OnBtnRemoveLetter()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
        return;
    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
        return;

    const PartitionEntry& e = m_partitions[m_selectedIndex];
    CPartitionEditor::VolumeMatch vol = CPartitionEditor::FindVolumeForPartition(m_drives[sel].driveIndex, e.startLBA);

    if (AfxMessageBox(_T("Remove this volume's drive letter? You can reassign one later."),
                       MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    CString err = CPartitionEditor::SetDriveLetter(vol, 0);
    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Drive letter removed."), MB_ICONINFORMATION);
        RefreshDriveLetterPanel();
    }
    else
    {
        AfxMessageBox(_T("Failed:\r\n") + err, MB_ICONERROR);
    }
}

void CPartitionEditDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}

void CPartitionEditDialog::OnBtnDelete()
{
    DeleteSelectedPartition();
}

void CPartitionEditDialog::OnBtnExtend()
{
    ExtendSelectedPartition();
}

void CPartitionEditDialog::DeleteSelectedPartition()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
        return;

    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
        return;

    const PartitionEntry& e     = m_partitions[m_selectedIndex];
    const DriveInfo&      drive = m_drives[sel];
    CString driveName = ShortDriveName(drive.devicePath);

    CString phrase;
    phrase.Format(_T("DELETE %s PART %d"), driveName.GetString(), e.index);

    CString warnMsg;
    warnMsg.Format(
        _T("You are about to DELETE partition #%d on %s.\r\n\r\n")
        _T("This removes the partition table entry. The partition will no longer be\r\n")
        _T("accessible and the space will appear unallocated. The data inside the\r\n")
        _T("partition is not immediately erased but cannot be recovered without a backup.\r\n\r\n")
        _T("Type  %s  to confirm."),
        e.index, driveName.GetString(), phrase.GetString());

    CConfirmTextDialog dlg(warnMsg, phrase, this);
    if (dlg.DoModal() != IDOK)
        return;

    CString err;
    if (e.scheme == _T("MBR"))
        err = CPartitionEditor::DeleteMbrEntry(drive.devicePath, drive.driveIndex, e.rawSlotIndex);
    else
        err = CPartitionEditor::DeleteGptEntry(drive.devicePath, drive.driveIndex, e.rawSlotIndex);

    if (err.IsEmpty())
    {
        AfxMessageBox(_T("Partition deleted."), MB_ICONINFORMATION);
        OnBtnLoad();
    }
    else
    {
        AfxMessageBox(_T("Failed to delete partition:\r\n") + err, MB_ICONERROR);
    }
}

void CPartitionEditDialog::ExtendSelectedPartition()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_partitions.size())
        return;

    int sel = m_comboDrive.GetCurSel();
    if (sel < 0 || sel >= (int)m_drives.size())
        return;

    CExtendPartitionDialog dlg(m_drives[sel], m_partitions[m_selectedIndex], this);
    if (dlg.DoModal() == IDOK)
        OnBtnLoad();
}

void CPartitionEditDialog::OnContextMenu(CWnd* pWnd, CPoint point)
{
    if (!pWnd || pWnd->GetSafeHwnd() != m_listPartitions.GetSafeHwnd())
        return;

    bool haveSelection = (m_selectedIndex >= 0 && m_selectedIndex < (int)m_partitions.size());
    bool canAct = haveSelection
        && !m_partitions[m_selectedIndex].isLogical
        && m_partitions[m_selectedIndex].rawSlotIndex >= 0;

    auto grayIfNo = [](bool b) -> UINT { return b ? 0u : (UINT)MF_GRAYED; };

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING | grayIfNo(canAct),       1, _T("Apply Changes"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | grayIfNo(canAct),       2, _T("Delete Partition"));
    menu.AppendMenu(MF_STRING | grayIfNo(canAct),       3, _T("Extend Partition"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | grayIfNo(haveSelection), 4, _T("Assign Drive Letter"));

    UINT result = (UINT)menu.TrackPopupMenu(
        TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, this);

    switch (result)
    {
    case 1: OnBtnApply();              break;
    case 2: DeleteSelectedPartition(); break;
    case 3: ExtendSelectedPartition(); break;
    case 4: m_comboNewLetter.SetFocus(); break;
    default: break;
    }
}

void CPartitionEditDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_listPartitions.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;

    // Drive combo stretches with cx; Load button stays right-anchored
    m_comboDrive.MoveWindow(L + 55, 10, R - 130 - (L + 55), 190, TRUE);
    m_btnLoad.MoveWindow(R - 120, 10, 120, 22, TRUE);

    // List: MapDialogRect converts template DLU to DPI-correct pixels so this
    // works at 96/125/150/175/200% DPI.  The list spans DLU rows 26..124.
    CRect rList(7, 26, 428, 124);
    MapDialogRect(&rList);
    m_listPartitions.MoveWindow(L, rList.top, R - L, rList.Height(), TRUE);

    // Status text stretches with cx; y is fixed by template (DLU 213..235).
    CRect rSt(7, 213, 428, 235);
    MapDialogRect(&rSt);
    m_staticStatus.MoveWindow(rSt.left, rSt.top, R - rSt.left, rSt.Height(), TRUE);

    // Bottom buttons are bottom- and right-anchored.
    // Use MapDialogRect for button height so it scales with DPI.
    CRect rBh(0, 0, 0, 17);
    MapDialogRect(&rBh);
    const int bh = rBh.Height(), by = cy - bh - 6;
    m_btnDelete.MoveWindow(L,       by, 140, bh, TRUE);
    m_btnExtend.MoveWindow(L + 150, by, 140, bh, TRUE);
    m_btnClose.MoveWindow(R - 90,   by,  90, bh, TRUE);

    // All other controls (MBR/GPT fields, Apply, letter-assignment section)
    // are at fixed positions in the template and do not need to move here;
    // the correctly-scaled DLU values render at the right pixel positions at
    // any DPI without hardcoding pixel coordinates.
}

void CPartitionEditDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
