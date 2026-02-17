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

    /// Emitted when the user wants to close the browser and return to the detail page.
    signal closeRequested()

    /// The resolved app URL (computed when properties change).
    readonly property string appUrl: {
        if (!appSlug || (!vpnActive && !agentId)) {
            return "";
        }
        return appBrowser.buildAppUrl(deploymentUrl, agentId, appSlug,
                                       workspaceName, agentName, vpnActive);
    }

    title: qsTr("App Browser")

    onAppUrlChanged: {
        console.log("[AppBrowser] appUrl changed to:", appUrl);
        if (webLoader.item && webLoader.item._webView && appUrl && appUrl.length > 0) {
            console.log("[AppBrowser] Updating WebEngineView URL to:", appUrl);
            // Re-inject cookie in case URL changed to different domain
            if (root.sessionToken && root.deploymentUrl) {
                appBrowser.injectSessionCookie(root.deploymentUrl, root.sessionToken);
            }
            webLoader.item._webView.url = appUrl;
        }
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

            ToolButton {
                id: backButton
                text: "◀"
                font.pixelSize: 14
                enabled: webLoader.item ? webLoader.item.canGoBack : false
                onClicked: {
                    if (webLoader.item) {
                        webLoader.item.goBack();
                    }
                }
                ToolTip.text: qsTr("Back")
                ToolTip.visible: hovered
            }

            ToolButton {
                id: forwardButton
                text: "▶"
                font.pixelSize: 14
                enabled: webLoader.item ? webLoader.item.canGoForward : false
                onClicked: {
                    if (webLoader.item) {
                        webLoader.item.goForward();
                    }
                }
                ToolTip.text: qsTr("Forward")
                ToolTip.visible: hovered
            }

            ToolButton {
                id: reloadButton
                text: webLoader.item && webLoader.item.loading ? "✕" : "↻"
                font.pixelSize: 16
                onClicked: {
                    if (webLoader.item) {
                        if (webLoader.item.loading) {
                            webLoader.item.stop();
                        } else {
                            webLoader.item.reload();
                        }
                    }
                }
                ToolTip.text: webLoader.item && webLoader.item.loading
                              ? qsTr("Stop") : qsTr("Reload")
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
            // Properties declared as var so they can be reassigned to real
            // WebEngineView methods once the engine is available.
            property bool canGoBack: false
            property bool canGoForward: false
            property bool loading: false
            property url url: root.appUrl

            property var goBack: function() {}
            property var goForward: function() {}
            property var stop: function() {}
            property var reload: function() {}
            property var _webView: null

            Component.onCompleted: {
                console.log("[AppBrowser] Component.onCompleted, appUrl:", root.appUrl);
                console.log("[AppBrowser] Properties: deploymentUrl=", root.deploymentUrl,
                            "agentId=", root.agentId, "appSlug=", root.appSlug,
                            "workspaceName=", root.workspaceName, "agentName=", root.agentName,
                            "vpnActive=", root.vpnActive);

                // Try to instantiate a real WebEngineView.
                try {
                    // Create WITHOUT url — we'll set it after cookie injection.
                    var src =
                        'import QtQuick; ' +
                        'import QtWebEngine; ' +
                        'WebEngineView { ' +
                        '    anchors.fill: parent; ' +
                        '    onLoadingChanged: function(loadReq) { ' +
                        '        console.log("[AppBrowser] loadingChanged:", loading, ' +
                        '                    "url:", url, "status:", loadReq.status, ' +
                        '                    "errorString:", loadReq.errorString); ' +
                        '        root.title = title || qsTr("App Browser"); ' +
                        '        appBrowser.setLoading(loading); ' +
                        '        appBrowser.setCurrentUrl(url.toString()); ' +
                        '    } ' +
                        '    onCertificateError: function(error) { ' +
                        '        console.warn("[AppBrowser] Certificate error:", error.description); ' +
                        '        error.acceptCertificate(); ' +
                        '    } ' +
                        '    onNavigationRequested: function(request) { ' +
                        '        console.log("[AppBrowser] Navigation:", request.url, ' +
                        '                    "type:", request.navigationType); ' +
                        '    } ' +
                        '}';
                    console.log("[AppBrowser] Creating WebEngineView...");
                    var obj = Qt.createQmlObject(src, webEnginePlaceholder,
                                                  "DynamicWebEngine");
                    if (obj) {
                        console.log("[AppBrowser] WebEngineView created successfully");

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

                        // Store reference for URL loading
                        webEnginePlaceholder._webView = obj;

                        // Inject session cookie then load URL
                        if (root.sessionToken && root.deploymentUrl) {
                            console.log("[AppBrowser] Injecting session cookie...");
                            appBrowser.injectSessionCookie(root.deploymentUrl, root.sessionToken);
                        } else {
                            console.warn("[AppBrowser] No session token or deployment URL for cookie injection");
                        }

                        // Now load the URL
                        if (root.appUrl && root.appUrl.length > 0) {
                            console.log("[AppBrowser] Loading URL:", root.appUrl);
                            obj.url = root.appUrl;
                        } else {
                            console.warn("[AppBrowser] appUrl is empty, waiting for it to resolve...");
                        }
                    } else {
                        console.warn("[AppBrowser] Qt.createQmlObject returned null");
                        fallbackMessage.visible = true;
                    }
                } catch (e) {
                    console.warn("[AppBrowser] QtWebEngine not available:", e);
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
        visible: true  // shown by default; hidden imperatively when WebEngine loads successfully

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
            visible: typeof settingsManager !== "undefined" && settingsManager.externalBrowserAllowed
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
    // AppBrowserWidget is exposed as the "appBrowser" context property
    // from main.cpp — no QML instantiation needed.

    Component.onCompleted: {
        console.log("[AppBrowser] Page created. deploymentUrl:", deploymentUrl,
                    "agentId:", agentId, "appSlug:", appSlug,
                    "workspaceName:", workspaceName, "agentName:", agentName,
                    "vpnActive:", vpnActive, "sessionToken length:", sessionToken.length);
    }
}
