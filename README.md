# DiskForge

A Windows MFC application that lets you browse raw disk sectors on physical drives.

## Features

- **Enumerates all physical drives** (PhysicalDrive0..N) on the system
- **Drive info display** — model, capacity, sector size, total sector count
- **Hex viewer** with dark theme showing:
  - 64-bit byte offset column
  - 16-byte hex dump with mid-group spacing
  - ASCII representation column
  - Byte-level click selection
  - Smooth mouse-wheel + keyboard scrolling
- **Sector navigation** — First, Prev, Next, Last, and direct jump-to-sector
- **Partition table viewer** (read-only) — click **Partitions...** once a drive is open to see:
  - Automatic **MBR** or **GPT** detection (via the protective-MBR type 0xEE check)
  - MBR: all 4 primary entries, plus logical partitions inside an extended partition (EBR chain walk)
  - GPT: disk GUID, all partition entries with type GUID, unique GUID-based type name, and partition name
  - Boot/ESP flag, start LBA, sector count, and human-readable size for each entry
  - Double-click (or "Go to Start Sector") to jump the hex view straight to a partition's first sector
  - Implemented in `PartitionParser.h/.cpp` (pure parsing, no disk I/O beyond `ReadSector`) and `PartitionDialog.h/.cpp` (UI) — it never writes to the drive
- **Disk Copy** — click **Disk Copy...** to clone a whole disk or a single partition to either another physical disk or an image file, **or restore a previously-saved image file back onto a physical disk**:
  - Source: any enumerated physical drive (whole-disk or a single partition via the partition picker), **or an existing image file**
  - Destination: another physical disk, or an image file on this PC (image-to-image isn't offered - restoring from a file requires a physical disk destination)
  - Safety rails, since this is the only write-capable feature in the app:
    - The drive Windows is currently running from can **never** be selected as a destination (`SystemDiskGuard.h/.cpp`, checked both in the UI and again inside the copy engine)
    - Source and destination can't be the same physical drive
    - Disk-to-disk copies require matching sector sizes (512e/4Kn mismatches are refused rather than silently truncated/misaligned); restoring from a file has no such restriction - the write side pads the final chunk to a full destination sector as needed
    - Destination capacity is checked before starting
    - All volumes on the destination disk are locked and dismounted (`FSCTL_LOCK_VOLUME`/`FSCTL_DISMOUNT_VOLUME`) before any raw sector write; if any volume can't be locked, the copy aborts instead of writing underneath a mounted filesystem
    - A **type-to-confirm** dialog (`ConfirmTextDialog.h/.cpp`) requires typing an exact phrase (e.g. `ERASE PhysicalDrive2`, or `OVERWRITE` for an existing image file) before anything is overwritten
    - Copy runs on a background thread with a live progress bar and a Cancel button; a cancelled disk-to-disk copy leaves the destination partially written and says so explicitly, while a cancelled/failed image-file copy deletes the partial file
  - Implemented in `DiskCopyEngine.h/.cpp` (the only module that opens a `GENERIC_WRITE` handle to a physical drive - now handles physical-drive-source, image-file-source, physical-drive-dest, and image-file-dest in every valid combination) and `DiskCopyDialog.h/.cpp` (UI)
  - **Restore/copy to a specific destination partition**, not just the whole disk: when the destination is a physical disk, choose "Write to one partition" and pick it via the same partition picker used elsewhere. This writes starting at that partition's own start LBA instead of always LBA 0, and every safety check (capacity, confirmation wording, the `OVERWRITE PhysicalDriveN PART <n>` type-to-confirm phrase) is scoped to that partition specifically - the confirmation dialog is explicit that other partitions on the disk are not touched, and the summary panel reflects the partition's own capacity rather than the whole disk's
- **Bad Sector Scan** (read-only) — click **Bad Sector Scan...** to run a surface test against a whole disk, a single partition, or a custom sector range:
  - Reads the range in adaptively-sized chunks (bigger chunks for bigger scans, capped at 64 MB, so progress updates stay frequent even on huge disks)
  - A chunk that fails to read is automatically bisected down to the exact failing sector(s), instead of aborting or falling back to a slow sector-by-sector scan of the whole disk
  - Chunks that succeed but took unusually long (under ~5 MB/s effective throughput) are flagged "Slow" - often an early warning sign, though this is a heuristic, not authoritative SMART data
  - A live color-coded surface map (green/yellow/red/gray = ok/slow/bad/not-yet-scanned) and a list of exact bad sector ranges (start LBA, sector count, byte offset) update as the scan runs
  - Runs on a background thread with Start/Stop; stopping is immediate and safe since nothing is ever written
  - **Caveat surfaced in the UI**: drives transparently remap sectors they've already identified as bad, so this can only detect sectors that are *currently* failing to read - it's a useful diagnostic, not a replacement for the drive's own SMART self-test
  - Implemented in `BadSectorScanner.h/.cpp` (pure read-only engine), `ScanMapView.h/.cpp` (custom-drawn surface map control), and `BadSectorScanDialog.h/.cpp` (UI)
  - **Repair Selected Sector...** (write-capable): select a row in the found-bad-sectors list and attempt to make it usable again by writing zeros and re-reading. This does **not** recover the data that was there - it was already unreadable - it only tries to make the LBA usable again, the same mechanism `chkdsk /r` and similar tools rely on (a write gives the drive's firmware a chance to fix the sector in place or reallocate it from its spare pool). If the write itself fails, nothing has improved; if the write succeeds but a re-read still fails, the dialog says so and suggests the drive may need replacing. Same write discipline as everything else (system-drive guard, volume locking) plus a plain-language warning before writing. Implemented in `SectorRepairer.h/.cpp`
- **UEFI Boot Entry Manager** (write-capable, no disk I/O) — click **UEFI Boot...** to manage NVRAM boot variables directly, entirely separate from every other feature in this app since it never touches a disk sector:
  - Only works on UEFI systems (checked via `GetFirmwareType`) - reports plainly when a system is legacy BIOS instead
  - Read-only: lists the current `BootOrder` with each entry's description and active flag
  - Write operations: reorder `BootOrder` (Move Up/Down + explicit Apply), set/clear a one-time `BootNext` override, delete an existing entry (also removes it from `BootOrder` for consistency)
  - **Scope limitation, stated plainly rather than worked around**: only entries currently listed in `BootOrder` are discovered, since there's no documented Win32 API to enumerate every NVRAM variable (only undocumented native APIs can do that). An orphaned `BootXXXX` entry not in `BootOrder` simply won't appear
  - **Deliberately does not create new boot entries.** That requires constructing a correct `EFI_LOAD_OPTION` including an EFI Device Path Protocol structure - intricate, firmware-implementation-sensitive binary data that genuinely needs validation against real hardware, which isn't available in this development environment. A malformed entry wouldn't damage anything, but it also wouldn't boot, and shipping that unvalidated isn't worth it
  - Implemented in `EfiBootManager.h/.cpp` and `EfiBootDialog.h/.cpp` (UI)

### Data Recovery
- **Lost Partition Search** (read-only) — click **Lost Partitions...** to scan a disk's currently-unallocated space for filesystem boot-sector signatures (NTFS, FAT32, FAT16, FAT12, exFAT), to find partitions whose table entry is missing or corrupted while the actual filesystem data is still intact:
  - Only scans space **not** already covered by an existing partition table entry - correct (an existing partition's own boot sector isn't "lost") and faster (skips known-allocated regions on an otherwise-full disk)
  - Parses each candidate's boot sector for bytes-per-sector, sectors-per-cluster, total volume size, and (for FAT12/16/32 only, since that's where it's actually stored) the volume label
  - **Scope limitation stated plainly**: this only reads and parses the boot sector - it never mounts or walks into the filesystem, and it never writes anything. Turning a found candidate back into a live partition table entry (creating a new partition) is a separate capability this tool does not perform
  - **Known false-positive case, called out rather than hidden**: NTFS keeps a *backup* copy of its boot sector at a volume's very last sector, which carries the same signature and describes the same volume. The scanner can legitimately report this as a second hit; when a candidate's reported size would extend past the physical disk, the size is shown as "(unknown)" rather than trusted, but the hit itself is still reported since the signature match is real
  - Runs on a background thread with progress and Stop, like the other scan features
  - Implemented in `LostPartitionScanner.h/.cpp` (pure read-only engine) and `LostPartitionDialog.h/.cpp` (UI)
- **Recover Deleted Files** (read-only against the source disk) — click **Recover Files...** to browse a FAT32 volume's directory tree, including deleted entries, and recover individual files:
  - **FAT32 only in this pass.** Not FAT12, FAT16, exFAT, or NTFS - each has different enough on-disk structures that supporting them well needs its own dedicated pass; bolting them onto one reader risked getting all of them wrong. exFAT/NTFS support could follow the same architecture later
  - Reads raw LBAs directly, so it works whether or not Windows has mounted the volume or shows it as "RAW" - as long as the FAT32 metadata is still physically present. It does **not** help after a full reformat with a different filesystem, which overwrites that metadata; recovering after that needs file-signature carving through raw unallocated space, a different technique not implemented here
  - Full long-filename (LFN) reconstruction with checksum verification, cluster-chain walking via the FAT table, and a lazy-loaded tree view so large volumes don't require walking the whole directory structure up front
  - **Deleted-file recovery is inherently best-effort, and the UI says so**: FAT clears a deleted file's cluster-chain links in the FAT table itself, so only the starting cluster survives. Recovery therefore assumes the file's clusters are contiguous from that point - correct for the common case, especially on flash media, but it will produce corrupted output for a file that was fragmented, and there's no way to detect that from FAT metadata alone. Live (non-deleted) files instead properly follow their intact FAT chain
  - A deleted short (8.3) entry has its first character overwritten by the OS and shown as `_`; when a preceding long-filename entry is still present, the real name is used instead (checksum-verified for live entries; trusted by adjacency for deleted ones, since the checksum can't be verified against an already-overwritten first byte)
  - Recovery always writes to a **new** file the user chooses via Save As - never back to the source disk
  - Implemented in `Fat32Reader.h/.cpp` (the parser) and `FileRecoveryDialog.h/.cpp` (UI)

### Utilities
- **File Unlocker** — click **File Unlocker...** to find which process(es) currently have a chosen file open/locked, and optionally terminate them to release it:
  - A **completely different subsystem** from every other feature in this app: it works at the process/file-handle level via the Windows Restart Manager API (`RmStartSession`/`RmRegisterResources`/`RmGetList` - the same mechanism behind Explorer's own "This file is open in another program" dialog), and never touches disk sectors or partition tables at all
  - Its only "write" is terminating another process, which the dialog always confirms first - listing exactly which process(es) and PIDs are about to be closed and warning that unsaved work in them will be lost
  - **Refuses to terminate a short list of core Windows processes** (`csrss.exe`, `wininit.exe`, `winlogon.exe`, `services.exe`, `lsass.exe`, `smss.exe`, `System`, `Registry`) even if requested, since force-killing those can crash or destabilize the whole system - this check lives in the engine itself (`FileUnlocker::IsCriticalSystemProcess`), not just the UI, so it can't be bypassed by a different call path later. It also always refuses to terminate this app's own process
  - This is a genuine safety floor, not a complete "important processes" list - killing *any* process can lose unsaved work in it, which is why the confirmation dialog applies regardless of which process is targeted
  - Implemented in `FileUnlocker.h/.cpp` (the engine) and `FileUnlockerDialog.h/.cpp` (UI)
- **Disk Health / S.M.A.R.T.** (read-only) — click **Disk Health...** to check a drive's own self-reported health:
  - Uses `IOCTL_STORAGE_PREDICT_FAILURE` (a modern, broadly-supported "is this drive predicting its own failure" flag) as the primary signal
  - Additionally attempts the classic legacy SMART attribute table (via the well-known "Disk Failure Prediction" IOCTLs) for a detailed attribute list (ID, current/worst normalized values, raw value) when the driver/controller supports it, plus current temperature when reported
  - Flags known-critical attributes (Reallocated Sectors, Reallocated Event Count, Current Pending Sector Count, Uncorrectable Sector Count, Reported Uncorrectable Errors) if their raw value is nonzero
  - Every IOCTL used here declares `FILE_ANY_ACCESS`, so the drive handle is opened with `GENERIC_READ` only - this feature never requests write access
  - **Caveat surfaced in the UI**: SMART passthrough support varies a lot by storage stack. NVMe drives, many USB enclosures, and some RAID controllers commonly don't expose either mechanism to Windows, in which case the dialog says "not supported" rather than guessing
  - Implemented in `SmartDataReader.h/.cpp` (the query logic) and `SmartHealthDialog.h/.cpp` (UI)
- **MBR ↔ GPT Conversion** (write-capable, MBR → GPT only) — click **MBR<->GPT...** to convert a disk's partitioning scheme in place, without moving, resizing, or touching any partition's actual data:
  - **Scope is deliberately conservative.** Only MBR → GPT is implemented (GPT → MBR is lossy - different partition types, a 4-partition/2 TB ceiling - and isn't offered). Only simple primary-partition MBR disks are supported; a disk with an extended partition / logical drives is refused outright, since there's no safe automatic way to linearize those into GPT.
  - **Never shrinks or moves anything.** GPT needs free space at both the very start (LBA 1-33) and very end (last 33 sectors) of the disk for its header/partition-array structures. If an existing partition already occupies that space, the tool refuses and explains exactly why - it will not auto-shrink a partition to make room.
  - **Enforced sequence in the UI:** Analyze (read-only) → mandatory backup of every sector about to be overwritten, saved to a file the user chooses → only then does Convert unlock → type-to-confirm (`CONVERT PhysicalDriveN`) before anything is written.
  - A **Restore From Backup** action reads that backup file back and writes each captured region to its original LBA, independent of the Disk Copy engine (which only ever restores starting at LBA 0) - this is the recovery path if a conversion needs to be undone.
  - Constructs a spec-correct protective MBR, primary + backup GPT headers, and primary + backup 128-entry partition arrays (CRC32-checked per the UEFI spec); writes backup structures first, primary structures last, and calls `IOCTL_DISK_UPDATE_PROPERTIES` afterward so Windows re-reads the new table.
  - **Does not touch boot configuration.** The dialog explicitly warns that an OS installed for legacy BIOS/MBR boot will very likely not boot after conversion without separately reconfiguring it for UEFI/GPT.
  - Implemented in `MbrGptConverter.h/.cpp` (the only other module besides `DiskCopyEngine` that opens a drive with `GENERIC_WRITE`) and `MbrGptConvertDialog.h/.cpp` (UI)
- **Partition Editor** (write-capable) — click **Partition Editor...** to edit, delete, or extend an existing partition's table entry, without touching filesystem data inside partitions that aren't being extended:
  - **MBR metadata edit**: toggle the Active/Boot flag; change the type byte (from a common-types list or free-form `0xNN`); toggle Hidden (remaps between the known visible/hidden type-byte pairs, e.g. `0x07`↔`0x17`)
  - **GPT metadata edit**: rename the partition (rewrites the `PartitionName` field), change the type GUID (from a fixed list of common types - defaults to "keep current type" so untouched fields are never silently overwritten), toggle the Legacy BIOS Bootable attribute bit, toggle the "no default drive letter" (hidden) attribute bit
  - **Delete Partition**: removes the partition table entry (zeroes the MBR slot or zeroes the GPT entry in both primary and backup arrays and recomputes CRCs). The data inside is not immediately erased but the partition is no longer accessible. Gated by a type-to-confirm dialog (`DELETE PhysicalDriveN PART x`) so an accidental click can't trigger it
  - **Extend Partition**: grows a partition into the contiguous free space that immediately follows it on disk. Uses `IOCTL_DISK_GROW_PARTITION` to update the partition table, then attempts `FSCTL_EXTEND_VOLUME` to expand the filesystem online (works for NTFS; for FAT32/exFAT the partition table is updated but the filesystem needs a manual CHKDSK pass). Also gated by a type-to-confirm dialog (`EXTEND PhysicalDriveN PART x`). The dialog shows the current partition size, the maximum available extension, and refuses to proceed if there is no free space immediately following the partition
  - **Shrink is not implemented, deliberately.** Shrinking a partition requires first shrinking the filesystem to a smaller size (a filesystem-specific operation that must understand file allocation and cluster maps, not a generic partition-table operation), then moving the partition's end boundary inward. Getting the ordering and the byte-exact boundary wrong corrupts data silently. The right tool for this is Disk Management / `diskpart` / a dedicated partition utility — bolting it onto this app without adequate on-hardware testing would be irresponsible
  - **Right-click context menu** on the partition list: Apply Changes / Delete Partition / Extend Partition / Assign Drive Letter
  - **Drive letter assignment** is handled separately via the Windows Mount Manager (`SetVolumeMountPoint`/`DeleteVolumeMountPoint`) - this involves no raw disk I/O at all, unlike everything else in this list
  - GPT edits update **both** the primary and backup partition arrays and recompute both header CRC32s, so the disk stays internally consistent after every write operation (metadata edits, deletes, and grows all follow this rule)
  - Same write discipline as the rest of the app: system-drive protection, volume locking/dismounting before any raw write, and a confirmation gate before applying
  - Implemented in `PartitionEditor.h/.cpp` (the third module with `GENERIC_WRITE` access), `PartitionEditDialog.h/.cpp` (main UI), and `ExtendPartitionDialog.h/.cpp` (the extend sub-dialog). Required a small additive extension to `PartitionEntry` (`PartitionParser.h/.cpp`) to carry raw GUID/attribute bytes and each entry's on-disk slot index
  - `VolumeLockHelper.h/.cpp` - the volume lock/dismount logic was extracted out of `DiskCopyEngine` into this shared module (also fixing a gap where `MbrGptConverter`'s writes weren't locking volumes first) so every write-capable feature uses the identical, single-source-of-truth safety mechanism
- **Startup Manager** — click **Startup Manager...** to view, add, delete, and enable/disable Windows startup registry entries:
  - Reads all four startup locations: `HKCU\...\Run`, `HKLM\...\Run`, `HKCU\...\RunOnce`, and `HKLM\...\RunOnce`
  - Shows each entry's name, command, enabled/disabled status, registry hive (HKCU/HKLM), and key type (Run/RunOnce) in a sortable list
  - **Enable/Disable** toggles the entry's `Explorer\StartupApproved\Run` binary flag (byte `0x02` = enabled, `0x03` = disabled) — the same mechanism Task Manager uses; the entry stays in the registry so it can be re-enabled
  - **Add** opens a sub-dialog to create a new startup entry: name, command (with file browser), choice of Current User (HKCU) or All Users (HKLM), and an optional Run Once flag
  - **Delete** removes the value from the Run/RunOnce key and also clears any residual `StartupApproved` entry, prompting for confirmation first
  - RunOnce entries cannot be enabled/disabled (they self-delete on first run, so there is nothing to toggle), and the button is grayed accordingly
  - Implemented in `StartupManagerDialog.h/.cpp` (contains both the main `CStartupManagerDialog` and the `CAddStartupEntryDialog` sub-dialog)
- **Registry Scanner** — click **Registry Scanner...** to find and optionally remove stale registry entries that point to missing files:
  - **15 scan categories** (each independently selectable via a checkbox):
    - **Application Paths** — `HKLM\App Paths` subkeys whose default-value executable is missing
    - **Browser Helper Objects** — `HKLM\…\Browser Helper Objects` CLSIDs whose `InprocServer32` DLL is missing
    - **File Extensions** — `HKCR` extension keys whose registered ProgID no longer exists
    - **Firewall Rules** — `HKLM\…\FirewallRules` values whose `App=` path no longer exists
    - **Fonts** — `HKLM\…\Fonts` values whose font file (relative to `%SystemRoot%\Fonts\` or absolute) is missing
    - **Help Files** — `HKLM\SOFTWARE\Microsoft\Windows\Help` values whose `.chm`/`.hlp` path is missing
    - **Installers (Obsolete Software)** — Uninstall subkeys whose `InstallLocation` directory no longer exists on disk
    - **Interface (COM)** — `HKCR\CLSID` entries with a `LocalServer32` whose executable is missing (System32 entries skipped)
    - **MUI Cache** — `HKCU\…\MuiCache` entries whose executable path (after stripping `.FriendlyAppName` / `.ApplicationCompany` suffixes) is missing
    - **Missing Shared DLLs** — `HKLM\SharedDLLs` values whose DLL file path no longer exists
    - **Obsolete Software** — `HKCU`/`HKLM` Uninstall subkeys whose `UninstallString` executable is missing (MSI/system executables such as `msiexec.exe` are automatically skipped to avoid false positives)
    - **Open With Apps** — `HKCR\Applications\*\shell\open\command` entries whose executable is missing
    - **Run At Startup** — `HKCU`/`HKLM` Run and RunOnce values whose executable path no longer exists on disk
    - **Sound Events** — `HKCU\AppEvents\Schemes` entries whose `.wav` file path is missing
    - **Windows Services** — `HKLM\SYSTEM\…\Services` ImagePath executables that are missing (handles `\SystemRoot\`, `\??\`, `%SystemRoot%\`, and bare-path prefixes; skips System32)
  - Scan runs on a **background thread** with a live progress bar and phase label; the UI stays responsive and all action buttons disable while scanning
  - Results list shows: category, name/key, problem description, hive source (HKCU/HKLM), and status (Pending/Fixed/Ignored)
  - **Fix Selected** — removes the selected entries from the registry after confirmation
  - **Fix All** — removes all pending entries after confirmation
  - **Backup & Fix All** — saves a proper UTF-16 LE Windows `.reg` file (double-clickable to restore) first, then fixes all entries; aborts if the backup write fails so nothing is changed without a valid backup
  - **Ignore Selected** — hides entries from the list for the current session without removing them
  - Registry path extraction handles quoted paths (`"C:\...\app.exe" /args`), unquoted paths, and environment-variable expansion (`%SystemRoot%`, etc.)
  - Fix method is type-aware: value-only issues (startup, shared DLL) delete just the value; subkey issues (uninstall, app path) use `RegDeleteTree` to remove the whole orphaned subkey
  - Implemented in `RegistryScanner.h/.cpp` (scanning engine + .reg-file backup writer) and `RegistryScannerDialog.h/.cpp` (UI)
- **Wipe Disk / Partition** (write-capable) — click **Wipe Disk...** to overwrite a whole disk or a single partition:
  - Patterns: zeros (1 pass), pseudo-random data (1 pass), zeros-then-random (2 passes), or random x3 (3 passes)
  - **Honesty note, surfaced in the UI and worth restating here**: this is a straightforward overwrite tool, not a certified secure-erase implementation. It doesn't claim to meet NIST 800-88, DoD 5220.22-M, or any other sanitization standard; it doesn't attempt to reach HPA/DCO-hidden regions or already-remapped-but-unerased SSD flash blocks; and its "random" pass uses the standard C library PRNG (seeded from `GetTickCount()`), not a cryptographic one. For genuinely sensitive data, a drive's own ATA/NVMe Secure Erase or Sanitize command (not implemented here) is the more thorough option
  - Same write discipline as everything else: system-drive protection, volume locking/dismounting before any write, background thread with progress/cancel, and a type-to-confirm gate (`ERASE PhysicalDriveN` for whole-disk, `WIPE PhysicalDriveN PART <n>` for a specific partition)
  - Implemented in `WipeEngine.h/.cpp` and `WipeDiskDialog.h/.cpp` (UI)
- **Resizable window** with a sensible minimum size
- **UAC elevation** — the manifest automatically requests Administrator rights at launch

## Requirements

- **OS:** Windows 10 / Windows 11 x64
- **Runtime:** Microsoft Visual C++ Redistributable 2022 (x64)  
  Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
- **Build:** Visual Studio 2022 (any edition) with "Desktop development with C++" workload and MFC libraries installed

## How to Build

1. Open `DiskForge.sln` in Visual Studio 2022
2. Select **Release | x64** configuration
3. Press **Ctrl+Shift+B**

Or from a Developer Command Prompt / PowerShell:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
    DiskForge.sln /p:Configuration=Release /p:Platform=x64
```

## How to Run

The application **must be run as Administrator** to access raw physical disk sectors.

1. Right-click `DiskForge.exe` → **Run as administrator**  
   (or the UAC prompt will appear automatically)
2. Click **Refresh** to enumerate all physical drives
3. Select a drive from the drop-down list
4. Click **Open Drive**
5. The first sector (sector 0 — the MBR or GPT header) loads automatically
6. Use **Prev / Next / First / Last** buttons or type a sector number and click **Go**

## Project Structure

| File | Description |
|------|-------------|
| `DiskForge.sln` | Visual Studio 2022 solution |
| `DiskForge.vcxproj` | MSBuild project (x64, v143 toolset) |
| `DiskForge.cpp/h` | MFC `CWinApp` application class |
| `DiskForgeDlg.cpp/h` | Main dialog — all controls created programmatically |
| `DiskManager.cpp/h` | Win32 disk I/O: `CreateFile`, `DeviceIoControl`, sector-aligned `ReadFile` |
| `HexView.cpp/h` | Custom GDI hex-dump control with double-buffering |
| `PartitionParser.cpp/h` | Read-only MBR/GPT partition table parser (uses `CDiskManager::ReadSector` only) |
| `PartitionDialog.cpp/h` | Modal dialog listing parsed partitions, with jump-to-sector |
| `SystemDiskGuard.cpp/h` | Identifies the physical drive(s) hosting the running Windows install, so Disk Copy can refuse to target them |
| `DiskCopyEngine.cpp/h` | Chunked, cancellable disk-copy engine; the only module that opens a drive with `GENERIC_WRITE` |
| `ConfirmTextDialog.cpp/h` | Generic "type this exact phrase to proceed" safety-gate dialog |
| `DiskCopyDialog.cpp/h` | UI for choosing source/destination and running a Disk Copy job |
| `BadSectorScanner.cpp/h` | Read-only surface scan engine with bisection to pinpoint exact bad sectors |
| `ScanMapView.cpp/h` | Custom-drawn control that renders the live surface-scan map |
| `BadSectorScanDialog.cpp/h` | UI for choosing a scan range and running/monitoring a Bad Sector Scan |
| `SmartDataReader.cpp/h` | Read-only S.M.A.R.T./predict-failure query (`GENERIC_READ` handle only) |
| `SmartHealthDialog.cpp/h` | UI for checking a drive's health/SMART data |
| `MbrGptConverter.cpp/h` | MBR->GPT conversion logic, sector backup/restore, CRC32 (only other module besides `DiskCopyEngine` that opens a drive with `GENERIC_WRITE`) |
| `MbrGptConvertDialog.cpp/h` | UI enforcing Analyze -> Backup -> Convert, plus Restore-From-Backup |
| `PartitionEditor.cpp/h` | Partition metadata edits, delete, extend (IOCTL_DISK_GROW_PARTITION + FSCTL_EXTEND_VOLUME), and drive-letter assignment |
| `PartitionEditDialog.cpp/h` | Main UI for the Partition Editor (metadata edit, delete, extend, context menu) |
| `ExtendPartitionDialog.cpp/h` | Sub-dialog for the Extend Partition operation |
| `VolumeLockHelper.cpp/h` | Shared volume lock/dismount logic used by every write-capable module |
| `WipeEngine.cpp/h` | Whole-disk/partition overwrite engine (zeros and/or pseudo-random, 1-3 passes) |
| `WipeDiskDialog.cpp/h` | UI for the Wipe Disk / Partition feature |
| `SectorRepairer.cpp/h` | Attempts to make a previously-bad sector usable again (write zeros + re-read verify) |
| `EfiBootManager.cpp/h` | UEFI NVRAM boot-variable read/reorder/set-next/delete (no disk sector I/O at all) |
| `EfiBootDialog.cpp/h` | UI for the UEFI Boot Entry Manager |
| `LostPartitionScanner.cpp/h` | Read-only filesystem-signature scan of unallocated space |
| `LostPartitionDialog.cpp/h` | UI for Lost Partition Search |
| `Fat32Reader.cpp/h` | Read-only FAT32 parser: boot sector, FAT chain walking, directory listing (incl. deleted entries), file recovery |
| `FileRecoveryDialog.cpp/h` | UI for browsing a FAT32 volume and recovering files |
| `FileUnlocker.cpp/h` | Finds/terminates processes locking a file via Restart Manager; refuses to kill core system processes or this app itself |
| `FileUnlockerDialog.cpp/h` | UI for the File Unlocker feature |
| `StartupManagerDialog.cpp/h` | Startup Manager: view/add/delete/enable/disable HKCU+HKLM Run and RunOnce registry entries; contains both the main dialog and the Add-Entry sub-dialog |
| `RegistryScanner.cpp/h` | Registry scanner engine: checks startup, uninstall, app-path, and shared-DLL entries for missing files; fixes by deleting values/subkeys; backup writer outputs UTF-16 LE .reg format |
| `RegistryScannerDialog.cpp/h` | UI for the Registry Scanner; background scan thread posts WM_RS_PROGRESS / WM_RS_COMPLETE messages back to the dialog |
| `SidebarButton.cpp/h` | Custom owner-drawn `CSidebarButton` control used for the left-sidebar navigation buttons |
| `pch.cpp/h` | Pre-compiled header |
| `DiskForge.rc` | Minimal RC resource file |
| `resource.h` | Resource ID definitions |
| `DiskForge.manifest` | DPI awareness + Win10/11 compatibility manifest |

## Technical Notes

- Uses `FILE_FLAG_NO_BUFFERING` with `VirtualAlloc`-aligned buffers for raw sector reads
- Queries disk geometry via `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX`
- Retrieves drive model strings via `IOCTL_STORAGE_QUERY_PROPERTY`
- Supports drives with sector sizes > 512 bytes (e.g. 4K native drives)
- Sectors are 0-indexed (sector 0 = LBA 0 = MBR/GPT header area)
