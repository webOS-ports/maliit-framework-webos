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

#include <algorithm>
#include <climits>

#include <QKeyEvent>
#include "imelunaservice.h"
#include "mimpluginmanager.h"
#include "mimjsonparams.h"
#include "minputcontextconnection.h"
#include "luna-service2/lunaservice.h"
#include "mimglobalsettings.h"

const char *IMELunaService::SubscriberKey = "REMOTE_KEYBOARD_LIST";
const char *IMELunaService::KeyboardStatusSubscriberKey = "KEYBOARD_STATUS_LIST";

#include <QJsonArray>
#include <QJsonObject>
#include <utility>

namespace {

// RAII-style helper that automatically cleans up LSError
class LSErrorWrapper
{
public:
    LSErrorWrapper() { LSErrorInit(&err); }
    ~LSErrorWrapper() { LSErrorFree(&err); }
    operator LSError *() { return &err; }
    QString message() const { return QString(err.message ? err.message : ""); }

    LSError err;

private:
    // The payload is owned by exactly one wrapper; copying would free it twice.
    Q_DISABLE_COPY(LSErrorWrapper)
};

class LSMessageAdapter
{
public:
    LSMessageAdapter(LSMessage *message)
        : m_message(message)
    {
    }

    bool isSubscription() const
    {
        return LSMessageIsSubscription(m_message);
    }

    bool addSubscription(const char *key)
    {
        LSErrorWrapper err;

        if (!LSSubscriptionAdd(LSMessageGetConnection(m_message), key, m_message, err)) {
            qWarning() << "failed to add subscription: " << err.message();
            return false;
        }

        return true;
    }

    QString uniqueToken()
    {
        return QString(LSMessageGetUniqueToken(m_message));
    }

    QJsonObject getPayload() const
    {
        QByteArray data(LSMessageGetPayload(m_message));
        return QJsonDocument::fromJson(data).object();
    }

    void respond(const QJsonObject &response)
    {
        LSErrorWrapper err;

        QJsonDocument document(response);

        if (!LSMessageRespond(m_message, document.toJson().constData(), err)) {
            qWarning() << "failed to reply to LS2 message";
        }
    }

    void replyTrue()
    {
        QJsonObject response;
        response.insert("returnValue", true);
        respond(response);
    }

    void replyError(const QString &errorText = "", int errorCode = -1000)
    {
        QJsonObject response;
        response.insert("returnValue", false);
        response.insert("errorCode", errorCode);
        response.insert("errorText", errorText);

        respond(response);
    }

protected:
    LSMessage *m_message;
};

} // namespace

IMELunaService::IMELunaService(QSharedPointer<MInputContextConnection> connection,
                               MIMPluginManager *pluginManager)
    : m_connection(std::move(connection))
    , m_pluginManager(pluginManager)
    , m_mainLoop(nullptr)
    , m_handle(nullptr)
    , m_focusChangedSinceLastBroadcast(false)
    , m_broadcastTimer(new QTimer(this))
{
    startService();

    connect(m_connection.data(), &MInputContextConnection::widgetStateChanged, this, &IMELunaService::onWidgetStateChanged);
    connect(m_connection.data(), &MInputContextConnection::resetInputMethodRequest, this, &IMELunaService::onReset);
    connect(m_broadcastTimer, &QTimer::timeout, this, &IMELunaService::broadcastWidgetState);
    if (m_pluginManager) {
        connect(m_pluginManager, &MIMPluginManager::hardwareKeyboardStatusChanged,
                this, &IMELunaService::onKeyboardStatusChanged);
    }

}

IMELunaService::~IMELunaService()
{
    if (m_handle) {
        LSErrorWrapper err;

        if (!LSUnregister(m_handle, err)) {
            qWarning() << "failed to unregister from the bus: " << err.message();
        }
        m_handle = nullptr;
    }

    if (m_mainLoop) {
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
    }
}

bool IMELunaService::hasSubscribers() const
{
    return !m_clientByToken.isEmpty();
}

void IMELunaService::broadcastToSubscribers(const QJsonObject& response)
{
    LSErrorWrapper err;
    QJsonDocument document(response);

    if (!LSSubscriptionReply(m_handle, IMELunaService::SubscriberKey, document.toJson().constData(), err)) {
        qWarning() << "failed to reply to LS2 message";
    }
}

// Returns a JSON object representing the current input widget state
QJsonObject IMELunaService::getWidgetStateJson() const
{
    QJsonObject state;
    bool valid = false;

    bool focusState = m_connection->focusState(valid);

    if (valid) {
        state.insert("focus", focusState);
    }

    bool correctionEnabled = m_connection->correctionEnabled(valid);

    if (valid) {
        state.insert("correctionEnabled", correctionEnabled);
    }

    bool predictionEnabled = m_connection->predictionEnabled(valid);

    if (valid) {
        state.insert("predictionEnabled", predictionEnabled);
    }

    bool autoCapitalizationEnabled = m_connection->autoCapitalizationEnabled(valid);

    if (valid) {
        state.insert("autoCapitalizationEnabled", autoCapitalizationEnabled);
    }

    bool hiddenText = m_connection->hiddenText(valid);

    if (valid) {
        state.insert("hiddenText", hiddenText);
    }

    QString surroundingText;
    int cursorPosition = 0;

    // For security, only send minimal information about surrounding text
    if (m_connection->surroundingText(surroundingText, cursorPosition)) {
        state.insert("hasSurroundingText", !surroundingText.isEmpty());

        if (!hiddenText) {
            state.insert("cursorPosition", cursorPosition);
            state.insert("surroundingTextLength", surroundingText.length());
        }
    }

    bool hasSelection = m_connection->hasSelection(valid);

    if (valid && hasSelection) {
        state.insert("hasSelection", hasSelection);

        if (!hiddenText) {
            int anchorPosition = m_connection->anchorPosition(valid);

            if (valid) {
                state.insert("anchorPosition", anchorPosition);
            }
        }
    }

    int contentTypeInt = m_connection->contentType(valid);

    if (valid) {
        QString contentType;

        switch (contentTypeInt) {
        case Maliit::FreeTextContentType: contentType = "text"; break;
        case Maliit::NumberContentType: contentType = "number"; break;
        case Maliit::PhoneNumberContentType: contentType = "phonenumber"; break;
        case Maliit::EmailContentType: contentType = "email"; break;
        case Maliit::UrlContentType: contentType = "url"; break;
        default: contentType = "text"; break;
        }

        state.insert("contentType", contentType);
    }

    int enterKeyTypeInt = m_connection->enterKeyType(valid);

    if (valid) {
        state.insert("enterKeyType", enterKeyTypeInt);
    }

    return state;
}

void IMELunaService::onWidgetStateChanged(unsigned int clientId, const QMap<QString, QVariant> &newState,
                                         const QMap<QString, QVariant> &oldState, bool focusChanged)
{
    Q_UNUSED(clientId);
    Q_UNUSED(oldState);
    Q_UNUSED(newState);

    if (!m_handle) {
        return;
    }

    if (focusChanged) {
        m_focusChangedSinceLastBroadcast = true;
    }

    if (hasSubscribers()) {
        // run when event queue is empty (OK if already queued; QTimer will reschedule)
        m_broadcastTimer->setSingleShot(true);
        m_broadcastTimer->start(0);
    }
}

void IMELunaService::broadcastWidgetState()
{
    QJsonObject widgetState = getWidgetStateJson();

    QJsonObject response;
    response.insert("currentWidget", widgetState);
    response.insert("focusChanged", m_focusChangedSinceLastBroadcast);

    // Broadcast if focus or widget changed
    if (widgetState != m_lastWidgetState || m_focusChangedSinceLastBroadcast) {
        broadcastToSubscribers(response);
    }

    m_focusChangedSinceLastBroadcast = false;
    m_lastWidgetState = widgetState;

    // The keyboard status carries inputFocus, so a field taking or losing the
    // focus moves it as surely as plugging a keyboard in does - and it is a
    // different subscription, on a different key, which nothing here was
    // telling. Subscribers to getKeyboardStatus were left with whatever the
    // answer had been when they subscribed, which for the shell's cut/copy/
    // paste overlay meant inputFocus false for ever and an overlay that could
    // never come up. Cheap to call from here: it compares against the last
    // status and says nothing when the answer has not moved.
    onKeyboardStatusChanged();
}

void IMELunaService::onReset()
{
    m_focusChangedSinceLastBroadcast = true;
    m_broadcastTimer->setSingleShot(true);
    m_broadcastTimer->start(0);
}

// Insert text at the current cursor position, replacing selected text (if any)
void IMELunaService::insertText(const QString& text, bool replace, int length)
{
    if (replace) {
        QString surroundingText;
        int cursorPos = 0;

        if (length >= 0) {
            // Replace some text
            m_connection->sendCommitString(text, -length, length);
            return;
        }

        // Replace all text, which needs to know where the cursor is
        if (!m_connection->surroundingText(surroundingText, cursorPos) || cursorPos < 0) {
            qWarning() << "no usable surrounding text, cannot replace the field contents";
            return;
        }

        m_connection->sendCommitString(text, -cursorPos, surroundingText.length());
    } else {
        // Insert text at current cursor position
        m_connection->sendCommitString(text);
    }
}

namespace {

// What counts as part of a word when finding the one at the caret. The same rule
// the keyboard plugin uses to find the misspelling it reports, so that the two
// agree on where the word is; the check against the reported word below is what
// makes a disagreement harmless.
bool isWordCharacter(const QString &text, int index)
{
    const QChar c = text.at(index);

    if (c.isLetter())
        return true;

    // An apostrophe inside a word - don't, it's - and not one that quotes it.
    return (c == QLatin1Char('\'') || c == QChar(0x2019))
            && index > 0 && index + 1 < text.length()
            && text.at(index - 1).isLetter() && text.at(index + 1).isLetter();
}

} // namespace

// Replace the misspelled word the caret is in with \a suggestion.
bool IMELunaService::applySpellingSuggestion(const QString& suggestion)
{
    QString text;
    int cursor = 0;

    if (!m_connection->surroundingText(text, cursor) || cursor < 0 || cursor > text.length()) {
        qWarning() << "no usable surrounding text, cannot apply a spelling suggestion";
        return false;
    }

    int start = cursor;
    int end = cursor;

    while (start > 0 && isWordCharacter(text, start - 1))
        --start;
    while (end < text.length() && isWordCharacter(text, end))
        ++end;

    if (start == end)
        return false;

    // Only the word the shell was told about. The caret may have moved on since
    // the suggestions were shown, and replacing whatever word it is in now would
    // be putting a correction somewhere nobody asked for one.
    if (text.mid(start, end - start) != m_pluginManager->spellingWord())
        return false;

    m_connection->sendCommitString(suggestion, start - cursor, end - start);
    return true;
}

// Delete characters at the current cursor position, or all selected text (if any)
void IMELunaService::deleteCharacters(int numChars, DeleteMode mode)
{
    if (mode == DirectMode) {
        QString surroundingText;
        int cursorPos = 0;

        // Generally less reliable in practice but more consistent.
        // Without a cursor position there is nothing to count back from, and
        // a negative count here would turn into a negative replace length.
        if (!m_connection->surroundingText(surroundingText, cursorPos) || cursorPos < 0) {
            qWarning() << "no usable surrounding text, cannot delete directly";
            return;
        }

        numChars = std::min(numChars, cursorPos);
        if (numChars <= 0) {
            return;
        }

        m_connection->sendCommitString("", -numChars, numChars);
    } else {
        bool valid = false;
        bool hasSelection = m_connection->hasSelection(valid);

        if (hasSelection && valid) {
            // only hit delete once if there's a selection
            numChars = 1;
        }

        // Inject delete key presses
        // Ugly, but currently more reliable than sendCommitString since Maliit
        // doesn't seem to calculate the number of bytes to replace correctly yet

        for (int i = 0; i < numChars; i++) {
            m_connection->sendKeyEvent( QKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier) );
            m_connection->sendKeyEvent( QKeyEvent(QEvent::KeyRelease, Qt::Key_Backspace, Qt::NoModifier) );
        }
    }
}

void IMELunaService::sendEnterKey()
{
    QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    m_connection->sendKeyEvent(keyEvent);
}

extern "C" {

/*
 * Handler for LS2 service method palm://com.webos.service.ime/registerRemoteKeyboard
 *
 * Listens for changes to the input model. Bus client should remain subscribed as
 * long as it is actively using the keyboard.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/registerRemoteKeyboard '{"subscribe": true}'
 *
 * Parameters:
 *   subscribe - boolean (required). Must be true.
 *
 * Return payload:
 *   subscribed - boolean (required)
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 *
 * Subscription update payload:
 *   focusChanged - boolean (required)
 *   currentWidget - object (optional)
 *     focus - boolean (optional)
 *     correctionEnabled - boolean (optional)
 *     predictionEnabled - boolean (optional)
 *     autoCapitalizationEnabled - boolean (optional)
 *     hiddenText - boolean (optional)
 *     hasSurroundingText - boolean (optional)
 *     surroundingTextLength - int (optional)
 *     cursorPosition - int (optional)
 *     hasSelection - boolean (optional)
 *     anchorPosition - int (optional)
 *     contentType - string (optional). One of "text", "number", "phonenumber", "email", "url"
 *     enterKeyType - int (optional). check enum EnterKeyType.
 */
bool IMELunaService::handleRegisterRemoteKeyboard(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);

    LSMessageAdapter msg(message);

    if (msg.isSubscription()) {
        if (!msg.addSubscription(IMELunaService::SubscriberKey)) {
            // Without a subscription there is no cancel callback either, so
            // recording the client would leave an entry that never goes away
            // and keeps hasSubscribers() true forever.
            msg.replyError("Failed to add subscription");
            return true;
        }

        // Track subscription
        QString token = msg.uniqueToken();

        QSharedPointer<RemoteKeyboardClient> client(new RemoteKeyboardClient());
        client->token = token;

        service->m_clientByToken.insert(token, client);

        qWarning() << "registering remote keyboard";

        // Send subscribe response with initial state
        QJsonObject response;
        response.insert("subscribed", true);

        QJsonObject state = service->getWidgetStateJson();
        if (!state.isEmpty()) {
            response.insert("currentWidget", state);
        }

        msg.respond(response);
    } else {
        QJsonObject response;

        response.insert("returnValue", false);
        response.insert("errorCode", -1000);
        response.insert("errorText", QString("Must subscribe to registerRemoteKeyboard"));

        msg.respond(response);
    }

    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/insertText
 *
 * Inserts text at the current insertion point, or replaces all text in the field.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/insertText '{"text": "hello world", "replace": true}'
 *
 * Parameters:
 *   text - string (required). Text to insert.
 *   replace - boolean (optional). If true, replace any existing text in field.
 *   replaceLength - number of characters to replace
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 */
bool IMELunaService::handleInsertText(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    QJsonObject payload = msg.getPayload();
    QJsonValue textParam = payload["text"];
    QJsonValue replaceParam = payload["replace"];
    QJsonValue replaceLengthParam = payload["replaceLength"];

    if (!textParam.isString()) {
        msg.replyError("Missing \"text\" parameter");
        return true;
    }

    const bool replace = replaceParam.isBool() ? replaceParam.toBool() : false;
    int length = -1;

    if (replaceLengthParam.isDouble()
        && !Maliit::Json::toInt(replaceLengthParam, INT_MIN, INT_MAX, &length)) {
        msg.replyError("Invalid \"replaceLength\" parameter");
        return true;
    }

    service->insertText(textParam.toString(), replace, length);
    msg.replyTrue();

    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/applySpellingSuggestion
 *
 * Replaces the misspelled word the caret is in with one of the suggestions
 * getKeyboardStatus reported for it. Refused if the caret is no longer in that word.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/applySpellingSuggestion '{"suggestion": "the"}'
 *
 * Parameters:
 *   suggestion - string (required). The replacement.
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorText - string (optional)
 */
bool IMELunaService::handleApplySpellingSuggestion(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    const QJsonValue suggestion = msg.getPayload()["suggestion"];

    if (!suggestion.isString() || suggestion.toString().isEmpty()) {
        msg.replyError("Missing \"suggestion\" parameter");
        return true;
    }

    if (!service->applySpellingSuggestion(suggestion.toString())) {
        msg.replyError("The caret is not in the word the suggestion was for");
        return true;
    }

    msg.replyTrue();
    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/deleteCharacters
 *
 * Deletes characters from the current insertion point.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/deleteCharacters '{"count": 1}'
 *
 * Parameters:
 *   count - number (required). Number of characters to delete.
 *   mode - "backspace" (send backspace key)
 *          "direct" (remove characters directly from text string; single-line only)
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 */
bool IMELunaService::handleDeleteCharacters(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    QJsonObject payload = msg.getPayload();
    QJsonValue characterCount = payload["count"];
    QJsonValue modeParam = payload["mode"];

    DeleteMode mode = BackspaceMode;

    if (modeParam.isString()) {
        QString modeString = modeParam.toString();

        if (modeString == "backspace") {
            mode = BackspaceMode;
        } else if (modeString == "direct") {
            mode = DirectMode;
        } else {
            msg.replyError("Unknown \"mode\"; supported modes are \"backspace\" and \"direct\"");
            return true;
        }
    }

    int count = 0;
    if (!Maliit::Json::toInt(characterCount, 1, INT_MAX, &count)) {
        msg.replyError("Missing or invalid \"count\" parameter");
        return true;
    }

    service->deleteCharacters(count, mode);
    msg.replyTrue();

    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/sendEnterKey
 *
 * Sends the enter key in the current field.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/sendEnterKey '{}'
 *
 * Parameters:
 *   no parameters
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 */
bool IMELunaService::handleSendEnterKey(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    service->sendEnterKey();

    msg.replyTrue();
    return true;
}

// Handle subscription cancellation
bool IMELunaService::handleSubscriptionCancel(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    service->m_clientByToken.remove(msg.uniqueToken());

    if (service->m_clientByToken.isEmpty()) {
        qWarning() << "all remote keyboard clients disconnected";

        // TODO: if all active clients have unsubscribed, do something (inform VKB?)
    }

    return true;
}

} // extern "C"

//! \brief What the shell needs to draw a "show the keyboard anyway" control, and
//!        what anything else needs to know a physical keyboard is in use.
QJsonObject IMELunaService::getKeyboardStatusJson() const
{
    QJsonObject status;

    if (!m_pluginManager)
        return status;

    QJsonObject hardware;
    hardware.insert("present", m_pluginManager->hardwareKeyboardPresent());
    hardware.insert("usable", m_pluginManager->hardwareKeyboardUsable());
    // True where the keyboard folds or slides away, so a caller knows "usable"
    // can change without anything being plugged in or out.
    hardware.insert("slider", m_pluginManager->hardwareKeyboardIsSlider());
    // The declared layout, or empty where none is - see
    // MIMPluginManager::hardwareKeyboardLayout(). Always present as a key so a
    // caller does not have to tell "not declared" from "old server".
    hardware.insert("layout", m_pluginManager->hardwareKeyboardLayout());
    // What the key faces say, for callers that take digits without going through
    // an input method at all - the lock screen's PIN pad is the shell's own QML,
    // running inside the compositor, so no plugin ever sees its keys. Keyed by
    // evdev scancode as a string.
    // What has been set, as opposed to what is in force: "layout" above is the
    // answer after the plugin and the device's declaration have had their say,
    // and a settings page needs to show which of those is being overridden.
    hardware.insert("layoutOverride", m_pluginManager->keyboardLayoutOverride());
    hardware.insert("keypadCounts", m_pluginManager->telephoneKeypadCounts());
    hardware.insert("keyFaceDigits",
                    QJsonObject::fromVariantMap(m_pluginManager->hardwareKeyFaceDigits()));

    status.insert("hardwareKeyboard", hardware);
    status.insert("onScreenKeyboardForced", m_pluginManager->onScreenKeyboardForced());

    // Whether a text field currently holds the input method's focus. The shell
    // shows its cut/copy/paste overlay only where there is something to edit,
    // the way legacy's enyo.EditMenu greyed its own items out
    // (autoDisableItems). False rather than absent when the connection cannot
    // say, so a caller never has to tell "no field" from "old server".
    bool focusValid = false;
    const bool focused = m_connection ? m_connection->focusState(focusValid) : false;
    status.insert("inputFocus", focusValid && focused);

    // What the shell's cut/copy/paste overlay needs to choose its items, the
    // way legacy's did: Cut and Copy only when something is selected, Select
    // All and Paste when nothing is. Booleans only - the text itself is not
    // something to put on a bus, least of all from a password field.
    bool selectionValid = false;
    const bool selected = m_connection ? m_connection->hasSelection(selectionValid) : false;
    status.insert("inputHasSelection", focused && selectionValid && selected);

    QString surroundingText;
    int cursorPosition = 0;
    const bool hasText = m_connection &&
                         m_connection->surroundingText(surroundingText, cursorPosition) &&
                         !surroundingText.isEmpty();
    status.insert("inputHasText", focused && hasText);

    // The misspelled word the caret is in, with what the plugin would put in its
    // place, for the shell's suggestion pill. Empty when the caret is in none, and
    // nothing at all when no field holds the focus.
    // Never from a field that hides what is typed in it, whatever the plugin says:
    // this is a word of the user's own text, going out on a bus.
    bool hiddenValid = false;
    const bool hiddenField = m_connection && m_connection->hiddenText(hiddenValid) && hiddenValid;
    const bool fieldFocused = focusValid && focused && !hiddenField;
    status.insert("spellingWord", fieldFocused ? m_pluginManager->spellingWord() : QString());
    status.insert("spellingSuggestions",
                  fieldFocused ? QJsonArray::fromStringList(m_pluginManager->spellingSuggestions())
                               : QJsonArray());

    // Where the caret is, in the client's own coordinates, so that the pill can
    // be put at the word as legacy's spelling widget was rather than wherever
    // the finger happened to land. Only with a misspelling to point at.
    bool rectValid = false;
    const QRect caret = m_connection ? m_connection->cursorRectangle(rectValid) : QRect();
    if (fieldFocused && !m_pluginManager->spellingWord().isEmpty() && rectValid && caret.isValid()) {
        QJsonObject rect;
        rect.insert("x", caret.x());
        rect.insert("y", caret.y());
        rect.insert("width", caret.width());
        rect.insert("height", caret.height());
        status.insert("spellingRect", rect);
    }

    return status;
}

void IMELunaService::onKeyboardStatusChanged()
{
    const QJsonObject status(getKeyboardStatusJson());

    // updateInputSource() runs for reasons unrelated to this - an accessory
    // setting, a plugin reload - so only say something when the answer moved.
    if (status == m_lastKeyboardStatus)
        return;

    m_lastKeyboardStatus = status;

    QJsonObject response(status);
    response.insert("returnValue", true);

    LSErrorWrapper err;
    QJsonDocument document(response);

    if (!LSSubscriptionReply(m_handle, IMELunaService::KeyboardStatusSubscriberKey,
                             document.toJson().constData(), err)) {
        qWarning() << "failed to broadcast the keyboard status";
    }
}

bool IMELunaService::handleGetKeyboardStatus(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);

    LSMessageAdapter msg(message);

    if (!service->m_pluginManager) {
        msg.replyError("No plugin manager; keyboard status is unavailable");
        return true;
    }

    QJsonObject response(service->getKeyboardStatusJson());
    response.insert("returnValue", true);

    if (msg.isSubscription()) {
        if (!msg.addSubscription(IMELunaService::KeyboardStatusSubscriberKey)) {
            msg.replyError("Failed to add subscription");
            return true;
        }

        response.insert("subscribed", true);

        // So the first broadcast after this is a real change and not a repeat of
        // what the subscriber was just handed.
        service->m_lastKeyboardStatus = service->getKeyboardStatusJson();
    }

    msg.respond(response);

    return true;
}

//! \brief Asks for the on-screen keyboard even though a physical one is attached.
//!
//! The way back to an emoji, a script the hardware has no keys for, or a key it
//! is missing. Sticky until turned off again, which is how LunaSysMgr's keyboard
//! key behaved: it toggled IMEController and left it there.
bool IMELunaService::handleSetOnScreenKeyboardForced(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);

    LSMessageAdapter msg(message);

    if (!service->m_pluginManager) {
        msg.replyError("No plugin manager; the on-screen keyboard cannot be forced");
        return true;
    }

    const QJsonObject payload(msg.getPayload());
    const QJsonValue forced(payload.value(QStringLiteral("forced")));

    if (!forced.isBool()) {
        msg.replyError("\"forced\" is required and must be a boolean");
        return true;
    }

    service->m_pluginManager->setOnScreenKeyboardForced(forced.toBool());

    QJsonObject response(service->getKeyboardStatusJson());
    response.insert("returnValue", true);
    msg.respond(response);

    return true;
}

//! \brief Says what layout the attached keyboard has, when nothing can tell.
//!
//! A USB or Bluetooth keyboard carries its layout in the compositor's xkb keymap
//! and a phone's own keyboard is identified by its profile, but neither covers
//! every case - so this is the way for somebody to state it. An empty string
//! restores "work it out".
bool IMELunaService::handleSetHardwareKeyboardLayout(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);

    LSMessageAdapter msg(message);

    if (!service->m_pluginManager) {
        msg.replyError("No plugin manager; the keyboard layout cannot be set");
        return true;
    }

    const QJsonObject payload(msg.getPayload());
    const QJsonValue layout(payload.value(QStringLiteral("layout")));

    if (!layout.isString()) {
        msg.replyError("\"layout\" is required and must be a string;"
                       " an empty one lets the hardware decide");
        return true;
    }

    service->m_pluginManager->setKeyboardLayoutOverride(layout.toString());

    QJsonObject response(service->getKeyboardStatusJson());
    response.insert("returnValue", true);
    msg.respond(response);

    return true;
}

//! \brief Whether a telephone keypad counts as a hardware keyboard.
//!
//! Off by default, because a keypad has the digits and none of the letters:
//! taking the on-screen keyboard away for one leaves no way to type a word. On a
//! device whose keypad is meant to be typed on by multi-tap, it is the point.
bool IMELunaService::handleSetTelephoneKeypadCounts(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);

    LSMessageAdapter msg(message);

    if (!service->m_pluginManager) {
        msg.replyError("No plugin manager; telephone keypads cannot be configured");
        return true;
    }

    const QJsonObject payload(msg.getPayload());
    const QJsonValue counts(payload.value(QStringLiteral("counts")));

    if (!counts.isBool()) {
        msg.replyError("\"counts\" is required and must be a boolean");
        return true;
    }

    service->m_pluginManager->setTelephoneKeypadCounts(counts.toBool());

    QJsonObject response(service->getKeyboardStatusJson());
    response.insert("returnValue", true);
    msg.respond(response);

    return true;
}

LSMethod IMELunaService::ime_bus_methods [] = {
    // Handlers for service methods for com.webos.service.ime
    {"registerRemoteKeyboard", IMELunaService::handleRegisterRemoteKeyboard, (LSMethodFlags) 0},
    {"insertText", IMELunaService::handleInsertText, (LSMethodFlags) 0},
    {"applySpellingSuggestion", IMELunaService::handleApplySpellingSuggestion, (LSMethodFlags) 0},
    {"deleteCharacters", IMELunaService::handleDeleteCharacters, (LSMethodFlags) 0},
    {"sendEnterKey", IMELunaService::handleSendEnterKey, (LSMethodFlags) 0},
    {"getKeyboardStatus", IMELunaService::handleGetKeyboardStatus, (LSMethodFlags) 0},
    {"setOnScreenKeyboardForced", IMELunaService::handleSetOnScreenKeyboardForced, (LSMethodFlags) 0},
    {"setHardwareKeyboardLayout", IMELunaService::handleSetHardwareKeyboardLayout, (LSMethodFlags) 0},
    {"setTelephoneKeypadCounts", IMELunaService::handleSetTelephoneKeypadCounts, (LSMethodFlags) 0},

    {nullptr, nullptr, (LSMethodFlags) 0}
};

void IMELunaService::startService()
{
    GMainContext *context = g_main_context_default();
    m_mainLoop = g_main_loop_new(context, TRUE);
    bool ret;
    LSErrorWrapper err;

    // TODO :
    // Currently, only MaliitServer with instanceId 0 can serve an LS2 service.
    // instanceId 0 means 'primary service' with service name of com.webos.service.ime.
    // Other instances will have service name as com.webos.service.ime-n where n is the instanceId.
    ret = LSRegister(MImGlobalSettings::instance()->getServiceName().toLatin1().data(), &m_handle, err);

    qInfo() << "MaliitServer: Starting IMELunaService [" << MImGlobalSettings::instance()->getServiceName() << "], instance:" << MImGlobalSettings::instance()->getInstanceId();

    if (!ret) {
        qCritical() << "failed to register [" << MImGlobalSettings::instance()->getServiceName() << "] on bus: " << err.message();
        return;
    }

    if (!LSRegisterCategory(m_handle, "/", ime_bus_methods, nullptr, nullptr, err)) {
        qCritical() << "failed to register category on bus: " << err.message();
        return;
    }

    if (!LSCategorySetData(m_handle, "/", this, err)) {
        qCritical() << "failed to set category data: " << err.message();
        return;
    }

    if (!LSGmainAttach(m_handle, m_mainLoop, err)) {
        qCritical() << "unable to attach LS2 to main loop" << err.message();
        return;
    }

    if (!LSSubscriptionSetCancelFunction(m_handle, &IMELunaService::handleSubscriptionCancel, this, err)) {
        qCritical() << "error registering LS2 cancel function" << err.message();
        return;
    }

    qWarning() << MImGlobalSettings::instance()->getServiceName() << " LS2 service running";
}
