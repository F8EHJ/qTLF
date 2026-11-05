#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Object exposed to the custom QML windows as "contest".
class ContestBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap score READ score NOTIFY updated)
    Q_PROPERTY(QVariantMap info READ info NOTIFY updated)
    Q_PROPERTY(QVariantList qsos READ qsos NOTIFY updated)
    Q_PROPERTY(QVariantList bandCounts READ bandCounts NOTIFY updated)
    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY updated)
    Q_PROPERTY(QString currentBand READ currentBand NOTIFY currentBandChanged)
    // call being typed on the entry line, and what qTLF knows about it:
    //   callInfo = { call, known (in call database), <database fields: exch, name, ...>,
    //                cty: {valid, cont, country, prefix, cqz, ituz}, workedBands: [...],
    //                dupe: bool }
    Q_PROPERTY(QString entryCall READ entryCall NOTIFY entryChanged)
    Q_PROPERTY(QVariantMap callInfo READ callInfo NOTIFY entryChanged)
    // my station from the country file: {valid, cont, country, prefix, cqz, ituz}
    Q_PROPERTY(QVariantMap myCty READ myCty NOTIFY updated)
    Q_PROPERTY(bool ctyLoaded READ ctyLoaded NOTIFY updated)
public:
    using QObject::QObject;

    QVariantMap score() const { return m_score; }
    QVariantMap info() const { return m_info; }
    QVariantList qsos() const { return m_qsos; }
    QVariantList bandCounts() const { return m_bandCounts; }
    int qsoCount() const { return m_qsos.size(); }
    QString currentBand() const { return m_band; }
    QString entryCall() const { return m_entryCall; }
    QVariantMap callInfo() const { return m_callInfo; }
    QVariantMap myCty() const { return m_myCty; }
    bool ctyLoaded() const { return m_ctyLoaded; }

    void update(const QVariantMap &score, const QVariantMap &info, const QVariantList &qsos,
                const QVariantList &bandCounts, const QVariantMap &myCty, bool ctyLoaded)
    {
        m_score = score;
        m_info = info;
        m_qsos = qsos;
        m_bandCounts = bandCounts;
        m_myCty = myCty;
        m_ctyLoaded = ctyLoaded;
        emit updated();
    }
    void setCurrentBand(const QString &b)
    {
        if (b != m_band) { m_band = b; emit currentBandChanged(); }
    }
    void setEntry(const QString &call, const QVariantMap &info)
    {
        m_entryCall = call;
        m_callInfo = info;
        emit entryChanged();
    }

signals:
    void updated();
    void currentBandChanged();
    void entryChanged();

private:
    QVariantMap m_score, m_info, m_myCty, m_callInfo;
    QVariantList m_qsos, m_bandCounts;
    QString m_band, m_entryCall;
    bool m_ctyLoaded = false;
};
