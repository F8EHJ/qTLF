#include "QsoLogModel.h"

#include "ContestRules.h"
#include "Database.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QStyle>

QsoLogModel::QsoLogModel(Database *db, const ContestRules *rules, QObject *parent)
    : QAbstractTableModel(parent), m_db(db), m_rules(rules)
{
}

void QsoLogModel::load(int contestId)
{
    beginResetModel();
    m_contestId = contestId;
    m_qsos = m_db->qsos(contestId);
    m_status.clear();
    endResetModel();
}

int QsoLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_qsos.size();
}

int QsoLogModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

static QString freqText(double f)
{
    if (f <= 0)
        return QString();
    if (qFuzzyCompare(f, double(qRound64(f))))
        return QString::number(qRound64(f));
    return QString::number(f, 'f', 1);
}

QVariant QsoLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_qsos.size())
        return {};
    const Qso &q = m_qsos.at(index.row());
    const int c = index.column();
    const QString status = m_status.value(q.id);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (c) {
        case ColNr: return index.row() + 1;
        case ColDate: return q.utc.date().toString("yyyy-MM-dd");
        case ColTime: return q.utc.time().toString(role == Qt::EditRole ? "HH:mm:ss" : "HH:mm");
        case ColBand: return q.band;
        case ColFreq: return freqText(q.freqKhz);
        case ColMode: return q.mode;
        case ColCall: return q.call;
        case ColRstS: return q.rstSent;
        case ColRstR: return q.rstRcvd;
        case ColExchS: return q.exchSent;
        case ColExchR: return q.exchRcvd;
        case ColStatus: return status;
        case ColComment: return q.comment;
        default: return {};
        }
    }
    if (role == SortRole) {
        switch (c) {
        case ColNr: return index.row();
        case ColDate:
        case ColTime: return q.utc.toMSecsSinceEpoch() * 1000000LL + q.id; // stable for equal times
        case ColFreq: return q.freqKhz;
        case ColTag: return q.tagged ? 1 : 0;
        default: return data(index, Qt::DisplayRole);
        }
    }
    if (role == Qt::CheckStateRole && c == ColTag)
        return q.tagged ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::DecorationRole && c == ColDelete) {
        static const QIcon trash = QIcon::fromTheme("user-trash",
            QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
        return trash;
    }
    if (role == Qt::ToolTipRole) {
        if (c == ColDelete) return tr("Delete this QSO");
        if (c == ColTag) return q.tagged ? tr("Tagged: %1").arg(q.comment) : tr("Tag this QSO for later");
        if (c == ColStatus && status != "OK") return status;
    }
    if (role == Qt::TextAlignmentRole) {
        switch (c) {
        case ColCall: case ColExchR: case ColExchS: case ColComment: case ColStatus:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        case ColFreq: case ColNr:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignCenter);
        }
    }
    if (role == Qt::BackgroundRole) {
        if (c == ColStatus && !status.isEmpty() && status != "OK")
            return QBrush(QColor(220, 53, 69, 90));
        if (q.tagged)
            return QBrush(QColor(255, 193, 7, 55));
    }
    if (role == Qt::FontRole && c == ColCall) {
        QFont f;
        f.setBold(true);
        return f;
    }
    return {};
}

QVariant QsoLogModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, o, role);
    switch (section) {
    case ColNr: return "#";
    case ColDate: return tr("Date");
    case ColTime: return tr("UTC");
    case ColBand: return tr("Band");
    case ColFreq: return tr("kHz");
    case ColMode: return tr("Mode");
    case ColCall: return tr("Call");
    case ColRstS: return tr("RST S");
    case ColRstR: return tr("RST R");
    case ColExchS: return m_rules->exchSentLabel;
    case ColExchR: return m_rules->exchRcvdLabel;
    case ColStatus: return tr("Status");
    case ColTag: return tr("Tag");
    case ColComment: return tr("Comment");
    case ColDelete: return QString();
    }
    return {};
}

Qt::ItemFlags QsoLogModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    switch (index.column()) {
    case ColNr: case ColStatus: case ColDelete:
        return f;
    case ColTag:
        return f | Qt::ItemIsUserCheckable;
    default:
        return f | Qt::ItemIsEditable;
    }
}

bool QsoLogModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;
    Qso q = m_qsos.at(index.row());
    const QString v = value.toString().trimmed();

    if (role == Qt::CheckStateRole && index.column() == ColTag) {
        q.tagged = value.toInt() == Qt::Checked;
        return commit(index.row(), q);
    }
    if (role != Qt::EditRole)
        return false;

    switch (index.column()) {
    case ColDate: {
        const QDate d = QDate::fromString(v, "yyyy-MM-dd");
        if (!d.isValid()) return false;
        q.utc = QDateTime(d, q.utc.time(), Qt::UTC);
        break;
    }
    case ColTime: {
        QTime t = QTime::fromString(v, "HH:mm:ss");
        if (!t.isValid()) t = QTime::fromString(v, "HH:mm");
        if (!t.isValid()) t = QTime::fromString(v, "HHmm");
        if (!t.isValid()) return false;
        q.utc = QDateTime(q.utc.date(), t, Qt::UTC);
        break;
    }
    case ColBand: {
        const BandDef *b = m_rules->band(v);
        if (!b) return false;
        q.band = b->name;
        if (q.freqKhz > 0 && m_rules->bandForFrequency(q.freqKhz) != q.band)
            q.freqKhz = 0;
        break;
    }
    case ColFreq: {
        if (v.isEmpty()) { q.freqKhz = 0; break; }
        bool ok = false;
        double f = v.toDouble(&ok);
        if (!ok || f <= 0) return false;
        if (f < 100) f *= 1000.0;      // typed in MHz
        q.freqKhz = f;
        const QString b = m_rules->bandForFrequency(f);
        if (!b.isEmpty()) q.band = b;
        break;
    }
    case ColMode: q.mode = v.toUpper(); break;
    case ColCall: if (v.isEmpty()) return false; q.call = v.toUpper(); break;
    case ColRstS: q.rstSent = v; break;
    case ColRstR: q.rstRcvd = v; break;
    case ColExchS: q.exchSent = v.toUpper(); break;
    case ColExchR: q.exchRcvd = v.toUpper(); break;
    case ColComment:
        q.comment = value.toString();
        if (!q.comment.isEmpty()) q.tagged = true;
        break;
    default: return false;
    }
    return commit(index.row(), q);
}

bool QsoLogModel::commit(int row, const Qso &updated)
{
    if (!m_db->updateQso(updated)) {
        m_error = m_db->lastError();
        return false;
    }
    m_qsos[row] = updated;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
    emit logChanged();
    return true;
}

bool QsoLogModel::addQso(Qso q)
{
    q.contestId = m_contestId;
    const int id = m_db->insertQso(q);
    if (id <= 0) {
        m_error = m_db->lastError();
        return false;
    }
    q.id = id;
    m_lastAddedId = id;
    // keep chronological order (QSOs are normally appended at the end)
    int row = m_qsos.size();
    while (row > 0 && m_qsos.at(row - 1).utc > q.utc)
        --row;
    beginInsertRows(QModelIndex(), row, row);
    m_qsos.insert(row, q);
    endInsertRows();
    emit logChanged();
    return true;
}

bool QsoLogModel::removeQsoAt(int row)
{
    if (row < 0 || row >= m_qsos.size())
        return false;
    if (!m_db->deleteQso(m_qsos.at(row).id)) {
        m_error = m_db->lastError();
        return false;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_status.remove(m_qsos.at(row).id);
    m_qsos.remove(row);
    endRemoveRows();
    emit logChanged();
    return true;
}

bool QsoLogModel::setTag(int row, bool tagged, const QString &comment)
{
    if (row < 0 || row >= m_qsos.size())
        return false;
    Qso q = m_qsos.at(row);
    q.tagged = tagged;
    q.comment = comment;
    return commit(row, q);
}

void QsoLogModel::setStatuses(const QHash<int, QString> &status)
{
    m_status = status;
    if (!m_qsos.isEmpty())
        emit dataChanged(index(0, ColStatus), index(m_qsos.size() - 1, ColStatus));
}

int QsoLogModel::nextSerial() const
{
    int maxSerial = m_rules->serialStart - 1;
    for (const Qso &q : m_qsos) {
        bool ok = false;
        const int n = q.exchSent.toInt(&ok);
        if (ok && n > maxSerial)
            maxSerial = n;
    }
    return maxSerial + 1;
}
