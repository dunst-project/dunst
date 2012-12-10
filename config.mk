OS := $(shell uname -o)
ifeq ($(OS), GNU/Linux)
	X11INC = /usr/include/X11
	X11LIB = /usr/lib/X11
else ifeq ($(OS), FreeBSD)
	X11INC = /usr/local/include
	X11LIB = /usr/local/lib
	XFTINC = -I${X11INC}/freetype2
else ifeq ($(OS), OpenBSD)
	X11INC = /usr/X11R6/include
	X11LIB = /usr/X11R6/lib
	XFTINC = -I/usr/include/freetype2
endif

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# In dist tarballs, the version is stored in the VERSION files.
VERSION := '$(shell [ -f VERSION ] && cat VERSION)'
ifeq ('',$(VERSION))
VERSION := $(shell git describe)
endif

XFTLIBS  = -lXft

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# uncomment to disable parsing of dunstrc
# or use "CFLAGS=-DSTATIC_CONFIG make" to build
#STATIC= -DSTATIC_CONFIG

# inih flags
INIFLAGS = -DINI_ALLOW_MULTILINE=0

# includes and libs
INCS = -I${X11INC} $(shell pkg-config --cflags dbus-1 libxdg-basedir) ${XFTINC}
LIBS = -lm -L${X11LIB} -lX11 -lXss ${XFTLIBS} ${XINERAMALIBS} $(shell pkg-config --libs dbus-1 libxdg-basedir)

# flags
CPPFLAGS += -D_BSD_SOURCE -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${INIFLAGS}
CFLAGS   += -g --std=c99 -pedantic -Wall -Wno-overlength-strings -Os ${INCS} ${STATIC} ${CPPFLAGS} ${EXTRACFLAGS}
LDFLAGS  += ${LIBS}
