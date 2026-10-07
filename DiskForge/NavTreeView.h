// NavTreeView.h - Left-pane navigation tree
#pragma once
#include "resource.h"

class CDiskForgeDlg;

class CNavTreeView : public CTreeView
{
    DECLARE_DYNCREATE(CNavTreeView)
public:
    CNavTreeView() = default;
    virtual ~CNavTreeView() = default;
protected:
    virtual void OnInitialUpdate() override;
    afx_msg void OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
private:
    CDiskForgeDlg* GetMainView();
};
