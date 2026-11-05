#include "ContestRules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QTextStream>

#include <algorithm>

int CsvFileDef::fieldIndex(const QString &name) const
{
    const int i = fields.indexOf(name);
    return i;
}

static QStringList toStringList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &x : v.toArray())
        out << x.toString();
    return out;
}

// plain file name inside the rules folder only
static bool safeFileName(const QString &f)
{
    return !f.isEmpty() && !f.contains('/') && !f.contains('\\') && !f.startsWith('.');
}

static bool readCsvDef(const QJsonObject &o, CsvFileDef &d, const QString &where, QString *error)
{
    d = CsvFileDef();
    if (o.isEmpty())
        return true;
    d.file = o["file"].toString();
    d.url = o["url"].toString().trimmed();
    d.separator = o["separator"].toString(";");
    d.encoding = o["encoding"].toString("auto");
    d.fields = toStringList(o["fields"]);
    if (!safeFileName(d.file)) {
        if (error) *error = QString("%1.file: plain file name of the rules folder expected").arg(where);
        return false;
    }
    if (!d.url.isEmpty() && !(d.url.startsWith("https://") || d.url.startsWith("http://"))) {
        if (error) *error = QString("%1.url: http:// or https:// expected").arg(where);
        return false;
    }
    return true;
}

// "HH:mm" -> minutes, -1 if invalid
static int minutesOf(const QString &hhmm)
{
    const QTime t = QTime::fromString(hhmm, "HH:mm");
    return t.isValid() ? t.msecsSinceStartOfDay() / 60000 : -1;
}

bool ContestRules::load(const QString &d, QString *error)
{
    QFile f(QDir(d).filePath("contest.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QString("Cannot open %1").arg(f.fileName());
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QString("%1: %2 (offset %3)").arg(f.fileName(), pe.errorString()).arg(pe.offset);
        return false;
    }
    const QJsonObject raw = doc.object();
    auto fail = [&](const QString &msg) {
        if (error) *error = QString("%1: %2").arg(f.fileName(), msg);
        return false;
    };
    // keys of older drafts of the format: a clear message instead of a silently ignored file
    if (raw.contains("call_database"))
        return fail("\"call_database\" is now called \"call_exchange\" — see docs/RULES.md");
    if (raw["exchange"].toObject()["received"].toObject()["completion"].isObject())
        return fail("exchange.received.completion is now true / false (values come from the log and "
                    "call_exchange) — see docs/RULES.md");

    dir = QDir(d).absolutePath();
    id = QFileInfo(dir).fileName();
    name = raw["name"].toString(id);
    shortName = raw["short_name"].toString(id);
    description = raw["description"].toString();
    url = raw["url"].toString();

    // ---- schedule
    const QJsonObject sch = raw["schedule"].toObject();
    const QJsonObject dd = sch["default_date"].toObject();
    defaultMonth = dd["month"].toInt();
    defaultDay = dd["day"].toInt();
    periods.clear();
    for (const QJsonValue &v : sch["periods"].toArray()) {
        const QJsonObject o = v.toObject();
        const int s = minutesOf(o["start_utc"].toString());
        const int e = minutesOf(o["end_utc"].toString());
        if (s < 0 || e < 0)
            return fail("schedule.periods: start_utc / end_utc must be \"HH:mm\"");
        PeriodDef p;
        const int day = o["day"].toInt();
        p.startMinutes = day * 1440 + s;
        int endMin;
        if (o.contains("end_day")) {                      // explicit end day: periods longer than 24 h
            endMin = o["end_day"].toInt() * 1440 + e;
            if (endMin <= p.startMinutes)
                return fail("schedule.periods: end_day / end_utc must be after day / start_utc");
        } else {
            endMin = day * 1440 + e;
            if (endMin <= p.startMinutes)
                endMin += 1440;                           // ends after midnight
        }
        p.durationMinutes = endMin - p.startMinutes;
        periods << p;
    }
    std::sort(periods.begin(), periods.end(),
              [](const PeriodDef &a, const PeriodDef &b) { return a.startMinutes < b.startMinutes; });

    // ---- bands / modes
    bands.clear();
    for (const QJsonValue &v : raw["bands"].toArray()) {
        const QJsonObject o = v.toObject();
        BandDef b;
        b.name = o["name"].toString();
        b.lowKhz = o["low_khz"].toDouble();
        b.highKhz = o["high_khz"].toDouble();
        const QJsonArray seg = o["segment"].toArray();
        if (seg.size() == 2) {
            b.segLowKhz = seg[0].toDouble();
            b.segHighKhz = seg[1].toDouble();
        }
        b.cabrillo = o["cabrillo"].toString(QString::number(qRound(b.lowKhz)));
        if (!b.name.isEmpty())
            bands << b;
    }
    if (bands.isEmpty())
        return fail("no bands defined");
    modes.clear();
    for (const QString &m : toStringList(raw["modes"]))
        modes << m.toUpper();
    if (modes.isEmpty())
        modes << "CW";

    // ---- exchange
    const QJsonObject ex = raw["exchange"].toObject();
    const QJsonObject rst = ex["rst"].toObject();
    rstSentDefault = rst["sent"].toString("599");
    rstRcvdDefault = rst["received"].toString("599");
    const QJsonObject es = ex["sent"].toObject();
    exchSentLabel = es["label"].toString("Exch Sent");
    exchSentType = es["type"].toString("serial");
    exchSentPrompt = es["prompt"].toString();
    serialStart = es["serial_start"].toInt(1);
    serialDigits = es["serial_digits"].toInt(3);
    const QJsonObject er = ex["received"].toObject();
    exchRcvdLabel = er["label"].toString("Exch Rcvd");
    exchRcvdType = er["type"].toString("text");
    exchRcvdRequired = er["required"].toBool(false);
    exchRcvdPattern = er["pattern"].toString();
    if (!exchRcvdPattern.isEmpty() && !QRegularExpression(exchRcvdPattern).isValid())
        return fail("exchange.received.pattern: invalid regular expression");
    exchRcvdCompletion = er["completion"].toBool(false);

    dupeRule = raw["dupe"].toString("band");

    const QJsonObject bc = raw["band_change"].toObject();
    bandChangeMinutes = qMax(0, bc["min_minutes"].toInt(0));

    // ---- reference files
    const QJsonObject cdb = raw["call_exchange"].toObject();
    if (!readCsvDef(cdb, callDb, "call_exchange", error))
        return false;
    if (!callDb.file.isEmpty() && callDb.fields.isEmpty())
        callDb.fields << "call" << "exch";
    callDbExchangeField = cdb["exchange_field"].toString(callDb.fields.value(1));


    const QJsonObject cf = raw["country_file"].toObject();
    ctyFile = cf["file"].toString();
    ctyUrl = cf["url"].toString().trimmed();
    if (!ctyFile.isEmpty() && !safeFileName(ctyFile))
        return fail("country_file.file: plain file name of the rules folder expected");

    // exchange.received.prefill: source used after the log of the contest
    //   "call_exchange" (default when the file exists) | "cqz" | "ituz" | "cont" | "dxcc" | "country" | "none"
    {
        const QJsonValue pv = ex["received"].toObject()["prefill"];
        if (pv.isBool())                                   // short form: true = default, false = none
            exchRcvdPrefill = pv.toBool() ? QString() : QString("none");
        else
            exchRcvdPrefill = pv.toString().trimmed().toLower();
        if (exchRcvdPrefill.isEmpty())
            exchRcvdPrefill = callDb.file.isEmpty() ? "none" : "call_exchange";
        static const QStringList ok = {"call_exchange", "cqz", "ituz", "cont", "dxcc", "country", "none"};
        if (!ok.contains(exchRcvdPrefill))
            return fail("exchange.received.prefill: one of " + ok.join(", ") + " expected");
        if (exchRcvdPrefill == "call_exchange" && callDb.file.isEmpty())
            return fail("exchange.received.prefill is \"call_exchange\" but there is no \"call_exchange\" file");
        if (exchRcvdPrefill != "call_exchange" && exchRcvdPrefill != "none" && ctyFile.isEmpty())
            return fail("exchange.received.prefill \"" + exchRcvdPrefill + "\" needs a \"country_file\" (cty.dat)");
    }

    remoteFiles.clear();
    if (!callDb.url.isEmpty()) remoteFiles.insert(callDb.file, callDb.url);
    if (!ctyUrl.isEmpty() && !ctyFile.isEmpty()) remoteFiles.insert(ctyFile, ctyUrl);
    remoteUpdate = raw["remote_update"].toString("on_open");

    // ---- scoring and windows
    scoringScript = raw["scoring"].toString();
    windows.clear();
    for (const QJsonValue &v : raw["windows"].toArray()) {
        const QJsonObject o = v.toObject();
        WindowDef w;
        w.qml = o["qml"].toString();
        w.id = o["id"].toString(QFileInfo(w.qml).baseName());
        w.title = o["title"].toString(w.id);
        w.area = o["area"].toString("right");
        w.visible = o["visible"].toBool(true);
        if (!w.qml.isEmpty())
            windows << w;
    }

    // ---- Cabrillo
    const QJsonObject cab = raw["cabrillo"].toObject();
    cabrilloContest = cab["contest"].toString(shortName);
    cabrilloFields.clear();
    for (const QJsonValue &v : cab["fields"].toArray()) {
        const QJsonObject o = v.toObject();
        CabrilloFieldDef fd;
        fd.tag = o["tag"].toString().toUpper();
        fd.label = o["label"].toString(fd.tag);
        fd.values = toStringList(o["values"]);
        fd.defaultValue = o["default"].toString(fd.values.value(0));
        if (!fd.tag.isEmpty())
            cabrilloFields << fd;
    }
    cabrilloHeader.clear();
    const QJsonObject hdr = cab["header"].toObject();
    for (auto it = hdr.begin(); it != hdr.end(); ++it)
        cabrilloHeader << qMakePair(it.key().toUpper(), it.value().toString());
    cabrilloQsoTemplate = cab["qso_template"].toString(
        "{freq:>5} {mode:2} {date} {time} {mycall:13} {rst_s:3} {exch_s:6} {call:13} {rst_r:3} {exch_r:6}");
    return true;
}

QList<ContestRules> ContestRules::discover(const QString &root, QStringList *errors)
{
    QList<ContestRules> out;
    const QDir r(root);
    const auto entries = r.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : entries) {
        if (!QFile::exists(fi.absoluteFilePath() + "/contest.json"))
            continue;
        ContestRules c;
        QString err;
        if (c.load(fi.absoluteFilePath(), &err))
            out << c;
        else if (errors)
            *errors << err;
    }
    return out;
}

QString ContestRules::bandForFrequency(double khz) const
{
    for (const BandDef &b : bands)
        if (khz >= b.lowKhz && khz <= b.highKhz)
            return b.name;
    return QString();
}

const BandDef *ContestRules::band(const QString &n) const
{
    for (const BandDef &b : bands)
        if (b.name.compare(n, Qt::CaseInsensitive) == 0)
            return &b;
    return nullptr;
}

bool ContestRules::inSegment(double khz) const
{
    for (const BandDef &b : bands) {
        if (khz >= b.lowKhz && khz <= b.highKhz) {
            if (b.segLowKhz <= 0)
                return true;
            return khz >= b.segLowKhz && khz <= b.segHighKhz;
        }
    }
    return false;
}

QStringList ContestRules::bandNames() const
{
    QStringList l;
    for (const BandDef &b : bands)
        l << b.name;
    return l;
}

QString ContestRules::filePath(const QString &rel) const
{
    return QDir(dir).filePath(rel);
}

QString ContestRules::userDataCopy(const QString &rel) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rules/" + id + "/" + rel;
}

QString ContestRules::dataFilePath(const QString &rel) const
{
    const QFileInfo local(filePath(rel));
    const QFileInfo copy(userDataCopy(rel));
    if (copy.exists() && (!local.exists() || copy.lastModified() > local.lastModified()))
        return copy.absoluteFilePath();
    return local.absoluteFilePath();
}

// Windows-1252 bytes 0x80..0x9F (the rest is identical to Latin-1)
static const char16_t cp1252High[32] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
    0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};

QString ContestRules::readText(const QString &path, const QString &encoding, bool *ok)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return QString();
    }
    if (ok) *ok = true;
    QByteArray data = f.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);                                   // UTF-8 BOM
    const QString enc = encoding.toLower();
    if (enc == "utf-8" || enc == "utf8")
        return QString::fromUtf8(data);
    if (enc == "latin1" || enc == "iso-8859-1")
        return QString::fromLatin1(data);
    if (enc != "windows-1252" && enc != "cp1252") {          // auto
        QStringDecoder utf8(QStringDecoder::Utf8);
        const QString s = utf8(data);
        if (!utf8.hasError())
            return s;
    }
    QString out;
    out.reserve(data.size());
    for (const char c : data) {
        const uchar u = uchar(c);
        out += (u >= 0x80 && u < 0xA0) ? QChar(cp1252High[u - 0x80]) : QChar(u);
    }
    return out;
}


QList<QStringList> ContestRules::readCsv(const QString &path, const CsvFileDef &def, bool *ok)
{
    QList<QStringList> out;
    const QString text = readText(path, def.encoding, ok);
    const QString sep = def.separator.isEmpty() ? QStringLiteral(";") : def.separator;
    bool first = true;
    for (QString line : text.split('\n')) {
        line = line.trimmed();                       // also removes \r (CRLF files)
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        QStringList f = line.split(sep);
        for (QString &x : f)
            x = x.trimmed();
        if (first) {                                 // optional header line = field names
            first = false;
            bool header = !def.fields.isEmpty() && f.size() >= 1;
            for (int i = 0; header && i < qMin(f.size(), def.fields.size()); ++i)
                header = f[i].compare(def.fields[i], Qt::CaseInsensitive) == 0;
            if (header)
                continue;
        }
        out << f;
    }
    return out;
}

QDate ContestRules::nextDefaultDate(const QDate &from) const
{
    if (defaultMonth < 1 || defaultDay < 1)
        return from;
    QDate d(from.year(), defaultMonth, defaultDay);
    if (d < from)
        d = QDate(from.year() + 1, defaultMonth, defaultDay);
    return d;
}

QList<QPair<QDateTime, QDateTime>> ContestRules::periodsUtc(const QDate &d) const
{
    QList<QPair<QDateTime, QDateTime>> out;
    const QDateTime day0(d, QTime(0, 0), Qt::UTC);
    for (const PeriodDef &p : periods) {
        const QDateTime s = day0.addSecs(qint64(p.startMinutes) * 60);
        out << qMakePair(s, s.addSecs(qint64(p.durationMinutes) * 60));
    }
    return out;
}

QDateTime ContestRules::periodStartUtc(const QDate &d) const
{
    const auto p = periodsUtc(d);
    return p.isEmpty() ? QDateTime() : p.first().first;
}

QDateTime ContestRules::periodEndUtc(const QDate &d) const
{
    const auto p = periodsUtc(d);
    return p.isEmpty() ? QDateTime() : p.last().second;
}

QString ContestRules::formatSerial(int n) const
{
    return QString("%1").arg(n, serialDigits, 10, QChar('0'));
}
