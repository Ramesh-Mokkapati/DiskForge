// ExtendPartitionDialog.h - Modal dialog for extending a partition into
// the contiguous free space that immediately follows it on disk.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "PartitionEditor.h"


class CExtendPartitionDialog : public CDialogEx
{
public:
    CExtendPartitionDialog(const DriveInfo& drive, const PartitionEntry& entry,
                            CWnd* pParent = nullptr);

    enum { IDD = IDD_EXTENDPARTITION };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnBtnExtend();
    afx_msg void OnBtnCancel();

    DECLARE_MESSAGE_MAP()

private:
    const DriveInfo&      m_drive;
    const PartitionEntry& m_entry;
    ULONGLONG             m_maxExtendBytes;

    CStatic  m_staticInfo;
    CStatic  m_staticCur;
    CStatic  m_staticAvail;
    CEdit    m_editMb;
    CStatic  m_staticMaxHint;
    CButton  m_btnExtend;
    CButton  m_btnCancel;
    CStatic  m_staticStatus;
};
