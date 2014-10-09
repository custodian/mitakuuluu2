#ifndef MAPREQUEST_H
#define MAPREQUEST_H

#include <QObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>

class MapRequest : public QObject
{
    Q_OBJECT
public:
    explicit MapRequest(const QString &source, const QString &latitude, const QString &longitude, int zoom, int width,
                        int height, const QStringList &jids, const QStringList &participants, const QString &broadcastName,
                        QObject *parent = 0);

private:
    QNetworkAccessManager *nam;
    QNetworkRequest _query;
    QStringList _jids;
    QStringList _participants;
    QString _broadcastName;
    QString _latitude;
    QString _longitude;

signals:
    void mapAvailable(const QByteArray &mapData, const QString &latitude, const QString &longitude, const QStringList &jids, const QStringList &participants, const QString &broadcastName, MapRequest* sender);
    void requestError(MapRequest* sender);

public slots:
    void doRequest();

private slots:
    void requestComplete();

};

#endif // MAPREQUEST_H
