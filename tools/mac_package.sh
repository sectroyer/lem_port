#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds a self-contained universal Lem_Port.app (SDL3 built in, Apple
# Silicon + Intel, macOS 11+) and packs it as a zip that can be given to
# other Macs.
#
#   tools/mac_package.sh [BUILD_DIR]      (default: build-mac)
set -e
src=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-build-mac}

cmake -S "$src" -B "$build" -DCMAKE_BUILD_TYPE=Release -DLEM_FETCH_SDL=ON \
    -DLEM_BUILD_TESTS=OFF "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$build" --target lem_port -j 8

app="$build/Lem_Port.app"
# Ad-hoc signature for the whole bundle. Without a paid Developer ID the
# first start on another Mac needs right click -> Open (once).
codesign --force --deep -s - "$app"
rm -f "$build/Lem_Port-macos.zip"
ditto -c -k --keepParent "$app" "$build/Lem_Port-macos.zip"
echo "created $build/Lem_Port-macos.zip"
