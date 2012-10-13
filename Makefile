# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

SRC = draw.c dunst.c list.c dunst_dbus.c ini.c utils.c
OBJ = ${SRC:.c=.o}

all: doc options dunst service

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

dunst: draw.o dunst.o list.o dunst_dbus.o ini.o utils.o
	@echo CC -o $@
	@${CC} ${CFLAGS} -o $@ dunst.o draw.o list.o dunst_dbus.o ini.o utils.o ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f ${OBJ}
	@rm -f dunst
	@rm -f dunst.1
	@rm -f org.knopwob.dunst.service
	@rm -f core

doc: dunst.1
dunst.1: README.pod
	pod2man --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@

service:
	@sed "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service

install: all
	@echo installing executables to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dunst ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dunst
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f dunst.1 ${DESTDIR}${MANPREFIX}/man1/
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/dunst.1
	@mkdir -p "${DESTDIR}${PREFIX}/share/dunst"
	@ cp -f dunstrc ${DESTDIR}${PREFIX}/share/dunst
	@mkdir -p "${DESTDIR}${PREFIX}/share/dbus-1/services/"
	@cp -vf org.knopwob.dunst.service "${DESTDIR}${PREFIX}/share/dbus-1/services/org.knopwob.dunst.service"

uninstall:
	@echo removing executables from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/dunst
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/dunst
	@rm -f ${DESTDIR}${PREFIX}/share/dbus-1/service/org.knopwob.dunst.service

.PHONY: all options clean dist install uninstall
