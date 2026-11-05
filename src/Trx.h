#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "TrxConfig.h"

// Abstract transceiver control ("TRX control"). One implementation per backend
// (Hamlib today; others later). Create through Trx::create().
//
// The backend reads the rig on its own (polling, in a worker thread when the calls can
// block) and reports changes with the signals below; the main window never waits for the rig.
class Trx : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~Trx() override = default;

    struct Backend {
        QString id, name;
        bool available;
    };
    static QList<Backend> backends();                       // for the configuration dialog
    static Trx *create(const TrxConfig &cfg, QObject *parent); // nullptr for "none" / unavailable

    virtual void start() = 0;                     // opens the rig (asynchronous, retries on failure)
    virtual void setFrequency(double khz) = 0;    // QSY
    virtual QString description() const = 0;     // e.g. "Icom IC-7300 /dev/ttyUSB0"

signals:
    void frequencyChanged(double khz);
    void modeChanged(const QString &rigMode);     // backend mode name: "CW", "USB", "PKTUSB"…
    void connectionChanged(bool connected, const QString &message);
};

// Maps a rig mode to one of the contest modes ("" if none fits).
QString contestModeForRigMode(const QString &rigMode, const QStringList &contestModes);
