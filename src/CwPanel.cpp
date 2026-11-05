#include "CwPanel.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

CwPanel::CwPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    QFont bold = font();
    bold.setBold(true);
    bold.setPointSize(bold.pointSize() + 2);

    m_mode = new QPushButton(this);
    m_mode->setFont(bold);
    m_mode->setFocusPolicy(Qt::NoFocus);
    m_mode->setMinimumWidth(70);
    m_mode->setToolTip(tr("CQ / S&&P mode — toggle with '+'"));
    connect(m_mode, &QPushButton::clicked, this, &CwPanel::modeToggled);
    lay->addWidget(m_mode);

    // speed: [▼] 20 WPM [▲]
    auto *speedBox = new QFrame(this);
    speedBox->setFrameShape(QFrame::StyledPanel);
    auto *sl = new QHBoxLayout(speedBox);
    sl->setContentsMargins(2, 0, 2, 0);
    sl->setSpacing(2);
    m_down = new QToolButton(speedBox);
    m_down->setArrowType(Qt::DownArrow);
    m_down->setToolTip(tr("Decrease CW speed by 1 WPM (PgDown)"));
    m_up = new QToolButton(speedBox);
    m_up->setArrowType(Qt::UpArrow);
    m_up->setToolTip(tr("Increase CW speed by 1 WPM (PgUp)"));
    for (QToolButton *b : {m_down, m_up}) {
        b->setFocusPolicy(Qt::NoFocus);
        b->setAutoRepeat(true);
    }
    connect(m_down, &QToolButton::clicked, this, [this] { emit speedStep(-1); });
    connect(m_up, &QToolButton::clicked, this, [this] { emit speedStep(+1); });
    m_speed = new QLabel(speedBox);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setBold(true);
    mono.setPointSize(bold.pointSize());
    m_speed->setFont(mono);
    m_speed->setAlignment(Qt::AlignCenter);
    m_speed->setMinimumWidth(QFontMetrics(mono).horizontalAdvance("60 WPM") + 6);
    sl->addWidget(m_down);
    sl->addWidget(m_speed);
    sl->addWidget(m_up);
    lay->addWidget(speedBox);

    // F1..F12
    auto *keys = new QWidget(this);
    auto *kl = new QGridLayout(keys);
    kl->setContentsMargins(0, 0, 0, 0);
    kl->setSpacing(3);
    for (int i = 0; i < 12; ++i) {
        auto *b = new QPushButton(keys);
        b->setFocusPolicy(Qt::NoFocus);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        b->setMinimumWidth(60);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setStyleSheet("QPushButton { text-align: left; padding: 2px 6px; }");
        connect(b, &QPushButton::clicked, this, [this, i] { emit keyClicked(i); });
        connect(b, &QPushButton::customContextMenuRequested, this, [this, b, i](const QPoint &p) {
            QMenu menu(this);
            menu.addAction(tr("Modify message…"), this, [this, i] { emit modifyRequested(i); });
            if (i == 11)
                menu.addAction(tr("Auto-CQ delay…"), this, &CwPanel::autoCqDelayRequested);
            menu.exec(b->mapToGlobal(p));
        });
        kl->addWidget(b, i / 6, i % 6);      // two rows: F1-F6 / F7-F12
        m_keys << b;
    }
    lay->addWidget(keys, 1);

    setCqMode(true);
    setSpeed(20, false);
}

void CwPanel::setMessages(const QStringList &messages)
{
    m_messages = messages;
    updateKeyTexts();
    for (int i = 0; i < m_keys.size(); ++i) {
        const QString msg = messages.value(i);
        m_keys[i]->setToolTip(QString("<b>F%1</b>%2<br><code>%3</code><br><i>%4</i>")
                                  .arg(i + 1)
                                  .arg(i == 11 ? tr(" — auto-CQ") : QString())
                                  .arg(msg.isEmpty() ? tr("(empty)") : msg.toHtmlEscaped())
                                  .arg(tr("Right-click to modify")));
    }
}

void CwPanel::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    QTimer::singleShot(0, this, &CwPanel::updateKeyTexts);   // after the layout has sized the buttons
}

// "F3 @ ++5NN--#" elided to the real width of each button
void CwPanel::updateKeyTexts()
{
    for (int i = 0; i < m_keys.size(); ++i) {
        QPushButton *b = m_keys[i];
        const QFontMetrics fm(b->font());
        const QString key = QString("F%1 ").arg(i + 1);
        const int room = b->width() - 14 - fm.horizontalAdvance(key);
        b->setText(key + fm.elidedText(m_messages.value(i), Qt::ElideRight, qMax(0, room)));
    }
}

void CwPanel::setSpeed(int wpm, bool available)
{
    m_speed->setText(QString("%1 WPM").arg(wpm));
    m_speed->setEnabled(available);
    m_up->setEnabled(available);
    m_down->setEnabled(available);
    m_speed->setToolTip(available ? tr("CW speed") : tr("No keyer configured / available"));
}

void CwPanel::setCqMode(bool cq)
{
    m_mode->setText(cq ? "CQ" : "S&&P");
    m_mode->setStyleSheet(cq ? "QPushButton { color: white; background: #c0392b; border-radius: 3px; padding: 3px 8px; }"
                             : "QPushButton { color: white; background: #2471a3; border-radius: 3px; padding: 3px 8px; }");
}

void CwPanel::setAutoCqActive(bool on)
{
    m_keys[11]->setStyleSheet(on ? "QPushButton { text-align: left; padding: 2px 6px; background: #f5b041; font-weight: bold; }"
                                 : "QPushButton { text-align: left; padding: 2px 6px; }");
}

void CwPanel::flashKey(int index)
{
    if (index < 0 || index >= m_keys.size())
        return;
    QPushButton *b = m_keys[index];
    b->setDown(true);
    QTimer::singleShot(180, b, [b] { b->setDown(false); });
}
