// ExFatFileSystem.h - exFAT parser for the System Browser.
//
// exFAT keeps two allocation structures: the Allocation Bitmap (one bit per
// cluster - the authoritative "in use" map) and the FAT, which is only
// consulted for fragmented files. Most files are written contiguously and
// flagged NoFatChain, so for a deleted exFAT file the full extent (first
// cluster + length) usually survives in its stream-extension entry, making
// recovery reliable as long as the bitmap still shows those clusters free.
#pragma once
#include "FileSystem.h"

class CExFatFileSystem : public IFileSystem
{
public:
    FsKind    Kind() const override { return FsKind::ExFAT; }

    void      BootRecordFields(std::vector<FieldRow>& out) const override;

    CString   AllocationTitle() const override { return _T("Allocation Bitmap & FAT"); }
    ULONGLONG AllocationLBA() const override;
    void      AllocationFields(std::vector<FieldRow>& out) override;

    FsEntry   Root() const override;
    bool      ListDirectory(const FsEntry& dir, std::vector<FsEntry>& out, CString& errorOut) override;
    CString   Recover(const FsEntry& entry, const CString& outputPath, ULONGLONG& bytesWritten) override;

protected:
    bool Init(const CString& devicePath, ULONGLONG volumeStartLBA, ULONGLONG volumeSectors, CString& errorOut) override;

private:
    std::vector<BYTE> m_boot;
    DWORD     m_fatOffset = 0, m_fatLength = 0;
    DWORD     m_heapOffset = 0, m_clusterCount = 0, m_rootCluster = 0;
    DWORD     m_spc = 1;
    std::vector<BYTE> m_bitmap;     // allocation bitmap (bit set = cluster in use)
    DWORD     m_bitmapCluster = 0;
    ULONGLONG m_bitmapLength = 0;
    bool      m_bitmapLoaded = false;

    ULONGLONG ClusterLBA(DWORD c) const { return m_volumeStartLBA + m_heapOffset + (ULONGLONG)(c - 2) * m_spc; }
    bool      LoadBitmap();
    bool      IsClusterInUse(DWORD c);  // true when unknown, to stay pessimistic
    bool      FatNext(DWORD c, DWORD& next);
    bool      ExtentRuns(DWORD first, ULONGLONG length, bool noFatChain, std::vector<ClusterRun>& runs);
    bool      ReadExtent(const std::vector<ClusterRun>& runs, ULONGLONG maxBytes, std::vector<BYTE>& data,
                         std::vector<ULONGLONG>& lbas);
};
