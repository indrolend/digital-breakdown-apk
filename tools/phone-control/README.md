# Digital Breakdown — Phone-Control Workflow

> Phone-side transparency, minimal Windows setup, traceable utilities.

---

## Quick Start

### One-command session start (Windows)

```powershell
. .\tools\phone-control\phone-session.ps1
```

This will:
1. Find ADB (searches common paths, prompts once if needed, saves config).
2. Check the connected device.
3. Forward SSH port 8022.
4. Detect Termux username.
5. Load helper commands into your PowerShell session.

### Windows PowerShell 5.1 parse check

Before running on a new machine, validate parser compatibility:

```powershell
Get-ChildItem .\tools\phone-control\*.ps1 | ForEach-Object {
  $errors = $null
  $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content $_.FullName -Raw), [ref]$errors)
  if ($errors) {
    Write-Host "Parse failed: $($_.Name)"
    $errors
    exit 1
  }
  Write-Host "Parsed: $($_.Name)"
}
```

### First-time phone bootstrap

Copy and run in Termux (or use `adb push` / `adb shell`):

```bash
bash /sdcard/Download/db-control/tools/termux-control-bootstrap.sh
```

If the file is not on the phone yet, push it from Windows:

```powershell
adb shell mkdir -p /sdcard/Download/db-control/tools
adb push .\tools\phone-control\termux-control-bootstrap.sh /sdcard/Download/db-control/tools/
```

Then run:

```powershell
PhoneCmd "bash /sdcard/Download/db-control/tools/termux-control-bootstrap.sh"
```

---

## New-Machine Setup

### What the Windows machine needs

- **ADB** (Android Debug Bridge) — part of Android Platform Tools.
  - Download: <https://developer.android.com/tools/releases/platform-tools>
  - `phone-session.ps1` searches common paths and saves the location to
    `$env:USERPROFILE\.db-phone-config.ps1` — you only need to provide it once.
- **SSH client** — built into Windows 10/11 (`ssh.exe`).
- **Optional:** `scrcpy` for visual screen mirroring.
  - Download: <https://github.com/Genymobile/scrcpy/releases>
  - `phone-session.ps1` searches common paths automatically.

### What the Windows machine does NOT need

- Repo clone (scripts can be copied individually)
- GitHub auth / `gh` CLI
- Node.js, Gradle, Android Studio
- Long-lived project state

### ADB path persistence

On first run, `phone-session.ps1` prompts for the ADB path and saves it:

```
$env:USERPROFILE\.db-phone-config.ps1
```

Delete that file to reset.

---

## Phone / Termux Setup

### Prerequisites installed by bootstrap

| Tool | Purpose |
|------|---------|
| `git` | Version control |
| `curl` | HTTP requests |
| `jq` | JSON processing |
| `openssh` | SSH server (`sshd`) |
| `nano` | Text editor |
| `termux-api` | Android clipboard bridge |
| `gh` | GitHub CLI (artifact downloads, PR/issue listing) |

### Verify GitHub auth

```bash
gh auth status
# If not authenticated:
gh auth login
```

### Verify SSH is running

```bash
pgrep -x sshd && echo "sshd running" || sshd
```

---

## Helper Commands (loaded by phone-session.ps1)

| Command | Description |
|---------|-------------|
| `OpenTermux` | Open Termux app on phone via ADB intent |
| `Phone` | Open interactive SSH session into Termux |
| `PhoneCmd "cmd"` | Run a single command in Termux over SSH |
| `DbMenu` | Open the `db-menu` interactive terminal menu |
| `PhoneClipSet "text"` | Set Android clipboard |
| `PhoneClipGet` | Read Android clipboard |
| `StartScrcpy` | Start screen mirror |
| `DbApkDemo ...` | Full APK download/install/evidence run |
| `DbApkInstall ...` | Pull + install APK from phone shared storage |

---

## APK Demo Workflow

### Full run (Termux downloads artifact, Windows installs)

```powershell
. .\tools\phone-control\phone-session.ps1

DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -PullEvidenceToWindows `
  -CleanBeforeRun
```

This runs:
1. SSH -> Termux: `db-apk-artifact-download` writes to phone stable APK path.
2. `adb shell pm install -r` installs from phone path.
3. `adb shell monkey` launches the app.
4. `adb logcat -c` (optional via `-CleanBeforeRun`) clears stale logs.
5. `adb logcat -d -v time` captures bounded logs.
6. `adb shell screencap` captures a screenshot to phone evidence path.
7. Evidence written to `/sdcard/Download/db-control/evidence/latest/`.
8. Optional Windows mirror with `-PullEvidenceToWindows` to `Downloads\db-control\evidence-latest\`.

### Termux-only fallback (APK already downloaded)

```bash
db-apk-artifact-download \
  --repo indrolend/digital-breakdown-apk \
  --run 28961104004 \
  --artifact digital-breakdown-native-debug-apk \
  --out /sdcard/Download/db-control/apks/current.apk
```

Then from Windows:

```powershell
DbApkInstall `
  -DevicePath /sdcard/Download/db-control/apks/current.apk `
  -Package com.indrolend.digitalbreakdown.native
```

### Skip download (APK already on phone)

```powershell
DbApkDemo `
  -RunId 28961104004 `
  -ArtifactName digital-breakdown-native-debug-apk `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -SkipDownload
```

### Local APK (no phone download needed)

```powershell
DbApkDemo -RunId 0 -ArtifactName unused `
  -Package com.indrolend.digitalbreakdown.native `
  -LocalApkPath "$env:USERPROFILE\Downloads\current.apk"
```

### Evidence-only (app already installed)

```powershell
DbApkDemo `
  -RunId 0 `
  -ArtifactName unused `
  -Package com.indrolend.digitalbreakdown.native `
  -OutName current.apk `
  -EvidenceOnly
```

Deprecated compatibility alias: `-SkipInstall` maps to the same behavior as `-EvidenceOnly` (broader than the legacy alias name).

### Evidence output structure (phone source of truth)

```
/sdcard/Download/db-control/evidence/latest/
  install.txt           # adb install output
  launch.txt            # monkey launch output
  logcat-full.txt       # Full logcat dump
  logcat-filtered.txt   # DBNATIVE + crash-related lines
  logcat-dbnative.txt   # DBNATIVE lines only (if found)
  logcat-crashes.txt    # Crash/fatal lines (if found)
  screen.png            # Device screenshot
  result.json           # Pass/fail summary
```

Archived snapshots (optional with `-ArchivePreviousEvidence`) are kept at:

```
/sdcard/Download/db-control/evidence/archive/YYYYMMDD-HHMMSS/
```

`result.json` shape:

```json
{
  "adb": "pass|fail|warning|skipped",
  "ssh": "pass|fail|warning|skipped",
  "ghAuth": "pass|fail|warning|skipped",
  "download": "pass|fail|warning|skipped",
  "pull": "pass|fail|warning|skipped",
  "install": "pass|fail|warning|skipped",
  "launch": "pass|fail|warning|skipped",
  "evidence": "pass|fail|warning|skipped",
  "dbnativeLogs": "pass|fail|warning|skipped",
  "crashScan": "pass|fail|warning|skipped",
  "screenshot": "pass|fail|warning|skipped",
  "apkPathPhone": "/sdcard/Download/db-control/apks/current.apk",
  "apkPathWindows": "optional",
  "evidencePathPhone": "/sdcard/Download/db-control/evidence/latest",
  "evidencePathWindows": "optional",
  "package": "com.indrolend.digitalbreakdown.native",
  "runId": "28961104004",
  "artifactName": "digital-breakdown-native-debug-apk",
  "timestamp": "ISO-8601",
  "logcatCleared": "pass|fail|warning|skipped",
  "errorSummary": []
}
```

---

## Terminal Menu (db-menu)

Run from Termux or via `DbMenu` from Windows:

```
=====================================
 Digital Breakdown - Phone Menu
=====================================
  1) Status overview
  2) APK / Demo Tools
  3) Repo / GitHub
  4) Clipboard
  5) Cleanup / Waste management
  6) Start / restart sshd
  7) gh auth login
  q) Quit
```

APK / Demo Tools submenu:

```
=== APK / Demo Tools ===
  1) Download latest native debug artifact from PR/run
  2) List APKs in /sdcard/Download/db-control/apks
  3) Launch native package  (Windows-side)
  4) Capture screenshot     (Windows-side)
  5) Capture logcat evidence (Windows-side)
  6) Full smoke test: download -> (pull/install on Windows)
  7) Clean old APK/evidence files (dry-run)
  8) Clean old APK/evidence files (confirm)
  b) Back
```

---

## Clipboard Bridge

### With Termux:API installed (real Android clipboard)

```bash
echo "hello" | db-clip-set
db-clip-get
```

### Without Termux:API (file fallback)

Clipboard is saved to `/sdcard/Download/db-control/.clipboard`.

### From Windows

```powershell
PhoneClipSet "some text"    # Sends text to phone clipboard
PhoneClipGet                # Reads phone clipboard
```

---

## Waste Management

### Phone-side cleanup

```bash
db-cleanup --dry-run    # Preview what would be deleted
db-cleanup              # Interactive cleanup with confirmation per item
```

Or from the menu: `5) Cleanup / Waste management`.

**Rules:**
- Never deletes GitHub auth.
- Never deletes broad `/sdcard/Download` contents.
- Only deletes known paths:
  - `/sdcard/Download/db-control/.clipboard`
  - Old APKs in `/sdcard/Download/db-control/apks/` (keeps newest)
  - Stale bootstrap copies in `/sdcard/Download/`

### Windows-side cleanup

Optional mirror folder is `Downloads\db-control\evidence-latest\`.
Default flow keeps evidence authoritative on phone.

---

## What Lives Where

| Item | Location |
|------|----------|
| GitHub auth | Phone / Termux (`~/.config/gh/`) |
| `git`, `gh`, `jq`, etc. | Phone / Termux |
| Project utility scripts | Phone / Termux (`/data/data/com.termux/files/usr/bin/`) |
| APK artifacts | Phone (`/sdcard/Download/db-control/apks/current.apk`) |
| Temp/state files (excluding `apks/` and `evidence/`) | Phone (`/sdcard/Download/db-control/`) |
| ADB path config | Windows (`$env:USERPROFILE\.db-phone-config.ps1`) |
| Evidence/logs (authoritative) | Phone (`/sdcard/Download/db-control/evidence/latest/`) |
| Evidence mirror (optional) | Windows (`$env:USERPROFILE\Downloads\db-control\evidence-latest\`) |

---

## Known Recovery Paths

### ADB not found

`phone-session.ps1` will prompt for the path. Or set manually:

```powershell
$env:DB_ADB_PATH = "C:\path\to\adb.exe"
```

### Device not authorized

On the phone: check for a USB debugging authorization dialog.

```powershell
adb kill-server
adb start-server
adb devices   # Accept prompt on phone
```

### Termux user file is a directory

The bootstrap script detects and repairs this:

```bash
bash /sdcard/Download/db-control/tools/termux-control-bootstrap.sh
```

### sshd not running

```bash
sshd
```

Or from the menu: `6) Start / restart sshd`.

### UTF-8 BOM warning in Termux

> `line 1: #!/data/data/com.termux/files/usr/bin/bash: No such file or directory`

This is a BOM artifact. The scripts in this repo are saved without BOM (UTF-8 without BOM, LF line endings). If you copy-paste a script and see this warning, re-save without BOM.

### scrcpy not found

`phone-session.ps1` searches common paths. Set manually:

```powershell
Save-PhoneConfig -AdbPath (Find-Adb) -ScrcpyPath "C:\path\to\scrcpy.exe"
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

Choose browser authentication if no browser is available on the phone: use device flow.

---

## File Reference

| File | Side | Purpose |
|------|------|---------|
| `phone-session.ps1` | Windows | Session launcher, ADB discovery, SSH forward, helper commands |
| `termux-control-bootstrap.sh` | Phone | Install packages, create utility scripts, start sshd |
| `db-apk-artifact-download.sh` | Phone | Standalone artifact downloader (also installed as `db-apk-artifact-download`) |
| `db-apk-demo.ps1` | Windows | Full demo orchestrator |
| `db-apk-install.ps1` | Windows | Pull + install APK helper |
| `db-apk-evidence.ps1` | Windows | Logcat + screenshot evidence capture |
| `README.md` | — | This document |
