// CQ WW DX Contest - CW - scoring for qTLF
//
// QSO points (rules IV.B):
//   different continent                               3
//   same continent, different country                 1
//   same continent, different country, both in NA     2
//   same country                                      0 (still counts as multiplier)
// Multipliers (rules IV.C), counted on each band:
//   CQ zones   : zone RECEIVED in the exchange (1-40)
//   countries  : DXCC + WAE entity from cty.dat; maritime mobile (/MM) = zone only
// Score = total QSO points x (zones + countries)
//
// Single-band entry (Cabrillo CATEGORY-BAND != ALL): only QSOs on that band are
// scored; the others stay in the log with the status "NOT SCORED" (rules X.2).
//
// Without cty.dat (contest.ctyLoaded == false) or for an unknown prefix:
//   continent unknown -> 1 point (same continent, different country), no country
//   multiplier; counted in "unknownCont" so the windows can warn the operator.

function isMaritimeMobile(call) {
    return /\/MM(\/|$)/i.test(call || "");
}

// Points for a contact, given my station and the other station's cty data.
// Exported to the windows through "rules" in the result (see below).
function qsoPoints(myCont, myCountry, cont, country) {
    if (!myCont || !cont) return 1;                    // unknown: default rule
    if (myCountry && country && myCountry === country) return 0;
    if (myCont !== cont) return 3;
    return myCont === "NA" ? 2 : 1;
}

function pad2(n) {
    return (n < 10 ? "0" : "") + n;
}

function score(qsos, contest) {
    var my = contest.my || {};
    var myCont = (contest.ctyLoaded && my.valid) ? (my.cont || "") : "";
    var myCountry = (contest.ctyLoaded && my.valid) ? (my.country || "") : "";

    // Single-band category: "160M" -> "160m"
    var catBand = ((contest.fields || {})["CATEGORY-BAND"] || "ALL").toUpperCase();
    var onlyBand = (catBand === "ALL" || catBand === "") ? "" : catBand.toLowerCase();

    var bands = qtlf.perBand(contest, function (b) {
        return { band: b, qsos: 0, valid: 0, points: 0, zones: {}, countries: {} };
    });

    // Rate data: valid QSOs per hour since the contest start
    var startMs = contest.start ? Date.parse(contest.start) : NaN;
    var endMs = contest.end ? Date.parse(contest.end) : NaN;
    var nHours = (!isNaN(startMs) && !isNaN(endMs) && endMs > startMs)
                 ? Math.ceil((endMs - startMs) / 3600000) : 48;
    var rateAll = [], rateBand = {};
    var h;
    for (h = 0; h < nHours; h++) rateAll.push(0);
    contest.bands.forEach(function (name) {
        var a = [];
        for (var i = 0; i < nHours; i++) a.push(0);
        rateBand[name] = a;
    });

    var status = {};
    var points = 0, errors = 0, unknownCont = 0, notScored = 0, zeroPts = 0;
    var lastQso = null;

    qsos.forEach(function (q) {
        var b = bands[q.band];
        if (!b) return;
        b.qsos++;
        if (q.error) { errors++; return; }             // already checked by qTLF

        // Rate (all valid QSOs, scored or not)
        if (!isNaN(startMs)) {
            var t = Date.parse(q.datetime);
            if (!isNaN(t)) {
                h = Math.floor((t - startMs) / 3600000);
                if (h >= 0 && h < nHours) {
                    rateAll[h]++;
                    rateBand[q.band][h]++;
                }
            }
        }

        if (onlyBand && q.band !== onlyBand) {
            status[q.id] = "NOT SCORED";
            notScored++;
            return;
        }

        b.valid++;
        var cont = contest.ctyLoaded ? (q.cont || "") : "";
        var country = contest.ctyLoaded ? (q.country || "") : "";
        var mm = isMaritimeMobile(q.call);
        if (!cont || !myCont) unknownCont++;

        var pts = qsoPoints(myCont, mm ? "" : myCountry, cont, mm ? "" : country);
        if (pts === 0) zeroPts++;
        b.points += pts;
        points += pts;

        var newZone = false, newCountry = false;
        var z = parseInt(q.exch_r, 10);
        if (z >= 1 && z <= 40) {
            var zk = pad2(z);
            if (!b.zones[zk]) { b.zones[zk] = true; newZone = true; }
        }
        if (!mm && country) {
            if (!b.countries[country]) { b.countries[country] = true; newCountry = true; }
        }
        lastQso = { call: q.call, band: q.band, points: pts, cont: cont,
                    country: mm ? "Maritime mobile" : country,
                    zone: (z >= 1 && z <= 40) ? z : 0,
                    newZone: newZone, newCountry: newCountry };
    });

    // Operating time (for the CLASSIC overlay: max 24 h, off times >= 60 min).
    // Every logged QSO counts (dupes included). A gap of less than 60 min between
    // two QSOs is operating time; a gap of 60 min or more is an off time.
    // An off time is only measured BETWEEN two logged QSOs (not from the contest start).
    var opMinutes = 0, prevMs = NaN, offTimes = 0, offMinutes = 0;
    qsos.forEach(function (q) {
        var t = Date.parse(q.datetime);
        if (isNaN(t)) return;
        if (isNaN(prevMs)) {
            opMinutes += 1;
        } else {
            var gap = (t - prevMs) / 60000;
            if (gap < 60) opMinutes += gap;
            else { opMinutes += 1; offTimes++; offMinutes += gap; }
        }
        prevMs = t;
    });
    opMinutes = Math.round(opMinutes);
    offMinutes = Math.round(offMinutes);

    var list = [], totZones = 0, totCountries = 0, totQsos = 0;
    contest.bands.forEach(function (name) {
        var b = bands[name];
        var nz = qtlf.count(b.zones), nc = qtlf.count(b.countries);
        totZones += nz;
        totCountries += nc;
        totQsos += b.valid;
        list.push({ band: name, qsos: b.valid, logged: b.qsos, points: b.points,
                    zones: nz, countries: nc, mults: nz + nc,
                    zoneList: Object.keys(b.zones).sort(),
                    countryList: Object.keys(b.countries).sort() });
    });

    var mults = totZones + totCountries;
    var total = points * mults;

    return {
        total: total,
        points: points,
        multipliers: mults,
        summary: points + " pts x (" + totZones + " zones + " + totCountries +
                 " ctys) = " + total,
        status: status,

        // Free keys for the windows
        bands: list,
        qsos: totQsos,
        zones: totZones,
        countries: totCountries,
        errors: errors,
        notScored: notScored,
        zeroPts: zeroPts,
        unknownCont: unknownCont,
        myCont: myCont,
        myCountry: myCountry,
        singleBand: onlyBand,
        opMinutes: opMinutes,
        offTimes: offTimes,
        offMinutes: offMinutes,
        lastQso: lastQso,
        rate: { start: contest.start || "", hours: nHours, all: rateAll, perBand: rateBand }
    };
}
