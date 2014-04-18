#include "contactsbasemodel.h"
#include "constants.h"
#include <QDebug>
#include <cmath>

#include <QUuid>

ContactsBaseModel::ContactsBaseModel(QObject *parent) :
    QAbstractListModel(parent)
{
    _keys << "jid";
    _keys << "pushname";
    _keys << "name";
    _keys << "nickname";
    _keys << "message";
    _keys << "contacttype";
    _keys << "owner";
    _keys << "subowner";
    _keys << "timestamp";
    _keys << "subtimestamp";
    _keys << "avatar";
    _keys << "unread";
    _keys << "available";
    _keys << "lastmessage";
    _keys << "blocked";
    _keys << "typing";
    int role = Qt::UserRole + 1;
    foreach (const QString &rolename, _keys) {
        _roles[role++] = rolename.toLatin1();
    }

    uuid = QUuid::createUuid().toString();

    iface = new QDBusInterface(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE, QDBusConnection::sessionBus(), this);

    dbExecutor = QueryExecutor::GetInstance();
    connect(dbExecutor, SIGNAL(actionDone(QVariant)), this, SLOT(dbResults(QVariant)));


    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "pictureUpdated", this, SLOT(pictureUpdated(QString,QString)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "setUnread", this, SLOT(setUnread(QString,int)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "pushnameUpdated", this, SLOT(pushnameUpdated(QString, QString)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactAvailable", this, SLOT(presenceAvailable(QString)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactUnavailable", this, SLOT(presenceUnavailable(QString)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactLastSeen", this, SLOT(presenceLastSeen(QString, int)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactChanged", this, SLOT(contactChanged(QVariantMap)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactSynced", this, SLOT(contactSynced(QVariantMap)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactsChanged", this, SLOT(contactsChanged()));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "newGroupSubject", this, SLOT(newGroupSubject(QVariantMap)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "messageReceived", this, SLOT(messageReceived(QVariantMap)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactStatus", this, SLOT(contactStatus(QString, QString)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactsBlocked", this, SLOT(contactsBlocked(QStringList)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "groupsMuted", this, SLOT(groupsMuted(QStringList)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactsAvailable", this, SLOT(contactsAvailable(QStringList)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactTyping", this, SIGNAL(contactTyping(QString)));
    QDBusConnection::sessionBus().connect(SERVER_SERVICE, SERVER_PATH, SERVER_INTERFACE,
                                          "contactPaused", this, SIGNAL(contactPaused(QString)));
    if (iface) {
        iface->call(QDBus::NoBlock, "getPrivacyList");
        iface->call(QDBus::NoBlock, "getMutedGroups");
        iface->call(QDBus::NoBlock, "getAvailableJids");
    }

    contactsChanged();
}

ContactsBaseModel::~ContactsBaseModel()
{
}

void ContactsBaseModel::reloadContact(const QString &jid)
{
    QVariantMap query;
    query["type"] = QueryType::ContactsReloadContact;
    query["jid"] = jid;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

void ContactsBaseModel::setPropertyByJid(const QString &jid, const QString &name, const QVariant &value)
{
    if (_modelData.keys().contains(jid)) {
        //qDebug() << "Model setPropertyByJid:" << jid << name << value;
        if (_modelData.keys().contains(jid)) {
            int row = _modelData.keys().indexOf(jid);
            if (name == "avatar") {
                _modelData[jid][name] = QString();
                Q_EMIT dataChanged(index(row), index(row));
            }
            _modelData[jid][name] = value;
            Q_EMIT dataChanged(index(row), index(row));
        }

        if (name == "unread")
            checkTotalUnread();
    }
}

void ContactsBaseModel::deleteContact(const QString &jid)
{
    if (_modelData.keys().contains(jid)) {
        int row = _modelData.keys().indexOf(jid);
        beginRemoveRows(QModelIndex(), row, row);
        _modelData.remove(jid);
        QVariantMap query;
        query["type"] = QueryType::ContactsRemove;
        query["jid"]  = jid;
        query["uuid"] = uuid;
        dbExecutor->queueAction(query);
        endRemoveRows();

        iface->call(QDBus::NoBlock, "contactRemoved", jid);
    }
}

QVariantMap ContactsBaseModel::getModel(const QString &jid)
{
    if (_modelData.keys().contains(jid))
        return _modelData[jid];
    return QVariantMap();
}

QVariantMap ContactsBaseModel::get(int index)
{
    if (index < 0 || index >= _modelData.count())
        return QVariantMap();
    return _modelData[_modelData.keys().at(index)];
}

QColor ContactsBaseModel::getColorForJid(const QString &jid)
{
    if (!_colors.keys().contains(jid))
        _colors[jid] = generateColor();
    QColor color = _colors[jid];
    //color.setAlpha(96);
    return color;
}

void ContactsBaseModel::clear()
{
    beginResetModel();
    _modelData.clear();
    endResetModel();
    //reset();
}

QColor ContactsBaseModel::generateColor()
{
    qreal golden_ratio_conjugate = 0.618033988749895;
    qreal h = (qreal)rand()/(qreal)RAND_MAX;
    h += golden_ratio_conjugate;
    h = fmod(h, 1);
    QColor color = QColor::fromHsvF(h, 0.5, 0.95);
    return color;
}

int ContactsBaseModel::getTotalUnread()
{
    return _totalUnread;
}

void ContactsBaseModel::checkTotalUnread()
{
    _totalUnread = 0;
    foreach (const QVariantMap &contact, _modelData.values()) {
        //qDebug() << contact["jid"].toString() << "unread:" << contact["unread"].toInt();
        _totalUnread += contact["unread"].toInt();
    }
    //qDebug() << "Total unread:" << QString::number(_totalUnread);
    Q_EMIT totalUnreadChanged();
}

bool ContactsBaseModel::getAvailable(const QString &jid)
{
    return _availableContacts.contains(jid);
}

bool ContactsBaseModel::getBlocked(const QString &jid)
{
    return _blockedContacts.contains(jid);
}

bool ContactsBaseModel::getMuted(QString jid)
{
    return _mutedGroups.contains(jid);
}

QString ContactsBaseModel::getNicknameBy(const QString &jid, const QString &message, const QString &name, const QString &pushname)
{
    QString nickname;
    if (jid.contains("-")) {
        nickname = message;
    }
    else if (name == jid.split("@").first() || name.isEmpty()) {
        if (!pushname.isEmpty())
            nickname = pushname;
        else
            nickname = jid.split("@").first();
    }
    else {
        nickname = name;
    }
    return nickname;
}

void ContactsBaseModel::pictureUpdated(const QString &jid, const QString &path)
{
    setPropertyByJid(jid, "avatar", path);
}

void ContactsBaseModel::contactChanged(const QVariantMap &data)
{
    QVariantMap contact = data;
    QString jid = contact["jid"].toString();

    QString name = contact["name"].toString();
    QString message = contact["message"].toString();
    QString pushname = contact["pushname"].toString();
    QString nickname = getNicknameBy(jid, message, name, pushname);

    contact["nickname"] = nickname;

    _modelData[jid] = contact;

    int row = _modelData.keys().indexOf(jid);
    Q_EMIT dataChanged(index(row), index(row));
}

void ContactsBaseModel::contactSynced(const QVariantMap &data)
{
    QVariantMap contact = data;
    QString jid = contact["jid"].toString();
    if (_modelData.keys().contains(jid)) {
        _modelData[jid]["timestamp"] = contact["timestamp"];
        QString message = contact["message"].toString();
        _modelData[jid]["message"] = message;

        QString name = contact["name"].toString();
        QString pushname = _modelData[jid]["pushname"].toString();

        _modelData[jid]["nickname"] = getNicknameBy(jid, message, name, pushname);

        bool blocked = getBlocked(jid);
        _modelData[jid]["blocked"] = blocked;

        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));

        if (_modelData[jid]["avatar"].toString().isEmpty())
            requestAvatar(jid);
    }
}

void ContactsBaseModel::contactStatus(const QString &jid, const QString &message)
{
    if (_modelData.keys().contains(jid)) {
        _modelData[jid]["message"] = message;
        int row = _modelData.keys().indexOf(jid);
        dataChanged(index(row), index(row));
    }
}

void ContactsBaseModel::newGroupSubject(const QVariantMap &data)
{
    QString jid = data["jid"].toString();
    if (_modelData.keys().contains(jid)) {
        QString message = data["message"].toString();
        QString subowner = data["subowner"].toString();
        int lastmessage = _modelData[jid]["lastmessage"].toInt();
        QString subtimestamp = data["subtimestamp"].toString();
        qDebug() << "Model upgate group subject" << message << "jid:" << jid << "lastmessage:" << QString::number(lastmessage);

        _modelData[jid]["message"] = message;
        _modelData[jid]["nickname"] = message;
        _modelData[jid]["subowner"] = subowner;
        _modelData[jid]["subtimestamp"] = subtimestamp;

        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));

        qDebug() << "New subject saved:" << message << "for jid:" << jid;
    }
}

void ContactsBaseModel::contactsChanged()
{
    QVariantMap query;
    query["type"] = QueryType::ContactsGetAll;
    query["uuid"] = uuid;
    dbExecutor->queueAction(query);
}

void ContactsBaseModel::deleteEverything()
{
    QVariantMap query;
    query["uuid"] = uuid;
    //query["type"] = QueryType::DeleteEverything;
    dbExecutor->queueAction(query, 1000);
}

void ContactsBaseModel::setUnread(const QString &jid, int count)
{
    setPropertyByJid(jid, "unread", count);
}

void ContactsBaseModel::pushnameUpdated(const QString &jid, const QString &pushName)
{
    if (_modelData.keys().contains(jid) && (pushName != jid.split("@").first())) {
        setPropertyByJid(jid, "pushname", pushName);

        QString nickname = _modelData[jid]["nickname"].toString();
        QString pushname = _modelData[jid]["pushname"].toString();
        QString message = _modelData[jid]["message"].toString();
        QString name = _modelData[jid]["name"].toString();

        nickname = getNicknameBy(jid, message, name, pushname);

        /*if (!jid.contains("-") && !pushname.isEmpty()) {
            if (name == jid.split("@").first())
                nickname = pushName;
        }*/
        _modelData[jid]["nickname"] = nickname;

        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));

        Q_EMIT nicknameChanged(jid, nickname);
    }
}

void ContactsBaseModel::presenceAvailable(const QString &jid)
{
    //qDebug() << "presenceAvailable" << jid;
    if (!_availableContacts.contains(jid))
        _availableContacts.append(jid);
    setPropertyByJid(jid, "available", true);
}

void ContactsBaseModel::presenceUnavailable(const QString &jid)
{
    //qDebug() << "presenceUnavailable" << jid;
    if (_availableContacts.contains(jid))
        _availableContacts.removeAll(jid);
    setPropertyByJid(jid, "available", false);
}

void ContactsBaseModel::presenceLastSeen(const QString jid, int timestamp)
{
    setPropertyByJid(jid, "timestamp", timestamp);
}

void ContactsBaseModel::messageReceived(const QVariantMap &data)
{
    //qDebug() << "MessageReceived:" << data["jid"] << data["message"];
    QString jid = data["jid"].toString();
    int lastmessage = data["timestamp"].toInt();
    if (_modelData.keys().contains(jid)) {
        _modelData[jid]["lastmessage"] = lastmessage;

        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));
    }
    else {
        contactsChanged();
    }
}

void ContactsBaseModel::contactsBlocked(const QStringList &jids)
{
    _blockedContacts = jids;
    foreach (const QString &jid, _modelData.keys()) {
        if (!jid.contains("-")) {
            if (jids.contains(jid))
                _modelData[jid]["blocked"] = true;
            else
                _modelData[jid]["blocked"] = false;
        }
    }
    Q_EMIT dataChanged(index(0), index(_modelData.count() - 1));
}

void ContactsBaseModel::groupsMuted(const QStringList &jids)
{
    _mutedGroups = jids;
    foreach (const QString &jid, _modelData.keys()) {
        if (jid.contains("-")) {
            if (jids.contains(jid))
                _modelData[jid]["blocked"] = true;
            else
                _modelData[jid]["blocked"] = false;
        }
    }
    Q_EMIT dataChanged(index(0), index(_modelData.count() - 1));
}

void ContactsBaseModel::contactsAvailable(const QStringList &jids)
{
    _availableContacts = jids;
    foreach (const QString &jid, _modelData.keys()) {
        if (jids.contains(jid))
            _modelData[jid]["available"] = true;
        else
            _modelData[jid]["available"] = false;

        Q_EMIT dataChanged(index(0), index(_modelData.count() - 1));
    }
}

void ContactsBaseModel::contactTyping(const QString &jid)
{
    if (_modelData.keys().contains(jid)) {
        _modelData[jid]["typing"] = true;

        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));
    }
}

void ContactsBaseModel::contactPaused(const QString &jid)
{
    if (_modelData.keys().contains(jid)) {
        _modelData[jid]["typing"] = false;

        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));
    }
}

void ContactsBaseModel::dbResults(const QVariant &result)
{
    //qDebug() << "dbResults received";
    QVariantMap reply = result.toMap();
    int vtype = reply["type"].toInt();

    if (vtype == QueryType::ContactsSaveModel) {
        contactChanged(reply);
    }

    if (reply["uuid"].toString() != uuid)
        return;
    switch (vtype) {
    case QueryType::ContactsReloadContact: {
        QVariantMap contact = reply["contact"].toMap();
        contactChanged(contact);
        break;
    }
    case QueryType::ContactsGetAll: {
        QVariantList records = reply["contacts"].toList();
        qDebug() << "Received QueryGetContacts reply. Size:" << QString::number(records.size());
        if (records.size() > 0) {
            beginResetModel();
            _modelData.clear();
            foreach (const QVariant &c, records) {
                QVariantMap data = c.toMap();
                QString jid = data["jid"].toString();
                QString pushname = data["pushname"].toString();
                QString name = data["name"].toString();
                QString message = data["message"].toString();
                qDebug() << "jid:" << jid << pushname << name << message;
                bool blocked = false;
                if (jid.contains("-"))
                    blocked = getMuted(jid);
                else
                    blocked = getBlocked(jid);
                data["blocked"] = blocked;
                bool available = getAvailable(jid);
                data["available"] = available;
                QString nickname = getNicknameBy(jid, message, name, pushname);
                data["nickname"] = nickname;
                data["typing"] = false;
                _modelData[jid] = data;
                if (!_colors.keys().contains(jid))
                    _colors[jid] = generateColor();
                if (data["avatar"].toString().isEmpty())
                    requestAvatar(jid);
            }
            endResetModel();
            //reset();
        }
        //qDebug() << "inserted" << QString::number(_modelData.size()) << "rows";
        checkTotalUnread();
        break;
    }
    //case QueryType::DeleteEverything: {
    //    Q_EMIT deleteEverythingSuccessful();
    //    break;
    //}
    }
}

int ContactsBaseModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    //qDebug() << "Model count:" << _modelData.size();
    return _modelData.size();
}

QVariant ContactsBaseModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= _modelData.count())
        return QVariantMap();
    return _modelData[_modelData.keys().at(row)][_roles[role]];
}

bool ContactsBaseModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    qDebug() << "Model setData" << index.row() << value << role;
    return false;
}

int ContactsBaseModel::count()
{
    return _modelData.count();
}


void ContactsBaseModel::renameContact(const QString &jid, const QString &name)
{
    if (_modelData.keys().contains(jid)) {
        _modelData[jid]["name"] = name;
        QString pushname = _modelData[jid]["pushname"].toString();

        QString nickname;
        if (name == jid.split("@").first()) {
            if (!pushname.isEmpty())
                nickname = pushname;
            else
                nickname = name;
        }
        else {
            nickname = name;
        }
        _modelData[jid]["nickname"] = nickname;
        int row = _modelData.keys().indexOf(jid);
        Q_EMIT dataChanged(index(row), index(row));

        QVariantMap query = _modelData[jid];
        query["type"] = QueryType::ContactsSaveModel;
        dbExecutor->queueAction(query);
    }
}

void ContactsBaseModel::requestAvatar(const QString &jid)
{
    if (iface) {
        iface->call(QDBus::NoBlock, "getPicture", jid);
    }
}
