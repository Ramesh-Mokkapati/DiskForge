// ConfirmTextDialog.h - Generic "type this exact phrase to proceed" dialog.
// Used as the final safety gate before any destructive/write operation.
#pragma once
#include "resource.h"


class CConfirmTextDialog : public CDialogEx
{
public:
    // message: the warning shown to the user, can be multi-line (\r\n).
    // requiredPhrase: the exact text the user must type (case-sensitive) for OK to be enabled.
    CConfirmTextDialog(const CString& message, const CString& requiredPhrase, CWnd* pParent = nullptr);

    enum { IDD = IDD_CONFIRM_TEXT };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    afx_msg void OnEditChange();
    afx_msg void OnBtnOk();
    afx_msg void OnBtnCancel();
    DECLARE_MESSAGE_MAP()

private:
    CString  m_message;
    CString  m_requiredPhrase;
    CStatic  m_staticMsg;
    CStatic  m_staticHint;
    CEdit    m_edit;
    CButton  m_btnOk;
    CButton  m_btnCancel;
};
