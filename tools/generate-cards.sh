#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<'EOF'
Usage: generate-cards.sh <text-file> <output-dir> [--font PATH] [--font-size N] [--cover-title TEXT]

Generate Xteink X4 card BMPs from a text list.

Example:
  ./tools/generate-cards.sh cards.txt ./sd/aaa --font ./fonts/NotoSansJP-Regular.otf

Output layout (copy output-dir to SD card root):
  /aaa/cover.bmp
  /aaa/000.bmp
  /aaa/001.bmp
EOF
}

if [[ $# -lt 2 ]]; then
  usage
  exit 1
fi

PYTHON_BIN="${PYTHON_BIN:-python3}"

if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  echo "python3 not found" >&2
  exit 1
fi

if ! "$PYTHON_BIN" -c "import PIL" >/dev/null 2>&1; then
  echo "Installing Pillow..."
  "$PYTHON_BIN" -m pip install --user pillow
fi

exec "$PYTHON_BIN" "$SCRIPT_DIR/generate_cards.py" "$@"
