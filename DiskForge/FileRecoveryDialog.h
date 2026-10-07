// FileRecoveryDialog.h - UI for browsing a FAT32 volume (including deleted
// entries) and recovering a selected file. Read-only against the source
// disk - it only ever writes to the NEW output file the user picks.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "Fat32Reader.h"


// One item's worth of context stashed via SetItemData, so we know what a
// tree node represents without re-deriving it from its text.
struct FrTreeItemData
{
    bool         isDirectory = false;
    bool         isDeleted = false;
    bool         childrenLoaded = false;
    DWORD        firstCluster = 0;
    ULONGLONG    fileSize = 0;
};

class CFileRecoveryDialog : public CDialogEx
{
public:
    explicit CFileRecoveryDialog(CWnd* pParent = nullptr);
    virtual ~CFileRecoveryDialog();

    enum { IDD = IDD_FILERECOVERY };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnComboDriveChange();
    afx_msg void OnBtnChoosePartition();
    afx_msg void OnBtnOpenVolume();
    afx_msg void OnTreeItemExpanding(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBtnRecover();
    afx_msg void OnBtnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox   m_comboDrive;
    CButton     m_btnChoosePart;
    CStatic     m_staticPart;
    CEdit       m_editCustomLba;
    CButton     m_btnOpen;
    CTreeCtrl   m_tree;
    CStatic     m_staticInfo;
    CButton     m_btnRecover;
    CStatic     m_staticStatus;
    CButton     m_btnClose;

    std::vector<DriveInfo> m_drives;
    PartitionEntry          m_chosenPartition;
    bool                    m_hasChosenPartition;

    CDiskManager    m_dm;       // kept open (read-only) while this dialog is up
    FatVolumeInfo   m_volume;
    bool            m_volumeOpen;

    static const int MIN_WIDTH  = 700;
    static const int MIN_HEIGHT = 520;

    void UpdateChosenPartitionLabel();
    void PopulateChildren(HTREEITEM parent, DWORD dirCluster);
    void CleanupTreeItemData(HTREEITEM item); // recursively deletes FrTreeItemData*
};
