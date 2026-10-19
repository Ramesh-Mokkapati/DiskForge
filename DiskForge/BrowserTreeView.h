// BrowserTreeView.h - Left pane of the System Browser (the NewTest
// framework's CLeftView): the on-disk structure tree
//
//   Disk -> Partition Table -> Partition -> Boot Record
//        -> File Allocation -> File System -> folders (live and deleted)
//
// Children are created lazily on expand. Opening an NTFS File System node
// scans the MFT on a worker thread; progress shows on the node and in the
// status bar, and the folder tree appears when the scan completes.
#pragma once
#include "DiskForgeDoc.h"
#include <atomic>

class CBrowserTreeView : public CTreeView
{
protected:
    CBrowserTreeView() = default;
    DECLARE_DYNCREATE(CBrowserTreeView)

public:
    CDiskForgeDoc* GetDocument() const { return reinterpret_cast<CDiskForgeDoc*>(m_pDocument); }

    BrowserNode* SelectedNode() const;

    // Navigation requested by the content list.
    bool OpenChildDirectory(const FsEntry& entry);  // select the child folder node matching entry
    bool OpenChildPartition(int partIndex);
    bool SelectParent();

    // Starts the NTFS MFT scan for a File System node (no-op if done/running).
    void StartIndex(BrowserNode* node);

    virtual ~CBrowserTreeView();

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual void OnInitialUpdate() override;
    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

    afx_msg void    OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void    OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg LRESULT OnIndexProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnIndexDone(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    struct IndexJob
    {
        std::shared_ptr<IFileSystem> fs;
        BrowserNode*      node = nullptr;
        UINT              generation = 0;
        HWND              hwnd = nullptr;
        std::atomic<bool> cancel{ false };
        bool              ok = false;
        CString           error;
    };
    std::vector<std::shared_ptr<IndexJob>> m_jobs;
    static UINT IndexThread(LPVOID param);

    HTREEITEM InsertNode(BrowserNode* node, HTREEITEM parent, bool hasChildren);
    void      Populate(HTREEITEM item);
    void      ResetChildren(HTREEITEM item);
    void      ResetDirectories(HTREEITEM item);  // after a filter change
    HTREEITEM FindItem(BrowserNode* node, HTREEITEM from = TVI_ROOT) const;
    CString   NodeTitle(const BrowserNode* node) const;
    int       NodeImage(const BrowserNode* node, bool selected) const;
    void      CancelJobs();

    static const LPARAM kPlaceholder = 0;   // item data of the "+" dummy child
};
