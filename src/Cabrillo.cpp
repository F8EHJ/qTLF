#include "Cabrillo.h"

#include "ContestRules.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

namespace Cabrillo {

static QString cabMode(const QString &m)
{
    const QString u = m.toUpper();
    if (u == "SSB" || u == "USB" || u == "LSB" || u == "AM") return "PH";
    if (u == "RTTY") return "RY";
    if (u == "FT8" || u == "FT4" || u == "PSK" || u == "DIGI") return "DG";
    return u.left(2);
}

static QString applyTemplate(const QString &tpl, const QHash<QString, QString> &v)
{
    static const QRegularExpression tok(R"(\{(\w+)(?::(>)?(\d+))?\})");
    QString out;
    int last = 0;
    auto it = tok.globalMatch(tpl);
    while (it.hasNext()) {
        const auto m = it.next();
        out += tpl.mid(last, m.capturedStart() - last);
        QString val = v.value(m.captured(1));
        const int width = m.captured(3).toInt();
        if (width > 0)
            val = m.captured(2) == ">" ? val.rightJustified(width, ' ') : val.leftJustified(width, ' ');
        out += val;
        last = m.capturedEnd();
    }
    out += tpl.mid(last);
    return out.trimmed();
}

bool write(const QString &path, const ContestInfo &c, const ContestRules &r, const QVector<Qso> &qsos,
           qint64 claimedScore, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    QTextStream out(&f);
    QSettings s;

    // header tags in order, each tag once: the fields chosen by the operator (cabrillo.fields),
    // then the fixed tags of the contest (cabrillo.header, they win, e.g. CATEGORY-MODE).
    // Station tags (CALLSIGN, CLAIMED-SCORE, GRID-LOCATOR, OPERATORS, NAME, ADDRESS, EMAIL)
    // are always written by qTLF.
    QStringList order;
    QHash<QString, QString> tags;
    auto set = [&](const QString &tag, const QString &value) {
        const QString t = tag.trimmed().toUpper();
        if (t.isEmpty())
            return;
        if (!tags.contains(t))
            order << t;
        tags.insert(t, value);
    };
    for (const CabrilloFieldDef &f : r.cabrilloFields)
        set(f.tag, c.fields.value(f.tag, f.defaultValue).toString());
    for (const auto &h : r.cabrilloHeader)
        set(h.first, h.second);

    out << "START-OF-LOG: 3.0\n";
    out << "CREATED-BY: qTLF " << QTLF_VERSION << "\n";
    out << "CONTEST: " << r.cabrilloContest << "\n";
    out << "CALLSIGN: " << c.myCall << "\n";
    QString operators = c.myCall;
    for (const QString &t : order) {
        if (t == "OPERATORS") {                       // written below with the station data
            if (!tags.value(t).trimmed().isEmpty())
                operators = tags.value(t).trimmed();
            continue;
        }
        out << t << ": " << tags.value(t) << "\n";
    }
    if (claimedScore >= 0)
        out << "CLAIMED-SCORE: " << claimedScore << "\n";
    if (!s.value("station/locator").toString().isEmpty())
        out << "GRID-LOCATOR: " << s.value("station/locator").toString() << "\n";
    out << "OPERATORS: " << operators << "\n";
    out << "NAME: " << s.value("station/name").toString() << "\n";
    const QStringList address = s.value("station/address").toString().split('\n', Qt::SkipEmptyParts);
    for (const QString &line : address.mid(0, 6))
        out << "ADDRESS: " << line.trimmed() << "\n";
    if (address.isEmpty())
        out << "ADDRESS: \n";
    out << "EMAIL: " << s.value("station/email").toString() << "\n";

    const QString tpl = r.cabrilloQsoTemplate;

    for (const Qso &q : qsos) {
        QString freq;
        if (q.freqKhz > 0)
            freq = QString::number(qRound64(q.freqKhz));
        else if (const BandDef *b = r.band(q.band))
            freq = b->cabrillo;
        const QHash<QString, QString> v = {
            {"freq", freq},
            {"mode", cabMode(q.mode)},
            {"date", q.utc.date().toString("yyyy-MM-dd")},
            {"time", q.utc.time().toString("HHmm")},
            {"mycall", c.myCall},
            {"rst_s", q.rstSent},
            {"exch_s", q.exchSent},
            {"call", q.call},
            {"rst_r", q.rstRcvd},
            {"exch_r", q.exchRcvd},
            {"band", q.band},
        };
        out << "QSO: " << applyTemplate(tpl, v) << "\n";
    }
    out << "END-OF-LOG:\n";
    return true;
}

} // namespace Cabrillo
