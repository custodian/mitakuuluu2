TARGET = harbour-mitakuuluu2-server
target.path = /usr/bin

QT += dbus sql quick qml multimedia
CONFIG += Qt5Contacts link_pkgconfig
PKGCONFIG += Qt5Contacts sailfishapp contactcache-qt5 qtcontacts-sqlite-qt5-extensions keepalive dconf mlite5 gobject-2.0
LIBS += -lmlite5

INCLUDEPATH += /usr/include/sailfishapp
INCLUDEPATH += /usr/include/mlite5
INCLUDEPATH += /usr/include/qt5/QtContacts
INCLUDEPATH += src/

#gui.files = qml
#gui.path = /usr/share/harbour-mitakuuluu2

#INSTALLS += gui

data.files = data/mime-types.tab \
             data/countries.csv
data.path = /usr/share/harbour-mitakuuluu2/data

notification.files = notification/harbour.mitakuuluu2.message.conf \
                     notification/harbour.mitakuuluu2.notification.conf \
                     notification/harbour.mitakuuluu2.private.conf \
                     notification/harbour.mitakuuluu2.group.conf \
                     notification/harbour.mitakuuluu2.media.conf
notification.path = /usr/share/lipstick/notificationcategories/

patterns.files = patterns/60-mitakuuluu_led.ini
patterns.path = /etc/mce

events.files = events/mitakuuluu_group.ini \
               events/mitakuuluu_media.ini \
               events/mitakuuluu_private.ini
events.path = /usr/share/ngfd/events.d/

profiled.files = profiled/60.mitakuuluu.ini
profiled.path = /etc/profiled

systemd.files = systemd/harbour-mitakuuluu2.service
systemd.path = /usr/lib/systemd/user

icons.files = icons/notification.png
icons.path = /usr/share/harbour-mitakuuluu2/images

dbus.files = dbus/harbour.mitakuuluu2.server.service
dbus.path = /usr/share/dbus-1/services

INSTALLS = target data notification patterns events profiled icons dbus systemd

SOURCES += src/whatsapp-server.cpp \
    ../threadworker/threadworker.cpp \
    ../threadworker/queryexecutor.cpp \
    ../qexifimageheader/qexifimageheader.cpp \
    src/client.cpp \
    src/Whatsapp/util/utilities.cpp \
    src/Whatsapp/util/messagedigest.cpp \
    src/Whatsapp/util/qtmd5digest.cpp \
    src/Whatsapp/protocoltreenode.cpp \
    src/Whatsapp/fmessage.cpp \
    src/Whatsapp/funstore.cpp \
    src/Whatsapp/key.cpp \
    src/Whatsapp/attributelist.cpp \
    src/Whatsapp/protocoltreenodelist.cpp \
    src/Whatsapp/bintreenodewriter.cpp \
    src/Whatsapp/protocoltreenodelistiterator.cpp \
    src/Whatsapp/attributelistiterator.cpp \
    src/Whatsapp/connection.cpp \
    src/Whatsapp/bintreenodereader.cpp \
    src/Whatsapp/ioexception.cpp \
    src/Whatsapp/protocolexception.cpp \
    src/Whatsapp/warequest.cpp \
    src/Whatsapp/phonereg.cpp \
    src/Whatsapp/smslistener.cpp \
    src/Whatsapp/util/datetimeutilities.cpp \
    src/Whatsapp/exception.cpp \
    src/Whatsapp/loginexception.cpp \
    src/Whatsapp/formdata.cpp \
    src/Whatsapp/keystream.cpp \
    src/Whatsapp/rc4.cpp \
    src/Whatsapp/util/qthmacsha1.cpp \
    src/Whatsapp/util/qtrfc2898.cpp \
    src/Whatsapp/mediaupload.cpp \
    src/Whatsapp/multipartuploader.cpp \
    src/Whatsapp/httprequestv2.cpp \
    src/Whatsapp/mediadownload.cpp \
    src/Whatsapp/util/datacounters.cpp \
    src/Whatsapp/maprequest.cpp \
    src/contactsfetch.cpp \
    ../dconf/mdconf.cpp \
    ../dconf/mdconfitem.cpp \
    ../dconf/mdconfagent.cpp

HEADERS += \
    ../threadworker/threadworker.h \
    ../threadworker/queryexecutor.h \
    ../qexifimageheader/qexifimageheader.h \
    src/version.h \
    src/globalconstants.h \
    src/client.h \
    src/Whatsapp/util/utilities.h \
    src/Whatsapp/util/messagedigest.h \
    src/Whatsapp/util/qtmd5digest.h \
    src/Whatsapp/protocoltreenode.h \
    src/Whatsapp/fmessage.h \
    src/Whatsapp/funstore.h \
    src/Whatsapp/key.h \
    src/Whatsapp/attributelist.h \
    src/Whatsapp/protocoltreenodelist.h \
    src/Whatsapp/bintreenodewriter.h \
    src/Whatsapp/protocoltreenodelistiterator.h \
    src/Whatsapp/attributelistiterator.h \
    src/Whatsapp/bintreenodereader.h \
    src/Whatsapp/connection.h \
    src/Whatsapp/ioexception.h \
    src/Whatsapp/protocolexception.h \
    src/Whatsapp/warequest.h \
    src/Whatsapp/phonereg.h \
    src/Whatsapp/smslistener.h \
    src/Whatsapp/util/datetimeutilities.h \
    src/Whatsapp/exception.h \
    src/Whatsapp/loginexception.h \
    src/Whatsapp/formdata.h \
    src/Whatsapp/keystream.h \
    src/Whatsapp/rc4.h \
    src/Whatsapp/util/qthmacsha1.h \
    src/Whatsapp/util/qtrfc2898.h \
    src/Whatsapp/mediaupload.h \
    src/Whatsapp/multipartuploader.h \
    src/Whatsapp/httprequestv2.h \
    src/Whatsapp/mediadownload.h \
    src/Whatsapp/util/datacounters.h \
    ../logging/logging.h \
    src/Whatsapp/maprequest.h \
    src/contactsfetch.h \
    ../dconf/mdconf_p.h \
    ../dconf/mdconfitem.h \
    ../dconf/mdconfagent.h

OTHER_FILES += $$files(rpm/*) \
    rpm/server.spec \
    notification/harbour.mitakuuluu2.notification.conf \
    dbus/harbour-mitakuuluu2.service

