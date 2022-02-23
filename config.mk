# paths
PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
SYSCONFDIR ?= ${PREFIX}/etc/xdg
SYSCONFFILE ?= ${SYSCONFDIR}/dunst/dunstrc
DATADIR ?= ${PREFIX}/share
# around for backwards compatibility
MANPREFIX ?= ${DATADIR}/man
MANDIR ?= ${MANPREFIX}
SERVICEDIR_DBUS ?= ${DATADIR}/dbus-1/services
SERVICEDIR_SYSTEMD ?= ${PREFIX}/lib/systemd/user
EXTRA_CFLAGS ?=

DOXYGEN ?= doxygen
FIND ?= find
GCOVR ?= gcovr
GIT ?= git
PKG_CONFIG ?= pkg-config
POD2MAN ?= pod2man
SED ?= sed
SYSTEMDAEMON ?= systemd
VALGRIND ?= valgrind

# Disable systemd service file installation,
# if you don't want to use systemd albeit installed
#SYSTEMD ?= 0

# Disable dependency on wayland. This will force dunst to use
# xwayland on wayland compositors
# You can also use "make WAYLAND=0" to build without wayland
# WAYLAND ?= 0

ifneq (0, ${WAYLAND})
ENABLE_WAYLAND= -DENABLE_WAYLAND
endif

# flags
DEFAULT_CPPFLAGS = -Wno-gnu-zero-variadic-macro-arguments -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\" -DSYSCONFDIR=\"${SYSCONFDIR}\"
DEFAULT_CFLAGS   = -g -std=gnu99 -pedantic -Wall -Wno-overlength-strings -Os ${ENABLE_WAYLAND} ${EXTRA_CFLAGS}
DEFAULT_LDFLAGS  = -lm -lrt

CPPFLAGS_DEBUG := -DDEBUG_BUILD
CFLAGS_DEBUG   := -O0
LDFLAGS_DEBUG  :=

pkg_config_packs := gio-2.0 \
                    gdk-pixbuf-2.0 \
                    "glib-2.0 >= 2.44" \
                    pangocairo \
                    x11 \
                    xinerama \
                    xext \
                    "xrandr >= 1.5" \
                    xscrnsaver \


# dunstify also needs libnotify
pkg_config_packs += libnotify

ifneq (0,${WAYLAND})
pkg_config_packs += wayland-client
pkg_config_packs += wayland-cursor
endif
