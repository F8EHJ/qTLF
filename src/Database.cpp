#include "Database.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <QVariant>

static const char *kConn = "qtlf";

static QSqlDatabase conn() { return QSqlDatabase::database(kConn); }

Database::~Database()
{
    {
        QSqlDatabase db = QSqlDatabase::database(kConn, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(kConn);
}

bool Database::open(const QString &path, QString *error)
{
    m_path = path;
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", kConn);
    db.setDatabaseName(path);
    if (!db.open()) {
        m_error = db.lastError().text();
        if (error) *error = m_error;
        return false;
    }
    QSqlQuery q(db);
    const char *schema[] = {
        "PRAGMA foreign_keys = ON",
        "CREATE TABLE IF NOT EXISTS contests ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " rules TEXT NOT NULL, name TEXT, date TEXT NOT NULL,"
        " mycall TEXT, category TEXT, exch_sent TEXT, created TEXT)",
        "CREATE TABLE IF NOT EXISTS qsos ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " contest_id INTEGER NOT NULL REFERENCES contests(id) ON DELETE CASCADE,"
        " ts_utc TEXT NOT NULL, band TEXT, freq_khz REAL, mode TEXT, call TEXT,"
        " rst_s TEXT, rst_r TEXT, exch_s TEXT, exch_r TEXT,"
        " tagged INTEGER DEFAULT 0, comment TEXT)",
        "CREATE INDEX IF NOT EXISTS idx_qsos_contest ON qsos(contest_id, ts_utc)",
        "CREATE INDEX IF NOT EXISTS idx_qsos_call ON qsos(contest_id, call)",
    };
    for (const char *s : schema) {
        if (!q.exec(QString::fromLatin1(s))) {
            m_error = q.lastError().text();
            if (error) *error = m_error;
            return false;
        }
    }
    // migrations of older databases
    bool hasFields = false;
    q.exec("PRAGMA table_info(contests)");
    while (q.next())
        if (q.value(1).toString() == "fields")
            hasFields = true;
    if (!hasFields && !q.exec("ALTER TABLE contests ADD COLUMN fields TEXT")) {
        m_error = q.lastError().text();
        if (error) *error = m_error;
        return false;
    }
    return true;
}

static ContestInfo readContest(const QSqlQuery &q)
{
    ContestInfo c;
    c.id = q.value("id").toInt();
    c.rulesId = q.value("rules").toString();
    c.name = q.value("name").toString();
    c.date = QDate::fromString(q.value("date").toString(), "yyyy-MM-dd");
    c.myCall = q.value("mycall").toString();
    c.category = q.value("category").toString();
    c.exchSent = q.value("exch_sent").toString();
    c.fields = QJsonDocument::fromJson(q.value("fields").toString().toUtf8()).object().toVariantMap();
    c.created = QDateTime::fromString(q.value("created").toString(), Qt::ISODate);
    c.qsoCount = q.value("n").toInt();
    return c;
}

QList<ContestInfo> Database::contests()
{
    QList<ContestInfo> out;
    QSqlQuery q(conn());
    q.exec("SELECT c.*, (SELECT COUNT(*) FROM qsos WHERE contest_id = c.id) AS n "
           "FROM contests c ORDER BY c.date DESC, c.id DESC");
    while (q.next())
        out << readContest(q);
    return out;
}

bool Database::contest(int id, ContestInfo &out)
{
    QSqlQuery q(conn());
    q.prepare("SELECT c.*, (SELECT COUNT(*) FROM qsos WHERE contest_id = c.id) AS n "
              "FROM contests c WHERE c.id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return false;
    out = readContest(q);
    return true;
}

int Database::createContest(const ContestInfo &c)
{
    QSqlQuery q(conn());
    q.prepare("INSERT INTO contests (rules, name, date, mycall, category, exch_sent, created, fields) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(c.rulesId);
    q.addBindValue(c.name);
    q.addBindValue(c.date.toString("yyyy-MM-dd"));
    q.addBindValue(c.myCall);
    q.addBindValue(c.category);
    q.addBindValue(c.exchSent);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.addBindValue(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(c.fields)).toJson(QJsonDocument::Compact)));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool Database::updateContestFields(int id, const QVariantMap &fields)
{
    QSqlQuery q(conn());
    q.prepare("UPDATE contests SET fields = ? WHERE id = ?");
    q.addBindValue(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(fields)).toJson(QJsonDocument::Compact)));
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteContest(int id)
{
    QSqlQuery q(conn());
    q.prepare("DELETE FROM qsos WHERE contest_id = ?");
    q.addBindValue(id);
    if (!q.exec()) { m_error = q.lastError().text(); return false; }
    q.prepare("DELETE FROM contests WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) { m_error = q.lastError().text(); return false; }
    return true;
}

QVector<Qso> Database::qsos(int contestId)
{
    QVector<Qso> out;
    QSqlQuery q(conn());
    q.prepare("SELECT * FROM qsos WHERE contest_id = ? ORDER BY ts_utc, id");
    q.addBindValue(contestId);
    q.exec();
    while (q.next()) {
        Qso r;
        r.id = q.value("id").toInt();
        r.contestId = q.value("contest_id").toInt();
        r.utc = QDateTime::fromString(q.value("ts_utc").toString(), Qt::ISODate).toUTC();
        r.band = q.value("band").toString();
        r.freqKhz = q.value("freq_khz").toDouble();
        r.mode = q.value("mode").toString();
        r.call = q.value("call").toString();
        r.rstSent = q.value("rst_s").toString();
        r.rstRcvd = q.value("rst_r").toString();
        r.exchSent = q.value("exch_s").toString();
        r.exchRcvd = q.value("exch_r").toString();
        r.tagged = q.value("tagged").toInt() != 0;
        r.comment = q.value("comment").toString();
        out << r;
    }
    return out;
}

static void bindQso(QSqlQuery &q, const Qso &r)
{
    q.addBindValue(r.utc.toUTC().toString(Qt::ISODate));
    q.addBindValue(r.band);
    q.addBindValue(r.freqKhz);
    q.addBindValue(r.mode);
    q.addBindValue(r.call);
    q.addBindValue(r.rstSent);
    q.addBindValue(r.rstRcvd);
    q.addBindValue(r.exchSent);
    q.addBindValue(r.exchRcvd);
    q.addBindValue(r.tagged ? 1 : 0);
    q.addBindValue(r.comment);
}

int Database::insertQso(const Qso &r)
{
    QSqlQuery q(conn());
    q.prepare("INSERT INTO qsos (ts_utc, band, freq_khz, mode, call, rst_s, rst_r, exch_s, exch_r,"
              " tagged, comment, contest_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)");
    bindQso(q, r);
    q.addBindValue(r.contestId);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool Database::updateQso(const Qso &r)
{
    QSqlQuery q(conn());
    q.prepare("UPDATE qsos SET ts_utc=?, band=?, freq_khz=?, mode=?, call=?, rst_s=?, rst_r=?,"
              " exch_s=?, exch_r=?, tagged=?, comment=? WHERE id=?");
    bindQso(q, r);
    q.addBindValue(r.id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteQso(int id)
{
    QSqlQuery q(conn());
    q.prepare("DELETE FROM qsos WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}
