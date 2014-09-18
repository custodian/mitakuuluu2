import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.mitakuuluu2.client 1.0
import org.nemomobile.configuration 1.0

Dialog {
    id: page
    objectName: "account"
    allowedOrientations: globalOrientation

    property string pushname: ""
    property string presence: ""
    property int creation: 0
    property int expiration: 0
    property bool active: true
    property string kind: "free"
    property string avatar: Mitakuuluu.getAvatarForJid(Mitakuuluu.myJid)

    canAccept: (Mitakuuluu.connectionStatus == Mitakuuluu.LoggedIn)
               && (pushnameArea.text.length > 0)
               && (presenceArea.text.length > 0)

    function accept() {
        if (canAccept) {
            _dialogDone(DialogResult.Accepted)
        }
        else {
            negativeFeedback()
        }

        // Attempt to navigate even if it will fail, so that feedback can be generated
        pageStack.navigateForward()
    }

    function resetAccount() {
        Mitakuuluu.clearGroup("account")
        if (Mitakuuluu.connectionStatus == Mitakuuluu.LoggedIn) {
            Mitakuuluu.disconnect()
        }
        pageStack.pop()
    }

    property bool cantAcceptReally: pageStack._forwardFlickDifference > 0 && pageStack._preventForwardNavigation
    onCantAcceptReallyChanged: {
        if (cantAcceptReally)
            negativeFeedback()
    }

    function negativeFeedback() {
        if (Mitakuuluu.connectionStatus != Mitakuuluu.LoggedIn) {
            banner.notify(qsTr("You should be online!", "Account page cant accept feedback"))
        }
        if (pushnameArea.text.trim().length == 0) {
            pushnameArea.forceActiveFocus()
        }
        if (presenceArea.text.trim().length == 0) {
            presenceArea.forceActiveFocus()
        }
    }

    function tr_noop() {
        qsTr("free", "Account type")
        qsTr("paid", "Account type")
        qsTr("blocked", "Account type")
    }

    Connections {
        target: Mitakuuluu

        onPictureUpdated: {
            if (pjid === Mitakuuluu.myJid) {
                avatarSet(path)
            }
        }
    }

    function avatarSet(avatarPath) {
        console.log("updated avatar: " + avatarPath)
        page.avatar = ""
        page.avatar = avatarPath
    }

    onStatusChanged: {
        if (status == DialogStatus.Opened) {
            pushname = accountConfiguration.pushname
            pushnameArea.text = pushname
            presence = accountConfiguration.presence
            presenceArea.text = presence
            creation = accountConfiguration.creation
            expiration = accountConfiguration.expiration
            kind = accountConfiguration.kind
            active = accountConfiguration.accountstatus
        }
    }

    onAccepted: {
        page.pushname = pushnameArea.text
        accountConfiguration.pushname = pushnameArea.text.trim();
        Mitakuuluu.setMyPushname(pushnameArea.text)
        ContactsBaseModel.renameContact(Mitakuuluu.myJid, pushnameArea.text.trim())
        pushnameArea.focus = false
        page.forceActiveFocus()

        page.presence = presenceArea.text
        accountConfiguration.presence = presenceArea.text.trim();
        Mitakuuluu.setMyPresence(presenceArea.text)
        presenceArea.focus = false
        page.forceActiveFocus()
    }

    ConfigurationGroup {
        id: accountConfiguration
        path: "/apps/harbour-mitakuuluu2/account"

        property string pushname: "Mitakuuluu user"
        property string presence: "I love Mitakuuluu!"

        property int creation: 0
        property int expiration: 0
        property string kind: "free"

        property string accountstatus: "active"
    }

    function timestampToFullDate(stamp) {
        var d = new Date(stamp*1000)
        return Qt.formatDateTime(d, "dd MMM yyyy")
    }

    SilicaFlickable {
        anchors.fill: page
        clip: true

        PullDownMenu {
            MenuItem {
                text: Mitakuuluu.connectionStatus == Mitakuuluu.LoggedIn ? qsTr("Remove account", "Account page menu item") :
                                                                           qsTr("Remove local data", "Account page menu item")
                onClicked: {
                    if (Mitakuuluu.connectionStatus == Mitakuuluu.LoggedIn) {
                        deleteDialog.open()
                    }
                    else {
                        Mitakuuluu.clearGroup("account")
                    }
                }
            }
            MenuItem {
                text: qsTr("Renew subscription", "Account page menu item")
                //visible: ((page.expiration * 1000) - (new Date().getTime())) < 259200000
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("Payments.qml"))
                }
            }
            MenuItem {
                text: qsTr("Privacy settings", "Account page menu item")
                visible: Mitakuuluu.connectionStatus == Mitakuuluu.LoggedIn
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("PrivacySettings.qml"))
                }
            }
        }

        DialogHeader {
            id: header
            title: qsTr("Account", "Account page title")
            acceptText: qsTr("Save", "Account page accept button text")
        }

        Label {
            id: pushnameLabel
            text: qsTr("Nickname:", "Account page nickname title")
            anchors.left: parent.left
            anchors.leftMargin: Theme.paddingMedium
            anchors.top: pushnameArea.top
            anchors.topMargin: Theme.paddingSmall
        }

        TextField {
            id: pushnameArea
            anchors.top: header.bottom
            anchors.left: pushnameLabel.right
            anchors.right: parent.right
            text: page.pushname
            errorHighlight: text.length == 0
            EnterKey.enabled: false
            EnterKey.highlighted: EnterKey.enabled
            onActiveFocusChanged: {
                if (activeFocus)
                    selectAll()
            }
        }

        Label {
            id: presenceLabel
            text: qsTr("Status:", "Account page status title")
            anchors.left: parent.left
            anchors.leftMargin: Theme.paddingMedium
            anchors.top: presenceArea.top
            anchors.topMargin: Theme.paddingSmall
        }

        TextField {
            id: presenceArea
            anchors.top: pushnameArea.bottom
            anchors.left: presenceLabel.right
            anchors.right: parent.right
            text: page.presence
            errorHighlight: text.length == 0
            EnterKey.enabled: false
            EnterKey.highlighted: EnterKey.enabled
            onActiveFocusChanged: {
                if (activeFocus)
                    selectAll()
            }
        }

        Label {
            id: labelCreated
            text: qsTr("Created: %1", "Account page created title").arg(timestampToFullDate(page.creation))
            anchors.top: presenceArea.bottom
            anchors.topMargin: Theme.paddingLarge
            anchors.left: ava.right
            anchors.leftMargin: Theme.paddingSmall
            anchors.right: parent.right
            anchors.rightMargin: Theme.paddingMedium
            wrapMode: Text.NoWrap
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Theme.fontSizeSmall
        }

        Label {
            id: labelExpired
            text: qsTr("Expiration: %1", "Account page expiration title").arg(timestampToFullDate(page.expiration))
            anchors.top: labelCreated.bottom
            anchors.topMargin: Theme.paddingSmall
            anchors.left: ava.right
            anchors.leftMargin: Theme.paddingSmall
            anchors.right: parent.right
            anchors.rightMargin: Theme.paddingMedium
            wrapMode: Text.NoWrap
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Theme.fontSizeSmall
        }

        Label {
            id: labelActive
            text: page.active ? qsTr("Account is active", "Account page account active label")
                              : qsTr("Account is blocked", "Account page account blocked label")
            anchors.top: labelExpired.bottom
            anchors.topMargin: Theme.paddingSmall
            anchors.left: ava.right
            anchors.leftMargin: Theme.paddingSmall
            anchors.right: parent.right
            anchors.rightMargin: Theme.paddingMedium
            wrapMode: Text.NoWrap
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Theme.fontSizeSmall
        }

        Label {
            id: labelType
            text: qsTr("Account type: %1", "Account page account type label").arg(qsTr(page.kind))
            anchors.top: labelActive.bottom
            anchors.topMargin: Theme.paddingSmall
            anchors.left: ava.right
            anchors.leftMargin: Theme.paddingSmall
            anchors.right: parent.right
            anchors.rightMargin: Theme.paddingMedium
            wrapMode: Text.NoWrap
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Theme.fontSizeSmall
        }

        AvatarHolder {
            id: ava
            anchors.top: presenceArea.bottom
            anchors.topMargin: Theme.paddingLarge
            anchors.left: parent.left
            anchors.leftMargin: Theme.paddingMedium
            width: 128
            height: 128
            source: page.avatar
            emptySource: "../images/avatar-empty.png"

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var avatarHistory = pageStack.push(Qt.resolvedUrl("AvatarHistory.qml"), {"jid": Mitakuuluu.myJid, "avatar": page.avatar, "owner": true})
                    avatarHistory.avatarSet.connect(page.avatarSet)
                }
            }
        }
    }

    RemorsePopup {
        id: remorseAccount
    }

    Dialog {
        id: deleteDialog
        SilicaFlickable {
            anchors.fill: parent
            DialogHeader {
                id: dheader
                title: qsTr("Remove account", "Account page remove dialog title")
            }
            Column {
                width: parent.width - (Theme.paddingLarge * 2)
                anchors.centerIn: parent
                spacing: Theme.paddingLarge
                Label {
                    width: parent.width
                    text: qsTr("This action will delete your account information from phone and from WhatsApp server.", "Account page remove dialog description")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("No, remove only local information")
                    onClicked: {
                        deleteDialog.reject()
                        resetAccount()
                    }
                }
            }
        }
        onAccepted: {
            //pageStack.pop(roster, PageStackAction.Immediate)
            //pageStack.replace(removePage)
            Mitakuuluu.removeAccountFromServer()
            resetAccount()
        }
    }
}
