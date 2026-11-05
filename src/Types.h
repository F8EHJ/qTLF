#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

// One contest instance = a set of rules (folder in rules/) + a date + station data.
struct ContestInfo
{
    int id = 0;
    QString rulesId;      // folder name in rules/, e.g. "SKM"
    QString name;         // e.g. "SKMC 2026"
    QDate date;           // contest date (start day)
    QString myCall;
    QString category;
    QString exchSent;     // fixed exchange (when rules say exchange_sent.type == "fixed")
    QVariantMap fields;   // contest-specific Cabrillo fields (cabrillo.fields), tag -> value
    QDateTime created;
    int qsoCount = 0;
};

struct Qso
{
    int id = 0;
    int contestId = 0;
    QDateTime utc;        // always UTC
    QString band;
    double freqKhz = 0;   // 0 = unknown (band only)
    QString mode;
    QString call;
    QString rstSent;
    QString rstRcvd;
    QString exchSent;
    QString exchRcvd;
    bool tagged = false;
    QString comment;
};

inline QVariantMap qsoToVariant(const Qso &q, const QString &status = QString())
{
    QVariantMap m;
    m["id"] = q.id;
    m["datetime"] = q.utc.toString(Qt::ISODate);
    m["date"] = q.utc.date().toString("yyyy-MM-dd");
    m["time"] = q.utc.time().toString("HH:mm:ss");
    m["band"] = q.band;
    m["freq"] = q.freqKhz;
    m["mode"] = q.mode;
    m["call"] = q.call;
    m["rst_s"] = q.rstSent;
    m["rst_r"] = q.rstRcvd;
    m["exch_s"] = q.exchSent;
    m["exch_r"] = q.exchRcvd;
    m["tagged"] = q.tagged;
    m["comment"] = q.comment;
    if (!status.isNull())
        m["status"] = status;
    return m;
}
