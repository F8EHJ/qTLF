// =====================================================================
//  Silent Key Memorial Contest (SKMC) - scoring for qTLF
//  https://skmc.hu/en/rules.html
//
//  qTLF already checked every QSO (band, mode, period, callsign, the SK call
//  received is a callsign, dupes per band): q.error is "" for a valid QSO.
//  Points:      every valid QSO = 1 point
//  Multipliers: different Silent Key calls received, per band
//  Final score: points x multipliers
// =====================================================================

function score(qsos, contest) {
    var bands = qtlf.perBand(contest, function (b) {
        return { band: b, qsos: 0, points: 0, dupes: 0, mults: {} };
    });
    var points = 0;

    qsos.forEach(function (q) {
        var b = bands[q.band];
        if (!b) return;
        b.qsos++;
        if (q.error) {
            if (q.error === "DUPE") b.dupes++;
            return;
        }
        b.points += 1;
        points += 1;
        b.mults[q.exch_r.toUpperCase()] = true;   // same SK call counts once per band
    });

    var list = [], multipliers = 0;
    contest.bands.forEach(function (name) {
        var b = bands[name];
        var calls = Object.keys(b.mults).sort();
        multipliers += calls.length;
        list.push({ band: name, qsos: b.qsos, points: b.points, dupes: b.dupes,
                    mults: calls.length, multList: calls });
    });

    return {
        points: points,
        multipliers: multipliers,
        total: points * multipliers,
        bands: list
    };
}
