# DiskForge

A Windows MFC application for exploring, recovering from and repairing physical disks at the raw-sector level.
Its window is built on the NewTest MFC framework (ribbon, Outlook-style tools bar, caption bar, docking
Properties / Output / Sector View panes), and its centre is the **System Browser**.

The registry and process tools that used to live inside DiskForge now have their own repositories - see
[Related Tools](#related-tools).

## System Browser

The System Browser walks a disk the way the data is laid out on it. Expand a disk in the left-hand tree:

```
Disk 0 - Samsung SSD (465.76 GB)
  Partition Table (GPT)
    #3  Basic data partition  -  NTFS  -  465 GB  [C:]
      Boot Record (NTFS)
        Master File Table ($MFT)          <- File Allocation for NTFS
          File System (\)
            Users
            OldStuff  (deleted)
            Orphaned deleted files
```

- **Partition Table** - MBR (including logical partitions inside an extended partition) or GPT, detected the
  same way as the Partition Table tool. Each partition shows the filesystem found in its first sector and its
  mounted drive letter. A disk formatted without a partition table (a "superfloppy" USB stick) shows one
  whole-disk volume.
- **Boot Record** - every BIOS Parameter Block field with its byte offset and meaning (FAT12/16/32, exFAT, NTFS).
- **File Allocation** - the structure that tracks which clusters are in use:
  - FAT12/16/32: the File Allocation Table - copies, size, entry width, used / free / bad clusters, FSInfo hints
  - exFAT: the Allocation Bitmap and FAT
  - NTFS: the Master File Table - location, record size, fragments, live and deleted record counts, and
    used/free clusters from `$Bitmap`
- **File System** - the folder tree and a file list with live **and deleted** entries. Deleted entries are shown
  in red with a delete overlay, and each one gets a **Recoverable** verdict and a note explaining it:

  | Verdict | Meaning |
  |---------|---------|
  | Yes - excellent | NTFS file small enough to be stored inside its MFT record; it comes back exactly |
  | Yes - good | Every cluster the file used is still free, so nothing has overwritten it |
  | Partially | Some of its clusters have been reused by other files; parts of the output will contain other data |
  | Unlikely | Most clusters reused, the file is compressed, or (exFAT) its fragment chain is gone |
  | No | All clusters reused, nothing recorded, or an EFS-encrypted file |

  How each filesystem is read:
  - **FAT12/16/32** - directory entries marked deleted (0xE5). Long file names are reassembled for deleted
    entries too, and the overwritten first character of the 8.3 name is reconstructed from the LFN checksum.
    FAT clears a deleted file's cluster chain, so recovery assumes the file was contiguous from its first
    cluster and checks those clusters are still free in the FAT. Deleted folders can be opened if their
    cluster still holds a directory.
  - **exFAT** - File / Stream / Name entry sets with the in-use bit cleared. Contiguous files (NoFatChain) keep
    their full extent, so they are judged against the Allocation Bitmap exactly.
  - **NTFS** - a deleted file disappears from its folder's index but its MFT record survives until reused, so
    the whole MFT is scanned once (on a background thread, with progress on the tree node and status bar) and
    the folder tree is rebuilt from each record's `$FILE_NAME` parent reference. Data runs (including
    attribute lists and fragmented files) are checked against `$Bitmap`. Deleted entries whose parent folder
    record has been reused are collected under **Orphaned deleted files**.
- **Recover Files** (ribbon, context menu or Tools bar) copies the selected files or folders - live or deleted,
  folders recursively - into a folder you choose, keeping their timestamps. It only ever writes to the
  destination; if that folder is on the same partition being recovered from, DiskForge warns first, since
  saving there can overwrite the deleted data before it is read. A log of every file goes to the Output pane.
- **Show deleted files / Deleted files only** filter the browser; **Show record / Show data** jump the Sector
  View to an entry's directory entry or MFT record, or to its first data sector.
- **Open Disk Image** browses a raw image file (for example one saved by Disk Copy) the same way, read-only.
  `DiskForge.exe <image>` opens one from the command line.
- Implemented in `FileSystem.h/.cpp` (shared interface, detection, run-based recovery), `FatFileSystem`,
  `ExFatFileSystem`, `NtfsFileSystem` (parsers), `DiskForgeDoc` (browser model), `BrowserTreeView` /
  `BrowserListView` (the two splitter panes) and `VolumeMap` (drive letters and the same-volume check).
  None of it opens a disk for writing.

## Features

- **Enumerates all physical drives** (PhysicalDrive0..N) on the system
- **Drive info display** — model, capacity, sector size, total sector count
- **Sector View pane** - hex viewer with dark theme that follows the System Browser selection, showing:
  - 64-bit byte offset column
  - 16-byte hex dump with mid-group spacing
  - ASCII representation column
  - Byte-level click selection
  - Smooth mouse-wheel + keyboard scrolling
- **Sector navigation** — First, Prev, Next, Last, and direct jump-to-sector (LBA box on the ribbon, decimal or 0x hex)
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
- **FAT32 Recovery (classic dialog)** (read-only against the source disk) — the original single-volume FAT32 recovery dialog, still available from the ribbon. The System Browser above supersedes it and also covers FAT12/16, exFAT and NTFS:
  - **FAT32 only in this pass.** Not FAT12, FAT16, exFAT, or NTFS - each has different enough on-disk structures that supporting them well needs its own dedicated pass; bolting them onto one reader risked getting all of them wrong. exFAT/NTFS support could follow the same architecture later
  - Reads raw LBAs directly, so it works whether or not Windows has mounted the volume or shows it as "RAW" - as long as the FAT32 metadata is still physically present. It does **not** help after a full reformat with a different filesystem, which overwrites that metadata; recovering after that needs file-signature carving through raw unallocated space, a different technique not implemented here
  - Full long-filename (LFN) reconstruction with checksum verification, cluster-chain walking via the FAT table, and a lazy-loaded tree view so large volumes don't require walking the whole directory structure up front
  - **Deleted-file recovery is inherently best-effort, and the UI says so**: FAT clears a deleted file's cluster-chain links in the FAT table itself, so only the starting cluster survives. Recovery therefore assumes the file's clusters are contiguous from that point - correct for the common case, especially on flash media, but it will produce corrupted output for a file that was fragmented, and there's no way to detect that from FAT metadata alone. Live (non-deleted) files instead properly follow their intact FAT chain
  - A deleted short (8.3) entry has its first character overwritten by the OS and shown as `_`; when a preceding long-filename entry is still present, the real name is used instead (checksum-verified for live entries; trusted by adjacency for deleted ones, since the checksum can't be verified against an already-overwritten first byte)
  - Recovery always writes to a **new** file the user chooses via Save As - never back to the source disk
  - Implemented in `Fat32Reader.h/.cpp` (the parser) and `FileRecoveryDialog.h/.cpp` (UI)

### Disk Tools
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
- **Wipe Disk / Partition** (write-capable) — click **Wipe Disk...** to overwrite a whole disk or a single partition:
  - Patterns: zeros (1 pass), pseudo-random data (1 pass), zeros-then-random (2 passes), or random x3 (3 passes)
  - **Honesty note, surfaced in the UI and worth restating here**: this is a straightforward overwrite tool, not a certified secure-erase implementation. It doesn't claim to meet NIST 800-88, DoD 5220.22-M, or any other sanitization standard; it doesn't attempt to reach HPA/DCO-hidden regions or already-remapped-but-unerased SSD flash blocks; and its "random" pass uses the standard C library PRNG (seeded from `GetTickCount()`), not a cryptographic one. For genuinely sensitive data, a drive's own ATA/NVMe Secure Erase or Sanitize command (not implemented here) is the more thorough option
  - Same write discipline as everything else: system-drive protection, volume locking/dismounting before any write, background thread with progress/cancel, and a type-to-confirm gate (`ERASE PhysicalDriveN` for whole-disk, `WIPE PhysicalDriveN PART <n>` for a specific partition)
  - Implemented in `WipeEngine.h/.cpp` and `WipeDiskDialog.h/.cpp` (UI)
- **User interface (NewTest framework)** - `CWinAppEx` / `CFrameWndEx` with a ribbon (Home: disks, System Browser,
  Sector View, tools; View: panes and application look), an Outlook-style Tools bar, a caption bar, dockable
  Properties / Output / Sector View panes that can float or auto-hide, a ribbon status bar, and the eleven
  application looks (Office 2007 styles, Windows 7, Visual Studio 2005/2008, ...). Layout and look are saved per user
- **Resizable window** with a sensible minimum size
- **UAC elevation** — the manifest automatically requests Administrator rights at launch

## Related Tools

These used to be DiskForge features. They have nothing to do with disks, so each is now a separate program
with its own repository, solution and build pipeline:

| Tool | What it does |
|------|--------------|
| **File Unlocker** | Finds and closes the processes locking a file or folder; adds "Unlock with File Unlocker" to Explorer's right-click menu |
| **Startup Manager** | Views, adds, deletes and enables/disables the programs Windows starts at logon (HKCU/HKLM Run and RunOnce) |
| **Registry Cleaner** | Finds registry entries pointing to missing files in 15 categories and removes them, with a `.reg` backup |

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

DiskForge **must run as Administrator** to read raw disk sectors; its manifest requests elevation. Disk images
can be browsed with **Open Disk Image** without touching a physical disk.

1. Start `DiskForge.exe` (accept the UAC prompt). Every physical disk appears in the System Browser
2. Expand a disk: **Partition Table -> partition -> Boot Record -> File Allocation -> File System**
3. Select items to see their fields in the list and the Properties pane; the Sector View shows the matching sector
4. In a folder, select deleted files (red) and click **Recover Files** to copy them to a folder on another disk
5. Disk tools (Partition Table, Disk Copy, Bad Sector Scan, Disk Health, MBR to GPT, Partition Editor, Wipe,
   UEFI Boot, Lost Partitions) are on the ribbon and the Tools bar

## Project Structure

| File | Description |
|------|-------------|
| `DiskForge.sln` | Visual Studio 2022 solution |
| `DiskForge.vcxproj` | MSBuild project (x64, v143 toolset) |
| `DiskForge.cpp/h` | `CWinAppEx` application class (NewTest framework: look, context menus, tooltips, saved layout) |
| `MainFrm.cpp/h` | `CFrameWndEx` main frame: ribbon, Tools bar, caption bar, docking panes, all commands |
| `DiskForgeDoc.cpp/h` | System Browser model: disks, partition tables, browser nodes, cached folder listings |
| `BrowserTreeView.cpp/h` | Left pane: the Disk -> Partition Table -> ... -> File System tree; runs the NTFS MFT scan thread |
| `BrowserListView.cpp/h` | Right pane: structure fields, partition entries, or files with deleted / recoverable columns |
| `FileSystem.cpp/h` | `IFileSystem` interface, filesystem detection, run-based recovery helpers |
| `FatFileSystem.cpp/h` | FAT12/16/32 parser for the System Browser (deleted entries, LFN repair, recoverability) |
| `ExFatFileSystem.cpp/h` | exFAT parser (entry sets, allocation bitmap, NoFatChain extents) |
| `NtfsFileSystem.cpp/h` | NTFS parser (MFT scan, fixups, data runs, attribute lists, `$Bitmap` checks) |
| `VolumeMap.cpp/h` | Maps drive letters to disk extents (partition labels, same-volume recovery warning) |
| `HexPane.cpp/h` | Sector View docking pane hosting `CHexView` |
| `OutputWnd.cpp/h` | Output docking pane (log) |
| `PropertiesWnd.cpp/h` | Properties docking pane (read-only property grid) |
| `ViewTree.cpp/h` | Tools bar page trees (each item sends a command) |
| `ShellIcons.cpp/h` | Windows stock icons for the ribbon, tree and list |
| `res\` | Application icon and the NewTest framework's pane / Outlook-bar / caption-bar images |
| `DiskManager.cpp/h` | Win32 disk I/O: `CreateFile`, `DeviceIoControl`, sector-aligned `ReadFile` / multi-sector `ReadSectors`; also opens raw image files |
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
| `pch.cpp/h` | Pre-compiled header |
| `DiskForge.rc` | Resources: dialogs, menus, strings, icons and bitmaps |
| `resource.h` | Resource ID definitions |
| `DiskForge.manifest` | DPI awareness + Win10/11 compatibility manifest |

## Technical Notes

- Uses `FILE_FLAG_NO_BUFFERING` with `VirtualAlloc`-aligned buffers for raw sector reads
- Queries disk geometry via `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX`
- Retrieves drive model strings via `IOCTL_STORAGE_QUERY_PROPERTY`
- Supports drives with sector sizes > 512 bytes (e.g. 4K native drives)
- Sectors are 0-indexed (sector 0 = LBA 0 = MBR/GPT header area)
