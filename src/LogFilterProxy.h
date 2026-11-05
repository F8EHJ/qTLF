#pragma once

#include <QList>
#include <QSortFilterProxyModel>

// "Smart" search over the log.
//  - plain words match any field:          HA6   7025   2026-11-01   dupe
//  - field:value (wildcards * and ?)        call:HA*   band:40   exch:YT7MM   comment:check
//  - comparisons                            freq>7020  freq<=3530  time>=06:30  date=2026-11-01
//  - flags                                  tagged   is:dupe   is:invalid   is:ok
//  - prefix with "-" to exclude             -band:80m   -is:dupe
// All terms must match (AND).
class LogFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit LogFilterProxy(QObject *parent = nullptr);
    void setQuery(const QString &query);
    QString query() const { return m_query; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    struct Term {
        QString key;     // empty = any field
        QString op;      // ":" "=" ">" "<" ">=" "<="
        QString value;
        bool negate = false;
        bool wildcard = false;
    };
    bool matches(const Term &t, int row) const;

    QString m_query;
    QList<Term> m_terms;
};
