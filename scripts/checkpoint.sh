#!/usr/bin/env bash
set -euo pipefail

MSG="${*:-}"

if [ -z "$MSG" ]; then
  echo "Usage:"
  echo "  ./scripts/checkpoint.sh \"Commit message\""
  exit 2
fi

echo "== Status before validation =="
git status --short

echo ""
echo "== Validate =="
npm run android:postboot
npm run lg:probe

echo ""
echo "== Stage all tracked and new files =="
git add -A

echo ""
echo "== Staged diff summary =="
git diff --cached --stat

echo ""
echo "== Commit =="
git commit -m "$MSG"

echo ""
echo "== Push =="
git push

echo ""
echo "== Final status =="
git status --short
