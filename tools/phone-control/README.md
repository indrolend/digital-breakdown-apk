# Digital Breakdown - Phone-Control Workflow

Phone-side transparency, host-compatible control, traceable runtime evidence.

## Architecture contract

```text
GitHub = code source of truth
Phone = runtime/evidence source of truth
Host computer = controller + optional evidence mirror
```

The stable phone root is always:

```text
/sdcard/Download/db-control/
  apks/
    current.apk
  evidence/
    latest/
      result.json
      install.txt
      launch.txt
      logcat-full.txt
      logcat-filtered.txt
      logcat-dbnative.txt
      logcat-crashes.txt
      screen.png
    archive/
      YYYYMMDD-HHMMSS/
  tools/
```

The APK stable path is:

```text
/sdcard/Download/db-control/apks/current.apk
```

The latest evidence stable path is:

```text
/sdcard/Download/db-control/evidence/latest/
```

## Compatibility contract

- PowerShell scripts must run under Windows PowerShell 5.1 and PowerShell 7+ on macOS.
- Host paths are built with PowerShell path helpers, not hardcoded Windows separators.
- Host home resolves from `USERPROFILE`, `HOME`, or the .NET user profile fallback.
- Host temp resolves from `[System.IO.Path]::GetTempPath()`.
- Host PATH edits use `[IO.Path]::PathSeparator`.
- Phone paths remain literal Android paths under `/sdcard/Download/db-control`.
- Termux utilities say `host side`, not `Windows side`, unless a command is truly Windows-only.
- `-PullEvidenceToWindows` remains supported as a legacy-compatible flag. The concept is now `host evidence mirror`.

## Quick start - Windows host

```powershell
. .\tools\phone-control\phone-session.ps1

DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -CleanBeforeRun `
  -ArchivePreviousEvidence
```

Optional host mirror:

```powershell
DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -PullEvidenceToWindows
```

## Quick start - macOS host

Install host tools:

```bash
brew install android-platform-tools
brew install --cask powershell || brew install powershell
```

Run the same PowerShell workflow:

```bash
pwsh
```

```powershell
. ./tools/phone-control/phone-session.ps1

DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -CleanBeforeRun `
  -ArchivePreviousEvidence
```

ADB must show the phone as authorized:

```bash
adb devices
```

## First-time phone bootstrap

Push the bootstrap script from the host if needed:

```bash
adb shell mkdir -p /sdcard/Download/db-control/tools
adb push tools/phone-control/termux-control-bootstrap.sh /sdcard/Download/db-control/tools/
adb shell monkey -p com.termux 1
```

Then run in Termux:

```bash
bash /sdcard/Download/db-control/tools/termux-control-bootstrap.sh
```

The bootstrap installs or checks:

| Tool | Purpose |
|------|---------|
| `git` | Version control |
| `curl` | HTTP requests |
| `jq` | JSON processing |
| `openssh` | SSH server (`sshd`) |
| `nano` | Text editor |
| `termux-api` | Android clipboard bridge |
| `gh` | GitHub CLI for artifact downloads and PR/run inspection |

Verify GitHub auth in Termux:

```bash
gh auth status
# If needed:
gh auth login
```

Verify SSH in Termux:

```bash
pgrep -x sshd && echo "sshd running" || sshd
```

## Host session helpers

Dot-source `phone-session.ps1` from Windows PowerShell or macOS `pwsh`:

```powershell
. ./tools/phone-control/phone-session.ps1
```

Loaded commands:

| Command | Description |
|---------|-------------|
| `OpenTermux` | Open Termux app on phone via ADB |
| `Phone` | Open interactive SSH session into Termux |
| `PhoneCmd "cmd"` | Run a single command in Termux over SSH |
| `DbMenu` | Open the `db-menu` interactive terminal menu |
| `PhoneClipSet "text"` | Set Android clipboard |
| `PhoneClipGet` | Read Android clipboard |
| `StartScrcpy` | Start screen mirror |
| `DbApkDemo ...` | Full APK download/install/evidence run |
| `DbApkInstall ...` | Install APK from phone stable path |

## APK demo workflow

Full run:

```powershell
DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -CleanBeforeRun `
  -ArchivePreviousEvidence
```

This runs:

1. SSH to Termux.
2. `db-apk-artifact-download` writes to `/sdcard/Download/db-control/apks/current.apk`.
3. `adb shell pm install -r` installs from the phone path.
4. `adb shell monkey` launches the app.
5. Optional `adb logcat -c` clears stale logs with `-CleanBeforeRun`.
6. `adb logcat -d -v time` captures bounded logs.
7. `adb shell screencap` captures a screenshot.
8. Evidence is written to `/sdcard/Download/db-control/evidence/latest/`.
9. Optional host mirror is pulled when `-PullEvidenceToWindows` is used.

Termux-only artifact download:

```bash
db-apk-artifact-download \
  --repo indrolend/digital-breakdown-apk \
  --run 28961104004 \
  --artifact digital-breakdown-native-debug-apk \
  --out /sdcard/Download/db-control/apks/current.apk \
  --force
```

Install an already-downloaded APK from the host:

```powershell
DbApkInstall `
  -DevicePath /sdcard/Download/db-control/apks/current.apk `
  -Package com.indrolend.digitalbreakdown.native
```

Skip download if the APK is already on the phone:

```powershell
DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -SkipDownload
```

Evidence-only capture:

```powershell
DbApkDemo `
  -RunId 0 `
  -ArtifactName unused `
  -Package com.indrolend.digitalbreakdown.native `
  -EvidenceOnly `
  -CleanBeforeRun
```

Deprecated compatibility alias: `-SkipInstall` maps to `-EvidenceOnly`.

## Evidence output

Phone source of truth:

```text
/sdcard/Download/db-control/evidence/latest/
  install.txt
  launch.txt
  logcat-full.txt
  logcat-filtered.txt
  logcat-dbnative.txt
  logcat-crashes.txt
  screen.png
  result.json
```

Archived snapshots:

```text
/sdcard/Download/db-control/evidence/archive/YYYYMMDD-HHMMSS/
```

Core `result.json` fields:

```json
{
  "adb": "pass|fail|warning|skipped",
  "ssh": "pass|fail|warning|skipped",
  "ghAuth": "pass|fail|warning|skipped",
  "download": "pass|fail|warning|skipped",
  "install": "pass|fail|warning|skipped",
  "launch": "pass|fail|warning|skipped",
  "evidence": "pass|fail|warning|skipped",
  "dbnativeLogs": "pass|fail|warning|skipped",
  "crashScan": "pass|fail|warning|skipped",
  "screenshot": "pass|fail|warning|skipped",
  "apkPathPhone": "/sdcard/Download/db-control/apks/current.apk",
  "evidencePathPhone": "/sdcard/Download/db-control/evidence/latest",
  "evidencePathHost": "optional host mirror path",
  "package": "com.indrolend.digitalbreakdown.native",
  "runId": "28961104004",
  "artifactName": "digital-breakdown-native-debug-apk",
  "timestamp": "ISO-8601"
}
```

## Terminal menu (`db-menu`)

Run from Termux or via `DbMenu` from the host session:

```text
=====================================
 Digital Breakdown - Phone Menu
=====================================
  1) Status dashboard
  2) APK download/artifact
  3) Install/launch
  4) Evidence/logs
  5) Full demo sequence
  6) GitHub/PR tools
  7) Server/SSH controls
  8) Cleanup/storage
  9) Settings/paths
  q) Quit
```

After each action, the footer supports:

```text
Enter = back to menu
r = repeat last action
l = print latest logs
s = capture screenshot
e = print latest result.json
x = restart sshd
q = quit
```

## Clipboard bridge

With Termux:API:

```bash
echo "hello" | db-clip-set
db-clip-get
```

Without Termux:API, clipboard fallback is stored at:

```text
/sdcard/Download/db-control/.clipboard
```

From the host session:

```powershell
PhoneClipSet "some text"
PhoneClipGet
```

## Cleanup

Phone-side cleanup:

```bash
db-cleanup --dry-run
db-cleanup
```

Rules:

- Never deletes GitHub auth.
- Never deletes broad `/sdcard/Download` contents.
- Deletes only known generated/project-control paths.
- Keeps the newest APK by default.
- Keeps recent evidence archives by default.

Optional host mirror folder defaults to:

```text
~/Downloads/db-control/evidence-latest/
```

on macOS/Linux PowerShell, or the equivalent Downloads path on Windows.

## What lives where

| Item | Location |
|------|----------|
| GitHub auth | Phone / Termux (`~/.config/gh/`) |
| `git`, `gh`, `jq`, etc. | Phone / Termux |
| Project utility scripts | Phone / Termux (`/data/data/com.termux/files/usr/bin/`) |
| APK artifact | Phone (`/sdcard/Download/db-control/apks/current.apk`) |
| Runtime evidence | Phone (`/sdcard/Download/db-control/evidence/latest/`) |
| Evidence mirror | Optional host Downloads path |
| ADB path config | Host user home (`.db-phone-config.ps1`) |

## Recovery paths

### ADB not found

Install platform-tools or set the path manually:

```powershell
$env:DB_ADB_PATH = "/path/to/adb"
```

Windows example:

```powershell
$env:DB_ADB_PATH = "C:\path\to\adb.exe"
```

### Device not authorized

On the phone, accept the USB debugging authorization dialog.

```powershell
adb kill-server
adb start-server
adb devices
```

### Termux user file is a directory

The bootstrap detects and repairs this:

```bash
bash /sdcard/Download/db-control/tools/termux-control-bootstrap.sh
```

### sshd not running

```bash
sshd
```

Or from the menu:

```text
7) Server/SSH controls
```

### UTF-8 BOM warning in Termux

If Termux shows a shebang warning after copy-paste, re-save the script as UTF-8 without BOM and LF line endings. The repo scripts should remain BOM-free.

### scrcpy not found

`phone-session.ps1` searches PATH first. Set manually if needed:

```powershell
Save-PhoneConfig -AdbPath (Find-Adb) -ScrcpyPath "/path/to/scrcpy"
```

### SSH connection refused

1. Ensure `sshd` is running in Termux.
2. Re-run ADB forward:

```powershell
adb forward tcp:8022 tcp:8022
```

3. Confirm username:

```powershell
Get-TermuxUser
```

### gh not authenticated

```bash
gh auth login
```

Choose device/browser authentication as needed.

## File reference

| File | Side | Purpose |
|------|------|---------|
| `phone-session.ps1` | Host | Session launcher, ADB discovery, SSH forward, helper commands |
| `termux-control-bootstrap.sh` | Phone | Install packages, create utility scripts, start sshd |
| `db-apk-artifact-download.sh` | Phone | Standalone artifact downloader |
| `db-apk-demo.ps1` | Host | Full demo orchestrator |
| `db-apk-install.ps1` | Host | Optional host pull + phone-path install helper |
| `db-apk-evidence.ps1` | Host | Bounded logcat + screenshot evidence capture |
| `README.md` | Host/Phone | Workflow reference |
