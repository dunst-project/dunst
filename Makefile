# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

VERSION := "1.2.0-non-git"
ifneq ($(wildcard ./.git/.),)
VERSION := $(shell git describe --tags)
endif

LIBS := $(shell pkg-config --libs   ${pkg_config_packs})
INCS := $(shell pkg-config --cflags ${pkg_config_packs})

ifneq (clean, $(MAKECMDGOALS))
ifeq ($(and $(INCS),$(LIBS)),)
$(error "pkg-config failed!")
endif
endif

CFLAGS += -I. ${INCS}
LDFLAGS+= -L. ${LIBS}

SRC := $(sort $(shell find src/ -name '*.c'))
OBJ := ${SRC:.c=.o}
TEST_SRC := $(sort $(shell find test/ -name '*.c'))
TEST_OBJ := $(TEST_SRC:.c=.o)

.PHONY: all debug
all: doc dunst service

debug: CFLAGS   += ${CFLAGS_DEBUG}
debug: LDFLAGS  += ${LDFLAGS_DEBUG}
debug: CPPFLAGS += ${CPPFLAGS_DEBUG}
debug: all

.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

${OBJ}: config.mk

dunst: ${OBJ} main.o
	${CC} ${CFLAGS} -o $@ ${OBJ} main.o ${LDFLAGS}

dunstify: dunstify.o
	${CC} ${CFLAGS} -o $@ dunstify.o ${LDFLAGS}

.PHONY: test
test: test/test
	cd test && ./test

test/test: ${OBJ} ${TEST_OBJ}
	${CC} ${CFLAGS} -o $@ ${TEST_OBJ} ${OBJ} ${LDFLAGS}

.PHONY: doc
doc: docs/dunst.1
docs/dunst.1: docs/dunst.pod
	pod2man --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@

.PHONY: service
service:
	@sed "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service
	@sed "s|##PREFIX##|$(PREFIX)|" dunst.systemd.service.in > dunst.systemd.service

.PHONY: clean clean-dunst clean-dunstify clean-doc clean-tests
clean: clean-dunst clean-dunstify clean-doc clean-tests

clean-dunst:
	rm -f dunst ${OBJ} main.o
	rm -f org.knopwob.dunst.service
	rm -f dunst.systemd.service

clean-dunstify:
	rm -f dunstify.o
	rm -f dunstify

clean-doc:
	rm -f docs/dunst.1

clean-tests:
	rm -f test/test test/*.o

.PHONY: install install-dunst install-doc install-service uninstall
install: install-dunst install-doc install-service

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
	install -Dm644 dunst.systemd.service ${DESTDIR}${PREFIX}/lib/systemd/user/dunst.service

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dunst
	rm -f ${DESTDIR}${MANPREFIX}/man1/dunst.1
	rm -f ${DESTDIR}${PREFIX}/share/dbus-1/services/org.knopwob.dunst.service
	rm -f ${DESTDIR}${PREFIX}/lib/systemd/user/dunst.service
	rm -rf ${DESTDIR}${PREFIX}/share/dunst
