// Lists the Silent Key callsigns (multipliers) worked on each band.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: pal.window
    implicitWidth: 320
    implicitHeight: 220
    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    readonly property var bands: (contest.score && contest.score.bands) ? contest.score.bands : []

    Flickable {
        anchors.fill: parent
        anchors.margins: 10
        contentHeight: col.implicitHeight
        clip: true

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 10

            Repeater {
                model: root.bands
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: modelData.band + "  —  " + modelData.mults + " Silent Key" + (modelData.mults === 1 ? "" : "s")
                        color: pal.windowText; font.bold: true; font.pixelSize: 13
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: modelData.multList
                            Rectangle {
                                radius: 3
                                color: pal.alternateBase
                                border.color: pal.mid
                                width: t.implicitWidth + 10; height: t.implicitHeight + 4
                                Text { id: t; anchors.centerIn: parent; text: modelData
                                       color: pal.text; font.family: "monospace"; font.pixelSize: 12 }
                            }
                        }
                    }
                    Text { visible: modelData.mults === 0; text: "none yet"; color: pal.windowText; opacity: 0.5; font.pixelSize: 12 }
                }
            }
        }
    }
}
