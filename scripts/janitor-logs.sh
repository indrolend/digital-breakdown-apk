#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p logs/_archives

echo "== Latest log folders =="
find logs -maxdepth 1 -type d \
  \( -name "root-probe-*" -o -name "lg-q710-interrogation-*" -o -name "lg-download-mode-probe-*" \) \
  -print | sort

echo ""
echo "== Delete empty .txt files =="
find logs -type f -name "*.txt" -size 0 -print -delete 2>/dev/null || true

echo ""
echo "== Archive probe folders older than 7 days =="
while IFS= read -r dir; do
  [ -d "$dir" ] || continue
  base="$(basename "$dir")"
  archive="logs/_archives/$base.tar.gz"

  if [ ! -f "$archive" ]; then
    echo "Archiving $dir -> $archive"
    tar -czf "$archive" "$dir"
  fi

  rm -rf "$dir"
done < <(
  find logs -maxdepth 1 -type d \
    \( -name "root-probe-*" -o -name "lg-q710-interrogation-*" -o -name "lg-download-mode-probe-*" \) \
    -mtime +7 \
    -print
)

echo ""
echo "== Disk usage =="
du -sh logs 2>/dev/null || true

echo ""
echo "Done."
