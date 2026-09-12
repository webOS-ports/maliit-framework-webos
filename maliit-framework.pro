include(./config.pri)

!isEmpty(HELP) {
    # Output help
    help_string = \
        Important build options: \
        \\n\\t PREFIX : Install prefix (default: /usr) \
        \\n\\t {BIN,LIB,INCLUDE,DOC}DIR : Install prefix for specific types of files \
        \\n\\t MALIIT_DEFAULT_PLUGIN : Default onscreen (virtual) keyboard plugin \
        \\n\\t MALIIT_DEFAULT_HW_PLUGIN : Default hardware keyboard plugin \
        \\n\\t MALIIT_SERVER_ARGUMENTS : Arguments to use for starting maliit-server by D-Bus activation \
        \\nRecognised CONFIG flags: \
        \\n\\t enable-pmloglib : Find and use pmloglib for logging if exists \
        \\n\\t local-install : Install everything underneath PREFIX, nothing to system directories reported by GTK+, Qt etc. \
        \\n\\t wayland : Compile with support for wayland \
        \\nInfluential environment variables: \
        \\n\\t PKG_CONFIG_PATH : Override standard directories to look for pkg-config information \
        \\nExamples: \
        \\n\\t qmake \
        \\n\\t qmake PREFIX=/usr LIBDIR=/usr/lib64 CONFIG+=wayland \
        \\n\\t qmake PREFIX=/usr MALIIT_DEFAULT_PLUGIN=libmykeyboard.so

    !build_pass:system(echo -e \"$$help_string\")
} else {
    config_string = Tip: Run qmake HELP=1 for a list of all supported build options

    !build_pass:system(echo -e \"$$config_string\")
}

CONFIG += ordered
TEMPLATE = subdirs

SUBDIRS = common

contains(QT_MAJOR_VERSION, 4) {
    error("Qt 5 is required. For the Qt 4 input context see maliit-inputcontext-qt4. For a Qt 4 Maliit please use the 0.81 or 0.94-qt4 branches/release series instead")
} else {
    SUBDIRS += connection src passthroughserver

    # The unit tests link against the libraries above, so they come last. They
    # are skipped entirely for CONFIG+=notests, which is what target image
    # builds pass; see tests/README.md for how to build and run them.
    !notests {
        SUBDIRS += tests
    }
}

# LS2 identity for MaliitServer: the role, service, permission, api and groups
# files under service/ describe this repo's own binary, so they live here
# rather than in whichever component happens to supply the keyboard plugin.
# webos-service.prf (qt-features-webos) substitutes the *.in files and installs
# all five into the luna-service2 directories; the manifest is generated from
# them at package time. SBINDIR and MALIIT_SERVER come from config.pri, which
# is included at the top of this file, and match passthroughserver.pro's
# target.path so the paths cannot drift apart.
CONFIG += webos-service
WEBOS_SYSBUS_DIR = service

QMAKE_EXTRA_TARGETS += check-xml
check-xml.target = check-xml
check-xml.CONFIG = recursive

QMAKE_EXTRA_TARGETS += check
check.target = check
check.CONFIG = recursive

DIST_NAME = $$MALIIT_PACKAGENAME-$$MALIIT_VERSION
DIST_PATH = $$OUT_PWD/$$DIST_NAME
TARBALL_SUFFIX = .tar.bz2
TARBALL_PATH = $$DIST_PATH$$TARBALL_SUFFIX

# The 'make dist' target
# Creates a tarball
QMAKE_EXTRA_TARGETS += dist
dist.target = dist
dist.commands += git archive HEAD --prefix=$$DIST_NAME/ | bzip2 > $$TARBALL_PATH;
dist.commands += md5sum $$TARBALL_PATH | cut -d \' \' -f 1 > $$DIST_PATH\\.md5

OTHER_FILES += NEWS README INSTALL.local
