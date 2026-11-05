#include "CwConfig.h"

#include <QCoreApplication>
#include <QHash>
#include <QSettings>

QStringList CwConfig::defaultMessages()
{
    return {
        "CQ de % TEST",         // F1  CQ
        "@ de %",               // F2  S&P call
        "@ ++5NN--#",           // F3  exchange
        "TU 73 %",              // F4  TU
        "@",                    // F5  his call
        "%",                    // F6  my call
        "@ sri qso b4 gl",      // F7  QSO B4
        "AGN",                  // F8
        "",                     // F9
        "QRZ?",                 // F10
        "QRZ?",                 // F11
        "+++TEST %---",         // F12 auto-CQ
        "TU %",                 // 13 CQ_TU_MSG
        "TU 5NN #",             // 14 S&P_TU_MSG
    };
}

QString CwConfig::messageName(int i)
{
    if (i == CqTuMsg) return "CQ_TU_MSG";
    if (i == SpTuMsg) return "S&P_TU_MSG";
    return QString("F%1").arg(i + 1);
}

QString CwConfig::messageHint(int i)
{
    // QT_TRANSLATE_NOOP: marked for the translation files, translated below (context "CwConfig")
    static const char *hints[MessageCount] = {
        QT_TRANSLATE_NOOP("CwConfig", "CQ message"), QT_TRANSLATE_NOOP("CwConfig", "S&P call message"),
        QT_TRANSLATE_NOOP("CwConfig", "Exchange message"), QT_TRANSLATE_NOOP("CwConfig", "TU message"),
        QT_TRANSLATE_NOOP("CwConfig", "Call of the other station"), QT_TRANSLATE_NOOP("CwConfig", "Your call"),
        QT_TRANSLATE_NOOP("CwConfig", "QSO B4 message"), "AGN", "",
        QT_TRANSLATE_NOOP("CwConfig", "QRZ message"), QT_TRANSLATE_NOOP("CwConfig", "QRZ message"),
        QT_TRANSLATE_NOOP("CwConfig", "Auto-CQ message"),
        QT_TRANSLATE_NOOP("CwConfig", "Exchange message in CQ mode (logs the QSO)"),
        QT_TRANSLATE_NOOP("CwConfig", "Exchange message in S&P mode"),
    };
    return hints[i][0] ? QCoreApplication::translate("CwConfig", hints[i]) : QString();
}

CwConfig CwConfig::load()
{
    CwConfig c;
    QSettings s;
    s.beginGroup("keyer");
    c.keyerType = s.value("type", c.keyerType).toString();
    c.address = s.value("address", c.address).toString();
    c.port = s.value("port", c.port).toInt();
    c.initHex = s.value("init_hex", c.initHex).toString();
    c.defaultSpeed = qBound(4, s.value("default_speed", c.defaultSpeed).toInt(), 60);
    c.autoCqMs = qBound(500, s.value("auto_cq_delay_ms", c.autoCqMs).toInt(), 50000);
    s.endGroup();
    c.myCall = s.value("station/call").toString();

    const QStringList def = defaultMessages();
    s.beginGroup("cw");
    for (int i = 0; i < MessageCount; ++i)
        c.messages << s.value(QString("message%1").arg(i + 1), def[i]).toString();
    s.endGroup();
    return c;
}

void CwConfig::save() const
{
    QSettings s;
    s.beginGroup("keyer");
    s.setValue("type", keyerType);
    s.setValue("address", address);
    s.setValue("port", port);
    s.setValue("init_hex", initHex);
    s.setValue("default_speed", defaultSpeed);
    s.remove("change_rst");                     // obsolete option (now always on)
    s.setValue("auto_cq_delay_ms", autoCqMs);
    s.endGroup();
    s.beginGroup("cw");
    for (int i = 0; i < MessageCount && i < messages.size(); ++i)
        s.setValue(QString("message%1").arg(i + 1), messages[i]);
    s.endGroup();
}

QString CwConfig::expandText(const QString &msg, const Fields &f) const
{
    QString out;
    out.reserve(msg.size() + 16);
    for (const QChar ch : msg) {
        switch (ch.unicode()) {
        case '%': out += myCall; break;
        case '@': out += f.call.isEmpty() ? QStringLiteral("?") : f.call; break;
        case '#': out += f.exchSent; break;
        case '[': out += f.rstSent; break;
        default: out += ch; break;
        }
    }
    return out.toUpper();
}

QString CwConfig::expand(int index, const Fields &f) const
{
    if (index < 0 || index >= messages.size())
        return QString();
    return expandText(messages[index], f);
}

// Morse length in dot units, including the 3-unit gap after the character.
static int charUnits(QChar c)
{
    static const QHash<QChar, QString> morse = [] {
        QHash<QChar, QString> m;
        const char *table[][2] = {
            {"A", ".-"}, {"B", "-..."}, {"C", "-.-."}, {"D", "-.."}, {"E", "."}, {"F", "..-."},
            {"G", "--."}, {"H", "...."}, {"I", ".."}, {"J", ".---"}, {"K", "-.-"}, {"L", ".-.."},
            {"M", "--"}, {"N", "-."}, {"O", "---"}, {"P", ".--."}, {"Q", "--.-"}, {"R", ".-."},
            {"S", "..."}, {"T", "-"}, {"U", "..-"}, {"V", "...-"}, {"W", ".--"}, {"X", "-..-"},
            {"Y", "-.--"}, {"Z", "--.."}, {"0", "-----"}, {"1", ".----"}, {"2", "..---"},
            {"3", "...--"}, {"4", "....-"}, {"5", "....."}, {"6", "-...."}, {"7", "--..."},
            {"8", "---.."}, {"9", "----."}, {"/", "-..-."}, {"?", "..--.."}, {".", ".-.-.-"},
            {",", "--..--"}, {"*", ".-.-."}, {"=", "-...-"}, {"<", "...-.-"}, {"(", "-.--."},
            {"!", "...-."}, {"&", ".-..."}, {">", "-...-.-"},
        };
        for (auto &t : table)
            m.insert(QChar(t[0][0]), QString::fromLatin1(t[1]));
        return m;
    }();
    const QString code = morse.value(c.toUpper());
    if (code.isEmpty())
        return 0;
    int u = 0;
    for (const QChar e : code)
        u += (e == '.' ? 1 : 3) + 1;   // element + intra-character gap
    return u + 2;                      // complete the 3-unit inter-character gap
}

int CwConfig::durationMs(const QString &text, int wpm)
{
    double ms = 0;
    int speed = qBound(4, wpm, 60);
    for (const QChar c : text) {
        if (c == '+') { speed = qMin(60, speed + 2); continue; }
        if (c == '-') { speed = qMax(4, speed - 2); continue; }
        const double unit = 1200.0 / speed;
        ms += (c == ' ' ? 4 : charUnits(c)) * unit;   // word gap = 7 = 3 (char) + 4
    }
    return int(ms);
}
