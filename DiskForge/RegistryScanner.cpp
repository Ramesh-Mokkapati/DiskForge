// RegistryScanner.cpp
#include "pch.h"
#include "RegistryScanner.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ---- Static helpers ----

CString CRegistryScanner::ExtractExePath(const CString& cmd)
{
    CString s = cmd;
    s.Trim();
    if (s.IsEmpty()) return _T("");

    CString path;
    if (s[0] == _T('"'))
    {
        int close = s.Find(_T('"'), 1);
        path = (close > 0) ? s.Mid(1, close - 1) : s.Mid(1);
    }
    else
    {
        int sp = s.Find(_T(' '));
        path = (sp > 0) ? s.Left(sp) : s;
    }

    TCHAR expanded[MAX_PATH * 2] = {};
    if (ExpandEnvironmentStrings(path, expanded, _countof(expanded)) > 0)
        path = expanded;

    return path;
}

bool CRegistryScanner::PathExists(const CString& path)
{
    if (path.IsEmpty()) return false;
    DWORD attrs = GetFileAttributes(path);
    return (attrs != INVALID_FILE_ATTRIBUTES);
}

bool CRegistryScanner::IsAlwaysValidExe(const CString& path)
{
    static const TCHAR* kAlways[] = {
        _T("msiexec.exe"), _T("rundll32.exe"), _T("cmd.exe"), _T("regsvr32.exe"),
        _T("wscript.exe"),  _T("cscript.exe"),  _T("schtasks.exe"),
        nullptr
    };
    CString lower = path;
    lower.MakeLower();
    int bs = lower.ReverseFind(_T('\\'));
    if (bs >= 0) lower = lower.Mid(bs + 1);
    for (int i = 0; kAlways[i]; ++i)
        if (lower == kAlways[i]) return true;
    return false;
}

bool CRegistryScanner::IsInSystemDir(const CString& fullPath)
{
    static TCHAR sysDir[MAX_PATH]  = {};
    static TCHAR sys32[MAX_PATH]   = {};
    if (!sysDir[0])
    {
        GetSystemDirectory(sysDir, MAX_PATH);
        GetSystemWow64Directory(sys32, MAX_PATH);
    }
    CString lower = fullPath; lower.MakeLower();
    CString s1    = sysDir;   s1.MakeLower();
    CString s2    = sys32;    s2.MakeLower();
    return (!s1.IsEmpty() && lower.Find(s1) == 0)
        || (!s2.IsEmpty() && lower.Find(s2) == 0);
}

CString CRegistryScanner::ExpandServiceImagePath(const CString& raw)
{
    CString path = raw; path.Trim();
    if (path.IsEmpty()) return _T("");

    // \SystemRoot\ prefix
    if (path.Left(12).CompareNoCase(_T("\\SystemRoot\\")) == 0)
    {
        TCHAR winDir[MAX_PATH] = {};
        GetWindowsDirectory(winDir, MAX_PATH);
        path = CString(winDir) + _T("\\") + path.Mid(12);
    }
    // \??\ device path prefix
    else if (path.Left(4) == _T("\\??\\"))
    {
        path = path.Mid(4);
    }

    TCHAR expanded[MAX_PATH * 2] = {};
    ExpandEnvironmentStrings(path, expanded, _countof(expanded));
    path = expanded;

    return ExtractExePath(path);
}

CString CRegistryScanner::EscapeRegStr(const CString& s)
{
    CString r;
    r.Preallocate(s.GetLength() + 16);
    for (int i = 0; i < s.GetLength(); ++i)
    {
        TCHAR c = s[i];
        if      (c == _T('\\')) r += _T("\\\\");
        else if (c == _T('"'))  r += _T("\\\"");
        else                    r += c;
    }
    return r;
}

CString CRegistryScanner::ValueToRegLine(const CString& name, DWORD type,
                                          const BYTE* data, DWORD dataLen)
{
    CString lhs = name.IsEmpty()
        ? CString(_T("@"))
        : (_T("\"") + EscapeRegStr(name) + _T("\""));

    CString line;
    switch (type)
    {
    case REG_SZ:
    {
        CString val(reinterpret_cast<LPCTSTR>(data));
        line.Format(_T("%s=\"%s\"\r\n"), lhs.GetString(), EscapeRegStr(val).GetString());
        break;
    }
    case REG_EXPAND_SZ:
        line = lhs + _T("=hex(2):");
        for (DWORD i = 0; i < dataLen; ++i)
        {
            CString b; b.Format(_T("%02x"), data[i]);
            if (i > 0) line += _T(",");
            line += b;
        }
        line += _T("\r\n");
        break;
    case REG_DWORD:
    {
        DWORD val = *reinterpret_cast<const DWORD*>(data);
        line.Format(_T("%s=dword:%08x\r\n"), lhs.GetString(), val);
        break;
    }
    default:
        line.Format(_T("; (skipped \"%s\", type=%u)\r\n"), name.GetString(), type);
        break;
    }
    return line;
}

// ---- Scan entry point ----

void CRegistryScanner::Scan(const ScanOptions& o, ProgressFn progressFn)
{
    m_issues.clear();

    // Count active categories for progress reporting
    int total = (o.appPaths ? 1:0) + (o.browserHelper ? 1:0) + (o.fileExt ? 1:0)
              + (o.firewall ? 1:0) + (o.fonts ? 1:0)  + (o.helpFiles ? 1:0)
              + (o.installers ? 1:0) + (o.interfaceCom ? 1:0) + (o.muiCache ? 1:0)
              + (o.sharedDlls ? 1:0) + (o.uninstall ? 1:0) + (o.openWith ? 1:0)
              + (o.startup ? 1:0) + (o.soundEvents ? 1:0) + (o.services ? 1:0);
    int done = 0;

    auto report = [&](const CString& phase) {
        if (progressFn)
            progressFn(total > 0 ? done * 100 / total : 100, phase);
    };

    if (o.appPaths)      { report(_T("App paths..."));            ScanAppPaths(progressFn);              ++done; }
    if (o.browserHelper) { report(_T("Browser helper objects...")); ScanBrowserHelperObjects(progressFn); ++done; }
    if (o.fileExt)       { report(_T("File extensions..."));      ScanFileExtensions(progressFn);        ++done; }
    if (o.firewall)      { report(_T("Firewall rules..."));       ScanFirewallRules(progressFn);         ++done; }
    if (o.fonts)         { report(_T("Fonts..."));                ScanFonts(progressFn);                 ++done; }
    if (o.helpFiles)     { report(_T("Help files..."));           ScanHelpFiles(progressFn);             ++done; }
    if (o.installers)    { report(_T("Installers..."));           ScanInstallers(progressFn);            ++done; }
    if (o.interfaceCom)  { report(_T("COM servers..."));          ScanInterfaceCom(progressFn);          ++done; }
    if (o.muiCache)      { report(_T("MUI cache..."));            ScanMuiCache(progressFn);              ++done; }
    if (o.sharedDlls)    { report(_T("Shared DLLs..."));          ScanSharedDlls(progressFn);            ++done; }
    if (o.uninstall)     { report(_T("Obsolete software..."));    ScanUninstallEntries(progressFn);      ++done; }
    if (o.openWith)      { report(_T("Open With apps..."));       ScanOpenWithApps(progressFn);          ++done; }
    if (o.startup)       { report(_T("Startup entries..."));      ScanStartupEntries(progressFn);        ++done; }
    if (o.soundEvents)   { report(_T("Sound events..."));         ScanSoundEvents(progressFn);           ++done; }
    if (o.services)      { report(_T("Windows services..."));     ScanWindowsServices(progressFn);       ++done; }

    report(_T("Scan complete."));
}

// ---- Category scanners ----

// --- App Paths ---

void CRegistryScanner::ScanAppPaths(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths");

    HKEY hBase = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hBase) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR subName[256] = {};
        DWORD subLen = _countof(subName);
        if (RegEnumKeyEx(hBase, idx, subName, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        HKEY hSub = nullptr;
        if (RegOpenKeyEx(hBase, subName, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
            continue;

        TCHAR  exePath[MAX_PATH * 2] = {};
        DWORD  epLen = sizeof(exePath), epType = 0;
        LONG rv = RegQueryValueEx(hSub, _T(""), nullptr, &epType,
                                  reinterpret_cast<BYTE*>(exePath), &epLen);
        RegCloseKey(hSub);

        if (rv != ERROR_SUCCESS) continue;
        if (epType != REG_SZ && epType != REG_EXPAND_SZ) continue;

        CString path(exePath);
        TCHAR expanded[MAX_PATH * 2] = {};
        if (ExpandEnvironmentStrings(path, expanded, _countof(expanded)) > 0)
            path = expanded;

        if (IsAlwaysValidExe(path)) continue;
        if (PathExists(path))       continue;

        CString fullKey;
        fullKey.Format(_T("%s\\%s"), kBase, subName);

        RegistryIssue e;
        e.typeLabel    = _T("App Path");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = fullKey;
        e.valueName    = _T("");
        e.valueData    = exePath;
        e.description.Format(_T("\"%s\" executable not found"), subName);
        e.deleteSubkey = true;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hBase);
}

// --- Browser Helper Objects ---

void CRegistryScanner::ScanBrowserHelperObjects(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects");

    HKEY hBase = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hBase) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR clsid[128] = {};
        DWORD len = _countof(clsid);
        if (RegEnumKeyEx(hBase, idx, clsid, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        CString inprocKey;
        inprocKey.Format(_T("CLSID\\%s\\InprocServer32"), clsid);

        HKEY hSrv = nullptr;
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT, inprocKey, 0, KEY_READ, &hSrv) != ERROR_SUCCESS)
            continue;

        TCHAR dllPath[MAX_PATH * 2] = {};
        DWORD dllLen = sizeof(dllPath), dllType = 0;
        LONG rv = RegQueryValueEx(hSrv, _T(""), nullptr, &dllType,
                                  reinterpret_cast<BYTE*>(dllPath), &dllLen);
        RegCloseKey(hSrv);

        if (rv != ERROR_SUCCESS || dllPath[0] == 0) continue;

        CString path(dllPath);
        TCHAR expanded[MAX_PATH * 2] = {};
        if (ExpandEnvironmentStrings(path, expanded, _countof(expanded)) > 0)
            path = expanded;

        if (IsInSystemDir(path)) continue;
        if (PathExists(path))    continue;

        CString fullKey;
        fullKey.Format(_T("%s\\%s"), kBase, clsid);

        RegistryIssue e;
        e.typeLabel    = _T("Browser Helper");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = fullKey;
        e.valueName    = _T("");
        e.valueData    = dllPath;
        e.description.Format(_T("BHO DLL not found: %s"), CString(dllPath).GetString());
        e.deleteSubkey = true;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hBase);
}

// --- File Extensions ---

void CRegistryScanner::ScanFileExtensions(ProgressFn& /*fn*/)
{
    HKEY hRoot = nullptr;
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, _T(""), 0, KEY_READ, &hRoot) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR extName[64] = {};
        DWORD len = _countof(extName);
        if (RegEnumKeyEx(hRoot, idx, extName, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        if (extName[0] != _T('.')) continue;

        // Read the default value (ProgID)
        HKEY hExt = nullptr;
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT, extName, 0, KEY_READ, &hExt) != ERROR_SUCCESS)
            continue;

        TCHAR progId[512] = {};
        DWORD progLen = sizeof(progId), progType = 0;
        LONG rv = RegQueryValueEx(hExt, _T(""), nullptr, &progType,
                                  reinterpret_cast<BYTE*>(progId), &progLen);
        RegCloseKey(hExt);

        if (rv != ERROR_SUCCESS || progId[0] == 0) continue;
        if (progType != REG_SZ) continue;

        // Check whether the ProgID key exists in HKCR
        HKEY hProg = nullptr;
        LONG rv2 = RegOpenKeyEx(HKEY_CLASSES_ROOT, progId, 0, KEY_READ, &hProg);
        if (rv2 == ERROR_SUCCESS) { RegCloseKey(hProg); continue; }

        RegistryIssue e;
        e.typeLabel    = _T("File Extension");
        e.hiveLabel    = _T("HKCR");
        e.hive         = HKEY_CLASSES_ROOT;
        e.keyPath      = extName;
        e.valueName    = _T("");
        e.valueData    = progId;
        e.description.Format(_T(".%s handler \"%s\" not registered"),
                             CString(extName).Mid(1).GetString(), progId);
        e.deleteSubkey = false;  // Delete just the default value
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hRoot);
}

// --- Firewall Rules ---

void CRegistryScanner::ScanFirewallRules(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules");

    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR  ruleName[512] = {};
        TCHAR  ruleData[4096] = {};
        DWORD  nameLen = _countof(ruleName), dataLen = sizeof(ruleData), type = 0;

        LONG r = RegEnumValue(hKey, idx, ruleName, &nameLen, nullptr, &type,
                              reinterpret_cast<BYTE*>(ruleData), &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || type != REG_SZ) continue;

        // Parse App= field from pipe-delimited rule string
        CString rule(ruleData);
        CString search(_T("|App="));
        int pos = rule.Find(search);
        if (pos < 0) continue;

        int start = pos + search.GetLength();
        int end   = rule.Find(_T('|'), start);
        CString appPath = (end >= 0) ? rule.Mid(start, end - start) : rule.Mid(start);
        appPath.Trim();

        // Skip system path entries and the System keyword
        if (appPath.IsEmpty() || appPath.CompareNoCase(_T("System")) == 0) continue;

        TCHAR expanded[MAX_PATH * 2] = {};
        ExpandEnvironmentStrings(appPath, expanded, _countof(expanded));
        CString fullPath(expanded);

        if (IsInSystemDir(fullPath)) continue;
        if (IsAlwaysValidExe(fullPath)) continue;
        if (PathExists(fullPath)) continue;

        RegistryIssue e;
        e.typeLabel    = _T("Firewall Rule");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = kBase;
        e.valueName    = ruleName;
        e.valueData    = appPath;
        e.description.Format(_T("Rule app not found: %s"), fullPath.GetString());
        e.deleteSubkey = false;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hKey);
}

// --- Fonts ---

void CRegistryScanner::ScanFonts(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts");

    TCHAR fontsDir[MAX_PATH] = {};
    GetWindowsDirectory(fontsDir, MAX_PATH);
    PathAppend(fontsDir, _T("Fonts"));

    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR fontName[512] = {};
        TCHAR fontFile[MAX_PATH * 2] = {};
        DWORD nameLen = _countof(fontName), dataLen = sizeof(fontFile), type = 0;

        LONG r = RegEnumValue(hKey, idx, fontName, &nameLen, nullptr, &type,
                              reinterpret_cast<BYTE*>(fontFile), &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
        if (fontFile[0] == 0) continue;

        CString path(fontFile);
        // If no directory separator, it's a filename relative to the Fonts folder
        if (path.Find(_T('\\')) < 0 && path.Find(_T('/')) < 0)
        {
            CString full(fontsDir);
            full += _T("\\");
            full += path;
            path = full;
        }
        else
        {
            TCHAR expanded[MAX_PATH * 2] = {};
            ExpandEnvironmentStrings(path, expanded, _countof(expanded));
            path = expanded;
        }

        if (PathExists(path)) continue;

        RegistryIssue e;
        e.typeLabel    = _T("Font");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = kBase;
        e.valueName    = fontName;
        e.valueData    = fontFile;
        e.description.Format(_T("Font file not found: %s"), CString(fontFile).GetString());
        e.deleteSubkey = false;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hKey);
}

// --- Help Files ---

void CRegistryScanner::ScanHelpFiles(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] = _T("SOFTWARE\\Microsoft\\Windows\\Help");

    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR  appName[512] = {};
        TCHAR  helpPath[MAX_PATH * 2] = {};
        DWORD  nameLen = _countof(appName), dataLen = sizeof(helpPath), type = 0;

        LONG r = RegEnumValue(hKey, idx, appName, &nameLen, nullptr, &type,
                              reinterpret_cast<BYTE*>(helpPath), &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
        if (helpPath[0] == 0) continue;

        TCHAR expanded[MAX_PATH * 2] = {};
        ExpandEnvironmentStrings(helpPath, expanded, _countof(expanded));
        CString path(expanded);

        if (PathExists(path)) continue;

        RegistryIssue e;
        e.typeLabel    = _T("Help File");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = kBase;
        e.valueName    = appName;
        e.valueData    = helpPath;
        e.description.Format(_T("Help path not found: %s"), path.GetString());
        e.deleteSubkey = false;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hKey);
}

// --- Installers (Uninstall entries with missing InstallLocation) ---

void CRegistryScanner::ScanInstallers(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    HKEY hives[]   = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    LPCTSTR labels[] = { _T("HKLM"), _T("HKCU") };

    for (int h = 0; h < 2; ++h)
    {
        HKEY hBase = nullptr;
        if (RegOpenKeyEx(hives[h], kBase, 0, KEY_READ, &hBase) != ERROR_SUCCESS)
            continue;

        for (DWORD idx = 0; ; ++idx)
        {
            TCHAR subName[256] = {};
            DWORD subLen = _countof(subName);
            if (RegEnumKeyEx(hBase, idx, subName, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            HKEY hSub = nullptr;
            if (RegOpenKeyEx(hBase, subName, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
                continue;

            TCHAR  locPath[2048] = {};
            DWORD  locLen = sizeof(locPath), locType = 0;
            LONG rv = RegQueryValueEx(hSub, _T("InstallLocation"), nullptr, &locType,
                                      reinterpret_cast<BYTE*>(locPath), &locLen);

            TCHAR dispName[512] = {};
            DWORD dispLen = sizeof(dispName);
            RegQueryValueEx(hSub, _T("DisplayName"), nullptr, nullptr,
                            reinterpret_cast<BYTE*>(dispName), &dispLen);
            RegCloseKey(hSub);

            if (rv != ERROR_SUCCESS) continue;
            if (locType != REG_SZ && locType != REG_EXPAND_SZ) continue;
            if (locPath[0] == 0) continue;

            TCHAR expanded[MAX_PATH * 2] = {};
            ExpandEnvironmentStrings(locPath, expanded, _countof(expanded));
            CString path(expanded);

            if (IsInSystemDir(path)) continue;
            if (PathExists(path)) continue;

            CString fullKey;
            fullKey.Format(_T("%s\\%s"), kBase, subName);

            RegistryIssue e;
            e.typeLabel    = _T("Installer");
            e.hiveLabel    = labels[h];
            e.hive         = hives[h];
            e.keyPath      = fullKey;
            e.valueName    = _T("InstallLocation");
            e.valueData    = locPath;
            e.description.Format(_T("\"%s\" install folder missing"),
                                 *dispName ? dispName : subName);
            e.deleteSubkey = true;
            e.fixed = e.ignored = false;
            m_issues.push_back(std::move(e));
        }
        RegCloseKey(hBase);
    }
}

// --- Interface / COM (LocalServer32 out-of-process servers) ---

void CRegistryScanner::ScanInterfaceCom(ProgressFn& /*fn*/)
{
    HKEY hBase = nullptr;
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, _T("CLSID"), 0, KEY_READ, &hBase) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR clsid[128] = {};
        DWORD len = _countof(clsid);
        if (RegEnumKeyEx(hBase, idx, clsid, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        CString srvKey;
        srvKey.Format(_T("CLSID\\%s\\LocalServer32"), clsid);

        HKEY hSrv = nullptr;
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT, srvKey, 0, KEY_READ, &hSrv) != ERROR_SUCCESS)
            continue;

        TCHAR srvPath[MAX_PATH * 2] = {};
        DWORD srvLen = sizeof(srvPath), srvType = 0;
        LONG rv = RegQueryValueEx(hSrv, _T(""), nullptr, &srvType,
                                  reinterpret_cast<BYTE*>(srvPath), &srvLen);
        RegCloseKey(hSrv);

        if (rv != ERROR_SUCCESS || srvPath[0] == 0) continue;

        CString exePath = ExtractExePath(CString(srvPath));
        if (exePath.IsEmpty()) continue;
        if (IsAlwaysValidExe(exePath)) continue;
        if (IsInSystemDir(exePath)) continue;
        if (PathExists(exePath)) continue;

        CString fullKey;
        fullKey.Format(_T("CLSID\\%s\\LocalServer32"), clsid);

        RegistryIssue e;
        e.typeLabel    = _T("COM Server");
        e.hiveLabel    = _T("HKCR");
        e.hive         = HKEY_CLASSES_ROOT;
        e.keyPath      = fullKey;
        e.valueName    = _T("");
        e.valueData    = srvPath;
        e.description.Format(_T("COM server not found: %s"), exePath.GetString());
        e.deleteSubkey = true;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hBase);
}

// --- MUI Cache ---

void CRegistryScanner::ScanMuiCache(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache");

    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, kBase, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    // Track which exe paths we've already reported to avoid duplicates
    std::vector<CString> seen;

    static const TCHAR* kSuffixes[] = {
        _T(".FriendlyAppName"), _T(".ApplicationCompany"),
        _T(".MUIVerification"), nullptr
    };

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR  valueName[2048] = {};
        DWORD  nameLen = _countof(valueName), type = 0;
        DWORD  dataLen = 0;

        LONG r = RegEnumValue(hKey, idx, valueName, &nameLen, nullptr, &type, nullptr, &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;

        // Strip known MUI property suffixes to get the exe path
        CString vn(valueName);
        CString exePath = vn;
        for (int i = 0; kSuffixes[i]; ++i)
        {
            CString sfx(kSuffixes[i]);
            if (vn.Right(sfx.GetLength()).CompareNoCase(sfx) == 0)
            {
                exePath = vn.Left(vn.GetLength() - sfx.GetLength());
                break;
            }
        }

        // Deduplicate
        bool alreadySeen = false;
        for (const auto& s : seen)
            if (s.CompareNoCase(exePath) == 0) { alreadySeen = true; break; }
        if (alreadySeen) continue;
        seen.push_back(exePath);

        if (IsInSystemDir(exePath)) continue;
        if (IsAlwaysValidExe(exePath)) continue;
        if (PathExists(exePath)) continue;

        RegistryIssue e;
        e.typeLabel    = _T("MUI Cache");
        e.hiveLabel    = _T("HKCU");
        e.hive         = HKEY_CURRENT_USER;
        e.keyPath      = kBase;
        e.valueName    = valueName;
        e.valueData    = exePath;
        e.description.Format(_T("Cached app not found: %s"), exePath.GetString());
        e.deleteSubkey = false;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hKey);
}

// --- Shared DLLs ---

void CRegistryScanner::ScanSharedDlls(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SharedDLLs");

    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR  dllPath[2048] = {};
        DWORD  nameLen = _countof(dllPath);
        DWORD  refCount = 0, dataLen = sizeof(refCount), type = 0;

        LONG r = RegEnumValue(hKey, idx, dllPath, &nameLen,
                              nullptr, &type, reinterpret_cast<BYTE*>(&refCount), &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;

        CString path(dllPath);
        TCHAR expanded[MAX_PATH * 2] = {};
        if (ExpandEnvironmentStrings(path, expanded, _countof(expanded)) > 0)
            path = expanded;

        if (PathExists(path)) continue;

        RegistryIssue e;
        e.typeLabel    = _T("Missing Shared DLL");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = kBase;
        e.valueName    = dllPath;
        e.valueData.Format(_T("Refcount: %u"), refCount);
        e.description  = _T("DLL file missing: ") + CString(dllPath);
        e.deleteSubkey = false;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hKey);
}

// --- Obsolete Software (Uninstall entries with missing executable) ---

void CRegistryScanner::ScanUninstallEntries(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] =
        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    HKEY hives[]   = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    LPCTSTR labels[] = { _T("HKLM"), _T("HKCU") };

    for (int h = 0; h < 2; ++h)
    {
        HKEY hBase = nullptr;
        if (RegOpenKeyEx(hives[h], kBase, 0, KEY_READ, &hBase) != ERROR_SUCCESS)
            continue;

        for (DWORD idx = 0; ; ++idx)
        {
            TCHAR subName[256] = {};
            DWORD subLen = _countof(subName);
            if (RegEnumKeyEx(hBase, idx, subName, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            HKEY hSub = nullptr;
            if (RegOpenKeyEx(hBase, subName, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
                continue;

            TCHAR  ustr[2048] = {};
            DWORD  ulen = sizeof(ustr), utype = 0;
            if (RegQueryValueEx(hSub, _T("UninstallString"), nullptr, &utype,
                                reinterpret_cast<BYTE*>(ustr), &ulen) == ERROR_SUCCESS
                && (utype == REG_SZ || utype == REG_EXPAND_SZ))
            {
                CString cmd(ustr);
                CString exePath = ExtractExePath(cmd);

                if (!IsAlwaysValidExe(exePath) && !PathExists(exePath))
                {
                    TCHAR dispName[512] = {};
                    DWORD dispLen = sizeof(dispName);
                    RegQueryValueEx(hSub, _T("DisplayName"), nullptr, nullptr,
                                    reinterpret_cast<BYTE*>(dispName), &dispLen);

                    CString fullKey;
                    fullKey.Format(_T("%s\\%s"), kBase, subName);

                    RegistryIssue e;
                    e.typeLabel    = _T("Obsolete Software");
                    e.hiveLabel    = labels[h];
                    e.hive         = hives[h];
                    e.keyPath      = fullKey;
                    e.valueName    = _T("UninstallString");
                    e.valueData    = cmd;
                    e.description.Format(_T("\"%s\" uninstaller not found"),
                                         *dispName ? dispName : subName);
                    e.deleteSubkey = true;
                    e.fixed = e.ignored = false;
                    m_issues.push_back(std::move(e));
                }
            }
            RegCloseKey(hSub);
        }
        RegCloseKey(hBase);
    }
}

// --- Open With Applications ---

void CRegistryScanner::ScanOpenWithApps(ProgressFn& /*fn*/)
{
    HKEY hBase = nullptr;
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, _T("Applications"), 0, KEY_READ, &hBase) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR appName[256] = {};
        DWORD len = _countof(appName);
        if (RegEnumKeyEx(hBase, idx, appName, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        CString cmdKey;
        cmdKey.Format(_T("Applications\\%s\\shell\\open\\command"), appName);

        HKEY hCmd = nullptr;
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT, cmdKey, 0, KEY_READ, &hCmd) != ERROR_SUCCESS)
            continue;

        TCHAR cmdStr[MAX_PATH * 2] = {};
        DWORD cmdLen = sizeof(cmdStr), cmdType = 0;
        LONG rv = RegQueryValueEx(hCmd, _T(""), nullptr, &cmdType,
                                  reinterpret_cast<BYTE*>(cmdStr), &cmdLen);
        RegCloseKey(hCmd);

        if (rv != ERROR_SUCCESS || cmdStr[0] == 0) continue;

        CString exePath = ExtractExePath(CString(cmdStr));
        if (exePath.IsEmpty()) continue;
        if (IsAlwaysValidExe(exePath)) continue;
        if (IsInSystemDir(exePath)) continue;
        if (PathExists(exePath)) continue;

        CString fullKey;
        fullKey.Format(_T("Applications\\%s\\shell\\open\\command"), appName);

        RegistryIssue e;
        e.typeLabel    = _T("Open With App");
        e.hiveLabel    = _T("HKCR");
        e.hive         = HKEY_CLASSES_ROOT;
        e.keyPath      = fullKey;
        e.valueName    = _T("");
        e.valueData    = cmdStr;
        e.description.Format(_T("Open-with app not found: %s"), exePath.GetString());
        e.deleteSubkey = true;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hBase);
}

// --- Run At Startup ---

void CRegistryScanner::ScanOneRunKey(HKEY hive, const CString& hiveLabel,
                                      LPCTSTR subkey, bool runOnce)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(hive, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR  name[512] = {};
        BYTE   data[4096] = {};
        DWORD  nameLen = _countof(name), dataLen = sizeof(data), type = 0;

        LONG r = RegEnumValue(hKey, idx, name, &nameLen, nullptr, &type, data, &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;

        CString cmd(reinterpret_cast<LPCTSTR>(data));
        CString exePath = ExtractExePath(cmd);

        if (IsAlwaysValidExe(exePath)) continue;
        if (PathExists(exePath))       continue;

        RegistryIssue e;
        e.typeLabel    = runOnce ? _T("Startup (RunOnce)") : _T("Run At Startup");
        e.hiveLabel    = hiveLabel;
        e.hive         = hive;
        e.keyPath      = subkey;
        e.valueName    = name;
        e.valueData    = cmd;
        e.description  = _T("Executable not found: ") + exePath;
        e.deleteSubkey = false;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hKey);
}

void CRegistryScanner::ScanStartupEntries(ProgressFn& /*fn*/)
{
    static const TCHAR kRun[]     = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
    static const TCHAR kRunOnce[] = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce");

    ScanOneRunKey(HKEY_CURRENT_USER,  _T("HKCU"), kRun,     false);
    ScanOneRunKey(HKEY_LOCAL_MACHINE, _T("HKLM"), kRun,     false);
    ScanOneRunKey(HKEY_CURRENT_USER,  _T("HKCU"), kRunOnce, true);
    ScanOneRunKey(HKEY_LOCAL_MACHINE, _T("HKLM"), kRunOnce, true);
}

// --- Sound Events ---

void CRegistryScanner::ScanSoundEvents(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] = _T("AppEvents\\Schemes\\Apps");

    HKEY hApps = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, kBase, 0, KEY_READ, &hApps) != ERROR_SUCCESS)
        return;

    for (DWORD ai = 0; ; ++ai)
    {
        TCHAR appName[256] = {};
        DWORD appLen = _countof(appName);
        if (RegEnumKeyEx(hApps, ai, appName, &appLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        HKEY hApp = nullptr;
        CString appPath(kBase);
        appPath += _T("\\"); appPath += appName;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, appPath, 0, KEY_READ, &hApp) != ERROR_SUCCESS)
            continue;

        for (DWORD ei = 0; ; ++ei)
        {
            TCHAR evtName[256] = {};
            DWORD evtLen = _countof(evtName);
            if (RegEnumKeyEx(hApp, ei, evtName, &evtLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            CString curKey = appPath + _T("\\") + evtName + _T("\\.Current");
            HKEY hCur = nullptr;
            if (RegOpenKeyEx(HKEY_CURRENT_USER, curKey, 0, KEY_READ, &hCur) != ERROR_SUCCESS)
                continue;

            TCHAR  wavPath[MAX_PATH * 2] = {};
            DWORD  wavLen = sizeof(wavPath), wavType = 0;
            LONG rv = RegQueryValueEx(hCur, _T(""), nullptr, &wavType,
                                      reinterpret_cast<BYTE*>(wavPath), &wavLen);
            RegCloseKey(hCur);

            if (rv != ERROR_SUCCESS || wavPath[0] == 0) continue;
            if (wavType != REG_SZ && wavType != REG_EXPAND_SZ) continue;

            TCHAR expanded[MAX_PATH * 2] = {};
            ExpandEnvironmentStrings(wavPath, expanded, _countof(expanded));
            CString path(expanded);

            if (PathExists(path)) continue;

            CString issueKey = appPath + _T("\\") + evtName + _T("\\.Current");

            RegistryIssue e;
            e.typeLabel    = _T("Sound Event");
            e.hiveLabel    = _T("HKCU");
            e.hive         = HKEY_CURRENT_USER;
            e.keyPath      = issueKey;
            e.valueName    = _T("");
            e.valueData    = wavPath;
            e.description.Format(_T("Sound file not found: %s"), path.GetString());
            e.deleteSubkey = false;
            e.fixed = e.ignored = false;
            m_issues.push_back(std::move(e));
        }
        RegCloseKey(hApp);
    }
    RegCloseKey(hApps);
}

// --- Windows Services ---

void CRegistryScanner::ScanWindowsServices(ProgressFn& /*fn*/)
{
    static const TCHAR kBase[] = _T("SYSTEM\\CurrentControlSet\\Services");

    HKEY hBase = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kBase, 0, KEY_READ, &hBase) != ERROR_SUCCESS)
        return;

    for (DWORD idx = 0; ; ++idx)
    {
        TCHAR svcName[256] = {};
        DWORD len = _countof(svcName);
        if (RegEnumKeyEx(hBase, idx, svcName, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        HKEY hSvc = nullptr;
        if (RegOpenKeyEx(hBase, svcName, 0, KEY_READ, &hSvc) != ERROR_SUCCESS)
            continue;

        TCHAR imgPath[2048] = {};
        DWORD imgLen = sizeof(imgPath), imgType = 0;
        LONG rv = RegQueryValueEx(hSvc, _T("ImagePath"), nullptr, &imgType,
                                  reinterpret_cast<BYTE*>(imgPath), &imgLen);
        RegCloseKey(hSvc);

        if (rv != ERROR_SUCCESS) continue;
        if (imgType != REG_SZ && imgType != REG_EXPAND_SZ) continue;
        if (imgPath[0] == 0) continue;

        CString exePath = ExpandServiceImagePath(CString(imgPath));
        if (exePath.IsEmpty()) continue;
        if (IsAlwaysValidExe(exePath)) continue;
        if (IsInSystemDir(exePath)) continue;
        if (PathExists(exePath)) continue;

        CString fullKey;
        fullKey.Format(_T("%s\\%s"), kBase, svcName);

        RegistryIssue e;
        e.typeLabel    = _T("Windows Service");
        e.hiveLabel    = _T("HKLM");
        e.hive         = HKEY_LOCAL_MACHINE;
        e.keyPath      = fullKey;
        e.valueName    = _T("ImagePath");
        e.valueData    = imgPath;
        e.description.Format(_T("Service executable not found: %s"), exePath.GetString());
        e.deleteSubkey = true;
        e.fixed = e.ignored = false;
        m_issues.push_back(std::move(e));
    }
    RegCloseKey(hBase);
}

// ---- Fix ----

bool CRegistryScanner::FixIssue(RegistryIssue& issue)
{
    LONG r;
    if (issue.deleteSubkey)
    {
        r = RegDeleteTree(issue.hive, issue.keyPath);
    }
    else
    {
        HKEY hKey = nullptr;
        r = RegOpenKeyEx(issue.hive, issue.keyPath, 0, KEY_WRITE, &hKey);
        if (r == ERROR_SUCCESS)
        {
            r = RegDeleteValue(hKey, issue.valueName);
            RegCloseKey(hKey);
        }
    }
    if (r == ERROR_SUCCESS)
    {
        issue.fixed = true;
        return true;
    }
    return false;
}

// ---- Backup to .reg file (UTF-16 LE, double-click to restore) ----

bool CRegistryScanner::BackupIssuesToFile(const CString& filePath,
                                           const std::vector<int>& indices) const
{
    CFile file;
    if (!file.Open(filePath, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive))
        return false;

    auto writeW = [&](const CString& s) {
        file.Write(s.GetString(), (UINT)(s.GetLength() * sizeof(TCHAR)));
    };

    WORD bom = 0xFEFF;
    file.Write(&bom, 2);
    writeW(_T("Windows Registry Editor Version 5.00\r\n"));
    writeW(_T("; Backup created by DiskForge Registry Scanner\r\n\r\n"));

    struct KeyGroup { HKEY hive; CString hiveLabel; CString keyPath; };
    std::vector<KeyGroup> seen;

    for (int idx : indices)
    {
        if (idx < 0 || idx >= (int)m_issues.size()) continue;
        const RegistryIssue& e = m_issues[idx];

        bool headerWritten = false;
        for (const auto& kg : seen)
            if (kg.hive == e.hive && kg.keyPath == e.keyPath)
            { headerWritten = true; break; }

        if (!headerWritten)
        {
            seen.push_back({ e.hive, e.hiveLabel, e.keyPath });

            CString header;
            header.Format(_T("[%s\\%s]\r\n"), e.hiveLabel.GetString(), e.keyPath.GetString());

            if (e.deleteSubkey)
            {
                HKEY hSub = nullptr;
                if (RegOpenKeyEx(e.hive, e.keyPath, 0, KEY_READ, &hSub) == ERROR_SUCCESS)
                {
                    writeW(header);
                    for (DWORD vi = 0; ; ++vi)
                    {
                        TCHAR  vname[512] = {};
                        BYTE   vdata[4096] = {};
                        DWORD  vnamelen = _countof(vname), vdatalen = sizeof(vdata), vtype = 0;
                        LONG   rv = RegEnumValue(hSub, vi, vname, &vnamelen,
                                                 nullptr, &vtype, vdata, &vdatalen);
                        if (rv == ERROR_NO_MORE_ITEMS) break;
                        if (rv != ERROR_SUCCESS) continue;
                        writeW(ValueToRegLine(CString(vname), vtype, vdata, vdatalen));
                    }
                    RegCloseKey(hSub);
                    writeW(_T("\r\n"));
                }
            }
            else
            {
                HKEY hSub = nullptr;
                if (RegOpenKeyEx(e.hive, e.keyPath, 0, KEY_READ, &hSub) == ERROR_SUCCESS)
                {
                    BYTE  vdata[4096] = {};
                    DWORD vdatalen = sizeof(vdata), vtype = 0;
                    if (RegQueryValueEx(hSub, e.valueName, nullptr, &vtype,
                                        vdata, &vdatalen) == ERROR_SUCCESS)
                    {
                        writeW(header);
                        writeW(ValueToRegLine(e.valueName, vtype, vdata, vdatalen));
                        writeW(_T("\r\n"));
                    }
                    RegCloseKey(hSub);
                }
            }
        }
        else if (!e.deleteSubkey)
        {
            HKEY hSub = nullptr;
            if (RegOpenKeyEx(e.hive, e.keyPath, 0, KEY_READ, &hSub) == ERROR_SUCCESS)
            {
                BYTE  vdata[4096] = {};
                DWORD vdatalen = sizeof(vdata), vtype = 0;
                if (RegQueryValueEx(hSub, e.valueName, nullptr, &vtype,
                                    vdata, &vdatalen) == ERROR_SUCCESS)
                    writeW(ValueToRegLine(e.valueName, vtype, vdata, vdatalen));
                RegCloseKey(hSub);
            }
        }
    }

    file.Close();
    return true;
}
