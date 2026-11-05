#include "ScoringEngine.h"

#include "ContestRules.h"

#include <QFile>
#include <QJSEngine>
#include <QJSValue>

ScoringEngine::ScoringEngine() = default;
ScoringEngine::~ScoringEngine() = default;

bool ScoringEngine::load(const QString &scriptPath, QString *error)
{
    m_loaded = false;
    m_engine = std::make_unique<QJSEngine>();
    m_engine->installExtensions(QJSEngine::ConsoleExtension);

    // helpers available to every scoring.js
    m_engine->evaluate(R"JS(
        var qtlf = {
            // true for a valid amateur callsign (same rule as the BAD CALL check of qTLF)
            isCallsign: function (s) {
                var m = /^([A-Z0-9]{1,4}\/)?([A-Z0-9]{1,3})([0-9])([A-Z0-9]{0,4}[A-Z])(\/[A-Z0-9]{1,4})?$/
                        .exec(String(s || "").toUpperCase().trim());
                return !!m && /[A-Z]/.test(m[2]);
            },
            // home call without portable prefix/suffix: EA8/F8EHJ, F8EHJ/P -> F8EHJ
            // F5ABC/P, F5ABC/M, F5ABC/MM -> F5ABC (same rule as the qTLF dupe check)
            stationCall: function (call) {
                var c = String(call || "").trim().toUpperCase(), m;
                while ((m = /^(.+)\/(P|M|MM|AM|QRP)$/.exec(c)) !== null) c = m[1];
                return c;
            },
            baseCall: function (call) {
                var parts = String(call || "").toUpperCase().split("/"), best = parts[0];
                for (var i = 1; i < parts.length; ++i)
                    if (parts[i].length > best.length) best = parts[i];
                return best;
            },
            // per band table: { "80m": init(), "40m": init(), ... }
            perBand: function (contest, init) {
                var t = {};
                contest.bands.forEach(function (b) { t[b] = init(b); });
                return t;
            },
            count: function (obj) { return Object.keys(obj).length; }
        };
    )JS");

    QFile f(scriptPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot open scoring script %1").arg(scriptPath);
        return false;
    }
    const QJSValue r = m_engine->evaluate(QString::fromUtf8(f.readAll()), scriptPath);
    if (r.isError()) {
        if (error)
            *error = QString("%1:%2: %3").arg(scriptPath, r.property("lineNumber").toString(), r.toString());
        return false;
    }
    if (!m_engine->globalObject().property("score").isCallable()) {
        if (error) *error = QString("%1: no score(qsos, contest) function").arg(scriptPath);
        return false;
    }
    m_loaded = true;
    return true;
}

QVariantMap ScoringEngine::compute(const QVariantList &qsos, const QVariantMap &contest)
{
    if (!m_loaded)
        return {{"error", "No scoring script loaded"}};

    QJSValue fn = m_engine->globalObject().property("score");
    const QJSValue result = fn.call({m_engine->toScriptValue(qsos), m_engine->toScriptValue(contest)});
    if (result.isError())
        return {{"error", QString("line %1: %2").arg(result.property("lineNumber").toString(), result.toString())}};
    if (!result.isObject())
        return {{"error", "score() must return an object"}};
    return result.toVariant().toMap();
}

QVariantMap ScoringEngine::contestToVariant(const ContestInfo &c, const ContestRules &r)
{
    QVariantMap m;
    m["id"] = c.id;
    m["name"] = c.name;
    m["rules"] = r.id;
    m["rulesName"] = r.name;
    m["shortName"] = r.shortName;
    m["date"] = c.date.toString("yyyy-MM-dd");
    m["mycall"] = c.myCall;
    m["category"] = c.category;
    m["exch_sent"] = c.exchSent;
    const QDateTime s = r.periodStartUtc(c.date), e = r.periodEndUtc(c.date);
    m["start"] = s.isValid() ? s.toString(Qt::ISODate) : QString();
    m["end"] = e.isValid() ? e.toString(Qt::ISODate) : QString();
    QVariantList periods;
    for (const auto &p : r.periodsUtc(c.date))
        periods << QVariantMap{{"start", p.first.toString(Qt::ISODate)}, {"end", p.second.toString(Qt::ISODate)}};
    m["periods"] = periods;              // [{start, end}] ISO UTC, end exclusive
    m["fields"] = c.fields;              // contest-specific Cabrillo fields (CLUB, ...)
    m["bands"] = r.bandNames();
    m["modes"] = r.modes;
    m["band_change"] = QVariantMap{{"min_minutes", r.bandChangeMinutes}};
    return m;
}
