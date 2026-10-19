// FileSystem.h - Read-only filesystem layer behind the System Browser.
//
// The System Browser walks a disk the way the data is laid out on it:
//
//   Disk -> Partition Table -> Partition -> Boot Record
//        -> File Allocation (FAT / exFAT allocation bitmap / NTFS $MFT)
//        -> File System (directory tree, including deleted entries)
//
// IFileSystem is that walk for one volume. Implementations parse the
// on-disk structures directly from raw sectors (CDiskManager), so they work
// whether or not Windows has the volume mounted, and they never write to the
// disk. Recovery only ever writes to a NEW file the user picks.
//
// Every implementation opens its OWN handle to the physical drive, so the
// NTFS index can be built on a worker thread while the UI keeps reading
// sectors for the hex view through a different handle.
#pragma once
#include "pch.h"
#include "DiskManager.h"
#include <functional>

enum class FsKind { Unknown, FAT12, FAT16, FAT32, ExFAT, NTFS };

// How likely a deleted file is to come back intact, judged from whether its
// data clusters are still free (unallocated) in the volume's allocation map.
enum class Recoverability
{
    NotApplicable,  // live (not deleted) entry
    Excellent,      // data is resident inside the metadata record itself (NTFS small files)
    Good,           // every data cluster is still free - nothing has reused it
    Partial,        // some clusters were reused by other files - output will be partly corrupt
    Poor,           // most clusters reused / layout unknown - very unlikely to be useful
    None,           // nothing left to recover (no clusters recorded, or all reused)
};

// One name/value row describing an on-disk structure: shown in the content
// list for Boot Record / File Allocation nodes, and in the Properties pane.
struct FieldRow
{
    CString group;   // Property-grid group ("BIOS Parameter Block", ...)
    CString name;
    CString value;
    CString offset;  // byte offset inside the structure, e.g. "0x0B" (may be empty)
    CString description;
};

// A contiguous run of clusters belonging to a file (NTFS data run / FAT
// contiguous extent). lcn == -1 marks a sparse (unallocated, reads-as-zero) run.
struct ClusterRun
{
    LONGLONG  lcn = 0;
    ULONGLONG length = 0;
};

struct FsEntry
{
    CString   name;
    bool      isDirectory = false;
    bool      isDeleted = false;
    ULONGLONG size = 0;
    FILETIME  created = {};
    FILETIME  modified = {};
    DWORD     attributes = 0;     // FILE_ATTRIBUTE_* style flags
    ULONGLONG id = 0;             // FAT/exFAT: first cluster; NTFS: MFT record number
    ULONGLONG firstLBA = 0;       // absolute LBA of the first data sector (0 = none)
    ULONGLONG metaLBA = 0;        // absolute LBA of the directory entry / MFT record
    Recoverability recover = Recoverability::NotApplicable;
    CString   recoverNote;

    // Implementation details needed to read the data back.
    bool      contiguous = false;          // exFAT NoFatChain / FAT deleted (assumed)
    bool      resident = false;            // NTFS resident $DATA
    std::vector<BYTE>       residentData;  // NTFS resident $DATA bytes
    std::vector<ClusterRun> runs;          // NTFS non-resident $DATA runs
    ULONGLONG allocatedClusters = 0;       // clusters the data occupies
};

class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    // Detects the filesystem in the boot sector at volumeStartLBA and returns
    // a parser for it (nullptr + errorOut when nothing recognisable is there).
    static std::shared_ptr<IFileSystem> Detect(const CString& devicePath, ULONGLONG volumeStartLBA,
                                               ULONGLONG volumeSectors, CString& errorOut);

    // Quick check without opening a parser: "NTFS", "FAT32", ... or empty.
    static CString Identify(const std::vector<BYTE>& bootSector);

    virtual FsKind  Kind() const = 0;
    CString         KindName() const;

    // ---- Boot record -------------------------------------------------------
    ULONGLONG       BootRecordLBA() const { return m_volumeStartLBA; }
    virtual void    BootRecordFields(std::vector<FieldRow>& out) const = 0;

    // ---- File allocation system --------------------------------------------
    virtual CString   AllocationTitle() const = 0;   // "File Allocation Table", "Master File Table ($MFT)"
    virtual ULONGLONG AllocationLBA() const = 0;     // first sector of that structure
    virtual void      AllocationFields(std::vector<FieldRow>& out) = 0;

    // ---- File system ---------------------------------------------------------
    // NTFS has to scan the whole MFT once before directories can be listed
    // (deleted files are only found that way). progress(0..100) returns false
    // to cancel. Called on a worker thread.
    virtual bool    NeedsIndex() const { return false; }
    virtual bool    BuildIndex(const std::function<bool(int)>& /*progress*/, CString& /*errorOut*/) { return true; }

    virtual FsEntry Root() const = 0;
    virtual bool    ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut) = 0;

    // Deleted entries whose parent directory no longer exists (NTFS only).
    virtual bool    HasOrphans() const { return false; }
    virtual void    ListOrphans(std::vector<FsEntry>& /*out*/) {}

    // Writes the entry's data to outputPath (a new file). Returns an empty
    // string on success, otherwise the reason. bytesWritten may be short of
    // entry.size when the volume ran out of data.
    virtual CString Recover(const FsEntry& entry, const CString& outputPath, ULONGLONG& bytesWritten) = 0;

    // Detail rows for one entry (Properties pane).
    virtual void    EntryFields(const FsEntry& entry, std::vector<FieldRow>& out) const;

    ULONGLONG VolumeStartLBA() const { return m_volumeStartLBA; }
    ULONGLONG VolumeSectors() const  { return m_volumeSectors; }
    DWORD     SectorSize() const     { return m_dm.GetBytesPerSector(); }
    ULONGLONG ClusterBytes() const   { return m_clusterBytes; }
    const CString& DevicePath() const { return m_dm.GetDevicePath(); }

    // Display helpers shared by the UI.
    static CString RecoverabilityText(Recoverability r);
    static CString FormatFileTime(const FILETIME& ft);
    static CString FormatSize(ULONGLONG bytes);
    static CString Hex(ULONGLONG v, int digits = 0);

protected:
    // Opens the drive and parses the boot record. Overrides call this first.
    virtual bool Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors, CString& errorOut);

    // Volume-relative cluster I/O helpers (all bounds-checked).
    bool ReadVolumeSectors(ULONGLONG relSector, DWORD count, std::vector<BYTE>& out);
    bool ReadRuns(const std::vector<ClusterRun>& runs, ULONGLONG clusterBase, ULONGLONG clusterStartLBA,
                  ULONGLONG fileSize, HANDLE hOut, ULONGLONG& written, CString& err);

    static void AddField(std::vector<FieldRow>& out, LPCTSTR group, LPCTSTR name, const CString& value,
                         int offset = -1, LPCTSTR description = nullptr);

    CDiskManager m_dm;
    ULONGLONG    m_volumeStartLBA = 0;
    ULONGLONG    m_volumeSectors = 0;
    ULONGLONG    m_clusterBytes = 0;
};

inline WORD      ReadLE16(const BYTE* p) { WORD v; memcpy(&v, p, sizeof(v)); return v; }
inline DWORD     ReadLE32(const BYTE* p) { DWORD v; memcpy(&v, p, sizeof(v)); return v; }
inline ULONGLONG ReadLE64(const BYTE* p) { ULONGLONG v; memcpy(&v, p, sizeof(v)); return v; }
