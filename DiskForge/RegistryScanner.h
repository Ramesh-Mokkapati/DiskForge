// RegistryScanner.h - Registry issue detection and repair engine.
#pragma once
#include <vector>
#include <functional>

struct RegistryIssue {
    CString typeLabel;       // Display category name
    CString hiveLabel;       // "HKCU" or "HKLM" or "HKCR"
    HKEY    hive;
    CString keyPath;         // Key path without hive prefix
    CString valueName;       // Value name; empty = default value
    CString valueData;       // Current data (for display)
    CString description;     // One-line problem description
    bool    deleteSubkey;    // true = RegDeleteTree(keyPath); false = RegDeleteValue(valueName)
    bool    fixed;
    bool    ignored;
};

struct ScanOptions {
    bool appPaths      = false;
    bool browserHelper = false;
    bool fileExt       = false;
    bool firewall      = false;
    bool fonts         = false;
    bool helpFiles     = false;
    bool installers    = false;
    bool interfaceCom  = false;
    bool muiCache      = false;
    bool sharedDlls    = false;
    bool uninstall     = false;   // "Obsolete Software"
    bool openWith      = false;
    bool startup       = false;   // "Run At Startup"
    bool soundEvents   = false;
    bool services      = false;
};

class CRegistryScanner
{
public:
    // progressFn(percentComplete 0–100, phaseDescription)
    using ProgressFn = std::function<void(int, const CString&)>;

    void Scan(const ScanOptions& opts, ProgressFn progressFn = nullptr);

    const std::vector<RegistryIssue>& GetIssues() const { return m_issues; }
          std::vector<RegistryIssue>& GetIssues()       { return m_issues; }

    static bool FixIssue(RegistryIssue& issue);
    bool BackupIssuesToFile(const CString& filePath,
                            const std::vector<int>& indices) const;

private:
    std::vector<RegistryIssue> m_issues;

    void ScanAppPaths(ProgressFn& fn);
    void ScanBrowserHelperObjects(ProgressFn& fn);
    void ScanFileExtensions(ProgressFn& fn);
    void ScanFirewallRules(ProgressFn& fn);
    void ScanFonts(ProgressFn& fn);
    void ScanHelpFiles(ProgressFn& fn);
    void ScanInstallers(ProgressFn& fn);
    void ScanInterfaceCom(ProgressFn& fn);
    void ScanMuiCache(ProgressFn& fn);
    void ScanSharedDlls(ProgressFn& fn);
    void ScanUninstallEntries(ProgressFn& fn);
    void ScanOpenWithApps(ProgressFn& fn);
    void ScanStartupEntries(ProgressFn& fn);
    void ScanSoundEvents(ProgressFn& fn);
    void ScanWindowsServices(ProgressFn& fn);

    void ScanOneRunKey(HKEY hive, const CString& hiveLabel,
                       LPCTSTR subkey, bool runOnce);

    static CString ExpandServiceImagePath(const CString& raw);
    static CString ExtractExePath(const CString& commandStr);
    static bool    PathExists(const CString& path);
    static bool    IsAlwaysValidExe(const CString& exePath);
    static bool    IsInSystemDir(const CString& fullPath);
    static CString EscapeRegStr(const CString& s);
    static CString ValueToRegLine(const CString& name, DWORD type,
                                  const BYTE* data, DWORD dataLen);
};
