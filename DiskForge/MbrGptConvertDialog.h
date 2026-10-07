// MbrGptConvertDialog.h - UI for MBR -> GPT conversion (and restoring a
// backup taken before a conversion). The UI deliberately enforces order:
// Analyze -> Save Backup -> (only then) Convert becomes available.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "MbrGptConverter.h"


class CMbrGptConvertDialog : public CDialogEx
{
public:
    explicit CMbrGptConvertDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_MBRGPTCONVERT };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnComboDriveChange();
    afx_msg void OnBtnAnalyze();
    afx_msg void OnBtnSaveBackup();
    afx_msg void OnBtnConvert();
    afx_msg void OnBtnRestore();
    afx_msg void OnBtnClose();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox   m_comboDrive;
    CButton     m_btnAnalyze;
    CListCtrl   m_listPartitions;
    CStatic     m_staticResult;
    CButton     m_btnSaveBackup;
    CStatic     m_staticBackup;
    CButton     m_btnConvert;
    CButton     m_btnRestore;
    CButton     m_btnClose;

    std::vector<DriveInfo>   m_drives;
    ConvertPreflightResult   m_lastAnalysis;
    bool                     m_backupSaved;
    CString                  m_backupFilePath;

    static const int MIN_WIDTH  = 700;
    static const int MIN_HEIGHT = 480;

    void ResetAnalysisState();
    void PopulatePartitionList();
};
