#include "BandSummaryWidget.h"

#include <QFontDatabase>
#include <QBrush>
#include <QHeaderView>

BandSummaryWidget::BandSummaryWidget(const QStringList &bands, QWidget *parent)
    : QTableWidget(bands.size() + 1, 1, parent), m_bands(bands)
{
    setHorizontalHeaderLabels({tr("QSOs")});
    QStringList rows = bands;
    rows << tr("Total");
    setVerticalHeaderLabels(rows);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(QFont().pointSize() + 2);
    f.setBold(true);
    for (int r = 0; r < rowCount(); ++r) {
        auto *it = new QTableWidgetItem("0");
        it->setTextAlignment(Qt::AlignCenter);
        it->setFont(f);
        setItem(r, 0, it);
    }
    const int h = horizontalHeader()->sizeHint().height()
                  + verticalHeader()->defaultSectionSize() * rowCount() + 2 * frameWidth();
    setFixedHeight(h);
    setFixedWidth(124);
    setToolTip(tr("QSOs per band"));
}

void BandSummaryWidget::setCounts(const QMap<QString, int> &counts)
{
    int total = 0;
    for (int i = 0; i < m_bands.size(); ++i) {
        const int n = counts.value(m_bands[i]);
        total += n;
        item(i, 0)->setText(QString::number(n));
    }
    item(m_bands.size(), 0)->setText(QString::number(total));
}

void BandSummaryWidget::setCurrentBand(const QString &band)
{
    m_current = band;
    for (int i = 0; i < m_bands.size(); ++i) {
        auto *h = verticalHeaderItem(i);
        QFont f = h->font();
        f.setBold(m_bands[i] == band);
        h->setFont(f);
        const bool cur = m_bands[i] == band;
        item(i, 0)->setBackground(cur ? QBrush(palette().highlight().color().lighter(170)) : QBrush());
        item(i, 0)->setForeground(cur ? QBrush(Qt::black) : QBrush());
    }
}
