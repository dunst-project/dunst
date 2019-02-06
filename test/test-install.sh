#!/usr/bin/env bash

set -euo pipefail

BASE="$(dirname "$(dirname "$(readlink -f "$0")")")"
PREFIX="${BASE}/install"

make -C "${BASE}" SYSTEMD=1 SERVICEDIR_SYSTEMD="${PREFIX}/systemd" SERVICEDIR_DBUS="${PREFIX}/dbus" PREFIX="${PREFIX}" install

diff -u <(find "${PREFIX}" -type f -printf "%P\n" | sort) - <<EOF
bin/dunst
dbus/org.knopwob.dunst.service
share/dunst/dunstrc
share/man/man1/dunst.1
systemd/dunst.service
EOF

make -C "${BASE}" SYSTEMD=1 SERVICEDIR_SYSTEMD="${PREFIX}/systemd" SERVICEDIR_DBUS="${PREFIX}/dbus" PREFIX="${PREFIX}" uninstall

if ! [ -z "$(find "${PREFIX}" -type f)" ]; then
        echo "Uninstall failed, following files weren't removed"
        find "${PREFIX}" -type f
        exit 1
fi
