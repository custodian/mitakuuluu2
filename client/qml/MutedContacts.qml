import QtQuick 2.1
import Sailfish.Silica 1.0
import harbour.mitakuuluu2.client 1.0
import "Utilities.js" as Utilities

Page {
	id: page
	objectName: "mutedContacts"
    allowedOrientations: Orientation.Portrait | (allowLandscapeInverted ? (Orientation.Landscape | Orientation.LandscapeInverted) : Orientation.Landscape)

    property int timeNow: 0

	onStatusChanged: {
		if (status == PageStatus.Active) {
			var mutedList = Mitakuuluu.loadGroup("muting")
			listModel.clear()
			for (var i = 0; i < mutedList.length; i++) {
				if (parseInt(mutedList[i].value) > new Date().getTime())
					listModel.append(mutedList[i])
			}
            updateTimer.start()
		}
		else if (status == PageStatus.Inactive) {
			updateTimer.stop()
		}
	}

    function getNicknameByJid(jid) {
        if (jid == Mitakuuluu.myJid)
            return qsTr("You", "Display You instead of your own nickname")
        var model = ContactsBaseModel.getModel(jid)
        if (model && model.nickname)
            return model.nickname
        else
            return jid.split("@")[0]
    }

    function getExpire(value, unused) {
    	return Format.formatDate(new Date(parseInt(value)), Formatter.DurationElapsed)
    }

    Timer {
    	id: updateTimer
    	interval: 60000
    	repeat: true
    	onTriggered: timeNow++
    }

	SilicaListView {
		id: listView
		anchors.fill: parent
		currentIndex: -1
        delegate: delegateComponent
        model: listModel
        header: PageHeader {
            title: qsTr("Muted contacts", "Contacts muting title text")
        }
        ViewPlaceholder {
            enabled: listView.count == 0
            text: qsTr("You have no muted contacts", "Empty muted contacts list placeholder")
        }

        VerticalScrollDecorator {}
    }

    ListModel {
    	id: listModel
    }

    Component {
        id: delegateComponent
        BackgroundItem {
            id: item

            AvatarHolder {
                id: ava
                source: ContactsBaseModel.getModel(model.jid).avatar
                emptySource: "../images/avatar-empty" + (model.jid.indexOf("-") > 0 ? "-group" : "") + ".png"
                anchors {
                	left: parent.left
                	leftMargin: Theme.paddingLarge
                	verticalCenter: parent.verticalCenter
                }
                width: Theme.iconSizeLarge
                height: Theme.iconSizeLarge
            }

            Column {
            	id: content
            	width: parent.width
                anchors {
                    left: ava.right
                    leftMargin: Theme.paddingMedium
                    right: remove.left
                    verticalCenter: parent.verticalCenter
                }
	            Label {
	            	width: parent.width
	                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: Utilities.emojify(getNicknameByJid(model.jid), emojiPath)
	                elide: Text.ElideRight
	                truncationMode: TruncationMode.Fade
	            }
	            Label {
	                width: parent.width
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: qsTr("Expiration: %1", "Contacts muting expiration text").arg(getExpire(model.value, timeNow))
	                font.pixelSize: Theme.fontSizeSmall
	            }
            }

            IconButton {
                id: remove
                width: Theme.iconSizeLarge
                height: Theme.iconSizeLarge
                anchors {
                	right: parent.right
                	rightMargin: Theme.paddingSmall
                	verticalCenter: parent.verticalCenter
                }
                icon.source: "image://theme/icon-m-clear"
                onClicked: {
                    Mitakuuluu.save("muting/" + model.jid, 0)
                    listModel.remove(index)
                }
            }

	        ListView.onAdd: AddAnimation {
	            target: item
	        }
	        ListView.onRemove: RemoveAnimation {
	            target: item
	        }
        }
    }
} 
