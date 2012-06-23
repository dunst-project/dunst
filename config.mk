# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xft, comment if you don't want it
XFTINC = -I/usr/include/freetype2
XFTLIBS  = -lXft

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# inih flags
INIFLAGS = -DINI_ALLOW_MULTILINE=0

# includes and libs
INCS = -I${X11INC} $(shell pkg-config --cflags dbus-1) ${XFTINC}
LIBS = -L${X11LIB} -lX11 -lXss ${XFTLIBS} ${XINERAMALIBS} $(shell pkg-config --libs dbus-1)

# flags
CPPFLAGS = -D_BSD_SOURCE -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${INIFLAGS}
CFLAGS   = -g -ansi -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = cc
