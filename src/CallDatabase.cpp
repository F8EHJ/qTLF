#include "CallDatabase.h"

#include "QsoChecks.h"

#include "ContestRules.h"

bool CallDatabase::load(const QString &path, const CsvFileDef &def, const QString &exchangeField,
                        QString *error)
{
    clear();
    m_fields = def.fields;
    m_exchIndex = qMax(0, int(m_fields.indexOf(exchangeField)));
    bool ok = false;
    const QList<QStringList> rows = ContestRules::readCsv(path, def, &ok);
    if (!ok) {
        if (error) *error = QString("cannot open %1").arg(path);
        return false;
    }
    for (QStringList f : rows) {
        const QString call = f.value(0).toUpper();
        if (call.isEmpty() || m_records.contains(call))
            continue;
        f[0] = call;
        m_records.insert(call, f);
    }
    m_calls = m_records.keys();
    m_calls.sort();
    return true;
}

// exact call first (a list may contain F5ABC/P on its own), then without /P /M /MM…
QHash<QString, QStringList>::const_iterator CallDatabase::find(const QString &call) const
{
    auto it = m_records.constFind(call.trimmed().toUpper());
    if (it == m_records.constEnd())
        it = m_records.constFind(QsoChecks::stationCall(call));
    return it;
}

QVariantMap CallDatabase::record(const QString &call) const
{
    QVariantMap m;
    const auto it = find(call);
    if (it == m_records.constEnd())
        return m;
    const QStringList &f = it.value();
    for (int i = 0; i < f.size(); ++i)
        m.insert(i < m_fields.size() && !m_fields[i].isEmpty() ? m_fields[i] : QString("field%1").arg(i + 1), f[i]);
    return m;
}

QString CallDatabase::exchange(const QString &call) const
{
    const auto it = find(call);
    return it == m_records.constEnd() ? QString() : it.value().value(m_exchIndex).toUpper();
}

QHash<QString, QString> CallDatabase::exchangeTable() const
{
    QHash<QString, QString> h;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        const QString x = it.value().value(m_exchIndex).toUpper();
        if (!x.isEmpty())
            h.insert(it.key(), x);
    }
    return h;
}

QStringList CallDatabase::exchangeValues() const
{
    QStringList out;
    for (auto it = m_records.constBegin(); it != m_records.constEnd(); ++it) {
        const QString v = it.value().value(m_exchIndex).trimmed().toUpper();
        if (!v.isEmpty())
            out << v;
    }
    out.removeDuplicates();
    out.sort();
    return out;
}
