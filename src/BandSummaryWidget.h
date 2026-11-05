#pragma once

#include <QMap>
#include <QStringList>
#include <QTableWidget>

// Small "QSOs per band" table shown in the corner of the main window.
class BandSummaryWidget : public QTableWidget
{
    Q_OBJECT
public:
    BandSummaryWidget(const QStringList &bands, QWidget *parent = nullptr);
    void setCounts(const QMap<QString, int> &counts);
    void setCurrentBand(const QString &band);

private:
    QStringList m_bands;
    QString m_current;
};
