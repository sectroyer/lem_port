#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Installs the signed iOS app on a USB-connected iPad or iPhone and, if given,
# copies the DOS Lemmings data folder into the app's files. Works for every
# iOS version, also those Xcode 16 can no longer run apps on.
# Needs libimobiledevice's tools: sudo port install ideviceinstaller
#
#   tools/ios_install.sh [-u UDID] APP [DATA_FOLDER]
#
#   APP          the built lem_port.app (Release-iphoneos/lem_port.app)
#   DATA_FOLDER  optional; the folder with main.dat, level000.dat, ...
#                Its files go to the app's Documents (files of the same name
#                are replaced, others are kept).
#   -u UDID      the device, when more than one is connected (idevice_id -l)
set -e

udid=""
if [ "$1" = "-u" ]; then
    udid="$2"
    shift 2
fi
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 [-u UDID] APP [DATA_FOLDER]" >&2
    exit 1
fi
app="$1"
data="$2"

for tool in idevice_id ideviceinstaller afcclient; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool is missing: sudo port install ideviceinstaller" >&2
        exit 1
    fi
done
if [ ! -f "$app/Info.plist" ]; then
    echo "$app is not an app bundle" >&2
    exit 1
fi
if [ -n "$data" ] && [ ! -d "$data" ]; then
    echo "$data is not a folder" >&2
    exit 1
fi

devices=$(idevice_id -l)
if [ -z "$devices" ]; then
    echo "no device connected (connect it by USB, unlock it, tap Trust)" >&2
    exit 1
fi
if [ -z "$udid" ]; then
    if [ "$(echo "$devices" | wc -l)" -gt 1 ]; then
        echo "more than one device, choose one with -u:" >&2
        echo "$devices" >&2
        exit 1
    fi
    udid="$devices"
fi

bundle=$(plutil -extract CFBundleIdentifier raw "$app/Info.plist")
echo "installing $bundle on $udid"
ideviceinstaller -u "$udid" install "$app"

if [ -n "$data" ]; then
    echo "copying $data into the app's files"
    # File by file: `put -r` onto a folder that is already there would nest
    # a second copy inside it, and the device's `rm` takes only empty folders.
    {
        (cd "$data" && find . -mindepth 1 -type d) | sed 's|^\./||' | while read -r d; do
            echo "mkdir \"Documents/$d\""
        done
        (cd "$data" && find . -type f ! -name .DS_Store) | sed 's|^\./||' | while read -r f; do
            echo "put -f \"$data/$f\" \"Documents/$f\""
        done
        echo quit
    } | afcclient -u "$udid" --documents "$bundle" >/dev/null 2>&1
    echo "app's files now:"
    printf 'ls Documents\nquit\n' | afcclient -u "$udid" --documents "$bundle" |
        sed 's/\x1b\[[0-9;]*m//g' | grep -v '^afc:' | sed 's/^/  /'
fi
echo "done: start lem_port on the device"
