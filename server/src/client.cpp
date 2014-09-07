/**
 * Copyright (C) 2013 Naikel Aparicio. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL EELI REILIN OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the author and should not be interpreted as representing
 * official policies, either expressed or implied, of the copyright holder.
 */

#include <QGuiApplication>

#include <QPixmap>
#include <QPainter>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>

#include <QImage>
#include <QBitmap>

#include <QNetworkConfiguration>
#include <QNetworkSession>

#include "client.h"
#include "globalconstants.h"

#include "Whatsapp/fmessage.h"
#include "Whatsapp/ioexception.h"
#include "Whatsapp/util/utilities.h"
#include "Whatsapp/util/qtmd5digest.h"
#include "Whatsapp/util/datetimeutilities.h"
#include "Whatsapp/util/rfc6234/sha224-256.c"

#include <QUuid>
#include <QJsonDocument>
#include <QJsonParseError>

#include <QDesktopServices>

#include <QImageReader>

#include <QMediaPlayer>
#include <QMediaMetaData>

#include "contactsfetch.h"

#include "../qexifimageheader/qexifimageheader.h"

/** ***********************************************************************
 ** Static global members
 **/

// Network data counters
DataCounters Client::dataCounters;

// Status of the connection
Client::ConnectionStatus Client::connectionStatus;

// Message number sequence
quint64 Client::seq;

// Own JID
QString Client::myJid;

// Country code
QString Client::cc;

QString Client::mcc;
QString Client::mnc;

// Phone number in local format (without the country code)
QString Client::number;

// Phone number in international format (with the country code)
QString Client::phoneNumber;

// User name or alias
QString Client::userName;

// User password
QString Client::password;

// Account creation timestamp
QString Client::creation;

// Account expiration timestamp
QString Client::expiration;

// Account kind (free/paid)
QString Client::kind;

// Account status (active/expired)
QString Client::accountstatus;

// User status
QString Client::myStatus;

// Import media into gallery
bool Client::importMediaToGallery;

// Sync frequency
int Client::syncFreq;

// Last time address book synchronizarion was performed
qint64 Client::lastSync;

QString Client::wauseragent;
QString Client::waresource;

QString Client::wanokiascratch1;
QString Client::wanokiascratch2;
QString Client::wandroidscratch1;
QString Client::wandroidscratch2;
QString Client::wandroidscratch3;
QString Client::wandroidscratch4;

bool Client::whatsappevil;

bool Client::acceptUnknown;

QNetworkAccessManager* Client::nam;

/**
    Constructs a Client object.

    @param minimized    Starts Yappari minimized if true.
    @param parent       QObject parent.
*/
Client::Client(QObject *parent) : QObject(parent)
{
    QProcess app;
    app.start("/bin/rpm", QStringList() << "-qa" << "--queryformat" << "%{version}-%{release}" <<  "harbour-mitakuuluu2");
    if (app.bytesAvailable() <= 0) {
        app.waitForFinished(5000);
    }
    qDebug() << "App version:" << app.readAll();

    qDebug() << "Locale" << QLocale::system().name();

    reg = 0;
    dbExecutor = 0;
    manager = 0;
    session = 0;
    connectionThread = 0;
    contactManager = 0;
    connectionNotification = 0;
    disconnectCount = 0;
    lastDisconnect = 0;
    reconnectInterval = 1;
    lastReconnect = 1;

    waitingCodeConnection = false;
    waitingRegConnection = false;

    _privacy = QVariantMap();
    _totalUnread = 0;
    _pendingCount = 0;

    readSettings();

    connectionNotification = NULL;

    QSettings settings("coderus", "mitakuuluu2", this);
    settings.sync();
    QString localeName = settings.value("settings/locale", QLocale::system().name()).toString();
    setLocale(localeName);

    qDBusRegisterMetaType<QVariantMapList>();

    bool ret =
        QDBusConnection::sessionBus().registerService(SERVICE_NAME) &&
        QDBusConnection::sessionBus().registerObject(OBJECT_NAME, this,
                                                 QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllSlots);

    if (ret)
        qDebug() << "Dbus registered";
    else {
        QGuiApplication::exit(0);
        return;
    }

    QDBusInterface *ofono = new QDBusInterface("org.ofono", "/ril_0", "org.ofono.SimManager", QDBusConnection::systemBus(), this);
    QDBusPendingCall async = ofono->asyncCall("GetProperties");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);
    if (watcher->isFinished()) {
       onSimParameters(watcher);
    }
    else {
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),this, SLOT(onSimParameters(QDBusPendingCallWatcher*)));
    }

    uuid = QUuid::createUuid().toString();

    // Initialization of sequence for messages IDs
    seq = 0;

    dbExecutor = QueryExecutor::GetInstance();
    connect(dbExecutor, SIGNAL(actionDone(QVariant)), this, SLOT(dbResults(QVariant)));

    /*QVariantMap muted;
    muted["uuid"] = uuid;
    muted["type"] = QueryType::ContactsGetMuted;
    dbExecutor->queueAction(muted);*/

    // Timers
    pendingMessagesTimer = new QTimer(this);
    pendingMessagesTimer->setSingleShot(true);
    QObject::connect(pendingMessagesTimer, SIGNAL(timeout()), this, SLOT(sendMessagesInQueue()));

    retryLoginTimer = new QTimer(this);
    retryLoginTimer->setSingleShot(true);
    QObject::connect(retryLoginTimer, SIGNAL(timeout()), this, SLOT(doCheckNetworkStatus()));

    reg = NULL;

    // Network Manager Configutarion

    nam = new QNetworkAccessManager(this);

    manager = new QNetworkConfigurationManager(this);
    connect(manager,SIGNAL(onlineStateChanged(bool)),
            this,SLOT(networkStatusChanged(bool)));

    connect(manager,SIGNAL(configurationChanged(QNetworkConfiguration)),
            this,SLOT(networkConfigurationChanged(QNetworkConfiguration)));

    if (manager->isOnline()) {
        qDebug() << "Have connection!";
    }
    else {
        qDebug() << "Have no connection!";
    }

    keepalive = new BackgroundActivity(this);
    connect(keepalive, SIGNAL(running()), this, SLOT(checkActivity()));
    connect(keepalive, SIGNAL(stopped()), this, SLOT(wakeupStopped()));

    recheckAccountAndConnect();
}

/**
    Destroys a Connection object
*/
Client::~Client()
{
    // Update data counters
    dataCounters.writeCounters();

    qDebug() << "Destroying database";
    if (dbExecutor)
        delete dbExecutor;
    qDebug() << "Application destroyed";
}

void Client::doCheckNetworkStatus()
{
    if ((connectionStatus == Disconnected || connectionStatus == WaitingForConnection) && isOnline) {
        verifyAndConnect();
    }
}

/** ***********************************************************************
 ** Settings methods
 **/

/**
    Reads the global settings and store them in the public static members.
*/
void Client::readSettings()
{
    QSettings settings("coderus", "mitakuuluu2", this);
    settings.sync();

    this->wanokiascratch1 = settings.value(SETTINGS_SCRATCH1, BUILD_KEY).toString();
    this->wanokiascratch2 = settings.value(SETTINGS_SCRATCH2, BUILD_HASH).toString();
    this->wandroidscratch1 = settings.value(SETTINGS_SCRATCH3, ANDROID_TT).toString();

    this->whatsappevil = settings.value(SETTINGS_EVIL, false).toBool();

    this->waversion = settings.value(SETTINGS_WAVERSION, USER_AGENT_VERSION).toString();
    this->waresource = QString("Android-%1-443").arg(this->waversion);
    this->wauseragent = QString("WhatsApp/%1 Android/4.2.1 Device/GalaxyS3").arg(this->waversion);
    //qDebug() << "UA:" << this->wauseragent;

    this->acceptUnknown = settings.value(SETTINGS_UNKNOWN, true).toBool();

    // Account
    this->myStatus = settings.value(SETTINGS_PRESENCE, "I'm using Mitakuuluu!").toString();

    this->creation = settings.value(SETTINGS_CREATION).toString();
    this->kind = settings.value(SETTINGS_KIND).toString();
    this->expiration = settings.value(SETTINGS_EXPIRATION).toString();
    this->accountstatus = settings.value(SETTINGS_ACCOUNTSTATUS).toString();

    // Last Synchronization
    this->lastSync = settings.value(SETTINGS_LAST_SYNC).toLongLong();

    alwaysOffline = settings.value("settings/alwaysOffline", false).toBool();

    this->importMediaToGallery = settings.value("settings/importmediatogallery", true).toBool();

    resizeImages = settings.value("settings/resizeImages", false).toBool();
    resizeWlan = settings.value("settings/resizeWlan").toBool();
    resizeBySize = settings.value("settings/resizeBySize", true).toBool();
    resizeImagesTo = settings.value("settings/resizeImagesTo", 1024*1024).toInt();
    resizeImagesToMPix = settings.value("settings/resizeImagesToMPix", 5.01).toFloat();

    autoDownloadMedia = settings.value(SETTINGS_AUTOMATIC_DOWNLOAD).toBool();
    autoBytes = settings.value(SETTINGS_AUTOMATIC_DOWNLOAD_BYTES, QVariant(DEFAULT_AUTOMATIC_DOWNLOAD)).toInt();
    autoDownloadWlan = settings.value("settings/autoDownloadWlan").toBool();

    notificationsMuted = settings.value("settings/notificationsMuted", false).toBool();
    notifyMessages = settings.value("settings/notifyMessages", false).toBool();
    systemNotifier = settings.value("settings/systemNotifier", false).toBool();

    useKeepalive = settings.value("settings/useKeepalive", true).toBool();
    reconnectionInterval = settings.value("settings/reconnectionInterval", 1).toInt();
    reconnectionLimit = settings.value("settings/reconnectionLimit", 20).toInt();
    disconnectStreamError = settings.value("settings/disconnectStreamError", false).toBool();

    usePhonebookAvatars = settings.value("settings/usePhonebookAvatars", false).toBool();

    showConnectionNotifications = settings.value("settings/showConnectionNotifications", false).toBool();
    notificationsDelay = settings.value("settings/notificationsDelay", 5).toInt();

    settings.beginGroup("muting");
    foreach (const QString &key, settings.childKeys()) {
        mutingList[key] = settings.value(key, 0).toULongLong();
    }
    settings.endGroup();

    // Read counters
    dataCounters.readCounters();
    lastCountersWrite = QDateTime::currentMSecsSinceEpoch();
}

void Client::saveAccountData()
{
    qDebug() << "saveAccountData" << phoneNumber << password;
    QSettings settings("coderus", "mitakuuluu2", this);
    settings.setValue(SETTINGS_PHONENUMBER, this->phoneNumber);
    settings.setValue(SETTINGS_PASSWORD, this->password);
    settings.setValue(SETTINGS_MYJID, this->myJid);
}

QStringList Client::getPhoneInfo(const QString &phone)
{
    QFile csv("/usr/share/harbour-mitakuuluu2/data/countries.csv");
    if (csv.exists() && csv.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&csv);
        QString line;
        while (!in.atEnd()) {
            line = in.readLine();
            if (line.split(",").length() > 1 && phone.startsWith(line.split(",").at(1)))
                break;
        }
        if (line.split(",").length() > 1 && phone.startsWith(line.split(",").at(1))) {
            csv.close();
            return line.split(",");
        }
        csv.close();
    }
    return QStringList();
}

void Client::addMessage(const FMessage &message)
{
    QStringList jids;
    if (message.broadcastJids.size() > 0) {
        jids = message.broadcastJids;
    }
    else {
        jids << message.key.remote_jid;
    }
    foreach (const QString &jid, jids) {
        qDebug() << "message from:" << jid;
        QString author;
        if (message.key.from_me)
            author = myJid;
        else {
            if (jid.contains("-"))
                author = message.remote_resource;
            else
                author = jid;
        }
        QString text = QString::fromUtf8(message.data.data());

        QVariantMap data;
        data["msgid"] = message.key.id;
        data["jid"] = jid;
        data["author"] = author;
        data["timestamp"] = message.timestamp;
        data["data"] = text;
        data["status"] = message.status;
        data["watype"] = message.media_wa_type;
        data["url"] = message.media_url;
        data["name"] = message.media_name;
        data["latitude"] = message.latitude;
        data["longitude"] = message.longitude;
        data["size"] = message.media_size;
        data["mime"] = message.media_mime_type;
        data["duration"] = message.media_duration_seconds;
        if (message.media_wa_type == FMessage::Image && !message.local_file_uri.isEmpty()) {
            if (message.media_width <= 0 || message.media_height <= 0) {
                QImageReader img(message.local_file_uri);
                data["width"] = img.size().width();
                data["height"] = img.size().height();
            }
            else {
                data["width"] = message.media_width;
                data["height"] = message.media_height;
            }
        }
        else if (message.media_wa_type == FMessage::Video && !message.local_file_uri.isEmpty()) {
            if (message.media_width <= 0 || message.media_height <= 0) {
                QMediaPlayer player;
                qDebug() << "should get metadata for:" << message.local_file_uri;
                player.setMedia(QUrl::fromLocalFile(message.local_file_uri));
                while (!player.availableMetaData().contains(QMediaMetaData::Resolution)) {
                    qDebug() << "waiting for video metadata";
                    QEventLoop loop;
                    QObject::connect(&player, SIGNAL(metaDataChanged()), &loop, SLOT(quit()));
                    loop.exec();
                }
                QSize resolution = player.metaData(QMediaMetaData::Resolution).toSize();
                qDebug() << "video metadata available:" << resolution;
                data["width"] = resolution.width();
                data["height"] = resolution.height();
            }
        }
        else {
            data["width"] = message.media_width;
            data["height"] = message.media_height;
        }
        data["local"] = message.local_file_uri;
        data["broadcast"] = message.broadcast ? 1 : 0;
        data["live"] = message.live ? 1 : 0;

        Q_EMIT messageReceived(data);

        QVariantMap query;
        query["type"] = QueryType::ContactsSetLastmessage;
        query["jid"] = jid;
        query["lastmessage"] = message.timestamp;
        query["uuid"] = uuid;
        dbExecutor->queueAction(query);

        data["type"] = QueryType::ConversationSaveMessage;
        data["uuid"] = uuid;
        dbExecutor->queueAction(data);

        qDebug() << "should show notification?" << "author:" << author << "jid:" << jid << "activeJid:" << _activeJid << "myJid:" << myJid << "offline:" << message.offline;
        if ((author != myJid) && (author != _activeJid) && (jid != _activeJid)) {
            int unread = getUnreadCount(jid);
            unread++;
            setUnreadCount(jid, unread);

            if (message.type == FMessage::MediaMessage) {
                switch (message.media_wa_type) {
                case FMessage::Image: text = tr("Image", "Notification media name text"); break;
                case FMessage::Audio: text = tr("Audio", "Notification media name text"); break;
                case FMessage::Video: text = tr("Video", "Notification media name text"); break;
                case FMessage::Contact: text = tr("Contact", "Notification media name text"); break;
                case FMessage::Location: text = tr("Location", "Notification media name text"); break;
                case FMessage::Voice: text = tr("Voice", "Notification media name text"); break;
                default: text = tr("System", "Notification media name text"); break;
                }
            }

            qDebug() << "show notification for:" << message.key.id << "jid:" << jid;
            QVariantMap notify;
            notify["type"] = QueryType::ConversationNotifyMessage;
            notify["phonebook"] = usePhonebookAvatars;
            notify["jid"] = jid;
            notify["pushName"] = message.notify_name;
            notify["media"] = message.media_wa_type != FMessage::Text;
            notify["msg"] = text;
            notify["uuid"] = uuid;
            notify["offline"] = message.offline;
            dbExecutor->queueAction(notify);
        }
    }
}

void Client::updateContactPushname(const QString &jid, const QString &pushName)
{
    qDebug() << "update pushname for" << jid << "to" << pushName;
    uint timestamp = QDateTime::currentDateTime().toTime_t();

    QVariantMap query;
    query["jid"] = jid;
    query["type"] = QueryType::ContactsUpdatePushname;
    query["timestamp"] = timestamp;
    query["pushName"] = pushName;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);

    if (!pushName.isEmpty() && pushName != jid.split("@").first())
        Q_EMIT pushnameUpdated(jid, pushName);
}

void Client::onSimParameters(QDBusPendingCallWatcher *call)
{
    QVariantMap value;
    QDBusPendingReply<QVariantMap> reply = *call;
    if (reply.isError()) {
        qDebug() << "error:" << reply.error().name() << reply.error().message();
    } else {
        value = reply.value();
    }
    mcc = value["MobileCountryCode"].toString();
    mnc = value["MobileNetworkCode"].toString();

    qDebug() << "mcc:" << mcc;
    qDebug() << "mnc:" << mnc;

    Q_EMIT simParameters(mcc, mnc);

    call->deleteLater();
}

void Client::notifyOfflineMessages(int count)
{
    qDebug() << "Set offline messages:" << count << "pending:" << _pendingCount;
    _totalUnread = count;

    if (_totalUnread == _pendingCount)
        showOfflineNotifications();
    /*QString text = tr("Offline messages: %n", "", count);
    offlineMesagesNotification = new MNotification("harbour.mitakuuluu2.notification", text, "Mitakuuluu");
    offlineMesagesNotification->setImage("/usr/share/themes/base/meegotouch/icons/harbour-mitakuuluu-popup.png");
    MRemoteAction action("harbour.mitakuuluu2.client", "/", "harbour.mitakuuluu2.client", "notificationCallback", QVariantList() << QString());
    offlineMesagesNotification->setAction(action);
    offlineMesagesNotification->publish();*/
}

int Client::getUnreadCount(const QString &jid)
{
    int unread = 0;
    if (_unreadCount.keys().contains(jid))
        unread = _unreadCount[jid];
    qDebug() << jid << "unread count:" << unread;
    return unread;
}

void Client::setUnreadCount(const QString &jid, int count)
{
    _unreadCount[jid] = count;
    _contacts[jid] = count;
    Q_EMIT setUnread(jid, count);
    qDebug() << jid << "set unread count:" << count;
    QVariantMap query;
    query["unread"] = count;
    query["jid"] = jid;
    query["type"] = QueryType::ContactsSetUnread;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

/** ***********************************************************************
 ** Network detection methods
 **/

/**
    Handles a network change

    @param isOnline     True if an network connection is currently available
*/
void Client::networkStatusChanged(bool isOnline)
{
    QString status = (isOnline) ? "Online" : "Offline";
    qDebug() << "Network connection changed:" << status;

    this->isOnline = isOnline;
    Q_EMIT networkAvailable(isOnline);

    if (!isOnline)
    {
        Q_EMIT networkOffline();
        activeNetworkID.clear();
        activeNetworkType = QNetworkConfiguration::BearerUnknown;
        if (connectionStatus == Connected || connectionStatus == LoggedIn)
            connectionClosed();
    }
    else
    {
        Q_EMIT networkOnline();
        updateActiveNetworkID();
        if ((connectionStatus == Disconnected || connectionStatus == WaitingForConnection)) {
            if (retryLoginTimer->isActive()) {
                retryLoginTimer->stop();
            }
            lastReconnect = 1;
            reconnectInterval = 1;
            retryLoginTimer->start(1000);
        }
        else if (connectionStatus == RegistrationFailed) {
            getTokenScratch();
            if (reg) {
                if (waitingCodeConnection) {
                    QTimer::singleShot(500, reg, SLOT(startRegRequest()));
                    waitingCodeConnection = false;
                }
                else if (waitingRegConnection) {
                    QTimer::singleShot(500, reg, SLOT(start()));
                    waitingRegConnection = false;
                }
            }
        }
    }
}

void Client::networkConfigurationChanged(const QNetworkConfiguration &conf)
{
    if (conf.state() == QNetworkConfiguration::Active)
    {
        qDebug() << "Network activated:" << conf.name();
    }
    else
    {
        qDebug() << "Network deactivated:" << conf.name();
    }

    if (isOnline && (connectionStatus == Connected || connectionStatus == LoggedIn) &&
            conf.state() == QNetworkConfiguration::Active &&
            activeNetworkID != conf.name())
    {
        isOnline = false;
        QTimer::singleShot(0,this,SLOT(connectionClosed()));
        QTimer::singleShot(3000,this,SLOT(connectionActivated()));
    }
}

void Client::connectionActivated()
{
    networkStatusChanged(true);
}

void Client::connectionDeactivated()
{
    networkStatusChanged(false);
}

bool Client::isNetworkAvailable()
{
    return isOnline;
}

bool Client::isAccountValid()
{
    return (!this->phoneNumber.isEmpty() || !this->password.isEmpty());
}

void Client::updateActiveNetworkID()
{
    if (isOnline)
    {
        // Let's save the current configuration identifier
        QList<QNetworkConfiguration> activeConfigs = manager->allConfigurations(QNetworkConfiguration::Active);
        for (int i = 0; i < activeConfigs.size(); i++)
        {
            QNetworkConfiguration conf = activeConfigs.at(i);
            if (conf.state() == QNetworkConfiguration::Active)
            {
                activeNetworkID = conf.identifier();
                activeNetworkType = conf.bearerType();
                qDebug() << "Current active connection:" << activeNetworkID << conf.name();
                return;
            }
        }
    }
}

void Client::registrationSuccessful(const QVariantMap &result)
{
    saveRegistrationData(result);

    connectionStatus = Disconnected;
    Q_EMIT connectionStatusChanged(connectionStatus);

    networkStatusChanged(isNetworkAvailable());
}

void Client::loginFailed()
{
    connectionStatus = LoginFailure;
    Q_EMIT connectionStatusChanged(connectionStatus);
    qDebug() << "Login failed";
    isRegistered = false;
    connectionClosed();
    Q_EMIT authFail(this->userName, "exception");
    Q_EMIT disconnected("login");
}

void Client::onAuthSuccess(const QString &creation, const QString &expiration, const QString &kind, const QString status, const QByteArray &nextChallenge)
{
    Q_EMIT authSuccess(this->userName);

    QSettings settings("coderus", "mitakuuluu2", this);
    settings.setValue(SETTINGS_NEXTCHALLENGE, QString::fromUtf8(nextChallenge.toBase64()));
    settings.setValue(SETTINGS_CREATION, creation);
    settings.setValue(SETTINGS_EXPIRATION, expiration);
    settings.setValue(SETTINGS_KIND, kind);
    settings.setValue(SETTINGS_ACCOUNTSTATUS, status);

    connectionStatus = LoggedIn;
    Q_EMIT connectionStatusChanged(connectionStatus);

    connect(connectionPtr.data(),SIGNAL(accountExpired(QVariantMap)),this,SLOT(expired(QVariantMap)));

    connect(connectionPtr.data(),SIGNAL(groupNewSubject(QString,QString,QString,QString,QString,QString,bool)),
            this,SLOT(groupNewSubject(QString,QString,QString,QString,QString,QString,bool)));

    connect(connectionPtr.data(),SIGNAL(groupInfoFromList(QString,QString,QString,QString,
                                                QString,QString,QString)),
            this,SLOT(groupInfoFromList(QString,QString,QString,QString,
                                        QString,QString,QString)));

    connect(connectionPtr.data(),SIGNAL(messageReceived(FMessage)),
            this,SLOT(onMessageReceived(FMessage)));

    connect(connectionPtr.data(),SIGNAL(messageStatusUpdate(QString, QString, int)),
            this,SLOT(messageStatusUpdate(QString, QString, int)));

    connect(connectionPtr.data(),SIGNAL(available(QString,bool)),
            this,SLOT(available(QString,bool)));

    connect(connectionPtr.data(),SIGNAL(composing(QString,QString)),
            this,SLOT(composing(QString,QString)));

    connect(connectionPtr.data(),SIGNAL(paused(QString)),
            this,SLOT(paused(QString)));

    connect(connectionPtr.data(),SIGNAL(groupLeft(QString)),
            this,SLOT(groupLeft(QString)));

    connect(connectionPtr.data(),SIGNAL(userStatusUpdated(QString, QString, int)),
            this,SLOT(userStatusUpdated(QString, QString, int)));

    connect(connectionPtr.data(),SIGNAL(lastOnline(QString,qint64)),
            this,SLOT(available(QString,qint64)));

    connect(connectionPtr.data(),SIGNAL(mediaUploadAccepted(FMessage)),
            this,SLOT(mediaUploadAccepted(FMessage)));

    connect(connectionPtr.data(),SIGNAL(photoIdReceived(QString,QString,QString,QString,QString,QString,bool)),
            this,SLOT(photoIdReceived(QString,QString,QString,QString,QString,QString,bool)));

    connect(connectionPtr.data(),SIGNAL(photoDeleted(QString,QString,QString,QString,QString)),
            this,SLOT(photoDeleted(QString,QString,QString,QString,QString)));

    connect(connectionPtr.data(),SIGNAL(photoReceived(QString,QByteArray,QString,bool)),
            this,SLOT(photoReceived(QString,QByteArray,QString,bool)));

    connect(connectionPtr.data(),SIGNAL(groupUsers(QString,QStringList)),
            this,SLOT(groupUsers(QString,QStringList)));

    connect(connectionPtr.data(),SIGNAL(groupAddUser(QString,QString,QString,QString,bool)),
            this,SLOT(groupAddUser(QString,QString,QString,QString,bool)));

    connect(connectionPtr.data(),SIGNAL(groupRemoveUser(QString,QString,QString,QString,bool)),
            this,SLOT(groupRemoveUser(QString,QString,QString,QString,bool)));

    connect(connectionPtr.data(),SIGNAL(groupError(QString)),
            this,SLOT(groupError(QString)));

    connect(connectionPtr.data(),SIGNAL(privacyListReceived(QStringList)),
            this,SLOT(privacyListReceived(QStringList)));

    connect(connectionPtr.data(),SIGNAL(privacySettingsReceived(QVariantMap)),
            this,SLOT(privacySettingsReceived(QVariantMap)));

    connect(connectionPtr.data(),SIGNAL(contactAdded(QString)),
            this,SLOT(newContactAdded(QString)));

    connect(connectionPtr.data(),SIGNAL(streamError()),
            this,SIGNAL(streamError()));

    connect(connectionPtr.data(), SIGNAL(groupCreated(QString)), this, SIGNAL(groupCreated(QString)));

    connect(connectionPtr.data(),SIGNAL(contactsStatus(QVariantList)), this, SLOT(syncContactsAvailable(QVariantList)));

    connect(connectionPtr.data(),SIGNAL(contactsSynced(QVariantList)), this, SLOT(syncResultsAvailable(QVariantList)));

    connect(connectionPtr.data(),SIGNAL(syncFinished()), this, SIGNAL(synchronizationFinished()));

    connect(connectionPtr.data(), SIGNAL(notifyOfflineMessages(int)), this, SLOT(notifyOfflineMessages(int)));

    updateNotification(tr("Connected", "System connection notification"));

    QVariantMap query;
    query["type"] = QueryType::ContactsGetJids;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);

    pendingMessagesTimer->start(CHECK_QUEUE_INTERVAL);

    Q_EMIT connectionSendGetServerProperties();
    Q_EMIT connectionSendGetClientConfig();
    Q_EMIT connectionSendGetPrivacyList();
    Q_EMIT connectionSendGetPrivacySettings();

    this->userName = settings.value(SETTINGS_USERNAME, this->myJid.split("@").first()).toString();
    changeUserName(this->userName);

    getPicture(this->myJid);

    getContactStatus(this->myJid);

    if (useKeepalive) {
        keepalive->wait(BackgroundActivity::TenMinutes);
    }

    reconnectInterval = 1;
}

void Client::authFailed()
{
    connectionStatus = LoginFailure;
    Q_EMIT connectionStatusChanged(connectionStatus);
    isRegistered = false;
    connectionClosed();
    Q_EMIT authFail(this->userName, "exception");
    Q_EMIT disconnected("login");
}

void Client::clientDestroyed()
{
    qDebug() << "client destroyed";
}

void Client::saveRegistrationData(const QVariantMap &result)
{
    cc = result["cc"].toString();
    number = result["number"].toString();
    phoneNumber = result["login"].toString();
    password = result["pw"].toString();
    myJid = phoneNumber + "@s.whatsapp.net";
    isRegistered = true;
    Q_EMIT registrationComplete();
    Q_EMIT myAccount(myJid);

    saveAccountData();
}

void Client::verifyAndConnect()
{
    if (connectionStatus != Disconnected && connectionStatus != WaitingForConnection)
        return;
    qDebug() << "Verify and connect";
    // Verify if the user is registered
    if (!this->phoneNumber.isEmpty() && !this->password.isEmpty()) {
        connectToServer();
    }
    else {
        Q_EMIT noAccountData();
    }
    if (session) {
        delete session;
        session = 0;
    }
}

void Client::connectToServer()
{
    qDebug() << "connectToServer";
    connectionMutex.lock();

    if (connectionStatus == Connected || connectionStatus == Connecting ||
        connectionStatus == Registering)
    {
        connectionMutex.unlock();
        return;
    }

    // Verify there's an available connection

    if (!isNetworkAvailable())
    {
        qDebug() << "No network available. Waiting for a connection...";
        connectionStatus = WaitingForConnection;
        Q_EMIT connectionStatusChanged(connectionStatus);
        connectionMutex.unlock();
        return;
    }

    getTokenScratch();

    connectionStatus = Connecting;
    Q_EMIT connectionStatusChanged(connectionStatus);

    // If there's a network mode change it will never reach this point
    // so it's safe to unlock de mutex here
    connectionMutex.unlock();

    // There's a connection available, now connect

    QSettings settings("coderus", "mitakuuluu2", this);
    QByteArray nextChallenge = QByteArray::fromBase64(settings.value(SETTINGS_NEXTCHALLENGE).toByteArray());
    settings.remove(SETTINGS_NEXTCHALLENGE);

    QByteArray password = QByteArray::fromBase64(this->password.toUtf8());

    settings.sync();
    QString server = settings.value(SETTINGS_SERVER, SERVER_DOMAIN).toString();

    qDebug() << "creating connection";

    Connection *conn = new Connection(server,443,JID_DOMAIN,waresource,phoneNumber,
                                      userName,password,nextChallenge,
                                      //systemInfo->currentLanguage(),systemInfo->currentCountryCode(),
                                      //networkInfo->currentMobileCountryCode(),networkInfo->currentMobileNetworkCode(),
                                      "en", "US", "000", "000",
                                      waversion,&dataCounters,0);

    qDebug() << "Assigning to pointer";

    connectionPtr.reset(conn);

    qDebug() << "connecting to signals";

    QObject::connect(this, SIGNAL(connectionSendMessage(FMessage)), connectionPtr.data(), SLOT(sendMessage(FMessage)));
    QObject::connect(this, SIGNAL(connectionSendSyncContacts(QStringList)), connectionPtr.data(), SLOT(sendSyncContacts(QStringList)));
    QObject::connect(this, SIGNAL(connectionSendQueryLastOnline(QString)), connectionPtr.data(), SLOT(sendQueryLastOnline(QString)));
    QObject::connect(this, SIGNAL(connectionSendGetStatus(QStringList)), connectionPtr.data(), SLOT(sendGetStatus(QStringList)));
    QObject::connect(this, SIGNAL(connectionSendPresenceSubscriptionRequest(QString)), connectionPtr.data(), SLOT(sendPresenceSubscriptionRequest(QString)));
    QObject::connect(this, SIGNAL(connectionSendUnsubscribeHim(QString)), connectionPtr.data(), SLOT(sendUnsubscribeHim(QString)));
    QObject::connect(this, SIGNAL(connectionSendGetPhoto(QString,QString,bool)), connectionPtr.data(), SLOT(sendGetPhoto(QString,QString,bool)));
    QObject::connect(this, SIGNAL(connectionSendSetPhoto(QString,QByteArray,QByteArray)), connectionPtr.data(), SLOT(sendSetPhoto(QString,QByteArray,QByteArray)));
    QObject::connect(this, SIGNAL(connectionSendGetPhotoIds(QStringList)), connectionPtr.data(), SLOT(sendGetPhotoIds(QStringList)));
    QObject::connect(this, SIGNAL(connectionSendVoiceNotePlayed(FMessage)), connectionPtr.data(), SLOT(sendVoiceNotePlayed(FMessage)));
    QObject::connect(this, SIGNAL(connectionSendCreateGroupChat(QString)), connectionPtr.data(), SLOT(sendCreateGroupChat(QString)));
    QObject::connect(this, SIGNAL(connectionSendAddParticipants(QString,QStringList)), connectionPtr.data(), SLOT(sendAddParticipants(QString,QStringList)));
    QObject::connect(this, SIGNAL(connectionSendRemoveParticipants(QString,QStringList)), connectionPtr.data(), SLOT(sendRemoveParticipants(QString,QStringList)));
    QObject::connect(this, SIGNAL(connectionSendVerbParticipants(QString,QStringList,QString,QString)), connectionPtr.data(), SLOT(sendVerbParticipants(QString,QStringList,QString,QString)));
    QObject::connect(this, SIGNAL(connectionSendGetParticipants(QString)), connectionPtr.data(), SLOT(sendGetParticipants(QString)));
    QObject::connect(this, SIGNAL(connectionSendGetGroupInfo(QString)), connectionPtr.data(), SLOT(sendGetGroupInfo(QString)));
    QObject::connect(this, SIGNAL(connectionUpdateGroupChats()), connectionPtr.data(), SLOT(updateGroupChats()));
    QObject::connect(this, SIGNAL(connectionSendSetGroupSubject(QString,QString)), connectionPtr.data(), SLOT(sendSetGroupSubject(QString,QString)));
    QObject::connect(this, SIGNAL(connectionSendLeaveGroup(QString)), connectionPtr.data(), SLOT(sendLeaveGroup(QString)));
    QObject::connect(this, SIGNAL(connectionSendRemoveGroup(QString)), connectionPtr.data(), SLOT(sendRemoveGroup(QString)));
    QObject::connect(this, SIGNAL(connectionSendGetPrivacyList()), connectionPtr.data(), SLOT(sendGetPrivacyList()));
    QObject::connect(this, SIGNAL(connectionSendSetPrivacyBlockedList(QStringList)), connectionPtr.data(), SLOT(sendSetPrivacyBlockedList(QStringList)));
    QObject::connect(this, SIGNAL(connectionSendGetPrivacySettings()), connectionPtr.data(), SLOT(sendGetPrivacySettings()));
    QObject::connect(this, SIGNAL(connectionSendSetPrivacySettings(QString,QString)), connectionPtr.data(), SLOT(sendSetPrivacySettings(QString,QString)));
    QObject::connect(this, SIGNAL(connectionSetNewUserName(QString,bool)), connectionPtr.data(), SLOT(setNewUserName(QString,bool)));
    QObject::connect(this, SIGNAL(connectionSetNewStatus(QString)), connectionPtr.data(), SLOT(sendSetStatus(QString)));
    QObject::connect(this, SIGNAL(connectionSendAvailableForChat(bool)), connectionPtr.data(), SLOT(sendAvailableForChat(bool)));
    QObject::connect(this, SIGNAL(connectionSendAvailable()), connectionPtr.data(), SLOT(sendAvailable()));
    QObject::connect(this, SIGNAL(connectionSendUnavailable()), connectionPtr.data(), SLOT(sendUnavailable()));
    QObject::connect(this, SIGNAL(connectionSendDeleteAccount()), connectionPtr.data(), SLOT(sendDeleteAccount()));
    QObject::connect(this, SIGNAL(connectionDisconnect()), connectionPtr.data(), SLOT(disconnectAndDelete()));
    QObject::connect(this, SIGNAL(connectionSendGetServerProperties()), connectionPtr.data(), SLOT(sendGetServerProperties()));
    QObject::connect(this, SIGNAL(connectionSendGetClientConfig()), connectionPtr.data(), SLOT(getClientConfig()));
    QObject::connect(this, SIGNAL(connectionSendComposing(QString,QString)), connectionPtr.data(), SLOT(sendComposing(QString,QString)));
    QObject::connect(this, SIGNAL(connectionSendPaused(QString,QString)), connectionPtr.data(), SLOT(sendPaused(QString,QString)));

    QObject::connect(connectionPtr.data(), SIGNAL(authSuccess(QString,QString,QString,QString,QByteArray)), this, SLOT(onAuthSuccess(QString,QString,QString,QString,QByteArray)));
    QObject::connect(connectionPtr.data(), SIGNAL(authFailed()), this, SLOT(authFailed()));
    QObject::connect(connectionPtr.data(), SIGNAL(connected()), this, SLOT(connected()));
    QObject::connect(connectionPtr.data(), SIGNAL(disconnected()), this, SLOT(disconnected()));
    QObject::connect(connectionPtr.data(), SIGNAL(destroyed()), this, SLOT(clientDestroyed()));

    QObject::connect(connectionPtr.data(), SIGNAL(socketBroken()), this, SLOT(destroyConnection()));

    qDebug() << "Threading connection";

    QThread *thread = new QThread(connectionPtr.data());
    connectionPtr.data()->moveToThread(thread);
    QObject::connect(thread, SIGNAL(started()), connectionPtr.data(), SLOT(init()));
    thread->start();
}

void Client::connected()
{
    connectionStatus = Connected;
    Q_EMIT connectionStatusChanged(connectionStatus);
}

void Client::disconnected()
{
    qDebug() << "Client disconnected";
    updateNotification(tr("Disconnected", "System connection notification"));

    if (keepalive->isWaiting()) {
        keepalive->stop();
    }

    connectionPtr.take();

    dataCounters.writeCounters();

    if (retryLoginTimer->isActive()) {
        retryLoginTimer->stop();
    }

    if (connectionStatus != Disconnected && connectionStatus != LoginFailure) {
        connectionStatus = Disconnected;
        Q_EMIT connectionStatusChanged(connectionStatus);

        lastReconnect = reconnectionInterval;
        retryLoginTimer->start(reconnectInterval * 1000);
        if (reconnectionInterval < 120) {
            reconnectionInterval += lastReconnect;
        }
    }
    else {
        disconnectCount = 0;
    }
}

void Client::destroyConnection()
{
    doCheckNetworkStatus();
}

void Client::synchronizeContacts()
{
    if (connectionStatus == LoggedIn)
    {
        QVariantMap query;
        query["type"] = QueryType::ContactsSyncContacts;
        query["uuid"] = uuid;
        dbExecutor->queueAction(query);

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        lastSync = now;
        QSettings settings("coderus", "mitakuuluu2", this);
        settings.setValue(SETTINGS_LAST_SYNC, lastSync);
    }
}

void Client::contactsAvailable(const QStringList &contacts, const QVariantMap &labels, const QVariantMap &avatars)
{
    qDebug() << "received" << contacts.size() << "contacts";
    _synccontacts = labels;
    _syncavatars = avatars;

    if (contacts.length() > 0) {
        Q_EMIT connectionSendSyncContacts(contacts);
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    lastSync = now;
    QSettings settings("coderus", "mitakuuluu2", this);
    settings.setValue(SETTINGS_LAST_SYNC, lastSync);
}

void Client::checkActivity()
{
    if (!connectionPtr.isNull()) {
        connectionPtr->checkActivity();
    }
    keepalive->wait();
}

void Client::wakeupStopped()
{
    qDebug() << "WAKEUP STOPPED! WHAT SHOULD I DO NOW!?";
}

void Client::onDelayedNotificationTriggered()
{
    QTimer *delayedTimer = qobject_cast<QTimer*>(sender());
    if (delayedTimer) {
        QString jid = delayedTimer->objectName();
        if (_notificationPendingJid.contains(jid) && _notificationPendingJid[jid] && !_notificationPendingJid[jid]->isPublished()) {
            if (_notificationJid.contains(jid)) {
                MNotification *notif = _notificationJid[jid];
                _notificationJid.remove(jid);
                if (notif) {
                    notif->remove();
                    delete notif;
                }
            }
            _notificationPendingJid[jid]->publish();
            _notificationJid[jid] = _notificationPendingJid[jid];
            _notificationPendingJid.remove(jid);
        }
    }
}

void Client::synchronizePhonebook()
{
    // Contacts syncer
    ContactsFetch *contacts = new ContactsFetch(0);
    QThread *thread = new QThread(contacts);
    contacts->moveToThread(thread);
    QObject::connect(thread, SIGNAL(started()), contacts, SLOT(allContacts()));
    QObject::connect(contacts, SIGNAL(contactsAvailable(QStringList,QVariantMap,QVariantMap)), this, SLOT(contactsAvailable(QStringList,QVariantMap,QVariantMap)));
    thread->start();
}

void Client::setActiveJid(const QString &jid)
{
    qDebug() << "activeJid:" << jid;
    _activeJid = jid;
    setUnreadCount(jid, 0);
    if (_notificationJid.contains(jid)) {
        qDebug() << "clear" << jid << "notifications";
        MNotification *notif = _notificationJid[jid];
        _notificationJid.remove(jid);
        if (notif) {
            notif->remove();
            delete notif;
        }
    }
    if (_notificationPendingJid.contains(jid)) {
        qDebug() << "clear" << jid << "notifications";
        MNotification *notif = _notificationPendingJid[jid];
        _notificationPendingJid.remove(jid);
        if (notif) {
            notif->remove();
            delete notif;
        }
    }
    if (_pendingNotificationsTimer.contains(jid) && _pendingNotificationsTimer[jid] && _pendingNotificationsTimer[jid]->isActive()) {
        _pendingNotificationsTimer[jid]->stop();
    }

}

void Client::syncResultsAvailable(const QVariantList &results)
{
    QVariantList contacts = results;
    if (!contacts.isEmpty()) {
        qDebug() << "got" << QString::number(contacts.count()) << "contacts on sync";

        for (int i = 0; i < contacts.size(); i++) {
            QVariantMap item = contacts.at(i).toMap();
            QString phone = item["phone"].toString();
            QString avatar;
            if (_syncavatars.keys().contains(phone))
                avatar = _syncavatars[phone].toString();
            if (_syncavatars.keys().contains("+" + phone))
                avatar = _syncavatars["+" + phone].toString();
            QString name;
            if (_synccontacts.keys().contains(phone)) {
                name = _synccontacts[phone].toString();
            }
            if (_synccontacts.keys().contains("+" + phone)) {
                name = _synccontacts["+" + phone].toString();
            }
            qDebug() << phone << name << avatar;
            item["avatar"] = avatar;
            item["alias"] = name;

            contacts[i] = item;
        }
        _synccontacts.clear();
        _syncavatars.clear();

        QVariantMap query;
        query["type"] = QueryType::ContactsSyncResults;
        query["contacts"] = contacts;
        query["blocked"] = _blocked;
        query["uuid"] = uuid;
        dbExecutor->queueAction(query);

        foreach (const QVariant &contact, contacts) {
            QVariantMap item = contact.toMap();
            QString jid = item["jid"].toString();
            refreshContact(jid);
            requestPresenceSubscription(jid);
        }
    }
}

void Client::syncContactsAvailable(const QVariantList &results)
{
    qDebug() << "syncContactsAvailable:" << QString::number(results.size());
    if (!results.isEmpty()) {
        qDebug() << "got" << QString::number(results.count()) << "contacts on sync";
        QString lastJid;
        foreach (QVariant vcontact, results) {
            QVariantMap contact = vcontact.toMap();
            QString jid = contact["jid"].toString();
            QString message = contact["message"].toString();
            if (contact.contains("hidden")) {
                message = tr("Hidden", "User hidden own status for privacy");
            }
            qDebug() << "Message" << message << "Jid:" << jid;

            if (jid == myJid) {
                myStatus = message;
                QSettings settings("coderus", "mitakuuluu2", this);
                settings.setValue(SETTINGS_STATUS,myStatus);
            }
            Q_EMIT contactStatus(jid, message, contact["timestamp"].toInt());

            contact["type"] = QueryType::ContactsSetSync;
            contact["uuid"] = uuid;
            dbExecutor->queueAction(contact);

            requestPresenceSubscription(jid);

            lastJid = jid;
        }
        if (results.size() > 1)
            Q_EMIT contactsChanged();
    }
}

void Client::changeStatus(const QString &newStatus)
{
    if (connectionStatus == LoggedIn) {
        myStatus = newStatus;
        QSettings settings("coderus", "mitakuuluu2", this);
        settings.setValue(SETTINGS_PRESENCE, myStatus);
        Q_EMIT connectionSetNewStatus(newStatus);
    }
}

void Client::changeUserName(const QString &newUserName)
{
    userName = newUserName;
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSetNewUserName(userName, alwaysOffline);
    }
}

void Client::sendRecentLogs()
{
    compressOldLogs();
    //QDesktopServices::openUrl(QString("mailto:coderusinbox@gmail.com?subject=Mitakuuluu log&body=Hello! I found bug in Mitakuuluu: <bug description>&attach=/home/nemo/.whatsapp/logs/whatsapp_log1.tar.gz"));
    QFile log("/home/nemo/.whatsapp/logs/whatsapp_log1.tar.gz");
    if (log.exists() && log.open(QFile::ReadOnly)) {
        QByteArray data = log.readAll().toBase64();
        Q_EMIT logfileReady(data, true);
        log.close();
    }
    else {
        Q_EMIT logfileReady(QByteArray(), false);
    }
}

void Client::connectionClosed()
{
    connectionMutex.lock();
    qDebug() << "Connection closed.";

    // Stop all timers
    qDebug() << "Stopping timers.";
    pendingMessagesTimer->stop();

    if (retryLoginTimer->isActive()) {
        retryLoginTimer->stop();
    }
    reconnectInterval = 1;
    lastReconnect = 1;

    // Sometimes the network is available but there was an error
    // because a DNS problem or can't reach the server at the moment.
    // In these cases the client should retry
    if (connectionStatus != LoginFailure && connectionStatus != Disconnected) {
        if (isNetworkAvailable())
        {
            connectionStatus = Disconnected;

            if (!isRegistered)
            {
                // Immediately try to re-register
                QTimer::singleShot(0,this,SLOT(verifyAndConnect()));
            }
            else
            {
                verifyAndConnect();
            }
        }
        else
        {
            qDebug() << "Waiting for a connection.";
            connectionStatus = WaitingForConnection;
        }
    }

    Q_EMIT connectionStatusChanged(connectionStatus);
    connectionMutex.unlock();
}

void Client::sendMessagesInQueue()
{
    if (connectionStatus == LoggedIn)
    {

        while (!pendingMessagesQueue.isEmpty())
        {
            if (connectionStatus != LoggedIn)
                break;
            pendingMessagesMutex.lock();
            FMessage message = pendingMessagesQueue.dequeue();
            pendingMessagesMutex.unlock();

            if (message.type == FMessage::BodyMessage) {
                QString text = QString::fromUtf8(message.data.data());
                text = text.replace("<br />", "\n")
                           .replace("&quot;","\"")
                           .replace("&lt;", "<")
                           .replace("&gt;", ">")
                           .replace("&amp;", "&");
                message.data = text.toUtf8();
            }

            // What if connection is not "connected" ?
            // maybe a timer and recall this function?
            // or a sleep?
            Q_EMIT connectionSendMessage(message);
        }

        if (connectionStatus == LoggedIn) {
            pendingMessagesTimer->start(CHECK_QUEUE_INTERVAL);
        }
    }
}

void Client::setGroupSubject(const QString &gjid, const QString &subject)
{
    if (connectionStatus == LoggedIn)
    {
        Q_EMIT connectionSendSetGroupSubject(gjid, subject);
    }

}

void Client::requestLeaveGroup(const QString &gjid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendLeaveGroup(gjid);
    }
}

void Client::requestRemoveGroup(const QString &gjid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendRemoveGroup(gjid);
    }
}

void Client::setPicture(const QString &jid, const QString &path)
{
    qDebug() << "requested setPicture" << path << "for" << jid;
    QString filepath = path;
    filepath = filepath.remove("file://");
    if (connectionStatus == LoggedIn) {
        QFile test(filepath);
        if (test.exists()) {
            QImage img;
            img.load(filepath);
            if (img.width() < 192 && img.width() < img.height()) {
                img = img.scaledToWidth(192, Qt::SmoothTransformation);
            }
            else if (img.height() < 192) {
                img = img.scaledToHeight(192, Qt::SmoothTransformation);
            }
            else if (img.height() > 320 && img.height() < img.width()) {
                img = img.scaledToHeight(320, Qt::SmoothTransformation);
            }
            else if (img.width() > 320) {
                img = img.scaledToWidth(320, Qt::SmoothTransformation);
            }
            qDebug() << "sending image" << img.height() << img.width();

            QByteArray image;
            image.clear();
            QBuffer imagebuffer(&image);
            img.save(&imagebuffer, "JPEG");

            /*int quality = 90;
            do
            {
                data.clear();
                QBuffer out(&data);
                img.save(&out, "JPEG", quality);
                quality -= 5;
            } while (quality > 10 && data.size() > MAX_PROFILE_PICTURE_SIZE);*/
            //qDebug() << "Setting picture for:" << jid << "path:" << filepath << "size:" << QString::number(data.size()) << "quality:" << QString::number(quality + 5);

            QByteArray thumbnail;

            QImage thumb = img.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            /*int quality = 80;
            do
            {
                thumbnail.clear();
                QBuffer out(&thumbnail);
                thumb.save(&out, "JPEG", quality);
                quality -= 5;
            } while ((quality > 10) && thumbnail.size() > MAX_PROFILE_PICTURE_SIZE);*/

            thumbnail.clear();
            QBuffer thumbnailbuffer(&thumbnail);
            thumb.save(&thumbnailbuffer, "JPEG", 80);

            Q_EMIT connectionSendSetPhoto(jid, image, thumbnail);
        }
    }
}

void Client::getContactStatus(const QString &jid)
{
    if (connectionStatus == LoggedIn && !_blocked.contains(jid))
    {
        Q_EMIT connectionSendGetStatus(QStringList() << jid);
    }
}

void Client::refreshContact(const QString &jid)
{
    getPicture(jid);
    if (!jid.contains("-")) {
        getContactStatus(jid);
    }
}

void Client::requestQueryLastOnline(const QString &jid)
{
  if (connectionStatus == LoggedIn) {
      Q_EMIT connectionSendQueryLastOnline(jid);
  }
}

void Client::queueMessage(const FMessage &message)
{
    pendingMessagesMutex.lock();
    pendingMessagesQueue.enqueue(message);
    pendingMessagesMutex.unlock();

    // EMIT SIGNAL THAT THERE'S SOMETHING IN THE QUEUE HERE
    // OR CREATE A NEW QUEUE CLASS
    // DON'T USE TIMERS <-- good idea I think.
}

void Client::getTokenScratch()
{
    if (nam) {
        qDebug() << "getToken";
        QNetworkRequest req;
        QUrl request("https://coderus.openrepos.net/whitesoft/whatsapp_scratch");
        req.setUrl(request);
        connect(nam->get(req), SIGNAL(finished()), this, SLOT(scratchRequestFinished()));
    }
}

void Client::clearNotification()
{
    if (connectionNotification) {
        delete connectionNotification;
        connectionNotification = 0;
    }
}

void Client::groupNotification(const QString &gjid, const QString &jid, int type, const QString &timestamp, const QString &notificationId, bool offline, QString notification)
{
    qDebug() << "groupNotification" << gjid << "from" << jid << "type" << QString::number(type);
    QString message;
    switch (type) {
    case ParticipantAdded:
        message = tr("Joined the group", "Notification group event text");
        break;
    case ParticipantRemoved:
        message = tr("Left the group", "Notification group event text");
        break;
    case SubjectSet:
        message = tr("Subject: %1", "Notification group event text").arg(notification);
        break;
    case PictureSet:
        message = tr("Picture changed", "Notification group event text");
        break;
    default:
        break;
    }

    QVariantMap data;
    data["msgid"] = QString("%1-%2").arg(timestamp).arg(notificationId);
    data["jid"] = gjid;
    data["author"] = jid;
    data["timestamp"] = timestamp;
    data["data"] = message;
    data["status"] = Notification;
    data["watype"] = FMessage::System;
    data["url"] = QString();
    data["name"] = QString();
    data["latitude"] = QString();
    data["longitude"] = QString();
    data["size"] = 0;
    data["mime"] = QString();
    data["duration"] = 0;
    data["width"] = 0;
    data["height"] = 0;
    data["local"] = QString();
    data["broadcast"] = QVariant::fromValue(false);
    data["live"] = QVariant::fromValue(false);

    Q_EMIT messageReceived(data);

    QVariantMap query;
    query["type"] = QueryType::ContactsSetLastmessage;
    query["jid"] = gjid;
    query["lastmessage"] = timestamp;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);

    data["type"] = QueryType::ConversationSaveMessage;
    data["uuid"] = uuid;
    dbExecutor->queueAction(data);

    qDebug() << "should show notification?" << "author:" << jid << "jid:" << gjid << "activeJid:" << _activeJid << "myJid:" << myJid << "offline:" << offline;
    if (!offline && (jid != myJid) && (gjid != _activeJid)) {
        qDebug() << "show notification for:" << data["msgid"].toString() << "jid:" << jid;
        QVariantMap notify;
        notify["jid"] = gjid;
        notify["pushName"] = QString();
        notify["type"] = QueryType::ConversationNotifyMessage;
        notify["media"] = false;
        notify["msg"] = message;
        notify["uuid"] = uuid;
        dbExecutor->queueAction(notify);
    }
}

void Client::startDownloadMessage(const FMessage &msg)
{
    MediaDownload *mediaDownload = new MediaDownload(msg);

    connect(mediaDownload,SIGNAL(progress(FMessage,float)),
            this,SLOT(mediaDownloadProgress(FMessage,float)));

    connect(mediaDownload,SIGNAL(downloadFinished(MediaDownload *, FMessage)),
            this,SLOT(mediaDownloadFinished(MediaDownload *, FMessage)));

    connect(mediaDownload,SIGNAL(httpError(MediaDownload*, FMessage, int)),
            this,SLOT(mediaDownloadError(MediaDownload*, FMessage, int)));

    _mediaDownloadHash[msg.key.id] = mediaDownload;

    QThread *thread = new QThread(mediaDownload);
    mediaDownload->moveToThread(thread);
    QObject::connect(thread, SIGNAL(started()), mediaDownload, SLOT(backgroundTransfer()));
    thread->start();
}

void Client::showOfflineNotifications()
{
    foreach (const QString &jid, _notificationJid.keys()) {
        if (_notificationJid[jid] && !_notificationJid[jid]->isPublished()) {
            qDebug() << "valid notification for" << jid << _notificationJid[jid]->body() << "published:" << _notificationJid[jid]->isPublished();
            _notificationJid[jid]->publish();
        }
        else {
            qDebug() << "invalid notification for" << jid;
        }
    }
    _totalUnread = 0;
    _pendingCount = 0;
}

void Client::ready()
{
    qDebug() << "Client loaded and requested initial data";
    Q_EMIT connectionStatusChanged(connectionStatus);
    Q_EMIT myAccount(myJid);
    Q_EMIT simParameters(mcc, mnc);
    foreach (const QString &jid, _contacts.keys()) {
        Q_EMIT setUnread(jid, _contacts[jid].toInt());
    }
}

void Client::saveCredentials(const QVariantMap &data)
{
    registrationSuccessful(data);
}

void Client::sendLocationRequest(const QByteArray &mapData, const QString &latitude, const QString &longitude,
                                 const QStringList &jids, MapRequest* sender)
{
    qDebug() << "Location request finished";
    QString jid = jids.size() > 1 ? "broadcast" : jids.first();
    FMessage msg(jid, true);
    msg.status = FMessage::Unsent;
    msg.type = FMessage::MediaMessage;
    msg.latitude = latitude.toDouble();
    msg.longitude = longitude.toDouble();
    msg.media_wa_type = FMessage::Location;

    QPixmap img;
    img.loadFromData(mapData);
    QPainter painter;
    if (painter.begin(&img)) {
        QPixmap marker("/usr/share/harbour-mitakuuluu2/images/location-marker.png");
        QRect targetRect(img.width() / 2 - marker.width() / 2, img.height() / 2 - marker.height() / 2, marker.width(), marker.height());
        painter.drawPixmap(targetRect, marker, marker.rect());
        painter.end();
    }

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPG", 100);

    msg.setData(data.toBase64());
    if (jids.size() > 1) {
        msg.broadcast = true;
        msg.broadcastJids = jids;
    }
    queueMessage(msg);
    addMessage(msg);
    sender->deleteLater();
}

void Client::mapError(MapRequest *sender)
{
    sender->deleteLater();
}

void Client::mediaUploadAccepted(const FMessage &message)
{
    qDebug() << "Media upload accepted:" << message.media_url << "jid:" << message.key.remote_jid << "resource:" << message.remote_resource << "file:" << message.media_name << "hash:" << QString::fromUtf8(message.data.data());

    MediaUpload *mediaUpload = new MediaUpload(message);

    connect(mediaUpload,SIGNAL(sendMessage(MediaUpload*,FMessage)),
            this,SLOT(mediaUploadFinished(MediaUpload*, FMessage)));

    connect(mediaUpload,SIGNAL(readyToSendMessage(MediaUpload*,FMessage)),
            this,SLOT(mediaUploadStarted(MediaUpload*, FMessage)));

    connect(mediaUpload,SIGNAL(sslError(MediaUpload*, FMessage)),
            this,SLOT(sslErrorHandler(MediaUpload*, FMessage)));

    connect(mediaUpload,SIGNAL(httpError(MediaUpload*, FMessage)),
            this,SLOT(httpErrorHandler(MediaUpload*, FMessage)));

    qDebug() << "Starting uploader in external thread";
    QThread *thread = new QThread(mediaUpload);
    mediaUpload->moveToThread(thread);
    QObject::connect(thread, SIGNAL(started()), mediaUpload, SLOT(upload()));
    thread->start();
}

void Client::mediaUploadStarted(MediaUpload *mediaUpload, const FMessage &msg)
{
    qDebug() << "mediaUploadStarted:" << msg.media_url << "to:" << msg.local_file_uri;

    Q_EMIT uploadStarted(msg.key.remote_jid, msg.key.id, msg.local_file_uri);

    connect(mediaUpload,SIGNAL(progress(FMessage,float)),
            this,SLOT(mediaUploadProgress(FMessage,float)));
    addMessage(msg);
}

void Client::mediaUploadFinished(MediaUpload *mediaUpload, const FMessage &msg)
{
    qDebug() << "mediaUploadFinished:" << msg.media_url << "to:" << msg.local_file_uri;

    Q_EMIT uploadFinished(msg.key.remote_jid, msg.key.id, msg.media_url);

    queueMessage(msg);

    mediaUpload->deleteLater();

    _mediaProgress.remove(msg.key.id);

    // Increase counters
    int type;
    switch (msg.media_wa_type)
    {
        case FMessage::Image:
            type = DataCounters::ImageBytes;
            break;

        case FMessage::Audio:
            type = DataCounters::AudioBytes;
            break;

        case FMessage::Video:
            type = DataCounters::VideoBytes;
            break;

        default:
            type = DataCounters::ProtocolBytes;
            break;
    }

    Client::dataCounters.increaseCounter(type, 0, msg.media_size);
}

void Client::mediaDownloadFinished(MediaDownload *mediaDownload, const FMessage &msg)
{
    qDebug() << "Media download finished" << msg.local_file_uri << "for:" << msg.key.id;
    _mediaProgress.remove(msg.key.id);

    QVariantMap query;
    query["uuid"] = uuid;
    query["type"] = QueryType::ConversationDownloadFinished;
    query["url"] = msg.local_file_uri;
    query["msgid"] = msg.key.id;
    query["jid"] = msg.key.remote_jid;
    dbExecutor->queueAction(query);

    Q_EMIT downloadProgress(msg.key.remote_jid, msg.key.id, 100);
    Q_EMIT downloadFinished(msg.key.remote_jid, msg.key.id, msg.local_file_uri);

    QObject::disconnect(mediaDownload, 0, 0, 0);
    _mediaDownloadHash[msg.key.id]->deleteLater();
    _mediaDownloadHash[msg.key.id] = 0;

    // Increase counters
    int type;
    switch (msg.media_wa_type)
    {
        case FMessage::Image:
            type = DataCounters::ImageBytes;
            break;

        case FMessage::Audio:
            type = DataCounters::AudioBytes;
            break;

        case FMessage::Video:
            type = DataCounters::VideoBytes;
            break;

        default:
            type = DataCounters::ProtocolBytes;
            break;
    }

    Client::dataCounters.increaseCounter(type, msg.media_size, 0);

}

void Client::mediaDownloadProgress(const FMessage &msg, float progress)
{
    qDebug() << "Media download progress:" << QString::number(progress) << "for:" << msg.key.id;

    _mediaProgress[msg.key.id] = (int)progress;

    Q_EMIT downloadProgress(msg.key.remote_jid, msg.key.id, (int)progress);
}

void Client::mediaDownloadError(MediaDownload *mediaDownload, const FMessage &msg, int errorCode)
{
    qDebug() << "Media download error" << QString::number(errorCode) << "for:" << msg.key.id;
    _mediaProgress.remove(msg.key.id);

    Q_EMIT downloadFailed(msg.key.remote_jid, msg.key.id);

    QObject::disconnect(mediaDownload, 0, 0, 0);
    _mediaDownloadHash[msg.key.id]->deleteLater();
    _mediaDownloadHash[msg.key.id] = 0;
}

void Client::sslErrorHandler(MediaUpload *mediaUpload, const FMessage &msg)
{
    Q_EMIT uploadFailed(msg.key.remote_jid, msg.key.id);
    mediaUpload->deleteLater();
}

void Client::httpErrorHandler(MediaUpload *mediaUpload, const FMessage &msg)
{
    Q_EMIT uploadFailed(msg.key.remote_jid, msg.key.id);
    mediaUpload->deleteLater();
}

void Client::mediaUploadProgress(const FMessage &msg, float progress)
{
    qDebug() << "mediaUploadProgress:" << msg.media_url << "file:" << msg.local_file_uri << "progress:" << QString::number(progress);
    Q_EMIT uploadProgress(msg.key.remote_jid, msg.key.id, (int)progress);
    _mediaProgress[msg.key.id] = (int)progress;
}

int Client::currentStatus()
{
    return connectionStatus;
}

void Client::recheckAccountAndConnect()
{
    QSettings settings("coderus", "mitakuuluu2", this);
    settings.sync();
    this->phoneNumber = settings.value(SETTINGS_PHONENUMBER, QString()).toString();
    this->myJid = settings.value(SETTINGS_MYJID, QString("%1@s.whatsapp.net").arg(this->phoneNumber)).toString();
    this->password = settings.value(SETTINGS_PASSWORD, QString()).toString();

    if (this->phoneNumber.isEmpty() || this->password.isEmpty())
        connectionStatus = RegistrationFailed;
    else
        connectionStatus = WaitingForConnection;
    Q_EMIT connectionStatusChanged(connectionStatus);

    disconnectCount = 0;
    reconnectInterval = 1;
    lastReconnect = 1;

    isOnline = manager->isOnline();
    networkStatusChanged(isOnline);
}

void Client::disconnect()
{
    dataCounters.writeCounters();
    qDebug() << "disconnect";
    if (connectionStatus == LoggedIn) {
        setPresenceUnavailable();
    }
    qDebug() << "Freeing up the socket.";
    connectionStatus = Disconnected;
    Q_EMIT connectionDisconnect();
    Q_EMIT connectionStatusChanged(connectionStatus);
}

void Client::getNetworkUsage()
{
    qDebug() << "getNetworkUsage start";
    QVariantList usage;
    for (int i = 0; i < 9; i++) {
        usage.append(dataCounters.getReceivedBytes(i));
        usage.append(dataCounters.getSentBytes(i));
    }
    Q_EMIT networkUsage(usage);
    qDebug() << "getNetworkUsage final";
}

void Client::resetNetworkUsage()
{
    dataCounters.resetCounters();
    getNetworkUsage();
}

void Client::ping()
{
    Q_EMIT pong();
}

void Client::renewAccount(const QString &method, int years)
{
    QString phone = myJid.split("@").first();
    QtMD5Digest md5;
    md5.update(phone.toLatin1());
    md5.update(QString("abc").toLatin1());
    QString hex = md5.digest().toHex();
    QDesktopServices::openUrl(QUrl(QString("https://www.whatsapp.com/payments/%1.php?phone=%2&cksum=%3&sku=%4")
                                   .arg(method) //google or paypal
                                   .arg(phone)
                                   .arg(hex)
                                   .arg(QString::number(years))));
}

void Client::unsubscribe(const QString &jid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendUnsubscribeMe(jid);
        Q_EMIT connectionSendUnsubscribeHim(jid);
    }
}

void Client::settingsChanged()
{
    readSettings();
}

void Client::saveHistory(const QString &sjid, const QString &sname)
{
    QVariantMap query;
    query["type"] = QueryType::ConversationSave;
    query["name"] = sname;
    query["jid"] = sjid;
    query["table"] = sjid.split("@").first().replace("-", "g");;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

void Client::requestContactMedia(const QString &sjid)
{
    QVariantMap query;
    query["type"] = QueryType::ConversationGetMedia;
    query["jid"] = sjid;
    query["table"] = sjid.split("@").first().replace("-", "g");;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

void Client::sendText(const QString &jid, const QString &message)
{
    FMessage msg(jid, true);
    msg.remote_resource = myJid;
    msg.type = FMessage::BodyMessage;
    msg.setData(message);
    queueMessage(msg);
    addMessage(msg);
}

void Client::sendMedia(const QStringList &jids, const QString &fileName, int waType)
{
    FMessage msg(JID_DOMAIN, true);

    QString file = fileName;
    file = file.replace("file://", "");
    QString fname = file;

    QString mime = Utilities::guessMimeType(file);
    if (mime.startsWith("image")) {
        QString tmp = QString("/var/tmp/%1").arg(file.split("/").last());
        int originalSize = QFile(file).size();

        qDebug() << "Original" << file << "size:" << QString::number(originalSize);
        if (resizeImages && (resizeWlan || activeNetworkType != QNetworkConfiguration::BearerWLAN)) {
            qDebug() << "resizeBySize:" << (resizeBySize ? "yes" : "no");
            qDebug() << "resizeImagesTo:" << QString::number(resizeImagesTo);
            qDebug() << "resizeImagesToMPix:" << QString::number(resizeImagesToMPix);
            QImage img(fname);
            int originalResolution = img.width() * img.height();
            int targetResolution = resizeImagesToMPix * 1000000;
            qDebug() << "w:" << img.width() << "h:" << img.height() << ((double)originalResolution / 1000000);
            QByteArray data;

            int orientation = 0;
            if (file.toLower().endsWith(".jpg") || file.toLower().endsWith(".jpeg")) {
                QExifImageHeader exif(file);
                orientation = exif.value(QExifImageHeader::Orientation).toSignedLong();
                if (orientation == 6) {
                    orientation = 90;
                }
                else {
                    orientation = 0;
                }
            }

            if (resizeBySize && originalSize > resizeImagesTo) {
                do
                {
                    data.clear();
                    QBuffer out(&data);
                    out.open(QIODevice::WriteOnly);
                    img = img.scaledToWidth(img.width() - 100, Qt::SmoothTransformation);
                    img.save(&out, mime.split("/").last().toUtf8().data(), 90);
                } while (data.size() > resizeImagesTo);
                if (orientation != 0) {
                    QTransform rotation;
                    rotation.rotate(orientation);
                    img = img.transformed(rotation, Qt::SmoothTransformation);
                }
                img.save(tmp, mime.split("/").last().toUtf8().data(), 90);
                qDebug() << "image w:" << QString::number(img.width())
                         << "h:" << QString::number(img.height())
                         << "s:" << QString::number(data.size());
                fname = tmp;
            }
            else if (originalResolution > targetResolution) {
                while (img.width() * img.height() > targetResolution) {
                    img = img.scaledToWidth(img.width() - 100, Qt::SmoothTransformation);
                }
                if (orientation != 0) {
                    QTransform rotation;
                    rotation.rotate(orientation);
                    img = img.transformed(rotation, Qt::SmoothTransformation);
                }
                img.save(tmp, mime.split("/").last().toUtf8().data(), 90);
                qDebug() << "image w:" << QString::number(img.width())
                         << "h:" << QString::number(img.height());
                fname = tmp;
            }

            qDebug() << "Check sharing options";
            if (_mediaStatusHash.contains(file)) {
                qDebug() << "Sharing" << file;
                _mediaNameHash[fname] = _mediaNameHash[file];
                _mediaStatusHash[fname] = true;
                _mediaStatusHash.remove(file);
                _mediaNameHash.remove(file);
            }
        }

        qDebug() << "Result file:" << fname << "size:" << QString::number(QFile(fname).size());
    }

    QFile mfile(fname);

    msg.status = FMessage::Unsent;
    msg.type = FMessage::RequestMediaMessage;
    msg.media_size = mfile.size();
    msg.media_wa_type = waType;
    msg.media_name = fname;
    if (jids.size() > 1) {
        msg.broadcast = true;
        msg.broadcastJids = jids;
        msg.remote_resource = "broadcast";
    }
    else {
        msg.remote_resource = jids.first();
    }
    msg.local_file_uri = file;

    // We still don't know the duration in seconds
    msg.media_duration_seconds = 0;

    if (mfile.open(QIODevice::ReadOnly))
    {
        QByteArray bytes = mfile.readAll();
        SHA256Context sha256;

        SHA256Reset(&sha256);
        SHA256Input(&sha256, reinterpret_cast<uint8_t *>(bytes.data()),
                    bytes.length());
        QByteArray result;
        result.resize(SHA256HashSize);
        SHA256Result(&sha256, reinterpret_cast<uint8_t *>(result.data()));
        msg.setData(result.toBase64());

        mfile.close();
    }

    queueMessage(msg);
}

void Client::sendMedia(const QStringList &jids, const QString &fileName, const QString &mediaType)
{
    FMessage::MediaWAType waType = FMessage::Text;
    if (mediaType == "image")
        waType = FMessage::Image;
    else if (mediaType == "audio")
        waType = FMessage::Audio;
    else if (mediaType == "video")
        waType = FMessage::Video;
    sendMedia(jids, fileName, waType);
}

void Client::sendMedia(const QStringList &jids, const QString fileName)
{
    QString type = Utilities::guessMimeType(fileName).split("/").first();
    sendMedia(jids, fileName, type);
}

void Client::sendVCard(const QStringList &jids, const QString &name, const QString &vcardData)
{
    if (vcardData.isEmpty())
        return;
    QString jid = jids.size() > 1 ? "broadcast" : jids.first();
    FMessage msg(jid, true);
    msg.status = FMessage::Unsent;
    msg.type = FMessage::MediaMessage;
    msg.media_wa_type = FMessage::Contact;
    msg.setData(vcardData);
    msg.media_name = name;
    if (jids.size() > 1) {
        msg.broadcast = true;
        msg.broadcastJids = jids;
    }
    queueMessage(msg);
    addMessage(msg);
}

void Client::sendLocation(const QStringList &jids, const QString &latitude, const QString &longitude, int zoom, const QString &source)
{
    if (nam && isOnline) {
        qDebug() << "sendLocation" << latitude << longitude;
        int w = 128;
        int h = 128;

        MapRequest *mapRequest = new MapRequest(source, latitude, longitude, zoom, w, h, jids);
        QObject::connect(mapRequest, SIGNAL(mapAvailable(QByteArray,QString,QString,QStringList,MapRequest*)),
                         this, SLOT(sendLocationRequest(QByteArray,QString,QString,QStringList,MapRequest*)));
        QObject::connect(mapRequest, SIGNAL(requestError(MapRequest*)), this, SLOT(mapError(MapRequest*)));

        QThread *thread = new QThread(mapRequest);
        mapRequest->moveToThread(thread);
        QObject::connect(thread, SIGNAL(started()), mapRequest, SLOT(doRequest()));
        thread->start();
    }
}

void Client::getGroupInfo(const QString &jid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendGetGroupInfo(jid);
    }
}

void Client::getPicture(const QString &jid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendGetPhoto(jid, QString(), true);
    }
}

void Client::userStatusUpdated(const QString &jid, const QString &message, int timestamp)
{
    qDebug() << "status updated for:" << jid << "status:" << message;
    if (!jid.contains("-")) {
        Q_EMIT contactStatus(jid, message, timestamp);

        QVariantMap contact;
        contact["jid"] = jid;
        contact["timestamp"] = timestamp;
        contact["message"] = message;
        contact["type"] = QueryType::ContactsSetSync;
        contact["uuid"] = uuid;
        dbExecutor->queueAction(contact);
    }
    if (jid == myJid) {
        myStatus = message;
        QSettings settings("coderus", "mitakuuluu2", this);
        settings.setValue(SETTINGS_STATUS,myStatus);
    }
}

void Client::photoIdReceived(const QString &jid, const QString &alias, const QString &author, const QString &timestamp, const QString &photoId, const QString &notificationId, bool offline)
{
    qDebug() << "photoIdReceived for:" << jid << "name:" << alias << "author:" << author << "timestamp:" << timestamp << "id:" << photoId;
    getPicture(jid);

    if (jid.contains("-")) {
        groupNotification(jid, author, PictureSet, timestamp, notificationId, offline);
    }
}

void Client::photoReceived(const QString &from, const QByteArray &data,
                           const QString &photoId, bool largeFormat)
{
    qDebug() << "photoReceived from:" << from << "id:" << photoId << "large:" << (largeFormat ? "yes" : "no");

    QString dirname = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/avatar";
    QString fname;

    if (data.isEmpty()) {
        if (photoId == "hidden") {
            fname = "/usr/share/harbour-mitakuuluu2/images/avatar-hidden.png";
        }
        else {
            QFile ava(QString("%1/%2-%3").arg(dirname).arg(from).arg(photoId));
            if (ava.exists())
                ava.remove();
        }
    }
    else {
        QDir avadir(dirname);
        if (!avadir.exists())
            avadir.mkpath(dirname);
        fname = QString("%1/%2-%3").arg(dirname).arg(from).arg(photoId);
        QFile ava(fname);
        if (ava.exists())
            ava.remove();
        if (ava.open(QFile::WriteOnly)) {
            ava.write(data);
            ava.close();
        }
    }
    Q_EMIT pictureUpdated(from, fname);

    QVariantMap query;
    query["type"] = QueryType::ContactsSetAvatar;
    query["jid"] = from;
    query["avatar"] = fname;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

void Client::photoRefresh(const QString &jid, const QString &expectedPhotoId, bool largeFormat)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendGetPhoto(jid, expectedPhotoId, largeFormat);
    }
}

void Client::photoDeleted(const QString &jid, const QString &alias, const QString &author, const QString &timestamp, const QString &notificationId)
{
    qDebug() << "photoDeleted for:" << jid << "photo:" << alias << "author:" << author << "timestamp:" << timestamp << notificationId;
    photoReceived(jid, QByteArray(), "empty", true);
}

void Client::setPhoto(const QString &jid, const QString &path)
{
    QFile photo(path);
    if (photo.exists() && connectionStatus == LoggedIn && photo.open(QFile::ReadOnly)) {
        QByteArray data = photo.readAll();
        photo.close();
        Q_EMIT connectionSendSetPhoto(jid, data, QByteArray());
    }
}

void Client::syncNumber(const QString &phone)
{
    QString jid = QString("%1@s.whatsapp.net").arg(phone);
    newContactAdded(jid);
}

void Client::startSharing(const QStringList &jids, const QString &name, const QString &data)
{
    if (data.isEmpty()) {
        QString media = name;
        media = media.replace("file://", "");
        qDebug() << "Sharing" << media;
        _mediaStatusHash[media] = true;
        _mediaNameHash[media] = media.split("/").last();
        sendMedia(jids, media);
    }
    else {
        sendVCard(jids, name, data);
    }
}

QString Client::getMyAccount()
{
    return myJid;
}

void Client::startTyping(const QString &jid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendComposing(jid, QString());
    }
}

void Client::startRecording(const QString &jid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendComposing(jid, "audio");
    }
}

void Client::endTyping(const QString &jid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendPaused(jid, QString());
    }
}

bool Client::getAvailable(const QString &jid)
{
    return _availableJids.contains(jid);
}

bool Client::getBlocked(const QString &jid)
{
    if (jid.contains("-"))
        return _mutedGroups.contains(jid);
    else
        return _blocked.contains(jid);
}

void Client::exit()
{
    dataCounters.writeCounters();
    connectionStatus = Disconnected;
    Q_EMIT connectionDisconnect();
    Q_EMIT connectionStatusChanged(connectionStatus);
    QGuiApplication::exit(0);
    qDebug() << system("killall harbour-mitakuuluu2-server");
}

void Client::broadcastSend(const QStringList &jids, const QString &message)
{
    FMessage msg("broadcast",true);
    msg.type = FMessage::BodyMessage;
    msg.broadcast = true;
    msg.broadcastJids = jids;
    msg.setData(message);
    msg.status = FMessage::ReceivedByTarget;

    queueMessage(msg);
    addMessage(msg);
}

void Client::downloadMedia(const QString &msgId, const QString &jid)
{
    qDebug() << "Media download requested:" << msgId;

    if (_mediaDownloadHash.keys().contains(msgId) && _mediaDownloadHash[msgId])
        return;

    _mediaDownloadHash[msgId] = NULL;

    QVariantMap query;
    query["type"] = QueryType::ConversationGetDownloadMessage;
    query["msgid"] = msgId;
    query["jid"] = jid;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

QVariantMap Client::getDownloads()
{
    return _mediaProgress;
}

void Client::cancelDownload(const QString &msgId, const QString &jid)
{
    Q_EMIT downloadFailed(jid, msgId);
    if (_mediaDownloadHash.keys().contains(msgId) && _mediaDownloadHash[msgId]) {
        _mediaDownloadHash[msgId]->deleteLater();
        _mediaDownloadHash[msgId] = 0;
    }
}

void Client::requestPresenceSubscription(const QString &jid)
{
    qDebug() << "requestPresenceSubscription" << jid;
    if (_blocked.contains(jid))
        return;
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendPresenceSubscriptionRequest(jid);
        //connection->sendPresenceSubscriptionRequest(jid);
    }
}

void Client::requestPresenceUnsubscription(const QString &jid)
{
    qDebug() << "requestPresenceUnsubscription" << jid;
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendUnsubscribeHim(jid);
        //connection->sendUnsubscribeHim(jid);
    }
}

void Client::createGroupChat(const QString &subject)
{
    //QString id = "create_group_" + QString::number(seq++);

    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendCreateGroupChat(subject);
        //connection->sendCreateGroupChat(subject);
    }
}

void Client::groupInfoFromList(const QString &id, const QString &from, const QString &author,
                               const QString &newSubject, const QString &creation,
                               const QString &subjectOwner, const QString &subjectTimestamp)
{
    Q_UNUSED(id);
    qDebug() << "groupInfoFromList:" << from << "author:" << author << "subject:" << newSubject << "creation:" << creation << "subject owner:" << subjectOwner << "subject creation:" << subjectTimestamp;
    saveGroupInfo(from, author, newSubject, subjectOwner, subjectTimestamp.toInt(), creation.toInt());
}

void Client::groupNewSubject(const QString &from, const QString &author, const QString &authorName, const QString &newSubject, const QString &creation, const QString &notificationId, bool offline)
{
    qDebug() << "groupNewSubject:" << newSubject << "from:" << from << "author:" << author << "authorName:" << authorName << "creation:" << creation;

    QVariantMap group;
    group["jid"] = from;
    group["subowner"] = author;
    group["subtimestamp"] = creation;
    group["message"] = newSubject;
    Q_EMIT newGroupSubject(group);

    group["type"] = QueryType::ContactsUpdateGroup;
    group["uuid"] = uuid;
    dbExecutor->queueAction(group);

    updateContactPushname(author, authorName);

    groupNotification(from, author, SubjectSet, creation, notificationId, offline, newSubject);
}

void Client::getParticipants(const QString &gjid)
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendGetParticipants(gjid);
    }
}

void Client::regRequest(const QString &cc, const QString &phone, const QString &method, const QString &password, const QString &mcc, const QString &mnc)
{
    qDebug() << "reg/request/" + method + "/" + cc + phone;

    if (reg) {
        delete reg;
        reg = 0;
    }

    reg = new PhoneReg(cc,phone,method,"",password,mcc,mnc);

    connect(reg,SIGNAL(finished(QVariantMap)),
            this,SLOT(registrationFinished(QVariantMap)));

    connect(reg,SIGNAL(codeFailed(QVariantMap)),this,SIGNAL(codeRequestFailed(QVariantMap)));
    connect(reg,SIGNAL(codeRequested(QVariantMap)),this,SIGNAL(codeRequested(QVariantMap)));
    connect(reg,SIGNAL(existsFailed(QVariantMap)),this,SIGNAL(existsRequestFailed(QVariantMap)));

    connect(reg,SIGNAL(expired(QVariantMap)),
            this,SLOT(expired(QVariantMap)));

    if (session)
        delete session;
    session = new QNetworkSession(manager->defaultConfiguration(), this);
    qDebug() << "regRequest session:" << session->isOpen() << "manager:" << manager->isOnline();
    if (!manager->isOnline()) {
        qDebug() << "offline";
        waitingRegConnection = true;
        openConnectionDialog();
    }
    else {
        QTimer::singleShot(500, reg, SLOT(start()));
    }
}

void Client::enterCode(const QString &cc, const QString &phone, const QString &smscode)
{
    qDebug() << "reg/register/" + cc + phone + "/" + smscode;

    if (reg) {
        delete reg;
        reg = 0;
    }

    reg = new PhoneReg(cc,phone,"",smscode);

    connect(reg,SIGNAL(finished(QVariantMap)),
            this,SLOT(registrationFinished(QVariantMap)));

    connect(reg,SIGNAL(expired(QVariantMap)),
            this,SLOT(expired(QVariantMap)));

    if (session) {
        delete session;
        session = 0;
    }
    session = new QNetworkSession(manager->defaultConfiguration(), this);
    qDebug() << "enterCode session:" << session->isOpen() << "manager:" << manager->isOnline();
    if (!manager->isOnline()) {
        qDebug() << "offline";
        waitingCodeConnection = true;
        openConnectionDialog();
    }
    else {
        QTimer::singleShot(500, reg, SLOT(startRegRequest()));
    }
}

void Client::registrationFinished(const QVariantMap& reply)
{
    qDebug() << "registrationFinished";
    if (reply["status"].toString() == "ok")
    {
        QVariantMap result = reply;
        result.insert("cc", this->cc);
        result.insert("number", this->number);
        registrationSuccessful(result);
    }
    else {
        Q_EMIT registrationFailed(reply);
    }
    QObject::disconnect(reg,0,0,0);
    delete reg;
}

void Client::expired(const QVariantMap &result)
{
    connectionStatus = AccountExpired;
    Q_EMIT connectionStatusChanged(connectionStatus);
    Q_EMIT accountExpired(result);
}

void Client::groupUsers(const QString &gjid, const QStringList &jids)
{
    qDebug() << "groupUsers" << gjid << "jid:" << jids;

    QVariantMap query;
    query["uuid"] = uuid;
    query["type"] = QueryType::ContactsGroupParticipants;
    query["jid"] = gjid;
    query["jids"] = jids;
    dbExecutor->queueAction(query);

    //Q_EMIT connectionSendSyncContacts(jids);

    Q_EMIT groupParticipants(gjid, jids);
}

void Client::addGroupParticipant(const QString &gjid, const QString &jid)
{
    if (connectionStatus == LoggedIn && jid != myJid)
    {
        QStringList participants;
        participants.append(jid);
        Q_EMIT connectionSendAddParticipants(gjid, participants);
    }
}

void Client::addGroupParticipants(const QString &gjid, const QStringList &jids)
{
    if (connectionStatus == LoggedIn && !jids.isEmpty())
    {
        Q_EMIT connectionSendAddParticipants(gjid, jids);
    }
}

void Client::groupAddUser(const QString &gjid, const QString &jid, const QString &timestamp, const QString &notificationId, bool offline)
{
    qDebug() << "groupAddUser" << gjid << "add:" << jid << "timestamp:" << timestamp;
    updateContactPushname(jid, "");
    Q_EMIT groupParticipantAdded(gjid, jid);

    groupNotification(gjid, jid, ParticipantAdded, timestamp, notificationId, offline);
}

void Client::groupRemoveUser(const QString &gjid, const QString &jid, const QString &timestamp, const QString &notificationId, bool offline)
{
    qDebug() << "groupRemoveUser" << gjid << "add:" << jid << "timestamp:" << timestamp;
    Q_EMIT groupParticipantRemoved(gjid, jid);

    groupNotification(gjid, jid, ParticipantRemoved, timestamp, notificationId, offline);
}

void Client::removeGroupParticipant(const QString &gjid, const QString &jid)
{
    if (connectionStatus == LoggedIn)
    {
        QStringList participants;
        participants.append(jid);
        Q_EMIT connectionSendRemoveParticipants(gjid, participants);
        //connection->sendRemoveParticipants(gjid, participants);
    }
}

void Client::requestPrivacyList()
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendGetPrivacyList();
        //connection->sendGetPrivacyList();
    }
}

void Client::requestPrivacySettings()
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendGetPrivacySettings();
    }
}

void Client::getPrivacyList()
{
    Q_EMIT contactsBlocked(_blocked);
}

void Client::getPrivacySettings()
{
    Q_EMIT privacySettings(_privacy);
}

void Client::getMutedGroups()
{
    qDebug() << _mutedGroups;
    Q_EMIT contactsMuted(_mutedGroups);
}

void Client::getAvailableJids()
{
    Q_EMIT contactsAvailable(_availableJids);
}

void Client::forwardMessage(const QStringList &jids, const QVariantMap &model)
{
    int msgtype = model["watype"].toInt();
    if (msgtype == 0) {
        if (jids.size() > 1 || !jids.at(0).contains("-"))  {
            broadcastSend(jids, model["data"].toString());
        }
        else {
            sendText(jids.at(0), model["data"].toString());
        }
    }
    else {
        FMessage msg(jids.size() > 1 ? "broadcast" : jids.first(), true);
        msg.type = FMessage::MediaMessage;
        if (jids.size() > 1) {
            msg.broadcast = true;
            msg.broadcastJids = jids;
        }
        msg.remote_resource = model["author"].toString();
        msg.media_wa_type = msgtype;
        msg.status = FMessage::Uploaded;
        msg.media_size = model["size"].toInt();
        msg.media_mime_type = model["mime"].toString();
        msg.media_name = model["name"].toString();
        msg.media_url = model["url"].toString();
        msg.local_file_uri = model["local"].toString();
        msg.setData(QByteArray::fromBase64(model["data"].toString().toUtf8()));
        msg.latitude = model["latitude"].toDouble();
        msg.longitude = model["longitude"].toDouble();
        msg.media_duration_seconds = model["duration"].toInt();
        msg.media_width = model["width"].toInt();
        msg.media_height = model["height"].toInt();
        queueMessage(msg);
        msg.setData(model["data"].toString());
        addMessage(msg);
    }
}

void Client::resendMessage(const QString &jid, const QVariantMap &model)
{
    FMessage msg(jid, true, model["msgid"].toString());
    int msgtype = model["watype"].toInt();
    msg.type = msgtype == FMessage::Text ? FMessage::BodyMessage : FMessage::MediaMessage;
    msg.remote_resource = model["author"].toString();
    msg.media_wa_type = msgtype;
    msg.status = FMessage::Uploaded;
    msg.media_size = model["size"].toInt();
    msg.media_mime_type = model["mime"].toString();
    msg.media_name = model["name"].toString();
    msg.media_url = model["url"].toString();
    msg.local_file_uri = model["local"].toString();
    if (msg.type == FMessage::MediaMessage) {
        msg.setData(QByteArray::fromBase64(model["data"].toString().toUtf8()));
    }
    else {
        msg.setData(model["data"].toString());
    }
    msg.latitude = model["latitude"].toDouble();
    msg.longitude = model["longitude"].toDouble();
    msg.media_duration_seconds = model["duration"].toInt();
    msg.media_width = model["width"].toInt();
    msg.media_height = model["height"].toInt();
    queueMessage(msg);
}

void Client::setPrivacyList()
{
    qDebug() << "Blocked people count:" << QString::number(_blocked.size());
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendSetPrivacyBlockedList(_blocked);
        //connection->sendSetPrivacyBlockedList(_blocked);
    }
    //requestPrivacyList();
}

void Client::blockOrUnblockContact(const QString &jid)
{
    qDebug() << "blockOrUnblockContact" << jid;
    if (_blocked.contains(jid)) {
        _blocked.removeAll(jid);
        requestPresenceSubscription(jid);
    }
    else {
        _blocked.append(jid);
        requestPresenceUnsubscription(jid);
    }
    Q_EMIT contactsBlocked(_blocked);

    setPrivacyList();
}

void Client::sendBlockedJids(const QStringList &jids)
{
    _blocked = jids;
    setPrivacyList();
    Q_EMIT contactsBlocked(_blocked);
}

void Client::setPrivacySettings(const QString &category, const QString &value)
{
    _privacy[category] = value;
    if (connectionStatus == LoggedIn)
        Q_EMIT connectionSendSetPrivacySettings(category, value);
}

void Client::muteOrUnmute(const QString &jid, int expire)
{
    qDebug() << "muteOrUnmuteGroup" << jid;

    QVariantMap query;
    query["uuid"] = uuid;
    query["type"] = QueryType::ContactsMuteGroup;
    query["jid"] = jid;
    query["mute"] = !_mutedGroups.contains(jid);
    dbExecutor->queueAction(query);

    if (_mutedGroups.contains(jid)) {
        if (expire == 0)
            _mutedGroups.remove(jid);
        else
            _mutedGroups[jid] = expire;
    }
    else if (expire != 0)
        _mutedGroups[jid] = expire;
    Q_EMIT contactsMuted(_mutedGroups);
}

void Client::privacyListReceived(const QStringList &list)
{
    qDebug() << "privacyListReceived:" << list.join(",");
    foreach (const QString &jid, list) {
        requestPresenceUnsubscription(jid);
    }
    _blocked = list;
    Q_EMIT contactsBlocked(list);
}

void Client::privacySettingsReceived(const QVariantMap &values)
{
    _privacy = values;
    Q_EMIT privacySettings(_privacy);
}

void Client::newContactAdded(QString jid)
{
    if (connectionStatus == LoggedIn)
    {
        Q_EMIT connectionSendSyncContacts(QStringList() << jid);
    }
}

void Client::newContactAdd(const QString &name, const QString &number)
{
    if (connectionStatus == LoggedIn)
    {
        _synccontacts.clear();
        _syncavatars.clear();
        qDebug() << "Requested contact adding:" << name << number;
        QStringList numbers;
        _synccontacts[number] = name;
        _syncavatars[number] = QString();
        numbers << number;
        if (!number.contains("+") && !number.contains("@")) {
            _synccontacts["+" + number] = name;
            _syncavatars["+" + number] = QString();
            numbers << ("+" + number);
        }
        Q_EMIT connectionSendSyncContacts(numbers);
    }
}

void Client::removeAccount()
{
    QVariantMap query;
    query["type"] = QueryType::AccountRemoveData;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);

    connectionStatus = LoginFailure;
    Q_EMIT connectionStatusChanged(connectionStatus);
}

void Client::syncContacts(const QStringList &numbers, const QStringList &names, const QStringList &avatars)
{
    if (!numbers.isEmpty()) {
        QStringList syncList;
        _synccontacts.clear();
        _syncavatars.clear();
        for (int i = 0; i < numbers.size(); i++) {
            QString phone = numbers.at(i);
            QString label = names.at(i);
            QString avatar = avatars.at(i);
            if (!syncList.contains(phone)) {
                _synccontacts[phone] = label;
                _syncavatars[phone] = avatar;
                syncList.append(phone);
            }
            if (!phone.contains("+") && !syncList.contains("+" + phone)) {
                _synccontacts["+" + phone] = label;
                _syncavatars["+" + phone] = avatar;
                syncList.append("+" + phone);
            }
        }
        Q_EMIT connectionSendSyncContacts(syncList);
    }
}

void Client::setPresenceAvailable()
{
    qDebug() << "set presence available";
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendAvailable();
    }
}

void Client::setPresenceUnavailable()
{
    qDebug() << "set presence unavailable";
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendUnavailable();
    }
}

void Client::removeAccountFromServer()
{
    if (connectionStatus == LoggedIn) {
        Q_EMIT connectionSendDeleteAccount();
    }
}

void Client::forceConnection()
{
    qDebug() << "Force connection";
    if (session)
        delete session;
    session = new QNetworkSession(manager->defaultConfiguration(), this);
    qDebug() << "session:" << session->isOpen() << "online:" << manager->isOnline();
    if (!manager->isOnline()) {
        openConnectionDialog();
    }
    else {
        networkStatusChanged(true);
    }
    delete session;
    session = 0;
}

void Client::openConnectionDialog()
{
    QDBusInterface *connSelectorInterface = new QDBusInterface(QStringLiteral("com.jolla.lipstick.ConnectionSelector"),
                                                                   QStringLiteral("/"),
                                                                   QStringLiteral("com.jolla.lipstick.ConnectionSelectorIf"),
                                                                   QDBusConnection::sessionBus(),
                                                                   this);

    QList<QVariant> args;
    args.append("wlan");
    QDBusMessage reply = connSelectorInterface->callWithArgumentList(QDBus::NoBlock,
                                                                     QStringLiteral("openConnection"), args);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qWarning() << "error:" << reply.errorMessage();
    }
}

void Client::contactRemoved(const QString &jid)
{
    if (_contacts.contains(jid))
        _contacts.remove(jid);
}

void Client::setLocale(const QString &locale)
{
    QGuiApplication::removeTranslator(&translator);

    qDebug() << "Loading translation:" << locale;
    qDebug() << (translator.load(locale, "/usr/share/harbour-mitakuuluu2/locales", QString(), ".qm") ? "Translator loaded" : "Error loading translator");
    qDebug() << (QGuiApplication::installTranslator(&translator) ? "Translator installed" : "Error installing translator");
}

void Client::windowActive()
{
    clearNotification();
}

void Client::sendVoiceNotePlayed(const FMessage &message)
{
    if (connectionStatus == LoggedIn)
        Q_EMIT connectionSendVoiceNotePlayed(message);
}

void Client::onMessageReceived(const FMessage &message)
{
    //qDebug() << "messageReceived:" << message.key.id << "text:" << QString::fromUtf8(message.data.data()) << "from:" << message.key.remote_jid << "pushname:" << message.notify_name << "status:" << QString::number(message.status);
    QString fromAttribute = message.key.remote_jid;
    QString pushName = message.notify_name;
    if (!fromAttribute.contains("-"))
        updateContactPushname(fromAttribute, pushName);
    else {
        updateContactPushname(fromAttribute, "");
        updateContactPushname(message.remote_resource, pushName);
    }

    if (_contacts.contains(fromAttribute) || acceptUnknown) {
        addMessage(message);
        if (autoDownloadMedia) {
            if (message.type == FMessage::MediaMessage && message.media_size <= autoBytes
                    && (activeNetworkType == QNetworkConfiguration::BearerWLAN || !autoDownloadWlan)) {
                startDownloadMessage(message);
            }
        }
    }
}

void Client::messageStatusUpdate(const QString &jid, const QString &msgId, int msgstatus)
{
    qDebug() << "messageStatusUpdated:" << msgId << "status:" << QString::number(msgstatus) << "from:" << jid;
    if (jid.contains("@")) {
        Q_EMIT messageStatusUpdated(jid, msgId, msgstatus);

        QVariantMap query;
        query["type"] = QueryType::ConversationMessageStatus;
        query["jid"] = jid;
        query["status"] = msgstatus;
        query["msgid"] = msgId;
        query["uuid"] = uuid;
        dbExecutor->queueAction(query);
    }
}

void Client::updateNotification(const QString &text)
{
    qDebug() << "Have published:" << (connectionNotification ? "yes" : "no");
    if (connectionNotification) {
        delete connectionNotification;
        connectionNotification = 0;
    }
    if (showConnectionNotifications) {
        qDebug() << "updateNotification:" << text;
        connectionNotification = new MNotification("harbour.mitakuuluu2.notification", text, "Mitakuuluu");
        connectionNotification->setImage("/usr/share/themes/base/meegotouch/icons/harbour-mitakuuluu-popup.png");
        MRemoteAction action("harbour.mitakuuluu2.client", "/", "harbour.mitakuuluu2.client", "notificationCallback", QVariantList() << QString());
        connectionNotification->setAction(action);
        connectionNotification->publish();
    }
}

void Client::available(QString jid, qint64 lastSeen)
{
    qDebug() << "Contact" << jid << "last seen:" << lastSeen;
    if (jid.contains("@") && !jid.contains("-")) {
        Q_EMIT contactLastSeen(jid, lastSeen);

        QVariantMap query;
        query["type"] = QueryType::ContactsSetLastSeen;
        query["jid"] = jid;
        query["timestamp"] = lastSeen;
        query["uuid"] = uuid;
        dbExecutor->queueAction(query);
    }
}

void Client::available(const QString &jid, bool available)
{
    qDebug() << "Contact" << jid << (available ? " available" : " unavailable");
    if (available) {
        if (!_availableJids.contains(jid))
            _availableJids.append(jid);
        Q_EMIT contactAvailable(jid);
    }
    else {
        if (_availableJids.contains(jid))
            _availableJids.removeAll(jid);
        Q_EMIT contactUnavailable(jid);
    }
}

void Client::composing(const QString &jid, const QString &media)
{
    qDebug() << "Contact" << jid << "composing:" << media;
    Q_EMIT contactTyping(jid);
}

void Client::paused(const QString &jid)
{
    qDebug() << "Contact" + jid;
    Q_EMIT contactPaused(jid);
}

void Client::groupError(const QString &error)
{
    qDebug() << "groupError:" + error;
}

void Client::groupLeft(const QString &jid)
{
    qDebug() << "groupLeft:" << jid;
}

void Client::saveGroupInfo(const QString &jid, const QString &owner, const QString &subject, const QString &subjectOwner, int subjectT, int creation)
{
    QVariantMap group;
    QString message = subject;
    group["jid"] = jid;
    QString from = jid.split("@").at(0);
    group["pushname"] = from;
    group["name"] = from;
    group["owner"] = owner;
    group["nickname"] = message;
    group["message"] = message;
    group["subowner"] = subjectOwner;
    group["subtimestamp"] = subjectT;
    group["timestamp"] = creation;
    group["contacttype"] = KnownContact;
    group["avatar"] = QString();
    group["available"] = getAvailable(jid);
    group["unread"] = getUnreadCount(jid);
    group["lastmessage"] = 0;
    group["blocked"] = getBlocked(jid);
    Q_EMIT groupInfo(group);

    group["type"] = QueryType::ContactsSaveModel;
    group["uuid"] = uuid;
    dbExecutor->queueAction(group);

    getPicture(jid);
}

void Client::dbResults(const QVariant &result)
{
    QVariantMap reply = result.toMap();
    if (reply["uuid"].toString() != uuid)
        return;
    int vtype = reply["type"].toInt();
    switch (vtype) {
    case QueryType::ContactsSaveModel: {
        if (!reply["exists"].toBool()) {
            getPicture(reply["jid"].toString());
        }
        break;
    }
    case QueryType::ContactsGetMuted: {
        QVariantMap jids = reply["result"].toMap();
        _contacts = jids;
        break;
    }
    case QueryType::ContactsSetLastmessage: {
        if (reply["exists"].toBool()) {
            Q_EMIT lastmessageUpdated(reply["jid"].toString(), reply["lastmessage"].toInt());
        }
        break;
    }
    case QueryType::ConversationNotifyMessage: {
        qDebug() << "show notification";
        QString jid = reply["jid"].toString();
        QString avatar = reply["avatar"].toString();
        avatar = avatar.replace("file://", "");
        avatar = avatar.replace("///", "/");
        QString name = reply["name"].toString();
        /*if (_notificationJid.contains(jid)) {
            MNotification *notif = _notificationJid[jid];
            _notificationJid.remove(jid);
            if (notif) {
                notif->remove();
                delete notif;
            }
        }*/
        int unread = getUnreadCount(jid);
        //unread++;
        //setUnreadCount(jid, unread);

        qlonglong muted = 0;
        if (mutingList.contains(jid)) {
            muted = mutingList[jid];
        }
        qlonglong msecs = QDateTime::currentDateTime().toMSecsSinceEpoch();

        if (!notificationsMuted && (!_contacts.contains(jid) || (muted < msecs))) {
            QString msg = reply["msg"].toString();
            if (!notifyMessages) {
                msg = tr("%n messages unread", "Message notification with unread messages count", unread);
            }
            qDebug() << "MNotification" << msg << name << avatar;

            QString notifyType = "harbour.mitakuuluu2.message";

            if (!systemNotifier) {
                bool media = reply["media"].toBool();
                if (media) {
                    notifyType = "harbour.mitakuuluu2.media";
                }
                else {
                    if (jid.contains("-")) {
                        notifyType = "harbour.mitakuuluu2.group";
                    }
                    else {
                        notifyType = "harbour.mitakuuluu2.private";
                    }
                }
            }

            MNotification *notification = new MNotification(notifyType, msg, name);
            notification->setImage(avatar.isEmpty() ? "/usr/share/harbour-mitakuuluu2/images/notification.png" : avatar);
            MRemoteAction action("harbour.mitakuuluu2.client", "/", "harbour.mitakuuluu2.client", "notificationCallback", QVariantList() << jid);
            notification->setAction(action);

            if (reply["offline"].toBool()) {
                if (_notificationJid.contains(jid)) {
                    MNotification *notif = _notificationJid[jid];
                    _notificationJid.remove(jid);
                    if (notif) {
                        notif->remove();
                        delete notif;
                    }
                }
                _notificationJid[jid] = notification;

                _pendingCount += 1;
                qDebug() << "notify pending" << _pendingCount << "unread" << _totalUnread;

                if (_totalUnread == _pendingCount)
                    showOfflineNotifications();
            }
            else {
                if (_pendingNotificationsTimer.contains(jid) && _pendingNotificationsTimer[jid] && _pendingNotificationsTimer[jid]->isActive()) {
                    if (_notificationPendingJid.contains(jid)) {
                        MNotification *notif = _notificationPendingJid[jid];
                        _notificationPendingJid.remove(jid);
                        if (notif) {
                            notif->remove();
                            delete notif;
                        }
                    }
                    _notificationPendingJid[jid] = notification;
                }
                else {
                    if (_notificationJid.contains(jid)) {
                        MNotification *notif = _notificationJid[jid];
                        _notificationJid.remove(jid);
                        if (notif) {
                            notif->remove();
                            delete notif;
                        }
                    }
                    _notificationJid[jid] = notification;
                    notification->publish();

                    if (!_pendingNotificationsTimer.contains(jid)) {
                        QTimer *delayedNotification = new QTimer(this);
                        delayedNotification->setSingleShot(true);
                        delayedNotification->setObjectName(jid);
                        QObject::connect(delayedNotification, SIGNAL(timeout()), this, SLOT(onDelayedNotificationTriggered()));
                        _pendingNotificationsTimer[jid] = delayedNotification;
                    }
                    if (_notificationPendingJid.contains(jid)) {
                        MNotification *notif = _notificationPendingJid[jid];
                        _notificationPendingJid.remove(jid);
                        if (notif) {
                            notif->remove();
                            delete notif;
                        }
                    }
                    _pendingNotificationsTimer[jid]->start(notificationsDelay * 1000);
                }
            }
        }
        break;
    }
    case QueryType::ContactsUpdatePushname: {
        if (!reply["exists"].toBool()) {
            QString jid = reply["jid"].toString();
            QString pushName = reply["pushName"].toString();
            int timestamp = reply["timestamp"].toInt();

            QVariantMap contact;
            contact["jid"] = jid;
            contact["name"] = jid.split("@").first();
            contact["pushname"] = pushName;
            contact["nickname"] = pushName;
            contact["message"] = QString();
            contact["contacttype"] = UnknownContact;
            contact["owner"] = QString();
            contact["subowner"] = QString();
            contact["timestamp"] = 0;
            contact["subtimestamp"] = 0;
            contact["avatar"] = QString();
            contact["available"] = getAvailable(jid);
            contact["unread"] = 0;
            contact["lastmessage"] = (pushName.isEmpty() && !jid.contains("-")) ? 0 : timestamp;
            contact["blocked"] = getBlocked(jid);

            Q_EMIT contactChanged(contact);

            if (jid.contains("-")) {
                getGroupInfo(jid);
            }
            else {
                newContactAdd(pushName, jid);
            }
        }
        break;
    }
    case QueryType::ContactsGetJids: {
        _contacts = reply["jids"].toMap();
        foreach (const QString& jid, _contacts.keys()) {
            if (!jid.contains("-")) {
                requestPresenceSubscription(jid);
            }
            Q_EMIT setUnread(jid, _contacts[jid].toInt());
        }
        if (_contacts.isEmpty()) {
            Q_EMIT connectionUpdateGroupChats();
        }
        break;
    }
    case QueryType::ContactsSyncContacts: {
        bool isEmpty = true;
        QStringList jids;
        QVariantList records = reply["jids"].toList();
        foreach (const QVariant& record, records) {
            QString jid = record.toString();
            if (!_blocked.contains(jid)) {
                jids.append(jid);
                isEmpty = false;
            }
        }

        if (isEmpty) {
            //synchronizePhonebook();
        }
        else {
            if (connectionStatus == LoggedIn)
            {
                Q_EMIT connectionSendSyncContacts(jids);
            }
        }
        break;
    }
    case QueryType::ContactsSyncResults: {
        QStringList jids = reply["jids"].toStringList();
        qDebug() << "Synced contacts:" << jids;
        if (jids.size() > 1) {
            Q_EMIT contactsChanged();
            foreach (const QString &jid, jids) {
                if (!_contacts.contains(jid))
                    _contacts[jid] = 0;
                requestPresenceSubscription(jid);
            }
        }
        else if (jids.size() == 1) {
            QVariantMap contact = reply["contact"].toMap();
            if (!contact.isEmpty()) {
                QString jid = contact["jid"].toString();
                contact["available"] = getAvailable(jid);
                contact["blocked"] = getBlocked(jid);
                Q_EMIT contactChanged(contact);
            }
            else {
                contact = reply["sync"].toMap();
                if (!contact.isEmpty()) {
                    QString jid = contact["jid"].toString();
                    contact["available"] = getAvailable(jid);
                    contact["blocked"] = getBlocked(jid);
                    Q_EMIT contactSynced(contact);
                }
            }
        }
        else if (jids.isEmpty()) {
            //synchronizePhonebook();
        }
        break;
    }
    case QueryType::ConversationGetDownloadMessage: {
        QVariantMap message = reply["message"].toMap();
        if (!message.isEmpty()) {
            int watype = message["watype"].toInt();
            QString url = message["url"].toString();
            QString medianame = message["name"].toString();
            if ((watype == FMessage::Image || watype == FMessage::Audio || watype == FMessage::Video)
                    && !url.isEmpty()
                    && !medianame.isEmpty()) {
                QString jid = message["jid"].toString();
                int mediasize = message["size"].toInt();
                QString mediamime = message["mime"].toString();
                QString author = message["author"].toString();
                int timestamp = message["timestamp"].toInt();
                int msgstatus = message["status"].toInt();
                QString mediathumb = message["data"].toString();
                QString msgId = message["msgid"].toString();

                FMessage msg(jid, false, msgId);
                msg.media_name = medianame;
                msg.media_url = url;
                msg.type = FMessage::MediaMessage;
                msg.media_wa_type = (FMessage::MediaWAType)watype;
                msg.media_size = mediasize;
                msg.media_mime_type = mediamime;
                msg.remote_resource = author;
                msg.timestamp = timestamp;
                msg.status = (FMessage::Status)msgstatus;
                msg.thumb_image = mediathumb;

                qDebug() << "Requested media download" << msg.media_url << "from" << msg.key.remote_jid;

                startDownloadMessage(msg);
            }
        }
        break;
    }
    case QueryType::ContactsUpdateGroup: {
        if (!reply["exists"].toBool()) {
            QString jid = reply["jid"].toString();
            getGroupInfo(jid);
        }
        break;
    }
    case QueryType::ContactsGroupParticipants: {
        QStringList unknown = reply["unknown"].toStringList();
        if (!unknown.isEmpty()) {
            Q_EMIT connectionSendSyncContacts(unknown);
        }
        break;
    }
    case QueryType::ConversationGetMedia: {
        QVariantMapList mapList = reply["media"].value< QVariantMapList >();
        if (mapList.size() > 0) {
            Q_EMIT mediaListReceived(reply["jid"].toString(), mapList);
        }
        break;
    }
    }
}

void Client::waversionRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray serverReply = reply->readAll();
        this->waversion = QString(serverReply);
        this->waresource = QString("Android-%1-443").arg(this->waversion);
        this->wauseragent = QString("WhatsApp/%1 Android/4.2.1 Device/GalaxyS3").arg(this->waversion);

        QSettings settings("coderus", "mitakuuluu2", this);
        settings.setValue(SETTINGS_WAVERSION, this->waversion);
    }
}

void Client::scratchRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply && reply->error() == QNetworkReply::NoError) {
        QByteArray json = reply->readAll();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(json, &error);
        if (error.error == QJsonParseError::NoError) {
            QVariantMap mapResult = doc.toVariant().toMap();
            this->wandroidscratch1 = mapResult["a"].toString();
            qDebug() << "wa/scratch/reply";
            this->wanokiascratch1 = mapResult["b"].toString();
            this->wanokiascratch2 = mapResult["c"].toString();
            this->waversion = mapResult["d"].toString();
            this->waresource = QString("Android-%1-443").arg(this->waversion);
            this->wauseragent = QString("WhatsApp/%1 Android/4.2.1 Device/GalaxyS3").arg(this->waversion);
            QSettings settings("coderus", "mitakuuluu2", this);
            settings.setValue(SETTINGS_SCRATCH1, this->wanokiascratch1);
            settings.setValue(SETTINGS_SCRATCH2, this->wanokiascratch2);
            settings.setValue(SETTINGS_SCRATCH3, this->wandroidscratch1);
            settings.setValue(SETTINGS_WAVERSION, this->waversion);
            qDebug() << this->wandroidscratch1;
            qDebug() << this->wauseragent;
        }
    }
}

void Client::compressOldLogs()
{
    /*qDebug() << "Compressing old logs...";
    QFile lastLog("/home/nemo/.whatsapp/whatsapp.log");
    if (lastLog.exists()) {
        QFile log3("/home/nemo/.whatsapp/logs/whatsapp_log3.tar.gz");
        QFile log2("/home/nemo/.whatsapp/logs/whatsapp_log2.tar.gz");
        QFile log1("/home/nemo/.whatsapp/logs/whatsapp_log1.tar.gz");
        if (log3.exists()) {
            log3.remove();
            QFile::copy(log2.fileName(), log3.fileName());
            QFile::copy(log1.fileName(), log2.fileName());
            log1.remove();
        }
        else if (log2.exists()) {
            QFile::copy(log2.fileName(), log3.fileName());
            QFile::copy(log1.fileName(), log2.fileName());
            log1.remove();
        }
        else if (log1.exists()) {
            QFile::copy(log1.fileName(), log2.fileName());
            log1.remove();
        }
    }
    QProcess tar;
    tar.start("tar", QStringList() << "czfv" << "/home/nemo/.whatsapp/logs/whatsapp_log1.tar.gz" << "/home/nemo/.whatsapp/whatsapp.log");
    tar.waitForFinished(-1);
    lastLog.remove();
    qDebug() << "Compressing done!";*/
}
