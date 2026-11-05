// =====================================================================
//  Rencontres UFT (Union Française des Télégraphistes) - scoring for qTLF
//
//  qTLF already checked every QSO (band, mode, the 3 periods, callsign,
//  exchange = number or NM, dupes per band): q.error is "" for a valid QSO.
//
//  Points:  F8UFT (official station)            20
//           UFT member, same continent             5
//           UFT member, other continent (DX)      10
//           non-member (NM), same continent        1
//           non-member (NM), DX                    2
//  Multipliers: each UFT member worked counts 1 per band (F8UFT too).
//  Final score = total points x total multipliers.
//  Continents: q.cont (station worked) and contest.my.cont (me), from cty.dat.
// =====================================================================

var OFFICIAL = "F8UFT";

function score(qsos, contest) {
    var myCont = contest.my.cont || "";
    var bands = qtlf.perBand(contest, function (b) {
        return { band: b, qsos: 0, valid: 0, points: 0, members: 0, nm: 0, mults: {}, conts: {} };
    });
    var allConts = {};
    var points = 0, unknownCont = 0;

    qsos.forEach(function (q) {
        var b = bands[q.band];
        if (!b) return;
        b.qsos++;
        if (q.error) return;

        var call = qtlf.baseCall(q.call);
        var official = call === OFFICIAL;
        var member = q.exch_r.toUpperCase() !== "NM";
        var cont = q.cont || "";
        var dx = cont !== "" && myCont !== "" && cont !== myCont;
        if (cont === "" || myCont === "") unknownCont++;        // counted as same continent

        var pts = official ? 20 : member ? (dx ? 10 : 5) : (dx ? 2 : 1);
        b.points += pts;
        b.valid++;
        points += pts;
        if (member || official) {
            b.members++;
            b.mults[call] = true;                              // 1 multiplier per member per band
        } else {
            b.nm++;
        }
        var c = cont || "?";
        b.conts[c] = (b.conts[c] || 0) + 1;
        allConts[c] = (allConts[c] || 0) + 1;
    });

    var list = [], multipliers = 0, valid = 0;
    contest.bands.forEach(function (name) {
        var b = bands[name];
        var mults = qtlf.count(b.mults);
        multipliers += mults;
        valid += b.valid;
        list.push({ band: name, qsos: b.qsos, valid: b.valid, points: b.points, mults: mults,
                    members: b.members, nm: b.nm,
                    continents: Object.keys(b.conts).sort(), contCounts: b.conts });
    });

    return {
        points: points,
        multipliers: multipliers,
        total: points * multipliers,
        valid: valid,
        bands: list,
        continents: Object.keys(allConts).sort(),
        contCounts: allConts,
        unknownCont: unknownCont
    };
}
