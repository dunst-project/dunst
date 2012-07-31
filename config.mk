# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

VERSION="0.3.0"

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

XFTINC = -I/usr/include/freetype2
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
LIBS = -L${X11LIB} -lX11 -lXss ${XFTLIBS} ${XINERAMALIBS} $(shell pkg-config --libs dbus-1 libxdg-basedir)

# flags
CPPFLAGS += -D_BSD_SOURCE -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${INIFLAGS}
CFLAGS   += -g --std=c99 -pedantic -Wall -Wno-overlength-strings -Os ${INCS} ${STATIC} ${CPPFLAGS}
LDFLAGS  += ${LIBS}
