import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Bluetooth 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: btModel

        PullDownMenu {
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("NOTICE!")
                onClicked: pageStack.push(Qt.resolvedUrl("NoticePage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: btModel.refresh()
            }
        }

        header: Column {
            width: listView.width

            PageHeader {
                title: "Felis BT Battery Audio"
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                visible: btModel.statusText.length > 0
                text: btModel.statusText
                color: Theme.errorColor
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
            }

            Item { width: 1; height: Theme.paddingSmall }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                text: qsTr("%1 paired · %2 connected · %3 reporting battery")
                      .arg(btModel.pairedCount)
                      .arg(btModel.connectedCount)
                      .arg(btModel.batteryDeviceCount)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Item { width: 1; height: Theme.paddingMedium }
        }

        delegate: BackgroundItem {
            id: delegateItem
            width: listView.width
            height: contentColumn.height + 2 * Theme.paddingMedium

            BluetoothDeviceInfo {
                id: deviceInfo
                address: model.address
                deviceClass: model.deviceClass
            }

            onClicked: btModel.toggleDeviceConnection(objectPath, connected)

            Column {
                id: contentColumn
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                y: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Image {
                        id: deviceTypeIcon
                        width: Theme.iconSizeMedium
                        height: Theme.iconSizeMedium
                        anchors.verticalCenter: parent.verticalCenter
                        source: "image://theme/" + (deviceInfo.icon || "icon-m-bluetooth")
                    }

                    Column {
                        width: parent.width - deviceTypeIcon.width - stateLabel.width - 2 * Theme.paddingMedium

                        Label {
                            width: parent.width
                            text: deviceName
                            color: connected ? Theme.primaryColor : Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeMedium
                            truncationMode: TruncationMode.Fade
                        }

                        Label {
                            width: parent.width
                            text: address
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            truncationMode: TruncationMode.Fade
                        }
                    }

                    Label {
                        id: stateLabel
                        text: connected ? qsTr("Connected") : qsTr("Disconnected")
                        color: connected ? Theme.highlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }

                Column {
                    width: parent.width
                    visible: batteries.length > 0
                    spacing: Theme.paddingSmall

                    Repeater {
                        model: batteries

                        Column {
                            width: contentColumn.width
                            spacing: Theme.paddingSmall

                            Row {
                                width: parent.width
                                spacing: Theme.paddingMedium

                                Label {
                                    width: parent.width - batteryValue.width - Theme.paddingMedium
                                    text: modelData.label || qsTr("Battery")
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeSmall
                                }

                                Label {
                                    id: batteryValue
                                    text: modelData.percentage + " %"
                                    color: Theme.highlightColor
                                    font.pixelSize: Theme.fontSizeLarge
                                }
                            }

                            Label {
                                width: parent.width
                                visible: modelData.source && modelData.source.length > 0
                                text: qsTr("Source: %1").arg(modelData.source || "")
                                color: Theme.secondaryColor
                                font.pixelSize: Theme.fontSizeExtraSmall
                                truncationMode: TruncationMode.Fade
                            }
                        }
                    }
                }

                Label {
                    width: parent.width
                    visible: connected && !batteryAvailable
                    text: qsTr("Battery is not reported by BlueZ")
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                }

                Label {
                    width: parent.width
                    text: connected ? qsTr("Tap to disconnect") : qsTr("Tap to connect")
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && btModel.statusText.length === 0
            text: qsTr("No paired Bluetooth devices")
            hintText: qsTr("Pair a Bluetooth device in system settings and refresh.")
        }

        VerticalScrollDecorator {}
    }
}
