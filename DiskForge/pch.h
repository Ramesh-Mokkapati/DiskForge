// pch.h - Pre-compiled header for DiskForge
#pragma once

#ifndef VC_EXTRA_LEAN
#define VC_EXTRA_LEAN
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxcview.h>       // CListView, CTreeView
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>       // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>         // MFC support for Windows Common Controls
#endif
#include <afxcontrolbars.h> // MFC support for ribbons and control bars
#include <afxdlgs.h>        // MFC common dialogs (CFileDialog, etc.) - used by Disk Copy

#include <winioctl.h>       // DeviceIoControl definitions
#include <tchar.h>          // _istdigit/_ttoi - used by MbrGptConverter
#include <vector>
#include <string>
#include <memory>
#include <algorithm>        // std::min/std::max - used by DiskCopyEngine
#include <cstring>          // memcpy/memcmp - used by PartitionParser
#include <utility>          // std::pair - used by DiskCopyDialog's thread param
