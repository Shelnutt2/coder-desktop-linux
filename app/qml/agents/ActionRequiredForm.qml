import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Dynamic tool runner for a pending action_required tool call.
//
// The tool call's args are shown read-only and dynamic tool input is always
// entered through the raw JSON editor. The typed form path (JsonSchemaParser)
// is used only when a declared input schema is supplied via schemaJson;
// today no tool declares one, so the form path is never taken. Tool-call
// args are NOT a schema and must never be fed to the schema parser.
//
// Actions follow the Android DynamicToolRunnerCard semantics: Submit posts
// the JSON output; "Submit as error" posts {"error": message} with
// is_error=true; "Cancel chat" interrupts the run.
Rectangle {
    id: form
    // The page's ChatController context object.
    property var chat: null
    property string toolCallId: ""
    property string toolName: ""
    property string argsJson: ""
    // Declared JSON input schema for the tool, when the server supplies
    // one. Empty (the only case today) selects the raw JSON editor.
    property string schemaJson: ""

    readonly property var schema: chat && schemaJson.length > 0
        ? chat.parseToolSchema(schemaJson)
        : ({ supported: false, fields: [] })
    readonly property bool useForm: schema.supported
    property bool errorEditorOpen: false
    property bool submitted: false

    // Field values keyed by "key" or "parentKey.childKey".
    property var formValues: ({})

    implicitHeight: col.implicitHeight + 24
    radius: CoderTheme.radius
    color: CoderTheme.warningSurface
    border.color: CoderTheme.warning
    border.width: 1

    function projectToJson() {
        if (!useForm) return rawEditor.text
        var out = {}
        var fields = schema.fields
        for (var i = 0; i < fields.length; ++i) {
            var f = fields[i]
            if (f.type === "object") {
                var nested = {}
                for (var j = 0; j < f.fields.length; ++j) {
                    var c = f.fields[j]
                    nested[c.key] = coerce(c, formValues[f.key + "." + c.key])
                }
                out[f.key] = nested
            } else {
                out[f.key] = coerce(f, formValues[f.key])
            }
        }
        return JSON.stringify(out)
    }

    function coerce(field, value) {
        if (field.type === "boolean") return value === true
        if (field.type === "number") return Number(value || 0)
        if (field.type === "integer") return parseInt(value || 0, 10)
        return value === undefined ? "" : String(value)
    }

    // One form row per scalar field; shared by top-level and nested fields.
    component FieldRow: ColumnLayout {
        property var field
        property string valueKey
        spacing: 2
        Layout.fillWidth: true

        Label {
            text: field.key + (field.required ? " *" : "")
            color: CoderTheme.textSecondary
            font.pixelSize: 11
        }
        Label {
            visible: field.description && field.description.length > 0
            text: field.description || ""
            color: CoderTheme.textDisabled
            font.pixelSize: 10
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Switch {
            visible: field.type === "boolean"
            checked: form.formValues[valueKey] === true
            onToggled: {
                var v = form.formValues
                v[valueKey] = checked
                form.formValues = v
            }
        }
        ComboBox {
            visible: field.type === "enum"
            Layout.fillWidth: true
            model: field.enumOptions || []
            font.pixelSize: 12
            onActivated: {
                var v = form.formValues
                v[valueKey] = currentText
                form.formValues = v
            }
            Component.onCompleted: if (visible && count > 0) {
                var v = form.formValues
                v[valueKey] = textAt(0)
                form.formValues = v
            }
        }
        TextField {
            visible: field.type === "string" || field.type === "number"
                     || field.type === "integer"
            Layout.fillWidth: true
            font.pixelSize: 12
            inputMethodHints: field.type === "string" ? Qt.ImhNone : Qt.ImhFormattedNumbersOnly
            onTextChanged: {
                var v = form.formValues
                v[valueKey] = text
                form.formValues = v
            }
        }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            spacing: 6
            Label { text: "!"; color: CoderTheme.warning; font.bold: true }
            Label {
                text: "Action required: " + (form.toolName || "tool")
                color: CoderTheme.warning
                font.pixelSize: 13
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // Read-only agent-supplied arguments.
        Label {
            visible: form.argsJson.length > 0
            text: form.argsJson
            color: CoderTheme.textSecondary
            font.family: "monospace"
            font.pixelSize: 10
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 6
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // Waiting placeholder after submit.
        Label {
            visible: form.submitted
            text: "Result submitted, waiting for the agent\u2026"
            color: CoderTheme.textSecondary
            font.pixelSize: 12
        }

        // ---- Generated form ----
        ColumnLayout {
            visible: form.useForm && !form.submitted && !form.errorEditorOpen
            spacing: 6
            Layout.fillWidth: true

            Repeater {
                model: form.useForm ? form.schema.fields : []
                delegate: ColumnLayout {
                    id: fieldDelegate
                    required property var modelData
                    readonly property var outerField: modelData
                    spacing: 4
                    Layout.fillWidth: true

                    FieldRow {
                        visible: fieldDelegate.outerField.type !== "object"
                        field: fieldDelegate.outerField
                        valueKey: fieldDelegate.outerField.key
                    }

                    // One nested object level: group label + child rows.
                    ColumnLayout {
                        visible: fieldDelegate.outerField.type === "object"
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.leftMargin: 8

                        Label {
                            text: fieldDelegate.outerField.key
                            color: CoderTheme.textPrimary
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                        Repeater {
                            model: fieldDelegate.outerField.type === "object"
                                   ? fieldDelegate.outerField.fields : []
                            delegate: FieldRow {
                                required property var modelData
                                field: modelData
                                valueKey: fieldDelegate.outerField.key + "." + modelData.key
                            }
                        }
                    }
                }
            }
        }

        // ---- Raw JSON fallback editor ----
        Rectangle {
            visible: !form.useForm && !form.submitted && !form.errorEditorOpen
            Layout.fillWidth: true
            implicitHeight: Math.min(rawEditor.implicitHeight + 12, 160)
            radius: CoderTheme.radiusSm
            color: CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1

            TextArea {
                id: rawEditor
                anchors.fill: parent
                anchors.margins: 4
                text: "{}"
                wrapMode: TextArea.Wrap
                font.family: "monospace"
                font.pixelSize: 11
                color: CoderTheme.textPrimary
                background: null
            }
        }

        // ---- Submit-as-error editor ----
        TextField {
            id: errorMessage
            visible: form.errorEditorOpen && !form.submitted
            Layout.fillWidth: true
            placeholderText: "Error message"
            font.pixelSize: 12
        }

        RowLayout {
            visible: !form.submitted
            spacing: 8

            CoderButton {
                text: form.errorEditorOpen ? "Submit error" : "Submit"
                onClicked: {
                    if (form.errorEditorOpen) {
                        var msg = errorMessage.text.length > 0
                            ? errorMessage.text : "(no message)"
                        form.chat.submitToolResult(form.toolCallId,
                                                   JSON.stringify({ error: msg }), true)
                    } else {
                        form.chat.submitToolResult(form.toolCallId, form.projectToJson(), false)
                    }
                    form.submitted = true
                }
            }
            CoderButton {
                text: form.errorEditorOpen ? "Back" : "Submit as error"
                variant: "outline"
                onClicked: form.errorEditorOpen = !form.errorEditorOpen
            }
            CoderButton {
                text: "Cancel chat"
                variant: "destructive"
                onClicked: form.chat.interrupt()
            }
        }
    }
}
