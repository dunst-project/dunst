# paths
PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man

VERSION := "1.2.0-non-git"
ifneq ($(wildcard ./.git/.),)
VERSION := $(shell git describe --tags)
endif

# Warning: This is deprecated behavior
# uncomment to disable parsing of dunstrc
# or use "CFLAGS=-DSTATIC_CONFIG make" to build
#STATIC= -DSTATIC_CONFIG

PKG_CONFIG:=$(shell which pkg-config)
ifeq (${PKG_CONFIG}, ${EMPTY})
$(error "Failed to find pkg-config, please make sure it is installed")
endif

# flags
CPPFLAGS += -D_DEFAULT_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS   += -g --std=gnu99 -pedantic -Wall -Wno-overlength-strings -Os ${STATIC} ${CPPFLAGS}

pkg_config_packs := dbus-1 x11 xscrnsaver \
                    "glib-2.0 >= 2.36" gio-2.0 \
                    pangocairo gdk-3.0 xrandr xinerama

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

# includes and libs
INCS := $(shell ${PKG_CONFIG} --cflags ${pkg_config_packs})
CFLAGS += ${INCS}
LDFLAGS += -lm -L${X11LIB} -lXss $(shell ${PKG_CONFIG} --libs ${pkg_config_packs})

# only make this an fatal error when where not cleaning
ifneq (clean, $(MAKECMDGOALS))
ifeq (${INCS}, ${EMPTY})
$(error "pkg-config failed, see errors above")
endif
endif
