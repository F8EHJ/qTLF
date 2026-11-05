// Rencontres UFT — information about the call being typed:
// first name and type from the UFT member list, UFT number, country / continent (cty.dat),
// DX or same continent, points this QSO would give, bands already worked.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: pal.window
    implicitWidth: 330
    implicitHeight: 210
    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    readonly property string call: contest.entryCall
    readonly property var ci: contest.callInfo || ({})
    readonly property var cty: ci.cty || ({})
    readonly property string myCont: (contest.myCty && contest.myCty.valid) ? contest.myCty.cont : ""
    readonly property bool official: call.split("/")[0] === "F8UFT"
    readonly property bool member: ci.known === true
    readonly property bool dx: cty.valid === true && myCont !== "" && cty.cont !== myCont

    function typeText(t) {
        if (t === "RC") return "Radio club"
        if (t === "YL") return "YL"
        if (t === "OM") return "OM"
        return t || ""
    }
    function expectedPoints() {
        if (official) return 20
        if (member) return dx ? 10 : 5
        return dx ? 2 : 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: root.call !== "" ? root.call : "Type a callsign…"
            color: pal.windowText; opacity: root.call !== "" ? 1 : 0.5
            font.pixelSize: root.call !== "" ? 24 : 15; font.bold: root.call !== ""; font.family: "monospace"
        }

        // --- member list
        Text {
            visible: root.call !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.RichText
            color: pal.windowText; font.pixelSize: 16
            text: root.member
                  ? "<b>" + (root.ci.name || "—") + "</b>  ·  " + root.typeText(root.ci.type)
                    + "  ·  UFT <b>" + root.ci.nr + "</b>"
                  : "<i>Not in the UFT list (non-member: NM?)</i>"
        }

        // --- country / continent
        Text {
            visible: root.call !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.RichText
            color: pal.windowText; font.pixelSize: 14
            text: root.cty.valid
                  ? root.cty.country + "  ·  <b>" + root.cty.cont + "</b>"
                    + (root.myCont === "" ? "" : (root.dx ? "  ·  <b style='color:#d35400'>DX</b>" : "  ·  same continent"))
                  : (contest.ctyLoaded ? "Country unknown" : "Country unknown (cty.dat not loaded)")
        }

        // --- points / multiplier / status
        Text {
            visible: root.call !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.RichText
            font.pixelSize: 14
            color: root.ci.dupe ? "#c0392b" : pal.windowText
            text: root.ci.dupe
                  ? "<b>DUPE</b> on " + contest.currentBand
                  : "QSO = <b>" + root.expectedPoints() + " pts</b>"
                    + ((root.member || root.official) ? " + multiplier" : "")
                    + (root.official ? "  (official station)" : "")
        }
        Text {
            visible: root.call !== "" && !!root.ci.workedBands && root.ci.workedBands.length > 0
            text: "Worked on: " + (root.ci.workedBands ? root.ci.workedBands.join(", ") : "")
            color: pal.windowText; opacity: 0.75; font.pixelSize: 13
        }
        Item { Layout.fillHeight: true }
    }
}
