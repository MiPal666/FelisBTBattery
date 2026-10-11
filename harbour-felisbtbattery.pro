TARGET = harbour-felisbtbattery

CONFIG += sailfishapp sailfishapp_i18n c++11
QT += core qml quick dbus

SOURCES += \
    src/main.cpp \
    src/bluetoothdevicemodel.cpp

HEADERS += \
    src/bluetoothdevicemodel.h

TRANSLATIONS += \
    translations/harbour-felisbtbattery-cs.ts

DISTFILES += \
    harbour-felisbtbattery.desktop \
    qml/harbour-felisbtbattery.qml \
    qml/pages/MainPage.qml \
    qml/pages/NoticePage.qml \
    qml/pages/AboutPage.qml \
    qml/cover/CoverPage.qml \
    rpm/harbour-felisbtbattery.spec \
    rpm/harbour-felisbtbattery.changes \
    backend/README.md \
    backend/Makefile \
    backend/src/felisbtbattery.c \
    backend/src/felisbtbattery.part1.inc \
    backend/src/felisbtbattery.part2.inc \
    backend/src/felisbtbattery.part3.inc \
    backend/src/felisbtbattery.part4.inc \
    backend/LICENSE.GPL-2.0 \
    icons/86x86/harbour-felisbtbattery.png \
    icons/108x108/harbour-felisbtbattery.png \
    icons/128x128/harbour-felisbtbattery.png \
    icons/172x172/harbour-felisbtbattery.png \
    README.md \
    LICENSE

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
