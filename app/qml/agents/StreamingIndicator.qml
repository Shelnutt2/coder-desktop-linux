import QtQuick
import CoderDesktop

// Animated typing indicator shown on the streaming tail row: three dots
// that pulse in sequence while the assistant is producing output.
Row {
    id: indicator
    spacing: 4

    Repeater {
        model: 3
        Rectangle {
            required property int index
            width: 6; height: 6; radius: 3
            color: CoderTheme.textSecondary
            opacity: 0.3
            SequentialAnimation on opacity {
                running: indicator.visible
                loops: Animation.Infinite
                PauseAnimation { duration: index * 180 }
                NumberAnimation { from: 0.3; to: 1.0; duration: 260 }
                NumberAnimation { from: 1.0; to: 0.3; duration: 260 }
                PauseAnimation { duration: (2 - index) * 180 }
            }
        }
    }
}
