#!/usr/bin/env bash

# Throw error any time a command fails
set -euo pipefail

MAKE=${MAKE:-make}

# Export parameters so they are useable by subshells and make
export BASE="$(dirname "$(dirname "$(readlink -f "$0")")")"
export DESTDIR="${BASE}/install"
export PREFIX="/testprefix"
export SYSCONFDIR="/sysconfdir"
export SYSCONFFILE="${SYSCONFDIR}/dunst/dunstrc"
export SYSTEMD=1
export SERVICEDIR_SYSTEMD="/systemd"
export SERVICEDIR_DBUS="/dbus"

do_make() {  # for convenience/conciseness
	${MAKE} -C "${BASE}" "$@"
}

check_dest() {
	# Check file list given on stdin and see if all are actually present
	diff -u <(find "${DESTDIR}" -type f -printf "%P\n" | sort) <(sort -)
}

do_make install

check_dest <<EOF
dbus/org.knopwob.dunst.service
sysconfdir/dunst/dunstrc
systemd/dunst.service
testprefix/bin/dunst
testprefix/bin/dunstctl
testprefix/bin/dunstify
testprefix/share/bash-completion/completions/dunst
testprefix/share/bash-completion/completions/dunstctl
testprefix/share/fish/vendor_completions.d/dunst.fish
testprefix/share/fish/vendor_completions.d/dunstctl.fish
testprefix/share/fish/vendor_completions.d/dunstify.fish
testprefix/share/man/man1/dunst.1
testprefix/share/man/man1/dunstctl.1
testprefix/share/man/man1/dunstify.1
testprefix/share/man/man5/dunst.5
testprefix/share/zsh/site-functions/_dunst
testprefix/share/zsh/site-functions/_dunstctl
EOF

do_make uninstall

# dunstrc should still be there
check_dest <<EOF
sysconfdir/dunst/dunstrc
EOF

do_make uninstall-purge

# Expect empty
if ! [ -z "$(find "${DESTDIR}" -type f)" ]; then
        echo "Uninstall failed, following files weren't removed"
        find "${DESTDIR}" -type f
        exit 1
fi
