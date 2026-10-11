#include "bluetoothdevicemodel.h"

#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QMap>
#include <algorithm>

typedef QMap<QString, QVariantMap> BluezInterfaceMap;
typedef QMap<QDBusObjectPath, BluezInterfaceMap> BluezManagedObjects;

Q_DECLARE_METATYPE(BluezInterfaceMap)
Q_DECLARE_METATYPE(BluezManagedObjects)

BluetoothDeviceModel::BluetoothDeviceModel(QObject *parent)
    : QAbstractListModel(parent),
      m_bus(QDBusConnection::systemBus())
{
    m_bus.connect(QStringLiteral("org.bluez"),
                  QStringLiteral("/"),
                  QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                  QStringLiteral("InterfacesAdded"),
                  this,
                  SLOT(onBluezObjectManagerSignal(QDBusMessage)));

    m_bus.connect(QStringLiteral("org.bluez"),
                  QStringLiteral("/"),
                  QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                  QStringLiteral("InterfacesRemoved"),
                  this,
                  SLOT(onBluezObjectManagerSignal(QDBusMessage)));

    // Live updates: no polling timer and no FelisBTBattery daemon.
    m_bus.connect(QStringLiteral("org.bluez"),
                  QString(),
                  QStringLiteral("org.freedesktop.DBus.Properties"),
                  QStringLiteral("PropertiesChanged"),
                  this,
                  SLOT(onBluezPropertiesChanged(QDBusMessage)));

    m_bus.connect(QStringLiteral("org.freedesktop.DBus"),
                  QStringLiteral("/org/freedesktop/DBus"),
                  QStringLiteral("org.freedesktop.DBus"),
                  QStringLiteral("NameOwnerChanged"),
                  this,
                  SLOT(onNameOwnerChanged(QString,QString,QString)));

    refresh();
}

int BluetoothDeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.count();
}

QVariant BluetoothDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.count())
        return QVariant();

    const Device &d = m_devices.at(index.row());

    switch (role) {
    case ObjectPathRole:        return d.objectPath;
    case NameRole:              return d.name;
    case AddressRole:           return d.address;
    case IconRole:              return d.icon;
    case ConnectedRole:         return d.connected;
    case PairedRole:            return d.paired;
    case BatteryAvailableRole:  return !d.batteries.isEmpty();
    case BatteryPercentageRole: return d.batteryPercentage;
    case BatterySourceRole:     return d.batterySource;
    case BatteriesRole:         return d.batteries;
    default:                    return QVariant();
    }
}

QHash<int, QByteArray> BluetoothDeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ObjectPathRole] = "objectPath";
    roles[NameRole] = "deviceName";
    roles[AddressRole] = "address";
    roles[IconRole] = "deviceIcon";
    roles[ConnectedRole] = "connected";
    roles[PairedRole] = "paired";
    roles[BatteryAvailableRole] = "batteryAvailable";
    roles[BatteryPercentageRole] = "batteryPercentage";
    roles[BatterySourceRole] = "batterySource";
    roles[BatteriesRole] = "batteries";
    return roles;
}

int BluetoothDeviceModel::pairedCount() const
{
    return m_pairedCount;
}

int BluetoothDeviceModel::connectedCount() const
{
    return m_connectedCount;
}

int BluetoothDeviceModel::batteryDeviceCount() const
{
    return m_batteryDeviceCount;
}

QString BluetoothDeviceModel::coverDeviceName() const
{
    return m_coverDeviceName;
}

QString BluetoothDeviceModel::coverBatteryText() const
{
    return m_coverBatteryText;
}

QString BluetoothDeviceModel::statusText() const
{
    return m_statusText;
}

QString BluetoothDeviceModel::displayName(const QVariantMap &p)
{
    const QString alias = p.value(QStringLiteral("Alias")).toString().trimmed();
    if (!alias.isEmpty())
        return alias;

    const QString name = p.value(QStringLiteral("Name")).toString().trimmed();
    if (!name.isEmpty())
        return name;

    const QString address = p.value(QStringLiteral("Address")).toString().trimmed();
    if (!address.isEmpty())
        return address;

    return tr("Bluetooth device");
}

QString BluetoothDeviceModel::batteryLabel(const QString &identifier)
{
    const QString id = identifier.trimmed().toLower();

    if (id.isEmpty() || id == QStringLiteral("overall") || id == QStringLiteral("main"))
        return tr("Battery");
    if (id == QStringLiteral("left"))
        return tr("Left");
    if (id == QStringLiteral("right"))
        return tr("Right");
    if (id == QStringLiteral("case"))
        return tr("Case");

    return identifier;
}

QString BluetoothDeviceModel::objectPathFromVariant(const QVariant &value)
{
    if (value.canConvert<QDBusObjectPath>()) {
        const QDBusObjectPath path = qvariant_cast<QDBusObjectPath>(value);
        if (!path.path().isEmpty())
            return path.path();
    }

    return value.toString();
}

void BluetoothDeviceModel::setStatusText(const QString &text)
{
    if (m_statusText == text)
        return;

    m_statusText = text;
    emit statusTextChanged();
}

void BluetoothDeviceModel::refresh()
{
    if (!m_bus.isConnected()) {
        beginResetModel();
        m_devices.clear();
        endResetModel();
        setStatusText(tr("System D-Bus is not available"));
        updateSummary();
        return;
    }

    QDBusInterface manager(QStringLiteral("org.bluez"),
                           QStringLiteral("/"),
                           QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                           m_bus);

    const QDBusMessage reply = manager.call(QStringLiteral("GetManagedObjects"));

    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        beginResetModel();
        m_devices.clear();
        endResetModel();

        if (reply.type() == QDBusMessage::ErrorMessage)
            setStatusText(tr("BlueZ is not available: %1").arg(reply.errorMessage()));
        else
            setStatusText(tr("BlueZ returned no objects"));

        updateSummary();
        return;
    }

    const BluezManagedObjects objects =
            qdbus_cast<BluezManagedObjects>(reply.arguments().at(0));

    QList<Device> newDevices;
    QMap<QString, int> deviceIndexByPath;

    // First pass: paired/connected devices. Keeping disconnected paired devices
    // visible makes it possible to reconnect them directly from the app.
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const BluezInterfaceMap &interfaces = it.value();

        if (!interfaces.contains(QStringLiteral("org.bluez.Device1")))
            continue;

        const QVariantMap devProps =
                interfaces.value(QStringLiteral("org.bluez.Device1"));

        Device d;
        d.objectPath = it.key().path();
        d.name = displayName(devProps);
        d.address = devProps.value(QStringLiteral("Address")).toString();
        d.icon = devProps.value(QStringLiteral("Icon")).toString();
        d.connected = devProps.value(QStringLiteral("Connected")).toBool();
        d.paired = devProps.value(QStringLiteral("Paired")).toBool();

        if (!d.paired && !d.connected)
            continue;

        deviceIndexByPath.insert(d.objectPath, newDevices.count());
        newDevices.append(d);
    }

    // Second pass: collect every Battery1 object. BlueZ 5.87 normally exposes
    // Battery1 directly on Device1. Newer/experimental providers can expose
    // component Battery1 objects with Device + Identifier (left/right/case).
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const BluezInterfaceMap &interfaces = it.value();
        if (!interfaces.contains(QStringLiteral("org.bluez.Battery1")))
            continue;

        const QVariantMap batteryProps =
                interfaces.value(QStringLiteral("org.bluez.Battery1"));

        bool ok = false;
        const int percentage =
                batteryProps.value(QStringLiteral("Percentage")).toInt(&ok);
        if (!ok || percentage < 0 || percentage > 100)
            continue;

        QString devicePath;
        QString identifier = batteryProps.value(QStringLiteral("Identifier")).toString();

        if (interfaces.contains(QStringLiteral("org.bluez.Device1"))) {
            devicePath = it.key().path();
            if (identifier.isEmpty())
                identifier = QStringLiteral("overall");
        } else {
            devicePath = objectPathFromVariant(batteryProps.value(QStringLiteral("Device")));
        }

        if (!deviceIndexByPath.contains(devicePath))
            continue;

        Device &d = newDevices[deviceIndexByPath.value(devicePath)];
        const QString source = batteryProps.value(QStringLiteral("Source")).toString();

        QVariantMap b;
        b.insert(QStringLiteral("role"), identifier.isEmpty() ? QStringLiteral("component") : identifier);
        b.insert(QStringLiteral("label"), batteryLabel(identifier));
        b.insert(QStringLiteral("percentage"), percentage);
        b.insert(QStringLiteral("source"), source);

        if (batteryProps.contains(QStringLiteral("Charging")))
            b.insert(QStringLiteral("charging"), batteryProps.value(QStringLiteral("Charging")).toBool());

        d.batteries.append(b);

        const QString normalizedId = identifier.trimmed().toLower();
        if (it.key().path() == devicePath || normalizedId.isEmpty() ||
                normalizedId == QStringLiteral("overall") ||
                normalizedId == QStringLiteral("main")) {
            d.batteryPercentage = percentage;
            d.batterySource = source;
        }
    }

    // Stable ordering of battery components without inventing/combining values.
    for (Device &d : newDevices) {
        std::sort(d.batteries.begin(), d.batteries.end(),
                  [](const QVariant &a, const QVariant &b) {
            const QString ar = a.toMap().value(QStringLiteral("role")).toString().toLower();
            const QString br = b.toMap().value(QStringLiteral("role")).toString().toLower();
            const QStringList order = {
                QStringLiteral("overall"), QStringLiteral("main"),
                QStringLiteral("left"), QStringLiteral("right"),
                QStringLiteral("case")
            };
            const int ai = order.indexOf(ar);
            const int bi = order.indexOf(br);
            if (ai >= 0 || bi >= 0) {
                if (ai < 0) return false;
                if (bi < 0) return true;
                return ai < bi;
            }
            return QString::localeAwareCompare(ar, br) < 0;
        });
    }

    std::sort(newDevices.begin(), newDevices.end(),
              [](const Device &a, const Device &b) {
        if (a.connected != b.connected)
            return a.connected > b.connected;
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    beginResetModel();
    m_devices = newDevices;
    endResetModel();

    setStatusText(QString());
    updateSummary();
}

void BluetoothDeviceModel::setDeviceConnected(const QString &objectPath, bool connected)
{
    if (!m_bus.isConnected() || objectPath.isEmpty()) {
        setStatusText(tr("Bluetooth service is not available"));
        return;
    }

    QDBusInterface device(QStringLiteral("org.bluez"),
                          objectPath,
                          QStringLiteral("org.bluez.Device1"),
                          m_bus);

    if (!device.isValid()) {
        setStatusText(tr("Bluetooth device is no longer available"));
        refresh();
        return;
    }

    setStatusText(connected ? tr("Connecting…") : tr("Disconnecting…"));

    QDBusPendingCall call = device.asyncCall(connected
                                              ? QStringLiteral("Connect")
                                              : QStringLiteral("Disconnect"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this, connected](QDBusPendingCallWatcher *finished) {
        QDBusPendingReply<> reply = *finished;
        if (reply.isError()) {
            setStatusText(connected
                          ? tr("Connection failed: %1").arg(reply.error().message())
                          : tr("Disconnection failed: %1").arg(reply.error().message()));
        } else {
            setStatusText(QString());
        }

        finished->deleteLater();
        refresh();
    });
}

void BluetoothDeviceModel::toggleDeviceConnection(const QString &objectPath,
                                                    bool currentlyConnected)
{
    setDeviceConnected(objectPath, !currentlyConnected);
}

void BluetoothDeviceModel::updateSummary()
{
    int paired = 0;
    int connected = 0;
    int withBattery = 0;
    QString firstName;
    QString firstBattery;

    for (const Device &d : m_devices) {
        if (d.paired)
            ++paired;
        if (d.connected)
            ++connected;

        if (!d.batteries.isEmpty()) {
            ++withBattery;
            if (firstName.isEmpty() && d.connected) {
                firstName = d.name;
                if (d.batteries.count() == 1) {
                    firstBattery = QString::number(
                                d.batteries.first().toMap().value(QStringLiteral("percentage")).toInt())
                            + QStringLiteral(" %");
                } else {
                    firstBattery = tr("%1 batteries").arg(d.batteries.count());
                }
            }
        }
    }

    if (firstName.isEmpty()) {
        for (const Device &d : m_devices) {
            if (!d.batteries.isEmpty()) {
                firstName = d.name;
                if (d.batteries.count() == 1) {
                    firstBattery = QString::number(
                                d.batteries.first().toMap().value(QStringLiteral("percentage")).toInt())
                            + QStringLiteral(" %");
                } else {
                    firstBattery = tr("%1 batteries").arg(d.batteries.count());
                }
                break;
            }
        }
    }

    const bool changed =
            paired != m_pairedCount ||
            connected != m_connectedCount ||
            withBattery != m_batteryDeviceCount ||
            firstName != m_coverDeviceName ||
            firstBattery != m_coverBatteryText;

    m_pairedCount = paired;
    m_connectedCount = connected;
    m_batteryDeviceCount = withBattery;
    m_coverDeviceName = firstName;
    m_coverBatteryText = firstBattery;

    if (changed)
        emit summaryChanged();
}

void BluetoothDeviceModel::onBluezObjectManagerSignal(const QDBusMessage &message)
{
    Q_UNUSED(message)
    refresh();
}

void BluetoothDeviceModel::onBluezPropertiesChanged(const QDBusMessage &message)
{
    const QList<QVariant> args = message.arguments();
    if (args.isEmpty())
        return;

    const QString interfaceName = args.at(0).toString();
    if (interfaceName == QStringLiteral("org.bluez.Device1") ||
        interfaceName == QStringLiteral("org.bluez.Battery1")) {
        refresh();
    }
}

void BluetoothDeviceModel::onNameOwnerChanged(const QString &name,
                                               const QString &oldOwner,
                                               const QString &newOwner)
{
    Q_UNUSED(oldOwner)

    if (name == QStringLiteral("org.bluez")) {
        if (newOwner.isEmpty())
            setStatusText(tr("BlueZ stopped"));
        refresh();
    }
}
