# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

CFLAGS += -I.
LDFLAGS += -L.

SRC = $(shell ls src/*.c)
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
	${CC} -o $@ -c $< ${CFLAGS}

${OBJ}: config.h config.mk

config.h: config.def.h
	@if test -s $@; then echo $< is newer than $@, merge and save $@. If you haven\'t edited $@ you can just delete it	&& exit 1; fi
	@echo creating $@ from $<
	@cp $< $@

dunst: options ${OBJ} main.o
	@echo "${CC} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}"
	@${CC} ${CFLAGS} -o $@ ${OBJ} main.o ${LDFLAGS}

clean-dunst:
	rm -f dunst ${OBJ} main.o
	rm -f org.knopwob.dunst.service
	rm -f dunst.systemd.service

clean-dunstify:
	rm -f dunstify.o
	rm -f dunstify

clean-doc:
	rm -f docs/dunst.1

clean: clean-dunst clean-dunstify clean-doc test-clean

distclean: clean clean-config

clean-config:
	rm -f config.h

doc: docs/dunst.1
docs/dunst.1: docs/dunst.pod
	pod2man --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@

service:
	@sed "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service
	@sed "s|##PREFIX##|$(PREFIX)|" contrib/dunst.systemd.service.in > dunst.systemd.service

install-dunst: dunst doc
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m755 dunst ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	install -m644 docs/dunst.1 ${DESTDIR}${MANPREFIX}/man1

install-doc:
	mkdir -p ${DESTDIR}${PREFIX}/share/dunst
	install -m644 dunstrc ${DESTDIR}${PREFIX}/share/dunst

install-service: service
	mkdir -p ${DESTDIR}${PREFIX}/share/dbus-1/services/
	install -m644 org.knopwob.dunst.service ${DESTDIR}${PREFIX}/share/dbus-1/services

install: install-dunst install-doc install-service

uninstall:
	@echo Removing executables from ${DESTDIR}${PREFIX}/bin
	rm -f ${DESTDIR}${PREFIX}/bin/dunst
	@echo Removing manual page from ${DESTDIR}${MANPREFIX}/man1
	rm -f ${DESTDIR}${MANPREFIX}/man1/dunst.1
	@echo Removing service file from ${DESTDIR}${PREFIX}/share/dbus-1/services
	rm -f ${DESTDIR}${PREFIX}/share/dbus-1/services/org.knopwob.dunst.service
	@echo Removing documentation directory ${DESTDIR}${PREFIX}/share/dunst
	rm -rf ${DESTDIR}${PREFIX}/share/dunst

test: test/test
	cd test && ./test

TEST_SRC = $(shell ls test/*.c)
TEST_OBJ = $(TEST_SRC:.c=.o)

test/test: ${OBJ} ${TEST_OBJ}
	${CC} ${CFLAGS} -o $@ ${TEST_OBJ} ${OBJ} ${LDFLAGS}

test-clean:
	rm -f test/test test/*.o

dunstify: dunstify.o
	${CC} ${CFLAGS} -o $@ dunstify.o $(shell pkg-config --libs --cflags glib-2.0 libnotify gdk-2.0)

.PHONY: all options clean dist install uninstall
