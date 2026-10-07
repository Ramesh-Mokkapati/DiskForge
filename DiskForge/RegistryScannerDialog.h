// RegistryScannerDialog.h - Registry scanner / fixer dialog
#pragma once
#include "resource.h"
#include "RegistryScanner.h"

#define WM_RS_PROGRESS  (WM_USER + 160)   // wParam=percent, lParam=heap-allocated CString*
#define WM_RS_COMPLETE  (WM_USER + 161)   // wParam=issueCount, lParam=0

class CRegistryScannerDialog : public CDialogEx
{
public:
    CRegistryScannerDialog(CWnd* pParent = nullptr);
    enum { IDD = IDD_REGISTRYSCANNER };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnBtnScan();
    afx_msg void OnBtnFixSelected();
    afx_msg void OnBtnFixAll();
    afx_msg void OnBtnBackupAndFix();
    afx_msg void OnBtnIgnore();
    afx_msg void OnBtnClose();
    afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg LRESULT OnScanProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnScanComplete(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    // Category checkboxes (alphabetical)
    CButton       m_chkAppPath;
    CButton       m_chkBrowserHelper;
    CButton       m_chkFileExt;
    CButton       m_chkFirewall;
    CButton       m_chkFonts;
    CButton       m_chkHelpFiles;
    CButton       m_chkInstallers;
    CButton       m_chkInterface;
    CButton       m_chkMuiCache;
    CButton       m_chkSharedDll;
    CButton       m_chkUninstall;     // "Obsolete Software"
    CButton       m_chkOpenWith;
    CButton       m_chkStartup;       // "Run At Startup"
    CButton       m_chkSoundEvents;
    CButton       m_chkServices;

    CButton       m_btnScan;
    CProgressCtrl m_progress;
    CListCtrl     m_list;
    CButton       m_btnFixSel;
    CButton       m_btnFixAll;
    CButton       m_btnBackupFix;
    CButton       m_btnIgnore;
    CButton       m_btnClose;
    CStatic       m_staticStatus;
    CStatic       m_staticProgress;

    CRegistryScanner m_scanner;
    bool             m_scanning;

    void PopulateList();
    void UpdateButtonState();
    void SetStatus(const CString& msg);
    void LayoutControls(int cx, int cy);
    void SetScanInProgress(bool inProgress);
    bool DoFixIssues(const std::vector<int>& indices);
    bool PromptBackup(std::vector<int>& outIndices);

    ScanOptions BuildScanOptions() const;

    struct ScanParams {
        CRegistryScannerDialog* pDlg;
        ScanOptions opts;
    };
    static UINT ScanThreadProc(LPVOID pParam);

    static const int MIN_W = 800;
    static const int MIN_H = 500;
};
