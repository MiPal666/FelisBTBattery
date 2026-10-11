import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("NOTICE!")
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("NOTICE!")
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeHuge
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: qsTr("Primary test scope: Bluetooth audio devices on Jolla Phone 2026 / Sailfish OS with BlueZ 5.87.")
                color: Theme.primaryColor
            }

            SectionHeader {
                text: qsTr("Multiple batteries")
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: qsTr("The GUI can display multiple BlueZ Battery1 components (for example left, right and case), but this has not been verified on physical hardware. On the tested BlueZ 5.87 stack, TWS audio devices normally expose only a single HFP/aggregate battery value.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            SectionHeader {
                text: qsTr("Non-audio devices")
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: qsTr("Suitable non-audio hardware reporting a battery through BlueZ was not available for testing. Devices exposing the standard org.bluez.Battery1 interface are intended to be displayed, but this is currently unverified on real hardware.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            SectionHeader {
                text: qsTr("No vendor-specific estimates")
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: qsTr("Felis BT Battery Audio does not use vendor-specific battery protocols and never estimates charge. It shows only values actually reported by the Bluetooth device and system stack.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: qsTr("If you can test separate left/right/case batteries or a non-audio Bluetooth device exposing Battery1, please report your results on the project GitHub page.")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        VerticalScrollDecorator {}
    }
}
