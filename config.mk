# paths
PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man

PKG_CONFIG ?= pkg-config

# Disable systemd service file installation,
# if you don't want to use systemd albeit installed
#SYSTEMD ?= 0

# uncomment to disable parsing of dunstrc
# or use "CFLAGS=-DSTATIC_CONFIG make" to build
#STATIC= -DSTATIC_CONFIG # Warning: This is deprecated behavior

# flags
DEFAULT_CPPFLAGS = -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\"
DEFAULT_CFLAGS   = -g --std=gnu99 -pedantic -Wall -Wno-overlength-strings -Os ${STATIC}
DEFAULT_LDFLAGS  = -lm

CPPFLAGS_DEBUG := -DDEBUG_BUILD
CFLAGS_DEBUG   := -O0
LDFLAGS_DEBUG  :=

pkg_config_packs := dbus-1 \
                    gio-2.0 \
                    gdk-pixbuf-2.0 \
                    "glib-2.0 >= 2.36" \
                    pangocairo \
                    x11 \
                    xinerama \
                    "xrandr >= 1.5" \
                    xscrnsaver

# check if we need libxdg-basedir
ifeq (,$(findstring STATIC_CONFIG,$(CFLAGS)))
	pkg_config_packs += libxdg-basedir
else
$(warning STATIC_CONFIG is deprecated behavior. It will get removed in future releases)
endif

# dunstify also needs libnotify
ifneq (,$(findstring dunstify,${MAKECMDGOALS}))
	pkg_config_packs += libnotify
endif
