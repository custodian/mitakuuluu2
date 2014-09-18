#include <QGuiApplication>
#include <QDebug>
#include <sailfishapp.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include <QLocale>
#include <QTranslator>

#include "client.h"

#include "../dconf/dconfmigration.h"
#include "../dconf/mdconfitem.h"
#include "../logging/logging.h"

Q_DECL_EXPORT
int main(int argc, char *argv[])
{
    setuid(getpwnam("nemo")->pw_uid);
    setgid(getgrnam("privileged")->gr_gid);

    migrate_dconf();

    MDConfItem ready("/apps/harbour-mitakuuluu2/migrationIsDone");
    if (!ready.value(false).toBool()) {
        qDebug() << "QSettings was migrated to dconf!";
        ready.set(true);
    }

    MDConfItem keepLogs("/apps/harbour-mitakuuluu2/settings/keepLogs");
    if (keepLogs.value(true).toBool())
        qInstallMessageHandler(fileHandler);
    else
        qInstallMessageHandler(stdoutHandler);

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName("harbour-mitakuuluu2");
    app->setApplicationName("harbour-mitakuuluu2");
    qRegisterMetaType<FMessage>("FMessage");
    qDebug() << "Creating D-Bus class";
    QScopedPointer<Client> dbus(new Client(app.data()));
    qDebug() << "D-Bus working";
    int retval = app->exec();
    qDebug() << "App exiting with code:" << QString::number(retval);
    return retval;
}

