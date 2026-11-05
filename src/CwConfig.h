#pragma once

#include <QString>
#include <QStringList>

// CW keyer configuration, stored in the qTLF INI file ([keyer] and [cw] groups).
//
// Message macros (expanded by CwConfig::expand):
//   %  my call            @  call field ("?" if empty)
//   #  exchange sent      [  RST sent
// Passed through unchanged, handled by cwdaemon:
//   * AR   = BT   < SK   ( KN   ! SN   & AS   > BK      + / -  speed +/- 2 wpm
struct CwConfig
{
    static constexpr int MessageCount = 14;     // F1..F12, CQ_TU_MSG (13), S&P_TU_MSG (14)
    static constexpr int CqTuMsg = 12;          // index of message 13
    static constexpr int SpTuMsg = 13;          // index of message 14

    QString keyerType = "cwdaemon";             // "none" | "cwdaemon"
    QString address = "localhost";
    int port = 6789;
    QString initHex;                            // e.g. "1B 30" sent once when the keyer starts
    int defaultSpeed = 20;                      // wpm
    int autoCqMs = 3000;                        // pause between two auto-CQ calls (500..50000)
    QString myCall;                             // macro %

    QStringList messages;                       // MessageCount entries

    static CwConfig load();
    void save() const;
    static QStringList defaultMessages();
    static QString messageName(int index);      // "F1", ..., "CQ_TU_MSG", "S&P_TU_MSG"
    static QString messageHint(int index);      // "CQ message", ...

    struct Fields {
        QString call, exchSent, rstSent;
    };
    QString expand(int index, const Fields &f) const;
    QString expandText(const QString &msg, const Fields &f) const;

    // Approximate sending time of an expanded message at the given speed (PARIS timing,
    // '+'/'-' speed changes included). Used to time auto-CQ.
    static int durationMs(const QString &text, int wpm);
};
