/* @@@LICENSE
*
*      Copyright (c) 2013-2019 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#ifndef IMELUNASERVICE_H
#define IMELUNASERVICE_H

#include <QtCore>
#include <QJsonObject>
#include <QHash>
#include <QSharedPointer>
#include "glib.h"
#include "luna-service2/lunaservice.h"

class MInputContextConnection;
class MIMPluginManager;

struct RemoteKeyboardClient
{
    QString token;
};

class IMELunaService : public QObject
{
    Q_OBJECT

public:
    /*! \param pluginManager the manager, for the hardware keyboard status and the
     *         on-screen keyboard override. May be null, in which case those two
     *         methods report that they are unavailable rather than crashing.
     */
    explicit IMELunaService(QSharedPointer<MInputContextConnection> connection,
                           MIMPluginManager *pluginManager = nullptr);
    virtual ~IMELunaService();

protected Q_SLOTS:
    void onWidgetStateChanged(unsigned int clientId, const QMap<QString, QVariant> &newState,
                              const QMap<QString, QVariant> &oldState, bool focusChanged);

    void onReset();

    //! Tells subscribers when the keyboard status moves, skipping the repeats:
    //! updateInputSource() runs for reasons that have nothing to do with this.
    //! Called for a focus change as well as a hardware one, because inputFocus
    //! is part of that status - hence the name without "hardware" in it.
    void onKeyboardStatusChanged();

protected:
    enum DeleteMode { BackspaceMode, DirectMode, MixedMode };

    void startService();
    void broadcastWidgetState();
    void broadcastToSubscribers(const QJsonObject& response);
    bool hasSubscribers() const;

    QJsonObject getWidgetStateJson() const;
    QJsonObject getKeyboardStatusJson() const;
    void insertText(const QString& text, bool replace, int length = 0);
    bool applySpellingSuggestion(const QString& suggestion);
    void deleteCharacters(int numChars, DeleteMode mode);
    void sendEnterKey();

    static bool handleRegisterRemoteKeyboard(LSHandle *handle, LSMessage *message, void *data);
    static bool handleInsertText(LSHandle *handle, LSMessage *message, void *data);
    static bool handleApplySpellingSuggestion(LSHandle *handle, LSMessage *message, void *data);
    static bool handleDeleteCharacters(LSHandle *handle, LSMessage *message, void *data);
    static bool handleSendEnterKey(LSHandle *handle, LSMessage *message, void *data);
    static bool handleGetKeyboardStatus(LSHandle *handle, LSMessage *message, void *data);
    static bool handleSetOnScreenKeyboardForced(LSHandle *handle, LSMessage *message, void *data);
    static bool handleSetHardwareKeyboardLayout(LSHandle *handle, LSMessage *message, void *data);
    static bool handleSetTelephoneKeypadCounts(LSHandle *handle, LSMessage *message, void *data);

    static bool handleSubscriptionCancel(LSHandle *handle, LSMessage *message, void *data);

    static LSMethod ime_bus_methods[];

    static const char *SubscriberKey;
    //! A key of its own: a remote keyboard subscribed for widget state has no
    //! use for keyboard status messages, and vice versa.
    static const char *KeyboardStatusSubscriberKey;

    QSharedPointer<MInputContextConnection> m_connection;
    QPointer<MIMPluginManager> m_pluginManager;
    GMainLoop *m_mainLoop;
    LSHandle *m_handle;

    bool m_focusChangedSinceLastBroadcast;
    QJsonObject m_lastWidgetState;
    QJsonObject m_lastKeyboardStatus;
    QTimer *m_broadcastTimer;

    QHash<QString, QSharedPointer<RemoteKeyboardClient> > m_clientByToken;
};

#endif // IMELUNASERVICE_H5
