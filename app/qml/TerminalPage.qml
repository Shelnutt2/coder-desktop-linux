import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop 1.0

Page {
    id: root

    /// UUID of the workspace agent to connect to.
    property string agentId: ""

    /// Human-readable agent name (shown in the toolbar title).
    property string agentName: ""

    /// Workspace name (for display purposes).
    property string workspaceName: ""

    /// Emitted when the user wants to close the terminal overlay.
    signal closeRequested()

    title: qsTr("Terminal — %1").arg(agentName || "Agent")

    // ---- C++ bridge (QML-instantiated, exposed to JS via WebChannel) ----
    TerminalBridge {
        id: bridge
    }

    Component.onCompleted: {
        bridge.setCredentials(sessionManager.currentUrl,
                              sessionManager.sessionToken());
        bridge.setAgentId(root.agentId);
    }

    onAgentIdChanged: {
        if (root.agentId.length > 0) {
            bridge.setAgentId(root.agentId);
        }
    }

    Component.onDestruction: {
        bridge.disconnectFromPty();
    }

    // -- Navigation bar --
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton {
                icon.name: "go-previous"
                text: qsTr("Back to workspace")
                display: AbstractButton.TextBesideIcon
                onClicked: root.closeRequested()
                ToolTip.text: text
                ToolTip.visible: hovered
            }

            ToolSeparator {}

            Label {
                Layout.fillWidth: true
                text: root.title
                elide: Text.ElideRight
                font.bold: true
            }
        }
    }

    // -- WebEngine + WebChannel content area --
    // Uses the same dynamic-creation pattern as AppBrowserPage.qml to
    // degrade gracefully when QtWebEngine is not installed.
    Loader {
        id: webLoader
        anchors.fill: parent
        visible: status === Loader.Ready
        active: bridge.webSocketsAvailable
        sourceComponent: terminalComponent
    }

    Component {
        id: terminalComponent

        Item {
            id: terminalPlaceholder

            Component.onCompleted: {
                try {
                    // 1. Create a WebChannel and register the bridge object.
                    var chSrc =
                        'import QtWebChannel; ' +
                        'WebChannel { }';
                    var channelObj = Qt.createQmlObject(chSrc,
                        terminalPlaceholder, "DynTerminalChannel");
                    channelObj.registerObject("terminalBridge", bridge);

                    // 2. Create a WebEngineView that uses the channel and
                    //    loads the local xterm.js HTML page.
                    var weSrc =
                        'import QtQuick; ' +
                        'import QtWebEngine; ' +
                        'WebEngineView { ' +
                        '    anchors.fill: parent; ' +
                        '    url: "qrc:/CoderDesktop/assets/terminal/terminal.html"; ' +
                        '    settings.javascriptEnabled: true; ' +
                        '    settings.localContentCanAccessRemoteUrls: true; ' +
                        '    onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) { ' +
                        '        var prefix = "[terminal.js:" + lineNumber + "] "; ' +
                        '        if (level === WebEngineView.InfoMessageLevel) ' +
                        '            console.log(prefix + message); ' +
                        '        else if (level === WebEngineView.WarningMessageLevel) ' +
                        '            console.warn(prefix + message); ' +
                        '        else ' +
                        '            console.error(prefix + message); ' +
                        '    } ' +
                        '    onLoadingChanged: function(loadReq) { ' +
                        '        if (loadReq.errorString) ' +
                        '            console.error("Terminal load error: " + loadReq.errorString); ' +
                        '    } ' +
                        '}';
                    var webView = Qt.createQmlObject(weSrc,
                        terminalPlaceholder, "DynTerminalWebEngine");
                    webView.webChannel = channelObj;

                    webEngineFallback.visible = false;
                } catch (e) {
                    console.warn("TerminalPage: QtWebEngine/WebChannel not available:", e);
                    webEngineFallback.visible = true;
                }
            }
        }
    }

    // -- Fallback: WebSocket support not compiled --
    ColumnLayout {
        id: webSocketFallback
        anchors.centerIn: parent
        spacing: 16
        visible: !bridge.webSocketsAvailable

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Terminal unavailable")
            font.pointSize: 16
            font.bold: true
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 400
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("WebSocket support is not available. The Qt WebSockets " +
                        "module must be installed to use the in-app terminal.\n\n" +
                        "Install the package (e.g. 'qt6-websockets' or " +
                        "'libqt6websockets6') and restart the application.")
            opacity: 0.7
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Close")
            onClicked: root.closeRequested()
        }
    }

    // -- Fallback: WebEngine not available (WebSockets OK) --
    ColumnLayout {
        id: webEngineFallback
        anchors.centerIn: parent
        spacing: 16
        visible: false  // shown by catch block if WebEngine creation fails

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Terminal unavailable")
            font.pointSize: 16
            font.bold: true
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 400
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Qt WebEngine and Qt WebChannel are required for the " +
                        "embedded terminal. Please install these Qt modules " +
                        "and restart the application.")
            opacity: 0.7
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Close")
            onClicked: root.closeRequested()
        }
    }
}
