// DiskForgeDoc.h - SDI document: the System Browser's model.
//
// Holds the enumerated physical disks, their parsed partition tables, and
// the nodes of the browser tree:
//
//   Disk -> Partition Table -> Partition -> Boot Record
//        -> File Allocation (FAT / $MFT / exFAT bitmap) -> File System -> folders...
//
// Views keep raw BrowserNode pointers in their item data; every pointer
// stays valid until the next RefreshDisks(), which bumps m_generation and
// notifies the views with HINT_DISKS_CHANGED so they drop them.
#pragma once
#include "DiskManager.h"
#include "PartitionParser.h"
#include "FileSystem.h"
#include "VolumeMap.h"

enum class NodeKind
{
    Disk,
    PartitionTable,
    Partition,
    BootRecord,
    Allocation,
    Directory,      // the File System root and every folder below it
    Orphans,        // NTFS: deleted entries whose folder no longer exists
};

struct DiskData
{
    DriveInfo                   drive;
    bool                        partsLoaded = false;
    CString                     scheme;
    CString                     diskGuid;
    CString                     error;
    std::vector<PartitionEntry> parts;
    std::vector<CString>        partFs;       // filesystem found in each partition's first sector
    std::vector<CString>        partLetters;  // mounted drive letters, if any
};

struct BrowserNode
{
    NodeKind     kind = NodeKind::Disk;
    BrowserNode* parent = nullptr;
    int          disk = -1;          // index into CDiskForgeDoc::m_disks
    int          part = -1;          // index into DiskData::parts
    ULONGLONG    startLBA = 0;       // volume range (Partition and below)
    ULONGLONG    sectors = 0;
    std::shared_ptr<IFileSystem> fs; // Boot Record and below
    FsEntry      entry;              // Directory
    bool         populated = false;  // tree children created
    bool         indexing = false;   // NTFS MFT scan running

    // Directory / Orphans: entries read from disk (unfiltered), cached so the
    // tree and the content list don't parse the same folder twice.
    bool                 listed = false;
    CString              listError;
    std::vector<FsEntry> listing;
};

class CDiskForgeDoc : public CDocument
{
    DECLARE_DYNCREATE(CDiskForgeDoc)
public:
    enum { HINT_DISKS_CHANGED = 1, HINT_FILTER_CHANGED = 2 };

    CDiskForgeDoc() = default;
    virtual ~CDiskForgeDoc() = default;

    std::vector<DiskData>     m_disks;
    std::vector<VolumeExtent> m_volumes;
    UINT                      m_generation = 0;

    // Browser filters (ribbon "Show Deleted" / "Deleted Only").
    bool m_showDeleted = true;
    bool m_deletedOnly = false;

    void         RefreshDisks();

    // Adds a raw disk image file (e.g. saved by Disk Copy) as an extra,
    // read-only "disk" in the browser. Kept across RefreshDisks().
    bool         AddImage(const CString& path, CString& errorOut);
    BrowserNode* NewNode(NodeKind kind, BrowserNode* parent);
    bool         EnsurePartitions(int disk);

    // Opens (once) the filesystem of a Partition node and shares it with its
    // descendants. Returns nullptr with errorOut when nothing is recognised.
    std::shared_ptr<IFileSystem> OpenFileSystem(BrowserNode* partitionNode, CString& errorOut);

    // Reads (once) the entries of a Directory or Orphans node into
    // node->listing. False (with node->listError) when the folder can't be read.
    bool ListNode(BrowserNode* node);

    // True when an entry passes the deleted-files filters.
    bool PassesFilter(const FsEntry& e) const
    {
        if (e.isDeleted && !m_showDeleted) return false;
        if (!e.isDeleted && m_deletedOnly) return false;
        return true;
    }

protected:
    virtual BOOL OnNewDocument() override;
    virtual void Serialize(CArchive&) override {}

    DECLARE_MESSAGE_MAP()

private:
    std::vector<std::unique_ptr<BrowserNode>> m_nodes;
    std::vector<CString>                      m_images;
};
