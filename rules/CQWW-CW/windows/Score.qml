import QtQuick
import QtQuick.Layouts

// CQ WW CW - Score window
//   - contest title and rules link
//   - per band: QSOs, points, zones, countries, multipliers + totals
//   - final score
//   - station being entered: country, continent, CQ zone, points it would give,
//     new zone / new country on the current band, dupe, bands already worked
Rectangle {
    id: root
    color: pal.window
    implicitWidth: 320
    implicitHeight: col.implicitHeight + 20

    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    readonly property string rulesUrl: "https://cqww.com/rules/"
    readonly property var s: contest.score || ({})
    readonly property var info: contest.info || ({})
    readonly property var ci: contest.callInfo || ({})
    readonly property var cty: ci.cty || ({})
    readonly property var myCty: contest.myCty || ({})
    readonly property bool hasEntry: (contest.entryCall || "") !== ""
    readonly property color warnColor: "#d9534f"
    readonly property color goodColor: "#2e9d4f"

    function fieldValue(tag) {
        return String(((root.info.fields || {})[tag]) || "").toUpperCase();
    }
    readonly property bool isClassic: fieldValue("CATEGORY-OVERLAY") === "CLASSIC"

    function isMM(call) { return /\/MM(\/|$)/i.test(call || ""); }

    // Same rule as scoring.js (qsoPoints)
    function entryPoints() {
        if (!contest.ctyLoaded || !cty.valid || !myCty.valid) return -1;
        var mm = isMM(contest.entryCall);
        if (!mm && myCty.country === cty.country) return 0;
        if (myCty.cont !== cty.cont) return 3;
        return myCty.cont === "NA" ? 2 : 1;
    }

    function currentBandData() {
        var l = s.bands || [];
        for (var i = 0; i < l.length; i++)
            if (l[i].band === contest.currentBand) return l[i];
        return ({ zoneList: [], countryList: [] });
    }

    function pad2(n) { return (n < 10 ? "0" : "") + n; }

    function isNewZone() {
        var z = parseInt(cty.cqz, 10);
        if (!(z >= 1 && z <= 40)) return false;
        return (currentBandData().zoneList || []).indexOf(pad2(z)) < 0;
    }

    function isNewCountry() {
        if (!cty.country || isMM(contest.entryCall)) return false;
        return (currentBandData().countryList || []).indexOf(cty.country) < 0;
    }

    function fmt(n) {
        var str = String(n || 0);
        return str.replace(/\B(?=(\d{3})+(?!\d))/g, " ");
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 4

        // ---- Title and rules link ---------------------------------------
        Text {
            Layout.fillWidth: true
            text: root.info.name || "CQ WorldWide DX Contest - CW"
            color: pal.windowText
            font.bold: true
            font.pixelSize: 16
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            textFormat: Text.StyledText
            text: "<a href=\"" + root.rulesUrl + "\">Contest rules (cqww.com)</a>"
            linkColor: pal.highlight
            color: pal.windowText
            elide: Text.ElideRight
            onLinkActivated: function (link) { Qt.openUrlExternally(link) }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: Qt.PointingHandCursor
            }
        }

        // ---- CLASSIC overlay reminder -------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            visible: root.isClassic
            // Shown until a first off time of 60 min or more is in the log
            Text {
                Layout.fillWidth: true
                visible: (root.s.offTimes || 0) === 0
                text: "Don't forget OFF-TIME ! (60 minutes)"
                color: root.warnColor
                font.bold: true
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: (root.s.offTimes || 0) > 0
                readonly property int m: root.s.offMinutes || 0
                text: "Off-time taken: " + (root.s.offTimes || 0) + " (" + Math.floor(m / 60) + "h" +
                      ((m % 60) < 10 ? "0" : "") + (m % 60) + ")"
                color: root.goodColor
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                readonly property int m: root.s.opMinutes || 0
                text: "Classic: operating time " + Math.floor(m / 60) + "h" +
                      ((m % 60) < 10 ? "0" : "") + (m % 60) + " / 24h00" +
                      (m > 1440 ? " — over 24 h: only the first 24 h count!" : "")
                color: m > 1440 ? root.warnColor : pal.windowText
                font.family: "monospace"
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: root.fieldValue("CATEGORY-ASSISTED") === "ASSISTED"
                text: "Assisted entries are not eligible for CLASSIC."
                color: root.warnColor
                wrapMode: Text.WordWrap
            }
        }

        // ---- Warnings ---------------------------------------------------
        Text {
            Layout.fillWidth: true
            visible: !contest.ctyLoaded
            text: "cty.dat not loaded — Contest › Update data files (Ctrl+U). Unknown continent counted as 1 point, no country multiplier."
            color: root.warnColor
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: contest.ctyLoaded && (root.s.unknownCont || 0) > 0
            text: (root.s.unknownCont || 0) + " QSO(s) with unknown continent (counted as 1 point)"
            color: root.warnColor
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: !!root.s.singleBand
            text: "Single-band entry: " + (root.s.singleBand || "") + " only (" +
                  (root.s.notScored || 0) + " QSO(s) on other bands not scored)"
            color: pal.windowText
            opacity: 0.8
            wrapMode: Text.WordWrap
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: pal.mid }

        // ---- Score table ------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: ["Band", "QSOs", "Pts", "Zn", "Cty", "Mult"]
                delegate: Text {
                    required property var modelData
                    required property int index
                    text: modelData
                    color: pal.windowText
                    font.bold: true
                    Layout.fillWidth: index === 0
                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                    Layout.preferredWidth: index === 0 ? 40 : 38
                }
            }
        }

        // One row per band (same widths as the header)
        Repeater {
            model: root.s.bands || []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 6
                readonly property bool cur: modelData.band === contest.currentBand
                opacity: modelData.qsos > 0 || cur ? 1 : 0.4
                Text { text: (cur ? "▸" : " ") + modelData.band; color: cur ? pal.highlight : pal.windowText; font.bold: cur
                       font.family: "monospace"; Layout.fillWidth: true; Layout.preferredWidth: 40 }
                Text { text: modelData.qsos;      color: pal.windowText; font.family: "monospace"; font.bold: cur
                       Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
                Text { text: modelData.points;    color: pal.windowText; font.family: "monospace"; font.bold: cur
                       Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
                Text { text: modelData.zones;     color: pal.windowText; font.family: "monospace"; font.bold: cur
                       Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
                Text { text: modelData.countries; color: pal.windowText; font.family: "monospace"; font.bold: cur
                       Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
                Text { text: modelData.mults;     color: pal.windowText; font.family: "monospace"; font.bold: cur
                       Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: pal.mid }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text { text: "Total"; color: pal.windowText; font.bold: true; font.family: "monospace"
                   Layout.fillWidth: true; Layout.preferredWidth: 40 }
            Text { text: root.s.qsos || 0;        color: pal.windowText; font.bold: true; font.family: "monospace"
                   Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
            Text { text: root.s.points || 0;      color: pal.windowText; font.bold: true; font.family: "monospace"
                   Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
            Text { text: root.s.zones || 0;       color: pal.windowText; font.bold: true; font.family: "monospace"
                   Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
            Text { text: root.s.countries || 0;   color: pal.windowText; font.bold: true; font.family: "monospace"
                   Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
            Text { text: root.s.multipliers || 0; color: pal.windowText; font.bold: true; font.family: "monospace"
                   Layout.preferredWidth: 38; horizontalAlignment: Text.AlignRight }
        }

        // ---- Final score ------------------------------------------------
        Text { text: "Final score"; color: pal.windowText; Layout.topMargin: 6 }
        Text {
            text: root.fmt(root.s.total)
            color: pal.highlight
            font.pixelSize: 28
            font.bold: true
        }
        Text {
            Layout.fillWidth: true
            text: (root.s.points || 0) + " points × (" + (root.s.zones || 0) + " zones + " +
                  (root.s.countries || 0) + " countries)"
            color: pal.windowText
            opacity: 0.7
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: (root.s.errors || 0) > 0
            text: (root.s.errors || 0) + " QSO(s) with errors (dupes, bad exch…) not counted"
            color: pal.windowText
            opacity: 0.7
            wrapMode: Text.WordWrap
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: pal.mid; Layout.topMargin: 6 }

        // ---- Station being entered ----------------------------------------
        Text {
            text: root.hasEntry ? ("Station: " + contest.entryCall.toUpperCase()) : "Station: —"
            color: pal.windowText
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        GridLayout {
            Layout.fillWidth: true
            visible: root.hasEntry
            columns: 2
            columnSpacing: 8
            rowSpacing: 2

            Text { text: "Country"; color: pal.windowText; opacity: 0.7 }
            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: pal.windowText
                text: root.isMM(contest.entryCall) ? "Maritime mobile (zone only)"
                      : (root.cty.valid ? (root.cty.country + " (" + (root.cty.prefix || "") + ")") : "unknown")
            }

            Text { text: "Continent"; color: pal.windowText; opacity: 0.7 }
            Text { text: root.cty.valid ? (root.cty.cont || "?") : "?"; color: pal.windowText; font.family: "monospace" }

            Text { text: "CQ zone"; color: pal.windowText; opacity: 0.7 }
            Text { text: root.cty.valid ? (root.cty.cqz || "?") : "?"; color: pal.windowText; font.family: "monospace" }

            Text { text: "Points"; color: pal.windowText; opacity: 0.7 }
            Text {
                readonly property int p: root.entryPoints()
                text: p < 0 ? "? (1 assumed)" : String(p)
                color: p === 3 ? root.goodColor : pal.windowText
                font.bold: true
                font.family: "monospace"
            }

            Text { text: "Mult (" + (contest.currentBand || "") + ")"; color: pal.windowText; opacity: 0.7 }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                readonly property bool dupe: !!root.ci.dupe
                readonly property bool nz: !dupe && root.cty.valid && root.isNewZone()
                readonly property bool nc: !dupe && root.cty.valid && root.isNewCountry()
                text: !root.cty.valid ? "?"
                      : (nz && nc ? "NEW ZONE + NEW COUNTRY"
                         : nz ? "NEW ZONE" : nc ? "NEW COUNTRY" : "none")
                color: (nz || nc) ? root.goodColor : pal.windowText
                font.bold: nz || nc
            }

            Text { text: "Dupe"; color: pal.windowText; opacity: 0.7 }
            Text {
                text: root.ci.dupe ? "DUPE on " + (contest.currentBand || "") : "no"
                color: root.ci.dupe ? root.warnColor : pal.windowText
                font.bold: !!root.ci.dupe
            }

            Text { text: "Worked"; color: pal.windowText; opacity: 0.7 }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: (root.ci.workedBands || []).length > 0 ? (root.ci.workedBands || []).join(" ") : "—"
                color: pal.windowText
                font.family: "monospace"
            }
        }
    }
}
