pragma Singleton
import QtQuick

QtObject {
    id: theme
    property string mode: "System"

    readonly property bool isDark: {
        if (mode === "Dark") return true
        if (mode === "Light") return false
        return Qt.styleHints.colorScheme === Qt.ColorScheme.Dark
    }

    // Backgrounds (zinc scale)
    readonly property color background:        isDark ? "#09090b" : "#fafafa"
    readonly property color surface:           isDark ? "#18181b" : "#f4f4f5"
    readonly property color surfaceSecondary:  isDark ? "#27272a" : "#e4e4e7"
    readonly property color surfaceTertiary:   isDark ? "#3f3f46" : "#d4d4d8"

    // Text
    readonly property color textPrimary:   isDark ? "#fafafa" : "#09090b"
    readonly property color textSecondary: isDark ? "#a1a1aa" : "#52525b"
    readonly property color textDisabled:  "#71717a"
    readonly property color textInvert:    isDark ? "#09090b" : "#fafafa"

    // Primary accent (sky blue)
    readonly property color primary:      isDark ? "#0ea5e9" : "#0284c7"
    readonly property color primaryLight: "#38bdf8"
    readonly property color primaryDark:  isDark ? "#0284c7" : "#0ea5e9"

    // Borders
    readonly property color border:      isDark ? "#3f3f46" : "#e4e4e7"
    readonly property color borderHover: isDark ? "#52525b" : "#d4d4d8"

    // Status
    readonly property color success: "#22c55e"
    readonly property color error:   isDark ? "#ef4444" : "#dc2626"
    readonly property color warning: "#f59e0b"
    readonly property color info:    isDark ? "#3b82f6" : "#2563eb"

    // Status surfaces
    readonly property color successSurface: isDark ? "#052e16" : "#f0fdf4"
    readonly property color errorSurface:   isDark ? "#450a0a" : "#fef2f2"
    readonly property color warningSurface: isDark ? "#451a03" : "#fffbeb"
    readonly property color activeSurface:  isDark ? "#082f49" : "#e0f2fe"

    readonly property color divider: border
    readonly property color hoverBg: isDark ? "#27272a" : "#f4f4f5"

    readonly property int radius: 8
    readonly property int radiusSm: 4

    function statusColor(status) {
        switch(status) {
            case "Running": return success
            case "Starting": case "Stopping": case "Canceling": return warning
            case "Failed": return error
            case "Deleting": return warning
            default: return textDisabled
        }
    }
    function statusSurface(status) {
        switch(status) {
            case "Running": return successSurface
            case "Starting": case "Stopping": return activeSurface
            case "Failed": return errorSurface
            case "Deleting": return warningSurface
            default: return surface
        }
    }
}
