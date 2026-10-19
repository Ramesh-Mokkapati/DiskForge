// BrowserListView.h - Right pane of the System Browser (the NewTest
// framework's list view): shows the content of the node selected in the tree
//
//   Disk / Partition / Boot Record / File Allocation -> structure fields
//   Partition Table                                  -> partition entries
//   File System / folders                            -> files and folders,
//        deleted ones in red with a "Recoverable" verdict and notes
//
// Double-clicking opens a folder or partition (selects it in the tree).
#pragma once
#include "DiskForgeDoc.h"

class CBrowserListView : public CListView
{
protected:
    CBrowserListView() = default;
    DECLARE_DYNCREATE(CBrowserListView)

public:
    CDiskForgeDoc* GetDocument() const { return reinterpret_cast<CDiskForgeDoc*>(m_pDocument); }

    void         ShowNode(BrowserNode* node);
    void         Refresh() { ShowNode(m_node); }
    BrowserNode* CurrentNode() const { return m_node; }

    // Selected rows in a folder listing (empty for other node kinds).
    std::vector<FsEntry> SelectedEntries() const;
    const FsEntry*       FocusedEntry() const;
    int                  FocusedPartition() const;   // partition index, or -1

    // Field rows describing a node (Disk, Partition, Boot Record, File
    // Allocation, folders) - shared with the Properties pane.
    static void NodeFields(CDiskForgeDoc* doc, BrowserNode* node, std::vector<FieldRow>& out);

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual void OnInitialUpdate() override;
    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

    afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDblClk(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnReturn(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    enum class Mode { None, Message, Fields, Partitions, Entries };
    Mode                  m_mode = Mode::None;
    BrowserNode*          m_node = nullptr;
    UINT                  m_nodeGeneration = 0;   // doc generation m_node belongs to
    std::vector<FieldRow> m_fields;
    int                   m_sortColumn = -1;
    bool                  m_sortAscending = true;

    void SetColumns(const std::vector<std::pair<LPCTSTR, int>>& cols);
    void ShowMessage(const CString& text);
    void ShowFields(const std::vector<FieldRow>& fields);
    void ShowPartitions();
    void ShowEntries();
    void OpenFocused();
};
