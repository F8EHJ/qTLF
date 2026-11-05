// Rencontres UFT — points, multipliers, final score and continents contacted, per band and total.
// Layout kept narrow (works from ~220 px wide) so it fits small screens.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: pal.window
    implicitWidth: 330
    implicitHeight: 400
    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    readonly property var s: contest.score || ({})
    readonly property var bands: s.bands || []
    readonly property var head: ["Band", "QSOs", "Points", "Mults"]

    function contText(list, counts) {
        if (!list || list.length === 0) return "—"
        var out = []
        for (var i = 0; i < list.length; ++i)
            out.push(list[i] + (counts ? " " + counts[list[i]] : ""))
        return out.join("  ")
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 10
        contentHeight: col.implicitHeight
        clip: true

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 6

            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: (contest.info.shortName || "") + " · " + (contest.info.mycall || "")
                      + (contest.myCty && contest.myCty.valid ? " (" + contest.myCty.cont + ")" : "")
                color: pal.windowText; font.bold: true; font.pixelSize: 14
            }

            // ---- per band: QSOs (valid), points, multipliers
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 3

                Repeater {
                    model: root.head
                    Text {
                        text: modelData; color: pal.windowText; opacity: 0.7
                        font.pixelSize: 12; font.bold: true
                        Layout.alignment: index === 0 ? Qt.AlignLeft : Qt.AlignRight
                        Layout.fillWidth: index === 0
                    }
                }
                Repeater {
                    model: root.bands.length * 4
                    Text {
                        readonly property var b: root.bands[Math.floor(index / 4)]
                        readonly property int c: index % 4
                        text: c === 0 ? b.band : c === 1 ? b.valid : c === 2 ? b.points : b.mults
                        color: pal.windowText
                        opacity: b.qsos === 0 ? 0.45 : 1.0
                        font.pixelSize: 15; font.family: "monospace"
                        Layout.alignment: c === 0 ? Qt.AlignLeft : Qt.AlignRight
                    }
                }
                Text { text: "Total"; color: pal.windowText; font.bold: true; font.pixelSize: 15 }
                Text { text: root.s.valid || 0; color: pal.windowText; font.bold: true; font.pixelSize: 15; font.family: "monospace"; Layout.alignment: Qt.AlignRight }
                Text { text: root.s.points || 0; color: pal.windowText; font.bold: true; font.pixelSize: 15; font.family: "monospace"; Layout.alignment: Qt.AlignRight }
                Text { text: root.s.multipliers || 0; color: pal.windowText; font.bold: true; font.pixelSize: 15; font.family: "monospace"; Layout.alignment: Qt.AlignRight }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: pal.mid }

            // ---- final score
            Text { text: "Final score"; color: pal.windowText; font.pixelSize: 13 }
            Text {
                text: (root.s.total || 0).toLocaleString(Qt.locale("en_US"), "f", 0)
                color: pal.highlight; font.pixelSize: 30; font.bold: true; font.family: "monospace"
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: (root.s.points || 0) + " points × " + (root.s.multipliers || 0) + " multipliers"
                color: pal.windowText; opacity: 0.7; font.pixelSize: 12
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: pal.mid }

            // ---- continents contacted, per band and total
            Text { text: "Continents contacted"; color: pal.windowText; font.bold: true; font.pixelSize: 13 }
            Repeater {
                model: root.bands
                Text {
                    visible: modelData.valid > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    textFormat: Text.RichText
                    text: "<b>" + modelData.band + "</b>  " + root.contText(modelData.continents, modelData.contCounts)
                    color: pal.windowText; font.pixelSize: 13; font.family: "monospace"
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                text: "<b>Total</b>  " + root.contText(root.s.continents, root.s.contCounts)
                      + "  (" + (root.s.continents ? root.s.continents.length : 0) + ")"
                color: pal.windowText; font.pixelSize: 13; font.family: "monospace"
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: !contest.ctyLoaded || (root.s.unknownCont || 0) > 0
                text: !contest.ctyLoaded
                      ? "cty.dat not loaded: continents unknown, every QSO counted as same continent. Contest › Update data files (Ctrl+U)."
                      : root.s.unknownCont + " QSO(s) with unknown continent, counted as same continent."
                color: "#c0392b"; font.pixelSize: 12
            }
            Text {
                visible: !!root.s.error
                text: "Scoring error: " + (root.s.error || "")
                color: "#c0392b"; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
        }
    }
}
