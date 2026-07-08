#!/data/data/com.termux/files/usr/bin/bash
set -e

REPO="${REPO:-indrolend/digital-breakdown-apk}"
TERMUX_USER="$(whoami)"

log() {
  printf '\n== %s ==\n' "$1"
}

log "PHONE / TERMUX BOOTSTRAP"

pkg update -y
pkg install -y gh git curl jq openssh nano termux-api

mkdir -p "$HOME/bin" "$HOME/db-control" "$HOME/.ssh"

if ! grep -q 'export PATH="$HOME/bin:$PATH"' "$HOME/.bashrc" 2>/dev/null; then
  printf '%s\n' 'export PATH="$HOME/bin:$PATH"' >> "$HOME/.bashrc"
fi

cat > "$HOME/.bashrc.tmp" <<'EOS'
export PATH="$HOME/bin:$PATH"
export REPO="indrolend/digital-breakdown-apk"
export PS1='[PHONE:Termux \W]\$ '
EOS
cat "$HOME/.bashrc" >> "$HOME/.bashrc.tmp" 2>/dev/null || true
mv "$HOME/.bashrc.tmp" "$HOME/.bashrc"

cat > "$HOME/bin/db-gh-settings" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -e
REPO="${REPO:-indrolend/digital-breakdown-apk}"

if ! gh auth status >/dev/null 2>&1; then
  gh auth login
fi

gh auth status

gh api --method PUT "repos/$REPO/actions/permissions" \
  -f enabled=true \
  -f allowed_actions=all

gh api --method PUT "repos/$REPO/actions/permissions/workflow" \
  -f default_workflow_permissions=write \
  -F can_approve_pull_request_reviews=false

gh api --method PUT "repos/$REPO/actions/permissions/fork-pr-contributor-approval" \
  -f approval_policy=first_time_contributors || true

gh api "repos/$REPO/actions/permissions" --jq .
gh api "repos/$REPO/actions/permissions/workflow" --jq .
gh api "repos/$REPO/actions/permissions/fork-pr-contributor-approval" --jq . || true
EOS
chmod +x "$HOME/bin/db-gh-settings"

cat > "$HOME/bin/db-pr2" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -e
REPO="${REPO:-indrolend/digital-breakdown-apk}"

gh pr view 2 --repo "$REPO"

cat <<MSG

Useful commands:
  gh pr ready 2 --repo $REPO
  gh pr checks 2 --repo $REPO --watch
  gh pr merge 2 --repo $REPO --squash
MSG
EOS
chmod +x "$HOME/bin/db-pr2"

cat > "$HOME/bin/db-workflows" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -e
REPO="${REPO:-indrolend/digital-breakdown-apk}"

gh workflow list --repo "$REPO"
printf '\n'
gh run list --repo "$REPO" --limit 10
EOS
chmod +x "$HOME/bin/db-workflows"

cat > "$HOME/bin/db-clip-set" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -e
if command -v termux-clipboard-set >/dev/null 2>&1; then
  termux-clipboard-set
else
  mkdir -p "$HOME/db-control"
  cat > "$HOME/db-control/phone-clipboard.txt"
  echo "Termux:API clipboard unavailable; saved to $HOME/db-control/phone-clipboard.txt" >&2
fi
EOS
chmod +x "$HOME/bin/db-clip-set"

cat > "$HOME/bin/db-clip-get" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -e
if command -v termux-clipboard-get >/dev/null 2>&1; then
  termux-clipboard-get
elif [ -f "$HOME/db-control/phone-clipboard.txt" ]; then
  cat "$HOME/db-control/phone-clipboard.txt"
else
  echo ""
fi
EOS
chmod +x "$HOME/bin/db-clip-get"

cat > "$HOME/bin/db-menu" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
REPO="${REPO:-indrolend/digital-breakdown-apk}"

while true; do
  clear
  echo "[PHONE:Termux] Digital Breakdown Control"
  echo "Repo: $REPO"
  echo ""
  echo "1) GitHub auth status"
  echo "2) GitHub login"
  echo "3) Apply GitHub Actions/settings automation"
  echo "4) View PR #2"
  echo "5) Mark PR #2 ready"
  echo "6) Watch PR #2 checks"
  echo "7) Squash-merge PR #2"
  echo "8) List workflows/runs"
  echo "9) Start sshd"
  echo "10) Show Termux user"
  echo "11) Show phone clipboard"
  echo "0) Exit"
  echo ""
  read -r -p "Select: " choice

  case "$choice" in
    1) gh auth status; read -r -p "Enter..." ;;
    2) gh auth login; read -r -p "Enter..." ;;
    3) db-gh-settings; read -r -p "Enter..." ;;
    4) gh pr view 2 --repo "$REPO"; read -r -p "Enter..." ;;
    5) gh pr ready 2 --repo "$REPO"; read -r -p "Enter..." ;;
    6) gh pr checks 2 --repo "$REPO" --watch; read -r -p "Enter..." ;;
    7) gh pr merge 2 --repo "$REPO" --squash; read -r -p "Enter..." ;;
    8) db-workflows; read -r -p "Enter..." ;;
    9) sshd || true; echo "sshd running on port 8022"; read -r -p "Enter..." ;;
    10) whoami; read -r -p "Enter..." ;;
    11) db-clip-get; echo ""; read -r -p "Enter..." ;;
    0) exit 0 ;;
    *) echo "Invalid"; sleep 1 ;;
  esac
done
EOS
chmod +x "$HOME/bin/db-menu"

# The shared-storage path can accidentally become a directory if an earlier
# command used mkdir on the full filename. Clean it before writing the file.
rm -rf /sdcard/Download/termux-user.txt 2>/dev/null || true
printf '%s\n' "$TERMUX_USER" > "$HOME/db-control/termux-user.txt"
printf '%s\n' "$TERMUX_USER" > /sdcard/Download/termux-user.txt 2>/dev/null || true

sshd || true

cat <<MSG

== COMPLETE ==
Termux user: $TERMUX_USER
Run: db-menu

If SSH asks for a password and none is set, run:
  passwd
MSG
