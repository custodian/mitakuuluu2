#include "translator.h"

#include <QGuiApplication>
#include <QStringList>
#include <QLocale>
#include <QSettings>
#include <QDebug>

Translator::Translator(QObject *parent) :
    QObject(parent)
{
    QSettings settings("coderus", "mitakuuluu2");
    QString currentLocale = settings.value("settings/locale", QString("%1.qm").arg(QLocale::system().name().split(".").first())).toString();

    translator = new QTranslator(this);
    qDebug() << "loading translation" << currentLocale;
    if (translator->load(currentLocale, "/usr/share/harbour-mitakuuluu2/locales", QString(), ".qm")) {
        qDebug() << "translation loaded";
        QGuiApplication::installTranslator(translator);
    }
    else {
        qDebug() << "translation not available";
    }
}
