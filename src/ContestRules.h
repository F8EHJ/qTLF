#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTime>

// =====================================================================================
//  Contest definition = rules/<ID>/contest.json
//  This class is the only place that reads contest.json; the rest of qTLF uses its fields.
//  Full reference: docs/RULES.md
// =====================================================================================

struct BandDef
{
    QString name;
    double lowKhz = 0;
    double highKhz = 0;
    double segLowKhz = 0;   // optional contest segment (0 = none)
    double segHighKhz = 0;
    QString cabrillo;       // band token for Cabrillo when no frequency is known
};

// One operating window, relative to 00:00 UTC of the contest date (may run past midnight).
struct PeriodDef
{
    int startMinutes = 0;      // e.g. day 1 07:00 -> 1440 + 420
    int durationMinutes = 0;
};

// A CSV data file of the contest: one record per line, fields separated by 'separator',
// '#' comments, optional header line (= the field names). Optional 'url' to download it.
struct CsvFileDef
{
    QString file;                       // relative to the rules folder; empty = not used
    QString url;
    QString separator = ";";
    QString encoding = "auto";          // auto | utf-8 | windows-1252 | latin1
    QStringList fields;                 // field names, first = callsign for a call database
    int fieldIndex(const QString &name) const;
};

// Cabrillo header field chosen by the operator (contest creation and Cabrillo export)
struct CabrilloFieldDef
{
    QString tag;                        // e.g. CATEGORY-POWER
    QString label;
    QStringList values;                 // choice list; empty = free text
    QString defaultValue;
};

struct WindowDef
{
    QString id;
    QString title;
    QString qml;            // relative to the rules folder
    QString area;           // left/right/top/bottom/float
    bool visible = true;
};

class ContestRules
{
public:

    bool load(const QString &dir, QString *error = nullptr);
    static QList<ContestRules> discover(const QString &root, QStringList *errors = nullptr);

    // --- identity
    QString id, dir, name, shortName, description, url;

    // --- schedule
    int defaultMonth = 0, defaultDay = 0;          // schedule.default_date
    QList<PeriodDef> periods;                      // schedule.periods

    // --- bands / modes
    QList<BandDef> bands;
    QStringList modes;

    // --- exchange
    QString rstSentDefault = "599";
    QString rstRcvdDefault = "599";
    QString exchSentLabel = "Exch Sent";
    QString exchSentType = "serial";               // serial | fixed
    QString exchSentPrompt;
    int serialStart = 1;
    int serialDigits = 3;
    QString exchRcvdLabel = "Exch Rcvd";
    QString exchRcvdType = "text";                 // text | number | callsign
    bool exchRcvdRequired = false;
    QString exchRcvdPattern;                       // optional regular expression
    bool exchRcvdCompletion = false;               // drop-down list: values of the log, then of call_exchange
    QString exchRcvdPrefill = "none";              // after the log: call_exchange | cqz | ituz | cont | dxcc | country | none

    QString dupeRule = "band";                     // band | band_mode | contest | none

    // "band_change": minimum time on a band before changing band (0 = no rule)
    int bandChangeMinutes = 0;                     // band_change.min_minutes, from the 1st QSO on the band

    // --- reference files
    CsvFileDef callDb;                             // call_exchange (CSV: call;exchange;other fields…)
    QString callDbExchangeField;
    QString ctyFile, ctyUrl;                       // country_file (cty.dat)
    QMap<QString, QString> remoteFiles;            // file -> url (every "url" above)
    QString remoteUpdate = "on_open";              // on_open | manual

    // --- scoring and windows
    QString scoringScript;
    QList<WindowDef> windows;

    // --- Cabrillo
    QString cabrilloContest;
    QList<CabrilloFieldDef> cabrilloFields;
    QList<QPair<QString, QString>> cabrilloHeader; // fixed tags, in file order
    QString cabrilloQsoTemplate;

    // --- helpers
    QString bandForFrequency(double khz) const;
    const BandDef *band(const QString &name) const;
    bool inSegment(double khz) const;
    QStringList bandNames() const;
    QString filePath(const QString &rel) const;
    // Data file to read: the downloaded copy in the user data folder when it is newer than the
    // one in the rules folder (rules folder not writable, e.g. C:\Program Files), else the rules one.
    QString dataFilePath(const QString &rel) const;
    QString userDataCopy(const QString &rel) const;
    QDate nextDefaultDate(const QDate &from) const;
    QDateTime periodStartUtc(const QDate &d) const;
    QDateTime periodEndUtc(const QDate &d) const;
    QList<QPair<QDateTime, QDateTime>> periodsUtc(const QDate &d) const;
    QString formatSerial(int n) const;

    // Reads a text file; "auto" = UTF-8 if valid, else Windows-1252 (CP1252)
    static QString readText(const QString &path, const QString &encoding = "auto", bool *ok = nullptr);
    // Reads a CSV data file: records (trimmed fields); comments, blank lines and header skipped
    static QList<QStringList> readCsv(const QString &path, const CsvFileDef &def, bool *ok = nullptr);
};
