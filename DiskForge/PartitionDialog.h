// PartitionDialog.h - Read-only partition table viewer dialog
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"


// Modal dialog that parses and displays the MBR/GPT partition table of the
// currently-open drive. Purely read-only: it never issues a write. If the
// user double-clicks (or selects + clicks "Go to Sector"), the dialog closes
// with IDOK and the caller can retrieve the chosen start sector.
class CPartitionDialog : public CDialogEx
{
public:
    explicit CPartitionDialog(CDiskManager& dm, CWnd* pParent = nullptr);

    enum { IDD = IDD_PARTITIONS };

    bool      HasSelection() const  { return m_hasSelection; }
    ULONGLONG GetSelectedSector() const { return m_selectedSector; }

    // Returns the full parsed entry for whatever the user selected (start LBA,
    // sector count, type, etc.) - used by features like Disk Copy that need
    // more than just the start sector. Returns false if nothing was selected.
    bool GetSelectedPartitionEntry(PartitionEntry& outEntry) const
    {
        if (!m_hasSelection) return false;
        outEntry = m_selectedEntry;
        return true;
    }

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnBtnGoto();
    afx_msg void OnBtnClose();
    afx_msg void OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CDiskManager&                m_diskManager;
    CListCtrl                    m_list;
    CStatic                      m_staticSummary;
    CButton                      m_btnGoto;
    CButton                      m_btnClose;

    std::vector<PartitionEntry>  m_partitions;
    PartitionEntry               m_selectedEntry;
    ULONGLONG                    m_selectedSector;
    bool                         m_hasSelection;

    void LoadAndPopulate();
    void CommitSelection(int itemIndex);

    static const int MIN_WIDTH  = 760;
    static const int MIN_HEIGHT = 420;
};
