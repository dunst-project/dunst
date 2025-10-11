# paths
PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
SYSCONFDIR ?= ${PREFIX}/etc/xdg
SYSCONFFILE ?= ${SYSCONFDIR}/dunst/dunstrc
DATADIR ?= ${PREFIX}/share
BASHCOMPLETIONDIR ?= ${DATADIR}/bash-completion/completions
FISHCOMPLETIONDIR ?= ${DATADIR}/fish/vendor_completions.d
ZSHCOMPLETIONDIR ?= ${DATADIR}/zsh/site-functions
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

# Disable dependency on libnotify. this will remove the ability
# to easily send notifications using dunstify or notify-send via D-Bus.
# Do this if you have your own utility to send notifications.
# Other applications will continue to work, as they use direct D-Bus.
# DUNSTIFY ?=0

# Disable installation of completions.
# COMPLETIONS ?= 0

ifneq (0, ${WAYLAND})
ENABLE_WAYLAND= -DENABLE_WAYLAND
endif

ifneq (0, ${X11})
ENABLE_X11= -DENABLE_X11
endif

# flags
DEFAULT_CPPFLAGS = -Wno-gnu-zero-variadic-macro-arguments -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\" -DSYSCONFDIR=\"${SYSCONFDIR}\" ${ENABLE_WAYLAND} ${ENABLE_X11}
DEFAULT_CFLAGS   = -g -std=gnu11 -pedantic -Wall -Wno-overlength-strings -Os ${EXTRA_CFLAGS}
DEFAULT_LDFLAGS  = -lm -lrt

CPPFLAGS_DEBUG :=
CFLAGS_DEBUG   := -O0
LDFLAGS_DEBUG  :=

pkg_config_packs := gio-2.0 \
                    gdk-pixbuf-2.0 \
                    "glib-2.0 >= 2.44" \
                    librsvg-2.0 \
                    pangocairo \

ifneq (0,${WAYLAND})
pkg_config_packs += wayland-client
pkg_config_packs += wayland-cursor
endif

ifneq (0,${X11})
pkg_config_packs += x11
pkg_config_packs += xinerama
pkg_config_packs += xext
pkg_config_packs += "xrandr >= 1.5"
pkg_config_packs += xscrnsaver
endif
