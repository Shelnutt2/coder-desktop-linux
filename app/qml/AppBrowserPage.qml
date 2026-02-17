import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Page {
    id: root

    /// Deployment base URL (e.g. "https://coder.example.com").
    property string deploymentUrl: ""

    /// The app's URL from the API (used for VPN rewrite or external apps).
    property string appUrl: ""

    /// Application slug identifier.
    property string appSlug: ""

    /// Workspace name (used in VPN-mode hostname and path-based proxy).
    property string workspaceName: ""

    /// Owner name of the workspace (used in path-based proxy URL).
    property string ownerName: ""

    /// Agent name within the workspace (used in VPN-mode hostname).
    property string agentName: ""

    /// Whether the VPN tunnel is currently active.
    property bool vpnActive: false

    /// Whether this is an external app (opens its URL directly).
    property bool isExternal: false

    /// Session token for cookie-based authentication.
    property string sessionToken: ""

    /// Emitted when the user wants to close the browser and return to the detail page.
    signal closeRequested()

    /// Whether the WebEngineView has been successfully created.
    property bool webEngineReady: false

    /// Whether cookies have been injected for this session.
    property bool cookieInjected: false

    /// Attempt to inject cookie and load the URL. Only proceeds when
    /// the WebEngine is created AND we have a token AND a resolvedUrl.
    function tryLoadUrl() {
        console.log("[AppBrowser] tryLoadUrl: webEngineReady=", webEngineReady,
                    "resolvedUrl=", resolvedUrl, "sessionToken length=", sessionToken.length,
                    "cookieInjected=", cookieInjected);

        if (!webEngineReady || !webLoader.item || !webLoader.item._webView) {
            console.log("[AppBrowser] tryLoadUrl: WebEngine not ready yet");
            return;
        }
        if (!resolvedUrl || resolvedUrl.length === 0) {
            console.log("[AppBrowser] tryLoadUrl: No URL to load yet");
            return;
        }

        // Inject cookie if we have a token and haven't injected yet
        if (sessionToken && sessionToken.length > 0 && deploymentUrl && !cookieInjected) {
            console.log("[AppBrowser] tryLoadUrl: Injecting session cookie for", deploymentUrl);
            appBrowser.injectSessionCookie(deploymentUrl, sessionToken);
            cookieInjected = true;
        }

        console.log("[AppBrowser] tryLoadUrl: Loading URL:", resolvedUrl);
        webLoader.item._webView.url = resolvedUrl;
    }

    /// The resolved URL to load (computed from properties).
    readonly property string resolvedUrl: {
        if (isExternal && appUrl) {
            return appUrl;
        }
        if (!appSlug) {
            return "";
        }
        return appBrowser.buildAppUrl(deploymentUrl, appUrl, appSlug,
                                       workspaceName, ownerName, agentName,
                                       vpnActive, isExternal);
    }

    title: qsTr("App Browser")
    background: Rectangle { color: CoderTheme.background }

    onResolvedUrlChanged: {
        console.log("[AppBrowser] resolvedUrl changed to:", resolvedUrl);
        // Reset cookie state if URL changed (e.g. different deployment)
        cookieInjected = false;
        tryLoadUrl();
    }

    onSessionTokenChanged: {
        console.log("[AppBrowser] sessionToken changed, length:", sessionToken.length);
        cookieInjected = false;
        tryLoadUrl();
    }

    // -- Navigation bar --
    header: ToolBar {
        background: Rectangle {
            color: CoderTheme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: CoderTheme.divider
            }
        }

        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton {
                icon.name: "go-previous"
                icon.color: hovered ? CoderTheme.textPrimary : CoderTheme.textSecondary
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
                palette.buttonText: hovered ? CoderTheme.textPrimary : CoderTheme.textSecondary
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
                palette.buttonText: hovered ? CoderTheme.textPrimary : CoderTheme.textSecondary
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
                palette.buttonText: hovered ? CoderTheme.textPrimary : CoderTheme.textSecondary
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
                text: webLoader.item ? webLoader.item.url : root.resolvedUrl
                font.pointSize: 9
                color: CoderTheme.textPrimary
                verticalAlignment: TextInput.AlignVCenter

                background: Rectangle {
                    color: CoderTheme.surfaceSecondary
                    radius: CoderTheme.radiusSm
                }
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
            property url url: root.resolvedUrl

            property var goBack: function() {}
            property var goForward: function() {}
            property var stop: function() {}
            property var reload: function() {}
            property var _webView: null

            Component.onCompleted: {
                console.log("[AppBrowser] WebEngine Component.onCompleted");

                try {
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
                        webEnginePlaceholder._webView = obj;

                        // Mark WebEngine as ready — tryLoadUrl will handle the rest
                        root.webEngineReady = true;
                        console.log("[AppBrowser] WebEngine ready, calling tryLoadUrl");
                        root.tryLoadUrl();
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
            color: CoderTheme.textPrimary
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 400
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Qt WebEngine is required to browse workspace applications " +
                        "directly within Coder Desktop. Please install the " +
                        "QtWebEngine module or open the app in an external browser.")
            color: CoderTheme.textSecondary
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Open in External Browser")
            visible: typeof settingsManager !== "undefined" && settingsManager.externalBrowserAllowed
            enabled: root.resolvedUrl !== ""
            onClicked: Qt.openUrlExternally(root.resolvedUrl)
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
        Material.accent: CoderTheme.primary
    }

    // -- C++ backend instance --
    // AppBrowserWidget is exposed as the "appBrowser" context property
    // from main.cpp — no QML instantiation needed.

    Component.onCompleted: {
        console.log("[AppBrowser] Page created. deploymentUrl:", deploymentUrl,
                    "appUrl:", appUrl, "appSlug:", appSlug,
                    "workspaceName:", workspaceName, "ownerName:", ownerName,
                    "agentName:", agentName, "vpnActive:", vpnActive,
                    "isExternal:", isExternal, "sessionToken length:", sessionToken.length);

        // When cookie is confirmed ready, log it (belt-and-suspenders)
        appBrowser.cookieReady.connect(function() {
            console.log("[AppBrowser] cookieReady signal received");
        });
    }
}
