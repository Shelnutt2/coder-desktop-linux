import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Login page shown when the user is not authenticated.
// Calls sessionManager.login(url, token) and handles loginSucceeded / loginFailed.
Item {
    id: loginPage

    property bool loggingIn: false
    property string errorText: ""

    Connections {
        target: sessionManager
        function onLoginSucceeded(username) {
            loginPage.loggingIn = false
            loginPage.errorText = ""
        }
        function onLoginFailed(error) {
            loginPage.loggingIn = false
            loginPage.errorText = error
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 360)
        spacing: 16

        // Logo / heading
        Label {
            text: "Coder Desktop"
            font.pixelSize: 28
            font.bold: true
            color: Material.accent
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "Sign in to your Coder deployment"
            font.pixelSize: 14
            color: Material.foreground
            opacity: 0.7
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.preferredHeight: 8 }

        // Deployment URL
        Label {
            text: "Deployment URL"
            font.pixelSize: 12
            font.bold: true
        }
        TextField {
            id: urlField
            placeholderText: "https://coder.example.com"
            Layout.fillWidth: true
            enabled: !loginPage.loggingIn
            selectByMouse: true
            inputMethodHints: Qt.ImhUrlCharactersOnly
            onAccepted: tokenField.forceActiveFocus()
        }

        // Session token
        Label {
            text: "Session Token"
            font.pixelSize: 12
            font.bold: true
        }
        TextField {
            id: tokenField
            placeholderText: "Paste your session token"
            echoMode: TextInput.Password
            Layout.fillWidth: true
            enabled: !loginPage.loggingIn
            selectByMouse: true
            onAccepted: signInButton.clicked()
        }

        // Token help link
        Label {
            text: "<a href='#'>Get your token from your Coder dashboard</a>"
            font.pixelSize: 12
            color: Material.accent
            Layout.alignment: Qt.AlignRight
            onLinkActivated: {
                var base = urlField.text.replace(/\/+$/, "")
                if (base.length > 0)
                    Qt.openUrlExternally(base + "/cli-auth")
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.linkActivated("#")
            }
        }

        Item { Layout.preferredHeight: 4 }

        // Error banner
        Rectangle {
            Layout.fillWidth: true
            height: errorLabel.implicitHeight + 16
            radius: 4
            color: Material.color(Material.Red, Material.Shade50)
            visible: loginPage.errorText.length > 0

            Label {
                id: errorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: loginPage.errorText
                color: Material.color(Material.Red)
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        // Sign In button
        Button {
            id: signInButton
            text: loginPage.loggingIn ? "Signing in…" : "Sign In"
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            enabled: !loginPage.loggingIn
                     && urlField.text.trim().length > 0
                     && tokenField.text.trim().length > 0
            highlighted: true
            Material.background: Material.accent

            onClicked: {
                loginPage.errorText = ""
                loginPage.loggingIn = true
                sessionManager.login(urlField.text.trim(), tokenField.text.trim())
            }
        }

        // Loading spinner
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: loginPage.loggingIn
            visible: loginPage.loggingIn
        }
    }
}
