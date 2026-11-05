// Generic contest: 1 point per valid QSO, no multipliers.
// qTLF has already checked band, mode, period, callsign, number received and dupes
// (q.error is "" for a valid QSO). See docs/RULES.md.
function score(qsos, contest) {
    var perBand = qtlf.perBand(contest, function () { return 0; });
    var points = 0;

    qsos.forEach(function (q) {
        if (q.error) return;
        points++;
        perBand[q.band]++;
    });

    return {
        points: points,
        total: points,
        bands: contest.bands.map(function (b) { return { band: b, points: perBand[b] }; })
    };
}
