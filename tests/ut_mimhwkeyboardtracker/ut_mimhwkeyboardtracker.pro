TARGET = ut_mimhwkeyboardtracker
include(../tests.pri)
CONFIG += link_pkgconfig
PKGCONFIG += libudev
SOURCES += ut_mimhwkeyboardtracker.cpp
