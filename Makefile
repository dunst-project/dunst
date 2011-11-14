# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

SRC = draw.c dunst.c
OBJ = ${SRC:.c=.o}

all: options dunst

options:
	@echo dunst build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC -c $<
	@${CC} -c $< ${CFLAGS}

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

dunst: draw.o dunst.o
	@echo CC -o $@
	@${CC} ${CFLAGS} -o $@ dunst.o draw.o ${LDFLAGS} 

clean:
	@echo cleaning
	@rm -f ${OBJ}
	@rm -f dunst

install: all
	@echo installing executables to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dunst ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dunst
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f dunst.1 ${DESTDIR}${MANPREFIX}/man1/
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/dunst.1

uninstall:
	@echo removing executables from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/dunst
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/dunst

.PHONY: all options clean dist install uninstall
