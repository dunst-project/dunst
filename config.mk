# paths
PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
SYSCONFDIR ?= /etc
DATADIR ?= ${PREFIX}/share
# around for backwards compatibility
MANPREFIX ?= ${DATADIR}/man
MANDIR ?= ${MANPREFIX}

DOXYGEN ?= doxygen
FIND ?= find
GCOVR ?= gcovr
GIT ?= git
PKG_CONFIG ?= pkg-config
POD2MAN ?= pod2man
SED ?= sed
SYSTEMCTL ?= systemctl
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

# uncomment to disable parsing of dunstrc
# or use "CFLAGS=-DSTATIC_CONFIG make" to build
#STATIC= -DSTATIC_CONFIG # Warning: This is deprecated behavior

# flags
DEFAULT_CPPFLAGS = -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\"
DEFAULT_CFLAGS   = -g --std=gnu99 -pedantic -Wall -Wno-overlength-strings -Os ${STATIC} ${ENABLE_WAYLAND}
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
endif

ifneq (,$(findstring STATIC_CONFIG,$(CFLAGS)))
$(warning STATIC_CONFIG is deprecated behavior. It will get removed in future releases)
endif
