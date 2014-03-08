# paths
PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man

# In dist tarballs, the version is stored in the VERSION files.
VERSION := '$(shell [ -f VERSION ] && cat VERSION)'
ifeq ('',$(VERSION))
VERSION := $(shell git describe)
endif

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# uncomment to disable parsing of dunstrc
# or use "CFLAGS=-DSTATIC_CONFIG make" to build
#STATIC= -DSTATIC_CONFIG

PKG_CONFIG:=$(shell which pkg-config)
ifeq (${PKG_CONFIG}, ${EMPTY})
	$(error "Failed to find pkg-config, please make sure it is installed")
endif

# flags
CPPFLAGS += -D_BSD_SOURCE -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${INIFLAGS}
CFLAGS   += -g --std=gnu99 -pedantic -Wall -Wno-overlength-strings -Os ${STATIC} ${CPPFLAGS} ${EXTRACFLAGS}

pkg_config_packs:="dbus-1 x11 freetype2 xext xft xscrnsaver glib-2.0 gio-2.0 pango cairo pangocairo"

# check if we need libxdg-basedir
ifeq (,$(findstring STATIC_CONFIG,$(CFLAGS)))
	pkg_config_packs += libxdg-basedir
endif

# includes and libs
INCS := $(shell ${PKG_CONFIG} --cflags ${pkg_config_packs})
CFLAGS += ${INCS}
LDFLAGS += -lm -L${X11LIB} -lXss ${XINERAMALIBS} $(shell ${PKG_CONFIG} --libs ${pkg_config_packs})

# only make this an fatal error when where not cleaning
ifneq (clean, $(MAKECMDGOALS))
ifeq (${INCS}, ${EMPTY})
$(error "pkg-config failed, see errors above")
endif
endif
