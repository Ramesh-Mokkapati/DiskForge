// EfiBootDialog.h - UI for the UEFI Boot Entry Manager.
#pragma once
#include "resource.h"
#include "EfiBootManager.h"


class CEfiBootDialog : public CDialogEx
{
public:
    explicit CEfiBootDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_EFIBOOT };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBtnMoveUp();
    afx_msg void OnBtnMoveDown();
    afx_msg void OnBtnApplyOrder();
    afx_msg void OnBtnSetNext();
    afx_msg void OnBtnClearNext();
    afx_msg void OnBtnDelete();
    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CListCtrl   m_listEntries;
    CButton     m_btnMoveUp;
    CButton     m_btnMoveDown;
    CButton     m_btnApplyOrder;
    CButton     m_btnSetNext;
    CButton     m_btnClearNext;
    CButton     m_btnDelete;
    CButton     m_btnRefresh;
    CStatic     m_staticStatus;
    CButton     m_btnClose;

    std::vector<EfiBootEntry> m_entries; // current display order (may differ from firmware until Applied)
    bool                       m_orderDirty;
    bool                       m_hasBootNext;
    WORD                       m_bootNext;

    static const int MIN_WIDTH  = 640;
    static const int MIN_HEIGHT = 440;

    void LoadFromFirmware();
    void PopulateList();
    void UpdateButtonStates();
};
