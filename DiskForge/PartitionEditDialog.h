// PartitionEditDialog.h - UI for partition metadata edits (active flag, type,
// hidden state, GPT name/attributes, drive-letter assignment), plus delete
// and extend operations and a context menu on the partition list.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "PartitionEditor.h"
#include "ConfirmTextDialog.h"
#include "ExtendPartitionDialog.h"


class CPartitionEditDialog : public CDialogEx
{
public:
    explicit CPartitionEditDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_PARTITIONEDIT };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnComboDriveChange();
    afx_msg void OnBtnLoad();
    afx_msg void OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnComboMbrTypeChange();
    afx_msg void OnBtnApply();
    afx_msg void OnBtnAssignLetter();
    afx_msg void OnBtnRemoveLetter();
    afx_msg void OnBtnDelete();
    afx_msg void OnBtnExtend();
    afx_msg void OnBtnClose();
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox   m_comboDrive;
    CButton     m_btnLoad;
    CListCtrl   m_listPartitions;

    CButton     m_chkMbrActive;
    CComboBox   m_comboMbrType;
    CButton     m_chkMbrHidden;

    CEdit       m_editGptName;
    CComboBox   m_comboGptType;
    CButton     m_chkGptLegacyBoot;
    CButton     m_chkGptNoLetter;

    CButton     m_btnApply;
    CStatic     m_staticStatus;

    CStatic     m_staticCurLetter;
    CComboBox   m_comboNewLetter;
    CButton     m_btnAssignLetter;
    CButton     m_btnRemoveLetter;

    CButton     m_btnDelete;
    CButton     m_btnExtend;
    CButton     m_btnClose;

    std::vector<DriveInfo>      m_drives;
    std::vector<PartitionEntry> m_partitions;
    int                         m_selectedIndex; // index into m_partitions, -1 if none

    static const int MIN_WIDTH  = 760;
    static const int MIN_HEIGHT = 560;

    void PopulateList();
    void ShowFieldsForSelection();
    void UpdateModeVisibility();
    void RefreshDriveLetterPanel();
    void DeleteSelectedPartition();
    void ExtendSelectedPartition();
};
