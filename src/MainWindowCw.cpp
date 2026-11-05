// CW keyer part of MainWindow: function keys, smart ENTER, CQ / S&P, speed, auto-CQ.
#include "MainWindow.h"

#include "CwPanel.h"
#include "EntryBar.h"
#include "Keyer.h"

#include <QInputDialog>
#include <QLabel>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>

void MainWindow::setupCw()
{
    m_autoCqTimer = new QTimer(this);
    m_autoCqTimer->setSingleShot(true);
    connect(m_autoCqTimer, &QTimer::timeout, this, &MainWindow::autoCqTick);

    connect(m_cwPanel, &CwPanel::keyClicked, this, &MainWindow::onFunctionKey);
    connect(m_cwPanel, &CwPanel::modifyRequested, this, &MainWindow::editCwMessage);
    connect(m_cwPanel, &CwPanel::autoCqDelayRequested, this, &MainWindow::editAutoCqDelay);
    connect(m_cwPanel, &CwPanel::modeToggled, this, &MainWindow::toggleCqMode);
    connect(m_cwPanel, &CwPanel::speedStep, this, &MainWindow::changeSpeed);

    connect(m_entry, &EntryBar::enterPressed, this, &MainWindow::onEnter);
    connect(m_entry, &EntryBar::logRequested, this, [this] {   // Ctrl+ENTER: log, no CW, no exchange check
        stopAutoCq();
        logQso(true);
    });
    connect(m_entry, &EntryBar::escapePressed, this, &MainWindow::onEscape);
    connect(m_entry, &EntryBar::modeToggleRequested, this, &MainWindow::toggleCqMode);
    connect(m_entry, &EntryBar::speedKey, this, &MainWindow::onSpeedKey);
    connect(m_entry, &EntryBar::callEdited, this, [this](const QString &call) {
        if (m_autoCq && !call.isEmpty())
            stopAutoCq();               // someone answered: typing a call stops auto-CQ
    });

    for (int i = 0; i < 12; ++i) {
        auto *sc = new QShortcut(QKeySequence(Qt::Key_F1 + i), this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, [this, i] { onFunctionKey(i); });
    }

    applyCwConfig();
}

void MainWindow::applyCwConfig()
{
    stopAutoCq();
    m_cw = CwConfig::load();
    if (m_cw.myCall.isEmpty())
        m_cw.myCall = m_contest.myCall;

    delete m_keyer;
    m_keyer = Keyer::create(m_cw, this);
    if (m_keyer) {
        QString err;
        if (!m_keyer->start(&err)) {
            statusBar()->showMessage(tr("Keyer error: %1").arg(err), 8000);
            delete m_keyer;
            m_keyer = nullptr;
        } else {
            connect(m_keyer, &Keyer::error, this, [this](const QString &e) {
                statusBar()->showMessage(e, 5000);
            });
        }
    }
    m_speed = m_cw.defaultSpeed;
    if (m_keyer)
        m_keyer->setSpeed(m_speed);

    m_cwPanel->setMessages(m_cw.messages);
    m_cwPanel->setSpeed(m_speed, m_keyer != nullptr);
    m_cwPanel->setCqMode(m_cqMode);
    m_keyerLabel->setText(m_keyer ? tr("Keyer: %1").arg(m_keyer->description()) : tr("Keyer: none"));
}

void MainWindow::sendCwMessage(int index)
{
    if (index >= 0 && index < 12)
        m_cwPanel->flashKey(index);
    sendCwText(CwConfig::messageName(index), m_cw.messages.value(index));
}

// Expands the macros of 'message' (% @ # [) and sends it; 'name' is shown in the status bar.
void MainWindow::sendCwText(const QString &name, const QString &message)
{
    const CwConfig::Fields f{m_entry->call(), m_entry->exchSent(), m_entry->rstSent()};
    const QString text = m_cw.expandText(message, f);
    if (text.trimmed().isEmpty())
        return;
    if (m_keyer) {
        m_keyer->sendText(text);
        statusBar()->showMessage(tr("CW %1 ▶ %2").arg(name, text), 4000);
    } else {
        statusBar()->showMessage(tr("CW %1 (no keyer) ▶ %2").arg(name, text), 4000);
    }
}

void MainWindow::onFunctionKey(int index)
{
    if (index == 11) {                       // F12 (key or click) = auto-CQ on / off
        if (m_autoCq) {
            stopAutoCq();                    // sends the keyer stop code
        } else {
            if (m_keyer)
                m_keyer->stop();             // F12 always stops the CW being sent first
            startAutoCq();
        }
        return;
    }
    stopAutoCq();
    sendCwMessage(index);
}

// ENTER sequence, see the table in showHelp().
void MainWindow::onEnter(EntryBar::Field field)
{
    stopAutoCq();
    const bool callEmpty = m_entry->call().isEmpty();
    const bool exchEmpty = m_entry->exchRcvd().isEmpty();

    // TU message of the current mode, then log. Expanded before logging: # = current serial
    auto sendAndLog = [this](int tuMessage) {
        if (!checkEntry())
            return;
        sendCwMessage(tuMessage);
        logQso();
    };

    // DUPE: ENTER from the Call field sends message 7 (QSO B4) in both modes and stays there.
    // ESC clears the call; from the exchange field / Log button a dupe can still be logged.
    if (field == EntryBar::CallField && !callEmpty && isDupe(m_entry->call())) {
        sendCwMessage(6);
        m_entry->setMessage(tr("DUPE — %1 already worked on %2: QSO B4 sent").arg(m_entry->call(), m_entry->band()),
                            "error");
        return;
    }

    if (m_cqMode) {
        switch (field) {
        case EntryBar::CallField:
            if (callEmpty) sendCwMessage(0);                              // F1 CQ
            else { sendCwMessage(2); m_entry->goToExchange(); }          // F3 exchange
            break;
        case EntryBar::ExchField:
            if (exchEmpty) sendCwMessage(2);                              // F3 again
            else sendAndLog(CwConfig::CqTuMsg);                           // CQ_TU_MSG + log
            break;
        case EntryBar::LogButton:
            sendAndLog(CwConfig::CqTuMsg);                                // CQ_TU_MSG (13) + log
            break;
        case EntryBar::OtherField:
            break;
        }
    } else {
        switch (field) {
        case EntryBar::CallField:
            if (callEmpty) sendCwText(tr("my call"), "%");                // my call (macro %)
            else { sendCwMessage(1); m_entry->goToExchange(); }          // F2 my call
            break;
        case EntryBar::ExchField:
            if (exchEmpty) sendCwMessage(1);                              // F2 again
            else { sendCwMessage(CwConfig::SpTuMsg); m_entry->focusLogButton(); }   // S&P_TU_MSG (14)
            break;
        case EntryBar::LogButton:
            logQso();                                                     // log, no CW (sent before)
            break;
        case EntryBar::OtherField:
            break;
        }
    }
}

// ESC always stops the CW (keyer-specific stop code, cwdaemon: <ESC>"4") and auto-CQ, then:
//   Call empty          -> clear Call and exchange received
//   Call filled         -> clear Call
//   Exchange empty      -> back to Call
//   Exchange filled     -> clear the exchange
//   Log button / other  -> back to Call
void MainWindow::onEscape(EntryBar::Field field)
{
    const bool autoCqWasOn = m_autoCq;
    stopAutoCq();                            // sends the stop code itself if auto-CQ was on
    if (m_keyer && !autoCqWasOn)
        m_keyer->stop();                     // exactly one stop per ESC

    switch (field) {
    case EntryBar::CallField:
        if (m_entry->call().isEmpty())
            m_entry->clearExchRcvd();
        m_entry->clearCall();
        m_entry->setMessage(QString());
        break;
    case EntryBar::ExchField:
        if (m_entry->exchRcvd().isEmpty())
            m_entry->focusCall();
        else
            m_entry->clearExchRcvd();
        break;
    case EntryBar::LogButton:
    case EntryBar::OtherField:
        m_entry->focusCall();
        break;
    }
}

void MainWindow::toggleCqMode()
{
    m_cqMode = !m_cqMode;
    if (!m_cqMode)
        stopAutoCq();
    m_cwPanel->setCqMode(m_cqMode);
    statusBar()->showMessage(m_cqMode ? tr("CQ mode") : tr("S&P mode"), 2000);
}

static QString bumpRst(QString rst, int delta)
{
    if (rst.size() < 2 || !rst[1].isDigit())
        return rst;
    rst[1] = QChar('0' + qBound(1, rst[1].digitValue() + delta, 9));
    return rst;
}

void MainWindow::onSpeedKey(int delta, EntryBar::Field field)
{
    // CALL filled: PgUp/PgDn change the 2nd digit of the RST instead of the speed
    //   cursor in CALL              -> RST sent
    //   cursor in exchange received -> RST received
    // CALL empty: CW speed +/- 1 WPM
    if (!m_entry->call().isEmpty()) {
        if (field == EntryBar::CallField) {
            m_entry->setRstSent(bumpRst(m_entry->rstSent(), delta));
            return;
        }
        if (field == EntryBar::ExchField) {
            m_entry->setRstRcvd(bumpRst(m_entry->rstRcvd(), delta));
            return;
        }
    }
    changeSpeed(delta);
}

void MainWindow::changeSpeed(int delta)
{
    if (!m_keyer)
        return;                              // greyed: no keyer
    const int s = qBound(4, m_speed + delta, 60);      // 1 WPM steps
    if (s == m_speed)
        return;
    m_speed = s;
    m_keyer->setSpeed(m_speed);              // <ESC>"2"<wpm>
    m_cwPanel->setSpeed(m_speed, true);
}

void MainWindow::startAutoCq()
{
    if (m_cw.messages.value(11).trimmed().isEmpty()) {
        statusBar()->showMessage(tr("F12 (auto-CQ) message is empty"), 3000);
        return;
    }
    if (!m_cqMode)
        toggleCqMode();
    m_autoCq = true;
    m_cwPanel->setAutoCqActive(true);
    autoCqTick();
}

// Auto-CQ off. If it was running, the CW being sent is stopped too (keyer stop code):
// F12 again / click on F12, ESC, first character typed in an empty Call, ENTER, another F key…
void MainWindow::stopAutoCq()
{
    if (!m_autoCqTimer)
        return;
    if (m_autoCq && m_keyer)
        m_keyer->stop();
    m_autoCq = false;
    m_autoCqTimer->stop();
    m_cwPanel->setAutoCqActive(false);
}

void MainWindow::autoCqTick()
{
    if (!m_autoCq)
        return;
    const CwConfig::Fields f{m_entry->call(), m_entry->exchSent(), m_entry->rstSent()};
    const QString text = m_cw.expand(11, f);
    sendCwMessage(11);
    // next call = estimated sending time of the message + the configured pause
    m_autoCqTimer->start(CwConfig::durationMs(text, m_speed) + m_cw.autoCqMs);
    statusBar()->showMessage(tr("Auto-CQ ▶ %1   (every %2 s + %3 ms)")
                                 .arg(text)
                                 .arg(CwConfig::durationMs(text, m_speed) / 1000.0, 0, 'f', 1)
                                 .arg(m_cw.autoCqMs),
                             m_autoCqTimer->interval());
}

void MainWindow::editCwMessage(int index)
{
    bool ok = false;
    const QString label = tr("<b>%1</b> — %2<br><br>"
                             "Macros: <b>%</b> my call, <b>@</b> his call, <b>#</b> exchange sent, <b>[</b> RST sent<br>"
                             "Prosigns: <b>*</b> AR <b>=</b> BT <b>&lt;</b> SK <b>(</b> KN <b>!</b> SN <b>&amp;&amp;</b> AS "
                             "<b>&gt;</b> BK — <b>+</b>/<b>-</b> speed ±2 WPM")
                              .arg(CwConfig::messageName(index), CwConfig::messageHint(index).toHtmlEscaped());
    const QString t = QInputDialog::getText(this, tr("Modify CW message"), label, QLineEdit::Normal,
                                            m_cw.messages.value(index), &ok);
    if (ok) {
        m_cw.messages[index] = t;
        m_cw.save();
        m_cwPanel->setMessages(m_cw.messages);
    }
    m_entry->focusCall();
}

void MainWindow::editAutoCqDelay()
{
    bool ok = false;
    const int v = QInputDialog::getInt(this, tr("Auto-CQ"), tr("Pause between two auto-CQ calls (ms):"),
                                       m_cw.autoCqMs, 500, 50000, 500, &ok);
    if (ok) {
        m_cw.autoCqMs = qBound(500, (v + 250) / 500 * 500, 50000);
        m_cw.save();
    }
    m_entry->focusCall();
}
