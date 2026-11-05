#include "QsoChecks.h"

#include "ContestRules.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace QsoChecks {

bool isCallsign(const QString &call)
{
    // optional "XX/" prefix, prefix with a letter, a digit, suffix ending with a letter, optional "/X"
    static const QRegularExpression re(
        R"(^([A-Z0-9]{1,4}/)?([A-Z0-9]{1,3})([0-9])([A-Z0-9]{0,4}[A-Z])(/[A-Z0-9]{1,4})?$)");
    const auto m = re.match(stationCall(call));         // F5ABC/P/QRP is checked as F5ABC
    return m.hasMatch() && m.captured(2).contains(QRegularExpression("[A-Z]"));
}

QString stationCall(const QString &call)
{
    static const QStringList suffixes = {"P", "M", "MM", "AM", "QRP"};
    QString c = call.trimmed().toUpper();
    for (;;) {
        const int slash = c.lastIndexOf('/');
        if (slash <= 0 || !suffixes.contains(c.mid(slash + 1)))
            return c;
        c.truncate(slash);
    }
}

QDateTime BandRun::changeAllowedAt(const ContestRules &rules) const
{
    if (rules.bandChangeMinutes <= 0 || band.isEmpty())
        return QDateTime();
    return first.addSecs(qint64(rules.bandChangeMinutes) * 60);
}

static int periodIndex(const QList<QPair<QDateTime, QDateTime>> &periods, const QDateTime &t)
{
    for (int i = 0; i < periods.size(); ++i)
        if (t >= periods[i].first && t < periods[i].second)
            return i;
    return -1;
}

// Applies the band_change rule to one QSO (already known to be on an allowed band, in period).
// Returns false if the QSO changes band too early; otherwise updates the run.
static bool stepBandRun(const ContestRules &rules, BandRun &run, const Qso &q, int period)
{
    if (rules.bandChangeMinutes <= 0)
        return true;
    if (!run.band.isEmpty() && period != run.period)
        run = BandRun();                                   // new period: free choice of band
    if (run.band.isEmpty() || q.band == run.band) {
        if (run.band.isEmpty()) {
            run.band = q.band;
            run.first = q.utc;
            run.period = period;
        }
        run.last = q.utc;
        return true;
    }
    if (q.utc < run.changeAllowedAt(rules))
        return false;                                      // too early: still on the old band
    run.band = q.band;
    run.first = run.last = q.utc;
    run.period = period;
    return true;
}

static QVector<const Qso *> timeOrder(const QVector<Qso> &qsos)
{
    QVector<const Qso *> order;
    order.reserve(qsos.size());
    for (const Qso &q : qsos)
        order << &q;
    std::stable_sort(order.begin(), order.end(), [](const Qso *a, const Qso *b) {
        return a->utc < b->utc || (a->utc == b->utc && a->id < b->id);
    });
    return order;
}

BandRun bandRun(const ContestRules &rules, const ContestInfo &contest, const QVector<Qso> &qsos)
{
    BandRun r;
    const auto periods = rules.periodsUtc(contest.date);
    for (const Qso *q : timeOrder(qsos)) {
        const int p = periodIndex(periods, q->utc);
        if (rules.band(q->band) && rules.modes.contains(q->mode.toUpper()) && (periods.isEmpty() || p >= 0))
            stepBandRun(rules, r, *q, p);
    }
    return r;
}

QHash<int, QString> run(const ContestRules &rules, const ContestInfo &contest, const QVector<Qso> &qsos)
{
    QHash<int, QString> err;
    const auto periods = rules.periodsUtc(contest.date);
    const QRegularExpression pattern(rules.exchRcvdPattern.isEmpty()
                                         ? QString()
                                         : QRegularExpression::anchoredPattern(rules.exchRcvdPattern),
                                     QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression number("^[0-9]+$");

    QSet<QString> worked;
    BandRun bandRun;
    for (const Qso *q : timeOrder(qsos)) {
        QString e;
        const QString exch = q->exchRcvd.trimmed();
        if (!rules.band(q->band))
            e = "BAND NOT ALLOWED";
        else if (!rules.modes.contains(q->mode.toUpper()))
            e = "MODE NOT ALLOWED";
        else if (!periods.isEmpty() && periodIndex(periods, q->utc) < 0)
            e = "OUT OF PERIOD";
        else if (!stepBandRun(rules, bandRun, *q, periodIndex(periods, q->utc)))
            e = "BAND CHANGE";
        else if (!isCallsign(q->call))
            e = "BAD CALL";
        else if (exch.isEmpty() && rules.exchRcvdRequired)
            e = "NO EXCH";
        else if (!exch.isEmpty()
                 && ((rules.exchRcvdType == "number" && !number.match(exch).hasMatch())
                     || (rules.exchRcvdType == "callsign" && !isCallsign(exch))
                     || (!rules.exchRcvdPattern.isEmpty() && !pattern.match(exch).hasMatch())))
            e = "BAD EXCH";

        if (e.isEmpty() && rules.dupeRule != "none") {
            QString key = stationCall(q->call);
            if (rules.dupeRule == "band")
                key = q->band + '|' + key;
            else if (rules.dupeRule == "band_mode")
                key = q->band + '|' + q->mode + '|' + key;
            if (worked.contains(key))
                e = "DUPE";
            else
                worked.insert(key);
        }
        err.insert(q->id, e);
    }
    return err;
}

} // namespace QsoChecks
