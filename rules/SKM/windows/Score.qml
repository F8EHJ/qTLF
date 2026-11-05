// SKMC score window.  "contest" is provided by qTLF:
//   contest.score      -> object returned by scoring.js
//   contest.info       -> contest metadata (name, date, mycall, ...)
//   contest.qsos       -> array of QSOs (with .status)
//   contest.bandCounts -> [{band, qsos}]
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: pal.window
    implicitWidth: 320
    implicitHeight: 260

    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    readonly property var s: contest.score || ({})
    readonly property var bands: s.bands || []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Text {
            text: (contest.info.shortName || "") + "  ·  " + (contest.info.mycall || "")
            color: pal.windowText; font.bold: true; font.pixelSize: 14
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 14
            rowSpacing: 4

            Repeater {
                model: ["Band", "QSOs", "Points", "Mults", "Dupes"]
                Text { text: modelData; color: pal.windowText; opacity: 0.7; font.pixelSize: 12; font.bold: true
                       Layout.alignment: index === 0 ? Qt.AlignLeft : Qt.AlignRight }
            }

            Repeater {
                model: root.bands.length * 5
                Text {
                    readonly property var b: root.bands[Math.floor(index / 5)]
                    readonly property int c: index % 5
                    text: c === 0 ? b.band : c === 1 ? b.qsos : c === 2 ? b.points : c === 3 ? b.mults : b.dupes
                    color: pal.windowText
                    font.pixelSize: 15
                    font.family: "monospace"
                    Layout.alignment: c === 0 ? Qt.AlignLeft : Qt.AlignRight
                }
            }

            Text { text: "Total"; color: pal.windowText; font.bold: true; font.pixelSize: 15 }
            Text { text: contest.qsoCount; color: pal.windowText; font.bold: true; font.pixelSize: 15; font.family: "monospace"; Layout.alignment: Qt.AlignRight }
            Text { text: root.s.points || 0; color: pal.windowText; font.bold: true; font.pixelSize: 15; font.family: "monospace"; Layout.alignment: Qt.AlignRight }
            Text { text: root.s.multipliers || 0; color: pal.windowText; font.bold: true; font.pixelSize: 15; font.family: "monospace"; Layout.alignment: Qt.AlignRight }
            Item { width: 1; height: 1 }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: pal.mid }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Final score"; color: pal.windowText; font.pixelSize: 14; Layout.fillWidth: true }
            Text {
                text: (root.s.total || 0).toLocaleString(Qt.locale("en_US"), "f", 0)
                color: pal.highlight; font.pixelSize: 30; font.bold: true; font.family: "monospace"
            }
        }
        Text {
            text: (root.s.points || 0) + " points × " + (root.s.multipliers || 0) + " multipliers"
            color: pal.windowText; opacity: 0.7; font.pixelSize: 12
        }

        Text {
            visible: !!root.s.error
            text: "Scoring error: " + (root.s.error || "")
            color: "#d9534f"; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
    }
}
