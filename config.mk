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


PKG_CONFIG=$(shell which pkg-config)
ifeq (${PKG_CONFIG}, ${EMPTY})
    $(error "Failed to find pkg-config, please make sure it is installed)
endif

# includes and libs
INCS = $(shell ${PKG_CONFIG} --cflags dbus-1 libxdg-basedir x11 freetype2 xext xft xscrnsaver)
LIBS = -lm -L${X11LIB} -lXss ${XINERAMALIBS} $(shell ${PKG_CONFIG} --libs dbus-1 libxdg-basedir x11 freetype2 xext xft xscrnsaver)

ifeq (${INCS}, ${EMPTY})
    $(error Failed to find one ore move required dependencies:  dbus-1 libxdg-basedir x11 freetype2 xext xft xscrnsaver)
endif

# flags
CPPFLAGS += -D_BSD_SOURCE -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${INIFLAGS}
CFLAGS   += -g --std=c99 -pedantic -Wall -Wno-overlength-strings -Os ${INCS} ${STATIC} ${CPPFLAGS} ${EXTRACFLAGS}
LDFLAGS  += ${LIBS}
