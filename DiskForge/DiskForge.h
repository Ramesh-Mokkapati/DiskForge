// DiskForge.h - Main application class
#pragma once

class CDiskForgeApp : public CWinApp
{
public:
    CDiskForgeApp();

    virtual BOOL InitInstance();
    virtual int ExitInstance();

    DECLARE_MESSAGE_MAP()
};

extern CDiskForgeApp theApp;
