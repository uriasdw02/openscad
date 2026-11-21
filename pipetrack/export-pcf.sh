#!/usr/bin/env bash
# Export a PipeTrack model to a PCF (Piping Component File) for Spoolgen,
# AutoCAD Plant 3D, Isogen and other piping software.
#
# Run from the MSYS2 UCRT64 shell:
#   ./pipetrack/export-pcf.sh pipetrack/examples/sketch_1050_800.scad
#   ./pipetrack/export-pcf.sh my_spool.scad  my_spool.pcf
#
# How it works: OpenSCAD runs the model with $pipe_pcf=true, pipe_run() prints
# every PCF line to the console with a "PCF|" tag, and this script keeps those.
set -e

SCAD="$1"
if [ -z "$SCAD" ] || [ ! -f "$SCAD" ]; then
  echo "Usage: $0 model.scad [output.pcf]" >&2
  exit 1
fi
OUT="${2:-${SCAD%.scad}.pcf}"

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OPENSCAD="${OPENSCAD:-$REPO_DIR/build/openscad.exe}"
[ -x "$OPENSCAD" ] || OPENSCAD="$(command -v openscad || true)"
if [ -z "$OPENSCAD" ]; then
  echo "Can't find openscad.exe - build it first or set OPENSCAD=/path/to/openscad" >&2
  exit 1
fi

ECHO_FILE="$(mktemp --suffix=.echo)"
"$OPENSCAD" -o "$ECHO_FILE" -D '$pipe_pcf=true' "$SCAD" 2>/dev/null || true

# ECHO: "PCF|PIPE"  ->  PIPE      (CRLF line endings, like most PCF readers expect)
grep '^ECHO: "PCF|' "$ECHO_FILE" | sed -e 's/^ECHO: "PCF|//' -e 's/"$//' | sed 's/$/\r/' > "$OUT"
rm -f "$ECHO_FILE"

if [ ! -s "$OUT" ]; then
  echo "No PCF data was produced - does the model call pipe_run()? Open it in OpenSCAD and check the console for errors." >&2
  exit 1
fi
echo "Wrote $OUT ($(grep -c . "$OUT") lines)"
