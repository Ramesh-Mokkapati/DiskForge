// DiskForge.h - Main application class (CWinAppEx, as in the NewTest
// framework: visual-manager themes, context-menu / keyboard / tooltip
// managers and persisted docking layout).
#pragma once

#include "resource.h"

class CDiskForgeApp : public CWinAppEx
{
public:
    CDiskForgeApp() noexcept;

    virtual BOOL InitInstance() override;
    virtual int  ExitInstance() override;

    UINT  m_nAppLook = 0;
    BOOL  m_bHiColorIcons = TRUE;

    virtual void PreLoadState() override;
    virtual void LoadCustomState() override {}
    virtual void SaveCustomState() override {}

    afx_msg void OnAppAbout();
    DECLARE_MESSAGE_MAP()
};

extern CDiskForgeApp theApp;
