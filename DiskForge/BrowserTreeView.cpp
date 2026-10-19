// BrowserTreeView.cpp - System Browser tree (left splitter pane)
#include "pch.h"
#include "DiskForge.h"
#include "BrowserTreeView.h"
#include "MainFrm.h"
#include "ShellIcons.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    const UINT WM_INDEX_PROGRESS = WM_APP + 41;   // wParam = percent, lParam = IndexJob*
    const UINT WM_INDEX_DONE     = WM_APP + 42;   // lParam = heap std::shared_ptr<IndexJob>*

    const COLORREF kDeletedText = RGB(170, 40, 40);
}

IMPLEMENT_DYNCREATE(CBrowserTreeView, CTreeView)

BEGIN_MESSAGE_MAP(CBrowserTreeView, CTreeView)
    ON_NOTIFY_REFLECT(TVN_ITEMEXPANDING, &CBrowserTreeView::OnItemExpanding)
    ON_NOTIFY_REFLECT(TVN_SELCHANGED, &CBrowserTreeView::OnSelChanged)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CBrowserTreeView::OnCustomDraw)
    ON_WM_CONTEXTMENU()
    ON_WM_RBUTTONDOWN()
    ON_MESSAGE(WM_INDEX_PROGRESS, &CBrowserTreeView::OnIndexProgress)
    ON_MESSAGE(WM_INDEX_DONE, &CBrowserTreeView::OnIndexDone)
END_MESSAGE_MAP()

CBrowserTreeView::~CBrowserTreeView()
{
    CancelJobs();
}

BOOL CBrowserTreeView::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;
    return CTreeView::PreCreateWindow(cs);
}

void CBrowserTreeView::OnInitialUpdate()
{
    GetTreeCtrl().SetImageList(&CShellIcons::SmallList(), TVSIL_NORMAL);
    CTreeView::OnInitialUpdate(); // -> OnUpdate(HINT 0) builds the tree
}

void CBrowserTreeView::CancelJobs()
{
    for (auto& job : m_jobs)
        job->cancel = true;
    m_jobs.clear();
}

// ---------------------------------------------------------------------------
// Building the tree

void CBrowserTreeView::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
    CTreeCtrl& tree = GetTreeCtrl();
    CDiskForgeDoc* doc = GetDocument();

    if (lHint == CDiskForgeDoc::HINT_FILTER_CHANGED)
    {
        ResetDirectories(TVI_ROOT);
        return;
    }

    // Initial update or disks re-enumerated: start over.
    CancelJobs();
    tree.SetRedraw(FALSE);
    tree.DeleteAllItems();
    HTREEITEM first = nullptr;
    for (int i = 0; i < (int)doc->m_disks.size(); ++i)
    {
        BrowserNode* n = doc->NewNode(NodeKind::Disk, nullptr);
        n->disk = i;
        n->sectors = doc->m_disks[i].drive.totalSectors;
        HTREEITEM h = InsertNode(n, TVI_ROOT, true);
        if (!first)
            first = h;
    }
    tree.SetRedraw(TRUE);

    if (doc->m_disks.empty())
    {
        if (CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame()))
            frame->Log(_T("No physical disks could be opened - DiskForge must run as Administrator."));
    }
    if (first)
        tree.SelectItem(first);
}

HTREEITEM CBrowserTreeView::InsertNode(BrowserNode* node, HTREEITEM parent, bool hasChildren)
{
    CTreeCtrl& tree = GetTreeCtrl();
    TVINSERTSTRUCT tvi = {};
    tvi.hParent = parent;
    tvi.hInsertAfter = TVI_LAST;
    tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE;
    CString title = NodeTitle(node);
    tvi.item.pszText = const_cast<LPTSTR>(title.GetString());
    tvi.item.iImage = NodeImage(node, false);
    tvi.item.iSelectedImage = NodeImage(node, true);
    tvi.item.lParam = (LPARAM)node;
    if (node->kind == NodeKind::Directory && node->entry.isDeleted)
    {
        tvi.item.state = INDEXTOOVERLAYMASK(CShellIcons::kDeletedOverlay);
        tvi.item.stateMask = TVIS_OVERLAYMASK;
    }
    HTREEITEM h = tree.InsertItem(&tvi);
    if (hasChildren)
        tree.InsertItem(_T(""), h); // placeholder so the "+" shows; replaced on expand
    return h;
}

CString CBrowserTreeView::NodeTitle(const BrowserNode* node) const
{
    const CDiskForgeDoc* doc = GetDocument();
    CString t;
    switch (node->kind)
    {
    case NodeKind::Disk:
    {
        const DriveInfo& d = doc->m_disks[node->disk].drive;
        if (d.driveIndex == 0xFFFFFFFF) // disk image file
            t.Format(_T("%s (%s)"), d.model.GetString(), IFileSystem::FormatSize(d.totalBytes).GetString());
        else
            t.Format(_T("Disk %u - %s (%s)"), d.driveIndex, d.model.GetString(),
                     IFileSystem::FormatSize(d.totalBytes).GetString());
        break;
    }
    case NodeKind::PartitionTable:
    {
        const DiskData& dd = doc->m_disks[node->disk];
        CString scheme = dd.scheme.IsEmpty() ? CString(_T("unreadable")) : dd.scheme;
        int paren = scheme.Find(_T(" ("));
        if (paren > 0 && scheme.Left(3) != _T("Non"))
            scheme = scheme.Left(paren);
        t.Format(_T("Partition Table (%s)"), scheme.GetString());
        break;
    }
    case NodeKind::Partition:
    {
        const DiskData& dd = doc->m_disks[node->disk];
        const PartitionEntry& pe = dd.parts[node->part];
        CString type = !pe.name.IsEmpty() ? pe.name : pe.typeName;
        t.Format(_T("#%d  %s"), pe.index, type.GetString());
        if (!dd.partFs[node->part].IsEmpty())
            t += _T("  -  ") + dd.partFs[node->part];
        t += _T("  -  ") + IFileSystem::FormatSize(pe.sizeBytes);
        if (!dd.partLetters[node->part].IsEmpty())
            t += _T("  [") + dd.partLetters[node->part] + _T("]");
        break;
    }
    case NodeKind::BootRecord:
        t.Format(_T("Boot Record (%s)"), node->fs ? node->fs->KindName().GetString() : _T("?"));
        break;
    case NodeKind::Allocation:
        t = node->fs ? node->fs->AllocationTitle() : CString(_T("File Allocation"));
        break;
    case NodeKind::Directory:
        if (!node->parent || node->parent->kind == NodeKind::Allocation)
            t = _T("File System (\\)");
        else
            t = node->entry.name + (node->entry.isDeleted ? _T("  (deleted)") : _T(""));
        break;
    case NodeKind::Orphans:
        t = _T("Orphaned deleted files");
        break;
    }
    return t;
}

int CBrowserTreeView::NodeImage(const BrowserNode* node, bool selected) const
{
    switch (node->kind)
    {
    case NodeKind::Disk:           return CShellIcons::Disk;
    case NodeKind::PartitionTable: return CShellIcons::Table;
    case NodeKind::Partition:      return CShellIcons::Partition;
    case NodeKind::BootRecord:     return CShellIcons::Boot;
    case NodeKind::Allocation:     return CShellIcons::Allocation;
    case NodeKind::Orphans:        return CShellIcons::Orphans;
    default:                       return selected ? CShellIcons::FolderOpen : CShellIcons::Folder;
    }
}

void CBrowserTreeView::ResetChildren(HTREEITEM item)
{
    CTreeCtrl& tree = GetTreeCtrl();
    HTREEITEM child = tree.GetChildItem(item);
    while (child)
    {
        HTREEITEM next = tree.GetNextSiblingItem(child);
        tree.DeleteItem(child);
        child = next;
    }
}

void CBrowserTreeView::ResetDirectories(HTREEITEM item)
{
    // A filter change alters which folders are shown: collapse every
    // populated folder back to an unexpanded "+" (listings stay cached).
    CTreeCtrl& tree = GetTreeCtrl();
    for (HTREEITEM child = tree.GetChildItem(item); child; child = tree.GetNextSiblingItem(child))
    {
        BrowserNode* n = (BrowserNode*)tree.GetItemData(child);
        if (n && (n->kind == NodeKind::Directory || n->kind == NodeKind::Orphans) && n->populated)
        {
            bool selectedInside = false;
            for (HTREEITEM s = tree.GetSelectedItem(); s; s = tree.GetParentItem(s))
                selectedInside = selectedInside || (tree.GetParentItem(s) == child);
            if (selectedInside)
                tree.SelectItem(child);
            tree.Expand(child, TVE_COLLAPSE);
            ResetChildren(child);
            n->populated = false;
            tree.InsertItem(_T(""), child);
        }
        else
            ResetDirectories(child);
    }
}

void CBrowserTreeView::Populate(HTREEITEM item)
{
    CTreeCtrl& tree = GetTreeCtrl();
    BrowserNode* node = (BrowserNode*)tree.GetItemData(item);
    if (!node || node->populated)
        return;

    CDiskForgeDoc* doc = GetDocument();
    CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame());

    if ((node->kind == NodeKind::Directory || node->kind == NodeKind::Orphans) && node->fs &&
        (node->indexing || node->fs->NeedsIndex()))
    {
        StartIndex(node); // children appear when the scan finishes
        return;
    }

    tree.SetRedraw(FALSE);
    ResetChildren(item);
    node->populated = true;

    switch (node->kind)
    {
    case NodeKind::Disk:
    {
        doc->EnsurePartitions(node->disk);
        InsertNode(doc->NewNode(NodeKind::PartitionTable, node), item, !doc->m_disks[node->disk].parts.empty());
        // The title shows the scheme, which is only known now.
        tree.SetItemText(item, NodeTitle(node));
        break;
    }
    case NodeKind::PartitionTable:
    {
        DiskData& dd = doc->m_disks[node->disk];
        for (int i = 0; i < (int)dd.parts.size(); ++i)
        {
            BrowserNode* p = doc->NewNode(NodeKind::Partition, node);
            p->part = i;
            p->startLBA = dd.parts[i].startLBA;
            p->sectors = dd.parts[i].sectorCount;
            InsertNode(p, item, !dd.partFs[i].IsEmpty());
        }
        break;
    }
    case NodeKind::Partition:
    {
        CString err;
        if (doc->OpenFileSystem(node, err))
            InsertNode(doc->NewNode(NodeKind::BootRecord, node), item, true);
        else if (frame)
            frame->Log(_T("Partition: ") + err);
        break;
    }
    case NodeKind::BootRecord:
        InsertNode(doc->NewNode(NodeKind::Allocation, node), item, true);
        break;
    case NodeKind::Allocation:
    {
        BrowserNode* root = doc->NewNode(NodeKind::Directory, node);
        root->entry = node->fs->Root();
        InsertNode(root, item, true);
        break;
    }
    case NodeKind::Directory:
    case NodeKind::Orphans:
    {
        doc->ListNode(node);
        for (const FsEntry& e : node->listing)
        {
            // Live folders stay visible in "Deleted Only" mode - they lead to deleted files.
            if (!e.isDirectory || (e.isDeleted && !doc->m_showDeleted))
                continue;
            BrowserNode* d = doc->NewNode(NodeKind::Directory, node);
            d->entry = e;
            InsertNode(d, item, true);
        }
        // The NTFS root also collects deleted entries whose folder is gone.
        if (node->kind == NodeKind::Directory && node->parent && node->parent->kind == NodeKind::Allocation &&
            node->fs->HasOrphans() && doc->m_showDeleted)
            InsertNode(doc->NewNode(NodeKind::Orphans, node), item, true);
        break;
    }
    }
    tree.SetRedraw(TRUE);
}

// ---------------------------------------------------------------------------
// NTFS MFT scan

void CBrowserTreeView::StartIndex(BrowserNode* node)
{
    if (!node || !node->fs || node->indexing || !node->fs->NeedsIndex())
        return;
    // Only one scan per filesystem, from whichever node asked first.
    for (auto& j : m_jobs)
        if (j->fs == node->fs)
            return;

    auto job = std::make_shared<IndexJob>();
    job->fs = node->fs;
    job->node = node;
    job->generation = GetDocument()->m_generation;
    job->hwnd = GetSafeHwnd();
    m_jobs.push_back(job);
    node->indexing = true;

    if (HTREEITEM item = FindItem(node))
    {
        ResetChildren(item);
        GetTreeCtrl().InsertItem(_T("Scanning the Master File Table..."), item);
        GetTreeCtrl().Expand(item, TVE_EXPAND);
    }
    if (CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame()))
        frame->Log(_T("Scanning the NTFS Master File Table to find live and deleted files..."));

    auto* param = new std::shared_ptr<IndexJob>(job);
    if (!AfxBeginThread(IndexThread, param, THREAD_PRIORITY_BELOW_NORMAL))
    {
        delete param;
        node->indexing = false;
        m_jobs.pop_back();
    }
}

UINT CBrowserTreeView::IndexThread(LPVOID param)
{
    std::shared_ptr<IndexJob> job = *static_cast<std::shared_ptr<IndexJob>*>(param);
    delete static_cast<std::shared_ptr<IndexJob>*>(param);

    IndexJob* raw = job.get();
    job->ok = job->fs->BuildIndex([raw](int pct)
    {
        ::PostMessage(raw->hwnd, WM_INDEX_PROGRESS, (WPARAM)pct, (LPARAM)raw);
        return !raw->cancel.load();
    }, job->error);

    ::PostMessage(job->hwnd, WM_INDEX_DONE, 0, (LPARAM)new std::shared_ptr<IndexJob>(job));
    return 0;
}

LRESULT CBrowserTreeView::OnIndexProgress(WPARAM wParam, LPARAM lParam)
{
    IndexJob* raw = (IndexJob*)lParam;
    for (auto& job : m_jobs)
    {
        if (job.get() != raw)
            continue;
        if (HTREEITEM item = FindItem(job->node))
        {
            HTREEITEM child = GetTreeCtrl().GetChildItem(item);
            CString text;
            text.Format(_T("Scanning the Master File Table... %d%%"), (int)wParam);
            if (child)
                GetTreeCtrl().SetItemText(child, text);
            if (CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame()))
                frame->SetStatusText(text);
        }
    }
    return 0;
}

LRESULT CBrowserTreeView::OnIndexDone(WPARAM, LPARAM lParam)
{
    std::unique_ptr<std::shared_ptr<IndexJob>> holder((std::shared_ptr<IndexJob>*)lParam);
    std::shared_ptr<IndexJob> job = *holder;

    auto it = std::find(m_jobs.begin(), m_jobs.end(), job);
    if (it == m_jobs.end() || job->generation != GetDocument()->m_generation)
        return 0; // cancelled by a refresh - the node is gone
    m_jobs.erase(it);

    BrowserNode* node = job->node;
    node->indexing = false;
    CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame());
    if (frame)
    {
        frame->Log(job->ok ? CString(_T("MFT scan complete.")) : _T("MFT scan failed: ") + job->error);
        frame->SetStatusText(job->ok ? _T("MFT scan complete") : _T("MFT scan failed"));
    }

    HTREEITEM item = FindItem(node);
    if (!item)
        return 0;
    ResetChildren(item);
    node->populated = false;
    if (job->ok)
    {
        Populate(item);
        GetTreeCtrl().Expand(item, TVE_EXPAND);
    }
    else
    {
        node->listed = true;
        node->listError = job->error;
    }
    if (GetTreeCtrl().GetSelectedItem() == item && frame)
        frame->OnBrowserNodeSelected(node); // refresh the content list
    return 0;
}

// ---------------------------------------------------------------------------
// Selection and navigation

BrowserNode* CBrowserTreeView::SelectedNode() const
{
    HTREEITEM item = GetTreeCtrl().GetSelectedItem();
    return item ? (BrowserNode*)GetTreeCtrl().GetItemData(item) : nullptr;
}

HTREEITEM CBrowserTreeView::FindItem(BrowserNode* node, HTREEITEM from) const
{
    CTreeCtrl& tree = GetTreeCtrl();
    for (HTREEITEM h = tree.GetChildItem(from); h; h = tree.GetNextSiblingItem(h))
    {
        if ((BrowserNode*)tree.GetItemData(h) == node)
            return h;
        if (HTREEITEM found = FindItem(node, h))
            return found;
    }
    return nullptr;
}

void CBrowserTreeView::OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMTREEVIEW* nm = reinterpret_cast<NMTREEVIEW*>(pNMHDR);
    *pResult = 0;
    if (nm->action == TVE_EXPAND)
        Populate(nm->itemNew.hItem);
}

void CBrowserTreeView::OnSelChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    BrowserNode* node = SelectedNode();
    if (!node)
        return;

    // Opening a File System node starts the MFT scan straight away.
    if (node->kind == NodeKind::Directory && node->fs && node->fs->NeedsIndex())
        StartIndex(node);

    if (CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame()))
        frame->OnBrowserNodeSelected(node);
}

bool CBrowserTreeView::OpenChildDirectory(const FsEntry& entry)
{
    CTreeCtrl& tree = GetTreeCtrl();
    HTREEITEM item = tree.GetSelectedItem();
    if (!item)
        return false;
    Populate(item);
    tree.Expand(item, TVE_EXPAND);
    for (HTREEITEM c = tree.GetChildItem(item); c; c = tree.GetNextSiblingItem(c))
    {
        BrowserNode* n = (BrowserNode*)tree.GetItemData(c);
        if (n && n->kind == NodeKind::Directory && n->entry.id == entry.id &&
            n->entry.isDeleted == entry.isDeleted && n->entry.name == entry.name)
        {
            tree.SelectItem(c);
            tree.EnsureVisible(c);
            return true;
        }
    }
    return false;
}

bool CBrowserTreeView::OpenChildPartition(int partIndex)
{
    CTreeCtrl& tree = GetTreeCtrl();
    HTREEITEM item = tree.GetSelectedItem();
    if (!item)
        return false;
    Populate(item);
    tree.Expand(item, TVE_EXPAND);
    for (HTREEITEM c = tree.GetChildItem(item); c; c = tree.GetNextSiblingItem(c))
    {
        BrowserNode* n = (BrowserNode*)tree.GetItemData(c);
        if (n && n->kind == NodeKind::Partition && n->part == partIndex)
        {
            tree.SelectItem(c);
            tree.EnsureVisible(c);
            return true;
        }
    }
    return false;
}

bool CBrowserTreeView::SelectParent()
{
    CTreeCtrl& tree = GetTreeCtrl();
    HTREEITEM item = tree.GetSelectedItem();
    HTREEITEM parent = item ? tree.GetParentItem(item) : nullptr;
    if (!parent)
        return false;
    tree.SelectItem(parent);
    return true;
}

void CBrowserTreeView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMTVCUSTOMDRAW* cd = reinterpret_cast<NMTVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
        *pResult = CDRF_NOTIFYITEMDRAW;
    else if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
    {
        BrowserNode* n = (BrowserNode*)cd->nmcd.lItemlParam;
        bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
        if (n && !selected && n->kind == NodeKind::Directory && n->entry.isDeleted)
            cd->clrText = kDeletedText;
    }
}

void CBrowserTreeView::OnRButtonDown(UINT /*nFlags*/, CPoint point)
{
    UINT flags = 0;
    HTREEITEM item = GetTreeCtrl().HitTest(point, &flags);
    if (item)
        GetTreeCtrl().SelectItem(item);
    ClientToScreen(&point);
    OnContextMenu(this, point);
}

void CBrowserTreeView::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
    theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_BROWSER, point.x, point.y, AfxGetMainWnd(), TRUE);
}
