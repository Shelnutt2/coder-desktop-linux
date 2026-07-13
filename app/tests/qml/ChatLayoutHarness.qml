import QtQuick
import CoderDesktop

// Offscreen harness window for ChatLayoutTest: instantiates the real
// AgentChatPage against mocked context properties (agentsController,
// workspaceModel, settingsManager) injected by tst_chatlayout.cpp.
Window {
    id: win
    width: 480
    height: 700
    visible: true
    color: "#101010"

    property alias page: page

    AgentChatPage {
        id: page
        objectName: "chatPage"
        anchors.fill: parent
        chatId: "chat-main"
    }
}
