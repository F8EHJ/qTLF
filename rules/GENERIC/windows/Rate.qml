// QSO rate: last 10 and last 60 minutes, computed from contest.qsos.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: pal.window
    implicitWidth: 260
    implicitHeight: 140
    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    property int last10: 0
    property int last60: 0

    function refresh() {
        var now = Date.now(), a = 0, b = 0
        var list = contest.qsos
        for (var i = list.length - 1; i >= 0; --i) {
            var age = (now - Date.parse(list[i].datetime)) / 60000
            if (age > 60) break
            if (age <= 10) a++
            b++
        }
        last10 = a; last60 = b
    }
    Connections { target: contest; function onUpdated() { root.refresh() } }
    Timer { interval: 15000; running: true; repeat: true; triggeredOnStart: true; onTriggered: root.refresh() }

    GridLayout {
        anchors.fill: parent; anchors.margins: 10
        columns: 2; rowSpacing: 6
        Text { text: "Last 10 min"; color: pal.windowText }
        Text { text: root.last10 + "  (" + root.last10 * 6 + "/h)"; color: pal.windowText; font.bold: true; font.pixelSize: 16; Layout.alignment: Qt.AlignRight }
        Text { text: "Last 60 min"; color: pal.windowText }
        Text { text: root.last60 + "/h"; color: pal.windowText; font.bold: true; font.pixelSize: 16; Layout.alignment: Qt.AlignRight }
        Text { text: "Score"; color: pal.windowText }
        Text { text: contest.score.total || 0; color: pal.highlight; font.bold: true; font.pixelSize: 22; Layout.alignment: Qt.AlignRight }
        Item { Layout.fillHeight: true }
    }
}
