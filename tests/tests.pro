include(../config.pri)

TEMPLATE = subdirs
CONFIG += ordered

# One binary per unit under test. Keep this list alphabetical.
SUBDIRS = \
    ut_evdevbits \
    ut_jsonparams \
    ut_keyoverridedata \
    ut_mattributeextensionid \
    ut_mattributeextensionmanager \
    ut_mimonscreenplugins \
    ut_mimserveroptions \
    ut_mimsettings \
    ut_settingdata \
    ut_sharedattributeextensionmanager \
    ut_windowgroup \
    ut_xkbmodifiers \

# "make check" recurses into every subdirectory.
QMAKE_EXTRA_TARGETS += check
check.target = check
check.CONFIG = recursive

OTHER_FILES += tests.pri README.md
