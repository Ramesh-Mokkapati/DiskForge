// NtfsFileSystem.h - NTFS parser for the System Browser.
//
// NTFS removes a deleted file from its folder's index, but its MFT record
// stays behind (marked "not in use") until NTFS hands the record to a new
// file. So instead of walking directory indexes, this parser scans the whole
// Master File Table once (BuildIndex, run on a worker thread) and rebuilds
// the folder tree from each record's $FILE_NAME parent reference. That shows
// live and deleted entries side by side.
//
// For a deleted file:
//   - small files whose data is resident inside the MFT record come back
//     exactly ("Excellent");
//   - otherwise the record still lists the file's data runs, and each run's
//     clusters are checked against the volume $Bitmap: clusters still free
//     have not been reused, so the data should be intact.
// Compressed and EFS-encrypted files are reported but not recovered.
#pragma once
#include "FileSystem.h"
#include <unordered_map>
#include <atomic>

class CNtfsFileSystem : public IFileSystem
{
public:
    FsKind    Kind() const override { return FsKind::NTFS; }

    void      BootRecordFields(std::vector<FieldRow>& out) const override;

    CString   AllocationTitle() const override { return _T("Master File Table ($MFT)"); }
    ULONGLONG AllocationLBA() const override { return m_volumeStartLBA + m_mftLcn * m_spc; }
    void      AllocationFields(std::vector<FieldRow>& out) override;

    bool      NeedsIndex() const override { return !m_indexed; }
    bool      BuildIndex(const std::function<bool(int)>& progress, CString& errorOut) override;

    FsEntry   Root() const override;
    bool      ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut) override;
    bool      HasOrphans() const override { return !m_orphans.empty(); }
    void      ListOrphans(std::vector<FsEntry>& out) override;
    CString   Recover(const FsEntry& entry, const CString& outputPath, ULONGLONG& bytesWritten) override;
    void      EntryFields(const FsEntry& entry, std::vector<FieldRow>& out) const override;

protected:
    bool Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors, CString& errorOut) override;

private:
    enum : BYTE { kInUse = 1, kDir = 2, kHasName = 4, kResident = 8, kCompressed = 16, kEncrypted = 32, kNoData = 64 };

    struct Rec
    {
        ULONGLONG parent = 0;
        WORD      parentSeq = 0;
        WORD      seq = 0;
        BYTE      flags = 0;
        DWORD     attributes = 0;
        ULONGLONG size = 0;
        FILETIME  created = {}, modified = {};
        std::wstring name;
    };

    struct DataInfo
    {
        bool      found = false;
        bool      resident = false;
        bool      compressed = false, encrypted = false;
        ULONGLONG size = 0;
        std::vector<BYTE>       residentData;
        std::vector<ClusterRun> runs;
    };

    std::vector<BYTE> m_boot;
    ULONGLONG m_spc = 1;              // sectors per cluster
    ULONGLONG m_mftLcn = 0, m_mftMirrLcn = 0;
    DWORD     m_recordSize = 1024;
    ULONGLONG m_totalClusters = 0;

    // $MFT layout as (absolute LBA, sector count) segments, in VCN order.
    std::vector<std::pair<ULONGLONG, ULONGLONG>> m_mftSegments;
    std::vector<ClusterRun> m_mftRuns;
    ULONGLONG m_mftBytes = 0;

    std::atomic<bool> m_indexed{ false };   // set by the worker thread when BuildIndex finishes
    std::vector<Rec> m_recs;
    std::unordered_map<ULONGLONG, std::vector<ULONGLONG>> m_children;
    std::vector<ULONGLONG> m_orphans;
    std::vector<BYTE> m_volBitmap;    // $Bitmap: bit set = cluster in use
    ULONGLONG m_inUse = 0, m_deleted = 0, m_dirs = 0;

    bool      ReadRecord(ULONGLONG recNo, std::vector<BYTE>& rec);
    bool      ReadMftBytes(ULONGLONG offset, size_t len, std::vector<BYTE>& out);
    bool      ApplyFixup(BYTE* rec, DWORD size) const;
    bool      ParseRuns(const BYTE* attr, DWORD attrLen, std::vector<ClusterRun>& runs) const;
    bool      ReadData(ULONGLONG recNo, DataInfo& info);
    void      ParseRecord(const BYTE* rec, Rec& r) const;
    void      Assess(ULONGLONG recNo, FsEntry& e);
    FsEntry   MakeEntry(ULONGLONG recNo) const;
    bool      IsClusterInUse(ULONGLONG lcn) const;
};
