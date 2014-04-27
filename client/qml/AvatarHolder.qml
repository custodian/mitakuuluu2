import QtQuick 2.1
import Sailfish.Silica 1.0

Image {
    id: root
    width: 64
    sourceSize.width: width
    height: 64
    smooth: true
    fillMode: Image.PreserveAspectCrop
    asynchronous: true
    cache: false
    clip: true

    property alias color: dummy.color
    property alias emptySource: empty.source

    Rectangle {
        id: dummy
        anchors.fill: root
        color: Theme.rgba(Theme.highlightColor, Theme.highlightBackgroundOpacity)
        visible: root.status !== Image.Ready
    }

    Image {
        id: empty
        anchors.centerIn: parent
        width: 64
        sourceSize.width: width
        height: 64
        smooth: true
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        clip: true
        visible: root.status !== Image.Ready
    }

    BusyIndicator {
        id: busy
        anchors.centerIn: root
        size: BusyIndicatorSize.Large
        running: visible
        visible: root.status == Image.Loading
    }
}
