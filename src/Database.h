#pragma once

#include "Types.h"

#include <QList>
#include <QHash>
#include <QString>
#include <QVector>

// Global database: all contests and all QSOs in one file.
// Written with portable SQL so the QSQLITE driver can later be swapped for
// QPSQL / QMYSQL (remote server) by changing open().
class Database
{
public:
    ~Database();
    bool open(const QString &path, QString *error = nullptr);
    QString path() const { return m_path; }

    QList<ContestInfo> contests();
    bool contest(int id, ContestInfo &out);
    int createContest(const ContestInfo &c);
    bool deleteContest(int id);
    bool updateContestFields(int id, const QVariantMap &fields);

    QVector<Qso> qsos(int contestId);
    int insertQso(const Qso &q);
    bool updateQso(const Qso &q);
    bool deleteQso(int id);

    QString lastError() const { return m_error; }

private:
    QString m_path;
    QString m_error;
};
