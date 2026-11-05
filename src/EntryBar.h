#pragma once

#include <QHash>
#include <functional>
#include <QWidget>

class QComboBox;
class QCompleter;
class QStringListModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class ContestRules;

// The QSO entry line: Freq/Band | Mode | Date | Time | Call | RST S | RST R | Exch Rcvd | Exch Sent | Log
// Keys: TAB toggles Call <-> Exch Rcvd, ENTER = smart key (handled by MainWindow),
// ESC = escapePressed(field), '+' = CQ/S&P toggle, PgUp/PgDn = speed (or RST), Ctrl+ENTER = log without CW.
// TAB only moves between Call <-> Exch Rcvd (Log button -> Exch Rcvd, any other field -> Call).
// In the Call field a number is a QSY (e.g. 7025 or 3.535) and ":xxx" is a command.
class EntryBar : public QWidget
{
    Q_OBJECT
public:
    enum Field { CallField, ExchField, LogButton, OtherField };   // OtherField: RST S/R, exch sent
    Q_ENUM(Field)

    explicit EntryBar(const ContestRules &rules, QWidget *parent = nullptr);

    // drop-down lists ("" = none): Call = calls of the log then of call_exchange,
    // Exch Rcvd (exchange.received.completion) = exchanges of the log then of call_exchange
    void setCallCompletion(const QStringList &list);
    void setExchCompletion(const QStringList &list);
    // exchange received pre-filled by TAB from the Call field (log, then call database…), "" = none
    void setExchangeLookup(std::function<QString(const QString &call)> f) { m_exchangeLookup = std::move(f); }

    double frequency() const { return m_freq; }
    void setFrequency(double khz);
    QString band() const;
    void setBand(const QString &band);
    QString mode() const;
    void setMode(const QString &mode);

    QString call() const;
    QString rstSent() const;
    QString rstRcvd() const;
    QString exchRcvd() const;
    QString exchSent() const;
    void setExchSent(const QString &s);
    void setRstSent(const QString &s);
    void setRstRcvd(const QString &s);

    void clearForNext();
    void clearEntry();          // Ctrl+L: Call + Exch Rcvd emptied, message cleared, cursor in Call
    void focusCall();
    void focusExchRcvd();
    void goToExchange();        // like TAB from the Call field (call-history pre-fill + focus)
    void focusLogButton();
    void clearCall();
    void clearExchRcvd();
    void setMessage(const QString &text, const QString &level = "info"); // info | warn | error | ok

signals:
    void submitRequested();                 // Log button clicked with the mouse
    void enterPressed(EntryBar::Field field);
    void logRequested();                    // Ctrl+ENTER: log without CW
    void escapePressed(EntryBar::Field field);
    void modeToggleRequested();             // '+'
    void speedKey(int delta, EntryBar::Field field);   // PgUp +1 / PgDn -1
    void commandEntered(const QString &cmd);
    void callEdited(const QString &call);
    void bandChanged(const QString &band);
    void frequencyEntered(double khz);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void updateClock();
    void onFreqEdited();
    void onCallReturn();
    void lookupExchange();
    void updateFreqStyle();
    QLineEdit *makeEdit(int chars, bool upper);
    QCompleter *makeCompleter(QLineEdit *edit, QStringListModel *model);

    const ContestRules &m_rules;
    std::function<QString(const QString &)> m_exchangeLookup;
    double m_freq = 0;
    bool m_settingBand = false;

    QCompleter *m_callComp = nullptr, *m_exchComp = nullptr;
    QStringListModel *m_callModel = nullptr, *m_exchModel = nullptr;
    QLineEdit *m_freqEdit, *m_call, *m_rstS, *m_rstR, *m_exchR, *m_exchS;
    QComboBox *m_bandCombo, *m_modeCombo;
    QLabel *m_date, *m_time, *m_msg;
    QPushButton *m_logBtn;
    QTimer *m_clock;
};
