import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: control
    property string variant: "default"  // "default", "outline", "subtle", "destructive"

    Material.background: {
        if (variant === "default") return CoderTheme.primary
        if (variant === "destructive") return CoderTheme.errorSurface
        return "transparent"
    }
    Material.foreground: {
        if (variant === "default") return CoderTheme.textInvert
        if (variant === "destructive") return CoderTheme.error
        return CoderTheme.textPrimary
    }
    Material.roundedScale: Material.SmallScale

    font.weight: Font.DemiBold

    background: Rectangle {
        implicitHeight: 36
        radius: CoderTheme.radius
        color: control.down ? Qt.darker(bgColor, 1.1) : control.hovered ? Qt.lighter(bgColor, 1.1) : bgColor
        border.color: variant === "outline" ? CoderTheme.border : "transparent"
        border.width: variant === "outline" ? 1 : 0

        property color bgColor: {
            if (!control.enabled) return CoderTheme.surfaceSecondary
            if (variant === "default") return CoderTheme.primary
            if (variant === "destructive") return CoderTheme.errorSurface
            return "transparent"
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: {
            if (!control.enabled) return CoderTheme.textDisabled
            if (variant === "default") return CoderTheme.textInvert
            if (variant === "destructive") return CoderTheme.error
            return CoderTheme.textPrimary
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
