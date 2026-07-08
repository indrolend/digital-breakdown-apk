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

# ---------------------------------------------------------------------------
# Storage access check
# ---------------------------------------------------------------------------

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

# ---------------------------------------------------------------------------
# Repair termux-user.txt (may have been created as a directory)
# ---------------------------------------------------------------------------

repair_user_file() {
    log "Checking $TERMUX_USER_FILE ..."
    if [ -d "$TERMUX_USER_FILE" ]; then
        warn "termux-user.txt is a directory — removing and recreating as a file."
        rm -rf "$TERMUX_USER_FILE"
    fi
    # Write current username
    whoami > "$TERMUX_USER_FILE"
    ok "Wrote Termux username '$(cat "$TERMUX_USER_FILE")' to $TERMUX_USER_FILE"
}

# ---------------------------------------------------------------------------
# Package installation
# ---------------------------------------------------------------------------

install_packages() {
    log "Updating package index ..."
    pkg update -y 2>/dev/null || warn "pkg update had non-zero exit (may be harmless)."

    # Map package name -> representative command to check for installation.
    # termux-api provides termux-clipboard-get (not a binary named termux-api).
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
            pkg install -y "$pkg" || warn "Failed to install $pkg — you may need to install it manually."
        fi
    done

    # gh (GitHub CLI) requires separate repo
    if command -v gh >/dev/null 2>&1; then
        ok "gh (GitHub CLI) already installed."
    else
        log "Installing gh (GitHub CLI) ..."
        pkg install -y gh 2>/dev/null || {
            warn "pkg gh not available directly."
            warn "Try: pkg install gh  (after: pkg update)"
            warn "Or install manually: https://github.com/cli/cli/releases"
        }
    fi
}

# ---------------------------------------------------------------------------
# sshd
# ---------------------------------------------------------------------------

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

# ---------------------------------------------------------------------------
# db-control directory
# ---------------------------------------------------------------------------

ensure_db_home() {
    mkdir -p "$DB_HOME"
    mkdir -p "$APK_DIR"
    mkdir -p "$EVIDENCE_DIR/latest"
    mkdir -p "$EVIDENCE_DIR/archive"
    mkdir -p "$TOOLS_DIR"
    ok "Directories ready: $DB_HOME  $APK_DIR  $EVIDENCE_DIR  $TOOLS_DIR"
}

persist_bootstrap_copy() {
    local target="$TOOLS_DIR/termux-control-bootstrap.sh"
    cp "$0" "$target" 2>/dev/null || true
    chmod +x "$target" 2>/dev/null || true
    ok "Bootstrap copy ready at $target"
}

# ---------------------------------------------------------------------------
# Utility script: db-menu
# ---------------------------------------------------------------------------

write_db_menu() {
    local target="$TERMUX_BIN/db-menu"
    cat > "$target" << 'MENU_EOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-menu — Digital Breakdown terminal menu

DB_HOME="/sdcard/Download/db-control"
SDCARD_DOWNLOAD="/sdcard/Download"
APK_DIR="$DB_HOME/apks"

show_status() {
    echo ""
    echo "=== Status ==="
    echo "Host:    $(hostname)"
    echo "User:    $(whoami)"
    echo "Date:    $(date)"
    echo ""

    # ADB (from phone perspective)
    echo "--- Termux packages ---"
    for cmd in git curl jq gh ssh nano termux-clipboard-get; do
        if command -v "$cmd" >/dev/null 2>&1; then
            echo "  [ok] $cmd"
        else
            echo "  [--] $cmd (not installed)"
        fi
    done

    echo ""
    echo "--- SSH daemon ---"
    if pgrep -x sshd >/dev/null 2>&1; then
        echo "  [ok] sshd running (port 8022)"
    else
        echo "  [--] sshd not running  ->  run: sshd"
    fi

    echo ""
    echo "--- GitHub auth ---"
    if command -v gh >/dev/null 2>&1; then
        if gh auth status >/dev/null 2>&1; then
            echo "  [ok] gh auth active"
        else
            echo "  [--] gh not authenticated  ->  run: gh auth login"
        fi
    else
        echo "  [--] gh not installed  ->  pkg install gh"
    fi

    echo ""
    echo "--- Shared storage ---"
    if [ -d "$SDCARD_DOWNLOAD" ]; then
        echo "  [ok] /sdcard/Download accessible"
    else
        echo "  [--] /sdcard/Download not accessible  ->  termux-setup-storage"
    fi

    echo ""
    echo "--- APK files in $APK_DIR ---"
    if [ -d "$APK_DIR" ]; then
        ls -lh "$APK_DIR" 2>/dev/null || echo "  (empty)"
    else
        echo "  (directory does not exist)"
    fi
}

menu_apk() {
    while true; do
        echo ""
        echo "=== APK / Demo Tools ==="
        echo "  1) Download latest native debug artifact from PR/run"
        echo "  2) List APKs in $APK_DIR"
        echo "  3) Launch native package"
        echo "  4) Capture screenshot"
        echo "  5) Capture logcat evidence"
        echo "  6) Full smoke test: download -> (pull/install on Windows)"
        echo "  7) Clean old APK/evidence files (dry-run)"
        echo "  8) Clean old APK/evidence files (confirm)"
        echo "  b) Back"
        echo ""
        read -r -p "Choose: " apk_choice
        case "$apk_choice" in
            1)
                read -r -p "GitHub run ID: " run_id
                read -r -p "Artifact name [digital-breakdown-native-debug-apk]: " art_name
                art_name="${art_name:-digital-breakdown-native-debug-apk}"
                read -r -p "Output filename [current.apk]: " out_name
                out_name="${out_name:-current.apk}"
                db-apk-artifact-download \
                    --repo indrolend/digital-breakdown-apk \
                    --run "$run_id" \
                    --artifact "$art_name" \
                    --out "$APK_DIR/$out_name" || true
                ;;
            2)
                echo "APKs in $APK_DIR:"
                ls -lh "$APK_DIR" 2>/dev/null || echo "(none)"
                ;;
            3)
                echo "Launch is handled via ADB from the Windows side."
                echo "  Run: DbApkInstall or DbApkDemo from phone-session.ps1"
                ;;
            4)
                echo "Screenshot is captured from the Windows side via:"
                echo "  adb shell screencap -p /sdcard/Download/db-control/evidence/latest/screen.png"
                echo "  adb pull /sdcard/Download/db-control/evidence/latest/screen.png"
                ;;
            5)
                echo "Logcat evidence is captured from the Windows side via:"
                echo "  . .\\tools\\phone-control\\db-apk-evidence.ps1"
                ;;
            6)
                echo "To run the full smoke test, on Windows run:"
                echo "  . .\\tools\\phone-control\\phone-session.ps1"
                echo "  DbApkDemo -RunId <id> -ArtifactName <name> -Package <pkg>"
                ;;
            7)
                db-cleanup --dry-run || true
                ;;
            8)
                db-cleanup || true
                ;;
            b|B) break ;;
            *) echo "Unknown choice." ;;
        esac
    done
}

menu_repo() {
    while true; do
        echo ""
        echo "=== Repo / GitHub ==="
        echo "  1) gh auth status"
        echo "  2) List open PRs"
        echo "  3) List recent workflow runs"
        echo "  4) List open issues"
        echo "  b) Back"
        echo ""
        read -r -p "Choose: " r_choice
        case "$r_choice" in
            1) gh auth status || true ;;
            2) gh pr list --repo indrolend/digital-breakdown-apk 2>/dev/null || echo "gh not authenticated." ;;
            3) gh run list --repo indrolend/digital-breakdown-apk --limit 10 2>/dev/null || echo "gh not authenticated." ;;
            4) gh issue list --repo indrolend/digital-breakdown-apk 2>/dev/null || echo "gh not authenticated." ;;
            b|B) break ;;
            *) echo "Unknown choice." ;;
        esac
    done
}

menu_cleanup() {
    while true; do
        echo ""
        echo "=== Cleanup / Waste Management ==="
        echo "  1) Dry-run cleanup (preview)"
        echo "  2) Run cleanup (confirm each category)"
        echo "  b) Back"
        echo ""
        read -r -p "Choose: " c_choice
        case "$c_choice" in
            1) db-cleanup --dry-run || true ;;
            2) db-cleanup || true ;;
            b|B) break ;;
            *) echo "Unknown choice." ;;
        esac
    done
}

# Main loop
while true; do
    echo ""
    echo "====================================="
    echo " Digital Breakdown - Phone Menu"
    echo "====================================="
    echo "  1) Status overview"
    echo "  2) APK / Demo Tools"
    echo "  3) Repo / GitHub"
    echo "  4) Clipboard"
    echo "  5) Cleanup / Waste management"
    echo "  6) Start / restart sshd"
    echo "  7) gh auth login"
    echo "  q) Quit"
    echo ""
    read -r -p "Choose: " choice
    case "$choice" in
        1) show_status ;;
        2) menu_apk ;;
        3) menu_repo ;;
        4)
            echo "  a) Get clipboard  b) Set clipboard"
            read -r -p "Choose: " clip_choice
            case "$clip_choice" in
                a) db-clip-get || true ;;
                b) read -r -p "Text to set: " clip_text; echo "$clip_text" | db-clip-set || true ;;
                *) echo "Unknown choice." ;;
            esac
            ;;
        5) menu_cleanup ;;
        6) sshd && echo "[ok] sshd started." || echo "[warn] sshd failed." ;;
        7) gh auth login ;;
        q|Q) echo "Bye."; exit 0 ;;
        *) echo "Unknown choice." ;;
    esac
done
MENU_EOF
    chmod +x "$target"
    ok "db-menu installed at $target"
}

# ---------------------------------------------------------------------------
# Utility script: db-apk-artifact-download
# ---------------------------------------------------------------------------

write_db_apk_download() {
    local target="$TERMUX_BIN/db-apk-artifact-download"
    cat > "$target" << 'DLEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-apk-artifact-download — Download a GitHub Actions artifact via gh CLI.
# Usage: db-apk-artifact-download --repo REPO --run RUN_ID --artifact NAME [--out PATH]

set -euo pipefail

REPO="indrolend/digital-breakdown-apk"
RUN_ID=""
ARTIFACT_NAME=""
OUT_PATH="/sdcard/Download/db-control/apks/current.apk"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo)      REPO="$2";          shift 2 ;;
        --run)       RUN_ID="$2";        shift 2 ;;
        --artifact)  ARTIFACT_NAME="$2"; shift 2 ;;
        --out)       OUT_PATH="$2";      shift 2 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

if [[ -z "$RUN_ID" || -z "$ARTIFACT_NAME" ]]; then
    echo "Usage: db-apk-artifact-download --repo REPO --run RUN_ID --artifact NAME [--out PATH]"
    exit 1
fi

echo "[download] Checking gh auth ..."
if ! gh auth status >/dev/null 2>&1; then
    echo "[fail] gh not authenticated. Run: gh auth login"
    exit 1
fi

# Destination directory
OUT_DIR="$(dirname "$OUT_PATH")"
mkdir -p "$OUT_DIR"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[download] Fetching artifact '$ARTIFACT_NAME' from run $RUN_ID ..."
gh run download "$RUN_ID" \
    --repo "$REPO" \
    --name "$ARTIFACT_NAME" \
    --dir "$TMP_DIR"

# Find the APK inside the downloaded directory
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

# ---------------------------------------------------------------------------
# Utility script: db-gh-settings
# ---------------------------------------------------------------------------

write_db_gh_settings() {
    local target="$TERMUX_BIN/db-gh-settings"
    cat > "$target" << 'GHEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-gh-settings — Check and configure GitHub CLI auth.
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

# ---------------------------------------------------------------------------
# Utility script: db-workflows
# ---------------------------------------------------------------------------

write_db_workflows() {
    local target="$TERMUX_BIN/db-workflows"
    cat > "$target" << 'WFEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-workflows — List recent GitHub Actions workflow runs.
set -euo pipefail
REPO="${1:-indrolend/digital-breakdown-apk}"
echo "=== Recent workflow runs for $REPO ==="
gh run list --repo "$REPO" --limit 15 || echo "gh not authenticated or not installed."
WFEOF
    chmod +x "$target"
    ok "db-workflows installed at $target"
}

# ---------------------------------------------------------------------------
# Utility script: db-clip-set / db-clip-get
# ---------------------------------------------------------------------------

write_db_clip() {
    local set_target="$TERMUX_BIN/db-clip-set"
    cat > "$set_target" << 'CLIPSET'
#!/data/data/com.termux/files/usr/bin/bash
# db-clip-set — Set Android clipboard. Reads from stdin or first argument.
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
# db-clip-get — Get Android clipboard.
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

# ---------------------------------------------------------------------------
# Utility script: db-cleanup
# ---------------------------------------------------------------------------

write_db_cleanup() {
    local target="$TERMUX_BIN/db-cleanup"
    cat > "$target" << 'CLEANEOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-cleanup — Cleanup generated project-control files.
# Usage: db-cleanup [--dry-run]
#
# Rules:
#   - Never deletes GitHub auth.
#   - Never deletes broad Downloads contents.
#   - Only deletes known generated/project-control paths.
#   - --dry-run previews without deleting.

set -euo pipefail

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" ]]; then DRY_RUN=1; fi

DB_CONTROL="/sdcard/Download/db-control"
APK_DIR="$DB_CONTROL/apks"
CLIP_FILE="$DB_CONTROL/.clipboard"

log_would() { echo "[would delete] $*"; }
log_deleted() { echo "[deleted] $*"; }
log_skip() { echo "[skip] $*"; }

safe_remove() {
    local path="$1"
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

echo "=== Cleanup Preview ===" && [[ $DRY_RUN -eq 1 ]] && echo "(dry-run — nothing will be deleted)"

echo ""
echo "--- Clipboard temp file ---"
safe_remove "$CLIP_FILE"

echo ""
echo "--- APK artifacts in $APK_DIR ---"
if [[ -d "$APK_DIR" ]]; then
    # Keep the newest APK, offer to remove older ones
    mapfile -t apks < <(ls -t "$APK_DIR"/*.apk 2>/dev/null || true)
    if [[ ${#apks[@]} -gt 1 ]]; then
        echo "Newest: ${apks[0]} (keeping)"
        for apk in "${apks[@]:1}"; do
            safe_remove "$apk"
        done
    else
        echo "(0 or 1 APK present — nothing to remove)"
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

# ---------------------------------------------------------------------------
# Utility script: db-pr2 (quick PR#2 smoke test helper)
# ---------------------------------------------------------------------------

write_db_pr2() {
    local target="$TERMUX_BIN/db-pr2"
    cat > "$target" << 'PR2EOF'
#!/data/data/com.termux/files/usr/bin/bash
# db-pr2 — Quick helper for PR#2 native debug APK smoke test.
# Downloads the latest native debug artifact and places it in /sdcard/Download/db-control/apks/.

set -euo pipefail

REPO="indrolend/digital-breakdown-apk"
ARTIFACT="digital-breakdown-native-debug-apk"
OUT="/sdcard/Download/db-control/apks/current.apk"

echo "=== PR#2 native debug APK download ==="

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

db-apk-artifact-download \
    --repo "$REPO" \
    --run "$RUN_ID" \
    --artifact "$ARTIFACT" \
    --out "$OUT"

echo ""
echo "APK ready at: $OUT"
echo ""
echo "On Windows, now run:"
echo "  . .\\tools\\phone-control\\phone-session.ps1"
echo "  DbApkInstall -DevicePath /sdcard/Download/db-control/apks/current.apk -Package com.indrolend.digitalbreakdown.native"
PR2EOF
    chmod +x "$target"
    ok "db-pr2 installed at $target"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

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
echo "  db-pr2                     - Quick PR#2 smoke test helper"
echo "  db-gh-settings             - GitHub auth status"
echo "  db-workflows               - List recent workflow runs"
echo "  db-clip-set / db-clip-get  - Clipboard bridge"
echo "  db-cleanup                 - Clean generated files"
echo ""
echo "Next steps:"
echo "  1. Authenticate GitHub CLI:  gh auth login"
echo "  2. Start SSH daemon:         sshd  (already started above)"
echo "  3. From Windows:             . .\\tools\\phone-control\\phone-session.ps1"
echo "  4. Open menu:                DbMenu   (or: db-menu)"
echo ""
echo "Termux user: $(whoami)"
echo "SSH port:    8022"
echo "ADB forward: adb forward tcp:8022 tcp:8022"
