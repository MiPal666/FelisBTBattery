import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.paddingSmall

        Label {
            width: parent.width - 2 * Theme.paddingMedium
            x: Theme.paddingMedium
            horizontalAlignment: Text.AlignHCenter
            text: btModel.coverDeviceName.length > 0
                  ? btModel.coverDeviceName
                  : "Felis BT Battery Audio"
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeMedium
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: btModel.coverBatteryText.length > 0
                  ? btModel.coverBatteryText
                  : "—"
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeHuge
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("%1 connected").arg(btModel.connectedCount)
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }
}
