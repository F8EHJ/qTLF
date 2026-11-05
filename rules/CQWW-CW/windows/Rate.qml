import QtQuick
import QtQuick.Layouts

// CQ WW CW - Rate window
// Curves of QSOs per hour over the whole contest period.
// Buttons: Current (band selected on the entry line), one per contest band, All.
// Each button shows / hides its curve (several can be on at the same time).
// The table gives, for each curve shown: QSOs in the last 60 min, this clock hour,
// average QSO/h since the start, best hour and total.
Rectangle {
    id: root
    color: pal.window
    implicitWidth: 340
    implicitHeight: 300

    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    readonly property var s: contest.score || ({})
    readonly property var rate: s.rate || ({})
    readonly property int nHours: rate.hours || 48
    readonly property var bandNames: (contest.info || {}).bands || []

    // Series keys: "current", band names ("160m"…), "all"
    readonly property var keys: ["current"].concat(bandNames).concat(["all"])
    property var selected: ({ "current": true, "all": true })
    property var stats: []

    readonly property var bandColors: ({
        "160m": "#8e44ad", "80m": "#c0392b", "40m": "#e67e22",
        "20m": "#27ae60", "15m": "#2980b9", "10m": "#16a085"
    })

    function label(key) {
        if (key === "current") return "Current";
        if (key === "all") return "All";
        return key;
    }

    function colorOf(key) {
        if (key === "current") return pal.highlight;
        if (key === "all") return pal.windowText;
        return bandColors[key] || pal.mid;
    }

    function rgba(c, a) {
        var col = Qt.lighter(c, 1.0);       // accepts a color or a "#rrggbb" string
        return "rgba(" + Math.round(col.r * 255) + "," + Math.round(col.g * 255) + "," +
               Math.round(col.b * 255) + "," + a + ")";
    }

    function dataOf(key) {
        var src;
        if (key === "all") src = rate.all || [];
        else if (key === "current") src = (rate.perBand || {})[contest.currentBand] || [];
        else src = (rate.perBand || {})[key] || [];
        var out = [];
        for (var i = 0; i < nHours; i++) out.push(src[i] || 0);
        return out;
    }

    function toggle(key) {
        var n = {};
        for (var k in selected) n[k] = selected[k];
        n[key] = !n[key];
        selected = n;
        recompute();
    }

    function recompute() {
        var st = Date.parse(rate.start || "");
        var list = contest.qsos || [];

        var lastMs = NaN;
        for (var k = list.length - 1; k >= 0; k--)
            if (!list[k].error) { lastMs = Date.parse(list[k].datetime); break; }

        // QSOs of the last 60 minutes, per band
        var l60 = {}, l60all = 0;
        if (!isNaN(lastMs)) {
            for (var j = list.length - 1; j >= 0; j--) {
                var q = list[j];
                var t = Date.parse(q.datetime);
                if (isNaN(t) || lastMs - t >= 3600000) break;
                if (q.error) continue;
                l60[q.band] = (l60[q.band] || 0) + 1;
                l60all++;
            }
        }

        var hi = (isNaN(lastMs) || isNaN(st)) ? -1 : Math.floor((lastMs - st) / 3600000);
        var elapsed = hi >= 0 ? Math.min(hi + 1, nHours) : 1;

        var out = [];
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i];
            if (!selected[key]) continue;
            var d = dataOf(key), tot = 0, best = 0;
            for (var h = 0; h < nHours; h++) {
                tot += d[h];
                if (d[h] > best) best = d[h];
            }
            var last = key === "all" ? l60all
                     : key === "current" ? (l60[contest.currentBand] || 0)
                     : (l60[key] || 0);
            out.push({ key: key,
                       name: key === "current" ? "Cur " + (contest.currentBand || "") : label(key),
                       last60: last,
                       thisHour: (hi >= 0 && hi < nHours) ? d[hi] : 0,
                       avg: (tot / elapsed).toFixed(1),
                       best: best, total: tot });
        }
        stats = out;
        chart.requestPaint();
    }

    Connections {
        target: contest
        function onUpdated() { root.recompute() }
        function onCurrentBandChanged() { root.recompute() }
    }
    Component.onCompleted: recompute()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: "Rate — QSOs per hour"
            color: pal.windowText
            font.bold: true
            elide: Text.ElideRight
        }

        // ---- Band buttons -------------------------------------------------
        Flow {
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: root.keys
                delegate: Rectangle {
                    id: btn
                    required property var modelData
                    readonly property bool on: !!root.selected[modelData]
                    readonly property color c: root.colorOf(modelData)
                    width: btnText.implicitWidth + 16
                    height: btnText.implicitHeight + 8
                    radius: 4
                    color: "transparent"
                    border.width: on ? 2 : 1
                    border.color: on ? pal.windowText : pal.mid
                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: btn.c
                            visible: btn.on
                        }
                        Text {
                            id: btnText
                            text: btn.modelData === "current"
                                  ? "Current" + (contest.currentBand ? " (" + contest.currentBand + ")" : "")
                                  : root.label(btn.modelData)
                            color: pal.windowText
                            opacity: btn.on ? 1 : 0.6
                            font.pixelSize: 11
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toggle(btn.modelData)
                    }
                }
            }
        }

        // ---- Table for the curves shown --------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: root.stats.length > 0
            Repeater {
                model: ["Q/h", "60min", "Hour", "Avg", "Best", "Total"]
                delegate: Text {
                    required property var modelData
                    required property int index
                    text: modelData
                    color: pal.windowText
                    opacity: 0.7
                    font.pixelSize: 11
                    Layout.fillWidth: index === 0
                    Layout.preferredWidth: index === 0 ? 70 : 36
                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                }
            }
        }
        Repeater {
            model: root.stats
            delegate: RowLayout {
                id: row
                required property var modelData
                Layout.fillWidth: true
                spacing: 4
                readonly property bool isAll: modelData.key === "all"
                Item {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 70
                    implicitHeight: nm.implicitHeight
                    Rectangle {
                        id: sw
                        width: 10; height: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.colorOf(row.modelData.key)
                    }
                    Text {
                        id: nm
                        anchors.left: sw.right; anchors.leftMargin: 4; anchors.right: parent.right
                        text: row.modelData.name
                        color: pal.windowText
                        font.bold: row.isAll
                        elide: Text.ElideRight
                    }
                }
                Text { text: row.modelData.last60;   color: pal.windowText; font.family: "monospace"; font.bold: row.isAll
                       Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
                Text { text: row.modelData.thisHour; color: pal.windowText; font.family: "monospace"; font.bold: row.isAll
                       Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
                Text { text: row.modelData.avg;      color: pal.windowText; font.family: "monospace"; font.bold: row.isAll
                       Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
                Text { text: row.modelData.best;     color: pal.windowText; font.family: "monospace"; font.bold: row.isAll
                       Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
                Text { text: row.modelData.total;    color: pal.windowText; font.family: "monospace"; font.bold: row.isAll
                       Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.stats.length === 0
            text: "Select a band (or All) to show its curve."
            color: pal.windowText
            opacity: 0.7
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: !root.rate.start
            text: "No contest period defined: no rate data."
            color: pal.windowText
            opacity: 0.7
            wrapMode: Text.WordWrap
        }

        // ---- Chart ----------------------------------------------------------
        Canvas {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            // Curve through the middle of each hour
            function drawCurve(ctx, data, left, top, cw, ch, ymax, stroke, width, fill) {
                var n = data.length, bw = cw / n;
                function px(i) { return left + (i + 0.5) * bw; }
                function py(v) { return top + ch - Math.min(v, ymax) / ymax * ch; }
                if (fill) {
                    ctx.beginPath();
                    ctx.moveTo(px(0), top + ch);
                    for (var i = 0; i < n; i++) ctx.lineTo(px(i), py(data[i]));
                    ctx.lineTo(px(n - 1), top + ch);
                    ctx.closePath();
                    ctx.fillStyle = fill;
                    ctx.fill();
                }
                ctx.beginPath();
                for (var k = 0; k < n; k++) {
                    if (k === 0) ctx.moveTo(px(k), py(data[k]));
                    else ctx.lineTo(px(k), py(data[k]));
                }
                ctx.strokeStyle = stroke;
                ctx.lineWidth = width;
                ctx.lineJoin = "round";
                ctx.stroke();
            }

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var W = width, H = height;
                var left = 26, bottom = 16, top = 4, right = 4;
                var cw = W - left - right, ch = H - top - bottom;
                if (cw <= 10 || ch <= 10) return;

                var n = root.nHours;
                var series = [];
                var max = 0;
                for (var i = 0; i < root.keys.length; i++) {
                    var key = root.keys[i];
                    if (!root.selected[key]) continue;
                    var d = root.dataOf(key);
                    for (var h = 0; h < n; h++) if (d[h] > max) max = d[h];
                    series.push({ key: key, data: d });
                }
                var step = max <= 10 ? 2 : max <= 50 ? 10 : max <= 100 ? 20 : max <= 250 ? 50 : 100;
                var ymax = Math.max(step, Math.ceil(max / step) * step);

                var txt = root.rgba(pal.windowText, 0.75);
                ctx.font = "9px monospace";
                ctx.lineWidth = 1;

                // Grid + y labels (QSO/h)
                ctx.textAlign = "right";
                for (var y = 0; y <= ymax; y += step) {
                    var gy = top + ch - (y / ymax) * ch;
                    ctx.strokeStyle = root.rgba(pal.windowText, 0.12);
                    ctx.beginPath(); ctx.moveTo(left, gy + 0.5); ctx.lineTo(left + cw, gy + 0.5); ctx.stroke();
                    ctx.fillStyle = txt;
                    ctx.fillText(String(y), left - 3, gy + 3);
                }

                // X axis: midnight separator, labels every 6 h (UTC)
                var bw = cw / n;
                var st = Date.parse(root.rate.start || "");
                var h0 = isNaN(st) ? 0 : new Date(st).getUTCHours();
                ctx.textAlign = "center";
                for (var t = 0; t <= n; t++) {
                    var hod = (h0 + t) % 24, gx = left + t * bw;
                    if (hod === 0 && t > 0 && t < n) {
                        ctx.strokeStyle = root.rgba(pal.windowText, 0.35);
                        ctx.lineWidth = 1;
                        ctx.beginPath(); ctx.moveTo(gx + 0.5, top); ctx.lineTo(gx + 0.5, top + ch); ctx.stroke();
                    }
                    if (hod % 6 === 0 && t < n) {
                        ctx.fillStyle = txt;
                        ctx.fillText((hod < 10 ? "0" : "") + hod, gx, H - 4);
                    }
                }

                // Draw "all" first (filled), bands next, "current" last (on top)
                var order = ["all"];
                for (var b = 0; b < root.bandNames.length; b++) order.push(root.bandNames[b]);
                order.push("current");
                for (var o = 0; o < order.length; o++) {
                    for (var s2 = 0; s2 < series.length; s2++) {
                        if (series[s2].key !== order[o]) continue;
                        var k2 = series[s2].key;
                        var c = root.colorOf(k2);
                        if (k2 === "all")
                            drawCurve(ctx, series[s2].data, left, top, cw, ch, ymax,
                                      root.rgba(c, 0.9), 2, root.rgba(c, 0.07));
                        else if (k2 === "current")
                            drawCurve(ctx, series[s2].data, left, top, cw, ch, ymax,
                                      root.rgba(c, 1), 2.5, root.rgba(c, 0.15));
                        else
                            drawCurve(ctx, series[s2].data, left, top, cw, ch, ymax,
                                      root.rgba(c, 1), 1.5, "");
                    }
                }

                // Axis line
                ctx.strokeStyle = root.rgba(pal.windowText, 0.5);
                ctx.lineWidth = 1;
                ctx.beginPath(); ctx.moveTo(left, top + ch + 0.5); ctx.lineTo(left + cw, top + ch + 0.5); ctx.stroke();
            }
        }
    }
}
