/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Copyright (C) 2015-2021 LG Electronics, Inc.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#ifndef MIMPLUGINMANAGER_H
#define MIMPLUGINMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>

#include <maliit/namespace.h>

#include <maliit/plugins/plugindescription.h>
#include <maliit/plugins/subviewdescription.h>

#include "mattributeextensionid.h"
#include "minputcontextconnection.h"

class QRegion;
class MIMPluginManagerPrivate;
class MAttributeExtensionId;
class MAbstractInputMethod;
class MAttributeExtensionManager;

namespace Maliit {

class AbstractPlatform;

namespace Plugins {
    class AbstractPluginSetting;
}
}
using Maliit::Plugins::AbstractPluginSetting;

//! \internal
/*! \ingroup maliitserver
 * \brief Manager of MInputMethodPlugin instances.
 * \note this class is not considered stable API
 */
class MIMPluginManager: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.meego.inputmethodpluginmanager1")

public:
    /*!
     * \Brief Constructs object MIMPluginManager
     */
    MIMPluginManager(const QSharedPointer<MInputContextConnection> &icConnection,
                     const QSharedPointer<Maliit::AbstractPlatform> &platform);

    virtual ~MIMPluginManager();

    //! Returns names of loaded plugins
    QStringList loadedPluginsNames() const;

    //! Returns names of loaded plugins which support \a state
    QStringList loadedPluginsNames(Maliit::HandlerState state) const;

    //! \brief Return information about loaded input method plugins which could work in specified \a state.
    QList<MImPluginDescription> pluginDescriptions(Maliit::HandlerState state) const;

    //! \brief Return information about previous, current and next subviews.
    //!
    //! \sa MAbstractInputMethodHost::surroundingSubViewDescriptions()
    QList<MImSubViewDescription> surroundingSubViewDescriptions(Maliit::HandlerState state) const;

    //! Returns names of activated plugins
    QStringList activePluginsNames() const;

    //! Returns names of activated plugin for \a state
    QString activePluginsName(Maliit::HandlerState state) const;

    //! Returns all subviews (IDs and titles) of loaded plugins which support \a state.
    QMap<QString, QString> availableSubViews(const QString &plugin,
                                             Maliit::HandlerState state
                                              = Maliit::OnScreen) const;

    //! Returns the ID of active subview of the activated plugin for \a state.
    QString activeSubView(Maliit::HandlerState state) const;

    //! Sets \a pluginName as the activated plugin for \a state.
    void setActivePlugin(const QString &pluginName, Maliit::HandlerState state);

    //! Sets \a subViewId as the active subview of the activated plugin for \a state.
    void setActiveSubView(const QString &subViewId, Maliit::HandlerState state);

    //! Switches plugin in according to given \a direction
    void switchPlugin(Maliit::SwitchDirection direction, MAbstractInputMethod *initiator);

    //! Switches active plugin to inactive plugin with given \a name
    void switchPlugin(const QString &name, MAbstractInputMethod *initiator);

    //! Enables all installed subviews
    void setAllSubViewsEnabled(bool enable);

    //! Register a setting entry for the specified plugin
    AbstractPluginSetting *registerPluginSetting(const QString &pluginId,
                                                 const QString &pluginDescription,
                                                 const QString &key,
                                                 const QString &description,
                                                 Maliit::SettingEntryType type,
                                                 const QVariantMap &attributes);

Q_SIGNALS:
    //! Emitted when the hardware keyboard status, or the override above, moves.
    void hardwareKeyboardStatusChanged();

    //! This signal is emitted when input method plugins are loaded, unloaded,
    //! enabled or disabled
    void pluginsChanged();

    void pluginLoaded();

public:
    //! \brief Whether a physical keyboard is attached.
    bool hardwareKeyboardPresent() const;

    //! \brief Whether one is attached and can be typed on right now.
    bool hardwareKeyboardUsable() const;

    //! \brief Whether the keyboard slides or folds away; see
    //!        MImHwKeyboardTracker::hasSwitch().
    bool hardwareKeyboardIsSlider() const;

    /*! \brief Whether the on-screen keyboard has been asked for anyway.
     *
     * A physical keyboard normally takes the on-screen one away. This is the way
     * back for the things it cannot do - an emoji, a script it has no keys for, a
     * key the hardware is missing - and it sticks until it is turned off again,
     * which is how LunaSysMgr's own keyboard key behaved.
     */
    bool onScreenKeyboardForced() const;

    /*! \brief The physical keyboard's layout, as declared for this device.
     *
     * Empty when nothing declared one, which is the normal case. It cannot be
     * worked out instead: evdev scancodes are positional, so a QWERTZ keyboard
     * and a QWERTY one advertise exactly the same keys, and a USB or Bluetooth
     * keyboard carries its layout in the compositor's xkb keymap rather than
     * anywhere this can see. Legacy declared it per device too - the KEYoBRD
     * token - and this is the same thing said in a settings key.
     */
    QString hardwareKeyboardLayout() const;

    /*! \brief The digits printed on the keyboard's key faces, by scancode.
     *
     * Empty unless a plugin has said. For the things that take digits without an
     * input method - the lock screen's PIN pad runs inside the compositor, where
     * no plugin ever sees the keys, so it substitutes them itself.
     */
    QVariantMap hardwareKeyFaceDigits() const;

    //! \brief The misspelled word the caret is in, empty when it is in none.
    QString spellingWord() const;

    //! \brief What the active plugin would put in its place, best first.
    QStringList spellingSuggestions() const;

    //! \brief The layout override, or empty when the hardware decides.
    QString keyboardLayoutOverride() const;

    //! \brief Whether a telephone keypad counts as a hardware keyboard.
    bool telephoneKeypadCounts() const;

    /*! \brief The layout this device declares, ignoring the settings override.
     *
     * Read from a file rather than a setting because it is a fact about the
     * hardware, not a preference: on LuneOS one rootfs boots every device, so it
     * is bind-mounted into place during boot by luneos-device-config's
     * 77-hwkeyboard-layout generator, from deviceinfo_keyboard_layout in the
     * adaptation. The shipped file is all comment, which is how a device that
     * declares nothing says so. MALIIT_HWKEYBOARD_LAYOUT_FILE overrides the
     * path.
     */
    static QString declaredKeyboardLayout();

public Q_SLOTS:
    void setOnScreenKeyboardForced(bool forced);

    /*! \brief Records the layout the active plugin identified, and reports it on.
     *
     * Preferred over the device's declaration because it names the keyboard that
     * is actually attached rather than the one the device usually has, and a
     * plugin only knows it by having matched the input device. Empty clears it.
     */
    void setHardwareKeyboardLayout(const QString &layout);

    //! \brief Records the digits the active plugin read off the key faces.
    void setHardwareKeyFaceDigits(const QVariantMap &digits);

    //! \brief Records the misspelling at the caret, as the active plugin sees it.
    void setSpellingSuggestions(const QString &word, const QStringList &suggestions);

    /*! \brief Overrides the layout the keyboard is taken to have.
     *
     * Empty restores "work it out" - the plugin's answer, then the device's own
     * declaration. For the keyboard nothing can identify: a USB or Bluetooth one
     * carries its layout in the compositor's xkb keymap, which is nowhere this
     * can see, so somebody has to be able to say.
     */
    void setKeyboardLayoutOverride(const QString &layout);

    /*! \brief Whether a telephone keypad counts as a hardware keyboard.
     *
     * Off by default: a keypad has the digits and no letters, so taking the
     * on-screen keyboard away for one leaves no way to type a word. On a device
     * whose keypad is meant to be typed on by multi-tap, it is the whole point.
     */
    void setTelephoneKeypadCounts(bool counts);

    //! Show active plugins.
    void showActivePlugins();

    //! Hide active plugins.
    void hideActivePlugins();

    void resetInputMethods();

private Q_SLOTS:
    void updatePlugins();

    //! Update and activate input source.
    void updateInputSource();

    //! Apply the hardware keyboard detection settings to the tracker.
    void updateHwKeyboardPolicy();

    //! Set toolbar to active plugin with given \a id
    void setToolbar(const MAttributeExtensionId &id);

    //! Update the key overrides for active plugin.
    void updateKeyOverrides();

    void handleAppOrientationChanged(int angle);
    void handleAppOrientationAboutToChange(int angle);
    void handleAppFocusChanged(WId id);

    void handleClientDisconnection();
    void handleClientConnection();
    void handleClientChange();
    void startShutdownTimer();
    void quitByShutdownTimer() const;
    void handleWidgetStateChanged(unsigned int clientId, const QMap<QString, QVariant> &newState,
                                  const QMap<QString, QVariant> &oldState, bool focusChanged);
    void handleMouseClickOnPreedit(const QPoint &pos, const QRect &preeditRect);
    void handlePreeditChanged(const QString &text, int cursorPos);

    void processKeyEvent(QEvent::Type keyType, Qt::Key keyCode,
                         Qt::KeyboardModifiers modifiers, const QString &text, bool autoRepeat,
                         int count, quint32 nativeScanCode, quint32 nativeModifiers, unsigned long time);

    void pluginSettingsRequested(int clientId, const QString &descriptionLanguage);

    /*!
     * \brief Handle global attribute change
     * \param id id of the attribute extension that triggered this change
     * \param targetItem Item name
     * \param attribute Attribute name
     * \param value New attribute value
     */
    void onGlobalAttributeChanged(const MAttributeExtensionId &id,
                                  const QString &targetItem,
                                  const QString &attribute,
                                  const QVariant &value);
private:
    QSet<MAbstractInputMethod *> targets();

protected:
    MIMPluginManagerPrivate *const d_ptr;

private:
    Q_DISABLE_COPY(MIMPluginManager)
    Q_DECLARE_PRIVATE(MIMPluginManager)

    Q_PRIVATE_SLOT(d_func(), void _q_syncHandlerMap(int))
    Q_PRIVATE_SLOT(d_func(), void _q_setActiveSubView(const QString &, Maliit::HandlerState))
    Q_PRIVATE_SLOT(d_func(), void _q_onScreenSubViewChanged())

    friend class Ut_MIMPluginManager;
    friend class Ut_MIMPluginManagerConfig;
    friend class Ft_MIMPluginManager;
    friend class Ut_MIMSettingsDialog;
};

#endif

