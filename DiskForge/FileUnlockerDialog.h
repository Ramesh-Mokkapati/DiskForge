// FileUnlockerDialog.h - UI for finding and (optionally) terminating
// processes that have a chosen file locked open.
#pragma once
#include "resource.h"
#include "FileUnlocker.h"


class CFileUnlockerDialog : public CDialogEx
{
public:
    explicit CFileUnlockerDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_FILEUNLOCKER };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnEnChangeFilePath();
    afx_msg void OnBtnBrowse();
    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnKillSelected();
    afx_msg void OnBtnKillAll();
    afx_msg void OnBtnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CEdit       m_editFilePath;
    CButton     m_btnBrowse;
    CListCtrl   m_listLockers;
    CButton     m_btnRefresh;
    CButton     m_btnKillSelected;
    CButton     m_btnKillAll;
    CStatic     m_staticStatus;
    CButton     m_btnClose;

    std::vector<LockingProcess> m_currentLockers;
    std::wstring                 m_targetPath;

    static const int MIN_WIDTH  = 620;
    static const int MIN_HEIGHT = 420;

    void RefreshLockerList();
    void KillProcesses(const std::vector<LockingProcess>& targets);
};
