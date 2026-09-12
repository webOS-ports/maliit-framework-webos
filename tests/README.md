# Unit tests and hardening harness

The suite covers the parts of MaliitServer that take input from somewhere it
does not control — the LS2 bus, the wayland compositor, plugin settings, the
evdev nodes — plus the bookkeeping around object lifetime that has produced
the crashes worth remembering. It is deliberately not a coverage exercise: a
test is here because the code it drives is either a boundary or a place
something already went wrong.

## Building and running

The tests are built only when qmake is run *without* `CONFIG+=notests`. Target
image builds pass that flag, so nothing here ships to a device unless asked
for.

Natively, with sanitizers on:

```sh
scripts/run-tests.sh
```

That configures into `build-tests/`, builds everything, and runs each test
binary through `make check`. `--no-sanitizers` gives a plain build,
`--build-dir DIR` puts it somewhere else, `--keep` reuses an existing build.

By hand:

```sh
mkdir build && cd build
qmake6 ../maliit-framework.pro CONFIG+=wayland
make -j"$(nproc)"
make -C tests check
```

A single test, with Qt Test's own options:

```sh
QT_QPA_PLATFORM=offscreen ./tests/ut_jsonparams/ut_jsonparams -v2
QT_QPA_PLATFORM=offscreen ./tests/ut_mimsettings/ut_mimsettings testUnset
```

### On a device

The suite links against `libmaliit-plugins`, so it needs the framework's own
dependencies (glib, luna-service2, libudev) but not a running LS2 bus: the
settings tests select `MImSettings::TemporarySettings`, which is backed by a
`QTemporaryFile`. To build it for a target, drop `CONFIG+=notests` from the
recipe's `EXTRA_QMAKEVARS_PRE` and `make install`; the binaries land in
`${MALIIT_TESTS_DIR}` (`/usr/opt/webos/tests/maliit-framework` by default).

## Sanitizers

`run-tests.sh` builds with ASan and UBSan by default. Most of what the suite
asserts is about lifetime and about integer and shift behaviour, and several
of the bugs it pins down were the kind that a passing test would happily miss
without an instrumented build:

* a registry entry that outlived the object it pointed at
* a shift by a modifier index the keymap never defined
* a stack buffer sized from a bit count rather than from what the kernel
  writes into it

`UBSAN_OPTIONS=halt_on_error=1` is set so an overflow fails the run instead of
printing and passing. ASan's leak detection is off: Qt keeps allocations alive
to exit by design and the reports are noise, while its use-after-free and
overflow detection is the point.

## Static analysis

```sh
scripts/static-analysis.sh
```

Runs cppcheck, and clang-tidy when a compilation database is available:

```sh
qmake6 ../maliit-framework.pro CONFIG+=wayland CONFIG+=notests
bear -- make -j"$(nproc)"
scripts/static-analysis.sh --compile-commands .
```

The clang-tidy selection lives in `.clang-tidy` at the top of the tree and is
narrow on purpose — defect classes only, no style checks — so that a finding
is worth reading.

## What each test covers

| Test | Covers |
| --- | --- |
| `ut_jsonparams` | `Maliit::Json::toInt()`: every number reaching MaliitServer from the LS2 bus. NaN, the infinities, values past `INT_MAX`, non-numbers, and that a rejected value leaves the caller's default alone. |
| `ut_xkbmodifiers` | `Maliit::xkbModifierIsSet()`: modifier indices from a keymap, including `XKB_MOD_INVALID` and anything else that cannot be a shift count. |
| `ut_evdevbits` | The evdev bitmap sizing rule, against a model of the kernel's `bits_to_user()`. Includes static assertions, so a wrong answer fails to compile on whichever architecture is being built. |
| `ut_settingdata` | `validateSettingValue()`: type, domain and range checking of plugin-settings writes, and that a malformed constraint rejects rather than degrading to "unconstrained". |
| `ut_mattributeextensionid` | Validity and identity of the key that keeps one client out of another client's attribute extension, including its behaviour as a `QHash` key. |
| `ut_keyoverridedata` | The `createKeyOverride()`/`keyOverride()` pair, and that a fetch for an id that was never created returns null. |
| `ut_mimserveroptions` | Command line parsing, mostly the argument accounting: an option that miscounts what it consumed desynchronises everything after it. |
| `ut_mimsettings` | The QSettings backend's process-wide watcher registry: several watchers on a key, watchers destroyed in either order, and a watcher deleted from inside a notification. |
| `ut_mimonscreenplugins` | Subview enable/available bookkeeping, the `plugin:subview` setting format against malformed entries, and that the enable-all override undoes itself. |
| `ut_attributeextensionmanager` | Per-client attribute extensions: client isolation, disconnect handling, and the handling of nonsense in every argument that arrives from a client. |
| `ut_sharedattributeextensionmanager` | Plugin settings shared across clients: who counts as subscribed, and that a write is validated before it reaches `MImSettings`. |
| `ut_windowgroup` | The guard rails around plugin windows: forced window flags, overruling a plugin that shows itself while inactive, and the input method area the compositor is told about. |

## Adding a test

```sh
mkdir tests/ut_thing
cat > tests/ut_thing/ut_thing.pro <<'PRO'
TARGET = ut_thing
include(../tests.pri)
SOURCES += ut_thing.cpp
PRO
```

Add `ut_thing` to `SUBDIRS` in `tests/tests.pro`, keeping the list
alphabetical. `tests.pri` supplies Qt Test, the framework libraries, the
`check` target and the install rule.

End the test source with `QTEST_GUILESS_MAIN` unless it needs `QWindow`, in
which case use `QTEST_MAIN`; the `check` target already forces the offscreen
platform plugin.
