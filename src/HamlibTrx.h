#pragma once

#include "HamlibApi.h"
#include "Trx.h"

#include <QList>
#include <QThread>

// TRX control through Hamlib (libhamlib, https://hamlib.github.io/).
// The library is loaded at run time (HamlibApi): libhamlib-4.dll on Windows (next to qtlf.exe,
// in C:\Program Files\hamlib-w64-*\bin, in the PATH or set in Configuration › TRX),
// libhamlib.so.4 on Linux. No Hamlib SDK is needed to build qTLF.
//
// RIGMODEL / RIGSPEED / RIGPORT are the same as rigctl's -m / -s / -r options:
//   rigctl -m 3073 -s 57600 -r /dev/ttyUSB0 f        (what qTLF does every poll_ms)
// Model 2 ("NET rigctl") talks to a running rigctld, RIGPORT = host:port (localhost:4532):
// useful to share the rig with other programs.
//
// All Hamlib calls run in a worker thread (they can block for seconds on a dead serial line).
// If the rig cannot be opened or stops answering, qTLF retries every few seconds.
class HamlibWorker;

class HamlibTrx : public Trx
{
    Q_OBJECT
public:
    HamlibTrx(const TrxConfig &cfg, QObject *parent = nullptr);
    ~HamlibTrx() override;

    void start() override;
    void setFrequency(double khz) override;
    QString description() const override;

    struct Model {
        int id;
        QString mfg, name, status;   // status: Stable, Beta, Alpha, Untested…
        bool network;                // port is host:port, no serial speed
    };
    static QList<Model> models(const QString &library = QString());   // all rigs of the loaded Hamlib, sorted
    static QString version();        // "" if no library loaded

    // library: file set in Configuration › TRX ("" = automatic)
    static QStringList libraryCandidates(const QString &library);
    static bool loadLibrary(const QString &library, QString *error);
    static QString libraryPath();    // file actually loaded

    // Opens the rig, reads the frequency and closes it (configuration dialog "Test" button).
    // Blocking: a few seconds at most.
    static bool probe(const TrxConfig &cfg, double *khz, QString *rigMode, QString *error);

private:
    TrxConfig m_cfg;
    QThread m_thread;
    HamlibWorker *m_worker;
};

// Worker living in HamlibTrx's thread. Not used directly.
class HamlibWorker : public QObject
{
    Q_OBJECT
public:
    explicit HamlibWorker(const TrxConfig &cfg) : m_cfg(cfg) {}
    ~HamlibWorker() override;

public slots:
    void begin();
    void finish();
    void setFrequency(double khz);

signals:
    void frequencyChanged(double khz);
    void modeChanged(const QString &rigMode);
    void connectionChanged(bool connected, const QString &message);

private:
    void tryOpen();
    void poll();
    void closeRig();
    void failed(const QString &why);

    TrxConfig m_cfg;
    hl::Rig *m_rig = nullptr;
    class QTimer *m_pollTimer = nullptr;
    class QTimer *m_retryTimer = nullptr;
    double m_lastKhz = -1;
    QString m_lastMode;
    QString m_lastError;
    unsigned m_errors = 0;      // error history of the last 8 reads (bit 0 = last)
    bool m_connected = false;
};
