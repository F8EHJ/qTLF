#include "Keyer.h"

#include "CwConfig.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QRegularExpression>
#include <QUdpSocket>

Keyer *Keyer::create(const CwConfig &cfg, QObject *parent)
{
    if (cfg.keyerType == "cwdaemon")
        return new CwDaemonKeyer(cfg.address, cfg.port, cfg.initHex, parent);
    return nullptr;
}

CwDaemonKeyer::CwDaemonKeyer(const QString &address, int port, const QString &initHex, QObject *parent)
    : Keyer(parent), m_address(address), m_port(port), m_initHex(initHex), m_socket(new QUdpSocket(this))
{
}

// "1B 30", "1b30", "0x1B,0x30" -> bytes
QByteArray CwDaemonKeyer::parseHex(const QString &hex, bool *ok)
{
    QString h = hex;
    h.remove(QRegularExpression("0[xX]"));
    h.remove(QRegularExpression("[\\s,;:]"));
    const bool valid = h.size() % 2 == 0 && QRegularExpression("^[0-9A-Fa-f]*$").match(h).hasMatch();
    if (ok) *ok = valid;
    return valid ? QByteArray::fromHex(h.toLatin1()) : QByteArray();
}

bool CwDaemonKeyer::start(QString *err)
{
    QHostAddress addr;
    if (!addr.setAddress(m_address)) {
        const QHostInfo info = QHostInfo::fromName(m_address);
        // prefer IPv4 (cwdaemon listens on IPv4)
        for (const QHostAddress &a : info.addresses())
            if (a.protocol() == QAbstractSocket::IPv4Protocol) { addr = a; break; }
        if (addr.isNull() && !info.addresses().isEmpty())
            addr = info.addresses().first();
    }
    if (addr.isNull()) {
        if (err) *err = tr("Cannot resolve keyer address '%1'").arg(m_address);
        return false;
    }
    m_address = addr.toString();
    m_socket->connectToHost(addr, quint16(m_port));   // UDP: only fixes the destination
    if (!m_initHex.trimmed().isEmpty()) {
        bool ok = false;
        const QByteArray init = parseHex(m_initHex, &ok);
        if (!ok) {
            if (err) *err = tr("Invalid init hex string '%1'").arg(m_initHex);
            return false;
        }
        send(init);
    }
    return true;
}

void CwDaemonKeyer::send(const QByteArray &datagram)
{
    if (datagram.isEmpty())
        return;
    if (m_socket->write(datagram) < 0)
        emit error(tr("Keyer: %1").arg(m_socket->errorString()));
}

void CwDaemonKeyer::sendText(const QString &text)
{
    // cwdaemon plays letters, digits, punctuation and the prosign characters * = < ( ! & >
    // itself, and + / - change the speed by 2 wpm.
    send(text.toUpper().toLatin1());
}

void CwDaemonKeyer::setSpeed(int wpm)
{
    send("\x1b" "2" + QByteArray::number(qBound(4, wpm, 60)));
}

void CwDaemonKeyer::stop()
{
    send("\x1b" "4");
}

QString CwDaemonKeyer::description() const
{
    return QString("cwdaemon %1:%2").arg(m_address).arg(m_port);
}
