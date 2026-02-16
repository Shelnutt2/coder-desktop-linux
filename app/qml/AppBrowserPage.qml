import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: root

    /// Deployment base URL (e.g. "https://coder.example.com").
    property string deploymentUrl: ""

    /// UUID of the workspace agent hosting the app.
    property string agentId: ""

    /// Application slug identifier.
    property string appSlug: ""

    /// Workspace name (used in VPN-mode hostname).
    property string workspaceName: ""

    /// Agent name within the workspace (used in VPN-mode hostname).
    property string agentName: ""

    /// Whether the VPN tunnel is currently active.
    property bool vpnActive: false

    /// Session token for cookie-based authentication.
    property string sessionToken: ""

    /// The resolved app URL (computed when properties change).
    readonly property string appUrl: {
        if (!appSlug || (!vpnActive && !agentId)) {
            return "";
        }
        return appBrowser.buildAppUrl(deploymentUrl, agentId, appSlug,
                                       workspaceName, agentName, vpnActive);
    }

    title: qsTr("App Browser")

    // -- Navigation bar --
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton {
                id: backButton
                icon.name: "go-previous"
                text: qsTr("Back")
                display: AbstractButton.IconOnly
                enabled: webLoader.item ? webLoader.item.canGoBack : false
                onClicked: {
                    if (webLoader.item) {
                        webLoader.item.goBack();
                    }
                }
                ToolTip.text: text
                ToolTip.visible: hovered
            }

            ToolButton {
                id: forwardButton
                icon.name: "go-next"
                text: qsTr("Forward")
                display: AbstractButton.IconOnly
                enabled: webLoader.item ? webLoader.item.canGoForward : false
                onClicked: {
                    if (webLoader.item) {
                        webLoader.item.goForward();
                    }
                }
                ToolTip.text: text
                ToolTip.visible: hovered
            }

            ToolButton {
                id: reloadButton
                icon.name: webLoader.item && webLoader.item.loading
                           ? "process-stop" : "view-refresh"
                text: webLoader.item && webLoader.item.loading
                      ? qsTr("Stop") : qsTr("Reload")
                display: AbstractButton.IconOnly
                onClicked: {
                    if (webLoader.item) {
                        if (webLoader.item.loading) {
                            webLoader.item.stop();
                        } else {
                            webLoader.item.reload();
                        }
                    }
                }
                ToolTip.text: text
                ToolTip.visible: hovered
            }

            TextField {
                id: urlField
                Layout.fillWidth: true
                readOnly: true
                text: webLoader.item ? webLoader.item.url : root.appUrl
                font.pointSize: 9
                verticalAlignment: TextInput.AlignVCenter
            }
        }
    }

    // -- WebEngine content area --
    // Use a Loader so the page degrades gracefully when QtWebEngine is
    // unavailable.  The Loader source is set at Component.onCompleted
    // using Qt.createComponent() to avoid a hard import-time failure.
    Loader {
        id: webLoader
        anchors.fill: parent
        visible: status === Loader.Ready
        sourceComponent: webEngineComponent
    }

    // Attempt to define a WebEngineView component.  If QtWebEngine is not
    // installed, Qt.createQmlObject will throw and we fall back to the
    // placeholder.
    Component {
        id: webEngineComponent

        // Placeholder item – replaced at runtime if WebEngine is available.
        Item {
            id: webEnginePlaceholder

            // Expose a duck-typed interface so the nav-bar bindings don't
            // produce errors even when using the fallback.
            property bool canGoBack: false
            property bool canGoForward: false
            property bool loading: false
            property url url: root.appUrl

            function goBack() {}
            function goForward() {}
            function stop() {}
            function reload() {}

            Component.onCompleted: {
                // Try to instantiate a real WebEngineView.
                try {
                    var src =
                        'import QtQuick; ' +
                        'import QtWebEngine; ' +
                        'WebEngineView { ' +
                        '    anchors.fill: parent; ' +
                        '    url: "' + root.appUrl + '"; ' +
                        '    onLoadingChanged: function(loadReq) { ' +
                        '        root.title = title || qsTr("App Browser"); ' +
                        '        appBrowser.setLoading(loading); ' +
                        '        appBrowser.setCurrentUrl(url.toString()); ' +
                        '    } ' +
                        '}';
                    var obj = Qt.createQmlObject(src, webEnginePlaceholder,
                                                  "DynamicWebEngine");
                    if (obj) {
                        // Re-export properties so nav-bar bindings work.
                        webEnginePlaceholder.canGoBack =
                            Qt.binding(function() { return obj.canGoBack; });
                        webEnginePlaceholder.canGoForward =
                            Qt.binding(function() { return obj.canGoForward; });
                        webEnginePlaceholder.loading =
                            Qt.binding(function() { return obj.loading; });
                        webEnginePlaceholder.url =
                            Qt.binding(function() { return obj.url; });

                        webEnginePlaceholder.goBack = function() { obj.goBack(); };
                        webEnginePlaceholder.goForward = function() { obj.goForward(); };
                        webEnginePlaceholder.stop = function() { obj.stop(); };
                        webEnginePlaceholder.reload = function() { obj.reload(); };

                        fallbackMessage.visible = false;
                    }
                } catch (e) {
                    console.warn("AppBrowserPage: QtWebEngine not available:", e);
                    fallbackMessage.visible = true;
                }
            }
        }
    }

    // -- Fallback when WebEngine is not available --
    ColumnLayout {
        id: fallbackMessage
        anchors.centerIn: parent
        spacing: 16
        visible: webLoader.status !== Loader.Ready || true  // shown by default, hidden on success

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Web views require Qt WebEngine")
            font.pointSize: 16
            font.bold: true
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 400
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Qt WebEngine is required to browse workspace applications " +
                        "directly within Coder Desktop. Please install the " +
                        "QtWebEngine module or open the app in an external browser.")
            opacity: 0.7
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Open in External Browser")
            enabled: root.appUrl !== ""
            onClicked: Qt.openUrlExternally(root.appUrl)
        }
    }

    // -- Loading progress bar --
    ProgressBar {
        id: loadingBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 10
        indeterminate: true
        visible: webLoader.item ? webLoader.item.loading : false
        Material.accent: Material.Blue
    }

    // -- C++ backend instance --
    // AppBrowserWidget must be registered as a QML type (e.g. via
    // qmlRegisterType or QML_ELEMENT) before this page is loaded.
    // When it is not yet registered, instantiation will produce a QML
    // warning but the page still degrades gracefully.
    AppBrowserWidget {
        id: appBrowser
    }
}
