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

# inih flags
INIFLAGS = -DINI_ALLOW_MULTILINE=0


PKG_CONFIG:=$(shell which pkg-config)
ifeq (${PKG_CONFIG}, ${EMPTY})
	$(error "Failed to find pkg-config, please make sure it is installed")
endif

pkg_config_packs:="dbus-1 libxdg-basedir x11 freetype2 xext xft xscrnsaver glib-2.0 gio-2.0 pango cairo pangocairo"

# includes and libs
INCS := $(shell ${PKG_CONFIG} --cflags ${pkg_config_packs})
LIBS := -lm -L${X11LIB} -lXss ${XINERAMALIBS} $(shell ${PKG_CONFIG} --libs ${pkg_config_packs})

ifeq (${INCS}, ${EMPTY})
    $(error "pkg-config failed, see errors above")
endif

# flags
CPPFLAGS += -D_BSD_SOURCE -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${INIFLAGS}
CFLAGS   += -g --std=c99 -pedantic -Wall -Wno-overlength-strings -Os ${INCS} ${STATIC} ${CPPFLAGS} ${EXTRACFLAGS}
LDFLAGS  += ${LIBS}
