#pragma once

#include <QObject>
#include <QString>

class QUdpSocket;
struct CwConfig;

// Abstract CW keyer. One implementation per backend (cwdaemon today; WinKeyer,
// Windows/macOS keyers later). Create through Keyer::create().
class Keyer : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~Keyer() override = default;

    static Keyer *create(const CwConfig &cfg, QObject *parent);   // nullptr for "none"
    static QStringList types() { return {"none", "cwdaemon"}; }

    virtual bool start(QString *error) = 0;
    virtual void sendText(const QString &text) = 0;   // already macro-expanded
    virtual void setSpeed(int wpm) = 0;
    virtual void stop() = 0;                          // abort the message being sent
    virtual QString description() const = 0;

signals:
    void error(const QString &message);
};

// cwdaemon client: one UDP datagram per message to address:port.
// Escape codes used (doc/specifications/escape_codes.txt of cwdaemon):
//   <ESC>"2"<wpm>   set speed (4..60)        <ESC>"4"   abort message
class CwDaemonKeyer : public Keyer
{
    Q_OBJECT
public:
    CwDaemonKeyer(const QString &address, int port, const QString &initHex, QObject *parent = nullptr);

    bool start(QString *error) override;
    void sendText(const QString &text) override;
    void setSpeed(int wpm) override;
    void stop() override;
    QString description() const override;

    static QByteArray parseHex(const QString &hex, bool *ok = nullptr);

private:
    void send(const QByteArray &datagram);

    QString m_address;
    int m_port;
    QString m_initHex;
    QUdpSocket *m_socket;
};
