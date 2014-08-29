import QtQuick 2.1
import Sailfish.Silica 1.0
import harbour.mitakuuluu2.client 1.0

Dialog {
    id: root
    objectName: "newVersionDialog"
    allowedOrientations: Orientation.Portrait | (allowLandscapeInverted ? (Orientation.Landscape | Orientation.LandscapeInverted) : Orientation.Landscape)

    onAccepted: {
        Qt.openUrlExternally("https://openrepos.net/content/coderus/mitakuuluu")
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.height

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: "Install"
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("New Mitakuuluu version available!")
                wrapMode: Text.Wrap
            }

            Label {
                anchors {
                    left: parent.left
                    right: parent.right
                    margins: Theme.paddingLarge
                }
                text: qsTr("Version: v%1").arg(Mitakuuluu.webVersion.version)
                wrapMode: Text.Wrap
            }

            Label {
                anchors {
                    left: parent.left
                    right: parent.right
                    margins: Theme.paddingLarge
                }
                text: qsTr("Release notes: \n%1").arg(Mitakuuluu.webVersion.comment)
                wrapMode: Text.Wrap
            }
        }
    }
}
