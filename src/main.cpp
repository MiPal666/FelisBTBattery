#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <sailfishapp.h>

#include "bluetoothdevicemodel.h"

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    QQuickView *view = SailfishApp::createView();

    BluetoothDeviceModel btModel;
    view->rootContext()->setContextProperty("btModel", &btModel);

    view->setSource(SailfishApp::pathTo("qml/harbour-felisbtbattery.qml"));
    view->show();

    return app->exec();
}
