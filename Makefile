# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

VERSION := "1.3.2-non-git"
ifneq ($(wildcard ./.git/),)
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

SERVICEDIR_DBUS ?= $(shell $(PKG_CONFIG) dbus-1 --variable=session_bus_services_dir)
SERVICEDIR_DBUS := ${SERVICEDIR_DBUS}
ifeq (,${SERVICEDIR_DBUS})
$(error "Failed to query $(PKG_CONFIG) for package 'dbus-1'!")
endif

ifneq (0,${SYSTEMD})
SERVICEDIR_SYSTEMD ?= $(shell $(PKG_CONFIG) systemd --variable=systemduserunitdir)
SERVICEDIR_SYSTEMD := ${SERVICEDIR_SYSTEMD}
ifeq (,${SERVICEDIR_SYSTEMD})
$(error "Failed to query $(PKG_CONFIG) for package 'systemd'!")
endif
endif

LIBS := $(shell $(PKG_CONFIG) --libs   ${pkg_config_packs})
INCS := $(shell $(PKG_CONFIG) --cflags ${pkg_config_packs})

ifneq (clean, $(MAKECMDGOALS))
ifeq ($(and $(INCS),$(LIBS)),)
$(error "$(PKG_CONFIG) failed!")
endif
endif

CFLAGS  := ${DEFAULT_CPPFLAGS} ${CPPFLAGS} ${DEFAULT_CFLAGS} ${CFLAGS} -I. ${INCS}
LDFLAGS := ${DEFAULT_LDFLAGS} ${LDFLAGS} -L. ${LIBS}

SRC := $(sort $(shell find src/ -name '*.c'))
OBJ := ${SRC:.c=.o}
TEST_SRC := $(sort $(shell find test/ -name '*.c'))
TEST_OBJ := $(TEST_SRC:.c=.o)

.PHONY: all debug
all: doc dunst service

debug: CFLAGS   += ${CPPFLAGS_DEBUG} ${CFLAGS_DEBUG}
debug: LDFLAGS  += ${LDFLAGS_DEBUG}
debug: CPPFLAGS += ${CPPFLAGS_DEBUG}
debug: all

.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

${OBJ}: config.mk

dunst: ${OBJ} main.o
	${CC} -o ${@} ${OBJ} main.o ${CFLAGS} ${LDFLAGS}

dunstify: dunstify.o
	${CC} -o ${@} dunstify.o ${CFLAGS} ${LDFLAGS}

.PHONY: test test-valgrind test-coverage
test: test/test clean-coverage-run
	cd test && ./test

test-valgrind: test/test
	cd ./test \
		&& valgrind \
			--suppressions=../.valgrind.suppressions \
			--leak-check=full \
			--show-leak-kinds=definite \
			--errors-for-leak-kinds=definite \
			--num-callers=40 \
			--error-exitcode=123 \
			./test

test-coverage: CFLAGS += -fprofile-arcs -ftest-coverage -O0
test-coverage: test

test-coverage-report: test-coverage
	mkdir -p docs/internal/coverage
	gcovr \
		-r . \
		--exclude=test \
		--html \
		--html-details \
		-o docs/internal/coverage/index.html

test/test: ${OBJ} ${TEST_OBJ}
	${CC} -o ${@} ${TEST_OBJ} ${OBJ} ${CFLAGS} ${LDFLAGS}

.PHONY: doc doc-doxygen
doc: docs/dunst.1
docs/dunst.1: docs/dunst.pod
	pod2man --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@
doc-doxygen:
	doxygen docs/internal/Doxyfile

.PHONY: service service-dbus service-systemd
service: service-dbus
service-dbus:
	@sed "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service
ifneq (0,${SYSTEMD})
service: service-systemd
service-systemd:
	@sed "s|##PREFIX##|$(PREFIX)|" dunst.systemd.service.in > dunst.systemd.service
endif

.PHONY: clean clean-dunst clean-dunstify clean-doc clean-tests clean-coverage clean-coverage-run
clean: clean-dunst clean-dunstify clean-doc clean-tests clean-coverage clean-coverage-run

clean-dunst:
	rm -f dunst ${OBJ} main.o
	rm -f org.knopwob.dunst.service
	rm -f dunst.systemd.service

clean-dunstify:
	rm -f dunstify.o
	rm -f dunstify

clean-doc:
	rm -f docs/dunst.1
	rm -fr docs/internal/html
	rm -fr docs/internal/coverage

clean-tests:
	rm -f test/test test/*.o

clean-coverage: clean-coverage-run
	find . -type f -name '*.gcno' -delete
	find . -type f -name '*.gcna' -delete
# Cleans the coverage data before every run to not double count any lines
clean-coverage-run:
	find . -type f -name '*.gcov' -delete
	find . -type f -name '*.gcda' -delete

.PHONY: install install-dunst install-doc \
        install-service install-service-dbus install-service-systemd \
        uninstall \
        uninstall-service uninstall-service-dbus uninstall-service-systemd
install: install-dunst install-doc install-service

install-dunst: dunst doc
	install -Dm755 dunst ${DESTDIR}${PREFIX}/bin/dunst
	install -Dm644 docs/dunst.1 ${DESTDIR}${MANPREFIX}/man1/dunst.1

install-doc:
	install -Dm644 dunstrc ${DESTDIR}${PREFIX}/share/dunst/dunstrc

install-service: install-service-dbus
install-service-dbus: service-dbus
	install -Dm644 org.knopwob.dunst.service ${DESTDIR}${SERVICEDIR_DBUS}/org.knopwob.dunst.service
ifneq (0,${SYSTEMD})
install-service: install-service-systemd
install-service-systemd: service-systemd
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
