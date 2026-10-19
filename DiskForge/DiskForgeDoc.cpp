// DiskForgeDoc.cpp - System Browser model
#include "pch.h"
#include "DiskForgeDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CDiskForgeDoc, CDocument)
BEGIN_MESSAGE_MAP(CDiskForgeDoc, CDocument)
END_MESSAGE_MAP()

BOOL CDiskForgeDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;
    SetTitle(_T("System Browser"));
    RefreshDisks();
    return TRUE;
}

void CDiskForgeDoc::RefreshDisks()
{
    CWaitCursor wait;
    ++m_generation;
    m_nodes.clear();
    m_disks.clear();

    std::vector<DriveInfo> drives;
    CDiskManager dm;
    dm.EnumerateDrives(drives);
    for (const DriveInfo& d : drives)
    {
        DiskData dd;
        dd.drive = d;
        m_disks.push_back(dd);
    }
    for (const CString& path : m_images)
    {
        CDiskManager img;
        if (!img.OpenDrive(path))
            continue;
        DiskData dd;
        dd.drive.devicePath = path;
        dd.drive.model = _T("Image: ") + path.Mid(path.ReverseFind(_T('\\')) + 1);
        dd.drive.bytesPerSector = img.GetBytesPerSector();
        dd.drive.totalSectors = img.GetTotalSectors();
        dd.drive.totalBytes = img.GetTotalSectors() * img.GetBytesPerSector();
        dd.drive.driveIndex = 0xFFFFFFFF; // never matches a mounted volume
        dd.drive.displayName = dd.drive.model;
        m_disks.push_back(dd);
    }
    m_volumes = CVolumeMap::Enumerate();

    UpdateAllViews(nullptr, HINT_DISKS_CHANGED);
}

bool CDiskForgeDoc::AddImage(const CString& path, CString& errorOut)
{
    CDiskManager img;
    if (!img.OpenDrive(path))
    {
        errorOut = img.GetLastError();
        return false;
    }
    if (img.GetTotalSectors() == 0)
    {
        errorOut = _T("The image file is empty.");
        return false;
    }
    for (const CString& p : m_images)
        if (p.CompareNoCase(path) == 0)
            return true;
    m_images.push_back(path);
    RefreshDisks();
    return true;
}

BrowserNode* CDiskForgeDoc::NewNode(NodeKind kind, BrowserNode* parent)
{
    m_nodes.push_back(std::make_unique<BrowserNode>());
    BrowserNode* n = m_nodes.back().get();
    n->kind = kind;
    n->parent = parent;
    if (parent)
    {
        n->disk = parent->disk;
        n->part = parent->part;
        n->startLBA = parent->startLBA;
        n->sectors = parent->sectors;
        n->fs = parent->fs;
    }
    return n;
}

bool CDiskForgeDoc::EnsurePartitions(int disk)
{
    if (disk < 0 || disk >= (int)m_disks.size())
        return false;
    DiskData& dd = m_disks[disk];
    if (dd.partsLoaded)
        return dd.error.IsEmpty();
    dd.partsLoaded = true;

    CDiskManager dm;
    if (!dm.OpenDrive(dd.drive.devicePath))
    {
        dd.error = dm.GetLastError();
        return false;
    }

    // A filesystem boot record at LBA 0 means a "superfloppy" (USB stick
    // formatted without a partition table) - the boot code area would
    // otherwise be misread as MBR entries.
    std::vector<BYTE> sec0;
    CString fsAt0 = dm.ReadSector(0, sec0) ? IFileSystem::Identify(sec0) : CString();
    if (!fsAt0.IsEmpty())
    {
        dd.scheme = _T("None (whole-disk volume)");
        PartitionEntry pe;
        pe.index = 1;
        pe.scheme = _T("None");
        pe.typeName = _T("Whole-disk volume");
        pe.startLBA = 0;
        pe.sectorCount = dd.drive.totalSectors;
        pe.sizeBytes = dd.drive.totalBytes;
        dd.parts.push_back(pe);
    }
    else if (!CPartitionParser::Parse(dm, dd.parts, dd.scheme, dd.diskGuid, dd.error))
        return false;

    for (const PartitionEntry& pe : dd.parts)
    {
        CString fs;
        bool container = pe.mbrType == 0x05 || pe.mbrType == 0x0F || pe.mbrType == 0x85;
        std::vector<BYTE> boot;
        if (!container && pe.sectorCount > 0 && dm.ReadSector(pe.startLBA, boot))
            fs = IFileSystem::Identify(boot);
        dd.partFs.push_back(fs);
        dd.partLetters.push_back(CVolumeMap::LettersFor(m_volumes, dd.drive.driveIndex, pe.startLBA,
                                                        dd.drive.bytesPerSector));
    }
    return true;
}

std::shared_ptr<IFileSystem> CDiskForgeDoc::OpenFileSystem(BrowserNode* node, CString& errorOut)
{
    if (node->fs)
        return node->fs;
    if (node->disk < 0 || node->disk >= (int)m_disks.size())
    {
        errorOut = _T("Unknown disk.");
        return nullptr;
    }
    node->fs = IFileSystem::Detect(m_disks[node->disk].drive.devicePath, node->startLBA, node->sectors, errorOut);
    return node->fs;
}

bool CDiskForgeDoc::ListNode(BrowserNode* node)
{
    if (node->listed)
        return node->listError.IsEmpty();
    if (!node->fs || node->indexing || node->fs->NeedsIndex())
        return false; // caller starts / waits for the MFT scan first
    node->listed = true;
    node->listing.clear();
    node->listError.Empty();

    CWaitCursor wait;
    if (node->kind == NodeKind::Orphans)
        node->fs->ListOrphans(node->listing);
    else if (!node->fs->ListDirectory(node->entry, node->listing, node->listError) && node->listError.IsEmpty())
        node->listError = _T("The folder could not be read.");

    // Folders first, then by name; live before deleted for equal names.
    std::stable_sort(node->listing.begin(), node->listing.end(), [](const FsEntry& a, const FsEntry& b)
    {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        int c = a.name.CompareNoCase(b.name);
        if (c != 0) return c < 0;
        return !a.isDeleted && b.isDeleted;
    });
    return node->listError.IsEmpty();
}
