pragma Singleton

import QtQuick

QtObject {
    readonly property real radiusSmall: 8
    readonly property real radiusMedium: 12
    readonly property real radiusLarge: 18
    readonly property real radiusHero: 24
    readonly property color backgroundStart: ThemeSettings.theme === "dark" ? "#111827" : "#334155"
    readonly property color backgroundEnd: ThemeSettings.theme === "dark" ? "#1e1b4b" : "#475569"
    readonly property color startupStart: "#2007df"
    readonly property color startupEnd: "#971ed8"
    readonly property color textPrimary: ThemeSettings.theme === "dark" ? "#e2e8f0" : "#1e293b"
    readonly property color textSecondary: ThemeSettings.theme === "dark" ? "#cbd5e1" : "#475569"
    readonly property color textMuted: "#64748b"
    readonly property color panelDark: "#991e293b"
    readonly property color panelLight: "#ccffffff"
    readonly property color fieldDark: "#0f172a"
    readonly property color fieldLight: "#f1f5f9"
    readonly property color borderDark: "#334155"
    readonly property color borderLight: "#cbd5e1"
    readonly property color accent: "#2563eb"
    readonly property color selectionDark: "#334e75"
    readonly property color selectionLight: "#dbeafe"
}
