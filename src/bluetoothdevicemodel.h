#ifndef BLUETOOTHDEVICEMODEL_H
#define BLUETOOTHDEVICEMODEL_H

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QVariantList>

class QDBusMessage;

class BluetoothDeviceModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int pairedCount READ pairedCount NOTIFY summaryChanged)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY summaryChanged)
    Q_PROPERTY(int batteryDeviceCount READ batteryDeviceCount NOTIFY summaryChanged)
    Q_PROPERTY(QString coverDeviceName READ coverDeviceName NOTIFY summaryChanged)
    Q_PROPERTY(QString coverBatteryText READ coverBatteryText NOTIFY summaryChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    enum Roles {
        ObjectPathRole = Qt::UserRole + 1,
        NameRole,
        AddressRole,
        IconRole,
        ConnectedRole,
        PairedRole,
        BatteryAvailableRole,
        BatteryPercentageRole,
        BatterySourceRole,
        BatteriesRole
    };

    explicit BluetoothDeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int pairedCount() const;
    int connectedCount() const;
    int batteryDeviceCount() const;
    QString coverDeviceName() const;
    QString coverBatteryText() const;
    QString statusText() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setDeviceConnected(const QString &objectPath, bool connected);
    Q_INVOKABLE void toggleDeviceConnection(const QString &objectPath, bool currentlyConnected);

signals:
    void summaryChanged();
    void statusTextChanged();

private slots:
    void onBluezObjectManagerSignal(const QDBusMessage &message);
    void onBluezPropertiesChanged(const QDBusMessage &message);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                            const QString &newOwner);

private:
    struct Device {
        QString objectPath;
        QString name;
        QString address;
        QString icon;
        bool connected = false;
        bool paired = false;
        int batteryPercentage = -1; // Overall battery only, never an estimate.
        QString batterySource;
        QVariantList batteries;
    };

    void setStatusText(const QString &text);
    void updateSummary();
    static QString displayName(const QVariantMap &deviceProperties);
    static QString batteryLabel(const QString &identifier);
    static QString objectPathFromVariant(const QVariant &value);

    QDBusConnection m_bus;
    QList<Device> m_devices;
    QString m_statusText;
    int m_pairedCount = 0;
    int m_connectedCount = 0;
    int m_batteryDeviceCount = 0;
    QString m_coverDeviceName;
    QString m_coverBatteryText;
};

#endif // BLUETOOTHDEVICEMODEL_H
