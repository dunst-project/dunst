# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

VERSION := "1.2.0-non-git"
ifneq ($(wildcard ./.git/.),)
VERSION := $(shell git describe --tags)
endif

ifeq (,${SYSTEMD})
# Check for systemctl to avoid discrepancies on systems, where
# systemd is installed, but systemd.pc is in another package 
systemctl := $(shell command -v systemctl >/dev/null && echo systemctl)
ifeq (systemctl,${systemctl})
SYSTEMD := 1
else
SYSTEMD := 0
endif
endif

SERVICEDIR_DBUS ?= $(shell pkg-config dbus-1 --variable=session_bus_services_dir)
SERVICEDIR_DBUS := ${SERVICEDIR_DBUS}
ifeq (,${SERVICEDIR_DBUS})
$(error "Failed to query pkg-config for package 'dbus-1'!")
endif

ifneq (0,${SYSTEMD})
SERVICEDIR_SYSTEMD ?= $(shell pkg-config systemd --variable=systemduserunitdir)
SERVICEDIR_SYSTEMD := ${SERVICEDIR_SYSTEMD}
ifeq (,${SERVICEDIR_SYSTEMD})
$(error "Failed to query pkg-config for package 'systemd'!")
endif
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

.PHONY: test test-valgrind
test: test/test
	cd test && ./test

test-valgrind: test/test
	cd ./test \
		&& valgrind \
			--suppressions=../.valgrind.suppressions \
			--leak-check=full \
			--show-leak-kinds=definite \
			--errors-for-leak-kinds=definite \
			--error-exitcode=123 \
			./test

test/test: ${OBJ} ${TEST_OBJ}
	${CC} ${CFLAGS} -o $@ ${TEST_OBJ} ${OBJ} ${LDFLAGS}

.PHONY: doc
doc: docs/dunst.1
docs/dunst.1: docs/dunst.pod
	pod2man --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@

.PHONY: service service-dbus service-systemd
service: service-dbus
service-dbus:
	@sed "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service
ifneq (0,${SYSTEMD})
service: service-systemd
service-systemd:
	@sed "s|##PREFIX##|$(PREFIX)|" dunst.systemd.service.in > dunst.systemd.service
endif

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

.PHONY: install install-dunst install-doc \
        install-service install-service-dbus install-service-systemd \
        uninstall \
        uninstall-service uninstall-service-dbus uninstall-service-systemd
install: install-dunst install-doc install-service

install-dunst: dunst doc
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m755 dunst ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	install -m644 docs/dunst.1 ${DESTDIR}${MANPREFIX}/man1

install-doc:
	mkdir -p ${DESTDIR}${PREFIX}/share/dunst
	install -m644 dunstrc ${DESTDIR}${PREFIX}/share/dunst

install-service: service install-service-dbus
install-service-dbus:
	install -Dm644 org.knopwob.dunst.service ${DESTDIR}${SERVICEDIR_DBUS}/org.knopwob.dunst.service
ifneq (0,${SYSTEMD})
install-service: install-service-systemd
install-service-systemd:
	install -Dm644 dunst.systemd.service ${DESTDIR}${SERVICEDIR_SYSTEMD}/dunst.service
endif

uninstall: uninstall-service
	rm -f ${DESTDIR}${PREFIX}/bin/dunst
	rm -f ${DESTDIR}${MANPREFIX}/man1/dunst.1
	rm -rf ${DESTDIR}${PREFIX}/share/dunst

uninstall-service: uninstall-service-dbus
uninstall-service-dbus:
	rm -f ${DESTDIR}${SERVICEDIR_DBUS}/org.knopwob.dunst.service

ifneq (0,${SYSTEMD})
uninstall-service: uninstall-service-systemd
uninstall-service-systemd:
	rm -f ${DESTDIR}${SERVICEDIR_SYSTEMD}/dunst.service
endif
