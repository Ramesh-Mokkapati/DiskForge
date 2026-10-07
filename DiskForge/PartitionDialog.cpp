// PartitionDialog.cpp - Read-only partition table viewer dialog
#include "pch.h"
#include "PartitionDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CPartitionDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDC_PART_BTN_GOTO,  &CPartitionDialog::OnBtnGoto)
    ON_BN_CLICKED(IDC_PART_BTN_CLOSE, &CPartitionDialog::OnBtnClose)
    ON_NOTIFY(NM_DBLCLK,   IDC_PART_LIST, &CPartitionDialog::OnListDblClick)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_PART_LIST, &CPartitionDialog::OnListSelChange)
END_MESSAGE_MAP()

CPartitionDialog::CPartitionDialog(CDiskManager& dm, CWnd* pParent)
    : CDialogEx(IDD_PARTITIONS, pParent)
    , m_diskManager(dm)
    , m_selectedSector(0)
    , m_hasSelection(false)
{
}

void CPartitionDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PART_STATIC_SUMMARY, m_staticSummary);
    DDX_Control(pDX, IDC_PART_LIST,           m_list);
    DDX_Control(pDX, IDC_PART_BTN_GOTO,       m_btnGoto);
    DDX_Control(pDX, IDC_PART_BTN_CLOSE,      m_btnClose);
}

BOOL CPartitionDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Partition Table (read-only)"));
    CenterWindow(GetParent());

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_list.InsertColumn(0, _T("#"),          LVCFMT_RIGHT, 36);
    m_list.InsertColumn(1, _T("Scheme"),     LVCFMT_LEFT,  60);
    m_list.InsertColumn(2, _T("Boot"),       LVCFMT_CENTER,44);
    m_list.InsertColumn(3, _T("Type"),       LVCFMT_LEFT,  200);
    m_list.InsertColumn(4, _T("Type ID"),    LVCFMT_LEFT,  90);
    m_list.InsertColumn(5, _T("Start LBA"),  LVCFMT_RIGHT, 100);
    m_list.InsertColumn(6, _T("Sectors"),    LVCFMT_RIGHT, 100);
    m_list.InsertColumn(7, _T("Size"),       LVCFMT_RIGHT, 90);
    m_list.InsertColumn(8, _T("Name"),       LVCFMT_LEFT,  140);

    m_btnGoto.EnableWindow(FALSE);

    CFont* pFont = GetFont();
    if (pFont)
    {
        m_staticSummary.SetFont(pFont);
        m_list.SetFont(pFont);
    }


    LoadAndPopulate();

    return TRUE;
}

void CPartitionDialog::LoadAndPopulate()
{
    CString scheme, diskGuid, error;
    bool ok = CPartitionParser::Parse(m_diskManager, m_partitions, scheme, diskGuid, error);

    m_list.DeleteAllItems();

    for (size_t i = 0; i < m_partitions.size(); ++i)
    {
        const PartitionEntry& pe = m_partitions[i];

        CString idxStr;
        idxStr.Format(_T("%d%s"), pe.index, pe.isLogical ? _T(" (logical)") : _T(""));
        int row = m_list.InsertItem(static_cast<int>(i), idxStr);

        m_list.SetItemText(row, 1, pe.scheme);
        m_list.SetItemText(row, 2, pe.bootable ? _T("Yes") : _T(""));
        m_list.SetItemText(row, 3, pe.typeName);
        m_list.SetItemText(row, 4, pe.typeId);

        CString startStr, sectStr, sizeStr;
        startStr.Format(_T("%llu"), pe.startLBA);
        sectStr.Format(_T("%llu"), pe.sectorCount);
        sizeStr = CPartitionParser::FormatSize(pe.sizeBytes);

        m_list.SetItemText(row, 5, startStr);
        m_list.SetItemText(row, 6, sectStr);
        m_list.SetItemText(row, 7, sizeStr);
        m_list.SetItemText(row, 8, pe.name);

        // Stash the start LBA on the item for quick retrieval on selection.
        m_list.SetItemData(row, static_cast<DWORD_PTR>(pe.startLBA));
    }

    CString summary;
    if (!ok)
    {
        summary.Format(_T("Could not read partition table: %s"), error.GetString());
    }
    else
    {
        CString schemeLine;
        schemeLine.Format(_T("Partitioning scheme: %s"), scheme.GetString());
        if (!diskGuid.IsEmpty())
            schemeLine += _T("   |   Disk GUID: ") + diskGuid;

        if (m_partitions.empty())
        {
            summary = schemeLine + _T("\r\nNo partitions found.");
        }
        else
        {
            CString countLine;
            countLine.Format(_T("\r\n%d partition(s) found."), (int)m_partitions.size());
            summary = schemeLine + countLine;
        }

        if (!error.IsEmpty())
            summary += _T("\r\nNote: ") + error;
    }
    m_staticSummary.SetWindowText(summary);
}

void CPartitionDialog::CommitSelection(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= (int)m_partitions.size())
    {
        m_hasSelection = false;
        return;
    }
    m_selectedEntry  = m_partitions[itemIndex];
    m_selectedSector = m_selectedEntry.startLBA;
    m_hasSelection = true;
}

void CPartitionDialog::OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNM = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if ((pNM->uChanged & LVIF_STATE) && (pNM->uNewState & LVIS_SELECTED))
    {
        m_btnGoto.EnableWindow(TRUE);
    }
    *pResult = 0;
}

void CPartitionDialog::OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNM = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    if (pNM->iItem >= 0)
    {
        CommitSelection(pNM->iItem);
        if (m_hasSelection)
            EndDialog(IDOK);
    }
    *pResult = 0;
}

void CPartitionDialog::OnBtnGoto()
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    CommitSelection(sel);
    if (m_hasSelection)
        EndDialog(IDOK);
    else
        AfxMessageBox(_T("Select a partition first."), MB_ICONWARNING);
}

void CPartitionDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}

void CPartitionDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_list.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    m_staticSummary.MoveWindow(8, 8, cx - 16, 36, TRUE);
    m_list.MoveWindow(8, 50, cx - 16, cy - 92, TRUE);
    m_btnGoto.MoveWindow(8, cy - 34, 160, 26, TRUE);
    m_btnClose.MoveWindow(cx - 100, cy - 34, 92, 26, TRUE);
}

void CPartitionDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}
