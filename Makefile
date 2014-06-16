# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

SRC = x.c  \
	  dunst.c \
	  dbus.c  \
	  utils.c \
	  option_parser.c \
	  settings.c \
	  rules.c \
	  menu.c \
	  notification.c
OBJ = ${SRC:.c=.o}

V ?= 0
ifeq (${V}, 0)
.SILENT:
endif

all: doc options dunst service

options:
	@echo dunst build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC -c $<
	${CC} -c $< ${CFLAGS}

${OBJ}: config.h config.mk

config.h: config.def.h
	@if test -s $@; then echo $< is newer than $@, merge and save $@. If you haven\'t edited $@ you can just delete it	&& exit 1; fi
	@echo creating $@ from $<
	@cp $< $@

dunst: ${OBJ}
	@echo "${CC} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}"
	@${CC} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}

dunstify:
	@${CC} -o $@ dunstify.c -std=c99 $(shell pkg-config --libs --cflags glib-2.0 libnotify) 

debug: ${OBJ}
	@echo CC -o $@
	@${CC} ${CFLAGS} -O0 -o dunst ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	rm -f ${OBJ}
	rm -f dunst
	rm -f dunst.1
	rm -f org.knopwob.dunst.service
	rm -f core
	rm -f dunstify

doc: dunst.1
dunst.1: README.pod
	pod2man --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@

service:
	@sed "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service

install: all
	@echo installing executables to ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dunst ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dunst
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f dunst.1 ${DESTDIR}${MANPREFIX}/man1/
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dunst.1
	mkdir -p "${DESTDIR}${PREFIX}/share/dunst"
	 cp -f dunstrc ${DESTDIR}${PREFIX}/share/dunst
	mkdir -p "${DESTDIR}${PREFIX}/share/dbus-1/services/"
	cp -vf org.knopwob.dunst.service "${DESTDIR}${PREFIX}/share/dbus-1/services/org.knopwob.dunst.service"

uninstall:
	@echo removing executables from ${DESTDIR}${PREFIX}/bin
	rm -f ${DESTDIR}${PREFIX}/bin/dunst
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	rm -f ${DESTDIR}${MANPREFIX}/man1/dunst
	rm -f ${DESTDIR}${PREFIX}/share/dbus-1/service/org.knopwob.dunst.service

.PHONY: all options clean dist install uninstall
