# dunst - Notification-daemon
# See LICENSE file for copyright and license details.

include config.mk

VERSION := "1.4.1-non-git"
ifneq ($(wildcard ./.git/),)
VERSION := $(shell ${GIT} describe --tags)
endif

ifeq (,${SYSTEMD})
# Check for systemctl to avoid discrepancies on systems, where
# systemd is installed, but systemd.pc is in another package
systemctl := $(shell command -v ${SYSTEMCTL} >/dev/null && echo systemctl)
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

CFLAGS  := ${DEFAULT_CPPFLAGS} ${CPPFLAGS} ${DEFAULT_CFLAGS} ${CFLAGS} ${INCS} -MMD -MP
LDFLAGS := ${DEFAULT_LDFLAGS} ${LDFLAGS} ${LIBS}

SRC := $(sort $(shell ${FIND} src/ -name '*.c'))
OBJ := ${SRC:.c=.o}
TEST_SRC := $(sort $(shell ${FIND} test/ -name '*.c'))
TEST_OBJ := $(TEST_SRC:.c=.o)
DEPS := ${SRC:.c=.d} ${TEST_SRC:.c=.d}


.PHONY: all debug
all: doc dunst service

debug: CFLAGS   += ${CPPFLAGS_DEBUG} ${CFLAGS_DEBUG}
debug: LDFLAGS  += ${LDFLAGS_DEBUG}
debug: CPPFLAGS += ${CPPFLAGS_DEBUG}
debug: all

-include $(DEPS)

${OBJ} ${TEST_OBJ}: Makefile config.mk

%.o: %.c
	${CC} -o $@ -c $< ${CFLAGS}

dunst: ${OBJ} main.o
	${CC} -o ${@} ${OBJ} main.o ${CFLAGS} ${LDFLAGS}

dunstify: dunstify.o
	${CC} -o ${@} dunstify.o ${CFLAGS} ${LDFLAGS}

.PHONY: test test-valgrind test-coverage
test: test/test clean-coverage-run
	./test/test -v

test-valgrind: test/test
	${VALGRIND} \
		--suppressions=.valgrind.suppressions \
		--leak-check=full \
		--show-leak-kinds=definite \
		--errors-for-leak-kinds=definite \
		--num-callers=40 \
		--error-exitcode=123 \
		./test/test -v

test-coverage: CFLAGS += -fprofile-arcs -ftest-coverage -O0
test-coverage: test

test-coverage-report: test-coverage
	mkdir -p docs/internal/coverage
	${GCOVR} \
		-r . \
		--exclude=test \
		--html \
		--html-details \
		-o docs/internal/coverage/index.html

test/%.o: test/%.c src/%.c
	${CC} -o $@ -c $< ${CFLAGS}

test/test: ${OBJ} ${TEST_OBJ}
	${CC} -o ${@} ${TEST_OBJ} $(filter-out ${TEST_OBJ:test/%=src/%},${OBJ}) ${CFLAGS} ${LDFLAGS}

.PHONY: doc doc-doxygen
doc: docs/dunst.1
docs/dunst.1: docs/dunst.pod
	${POD2MAN} --name=dunst -c "Dunst Reference" --section=1 --release=${VERSION} $< > $@
doc-doxygen:
	${DOXYGEN} docs/internal/Doxyfile

.PHONY: service service-dbus service-systemd
service: service-dbus
service-dbus:
	@${SED} "s|##PREFIX##|$(PREFIX)|" org.knopwob.dunst.service.in > org.knopwob.dunst.service
ifneq (0,${SYSTEMD})
service: service-systemd
service-systemd:
	@${SED} "s|##PREFIX##|$(PREFIX)|" dunst.systemd.service.in > dunst.systemd.service
endif

.PHONY: clean clean-dunst clean-dunstify clean-doc clean-tests clean-coverage clean-coverage-run
clean: clean-dunst clean-dunstify clean-doc clean-tests clean-coverage clean-coverage-run

clean-dunst:
	rm -f dunst ${OBJ} main.o main.d ${DEPS}
	rm -f org.knopwob.dunst.service
	rm -f dunst.systemd.service

clean-dunstify:
	rm -f dunstify.o
	rm -f dunstify.d
	rm -f dunstify

clean-doc:
	rm -f docs/dunst.1
	rm -fr docs/internal/html
	rm -fr docs/internal/coverage

clean-tests:
	rm -f test/test test/*.o test/*.d

clean-coverage: clean-coverage-run
	${FIND} . -type f -name '*.gcno' -delete
	${FIND} . -type f -name '*.gcna' -delete
# Cleans the coverage data before every run to not double count any lines
clean-coverage-run:
	${FIND} . -type f -name '*.gcov' -delete
	${FIND} . -type f -name '*.gcda' -delete

.PHONY: install install-dunst install-doc \
        install-service install-service-dbus install-service-systemd \
        uninstall \
        uninstall-service uninstall-service-dbus uninstall-service-systemd
install: install-dunst install-doc install-service

install-dunst: dunst doc
	install -Dm755 dunst ${DESTDIR}${BINDIR}/dunst
	install -Dm644 docs/dunst.1 ${DESTDIR}${MANPREFIX}/man1/dunst.1

install-doc:
	install -Dm644 dunstrc ${DESTDIR}${DATADIR}/dunst/dunstrc

install-service: install-service-dbus
install-service-dbus: service-dbus
	install -Dm644 org.knopwob.dunst.service ${DESTDIR}${SERVICEDIR_DBUS}/org.knopwob.dunst.service
ifneq (0,${SYSTEMD})
install-service: install-service-systemd
install-service-systemd: service-systemd
	install -Dm644 dunst.systemd.service ${DESTDIR}${SERVICEDIR_SYSTEMD}/dunst.service
endif

uninstall: uninstall-service
	rm -f ${DESTDIR}${BINDIR}/dunst
	rm -f ${DESTDIR}${MANPREFIX}/man1/dunst.1
	rm -rf ${DESTDIR}${DATADIR}/dunst

uninstall-service: uninstall-service-dbus
uninstall-service-dbus:
	rm -f ${DESTDIR}${SERVICEDIR_DBUS}/org.knopwob.dunst.service

ifneq (0,${SYSTEMD})
uninstall-service: uninstall-service-systemd
uninstall-service-systemd:
	rm -f ${DESTDIR}${SERVICEDIR_SYSTEMD}/dunst.service
endif
