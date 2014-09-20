#include "mediadownload.h"

#include "util/utilities.h"

#include "src/globalconstants.h"

#include "src/client.h"

MediaDownload::MediaDownload(FMessage message, QObject *parent) :
    HttpRequestv2(parent)
{
    this->message = message;
}

void MediaDownload::backgroundTransfer()
{
    fileName = getFileNameForMessage(message);
    file.setFileName(fileName);

    connect(this,SIGNAL(finished()),
            this,SLOT(onResponse()));

    qDebug() << "Download media:" << message.media_url << "to" << fileName;
    get(message.media_url);

    emit progress(this->message, 1.0);
}

void MediaDownload::onSocketError(QAbstractSocket::SocketError error)
{
    qDebug() << "Media download socket error:" << QString::number(error);
    Q_EMIT httpError(this, this->message, 123);
}

void MediaDownload::onResponse()
{
    qDebug() << "HTTP Response. Bytes available:" << QString::number(socket->bytesAvailable());

    if (errorCode != 200)
    {
        emit httpError(this, message, errorCode);
        return;
    }

    totalLength = getHeader("Content-Length").toLongLong();
    bytesWritten = 0;

    if (!file.open(QIODevice::WriteOnly))
    {
        // An error has occurred
        qDebug() << "MediaDownload: Error while trying to opening file:" << fileName;
        socket->close();
    }

    if (socket->bytesAvailable())
        writeToFile();

    connect(socket,SIGNAL(readyRead()),this,SLOT(writeToFile()));
}

void MediaDownload::writeToFile()
{
    qint64 bytesToRead = socket->bytesAvailable();

    QByteArray buffer;
    buffer.resize(bytesToRead);

    if (bytesToRead + bytesWritten > totalLength)
        bytesToRead = totalLength - bytesWritten;

    if (file.write(socket->read(bytesToRead)) != bytesToRead)
    {
        // An error has occurred
        qDebug() << "MediaDownload: Error while trying to opening file";
        file.close();
        socket->close();
    }

    bytesWritten += bytesToRead;

    float p = ((float)((bytesWritten) * 100.0)) / ((float)totalLength);

    emit progress(message,p);

    if (bytesWritten == totalLength)
    {
        file.close();
        socket->close();

        qDebug() << "MediaDownload: Downloading finished.";

        message.local_file_uri = fileName;

        emit downloadFinished(this, message);
    }
}

QString MediaDownload::getFileNameForMessage(FMessage message)
{
    bool importMediaToGallery = Client::dconf->value("settings/importToGallery").toBool();
    QString path = Utilities::getPathFor(message.media_wa_type, importMediaToGallery);

    QDir home = QDir::home();
    if (!home.exists(path))
        home.mkpath(path);

    // Let's try to be organized here with the downloads
    /*if (Client::importMediaToGallery)
    {
        path.append(WHATSAPP_DIR);
        QDir home = QDir::home();
        if (!home.exists(path))
            home.mkpath(path);
    }*/

    QString fileName = path + "/" + message.media_name;
    int pos = fileName.lastIndexOf('.');
    if (pos < 0)
        pos = fileName.length();

    QString extension = fileName.mid(pos);

    // Loop until a fileName is not currently being used
    QFile file(fileName);
    for (int count = 1; file.exists(); count++)
        file.setFileName(fileName.left(pos) + "_" + QString::number(count) + extension);

    return file.fileName();
}
