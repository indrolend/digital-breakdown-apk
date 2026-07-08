# Phone control workflow

This directory captures the current cross-device control workflow for using an Android phone running Termux as the persistent project control box.

## Goal

Keep per-machine setup minimal while keeping project control transparent and traceable.

The desired workflow is:

```text
any Windows computer with USB access
  -> adb detects phone
  -> PowerShell launcher opens Termux
  -> adb forwards localhost:8022 to Termux sshd
  -> SSH controls Termux directly
  -> Termux holds GitHub auth, repo utilities, clipboard bridge, and project menus
```

The computer should not need a repository clone, GitHub auth, Node, Gradle, Android Studio, or long-lived project state just to manage GitHub and device-side utilities.

## Current pieces

- `phone-session.ps1`
  - Windows-side session launcher.
  - Finds local `adb.exe`.
  - Verifies the phone is authorized.
  - Opens Termux.
  - Forwards SSH over ADB.
  - Reads or repairs `/sdcard/Download/termux-user.txt`.
  - Loads helper functions:
    - `OpenTermux`
    - `Phone`
    - `PhoneCmd`
    - `DbMenu`
    - `PhoneClipSet`
    - `PhoneClipGet`
    - `StartScrcpy`

- `termux-control-bootstrap.sh`
  - Phone-side Termux bootstrap.
  - Installs `gh`, `git`, `curl`, `jq`, `openssh`, `nano`, and `termux-api`.
  - Creates `~/bin` utilities:
    - `db-menu`
    - `db-gh-settings`
    - `db-pr2`
    - `db-workflows`
    - `db-clip-set`
    - `db-clip-get`
  - Starts `sshd`.
  - Writes the Termux username to `/sdcard/Download/termux-user.txt` for future automated sessions.

## Current operating model

1. Run the Termux bootstrap once on the phone.
2. Start each future work session with:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
& "$env:USERPROFILE\Downloads\phone-session.ps1"
```

3. Use the loaded PowerShell functions:

```powershell
PhoneCmd whoami
Phone
DbMenu
PhoneClipSet
PhoneClipGet
```

## Recent workflow facts

The working device was detected over ADB as `LMQ710MS668bc658`.

The Termux user was `u0_a234`.

SSH over ADB forwarding worked with:

```powershell
adb forward tcp:8022 tcp:8022
ssh -p 8022 u0_a234@127.0.0.1
```

The bootstrap completed after a Windows UTF-8 BOM warning on line 1. The warning was non-fatal, but future script generation should avoid BOM output.

One bad state was observed: `/sdcard/Download/termux-user.txt` existed as a directory instead of a file. The hardened launcher and bootstrap should remove that directory before writing the username file.

## Desired hardening

- Avoid BOM and CRLF issues across PowerShell -> Android -> Termux.
- Avoid fragile `adb shell input text` for long commands.
- Make setup idempotent.
- Detect whether Termux has storage permission.
- Detect whether `sshd` is running.
- Detect whether Termux:API Android app is installed before assuming clipboard works.
- Support a no-scrcpy mode and optional scrcpy path discovery.
- Provide a simple terminal UI for utilities, file browsing, logs, script status, GitHub controls, and device connection status.
- Keep generated logs and temporary files organized and disposable.

## Waste management principle

The phone should keep only useful persistent state:

- GitHub auth
- small control scripts
- small logs/status files
- current APK/test artifacts when needed

Generated scratch files, stale copied scripts, old logs, and duplicate artifacts should be easy to inspect and delete from the terminal UI.
