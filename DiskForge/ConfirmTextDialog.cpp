// ConfirmTextDialog.cpp
#include "pch.h"
#include "ConfirmTextDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CConfirmTextDialog, CDialogEx)
    ON_EN_CHANGE(IDC_CONFIRM_EDIT, &CConfirmTextDialog::OnEditChange)
    ON_BN_CLICKED(IDC_CONFIRM_BTN_OK,     &CConfirmTextDialog::OnBtnOk)
    ON_BN_CLICKED(IDC_CONFIRM_BTN_CANCEL, &CConfirmTextDialog::OnBtnCancel)
END_MESSAGE_MAP()

CConfirmTextDialog::CConfirmTextDialog(const CString& message, const CString& requiredPhrase, CWnd* pParent)
    : CDialogEx(IDD_CONFIRM_TEXT, pParent)
    , m_message(message)
    , m_requiredPhrase(requiredPhrase)
{
}

void CConfirmTextDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CONFIRM_STATIC_MSG,  m_staticMsg);
    DDX_Control(pDX, IDC_CONFIRM_STATIC_HINT, m_staticHint);
    DDX_Control(pDX, IDC_CONFIRM_EDIT,        m_edit);
    DDX_Control(pDX, IDC_CONFIRM_BTN_OK,      m_btnOk);
    DDX_Control(pDX, IDC_CONFIRM_BTN_CANCEL,  m_btnCancel);
}

BOOL CConfirmTextDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Confirm - This Will Overwrite Data"));

    CString hint;
    hint.Format(_T("Type the following exactly to enable OK:  %s"), m_requiredPhrase.GetString());
    m_staticMsg.SetWindowText(m_message);
    m_staticHint.SetWindowText(hint);
    m_btnOk.EnableWindow(FALSE);

    m_edit.SetFocus();
    return FALSE;
}

void CConfirmTextDialog::OnEditChange()
{
    CString text;
    m_edit.GetWindowText(text);
    // Case-sensitive, exact match required - this is a deliberate speed bump,
    // not a puzzle, but it must not be satisfiable by accident.
    m_btnOk.EnableWindow(text == m_requiredPhrase);
}

void CConfirmTextDialog::OnBtnOk()
{
    CString text;
    m_edit.GetWindowText(text);
    if (text == m_requiredPhrase)
        EndDialog(IDOK);
}

void CConfirmTextDialog::OnBtnCancel()
{
    EndDialog(IDCANCEL);
}
