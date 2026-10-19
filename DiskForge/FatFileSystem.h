// FatFileSystem.h - FAT12 / FAT16 / FAT32 parser for the System Browser.
//
// Lists live and deleted directory entries (deleted ones keep their long
// file name when the LFN entries survived; the lost first character of the
// short name is reconstructed from the LFN checksum when possible) and
// judges each deleted file's chance of recovery from the FAT itself:
//
//   FAT clears a deleted file's cluster chain, so only the first cluster is
//   known. Recovery therefore assumes the file was stored contiguously and
//   checks whether those clusters are still free. "Good" means none of them
//   has been reused; a fragmented file can still come back corrupted, which
//   FAT metadata alone cannot reveal.
#pragma once
#include "FileSystem.h"
#include <unordered_map>

class CFatFileSystem : public IFileSystem
{
public:
    FsKind    Kind() const override { return m_kind; }

    void      BootRecordFields(std::vector<FieldRow>& out) const override;

    CString   AllocationTitle() const override { return _T("File Allocation Table"); }
    ULONGLONG AllocationLBA() const override { return m_volumeStartLBA + m_fatStartRel; }
    void      AllocationFields(std::vector<FieldRow>& out) override;

    FsEntry   Root() const override;
    bool      ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut) override;
    CString   Recover(const FsEntry& entry, const CString& outputPath, ULONGLONG& bytesWritten) override;

protected:
    bool Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors, CString& errorOut) override;

private:
    FsKind    m_kind = FsKind::Unknown;
    std::vector<BYTE> m_boot;

    DWORD     m_bps = 512;
    DWORD     m_spc = 1;
    DWORD     m_reserved = 0;
    DWORD     m_numFats = 2;
    DWORD     m_rootEntries = 0;
    ULONGLONG m_totalSectors = 0;
    DWORD     m_fatSectors = 0;
    DWORD     m_rootCluster = 0;     // FAT32 only
    ULONGLONG m_fatStartRel = 0;     // volume-relative sector of FAT #1
    ULONGLONG m_rootDirRel = 0;      // FAT12/16 fixed root directory
    DWORD     m_rootDirSectors = 0;
    ULONGLONG m_dataStartRel = 0;    // volume-relative sector of cluster #2
    DWORD     m_clusterCount = 0;

    // FAT #1, loaded on demand in blocks of kFatBlockSectors sectors.
    static const DWORD kFatBlockSectors = 256;
    std::unordered_map<ULONGLONG, std::vector<BYTE>> m_fatBlocks;

    bool      m_statsDone = false;
    DWORD     m_freeClusters = 0, m_badClusters = 0;

    DWORD     MaxCluster() const { return m_clusterCount + 1; }
    ULONGLONG ClusterLBA(DWORD cluster) const
    { return m_volumeStartLBA + m_dataStartRel + (ULONGLONG)(cluster - 2) * m_spc; }

    bool      GetFatEntry(DWORD cluster, DWORD& value);
    bool      IsEndOfChain(DWORD value) const;
    std::vector<DWORD> Chain(DWORD start, DWORD maxLen = 4000000);
    bool      ReadDirectoryBytes(const FsEntry& dir, std::vector<BYTE>& data, std::vector<ULONGLONG>& sectorLBAs);
    void      AssessDeleted(FsEntry& e);
    void      ComputeStats();
};
