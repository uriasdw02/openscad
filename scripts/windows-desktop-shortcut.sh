#!/usr/bin/env bash
# Creates a Windows desktop shortcut for this OpenSCAD build.
# Run from the MSYS2 UCRT64 shell:  ./scripts/windows-desktop-shortcut.sh
#
# OpenSCAD built with MSYS2 needs the MSYS2 libraries (DLLs) on PATH, so the
# shortcut starts a tiny launcher that adds them and then opens openscad.exe.
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$REPO_DIR/build}"
NAME="${SHORTCUT_NAME:-My OpenSCAD}"

if [ ! -f "$BUILD_DIR/openscad.exe" ]; then
  echo "Can't find $BUILD_DIR/openscad.exe - build it first (cmake --build build)." >&2
  exit 1
fi

MSYS_BIN_WIN="$(cygpath -w "${MINGW_PREFIX:-/ucrt64}/bin")"
BUILD_WIN="$(cygpath -w "$BUILD_DIR")"

# 1. Launcher next to openscad.exe (CRLF line endings for cmd.exe).
printf '@echo off\r\nset "PATH=%s;%%PATH%%"\r\nstart "" "%%~dp0openscad.exe" %%*\r\n' \
  "$MSYS_BIN_WIN" > "$BUILD_DIR/Launch OpenSCAD.cmd"

# 2. Desktop shortcut that runs the launcher minimized and uses OpenSCAD's own icon.
PS1="$(mktemp --suffix=.ps1)"
cat > "$PS1" <<EOF
\$desktop = [Environment]::GetFolderPath('Desktop')
\$s = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path \$desktop '$NAME.lnk'))
\$s.TargetPath = '$BUILD_WIN\\Launch OpenSCAD.cmd'
\$s.WorkingDirectory = '$BUILD_WIN'
\$s.IconLocation = '$BUILD_WIN\\openscad.exe,0'
\$s.WindowStyle = 7
\$s.Description = 'OpenSCAD (my build)'
\$s.Save()
Write-Host ('Shortcut created: ' + (Join-Path \$desktop '$NAME.lnk'))
EOF
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$PS1")"
rm -f "$PS1"
