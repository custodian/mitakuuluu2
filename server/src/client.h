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

#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H

#include <QDir>
#include <QThread>

#include <QString>
#include <QTcpSocket>
#include <QQueue>
#include <QTimer>
#include <QMutex>
#include <QAbstractSocket>
#include <QNetworkConfigurationManager>

#include "Whatsapp/util/datacounters.h"
#include "Whatsapp/mediaupload.h"
#include "Whatsapp/mediadownload.h"

#include "Whatsapp/warequest.h"
#include "Whatsapp/connection.h"
#include "Whatsapp/phonereg.h"
#include "Whatsapp/fmessage.h"

#include "Whatsapp/maprequest.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>

#include <QProcess>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlResult>
#include <QSqlRecord>
#include <QSqlError>

#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactDetailFilter>
#include <QContactName>
#include <QContactPhoneNumber>
#include <QContactAvatar>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QNetworkConfigurationManager>
#include <QNetworkSession>

#include <QCryptographicHash>

#include <QImage>
#include <QBuffer>

#include <QDateTime>
#include <QStringList>

#include <mlite-global.h>
#include <mlite5/MNotification>
#include <mlite5/MRemoteAction>

#include "keepalive/backgroundactivity.h"
#include "../threadworker/queryexecutor.h"
#include "../dconf/mdconfagent.h"

typedef QList<QVariantMap> QVariantMapList;
Q_DECLARE_METATYPE(QVariantMapList)

using namespace QtContacts;

/**
    @class      Client

    @brief      This is the main class of Yappari.

                It provides interaction between the GUI and the Connection object
                that keeps the connection to the WhatsApp servers.

                The object from this class is always running in background, and also
                interacts with the status menu applet.
*/

class Client : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "harbour.mitakuuluu2.server")

public:

    /** ***********************************************************************
     ** Enumerations
     **/

    // Status of the connection to the WhatsApp servers
    enum ConnectionStatus {
        Unknown,
        WaitingForConnection,
        Connecting,
        Connected,
        LoggedIn,
        LoginFailure,
        Disconnected,
        Registering,
        RegistrationFailed,
        AccountExpired
    };

    // Frequency of the address book synchronization
    enum SyncFrequencies {
        onConnect,
        onceADay,
        onceAWeek,
        onceAMonth
    };

    enum MessageType {
        IncomingText = 0,
        IncomingPicture,
        IncomingAudio,
        IncomingVideo,
        IncomingLocation,
        IncomingVCard,
        OutgoigText,
        OutgoigPicture,
        OutgoigAudio,
        OutgoigVideo,
        OutgoigLocation,
        OutgoigVCard,
        DownloadedMedia,
        Notification = 100
    };

    enum NotificationStatus {
        ParticipantAdded = 0,
        ParticipantRemoved,
        SubjectSet,
        PictureSet
    };

    enum ContactType {
        UnknownContact = 0,
        KnownContact
    };

    enum MessageStatus {
        MessageSending = 0,
        MessageSent,
        MessageDelivered
    };

    /** ***********************************************************************
     ** Public members
     **/

    // Network data counters
    static DataCounters dataCounters;

    // Status of the connection
    static ConnectionStatus connectionStatus;

    // Message number sequence
    static quint64 seq;

    // Own JID
    static QString myJid;

    // Country code
    static QString cc;

    static QString mcc;
    static QString mnc;

    // Phone number in local format (without the country code)
    static QString number;

    // Phone number in international format (with the country code)
    static QString phoneNumber;

    // User name or alias
    static QString userName;

    // User password
    static QString password;

    // Account creation timestamp
    static QString creation;

    // Account expiration timestamp
    static QString expiration;

    // Account kind (free/paid)
    static QString kind;

    // Account status (active/expired)
    static QString accountstatus;

    // User Status
    static QString myStatus;

    // Import media into gallery
    bool importMediaToGallery;

    // Protocol constants
    QString wauseragent;
    QString waresource;

    QString wanokiascratch1;
    QString wanokiascratch2;
    QString wandroidscratch1;
    QString wandroidscratch2;
    QString wandroidscratch3;
    QString wandroidscratch4;

    bool whatsappevil;

    static MDConfAgent *dconf;

    QNetworkAccessManager* nam;

    /** ***********************************************************************
     ** Constructors and destructors
     **/

    // Create a Client object
    explicit Client(QObject *parent = 0);

    // Destroy a Client object
    ~Client();


private slots:
    /** ***********************************************************************
     ** Network Detection methods
     **/

    // Handles a network change
    void doCheckNetworkStatus();
    void networkStatusChanged(bool isOnline);
    void registrationSuccessful(const QVariantMap &result);

    void loginFailed();

    void onAuthSuccess(const QString &creation, const QString &expiration,
                   const QString &kind, const QString status,
                   const QByteArray &nextChallenge);

    void authFailed();
    void clientDestroyed();

    void networkConfigurationChanged(const QNetworkConfiguration &conf);
    void verifyAndConnect();
    void connected();
    void disconnected();
    void destroyConnection();
    void connectionActivated();
    void connectionDeactivated();
    void queueMessage(const FMessage &message);
    void sendMessagesInQueue();
    void userStatusUpdated(const QString &jid, const QString &message, int timestamp);
    void syncResultsAvailable(const QVariantList &results);
    void syncContactsAvailable(const QVariantList &results);
    void photoRefresh(const QString &jid, const QString &expectedPhotoId, bool largeFormat);
    void requestPresenceSubscription(const QString &jid);
    void requestPresenceUnsubscription(const QString &jid);
    void groupUsers(const QString &gjid, const QStringList &jids);
    void groupAddUser(const QString &gjid, const QString &jid, const QString &timestamp, const QString &notificationId, bool offline);
    void groupRemoveUser(const QString &gjid, const QString &jid, const QString &timestamp, const QString &notificationId, bool offline);
    void setPrivacyList();
    void sendVoiceNotePlayed(const FMessage &message);

    void onMessageReceived(const FMessage &message);
    void messageStatusUpdate(const QString &jid, const QString &msgId, int msgstatus);
    void available(QString jid, qint64 lastSeen);
    void available(const QString &jid, bool available);
    void composing(const QString &jid, const QString &media);
    void paused(const QString &jid);
    void groupError(const QString &error);
    void groupLeft(const QString &jid);
    void groupInfoFromList(const QString &id, const QString &from, const QString &author,
                           const QString &newSubject, const QString &creation,
                           const QString &subjectOwner, const QString &subjectTimestamp);
    void groupNewSubject(const QString &from, const QString &author, const QString &authorName,
                         const QString &newSubject, const QString &creation, const QString &notificationId, bool offline);
    void photoDeleted(const QString &jid, const QString &alias, const QString &author, const QString &timestamp, const QString &notificationId);
    void photoIdReceived(const QString &jid, const QString &name, const QString &author, const QString &timestamp, const QString &pictureId, const QString &notificationId, bool offline);
    void photoReceived(const QString &from, const QByteArray &data,
                       const QString &photoId, bool largeFormat);
    void privacyListReceived(const QStringList &list);
    void privacySettingsReceived(const QVariantMap &values);

    void sendLocationRequest(const QByteArray &mapData, const QString &latitude, const QString &longitude, const QStringList &jids, MapRequest* sender);
    void mapError(MapRequest* sender);

    void mediaUploadAccepted(const FMessage &message);
    void mediaUploadStarted(MediaUpload *mediaUpload, const FMessage &msg);
    void mediaUploadFinished(MediaUpload *mediaUpload, const FMessage &msg);
    void mediaDownloadFinished(MediaDownload *mediaDownload, const FMessage &msg);
    void mediaDownloadProgress(const FMessage &msg, float progress);
    void mediaDownloadError(MediaDownload *mediaDownload, const FMessage &msg, int errorCode);
    void sslErrorHandler(MediaUpload *mediaUpload, const FMessage &msg);
    void httpErrorHandler(MediaUpload *mediaUpload, const FMessage &msg);
    void mediaUploadProgress(const FMessage &msg, float progress);

    void registrationFinished(const QVariantMap &reply);
    void expired(const QVariantMap &result);
    void dbResults(const QVariant &result);
    void scratchRequestFinished();

    void updateContactPushname(const QString &jid, const QString &pushName);

    void onSimParameters(QDBusPendingCallWatcher *call);

    void notifyOfflineMessages(int count);

    void contactsAvailable(const QStringList &contacts, const QVariantMap &labels, const QVariantMap &avatars);

    void checkActivity();
    void wakeupStopped();
    void onDelayedNotificationTriggered();

    void threadStarted();
    void threadFinished();

    void onMediaTitleReceived(const QString &msgid, const QString &title, const QString &jid);

    void onSettingsChanged(const QString &key);

signals:
    void authFail(const QString &username, const QString &reason);
    void authSuccess(const QString &username);
    void disconnected(const QString &reason);
    void networkAvailable(bool value);
    void networkOnline();
    void networkOffline();
    void messageReceived(const QVariantMap &data);
    void smsTimeout(int timeout);
    void noAccountData();
    void registrationFailed(const QVariantMap &reason);
    void accountExpired(const QVariantMap &reason);
    void messageStatusUpdated(const QString &jid, const QString &msgId, int msgstatus);
    void pictureUpdated(const QString &jid, const QString &path);
    void pushnameUpdated(const QString &jid, const QString &pushName);
    void lastmessageUpdated(const QString &jid, int lastmessage);
    void contactsChanged();
    void contactChanged(const QVariantMap &data);
    void contactSynced(const QVariantMap &data);
    void newGroupSubject(const QVariantMap &data);
    void notificationOpenJid(const QString &jid);
    void setUnread(const QString &jid, int count);
    void contactAvailable(const QString &jid);
    void contactUnavailable(const QString &jid);
    void contactLastSeen(const QString &jid, int seconds);
    void contactTyping(const QString &jid);
    void contactPaused(const QString &jid);
    void connectionStatusChanged(int status);
    void downloadProgress(const QString &jid, const QString &msgId, int percent);
    void downloadFinished(const QString &jid, const QString &msgId, const QString &localUrl);
    void downloadFailed(const QString &jid, const QString &msgId);
    void uploadStarted(const QString &jid, const QString &msgId, const QString &localUrl);
    void uploadProgress(const QString &jid, const QString &msgId, int percent);
    void uploadFinished(const QString &jid, const QString &msgId, const QString &remoteUrl);
    void uploadFailed(const QString &jid, const QString &msgId);
    void groupParticipants(const QString &jid, const QStringList &participants);
    void groupParticipantAdded(const QString &jid, const QString &participant);
    void groupParticipantRemoved(const QString &jid, const QString &participant);
    void groupInfo(const QVariantMap &group);
    void groupCreated(const QString &jid);
    void contactsBlocked(const QStringList &jids);
    void privacySettings(const QVariantMap &values);
    void contactsMuted(const QVariantMap &jids);
    void contactsAvailable(const QStringList &jids);
    void avatarChanged(const QString &jid, const QString &avatar);
    void contactStatus(const QString &jid, const QString &message, int timestamp);
    void synchronizationFinished();
    void synchronizationFailed();
    void codeRequested(const QVariantMap &method);
    void existsRequestFailed(const QVariantMap &serverReply);
    void codeRequestFailed(const QVariantMap &serverReply);
    void registrationComplete();
    void dissectError();
    void connectionLogin(const QByteArray &nextChallenge);
    void connectionSendMessage(const FMessage &message);
    void connectionSendSyncContacts(const QStringList &numbers);
    void connectionSendQueryLastOnline(const QString &jid);
    void connectionSendGetStatus(const QStringList &jids);
    void connectionSendPresenceSubscriptionRequest(const QString &jid);
    void connectionSendUnsubscribeHim(const QString &jid);
    void connectionSendUnsubscribeMe(const QString &jid);
    void connectionSendGetPhoto(const QString &jid, const QString &expectedPhotoId, bool largeFormat);
    void connectionSendSetPhoto(const QString &jid, const QByteArray &imageBytes, const QByteArray &thumbBytes);
    void connectionSendGetPhotoIds(const QStringList &jids);
    void connectionSendVoiceNotePlayed(const FMessage &message);
    void connectionSendCreateGroupChat(const QString &subject);
    void connectionSendAddParticipants(const QString &gjid, const QStringList &participants);
    void connectionSendRemoveParticipants(const QString &gjid, const QStringList &participants);
    void connectionSendVerbParticipants(const QString &gjid, const QStringList &participants, const QString &id, const QString &innerTag);
    void connectionSendGetParticipants(const QString &gjid);
    void connectionSendGetGroupInfo(const QString &gjid);
    void connectionUpdateGroupChats();
    void connectionSendSetGroupSubject(const QString &gjid, const QString &subject);
    void connectionSendLeaveGroup(const QString &gjid);
    void connectionSendRemoveGroup(const QString &gjid);
    void connectionSendGetPrivacyList();
    void connectionSendSetPrivacyBlockedList(const QStringList &jidList);
    void connectionSendGetPrivacySettings();
    void connectionSendSetPrivacySettings(const QString &category, const QString &value);
    void connectionSetNewUserName(const QString &push_name, bool hide);
    void connectionSetNewStatus(const QString &status);
    void connectionSendAvailableForChat(bool hide);
    void connectionSendAvailable();
    void connectionSendUnavailable();
    void connectionSendStreamEnd();
    void connectionSendDeleteAccount();
    void connectionSendMessageReceived(const FMessage &message);
    void connectionDisconnect();
    void connectionSendGetServerProperties();
    void connectionSendGetClientConfig();
    void connectionSendComposing(const QString &jid, const QString &media);
    void connectionSendPaused(const QString &jid, const QString &media);
    void networkUsage(const QVariantList &networkUsage);
    void logfileReady(const QByteArray &data, bool isReady);
    void pong();
    void myAccount(const QString &account);
    void simParameters(const QString &mcc, const QString &mnc);
    void mediaListReceived(const QString &pjid, const QVariantMapList &mediaList);
    void streamError();
    void mediaTitleReceived(const QString &msgid, const QString &title, const QString &jid);

private:
    QueryExecutor *dbExecutor;

    QScopedPointer<Connection> connectionPtr;
    //Connection *connection;
    QNetworkConfigurationManager *manager;
    QNetworkSession *session;
    QSqlDatabase db;
    QString lastError;
    QString activeNetworkID;
    QNetworkConfiguration::BearerType activeNetworkType;
    qint64 lastCountersWrite;
    PhoneReg *reg;
    bool waitingCodeConnection;
    bool waitingRegConnection;

    // Timers
    QTimer *pendingMessagesTimer;
    QTimer *retryLoginTimer;

    // Queues
    QQueue<FMessage> pendingMessagesQueue;

    QThread *connectionThread;

    // Mutex
    QMutex connectionMutex;
    QMutex pendingMessagesMutex;

    // Registration
    bool isRegistered;

    // Online
    bool isOnline;

    // Internal
    QStringList _blocked;
    QVariantMap _privacy;
    QVariantMap _mutedGroups;
    QString _activeJid;
    QVariantMap _contacts;
    QHash<QString, int> _unreadCount;
    int _totalUnread;
    int _pendingCount;
    //QHash<QString, MNotification*> _offlineMessages;
    QStringList _availableJids;
    QHash<QString, MNotification*> _notificationJid;
    QHash<QString, MNotification*> _notificationPendingJid;
    QHash<QString, QTimer*> _pendingNotificationsTimer;
    QHash<QString, QString> _mediaJidHash;
    QHash<QString, QString> _mediaMsgidHash;
    QHash<QString, QString> _mediaNameHash;
    QHash<QString, bool> _mediaStatusHash;
    QHash<QString, MediaDownload*> _mediaDownloadHash;

    QVariantMap _synccontacts;
    QVariantMap _syncavatars;

    QContactManager *contactManager;

    MNotification *connectionNotification;
    MNotification *offlineMesagesNotification;
    MNotification *offlineNotificationsNotification;
    void updateNotification(const QString &text);

    qint64 lastDisconnect;
    int disconnectCount;
    int reconnectInterval;
    int lastReconnect;

    QVariantMap _mediaProgress;

    QString uuid;

    QString waversion;

    QTranslator translator;

    BackgroundActivity *keepalive;

    /** ***********************************************************************
     ** Private methods
     **/

    /** ***********************************************************************
     ** Settings
     **/

    // Sync frequency (see SyncFrequencies enum above)
    int syncFreq;

    // Last time address book synchronizarion was performed
    qint64 lastSync;

    bool acceptUnknown;

    //static bool softbankReplacer;
    bool alwaysOffline;
    bool resizeImages;
    bool resizeWlan;
    bool resizeBySize;
    int resizeImagesTo;
    float resizeImagesToMPix;
    bool autoDownloadMedia;
    int autoBytes;
    bool autoDownloadWlan;
    bool showConnectionNotifications;
    bool notificationsMuted;
    bool notifyMessages;
    bool systemNotifier;
    bool useKeepalive;
    int reconnectionInterval;
    int reconnectionLimit;
    bool disconnectStreamError;
    bool usePhonebookAvatars;
    int notificationsDelay;

    QMap<QString, qlonglong> mutingList;

    // Reads the global settings and store them in the public static members
    void readSettings();
    void saveAccountData();
    void saveRegistrationData(const QVariantMap &result);
    QStringList getPhoneInfo(const QString &phone);
    void addMessage(const FMessage &message);
    void saveGroupInfo(const QString &jid, const QString &owner, const QString &subject, const QString &subjectOwner, int subjectT, int creation);

    int getUnreadCount(const QString &jid);
    void setUnreadCount(const QString &jid, int count);

    void updateActiveNetworkID();

    void getTokenScratch();

    void notyfyConnectionStatus();
    void clearNotification();

    void groupNotification(const QString &gjid, const QString &jid, int type, const QString &timestamp, const QString &notificationId, bool offline = false, QString notification = QString());

    void startDownloadMessage(const FMessage &msg);

    void showOfflineNotifications();

public slots:
    void ready();

    void saveCredentials(const QVariantMap &data);
    void connectionClosed();
    int currentStatus();
    bool isNetworkAvailable();
    bool isAccountValid();
    void recheckAccountAndConnect();
    void sendText(const QString &jid, const QString &message);
    void sendMedia(const QStringList &jids, const QString &fileName, int waType, const QString &title);
    void sendMedia(const QStringList &jids, const QString &fileName, const QString &mediaType, const QString &title);
    void sendMedia(const QStringList &jids, const QString &fileName, const QString &title = QString());
    void sendVCard(const QStringList &jids, const QString &name, const QString &vcardData);
    void sendLocation(const QStringList &jids, const QString &latitude, const QString &longitude, int zoom, const QString &source);
    void getGroupInfo(const QString &jid);
    void getPicture(const QString &jid);
    void setPhoto(const QString &jid, const QString &path);
    void synchronizeContacts();
    void synchronizePhonebook();
    void setActiveJid(const QString &jid);
    void syncNumber(const QString &phone);
    void startSharing(const QStringList &jids, const QString &name, const QString &data);
    QString getMyAccount();
    void startTyping(const QString &jid);
    void startRecording(const QString &jid);
    void endTyping(const QString &jid);
    bool getAvailable(const QString &jid);
    bool getBlocked(const QString &jid);
    void exit();
    void broadcastSend(const QStringList &jids, const QString &message);
    void downloadMedia(const QString &msgId, const QString &jid);
    QVariantMap getDownloads();
    void cancelDownload(const QString &msgId, const QString &jid);
    void createGroupChat(const QString &subject);
    void getParticipants(const QString &gjid);
    void regRequest(const QString &cc, const QString &phone, const QString &method, const QString &password, const QString &mcc, const QString &mnc);
    void enterCode(const QString &cc, const QString &phone, const QString &smscode);
    void setGroupSubject(const QString &gjid, const QString &subject);
    void removeGroupParticipant(const QString &gjid, const QString &jid);
    void addGroupParticipant(const QString &gjid, const QString &jid);
    void addGroupParticipants(const QString &gjid, const QStringList &jids);
    void blockOrUnblockContact(const QString &jid);
    void sendBlockedJids(const QStringList &jids);
    void setPrivacySettings(const QString &category, const QString &value);
    void muteOrUnmute(const QString &jid, int expire);
    void requestLeaveGroup(const QString &gjid);
    void requestRemoveGroup(const QString &gjid);
    void setPicture(const QString &jid, const QString &path);
    void getContactStatus(const QString &jid);
    void refreshContact(const QString &jid);
    void requestQueryLastOnline(const QString &jid);
    void requestPrivacyList();
    void requestPrivacySettings();
    void getPrivacyList();
    void getPrivacySettings();
    void getMutedGroups();
    void getAvailableJids();
    void forwardMessage(const QStringList &jids, const QVariantMap &model);
    void resendMessage(const QString &jid, const QVariantMap &model);
    void changeStatus(const QString &newStatus);
    void changeUserName(const QString &newUserName);
    void newContactAdded(QString jid);
    void newContactAdd(const QString &name, const QString &number);
    void removeAccount();
    void syncContacts(const QStringList &numbers, const QStringList &names, const QStringList &avatars);
    void setPresenceAvailable();
    void setPresenceUnavailable();
    void sendStreamEnd();
    void removeAccountFromServer();
    void forceConnection();
    void openConnectionDialog();
    void contactRemoved(const QString &jid);
    void setLocale(const QString &locale);
    void windowActive();
    void connectToServer();
    void disconnect();
    void getNetworkUsage();
    void resetNetworkUsage();
    void ping();
    void renewAccount(const QString &method, int years);
    void unsubscribe(const QString &jid);
    void settingsChanged();
    void saveHistory(const QString &sjid, const QString &sname);
    void requestContactMedia(const QString &sjid);
};

#endif // CLIENTTHREAD_H
