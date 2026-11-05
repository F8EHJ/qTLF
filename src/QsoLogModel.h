#pragma once

#include "Types.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

class Database;
class ContestRules;

// Editable table model of the QSOs of one contest; every edit is written to the database.
class QsoLogModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColNr, ColDate, ColTime, ColBand, ColFreq, ColMode, ColCall,
        ColRstS, ColRstR, ColExchR, ColExchS, ColStatus, ColTag, ColComment, ColDelete,
        ColCount
    };
    static constexpr int SortRole = Qt::UserRole + 1;

    QsoLogModel(Database *db, const ContestRules *rules, QObject *parent = nullptr);

    void load(int contestId);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    bool addQso(Qso q);
    int lastAddedId() const { return m_lastAddedId; }   // id of the QSO added by the last addQso()
    bool removeQsoAt(int row);
    bool setTag(int row, bool tagged, const QString &comment);

    const QVector<Qso> &qsos() const { return m_qsos; }
    const Qso &qsoAt(int row) const { return m_qsos.at(row); }
    QString statusOf(int qsoId) const { return m_status.value(qsoId); }
    void setStatuses(const QHash<int, QString> &status);
    int nextSerial() const;
    QString lastError() const { return m_error; }

signals:
    void logChanged();

private:
    bool commit(int row, const Qso &updated);

    Database *m_db;
    const ContestRules *m_rules;
    int m_contestId = 0;
    QVector<Qso> m_qsos;
    QHash<int, QString> m_status;
    QString m_error;
    int m_lastAddedId = -1;
};
