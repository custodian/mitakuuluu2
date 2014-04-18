import QtQuick 2.1
import Sailfish.Silica 1.0
import harbour.mitakuuluu2.client 1.0

Dialog {
    id: renameContact
    objectName: "renameContact"
    onStatusChanged: {
        if (status == DialogStatus.Opened) {
            nameField.text = ContactsBaseModel.getModel(jid).nickname
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
    }

    property string jid
    function showData(ojid, oname) {
    }
    canAccept: nameField.text.trim().length > 0

    DialogHeader {
        title: qsTr("Rename contact", "Rename contact page title")
    }

    TextField {
        id: nameField
        anchors.centerIn: parent
        width: parent.width - (Theme.paddingLarge * 2)
        placeholderText: qsTr("Enter new name", "Registration information constructor")
        EnterKey.enabled: text.length > 0
        EnterKey.highlighted: text.length > 0
        EnterKey.iconSource: "image://theme/icon-m-enter-next"
        EnterKey.onClicked: renameContact.accept()
    }

    onAccepted: {
        nameField.deselect()
        ContactsBaseModel.renameContact(renameContact.jid, nameField.text.trim())
    }
} 
