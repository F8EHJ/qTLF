#include "LogFilterProxy.h"

#include "QsoLogModel.h"

#include <QRegularExpression>

LogFilterProxy::LogFilterProxy(QObject *parent) : QSortFilterProxyModel(parent)
{
    setSortRole(QsoLogModel::SortRole);
}

static QString canonicalKey(const QString &k)
{
    static const QHash<QString, QString> alias = {
        {"call", "call"}, {"c", "call"},
        {"band", "band"}, {"b", "band"},
        {"freq", "freq"}, {"f", "freq"}, {"qrg", "freq"},
        {"mode", "mode"}, {"m", "mode"},
        {"date", "date"}, {"d", "date"},
        {"time", "time"}, {"t", "time"}, {"utc", "time"},
        {"rst", "rst"}, {"rsts", "rsts"}, {"rstr", "rstr"},
        {"exch", "exch"}, {"x", "exch"},
        {"exchs", "exchs"}, {"sent", "exchs"},
        {"exchr", "exchr"}, {"rcvd", "exchr"}, {"sk", "exchr"},
        {"comment", "comment"}, {"note", "comment"},
        {"status", "status"}, {"s", "status"},
        {"is", "is"}, {"nr", "nr"},
    };
    return alias.value(k.toLower());
}

static QString normTime(QString v)
{
    v.remove(':');
    if (v.size() == 4 || v.size() == 6)
        return v.left(2) + ":" + v.mid(2, 2);
    return v;
}

void LogFilterProxy::setQuery(const QString &query)
{
    m_query = query;
    m_terms.clear();
    static const QRegularExpression tokRe(R"(("[^"]*"|\S+))");
    static const QRegularExpression cmpRe(R"(^([a-zA-Z]+)(>=|<=|=|>|<|:)(.*)$)");
    auto it = tokRe.globalMatch(query);
    while (it.hasNext()) {
        QString tok = it.next().captured(1);
        Term t;
        if (tok.startsWith('-') && tok.size() > 1) {
            t.negate = true;
            tok = tok.mid(1);
        }
        tok.remove('"');
        if (tok.isEmpty())
            continue;
        const auto m = cmpRe.match(tok);
        if (m.hasMatch() && !canonicalKey(m.captured(1)).isEmpty()) {
            t.key = canonicalKey(m.captured(1));
            t.op = m.captured(2);
            t.value = m.captured(3);
        } else if (tok.compare("tagged", Qt::CaseInsensitive) == 0) {
            t.key = "is";
            t.op = ":";
            t.value = "tagged";
        } else {
            t.op = ":";
            t.value = tok;
        }
        t.wildcard = t.value.contains('*') || t.value.contains('?');
        m_terms << t;
    }
    invalidateFilter();
}

bool LogFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    for (const Term &t : m_terms) {
        if (matches(t, sourceRow) == t.negate)
            return false;
    }
    return true;
}

static bool textMatch(const QString &field, const QString &value, bool wildcard, bool anchored)
{
    if (!wildcard)
        return field.contains(value, Qt::CaseInsensitive);
    QString re;
    for (const QChar ch : value) {
        if (ch == '*') re += ".*";
        else if (ch == '?') re += ".";
        else re += QRegularExpression::escape(QString(ch));
    }
    if (anchored)
        re = "^" + re + "$";
    return QRegularExpression(re, QRegularExpression::CaseInsensitiveOption).match(field).hasMatch();
}

template <typename T>
static bool compare(const T &a, const QString &op, const T &b)
{
    if (op == ">") return a > b;
    if (op == "<") return a < b;
    if (op == ">=") return a >= b;
    if (op == "<=") return a <= b;
    return a == b;
}

bool LogFilterProxy::matches(const Term &t, int row) const
{
    auto *m = qobject_cast<QsoLogModel *>(sourceModel());
    if (!m)
        return true;
    const Qso &q = m->qsoAt(row);
    const QString status = m->statusOf(q.id);
    const QString date = q.utc.date().toString("yyyy-MM-dd");
    const QString time = q.utc.time().toString("HH:mm:ss");
    const QString freq = q.freqKhz > 0 ? QString::number(q.freqKhz, 'f', 1) : QString();
    const bool cmp = t.op != ":";

    if (t.key.isEmpty()) {
        const QStringList fields = {q.call, q.band, freq, q.mode, date, time, q.rstSent, q.rstRcvd,
                                    q.exchSent, q.exchRcvd, q.comment, status};
        for (const QString &f : fields)
            if (!f.isEmpty() && textMatch(f, t.value, t.wildcard, false))
                return true;
        return false;
    }
    if (t.key == "is") {
        const QString v = t.value.toLower();
        if (v == "tagged" || v == "tag") return q.tagged;
        if (v == "dupe") return status == "DUPE";
        if (v == "ok" || v == "valid") return status == "OK";
        if (v == "invalid") return !status.isEmpty() && status != "OK";
        return false;
    }
    if (t.key == "freq" && cmp) {
        bool ok = false;
        double v = t.value.toDouble(&ok);
        if (!ok || q.freqKhz <= 0) return false;
        if (v < 100) v *= 1000;
        return compare(q.freqKhz, t.op, v);
    }
    if (t.key == "time" && cmp)
        return compare(time.left(5), t.op, normTime(t.value));
    if (t.key == "date" && cmp)
        return compare(date, t.op, t.value);
    if (t.key == "nr" && cmp)
        return compare(row + 1, t.op, t.value.toInt());

    QStringList fields;
    bool anchored = true;
    if (t.key == "call") fields << q.call;
    else if (t.key == "band") { fields << q.band; anchored = false; }
    else if (t.key == "freq") { fields << freq; anchored = false; }
    else if (t.key == "mode") fields << q.mode;
    else if (t.key == "date") { fields << date; anchored = false; }
    else if (t.key == "time") { fields << time; anchored = false; }
    else if (t.key == "rst") fields << q.rstSent << q.rstRcvd;
    else if (t.key == "rsts") fields << q.rstSent;
    else if (t.key == "rstr") fields << q.rstRcvd;
    else if (t.key == "exch") fields << q.exchSent << q.exchRcvd;
    else if (t.key == "exchs") fields << q.exchSent;
    else if (t.key == "exchr") fields << q.exchRcvd;
    else if (t.key == "comment") { fields << q.comment; anchored = false; }
    else if (t.key == "status") { fields << status; anchored = false; }
    else if (t.key == "nr") fields << QString::number(row + 1);

    for (const QString &f : fields)
        if (textMatch(f, t.value, t.wildcard, anchored))
            return true;
    return false;
}
