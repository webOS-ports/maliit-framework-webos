# Common configuration for the unit tests.
#
# A test .pro includes this after setting TARGET, and gets: Qt Test, the
# framework libraries, an "install into the test directory" rule, and a
# "make check" hook that runs the binary in place.
#
# The suite is built only when qmake is run without CONFIG+=notests. The OE
# recipe passes notests for target images, so nothing here reaches a device
# unless it is asked for explicitly (see tests/README.md).

TOP_DIR = ../..

include($$TOP_DIR/config.pri)

TEMPLATE = app
CONFIG -= app_bundle
CONFIG += console
QT += core gui testlib

include($$TOP_DIR/src/libmaliit-plugins.pri)
include($$TOP_DIR/common/libmaliit-common.pri)

# The connection library itself is not linked: the only thing the tests take
# from it is a header-only helper, and linking it would drag wayland and
# xkbcommon into every test binary.
INCLUDEPATH += $$TOP_DIR/src $$TOP_DIR/common $$TOP_DIR/connection

DESTDIR = $$OUT_PWD

# So the binaries find libmaliit-plugins in the build tree rather than needing
# it installed first.
QMAKE_RPATHDIR += $$absolute_path($$TOP_DIR/lib, $$OUT_PWD)

# Tests run headless: the ones that touch QWindow need a platform plugin that
# does not want a display, and the ones that do not are unaffected by it.
check.target = check
check.commands = QT_QPA_PLATFORM=offscreen ./$$TARGET
check.depends = $$TARGET
QMAKE_EXTRA_TARGETS += check

target.path = $$MALIIT_TESTS_DIR/$$TARGET
INSTALLS += target
