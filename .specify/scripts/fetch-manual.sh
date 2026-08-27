#!/usr/bin/env bash
# Fetch the behavioural-contract manual into a local, git-ignored directory.
# The PDF is HP copyright and is deliberately NOT vendored in this repo.
set -euo pipefail

URL="http://nonstoptools.com/manuals/SqlMp-C-Reference.pdf"
DEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/manual"
DEST="$DEST_DIR/SqlMp-C-Reference.pdf"

# Known-good fixture: part number 429847-008, August 2012, 331 pages.
EXPECTED_BYTES=1130780

mkdir -p "$DEST_DIR"

if [[ -f "$DEST" ]]; then
  echo "Already present: $DEST"
else
  echo "Fetching $URL"
  curl -fsSL -o "$DEST" "$URL"
fi

actual=$(wc -c < "$DEST" | tr -d ' ')
if [[ "$actual" != "$EXPECTED_BYTES" ]]; then
  echo "WARNING: size $actual != expected $EXPECTED_BYTES." >&2
  echo "The upstream document may have been revised. Citations in specs/ are" >&2
  echo "pinned to part number 429847-008 — verify before trusting page labels." >&2
fi

echo "Manual at: $DEST"
echo "Next: ./.specify/scripts/extract-manual.py"
