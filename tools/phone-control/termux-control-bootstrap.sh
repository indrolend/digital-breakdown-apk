#!/data/data/com.termux/files/usr/bin/bash
# termux-control-bootstrap.sh
# Phone-side setup for Digital Breakdown phone-control workflow.
# Idempotent: safe to run multiple times.
# No UTF-8 BOM, LF line endings only.

set -euo pipefail

DB_HOME="/sdcard/Download/db-control"
TERMUX_BIN="/data/data/com.termux/files/usr/bin"
SDCARD_DOWNLOAD="/sdcard/Download"
TERMUX_USER_FILE="$DB_HOME/termux-user.txt"
APK_DIR="$DB_HOME/apks"
EVIDENCE_DIR="$DB_HOME/evidence"
TOOLS_DIR="$DB_HOME/tools"

log() { echo "[bootstrap] $*"; }
ok()  { echo "[ok] $*"; }
warn(){ echo "[warn] $*"; }
fail(){ echo "[fail] $*"; return 1; }

check_storage() {
    log "Checking shared storage access ..."
    if [ ! -d "$SDCARD_DOWNLOAD" ]; then
        warn "/sdcard/Download not accessible."
        warn "Grant storage permission in Termux with:"
        warn "  termux-setup-storage"
        warn "Then re-run this script."
        return 1
    fi
    ok "Shared storage accessible."
}

repair_user_file() {
    log "Checking $TERMUX_USER_FILE ..."
    if [ -d "$TERMUX_USER_FILE" ]; then
        warn "termux-user.txt is a directory; removing and recreating as a file."
        rm -rf "$TERMUX_USER_FILE"
    fi
    whoami > "$TERMUX_USER_FILE"
    ok "Wrote Termux username '$(cat "$TERMUX_USER_FILE")' to $TERMUX_USER_FILE"
}

install_packages() {
    log "Updating package index ..."
    pkg update -y 2>/dev/null || warn "pkg update had non-zero exit (may be harmless)."

    local check_cmd
    declare -A pkg_cmd=(
        [git]=git
        [curl]=curl
        [jq]=jq
        [openssh]=sshd
        [nano]=nano
        [termux-api]=termux-clipboard-get
    )
    for pkg in git curl jq openssh nano termux-api; do
        check_cmd="${pkg_cmd[$pkg]}"
        if command -v "$check_cmd" >/dev/null 2>&1; then
            ok "$pkg already installed."
        else
            log "Installing $pkg ..."
            pkg install -y "$pkg" || warn "Failed to install $pkg; you may need to install it manually."
        fi
    done

    if command -v gh >/dev/null 2>&1; then
        ok "gh (GitHub CLI) already installed."
    else
        log "Installing gh (GitHub CLI) ..."
        pkg install -y gh 2>/dev/null || {
            warn "pkg gh not available directly."
            warn "Try: pkg install gh  (after: pkg update)"
            warn "Or install manually from GitHub CLI releases."
        }
    fi
}

ensure_sshd() {
    log "Checking sshd ..."
    if pgrep -x sshd >/dev/null 2>&1; then
        ok "sshd is already running."
    else
        log "Starting sshd ..."
        sshd && ok "sshd started on port 8022." || {
            warn "sshd failed to start."
            warn "Ensure openssh is installed: pkg install openssh"
            warn "Then run: sshd"
        }
    fi
}

ensure_db_home() {
    mkdir -p "$DB_HOME" "$APK_DIR" "$EVIDENCE_DIR/latest" "$EVIDENCE_DIR/archive" "$TOOLS_DIR"
    ok "Directories ready: $DB_HOME  $APK_DIR  $EVIDENCE_DIR  $TOOLS_DIR"
}

persist_bootstrap_copy() {
    local target="$TOOLS_DIR/termux-control-bootstrap.sh"
    cp "$0" "$target" 2>/dev/null || true
    chmod +x "$target" 2>/dev/null || true
    ok "Bootstrap copy ready at $target"
}

write_db_menu() {
    local target="$TERMUX_BIN/db-menu"
    cat > "$target" << 'MENU_EOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-menu - Digital Breakdown terminal menu

set +e

DB_HOME="/sdcard/Download/db-control"
SDCARD_DOWNLOAD="/sdcard/Download"
APK_DIR="$DB_HOME/apks"
EVIDENCE_LATEST="$DB_HOME/evidence/latest"
PACKAGE="com.indrolend.digitalbreakdown.native"
REPO="indrolend/digital-breakdown-apk"
DEFAULT_ARTIFACT="digital-breakdown-native-debug-apk"
DEFAULT_RUN="28961104004"
LAST_ACTION=""

pause_footer() {
    echo ""
    echo "Enter = back to menu"
    echo "r = repeat last action"
    echo "l = print latest logs"
    echo "s = capture screenshot"
    echo "e = print latest result.json"
    echo "x = restart sshd"
    echo "q = quit"
    read -r -p "Choose: " footer_choice
    case "$footer_choice" in
        r|R) if [ -n "$LAST_ACTION" ]; then "$LAST_ACTION"; pause_footer; fi ;;
        l|L) print_latest_logs; pause_footer ;;
        s|S) capture_screenshot_hint; pause_footer ;;
        e|E) print_result_json; pause_footer ;;
        x|X) restart_sshd; pause_footer ;;
        q|Q) exit 0 ;;
        *) return 0 ;;
    esac
}

run_action() {
    LAST_ACTION="$1"
    "$1"
    pause_footer
}

print_result_json() {
    echo ""
    echo "=== Latest result.json ==="
    if [ -f "$EVIDENCE_LATEST/result.json" ]; then
        if command -v jq >/dev/null 2>&1; then
            jq . "$EVIDENCE_LATEST/result.json" || cat "$EVIDENCE_LATEST/result.json"
        else
            cat "$EVIDENCE_LATEST/result.json"
        fi
    else
        echo "No result.json at $EVIDENCE_LATEST/result.json"
    fi
}

print_latest_logs() {
    echo ""
    echo "=== Latest DBNATIVE logs ==="
    if [ -f "$EVIDENCE_LATEST/logcat-dbnative.txt" ]; then
        tail -60 "$EVIDENCE_LATEST/logcat-dbnative.txt"
    else
        echo "No logcat-dbnative.txt found."
    fi
    echo ""
    echo "=== Latest crash scan ==="
    if [ -f "$EVIDENCE_LATEST/logcat-crashes.txt" ]; then
        tail -100 "$EVIDENCE_LATEST/logcat-crashes.txt"
    else
        echo "No logcat-crashes.txt found."
    fi
}

capture_screenshot_hint() {
    echo ""
    echo "=== Screenshot ==="
    echo "Screenshots are captured over ADB from the host side:"
    echo "  adb shell screencap -p $EVIDENCE_LATEST/screen.png"
    echo "Or run the host PowerShell evidence/demo scripts."
}

restart_sshd() {
    echo ""
    echo "=== Restart sshd ==="
    pkill sshd >/dev/null 2>&1 || true
    sshd && echo "[ok] sshd started on port 8022" || echo "[warn] sshd failed"
}

show_status() {
    echo ""
    echo "=== Status Dashboard ==="
    echo "Phone host: $(hostname)"
    echo "Termux user: $(whoami)"
    echo "Date: $(date)"
    echo "DB_HOME: $DB_HOME"
    echo "APK: $APK_DIR/current.apk"
    echo "Evidence: $EVIDENCE_LATEST"
    echo ""

    echo "--- Termux packages ---"
    for cmd in git curl jq gh ssh sshd nano termux-clipboard-get; do
        if command -v "$cmd" >/dev/null 2>&1; then
            echo "  [ok] $cmd"
        else
            echo "  [--] $cmd"
        fi
    done

    echo ""
    echo "--- SSH daemon ---"
    if pgrep -x sshd >/dev/null 2>&1; then
        echo "  [ok] sshd running (port 8022)"
    else
        echo "  [--] sshd not running"
    fi

    echo ""
    echo "--- GitHub auth ---"
    if command -v gh >/dev/null 2>&1; then
        gh auth status >/dev/null 2>&1 && echo "  [ok] gh auth active" || echo "  [--] gh not authenticated"
    else
        echo "  [--] gh not installed"
    fi

    echo ""
    echo "--- Shared storage ---"
    [ -d "$SDCARD_DOWNLOAD" ] && echo "  [ok] /sdcard/Download accessible" || echo "  [--] /sdcard/Download not accessible"

    echo ""
    echo "--- APK files ---"
    ls -lh "$APK_DIR" 2>/dev/null || echo "  (none)"

    echo ""
    echo "--- Latest result ---"
    [ -f "$EVIDENCE_LATEST/result.json" ] && cat "$EVIDENCE_LATEST/result.json" || echo "  (no result.json yet)"
}

apk_download() {
    echo ""
    echo "=== APK Download / Artifact ==="
    read -r -p "GitHub run ID [$DEFAULT_RUN]: " run_id
    run_id="${run_id:-$DEFAULT_RUN}"
    read -r -p "Artifact name [$DEFAULT_ARTIFACT]: " art_name
    art_name="${art_name:-$DEFAULT_ARTIFACT}"
    read -r -p "Output path [$APK_DIR/current.apk]: " out_path
    out_path="${out_path:-$APK_DIR/current.apk}"
    db-apk-artifact-download --repo "$REPO" --run "$run_id" --artifact "$art_name" --out "$out_path" --force
}

install_launch_hint() {
    echo ""
    echo "=== Install / Launch ==="
    echo "Install and launch run from the host side through ADB."
    echo "From Windows PowerShell or Mac/Linux pwsh:"
    echo "  . ./tools/phone-control/phone-session.ps1"
    echo "  DbApkInstall -DevicePath $APK_DIR/current.apk -Package $PACKAGE"
    echo ""
    echo "Full scripted run:"
    echo "  DbApkDemo -RunId $DEFAULT_RUN -ArtifactName $DEFAULT_ARTIFACT -Package $PACKAGE -OutName current.apk -CleanBeforeRun"
}

evidence_hint() {
    echo ""
    echo "=== Evidence / Logs ==="
    print_result_json
    print_latest_logs
    echo ""
    echo "To recapture bounded evidence from the host side:"
    echo "  . ./tools/phone-control/phone-session.ps1"
    echo "  DbApkDemo -RunId 0 -ArtifactName unused -Package $PACKAGE -EvidenceOnly -CleanBeforeRun"
}

full_demo_hint() {
    echo ""
    echo "=== Full Demo Sequence ==="
    echo "This sequence requires host ADB for install, launch, logcat, and screenshot."
    echo "From Windows PowerShell or Mac/Linux pwsh:"
    echo "  . ./tools/phone-control/phone-session.ps1"
    echo "  DbApkDemo -RunId $DEFAULT_RUN -ArtifactName $DEFAULT_ARTIFACT -Package $PACKAGE -OutName current.apk -CleanBeforeRun -ArchivePreviousEvidence"
    echo ""
    echo "Termux can perform the artifact download step now:"
    echo "  db-apk-artifact-download --repo $REPO --run $DEFAULT_RUN --artifact $DEFAULT_ARTIFACT --out $APK_DIR/current.apk --force"
}

github_tools() {
    echo ""
    echo "=== GitHub / PR Tools ==="
    echo "1) gh auth status"
    echo "2) gh auth login"
    echo "3) List open PRs"
    echo "4) List recent workflow runs"
    echo "5) View PR #5"
    read -r -p "Choose: " g_choice
    case "$g_choice" in
        1) gh auth status || true ;;
        2) gh auth login || true ;;
        3) gh pr list --repo "$REPO" || true ;;
        4) gh run list --repo "$REPO" --limit 10 || true ;;
        5) gh pr view 5 --repo "$REPO" || true ;;
        *) echo "Unknown choice." ;;
    esac
}

cleanup_storage() {
    echo ""
    echo "=== Cleanup / Storage ==="
    echo "1) Dry-run cleanup"
    echo "2) Run cleanup with prompts"
    echo "3) Show db-control size"
    read -r -p "Choose: " c_choice
    case "$c_choice" in
        1) db-cleanup --dry-run || true ;;
        2) db-cleanup || true ;;
        3) du -sh "$DB_HOME" 2>/dev/null || true; find "$DB_HOME" -maxdepth 3 -type f -printf "%s %p\n" 2>/dev/null | sort -nr | head -20 || true ;;
        *) echo "Unknown choice." ;;
    esac
}

settings_paths() {
    echo ""
    echo "=== Settings / Paths ==="
    echo "Repo: $REPO"
    echo "Package: $PACKAGE"
    echo "DB_HOME: $DB_HOME"
    echo "APK_DIR: $APK_DIR"
    echo "Current APK: $APK_DIR/current.apk"
    echo "Evidence latest: $EVIDENCE_LATEST"
    echo "Evidence archive: $DB_HOME/evidence/archive"
    echo "Tools: $DB_HOME/tools"
    echo "Termux bin: /data/data/com.termux/files/usr/bin"
    echo "Termux user file: $DB_HOME/termux-user.txt"
}

while true; do
    echo ""
    echo "====================================="
    echo " Digital Breakdown - Phone Menu"
    echo "====================================="
    echo "  1) Status dashboard"
    echo "  2) APK download/artifact"
    echo "  3) Install/launch"
    echo "  4) Evidence/logs"
    echo "  5) Full demo sequence"
    echo "  6) GitHub/PR tools"
    echo "  7) Server/SSH controls"
    echo "  8) Cleanup/storage"
    echo "  9) Settings/paths"
    echo "  q) Quit"
    echo ""
    read -r -p "Choose: " choice
    case "$choice" in
        1) run_action show_status ;;
        2) run_action apk_download ;;
        3) run_action install_launch_hint ;;
        4) run_action evidence_hint ;;
        5) run_action full_demo_hint ;;
        6) run_action github_tools ;;
        7) run_action restart_sshd ;;
        8) run_action cleanup_storage ;;
        9) run_action settings_paths ;;
        q|Q) echo "Bye."; exit 0 ;;
        *) echo "Unknown choice." ;;
    esac
done
MENU_EOF
    chmod +x "$target"
    ok "db-menu installed at $target"
}

write_db_apk_download() {
    local target="$TERMUX_BIN/db-apk-artifact-download"
    cat > "$target" << 'DLEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-apk-artifact-download - Download a GitHub Actions artifact via gh CLI.
# Usage: db-apk-artifact-download --repo REPO --run RUN_ID --artifact NAME [--out PATH] [--force]

set -euo pipefail

REPO="indrolend/digital-breakdown-apk"
RUN_ID=""
ARTIFACT_NAME=""
OUT_PATH="/sdcard/Download/db-control/apks/current.apk"
FORCE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo)      REPO="$2";          shift 2 ;;
        --run)       RUN_ID="$2";        shift 2 ;;
        --artifact)  ARTIFACT_NAME="$2"; shift 2 ;;
        --out)       OUT_PATH="$2";      shift 2 ;;
        --force)     FORCE=1;            shift 1 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

if [[ -z "$RUN_ID" || -z "$ARTIFACT_NAME" ]]; then
    echo "Usage: db-apk-artifact-download --repo REPO --run RUN_ID --artifact NAME [--out PATH] [--force]"
    exit 1
fi

if ! command -v gh >/dev/null 2>&1; then
    echo "[fail] gh not installed. Run: pkg install gh"
    exit 1
fi

echo "[download] Checking gh auth ..."
if ! gh auth status >/dev/null 2>&1; then
    echo "[fail] gh not authenticated. Run: gh auth login"
    exit 1
fi

OUT_DIR="$(dirname "$OUT_PATH")"
mkdir -p "$OUT_DIR"

if [[ -f "$OUT_PATH" && "$FORCE" -ne 1 ]]; then
    read -r -p "APK exists at $OUT_PATH. Overwrite? [y/N] " overwrite
    if [[ ! "$overwrite" =~ ^[Yy]$ ]]; then
        echo "[ok] Keeping existing APK."
        exit 0
    fi
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[download] Fetching artifact '$ARTIFACT_NAME' from run $RUN_ID ..."
gh run download "$RUN_ID" --repo "$REPO" --name "$ARTIFACT_NAME" --dir "$TMP_DIR"

APK_FILE="$(find "$TMP_DIR" -name '*.apk' | head -1)"
if [[ -z "$APK_FILE" ]]; then
    echo "[fail] No APK found in downloaded artifact."
    ls -la "$TMP_DIR"
    exit 1
fi

echo "[download] Found APK: $APK_FILE"
cp "$APK_FILE" "$OUT_PATH"
echo "[ok] APK saved to: $OUT_PATH"
ls -lh "$OUT_PATH"
DLEOF
    chmod +x "$target"
    ok "db-apk-artifact-download installed at $target"
}

write_db_gh_settings() {
    local target="$TERMUX_BIN/db-gh-settings"
    cat > "$target" << 'GHEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-gh-settings - Check and configure GitHub CLI auth.
echo "=== GitHub CLI Status ==="
if command -v gh >/dev/null 2>&1; then
    gh auth status || echo "Not authenticated. Run: gh auth login"
else
    echo "gh not installed. Run: pkg install gh"
fi
GHEOF
    chmod +x "$target"
    ok "db-gh-settings installed at $target"
}

write_db_workflows() {
    local target="$TERMUX_BIN/db-workflows"
    cat > "$target" << 'WFEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-workflows - List recent GitHub Actions workflow runs.
set -euo pipefail
REPO="${1:-indrolend/digital-breakdown-apk}"
echo "=== Recent workflow runs for $REPO ==="
gh run list --repo "$REPO" --limit 15 || echo "gh not authenticated or not installed."
WFEOF
    chmod +x "$target"
    ok "db-workflows installed at $target"
}

write_db_clip() {
    local set_target="$TERMUX_BIN/db-clip-set"
    cat > "$set_target" << 'CLIPSET'
#!/data/data/com.termux/files/usr/bin/bash
# db-clip-set - Set Android clipboard. Reads from stdin or first argument.
CLIP_FILE="/sdcard/Download/db-control/.clipboard"
mkdir -p "$(dirname "$CLIP_FILE")"

if [[ $# -gt 0 ]]; then
    TEXT="$*"
else
    TEXT="$(cat)"
fi

if command -v termux-clipboard-set >/dev/null 2>&1; then
    echo "$TEXT" | termux-clipboard-set
    echo "[clip] Set via termux-api."
else
    echo "$TEXT" > "$CLIP_FILE"
    echo "[clip] termux-api unavailable; saved to $CLIP_FILE"
fi
CLIPSET
    chmod +x "$set_target"
    ok "db-clip-set installed at $set_target"

    local get_target="$TERMUX_BIN/db-clip-get"
    cat > "$get_target" << 'CLIPGET'
#!/data/data/com.termux/files/usr/bin/bash
# db-clip-get - Get Android clipboard.
CLIP_FILE="/sdcard/Download/db-control/.clipboard"

if command -v termux-clipboard-get >/dev/null 2>&1; then
    termux-clipboard-get
else
    if [[ -f "$CLIP_FILE" ]]; then
        cat "$CLIP_FILE"
    else
        echo "[clip] No clipboard data found."
    fi
fi
CLIPGET
    chmod +x "$get_target"
    ok "db-clip-get installed at $get_target"
}

write_db_cleanup() {
    local target="$TERMUX_BIN/db-cleanup"
    cat > "$target" << 'CLEANEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-cleanup - Cleanup generated project-control files.
# Usage: db-cleanup [--dry-run]

set -euo pipefail

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" ]]; then DRY_RUN=1; fi

DB_CONTROL="/sdcard/Download/db-control"
APK_DIR="$DB_CONTROL/apks"
EVIDENCE_ARCHIVE="$DB_CONTROL/evidence/archive"
CLIP_FILE="$DB_CONTROL/.clipboard"

log_would() { echo "[would delete] $*"; }
log_deleted() { echo "[deleted] $*"; }
log_skip() { echo "[skip] $*"; }

safe_remove() {
    local path="$1"
    case "$path" in
        /sdcard/Download/db-control/*|/sdcard/Download/termux-control-bootstrap*.sh) ;;
        *) echo "[skip] unsafe path: $path"; return 0 ;;
    esac
    if [[ -e "$path" || -d "$path" ]]; then
        if [[ $DRY_RUN -eq 1 ]]; then
            log_would "$path"
        else
            read -r -p "Delete $path? [y/N] " confirm
            if [[ "$confirm" =~ ^[Yy]$ ]]; then
                rm -rf "$path"
                log_deleted "$path"
            else
                log_skip "$path"
            fi
        fi
    fi
}

echo "=== Cleanup Preview ==="
[[ $DRY_RUN -eq 1 ]] && echo "(dry-run; nothing will be deleted)"

echo ""
echo "--- Clipboard temp file ---"
safe_remove "$CLIP_FILE"

echo ""
echo "--- APK artifacts in $APK_DIR ---"
if [[ -d "$APK_DIR" ]]; then
    mapfile -t apks < <(ls -t "$APK_DIR"/*.apk 2>/dev/null || true)
    if [[ ${#apks[@]} -gt 1 ]]; then
        echo "Newest: ${apks[0]} (keeping)"
        for apk in "${apks[@]:1}"; do
            safe_remove "$apk"
        done
    else
        echo "(0 or 1 APK present; nothing to remove)"
    fi
fi

echo ""
echo "--- Evidence archives in $EVIDENCE_ARCHIVE ---"
if [[ -d "$EVIDENCE_ARCHIVE" ]]; then
    mapfile -t archives < <(ls -1dt "$EVIDENCE_ARCHIVE"/* 2>/dev/null || true)
    if [[ ${#archives[@]} -gt 10 ]]; then
        echo "Keeping newest 10 archives."
        for archive in "${archives[@]:10}"; do
            safe_remove "$archive"
        done
    else
        echo "(10 or fewer archives present; nothing to remove)"
    fi
fi

echo ""
echo "--- Stale bootstrap copies in /sdcard/Download ---"
for f in /sdcard/Download/termux-control-bootstrap*.sh; do
    [[ -e "$f" ]] && safe_remove "$f"
done

echo ""
echo "=== Cleanup done. ==="
CLEANEOF
    chmod +x "$target"
    ok "db-cleanup installed at $target"
}

write_db_pr2() {
    local target="$TERMUX_BIN/db-pr2"
    cat > "$target" << 'PR2EOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-pr2 - Quick helper for native debug APK smoke test.
# Downloads a native debug artifact and places it at /sdcard/Download/db-control/apks/current.apk.

set -euo pipefail

REPO="indrolend/digital-breakdown-apk"
ARTIFACT="digital-breakdown-native-debug-apk"
OUT="/sdcard/Download/db-control/apks/current.apk"

echo "=== Native debug APK download ==="

if ! command -v gh >/dev/null 2>&1; then
    echo "[fail] gh not installed. Run: pkg install gh"
    exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
    echo "[fail] gh not authenticated. Run: gh auth login"
    exit 1
fi

echo "Listing recent runs ..."
gh run list --repo "$REPO" --limit 5

read -r -p "Enter run ID to download from: " RUN_ID

if [[ -z "$RUN_ID" ]]; then
    echo "Cancelled."
    exit 0
fi

db-apk-artifact-download --repo "$REPO" --run "$RUN_ID" --artifact "$ARTIFACT" --out "$OUT"

echo ""
echo "APK ready at: $OUT"
echo ""
echo "From the host PowerShell session, run:"
echo "  . ./tools/phone-control/phone-session.ps1"
echo "  DbApkInstall -DevicePath /sdcard/Download/db-control/apks/current.apk -Package com.indrolend.digitalbreakdown.native"
PR2EOF
    chmod +x "$target"
    ok "db-pr2 installed at $target"
}

log "Starting Digital Breakdown Termux bootstrap ..."
log "Script: $0"
log "User:   $(whoami)"
log "Home:   $HOME"

check_storage
ensure_db_home
persist_bootstrap_copy
repair_user_file
install_packages
ensure_sshd
write_db_menu
write_db_apk_download
write_db_gh_settings
write_db_workflows
write_db_clip
write_db_cleanup
write_db_pr2

echo ""
echo "=== Bootstrap complete ==="
echo ""
echo "Installed utilities:"
echo "  db-menu                    - Main terminal menu"
echo "  db-apk-artifact-download   - Download GitHub Actions artifact"
echo "  db-pr2                     - Quick smoke test helper"
echo "  db-gh-settings             - GitHub auth status"
echo "  db-workflows               - List recent workflow runs"
echo "  db-clip-set / db-clip-get  - Clipboard bridge"
echo "  db-cleanup                 - Clean generated files"
echo ""
echo "Next steps:"
echo "  1. Authenticate GitHub CLI:  gh auth login"
echo "  2. Start SSH daemon:         sshd  (already started above)"
echo "  3. From host PowerShell:     . ./tools/phone-control/phone-session.ps1"
echo "  4. Open menu:                DbMenu   (or: db-menu)"
echo ""
echo "Termux user: $(whoami)"
echo "SSH port:    8022"
echo "ADB forward: adb forward tcp:8022 tcp:8022"