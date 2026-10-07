// SmartHealthDialog.h - UI for the read-only Disk Health / S.M.A.R.T. feature.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "SmartDataReader.h"


class CSmartHealthDialog : public CDialogEx
{
public:
    explicit CSmartHealthDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_SMARTHEALTH };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnCheck();
    afx_msg void OnBtnClose();
    afx_msg void OnComboDriveChange();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox   m_comboDrive;
    CButton     m_btnRefresh;
    CButton     m_btnCheck;
    CStatic     m_staticOverall;
    CListCtrl   m_listAttributes;
    CStatic     m_staticCaveat;
    CButton     m_btnClose;

    std::vector<DriveInfo> m_drives;

    static const int MIN_WIDTH  = 640;
    static const int MIN_HEIGHT = 460;

    void RefreshDriveList();
    void RunHealthCheck();
};
