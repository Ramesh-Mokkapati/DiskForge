// DiskCopyDialog.h - UI for the Disk Copy feature (whole disk or single
// partition -> another physical disk or an image file).
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "DiskCopyEngine.h"


class CDiskCopyDialog : public CDialogEx
{
public:
    explicit CDiskCopyDialog(CWnd* pParent = nullptr);
    virtual ~CDiskCopyDialog();

    enum { IDD = IDD_DISKCOPY };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnCancel();
    virtual void OnOK() {} // Enter must not silently trigger a copy

    afx_msg void OnComboSrcChange();
    afx_msg void OnComboDstChange();
    afx_msg void OnRadioSrcMode();
    afx_msg void OnRadioSrcTypeMode();
    afx_msg void OnRadioDstMode();
    afx_msg void OnRadioDstRangeMode();
    afx_msg void OnBtnChoosePartition();
    afx_msg void OnBtnChooseDstPartition();
    afx_msg void OnBtnBrowseDestFile();
    afx_msg void OnBtnBrowseSrcFile();
    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnStart();
    afx_msg void OnBtnCancelCopy();
    afx_msg void OnBtnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    // Controls
    CButton       m_radioSrcTypeDrive;
    CButton       m_radioSrcTypeFile;
    CComboBox     m_comboSrc;
    CButton       m_radioSrcWhole;
    CButton       m_radioSrcPart;
    CButton       m_btnChoosePart;
    CStatic       m_staticPart;
    CEdit         m_editSrcFile;
    CButton       m_btnBrowseSrc;

    CButton       m_radioDstDisk;
    CButton       m_radioDstFile;
    CComboBox     m_comboDst;
    CEdit         m_editDstFile;
    CButton       m_btnBrowse;
    CButton       m_radioDstWhole;
    CButton       m_radioDstPart;
    CButton       m_btnChooseDstPart;
    CStatic       m_staticDstPart;

    CStatic       m_staticSrcHdr;
    CStatic       m_staticDstHdr;
    CButton       m_btnRefresh;
    CStatic       m_staticSummary;
    CProgressCtrl m_progress;
    CStatic       m_staticStatus;
    CButton       m_btnStart;
    CButton       m_btnCancelCopy;
    CButton       m_btnClose;

    // Data
    std::vector<DriveInfo> m_drives;          // Shared list for both source & destination combos
    PartitionEntry         m_chosenPartition;
    bool                   m_hasChosenPartition;
    PartitionEntry         m_chosenDstPartition;
    bool                   m_hasChosenDstPartition;

    CDiskCopyEngine        m_engine;
    bool                   m_copyRunning;
    CWinThread*            m_pThread;

    static const int MIN_WIDTH  = 640;  // pixels
    static const int MIN_HEIGHT = 520;  // pixels
    static const UINT_PTR TIMER_ID = 1;

    void RefreshDriveLists();
    void UpdateChosenPartitionLabel();
    void UpdateChosenDstPartitionLabel();
    void UpdateModeEnablement();
    void UpdateSummary();
    void SetUiEnabled(bool enabled);
    void SetStatus(const CString& msg);

    bool BuildJob(DiskCopyJob& jobOut, CString& errorOut);

    static UINT __cdecl CopyThreadProc(LPVOID pParam);
};
