#pragma once

#include "Types.h"

#include <QHash>
#include <QString>
#include <QVector>

class ContestRules;

// Checks that are the same for every contest, done by qTLF before scoring.js runs.
// Each QSO gets an error text, empty when the QSO is valid:
//   BAND NOT ALLOWED   band not in "bands"
//   MODE NOT ALLOWED   mode not in "modes"
//   OUT OF PERIOD      outside every schedule.periods window (when periods are defined)
//   BAD CALL           not a valid callsign
//   NO EXCH            exchange.received.required and the exchange is empty
//   BAD EXCH           exchange.received.type (number / callsign) or .pattern not matched
//   BAND CHANGE        band changed less than band_change.min_minutes after the first QSO
//                      made on the current band
//   DUPE               same station already worked (rule "dupe"), counted on valid QSOs only;
//                      F5ABC, F5ABC/P, F5ABC/M, F5ABC/MM are the same station (stationCall)
namespace QsoChecks {

bool isCallsign(const QString &call);

// Call used to recognise a station: upper case, without the operating suffixes
// /P /M /MM /AM /QRP (F5ABC/P -> F5ABC). Prefixes (EA8/F5ABC) and call areas (W1ABC/6) are kept.
// Used for dupes, worked bands and call database lookups; the log and scoring.js keep the full call.
QString stationCall(const QString &call);

// Band currently "held" by the operator under the band_change rule, after the given QSOs.
struct BandRun {
    QString band;          // "" = no QSO yet (or new period: free choice of band)
    QDateTime first;       // first QSO of this band run
    QDateTime last;        // last QSO of this band run
    int period = -1;       // index of the period of the run
    // earliest time a QSO on another band is accepted (invalid if no rule / no run)
    QDateTime changeAllowedAt(const ContestRules &rules) const;
};
BandRun bandRun(const ContestRules &rules, const ContestInfo &contest, const QVector<Qso> &qsos);

// qso id -> error ("" = valid). QSOs are taken in time order for the dupe check.
QHash<int, QString> run(const ContestRules &rules, const ContestInfo &contest, const QVector<Qso> &qsos);

}
