// BrowserListView.cpp - System Browser content list (right splitter pane)
#include "pch.h"
#include "DiskForge.h"
#include "BrowserListView.h"
#include "BrowserTreeView.h"
#include "MainFrm.h"
#include "ShellIcons.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    const COLORREF kDeletedText = RGB(170, 40, 40);

    COLORREF RecoverColor(Recoverability r)
    {
        switch (r)
        {
        case Recoverability::Excellent:
        case Recoverability::Good:    return RGB(0, 128, 0);
        case Recoverability::Partial: return RGB(190, 110, 0);
        case Recoverability::Poor:
        case Recoverability::None:    return RGB(190, 0, 0);
        default:                      return CLR_DEFAULT;
        }
    }

    enum EntryColumn { ColName, ColStatus, ColSize, ColModified, ColCreated, ColRecover, ColLocation, ColNotes };
}

IMPLEMENT_DYNCREATE(CBrowserListView, CListView)

BEGIN_MESSAGE_MAP(CBrowserListView, CListView)
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, &CBrowserListView::OnItemChanged)
    ON_NOTIFY_REFLECT(NM_DBLCLK, &CBrowserListView::OnDblClk)
    ON_NOTIFY_REFLECT(NM_RETURN, &CBrowserListView::OnReturn)
    ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, &CBrowserListView::OnColumnClick)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CBrowserListView::OnCustomDraw)
    ON_WM_CONTEXTMENU()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

BOOL CBrowserListView::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.style &= ~LVS_TYPEMASK;
    cs.style |= LVS_REPORT | LVS_SHOWSELALWAYS;
    return CListView::PreCreateWindow(cs);
}

void CBrowserListView::OnInitialUpdate()
{
    CListCtrl& list = GetListCtrl();
    list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);
    list.SetImageList(&CShellIcons::SmallList(), LVSIL_SMALL);
    CListView::OnInitialUpdate();
}

void CBrowserListView::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
    if (lHint == CDiskForgeDoc::HINT_FILTER_CHANGED)
    {
        Refresh();
        return;
    }
    // Disks re-enumerated (or first update): old nodes are gone - unless the
    // tree (updated first) has already shown one of the new ones here.
    if (m_node && m_nodeGeneration == GetDocument()->m_generation)
        return;
    m_node = nullptr;
    ShowMessage(_T("Select a disk in the System Browser."));
}

// ---------------------------------------------------------------------------
// Filling

void CBrowserListView::SetColumns(const std::vector<std::pair<LPCTSTR, int>>& cols)
{
    CListCtrl& list = GetListCtrl();
    list.DeleteAllItems();
    while (list.DeleteColumn(0)) {}
    int i = 0;
    for (const auto& c : cols)
        list.InsertColumn(i++, c.first, LVCFMT_LEFT, c.second);
    list.EnableGroupView(FALSE);
    list.RemoveAllGroups();
}

void CBrowserListView::ShowMessage(const CString& text)
{
    m_mode = Mode::Message;
    SetColumns({ { _T(""), 900 } });
    GetListCtrl().InsertItem(0, text, CShellIcons::Field);
}

void CBrowserListView::ShowNode(BrowserNode* node)
{
    m_node = node;
    m_nodeGeneration = GetDocument()->m_generation;
    CListCtrl& list = GetListCtrl();
    list.SetRedraw(FALSE);

    CDiskForgeDoc* doc = GetDocument();
    if (!node)
        ShowMessage(_T("Select a disk in the System Browser."));
    else switch (node->kind)
    {
    case NodeKind::PartitionTable:
        ShowPartitions();
        break;
    case NodeKind::Directory:
    case NodeKind::Orphans:
        if (node->indexing || (node->fs && node->fs->NeedsIndex()))
            ShowMessage(_T("Scanning the Master File Table to find live and deleted files - this can take a minute ")
                        _T("on a large volume. The folder tree appears when it finishes."));
        else
            ShowEntries();
        break;
    default:
    {
        std::vector<FieldRow> fields;
        NodeFields(doc, node, fields);
        ShowFields(fields);
        break;
    }
    }

    list.SetRedraw(TRUE);
    list.Invalidate();
}

void CBrowserListView::ShowFields(const std::vector<FieldRow>& fields)
{
    m_mode = Mode::Fields;
    m_fields = fields;
    SetColumns({ { _T("Field"), 220 }, { _T("Value"), 300 }, { _T("Offset"), 60 }, { _T("Description"), 380 } });

    CListCtrl& list = GetListCtrl();
    list.EnableGroupView(TRUE);
    std::vector<CString> groups;
    for (int i = 0; i < (int)m_fields.size(); ++i)
    {
        const FieldRow& f = m_fields[i];
        auto it = std::find(groups.begin(), groups.end(), f.group);
        int groupId = (int)(it - groups.begin());
        if (it == groups.end())
        {
            groups.push_back(f.group);
            LVGROUP g = {};
            g.cbSize = sizeof(g);
            g.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
            g.state = LVGS_NORMAL;
            g.stateMask = LVGS_NORMAL;
            CString header = f.group.IsEmpty() ? CString(_T("General")) : f.group;
            g.pszHeader = const_cast<LPWSTR>(header.GetString());
            g.iGroupId = groupId;
            list.InsertGroup(groupId, &g);
        }

        LVITEM item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID | LVIF_IMAGE;
        item.iItem = i;
        item.pszText = const_cast<LPTSTR>(f.name.GetString());
        item.lParam = i;
        item.iGroupId = groupId;
        item.iImage = CShellIcons::Field;
        int idx = list.InsertItem(&item);
        list.SetItemText(idx, 1, f.value);
        list.SetItemText(idx, 2, f.offset);
        list.SetItemText(idx, 3, f.description);
    }
}

void CBrowserListView::ShowPartitions()
{
    m_mode = Mode::Partitions;
    CDiskForgeDoc* doc = GetDocument();
    DiskData& dd = doc->m_disks[m_node->disk];
    doc->EnsurePartitions(m_node->disk);
    if (!dd.error.IsEmpty())
    {
        ShowMessage(_T("Could not read the partition table: ") + dd.error);
        return;
    }
    if (dd.parts.empty())
    {
        ShowMessage(_T("No partitions are defined on this disk (") + dd.scheme + _T(")."));
        return;
    }

    SetColumns({ { _T("#"), 50 }, { _T("Type"), 200 }, { _T("Filesystem"), 80 }, { _T("Drive"), 50 },
                 { _T("Start LBA"), 100 }, { _T("End LBA"), 100 }, { _T("Sectors"), 100 }, { _T("Size"), 80 },
                 { _T("Name / Type ID"), 260 }, { _T("Flags"), 90 } });
    CListCtrl& list = GetListCtrl();
    for (int i = 0; i < (int)dd.parts.size(); ++i)
    {
        const PartitionEntry& pe = dd.parts[i];
        CString s;
        s.Format(_T("%d"), pe.index);
        int idx = list.InsertItem(i, s, CShellIcons::Partition);
        list.SetItemData(idx, i);
        list.SetItemText(idx, 1, pe.typeName);
        list.SetItemText(idx, 2, dd.partFs[i].IsEmpty() ? CString(_T("-")) : dd.partFs[i]);
        list.SetItemText(idx, 3, dd.partLetters[i]);
        s.Format(_T("%llu"), pe.startLBA);                                  list.SetItemText(idx, 4, s);
        s.Format(_T("%llu"), pe.sectorCount ? pe.startLBA + pe.sectorCount - 1 : pe.startLBA);
                                                                            list.SetItemText(idx, 5, s);
        s.Format(_T("%llu"), pe.sectorCount);                               list.SetItemText(idx, 6, s);
        list.SetItemText(idx, 7, IFileSystem::FormatSize(pe.sizeBytes));
        list.SetItemText(idx, 8, !pe.name.IsEmpty() ? pe.name + _T("  ") + pe.typeId : pe.typeId);
        CString flags;
        if (pe.bootable)  flags += pe.scheme == _T("GPT") ? _T("ESP ") : _T("Active ");
        if (pe.isLogical) flags += _T("Logical");
        list.SetItemText(idx, 9, flags);
    }
}

void CBrowserListView::ShowEntries()
{
    CDiskForgeDoc* doc = GetDocument();
    if (!doc->ListNode(m_node))
    {
        ShowMessage(m_node->listError.IsEmpty() ? CString(_T("This folder could not be read.")) : m_node->listError);
        return;
    }

    m_mode = Mode::Entries;
    bool ntfs = m_node->fs && m_node->fs->Kind() == FsKind::NTFS;
    SetColumns({ { _T("Name"), 260 }, { _T("Status"), 70 }, { _T("Size"), 90 }, { _T("Modified"), 130 },
                 { _T("Created"), 130 }, { _T("Recoverable"), 100 },
                 { ntfs ? _T("MFT Record") : _T("First Cluster"), 90 }, { _T("Notes"), 480 } });

    std::vector<int> rows;
    for (int i = 0; i < (int)m_node->listing.size(); ++i)
        if (doc->PassesFilter(m_node->listing[i]))
            rows.push_back(i);

    if (m_sortColumn >= 0)
    {
        const auto& L = m_node->listing;
        int col = m_sortColumn;
        bool asc = m_sortAscending;
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b)
        {
            const FsEntry& x = L[a];
            const FsEntry& y = L[b];
            int c = 0;
            switch (col)
            {
            case ColStatus:   c = (int)x.isDeleted - (int)y.isDeleted; break;
            case ColSize:     c = x.size < y.size ? -1 : x.size > y.size ? 1 : 0; break;
            case ColModified: c = CompareFileTime(&x.modified, &y.modified); break;
            case ColCreated:  c = CompareFileTime(&x.created, &y.created); break;
            case ColRecover:  c = (int)x.recover - (int)y.recover; break;
            case ColLocation: c = x.id < y.id ? -1 : x.id > y.id ? 1 : 0; break;
            default:          c = x.name.CompareNoCase(y.name); break;
            }
            return asc ? c < 0 : c > 0;
        });
    }

    CListCtrl& list = GetListCtrl();
    int live = 0, deleted = 0;
    for (int i : rows)
    {
        const FsEntry& e = m_node->listing[i];
        LVITEM item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_STATE;
        item.iItem = list.GetItemCount();
        item.pszText = const_cast<LPTSTR>(e.name.GetString());
        item.lParam = i;
        item.iImage = e.isDirectory ? CShellIcons::Folder : CShellIcons::File;
        if (e.isDeleted)
        {
            item.state = INDEXTOOVERLAYMASK(CShellIcons::kDeletedOverlay);
            item.stateMask = LVIS_OVERLAYMASK;
        }
        int idx = list.InsertItem(&item);
        list.SetItemText(idx, ColStatus, e.isDeleted ? _T("Deleted") : _T("Live"));
        list.SetItemText(idx, ColSize, e.isDirectory ? CString() : IFileSystem::FormatSize(e.size));
        list.SetItemText(idx, ColModified, IFileSystem::FormatFileTime(e.modified));
        list.SetItemText(idx, ColCreated, IFileSystem::FormatFileTime(e.created));
        list.SetItemText(idx, ColRecover, IFileSystem::RecoverabilityText(e.recover));
        CString loc;
        loc.Format(_T("%llu"), e.id);
        list.SetItemText(idx, ColLocation, loc);
        list.SetItemText(idx, ColNotes, e.recoverNote);
        (e.isDeleted ? deleted : live)++;
    }

    if (CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame()))
    {
        CString s;
        s.Format(_T("%d live, %d deleted item(s) shown"), live, deleted);
        frame->SetStatusText(s);
    }
}

// ---------------------------------------------------------------------------
// Node descriptions

void CBrowserListView::NodeFields(CDiskForgeDoc* doc, BrowserNode* node, std::vector<FieldRow>& out)
{
    if (!node)
        return;
    auto add = [&](LPCTSTR g, LPCTSTR n, const CString& v, LPCTSTR d = nullptr)
    {
        FieldRow r;
        r.group = g; r.name = n; r.value = v;
        if (d) r.description = d;
        out.push_back(r);
    };
    CString v;

    switch (node->kind)
    {
    case NodeKind::Disk:
    case NodeKind::PartitionTable:
    {
        DiskData& dd = doc->m_disks[node->disk];
        doc->EnsurePartitions(node->disk);
        LPCTSTR g = _T("Physical Disk");
        add(g, _T("Device"), dd.drive.devicePath);
        add(g, _T("Model"), dd.drive.model);
        v.Format(_T("%llu bytes (%s)"), dd.drive.totalBytes, IFileSystem::FormatSize(dd.drive.totalBytes).GetString());
        add(g, _T("Capacity"), v);
        v.Format(_T("%u bytes"), dd.drive.bytesPerSector);   add(g, _T("Sector size"), v);
        v.Format(_T("%llu"), dd.drive.totalSectors);         add(g, _T("Total sectors"), v);
        g = _T("Partitioning");
        add(g, _T("Scheme"), dd.scheme.IsEmpty() ? dd.error : dd.scheme);
        if (!dd.diskGuid.IsEmpty())
            add(g, _T("Disk GUID"), dd.diskGuid);
        v.Format(_T("%u"), (UINT)dd.parts.size());           add(g, _T("Partitions"), v);
        add(g, _T("Table location"), dd.scheme.Left(3) == _T("GPT")
            ? CString(_T("LBA 0 protective MBR, LBA 1 GPT header, LBA 2+ entries")) : CString(_T("LBA 0 (MBR)")));
        break;
    }
    case NodeKind::Partition:
    {
        DiskData& dd = doc->m_disks[node->disk];
        const PartitionEntry& pe = dd.parts[node->part];
        LPCTSTR g = _T("Partition Entry");
        v.Format(_T("%d"), pe.index);                        add(g, _T("Number"), v);
        add(g, _T("Scheme"), pe.scheme);
        add(g, _T("Type"), pe.typeName);
        add(g, _T("Type ID"), pe.typeId);
        if (!pe.name.IsEmpty())
            add(g, _T("Name"), pe.name);
        v.Format(_T("%llu"), pe.startLBA);                   add(g, _T("Start LBA"), v);
        v.Format(_T("%llu"), pe.sectorCount);                add(g, _T("Sectors"), v);
        add(g, _T("Size"), IFileSystem::FormatSize(pe.sizeBytes));
        add(g, _T("Bootable / ESP"), pe.bootable ? _T("Yes") : _T("No"));
        if (pe.scheme == _T("MBR"))
            add(g, _T("Logical (in extended)"), pe.isLogical ? _T("Yes") : _T("No"));
        g = _T("Volume");
        add(g, _T("Filesystem"), dd.partFs[node->part].IsEmpty() ? CString(_T("Not recognised")) : dd.partFs[node->part],
            _T("Detected from the boot record at the partition's first sector"));
        add(g, _T("Mounted as"), dd.partLetters[node->part].IsEmpty() ? CString(_T("-")) : dd.partLetters[node->part]);
        break;
    }
    case NodeKind::BootRecord:
        if (node->fs)
            node->fs->BootRecordFields(out);
        break;
    case NodeKind::Allocation:
        if (node->fs)
        {
            CWaitCursor wait; // FAT statistics scan the whole table once
            node->fs->AllocationFields(out);
        }
        break;
    case NodeKind::Directory:
        if (node->fs)
            node->fs->EntryFields(node->entry, out);
        break;
    case NodeKind::Orphans:
        add(_T("Orphaned Entries"), _T("What"), _T("Deleted files and folders whose parent folder record has been reused"));
        break;
    }
}

// ---------------------------------------------------------------------------
// Selection

std::vector<FsEntry> CBrowserListView::SelectedEntries() const
{
    std::vector<FsEntry> out;
    if (m_mode != Mode::Entries || !m_node)
        return out;
    CListCtrl& list = GetListCtrl();
    POSITION pos = list.GetFirstSelectedItemPosition();
    while (pos)
    {
        int i = list.GetNextSelectedItem(pos);
        size_t idx = (size_t)list.GetItemData(i);
        if (idx < m_node->listing.size())
            out.push_back(m_node->listing[idx]);
    }
    return out;
}

const FsEntry* CBrowserListView::FocusedEntry() const
{
    if (m_mode != Mode::Entries || !m_node)
        return nullptr;
    CListCtrl& list = GetListCtrl();
    int i = list.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED);
    if (i < 0)
        i = list.GetNextItem(-1, LVNI_SELECTED);
    if (i < 0)
        return nullptr;
    size_t idx = (size_t)list.GetItemData(i);
    return idx < m_node->listing.size() ? &m_node->listing[idx] : nullptr;
}

int CBrowserListView::FocusedPartition() const
{
    if (m_mode != Mode::Partitions)
        return -1;
    int i = GetListCtrl().GetNextItem(-1, LVNI_SELECTED);
    return i < 0 ? -1 : (int)GetListCtrl().GetItemData(i);
}

void CBrowserListView::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLISTVIEW* nm = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    *pResult = 0;
    if (!(nm->uChanged & LVIF_STATE) || !(nm->uNewState & LVIS_SELECTED) || (nm->uOldState & LVIS_SELECTED))
        return;
    CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame());
    if (!frame || !m_node)
        return;

    if (m_mode == Mode::Entries)
    {
        if (const FsEntry* e = FocusedEntry())
            frame->OnBrowserEntrySelected(m_node, *e);
    }
    else if (m_mode == Mode::Partitions)
        frame->OnBrowserPartitionSelected(m_node, FocusedPartition());
    else if (m_mode == Mode::Fields && nm->lParam >= 0 && nm->lParam < (LPARAM)m_fields.size())
        frame->ShowProperties(m_fields[nm->lParam].name, { m_fields[nm->lParam] });
}

void CBrowserListView::OpenFocused()
{
    CMainFrame* frame = DYNAMIC_DOWNCAST(CMainFrame, GetParentFrame());
    if (!frame)
        return;
    CBrowserTreeView* tree = frame->GetTreeView();
    if (m_mode == Mode::Partitions)
    {
        int part = FocusedPartition();
        if (part >= 0 && tree)
            tree->OpenChildPartition(part);
    }
    else if (m_mode == Mode::Entries)
    {
        const FsEntry* e = FocusedEntry();
        if (!e)
            return;
        if (e->isDirectory && tree)
        {
            FsEntry copy = *e; // the listing is about to be replaced
            if (!tree->OpenChildDirectory(copy))
                frame->Log(_T("That folder is hidden by the current filter."));
        }
        else
            frame->SendMessage(WM_COMMAND, ID_BROWSE_SHOW_DATA);
    }
}

void CBrowserListView::OnDblClk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    OpenFocused();
}

void CBrowserListView::OnReturn(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    OpenFocused();
}

void CBrowserListView::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLISTVIEW* nm = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    *pResult = 0;
    if (m_mode != Mode::Entries)
        return;
    if (m_sortColumn == nm->iSubItem)
        m_sortAscending = !m_sortAscending;
    else
        m_sortColumn = nm->iSubItem, m_sortAscending = true;
    Refresh();
}

void CBrowserListView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;
    switch (cd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;
    case CDDS_ITEMPREPAINT:
        *pResult = (m_mode == Mode::Entries) ? CDRF_NOTIFYSUBITEMDRAW : CDRF_DODEFAULT;
        break;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
    {
        size_t idx = (size_t)cd->nmcd.lItemlParam;
        if (!m_node || idx >= m_node->listing.size())
            break;
        const FsEntry& e = m_node->listing[idx];
        cd->clrText = CLR_DEFAULT;
        if (cd->iSubItem == ColRecover && e.isDeleted)
            cd->clrText = RecoverColor(e.recover);
        else if (e.isDeleted)
            cd->clrText = kDeletedText;
        break;
    }
    }
}

void CBrowserListView::OnRButtonUp(UINT /*nFlags*/, CPoint point)
{
    ClientToScreen(&point);
    OnContextMenu(this, point);
}

void CBrowserListView::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
    if (point.x == -1 && point.y == -1)
    {
        CRect rc;
        GetClientRect(&rc);
        point = rc.CenterPoint();
        ClientToScreen(&point);
    }
    theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_BROWSER, point.x, point.y, AfxGetMainWnd(), TRUE);
}
