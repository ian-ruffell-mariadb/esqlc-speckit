#!/usr/bin/env bash
# Scaffold specs/NNN-slug/ from the templates.
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $(basename "$0") \"short feature name\"" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
NAME="$*"
SLUG=$(printf '%s' "$NAME" | tr '[:upper:]' '[:lower:]' \
       | sed -E 's/[^a-z0-9]+/-/g; s/^-+|-+$//g')

last=$(find "$ROOT/specs" -maxdepth 1 -type d -name '[0-9][0-9][0-9]-*' \
        -exec basename {} \; 2>/dev/null | sort | tail -1 | cut -d- -f1)
next=$(printf '%03d' $((10#${last:-0} + 1)))

DIR="$ROOT/specs/$next-$SLUG"
mkdir -p "$DIR/contracts"

for t in spec plan tasks; do
  sed -e "s/\[NAME\]/$NAME/g" \
      -e "s|NNN-slug|$next-$SLUG|g" \
      -e "s/NNN/$next/g" \
      "$ROOT/.specify/templates/$t-template.md" > "$DIR/$t.md"
done

echo "Created $DIR"
echo "Now: /speckit.specify  (then plan, tasks, analyze, implement)"
