import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Login page with two-step flow:
//   Step 1 ("url"):      Enter deployment URL → "Sign In" opens embedded browser
//   Step 2 ("browser"):  Embedded WebEngineView loads the login page; cookie
//                         capture auto-completes login
//   Fallback ("token"):  Manual token entry (when WebEngine unavailable or
//                         user clicks "Use token instead")
Item {
    id: loginPage

    property string currentStep: "url"   // "url" | "browser" | "token"
    property bool loggingIn: false
    property string errorText: ""

    Connections {
        target: sessionManager
        function onLoginSucceeded(username) {
            loginPage.loggingIn = false
            loginPage.errorText = ""
            loginPage.currentStep = "url"
        }
        function onLoginFailed(error) {
            loginPage.loggingIn = false
            loginPage.errorText = error
            // If browser flow failed, drop back to token entry so the user
            // can see the error and try manually.
            if (loginPage.currentStep === "browser")
                loginPage.currentStep = "token"
        }
    }

    Connections {
        target: loginFlowController
        function onTokenObtained(deploymentUrl, token) {
            // Token captured from the embedded browser cookie — login is
            // already in progress via SessionManager.  Show a spinner.
            loginPage.loggingIn = true
        }
        function onExternalBrowserOpened(cliAuthUrl) {
            // WebEngine unavailable — switched to manual token entry.
            loginPage.currentStep = "token"
        }
    }

    // =======================================================================
    // Step 1: Deployment URL entry
    // =======================================================================
    ColumnLayout {
        id: urlStep
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 400)
        spacing: 16
        visible: loginPage.currentStep === "url"

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

        Label {
            text: "Deployment URL"
            font.pixelSize: 12
            font.bold: true
        }
        TextField {
            id: urlField
            placeholderText: "https://coder.example.com"
            Layout.fillWidth: true
            selectByMouse: true
            inputMethodHints: Qt.ImhUrlCharactersOnly
            onAccepted: signInButton.clicked()
        }

        // Error banner (shared across steps, shown here for URL-level errors)
        Rectangle {
            id: errorBanner
            Layout.fillWidth: true
            height: errorLabel.implicitHeight + 16
            radius: 4
            color: Material.color(Material.Red, Material.Shade50)
            visible: loginPage.errorText.length > 0
                     && loginPage.currentStep !== "browser"

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

        Button {
            id: signInButton
            text: "Sign In"
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            enabled: urlField.text.trim().length > 0
            highlighted: true
            Material.background: Material.accent

            onClicked: {
                loginPage.errorText = ""
                loginFlowController.startFlow(urlField.text.trim())
                if (loginFlowController.webEngineAvailable) {
                    loginPage.currentStep = "browser"
                }
                // If WebEngine unavailable, the externalBrowserOpened signal
                // switches to "token" step automatically.
            }
        }

        Label {
            text: "<a href='#'>Use session token instead</a>"
            font.pixelSize: 12
            color: Material.accent
            Layout.alignment: Qt.AlignHCenter
            onLinkActivated: loginPage.currentStep = "token"
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.linkActivated("#")
            }
        }
    }

    // =======================================================================
    // Step 2: Embedded browser (WebEngine)
    // =======================================================================
    ColumnLayout {
        id: browserStep
        anchors.fill: parent
        spacing: 0
        visible: loginPage.currentStep === "browser"

        // Toolbar
        ToolBar {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                spacing: 4

                ToolButton {
                    text: "✕"
                    font.pixelSize: 16
                    onClicked: {
                        loginFlowController.cancelFlow()
                        loginPage.currentStep = "url"
                    }
                    ToolTip.text: "Cancel sign-in"
                    ToolTip.visible: hovered
                }

                Label {
                    text: "Sign in to " + urlField.text.trim()
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                BusyIndicator {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    running: webLoader.item ? webLoader.item.loading : false
                    visible: running
                }
            }
        }

        // WebEngine content (dynamically loaded — same pattern as AppBrowserPage)
        Loader {
            id: webLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: webEngineComponent
            active: loginPage.currentStep === "browser"
        }

        Component {
            id: webEngineComponent

            Item {
                id: webEnginePlaceholder
                property bool loading: false
                property url url: ""

                Component.onCompleted: {
                    if (!loginFlowController.webEngineAvailable) {
                        browserFallbackLabel.visible = true
                        return
                    }

                    try {
                        var loginUrl = loginFlowController.loginUrl
                        var src =
                            'import QtQuick; ' +
                            'import QtWebEngine; ' +
                            'WebEngineView { ' +
                            '    anchors.fill: parent; ' +
                            '    url: "' + loginUrl + '"; ' +
                            '    onLoadingChanged: function(loadReq) { ' +
                            '        parent.loading = loading; ' +
                            '        parent.url = url; ' +
                            '    } ' +
                            '}';
                        var obj = Qt.createQmlObject(src, webEnginePlaceholder,
                                                      "LoginWebEngine");
                        if (obj) {
                            webEnginePlaceholder.loading =
                                Qt.binding(function() { return obj.loading; });
                            webEnginePlaceholder.url =
                                Qt.binding(function() { return obj.url; });
                            browserFallbackLabel.visible = false;
                        }
                    } catch (e) {
                        console.warn("LoginPage: QtWebEngine not available:", e);
                        browserFallbackLabel.visible = true;
                        // Fall back to manual token entry
                        loginFlowController.openExternalCliAuth(urlField.text.trim())
                        loginPage.currentStep = "token"
                    }
                }
            }
        }

        Label {
            id: browserFallbackLabel
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 32
            text: "Loading browser…"
            visible: false
        }
    }

    // =======================================================================
    // Fallback: Manual token entry
    // =======================================================================
    ColumnLayout {
        id: tokenStep
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 400)
        spacing: 16
        visible: loginPage.currentStep === "token"

        Label {
            text: "Coder Desktop"
            font.pixelSize: 28
            font.bold: true
            color: Material.accent
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "Paste your session token"
            font.pixelSize: 14
            color: Material.foreground
            opacity: 0.7
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.preferredHeight: 4 }

        Label {
            text: "Deployment URL"
            font.pixelSize: 12
            font.bold: true
        }
        TextField {
            id: tokenUrlField
            text: urlField.text
            placeholderText: "https://coder.example.com"
            Layout.fillWidth: true
            enabled: !loginPage.loggingIn
            selectByMouse: true
            inputMethodHints: Qt.ImhUrlCharactersOnly
            onAccepted: tokenField.forceActiveFocus()
        }

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
            onAccepted: tokenSignInButton.clicked()
        }

        Label {
            text: "<a href='#'>Get your token from your Coder dashboard</a>"
            font.pixelSize: 12
            color: Material.accent
            Layout.alignment: Qt.AlignRight
            onLinkActivated: {
                loginFlowController.openExternalCliAuth(tokenUrlField.text.trim())
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.linkActivated("#")
            }
        }

        // Error banner
        Rectangle {
            Layout.fillWidth: true
            height: tokenErrorLabel.implicitHeight + 16
            radius: 4
            color: Material.color(Material.Red, Material.Shade50)
            visible: loginPage.errorText.length > 0
                     && loginPage.currentStep === "token"

            Label {
                id: tokenErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: loginPage.errorText
                color: Material.color(Material.Red)
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        Button {
            id: tokenSignInButton
            text: loginPage.loggingIn ? "Signing in…" : "Sign In"
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            enabled: !loginPage.loggingIn
                     && tokenUrlField.text.trim().length > 0
                     && tokenField.text.trim().length > 0
            highlighted: true
            Material.background: Material.accent

            onClicked: {
                loginPage.errorText = ""
                loginPage.loggingIn = true
                sessionManager.login(tokenUrlField.text.trim(),
                                     tokenField.text.trim())
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: loginPage.loggingIn
            visible: loginPage.loggingIn
        }

        Label {
            text: "<a href='#'>← Back</a>"
            font.pixelSize: 12
            color: Material.accent
            Layout.alignment: Qt.AlignHCenter
            onLinkActivated: {
                loginPage.errorText = ""
                loginPage.currentStep = "url"
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.linkActivated("#")
            }
        }
    }
}
