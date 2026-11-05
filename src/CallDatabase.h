#pragma once

#include <QHash>
#include <QStringList>
#include <QVariantMap>

struct CsvFileDef;

// The "call_exchange" CSV file of a contest: one line per known station, first column = call,
// one column = the exchange it sends ("exchange_field"), other columns free (name, type…).
// e.g. a club member list  CALL;NUMBER;NAME;TYPE
// Used for: Call completion (calls), exchange completion (exchange column), exchange pre-fill,
// and every field of the record given to scoring.js (q.db) and to QML (contest.callInfo).
class CallDatabase
{
public:
    bool load(const QString &path, const CsvFileDef &def, const QString &exchangeField,
              QString *error = nullptr);
    void clear() { m_records.clear(); m_calls.clear(); }
    bool isEmpty() const { return m_records.isEmpty(); }
    int size() const { return m_records.size(); }

    bool contains(const QString &call) const { return find(call) != m_records.constEnd(); }  // F5ABC/P finds F5ABC
    QVariantMap record(const QString &call) const;       // {call, exch, name, ...} or empty
    QString exchange(const QString &call) const;          // value of the exchange field
    QStringList calls() const { return m_calls; }        // sorted
    QStringList exchangeValues() const;                  // distinct values of the exchange column, sorted
    QHash<QString, QString> exchangeTable() const;

private:
    QHash<QString, QStringList>::const_iterator find(const QString &call) const;
    QStringList m_fields;
    int m_exchIndex = 1;
    QHash<QString, QStringList> m_records;   // CALL -> fields
    QStringList m_calls;                     // sorted
};
