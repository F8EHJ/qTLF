#include "CtyDat.h"

#include "ContestRules.h"

#include <QRegularExpression>

QVariantMap CtyDat::Info::toVariant() const
{
    QVariantMap m;
    m["valid"] = valid;
    m["country"] = country;
    m["cont"] = continent;
    m["prefix"] = prefix;
    m["cqz"] = cqZone;
    m["ituz"] = ituZone;
    return m;
}

bool CtyDat::load(const QString &path, QString *error)
{
    m_entities.clear();
    m_exact.clear();
    m_prefixes.clear();
    m_maxPrefix = 0;

    bool ok = false;
    const QString text = ContestRules::readText(path, "auto", &ok);
    if (!ok) {
        if (error) *error = QString("cannot open %1").arg(path);
        return false;
    }

    // Records: 8 header fields ended by ':' then aliases separated by ',' and ended by ';'
    static const QRegularExpression overrideRe(R"(\((\d+)\)|\[(\d+)\]|\{(\w+)\}|<[^>]*>|~[^~]*~)");
    int pos = 0;
    const int n = text.size();
    while (pos < n) {
        // header: 8 fields
        QStringList hdr;
        while (hdr.size() < 8 && pos < n) {
            const int c = text.indexOf(':', pos);
            if (c < 0) { pos = n; break; }
            hdr << text.mid(pos, c - pos).trimmed();
            pos = c + 1;
        }
        if (hdr.size() < 8)
            break;
        const int semi = text.indexOf(';', pos);
        if (semi < 0)
            break;
        const QString aliasText = text.mid(pos, semi - pos);
        pos = semi + 1;

        Entity e;
        e.name = hdr[0];
        e.cqZone = hdr[1].toInt();
        e.ituZone = hdr[2].toInt();
        e.continent = hdr[3].toUpper();
        e.prefix = hdr[7];
        if (e.prefix.startsWith('*')) {
            e.wae = true;
            e.prefix.remove(0, 1);
        }
        const int idx = m_entities.size();
        m_entities << e;

        const QStringList aliases = aliasText.split(',', Qt::SkipEmptyParts);
        for (QString a : aliases) {
            a = a.trimmed();
            if (a.isEmpty())
                continue;
            Alias al;
            al.entity = idx;
            auto it = overrideRe.globalMatch(a);
            while (it.hasNext()) {
                const auto m = it.next();
                if (!m.captured(1).isEmpty()) al.cqZone = m.captured(1).toInt();
                if (!m.captured(2).isEmpty()) al.ituZone = m.captured(2).toInt();
                if (!m.captured(3).isEmpty()) al.continent = m.captured(3).toUpper();
            }
            a.remove(overrideRe);
            a = a.trimmed().toUpper();
            if (a.startsWith('=')) {
                a.remove(0, 1);
                if (!m_exact.contains(a))          // first occurrence wins
                    m_exact.insert(a, al);
            } else if (!a.isEmpty() && !m_prefixes.contains(a)) {
                m_prefixes.insert(a, al);
                m_maxPrefix = qMax(m_maxPrefix, int(a.size()));
            }
        }
    }
    if (m_entities.isEmpty()) {
        if (error) *error = QString("%1: no entity found (not a cty.dat file?)").arg(path);
        return false;
    }
    return true;
}

CtyDat::Info CtyDat::make(const Alias &a) const
{
    Info i;
    if (a.entity < 0 || a.entity >= m_entities.size())
        return i;
    const Entity &e = m_entities[a.entity];
    i.valid = true;
    i.country = e.name;
    i.prefix = e.prefix;
    i.continent = a.continent.isEmpty() ? e.continent : a.continent;
    i.cqZone = a.cqZone ? a.cqZone : e.cqZone;
    i.ituZone = a.ituZone ? a.ituZone : e.ituZone;
    return i;
}

bool CtyDat::findPrefix(const QString &s, Alias &out) const
{
    for (int len = qMin(int(s.size()), m_maxPrefix); len > 0; --len) {
        const auto it = m_prefixes.constFind(s.left(len));
        if (it != m_prefixes.constEnd()) {
            out = it.value();
            return true;
        }
    }
    return false;
}

CtyDat::Info CtyDat::lookup(const QString &callsign) const
{
    QString call = callsign.trimmed().toUpper();
    if (call.isEmpty() || m_entities.isEmpty())
        return Info();

    // 1. exact match on the full call (e.g. =F8UFT/P listed in the file)
    if (const auto it = m_exact.constFind(call); it != m_exact.constEnd())
        return make(it.value());

    // 2. remove the usual operating suffixes
    static const QStringList ignored = {"P", "M", "QRP", "QRPP", "A", "B", "LH", "J", "R", "LGT", "AE", "AG"};
    QStringList parts = call.split('/', Qt::SkipEmptyParts);
    if (parts.size() > 1 && (parts.last() == "MM" || parts.last() == "AM"))
        return Info();                                     // maritime / aeronautical mobile: no entity
    while (parts.size() > 1 && ignored.contains(parts.last()))
        parts.removeLast();
    if (parts.isEmpty())
        return Info();
    const QString base = parts.join('/');
    if (const auto it = m_exact.constFind(base); it != m_exact.constEnd())
        return make(it.value());

    // 3. portable prefix: EA8/F8EHJ or F8EHJ/EA8 -> the shorter part is the prefix;
    //    W1ABC/4 -> W4
    QString key = parts.first();
    if (parts.size() >= 2) {
        const QString a = parts[0], b = parts[1];
        static const QRegularExpression digit("^\\d$");
        if (digit.match(b).hasMatch()) {
            static const QRegularExpression callPrefix("^([A-Z0-9]*?[A-Z])\\d");
            const auto m = callPrefix.match(a);
            key = (m.hasMatch() ? m.captured(1) : a) + b;
        } else {
            key = (b.size() < a.size()) ? b : a;
        }
    }
    Alias al;
    if (findPrefix(key, al))
        return make(al);
    return Info();
}
