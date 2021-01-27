#!/usr/bin/env bash

# Throw error any time a command fails
set -euo pipefail

BASE="$(dirname "$(dirname "$(readlink -f "$0")")")"
DESTDIR="${BASE}/install"
PREFIX="/testprefix"
SYSCONFDIR="/sysconfdir"

make -C "${BASE}" SYSTEMD=1 DESTDIR="${DESTDIR}" PREFIX="${PREFIX}" SYSCONFDIR="${SYSCONFDIR}" SERVICEDIR_SYSTEMD="/systemd" SERVICEDIR_DBUS="/dbus" install

diff -u <(find "${DESTDIR}" -type f -printf "%P\n" | sort) - <<EOF
dbus/org.knopwob.dunst.service
sysconfdir/dunst/dunstrc
systemd/dunst.service
testprefix/bin/dunst
testprefix/bin/dunstctl
testprefix/bin/dunstify
testprefix/share/man/man1/dunst.1
testprefix/share/man/man1/dunstctl.1
testprefix/share/man/man5/dunst.5
EOF
# make sure to manually sort the above values

make -C "${BASE}" SYSTEMD=1 DESTDIR="${DESTDIR}" PREFIX="${PREFIX}" SYSCONFDIR="${SYSCONFDIR}" SERVICEDIR_SYSTEMD="/systemd" SERVICEDIR_DBUS="/dbus" uninstall

if ! [ -z "$(find "${DESTDIR}" -type f)" ]; then
        echo "Uninstall failed, following files weren't removed"
        find "${DESTDIR}" -type f
        exit 1
fi
