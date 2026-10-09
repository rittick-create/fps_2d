#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ -n "${QMAKE:-}" ]]; then
    grid_qmake="$QMAKE"
elif command -v qmake6 >/dev/null 2>&1; then
    grid_qmake="$(command -v qmake6)"
else
    grid_qmake=""
    for candidate in "$HOME"/Qt/6.*/macos/bin/qmake; do
        [[ -x "$candidate" ]] && grid_qmake="$candidate"
    done
    if [[ -z "$grid_qmake" ]] && command -v qmake >/dev/null 2>&1; then
        grid_qmake="$(command -v qmake)"
    fi
fi
if [[ -z "$grid_qmake" ]]; then
    echo 'Qt 6 qmake is required. Set QMAKE=/path/to/Qt6/bin/qmake and run again.' >&2
    exit 1
fi
mkdir -p build
cd build
"$grid_qmake" ../GridBreach2D.pro
make -j8
if [[ "$(uname -s)" == Darwin ]]; then
    open GridBreach2D.app
else
    exec ./GridBreach2D
fi
