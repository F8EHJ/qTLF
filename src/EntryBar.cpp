#include "EntryBar.h"

#include "QsoChecks.h"

#include "ContestRules.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFontDatabase>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringListModel>
#include <QTimer>

static QFont bigFont(int delta = 7)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(qMax(10, QFont().pointSize() + delta));
    f.setBold(true);
    return f;
}

QLineEdit *EntryBar::makeEdit(int chars, bool upper)
{
    auto *e = new QLineEdit(this);
    e->setFont(bigFont());
    const int w = QFontMetrics(e->font()).horizontalAdvance(QString(chars, 'W')) + 16;
    e->setMinimumWidth(w);
    if (upper) {
        connect(e, &QLineEdit::textEdited, e, [e](const QString &t) {
            if (t.startsWith(':'))
                return;                       // keep the case of commands (":tag Check this")
            const QString u = t.toUpper();
            if (u != t) {
                const int pos = e->cursorPosition();
                e->setText(u);
                e->setCursorPosition(pos);
            }
        });
    }
    connect(e, &QLineEdit::returnPressed, this, [this, e] {
        if (e == m_call) onCallReturn();
        else emit enterPressed(ExchField);   // exchange, RST and exch-sent fields
    });
    e->installEventFilter(this);
    return e;
}

EntryBar::EntryBar(const ContestRules &rules, QWidget *parent) : QWidget(parent), m_rules(rules)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(2);

    // minimum widths (characters) chosen so that the window fits a 1366 px screen (LG W1941S,
    // laptops) with the docks; every field grows when there is room
    m_freqEdit = makeEdit(5, false);
    m_freqEdit->setPlaceholderText("kHz");
    m_freqEdit->setAlignment(Qt::AlignRight);
    m_freqEdit->setToolTip(tr("Frequency in kHz"));
    disconnect(m_freqEdit, &QLineEdit::returnPressed, nullptr, nullptr);
    connect(m_freqEdit, &QLineEdit::editingFinished, this, &EntryBar::onFreqEdited);
    connect(m_freqEdit, &QLineEdit::returnPressed, this, [this] { focusCall(); });

    m_bandCombo = new QComboBox(this);
    m_bandCombo->setFont(bigFont());
    m_bandCombo->addItems(rules.bandNames());
    m_bandCombo->setFocusPolicy(Qt::ClickFocus);
    connect(m_bandCombo, &QComboBox::currentTextChanged, this, [this](const QString &b) {
        if (m_settingBand)
            return;
        // band picked by hand: drop a frequency that is not in this band
        if (m_freq > 0 && m_rules.bandForFrequency(m_freq) != b) {
            m_freq = 0;
            m_freqEdit->clear();
        }
        updateFreqStyle();
        emit bandChanged(b);
    });

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setFont(bigFont());
    m_modeCombo->addItems(rules.modes);
    m_modeCombo->setFocusPolicy(Qt::ClickFocus);
    m_bandCombo->installEventFilter(this);       // TAB from a combo goes back to Call
    m_modeCombo->installEventFilter(this);

    m_date = new QLabel(this);
    m_time = new QLabel(this);
    for (QLabel *l : {m_date, m_time}) {
        l->setFont(bigFont(3));               // date / time: information only, smaller font
        l->setAlignment(Qt::AlignCenter);
        l->setFrameShape(QFrame::StyledPanel);
        l->setMinimumHeight(m_freqEdit->sizeHint().height());
    }
    m_date->setToolTip(tr("UTC date (system clock)"));
    m_time->setToolTip(tr("UTC time (system clock) - taken when ENTER is pressed"));

    m_call = makeEdit(8, true);
    m_call->setPlaceholderText(tr("CALL"));
    // room for the (translated) placeholder: CALL, INDICATIF, NOMINATIVO, RUFZEICHEN…
    m_call->setMinimumWidth(qMax(m_call->minimumWidth(),
                                 QFontMetrics(m_call->font()).horizontalAdvance(m_call->placeholderText()) + 20));
    connect(m_call, &QLineEdit::textChanged, this, [this] { emit callEdited(call()); });
    m_rstS = makeEdit(3, true);
    m_rstR = makeEdit(3, true);
    // minimum widths (in characters) kept small so the window fits a 1366 px screen; fields grow
    m_exchR = makeEdit(rules.exchRcvdType == "callsign" ? 7 : 5, true);
    m_exchS = makeEdit(rules.exchSentType == "serial" ? 4 : 5, true);
    m_callModel = new QStringListModel(this);
    m_exchModel = new QStringListModel(this);
    m_callComp = makeCompleter(m_call, m_callModel);
    m_exchComp = makeCompleter(m_exchR, m_exchModel);
    m_call->setCompleter(m_callComp);
    m_exchR->setCompleter(m_exchComp);
    m_rstS->setText(rules.rstSentDefault);
    m_rstR->setText(rules.rstRcvdDefault);
    for (QLineEdit *e : {m_rstS, m_rstR, m_exchS})
        e->setFocusPolicy(Qt::ClickFocus);
    m_call->setStyleSheet("QLineEdit { border: 2px solid palette(highlight); }");
    m_exchR->setStyleSheet("QLineEdit { border: 2px solid palette(highlight); }");

    m_logBtn = new QPushButton(tr("Log"), this);
    m_logBtn->setFont(bigFont(3));
    m_logBtn->setFocusPolicy(Qt::ClickFocus);     // reached by ENTER in S&P mode
    m_logBtn->setToolTip(tr("Log the QSO"));
    m_logBtn->setStyleSheet("QPushButton:focus { border: 3px solid palette(highlight); font-weight: bold; }");
    m_logBtn->installEventFilter(this);
    connect(m_logBtn, &QPushButton::clicked, this, &EntryBar::submitRequested);

    struct Col { QString label; QWidget *w; };
    QList<Col> cols = {
        {tr("Freq (kHz)"), m_freqEdit},
        {tr("Band"), m_bandCombo},
        {tr("Mode"), m_modeCombo},
        {tr("Date (UTC)"), m_date},
        {tr("Time (UTC)"), m_time},
        {tr("Call"), m_call},
        {tr("RST S"), m_rstS},
        {tr("RST R"), m_rstR},
        {rules.exchRcvdLabel, m_exchR},
        {rules.exchSentLabel, m_exchS},
        {QString(), m_logBtn},
    };
    int c = 0;
    for (const Col &col : cols) {
        auto *lab = new QLabel(col.label, this);
        QFont lf = lab->font();
        lf.setPointSize(qMax(7, lf.pointSize() - 1));
        lab->setFont(lf);
        lab->setEnabled(false); // dimmed caption
        grid->addWidget(lab, 0, c);
        grid->addWidget(col.w, 1, c);
        ++c;
    }
    grid->setColumnStretch(5, 3);   // call
    grid->setColumnStretch(8, 2);   // exch rcvd
    if (rules.modes.size() < 2) {
        m_modeCombo->hide();
        grid->itemAtPosition(0, 2)->widget()->hide();
    }

    m_msg = new QLabel(this);
    m_msg->setTextFormat(Qt::PlainText);
    m_msg->setMinimumHeight(QFontMetrics(m_msg->font()).height() + 4);
    grid->addWidget(m_msg, 2, 0, 1, c);

    setTabOrder(m_call, m_exchR);

    m_clock = new QTimer(this);
    connect(m_clock, &QTimer::timeout, this, &EntryBar::updateClock);
    m_clock->start(250);
    updateClock();
    updateFreqStyle();
}

void EntryBar::updateClock()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_date->setText(now.date().toString("yyyy-MM-dd"));
    m_time->setText(now.time().toString("HH:mm:ss"));
}

// Drop-down lists of the Call and Exch Rcvd fields: the lines containing what is typed,
// in the order of the list (log of the contest first, then the call_exchange file).
QCompleter *EntryBar::makeCompleter(QLineEdit *edit, QStringListModel *model)
{
    auto *comp = new QCompleter(model, edit);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    comp->setCompletionMode(QCompleter::PopupCompletion);
    comp->setFilterMode(Qt::MatchContains);
    comp->setModelSorting(QCompleter::UnsortedModel);   // keep our order: log first
    comp->setMaxVisibleItems(12);
    comp->popup()->installEventFilter(this);            // ENTER, TAB, ESC, +, PgUp/PgDn in the list
    return comp;
}

void EntryBar::setCallCompletion(const QStringList &list)
{
    m_callModel->setStringList(list);           // the completer stays attached; an empty list shows nothing
}

void EntryBar::setExchCompletion(const QStringList &list)
{
    m_exchModel->setStringList(list);
}

void EntryBar::setFrequency(double khz)
{
    m_freq = khz;
    if (!m_freqEdit->hasFocus())
        m_freqEdit->setText(khz > 0 ? QString::number(khz, 'f', khz == qRound64(khz) ? 0 : 1) : QString());
    const QString b = m_rules.bandForFrequency(khz);
    if (!b.isEmpty() && b != m_bandCombo->currentText()) {
        m_settingBand = true;
        m_bandCombo->setCurrentText(b);
        m_settingBand = false;
        emit bandChanged(b);
    }
    updateFreqStyle();
}

void EntryBar::onFreqEdited()
{
    const QString t = m_freqEdit->text().trimmed();
    if (t.isEmpty()) {
        m_freq = 0;
        updateFreqStyle();
        return;
    }
    bool ok = false;
    double f = t.toDouble(&ok);
    if (!ok || f <= 0) {
        setMessage(tr("Invalid frequency '%1'").arg(t), "error");
        return;
    }
    if (f < 100)
        f *= 1000.0; // MHz
    setFrequency(f);
    emit frequencyEntered(f);
}

void EntryBar::updateFreqStyle()
{
    if (m_freq > 0 && m_rules.bandForFrequency(m_freq).isEmpty()) {
        m_freqEdit->setStyleSheet("QLineEdit { background: rgba(220,53,69,90); }");
        m_freqEdit->setToolTip(tr("Frequency is outside the bands allowed by the rules"));
    } else if (m_freq > 0 && !m_rules.inSegment(m_freq)) {
        m_freqEdit->setStyleSheet("QLineEdit { background: rgba(255,193,7,90); }");
        m_freqEdit->setToolTip(tr("Frequency is outside the contest segment"));
    } else {
        m_freqEdit->setStyleSheet(QString());
        m_freqEdit->setToolTip(tr("Frequency in kHz"));
    }
}

QString EntryBar::band() const
{
    if (m_freq > 0) {
        const QString b = m_rules.bandForFrequency(m_freq);
        if (!b.isEmpty())
            return b;
    }
    return m_bandCombo->currentText();
}

void EntryBar::setBand(const QString &band)
{
    if (const BandDef *b = m_rules.band(band))
        m_bandCombo->setCurrentText(b->name);
}

QString EntryBar::mode() const { return m_modeCombo->currentText(); }
void EntryBar::setMode(const QString &mode)
{
    const int i = m_modeCombo->findText(mode, Qt::MatchFixedString);
    if (i >= 0) m_modeCombo->setCurrentIndex(i);
}

QString EntryBar::call() const { return m_call->text().trimmed().toUpper(); }
QString EntryBar::rstSent() const { return m_rstS->text().trimmed(); }
QString EntryBar::rstRcvd() const { return m_rstR->text().trimmed(); }
QString EntryBar::exchRcvd() const { return m_exchR->text().trimmed().toUpper(); }
QString EntryBar::exchSent() const { return m_exchS->text().trimmed().toUpper(); }
void EntryBar::setExchSent(const QString &s) { m_exchS->setText(s); }
void EntryBar::setRstSent(const QString &s) { m_rstS->setText(s); }
void EntryBar::setRstRcvd(const QString &s) { m_rstR->setText(s); }

void EntryBar::clearForNext()
{
    m_call->clear();
    m_exchR->clear();
    m_rstS->setText(m_rules.rstSentDefault);
    m_rstR->setText(m_rules.rstRcvdDefault);
    focusCall();
}

void EntryBar::clearEntry()
{
    m_call->clear();
    m_exchR->clear();
    setMessage(QString());
    focusCall();
}

void EntryBar::focusCall()
{
    m_call->setFocus();
    m_call->end(false);
}

void EntryBar::focusExchRcvd()
{
    m_exchR->setFocus();
    m_exchR->selectAll();
}

void EntryBar::goToExchange()
{
    lookupExchange();
    focusExchRcvd();
}

void EntryBar::clearCall() { m_call->clear(); }
void EntryBar::clearExchRcvd() { m_exchR->clear(); }

void EntryBar::focusLogButton()
{
    m_logBtn->setFocus(Qt::OtherFocusReason);
}

void EntryBar::setMessage(const QString &text, const QString &level)
{
    QString color;
    if (level == "error") color = "#dc3545";
    else if (level == "warn") color = "#e0a800";
    else if (level == "ok") color = "#28a745";
    m_msg->setStyleSheet(color.isEmpty() ? QString() : QString("QLabel { color: %1; font-weight: bold; }").arg(color));
    m_msg->setText(text);
}

void EntryBar::onCallReturn()
{
    const QString t = m_call->text().trimmed();
    static const QRegularExpression freqRe(R"(^\d+([.,]\d+)?$)");
    if (t.startsWith(':') || freqRe.match(t).hasMatch()) {
        m_call->clear();
        emit commandEntered(t);
        return;
    }
    emit enterPressed(CallField);
}

void EntryBar::lookupExchange()
{
    if (!m_exchR->text().trimmed().isEmpty() || !m_exchangeLookup || call().isEmpty())
        return;
    const QString x = m_exchangeLookup(call());
    if (!x.isEmpty())
        m_exchR->setText(x);
}

bool EntryBar::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        const int key = ke->key();
        // a key pressed while a completion list is open counts as pressed in its field
        QLineEdit *popupOf = nullptr;
        if (obj == m_callComp->popup())
            popupOf = m_call;
        else if (obj == m_exchComp->popup())
            popupOf = m_exchR;
        const bool popup = popupOf != nullptr;
        QObject *src = popup ? popupOf : obj;
        auto hidePopup = [&] {
            if (popupOf)
                popupOf->completer()->popup()->hide();
        };
        const bool qsoField = src == m_call || src == m_exchR || src == m_rstS || src == m_rstR
                              || src == m_exchS || src == m_logBtn;
        const bool entryWidget = qsoField || src == m_freqEdit || src == m_bandCombo || src == m_modeCombo;

        // TAB / Shift+TAB never leave the entry line:
        //   Call -> Exch Rcvd (pre-filled)   Exch Rcvd -> Call   Log button -> Exch Rcvd   others -> Call
        if (entryWidget && (key == Qt::Key_Tab || key == Qt::Key_Backtab)) {
            hidePopup();
            if (src == m_call)
                goToExchange();
            else if (src == m_logBtn)
                focusExchRcvd();
            else
                focusCall();
            return true;
        }
        // Ctrl+ENTER: log the QSO without any CW message
        if (qsoField && (key == Qt::Key_Return || key == Qt::Key_Enter) && (ke->modifiers() & Qt::ControlModifier)) {
            hidePopup();
            emit logRequested();
            return true;
        }
        if (src == m_logBtn && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
            emit enterPressed(LogButton);
            return true;
        }
        // ENTER in an open list with no line chosen: the list closes and ENTER acts in the field
        if (popup && (key == Qt::Key_Return || key == Qt::Key_Enter)
            && !popupOf->completer()->popup()->currentIndex().isValid()) {
            hidePopup();
            if (popupOf == m_call) onCallReturn();
            else emit enterPressed(ExchField);
            return true;
        }
        if (key == Qt::Key_Escape && (qsoField || src == m_freqEdit)) {
            hidePopup();                                   // ESC in a list = ESC in its field
            emit escapePressed(src == m_call ? CallField : src == m_exchR ? ExchField
                               : src == m_logBtn ? LogButton : OtherField);
            return true;
        }
        if (qsoField && key == Qt::Key_Plus && !(src == m_call && m_call->text().startsWith(':'))) {
            hidePopup();
            emit modeToggleRequested();
            return true;
        }
        if ((src == m_call || src == m_exchR) && (key == Qt::Key_PageUp || key == Qt::Key_PageDown)) {
            hidePopup();
            emit speedKey(key == Qt::Key_PageUp ? +1 : -1, src == m_call ? CallField : ExchField);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}
