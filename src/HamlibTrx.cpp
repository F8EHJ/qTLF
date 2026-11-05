#include "HamlibTrx.h"

#include "HamlibApi.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QTimer>

#include <algorithm>
#include <bitset>
#include <cmath>

// ---- the Hamlib library, loaded at run time (libhamlib-4.dll / libhamlib.so.4)
static hl::Api g_api;
static QMutex g_apiMutex;
static QString g_loadError;

QStringList HamlibTrx::libraryCandidates(const QString &configured)
{
    QStringList c;
    if (!configured.trimmed().isEmpty())               // set by the user: only this file
        return {QDir::toNativeSeparators(configured.trimmed())};
#ifdef Q_OS_WIN
    // next to qtlf.exe, then the usual install folders of the Hamlib zip, newest first, then PATH
    c << QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/libhamlib-4.dll");
    for (const QString &root : {QString::fromLocal8Bit(qgetenv("ProgramFiles")), QString("C:/Program Files"),
                                QString("C:/"), QDir::homePath()}) {
        if (root.isEmpty())
            continue;
        QDir d(root);
        const QStringList dirs = d.entryList({"hamlib*"}, QDir::Dirs, QDir::Name | QDir::Reversed);
        for (const QString &h : dirs) {
            const QString dll = d.filePath(h + "/bin/libhamlib-4.dll");
            if (QFileInfo::exists(dll))
                c << QDir::toNativeSeparators(dll);
        }
    }
#endif
    for (const std::string &n : hl::Api::defaultNames())
        c << QString::fromStdString(n);
    c.removeDuplicates();
    return c;
}

// Loads (once) the Hamlib library; reloads it if another file is configured.
static bool loadHamlib(const QString &configured, QString *error)
{
    QMutexLocker lock(&g_apiMutex);
    const QStringList cand = HamlibTrx::libraryCandidates(configured);
    if (g_api.loaded()) {
        // keep the loaded library unless the user configured another file
        if (configured.trimmed().isEmpty()
            || QDir::toNativeSeparators(configured.trimmed()) == QString::fromStdString(g_api.path()))
            return true;
    }
    std::vector<std::string> v;
    for (const QString &x : cand)
        v.push_back(x.toStdString());
    std::string err;
    if (!g_api.load(v, &err)) {
        g_loadError = QString::fromStdString(err);
#ifdef Q_OS_WIN
        *error = QObject::tr("libhamlib-4.dll not found or not usable (%1). Copy the files of the bin folder "
                             "of hamlib-w64 (libhamlib-4.dll, libusb-1.0.dll, libwinpthread-1.dll, …) next to "
                             "qtlf.exe, or set the library in Configuration › TRX.")
                     .arg(g_loadError);
#else
        *error = QObject::tr("Hamlib library not found (%1): sudo apt install libhamlib4 "
                             "(libhamlib4t64 on Ubuntu 24.04), or set the library in Configuration › TRX.")
                     .arg(g_loadError);
#endif
        return false;
    }
    g_loadError.clear();
    // Hamlib writes its traces to stderr; silent unless QTLF_HAMLIB_DEBUG is set (1..5)
    const int level = qEnvironmentVariableIntValue("QTLF_HAMLIB_DEBUG");
    g_api.rig_set_debug(level > 0 ? qBound(1, level, 5) : 0);
    g_api.rig_load_all_backends();
    return true;
}

bool HamlibTrx::loadLibrary(const QString &configured, QString *error)
{
    QString e;
    const bool ok = loadHamlib(configured, &e);
    if (error) *error = e;
    return ok;
}

QString HamlibTrx::libraryPath()
{
    QMutexLocker lock(&g_apiMutex);
    return g_api.loaded() ? QString::fromStdString(g_api.path()) : QString();
}

static int portType(hl::rig_model_t model, const QString &port)
{
    if (g_api.rig_get_caps_int)
        return int(g_api.rig_get_caps_int(model, hl::CAPS_PORT_TYPE));
    return port.contains(':') && !port.contains('\\') ? hl::PORT_NETWORK : hl::PORT_SERIAL;   // old Hamlib
}

// rigerror() of some Hamlib versions appends debug traces: short messages of our own
static QString hamlibError(int ret)
{
    static const char *msg[] = {
        "OK", "invalid parameter", "invalid configuration", "out of memory", "function not implemented",
        "communication timed out (rig off? wrong RIGSPEED?)", "I/O error", "internal Hamlib error",
        "protocol error (wrong RIGMODEL or RIGSPEED?)", "command rejected by the rig", "string truncated",
        "function not available", "VFO not targetable", "bus error", "bus busy (collision)",
        "invalid argument", "invalid VFO", "argument out of domain"};
    const int e = ret < 0 ? -ret : ret;
    if (e < int(sizeof msg / sizeof *msg))
        return QString::fromLatin1(msg[e]);
    return QObject::tr("Hamlib error %1").arg(ret);
}

static double toKhz(hl::freq_t hz)
{
    return std::round(double(hz) / 100.0) / 10.0;   // 0.1 kHz resolution
}

// Opens a rig with RIGMODEL / RIGSPEED / RIGPORT. nullptr + error on failure.
static hl::Rig *openRig(const TrxConfig &cfg, QString *error)
{
    if (!loadHamlib(cfg.hamlibLibrary, error))
        return nullptr;
    const hl::rig_model_t model = hl::rig_model_t(cfg.rigModel);
    hl::Rig *rig = g_api.rig_init(model);
    if (!rig) {
        *error = QObject::tr("unknown Hamlib rig model %1 (see rigctl -l)").arg(cfg.rigModel);
        return nullptr;
    }
    const QString port = cfg.rigPort.trimmed();
    const bool serial = portType(model, port) == hl::PORT_SERIAL;
    if (!port.isEmpty())
        g_api.rig_set_conf(rig, g_api.rig_token_lookup(rig, "rig_pathname"), port.toLocal8Bit().constData());
    if (serial && cfg.rigSpeed > 0)
        g_api.rig_set_conf(rig, g_api.rig_token_lookup(rig, "serial_speed"), QByteArray::number(cfg.rigSpeed).constData());

    const int ret = g_api.rig_open(rig);
    if (ret != hl::OK) {
        QString why = hamlibError(ret);
        if (serial) {
#ifdef Q_OS_WIN
            why += QObject::tr(" — check that %1 is the right COM port (Device Manager › Ports) and that no other "
                               "program uses it").arg(port);
#else
            const QFileInfo fi(port);
            if (!fi.exists())
                why = QObject::tr("%1 does not exist (rig off or cable unplugged?)").arg(port);
            else if (!fi.isReadable() || !fi.isWritable())
                why = QObject::tr("no permission on %1 — add yourself to the dialout group: "
                                  "sudo usermod -aG dialout $USER, then log out and in again")
                          .arg(port);
#endif
        }
        *error = why;
        g_api.rig_cleanup(rig);
        return nullptr;
    }
    return rig;
}

// ===================================================================== HamlibTrx

HamlibTrx::HamlibTrx(const TrxConfig &cfg, QObject *parent)
    : Trx(parent), m_cfg(cfg), m_worker(new HamlibWorker(cfg))
{
    m_thread.setObjectName("hamlib");
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &HamlibWorker::frequencyChanged, this, &Trx::frequencyChanged);
    connect(m_worker, &HamlibWorker::modeChanged, this, &Trx::modeChanged);
    connect(m_worker, &HamlibWorker::connectionChanged, this, &Trx::connectionChanged);
    m_thread.start();
}

HamlibTrx::~HamlibTrx()
{
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::finish, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void HamlibTrx::start()
{
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::begin, Qt::QueuedConnection);
}

void HamlibTrx::setFrequency(double khz)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, khz] { w->setFrequency(khz); }, Qt::QueuedConnection);
}

QString HamlibTrx::description() const
{
    QString rig = QString::number(m_cfg.rigModel);
    for (const Model &m : models(m_cfg.hamlibLibrary))
        if (m.id == m_cfg.rigModel) {
            rig = m.mfg + " " + m.name;
            break;
        }
    return QString("%1 %2").arg(rig, m_cfg.rigPort);
}

QString HamlibTrx::version()
{
    // "Hamlib 4.5.5 Apr 05 11:43:08Z 2023 SHA=6eecd3" -> "4.5.5"
    QMutexLocker lock(&g_apiMutex);
    if (!g_api.loaded() || !g_api.version)
        return QString();
    return QString::fromLatin1(g_api.version).section(' ', 1, 1);
}

static int collectModel(const void *caps, void *data)
{
    auto *list = static_cast<QList<HamlibTrx::Model> *>(data);
    // first member of struct rig_caps = rig_model (the same in every Hamlib 4.x); the rest is read
    // with rig_get_caps_cptr / rig_get_caps_int, never from the structure
    const hl::rig_model_t id = *static_cast<const hl::rig_model_t *>(caps);
    HamlibTrx::Model m{int(id), QString(), QString("Model %1").arg(id), QString(), false};
    if (g_api.rig_get_caps_cptr) {
        m.mfg = QString::fromLatin1(g_api.rig_get_caps_cptr(id, hl::CAPS_MFG_NAME));
        m.name = QString::fromLatin1(g_api.rig_get_caps_cptr(id, hl::CAPS_MODEL_NAME));
        m.status = QString::fromLatin1(g_api.rig_get_caps_cptr(id, hl::CAPS_STATUS));
    }
    if (g_api.rig_get_caps_int) {
        const int pt = int(g_api.rig_get_caps_int(id, hl::CAPS_PORT_TYPE));
        m.network = pt == hl::PORT_NETWORK || pt == hl::PORT_UDP_NETWORK;
    }
    list->append(m);
    return 1;   // continue
}

QList<HamlibTrx::Model> HamlibTrx::models(const QString &configured)
{
    static QList<Model> cache;
    static QString cachePath;
    QString err;
    if (!loadHamlib(configured, &err))
        return {};
    QMutexLocker lock(&g_apiMutex);
    const QString path = QString::fromStdString(g_api.path());
    if (cache.isEmpty() || cachePath != path) {
        cache.clear();
        cachePath = path;
        g_api.rig_list_foreach(collectModel, &cache);
        std::sort(cache.begin(), cache.end(), [](const Model &a, const Model &b) {
            const int c = a.mfg.compare(b.mfg, Qt::CaseInsensitive);
            return c != 0 ? c < 0 : a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
    }
    return cache;
}

bool HamlibTrx::probe(const TrxConfig &cfg, double *khz, QString *rigMode, QString *error)
{
    hl::Rig *rig = openRig(cfg, error);
    if (!rig)
        return false;
    hl::freq_t f = 0;
    const int ret = g_api.rig_get_freq(rig, hl::VFO_CURR, &f);
    const bool ok = ret == hl::OK;
    if (ok) {
        *khz = toKhz(f);
        hl::rmode_t m = 0;
        hl::pbwidth_t w = 0;
        if (g_api.rig_get_mode(rig, hl::VFO_CURR, &m, &w) == hl::OK)
            *rigMode = QString::fromLatin1(g_api.rig_strrmode(m));
    } else {
        *error = QObject::tr("rig opened but no frequency: %1 (check RIGSPEED and the rig's CAT settings)")
                     .arg(hamlibError(ret));
    }
    g_api.rig_close(rig);
    g_api.rig_cleanup(rig);
    return ok;
}

// ===================================================================== worker (hamlib thread)

HamlibWorker::~HamlibWorker()
{
    closeRig();
}

void HamlibWorker::begin()
{
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(m_cfg.pollMs);
        connect(m_pollTimer, &QTimer::timeout, this, &HamlibWorker::poll);
        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        m_retryTimer->setInterval(5000);
        connect(m_retryTimer, &QTimer::timeout, this, &HamlibWorker::tryOpen);
    }
    tryOpen();
}

void HamlibWorker::finish()
{
    if (m_pollTimer) m_pollTimer->stop();
    if (m_retryTimer) m_retryTimer->stop();
    closeRig();
}

void HamlibWorker::tryOpen()
{
    closeRig();
    QString err;
    m_rig = openRig(m_cfg, &err);
    if (!m_rig) {
        if (err != m_lastError || m_connected)
            emit connectionChanged(false, err);
        m_lastError = err;
        m_connected = false;
        m_retryTimer->start();
        return;
    }
    m_errors = 0;
    m_lastError.clear();
    m_lastKhz = -1;
    m_lastMode.clear();
    m_connected = true;
    emit connectionChanged(true, QString());
    poll();
    m_pollTimer->start();
}

void HamlibWorker::closeRig()
{
    if (!m_rig)
        return;
    g_api.rig_close(m_rig);
    g_api.rig_cleanup(m_rig);
    m_rig = nullptr;
}

void HamlibWorker::failed(const QString &why)
{
    m_pollTimer->stop();
    closeRig();
    m_connected = false;
    m_lastError = why;
    emit connectionChanged(false, why);
    m_retryTimer->start();
}

void HamlibWorker::poll()
{
    if (!m_rig)
        return;
    hl::freq_t f = 0;
    const int ret = g_api.rig_get_freq(m_rig, hl::VFO_CURR, &f);
    // The mode is always read: some Hamlib versions return the cached frequency when the link
    // is lost, the mode read then reports the I/O error.
    hl::rmode_t m = 0;
    hl::pbwidth_t w = 0;
    const int mret = g_api.rig_get_mode(m_rig, hl::VFO_CURR, &m, &w);
    const bool linkLost = mret == -hl::E_IO || mret == -hl::E_TIMEOUT || mret == -hl::E_PROTO;
    // last 8 reads, 1 = error: 3 errors -> link lost (a lost link can alternate errors and
    // cached answers, so a single good read does not mean the rig is back)
    m_errors = ((m_errors << 1) | ((ret != hl::OK || linkLost) ? 1 : 0)) & 0xFF;
    if (ret != hl::OK || linkLost) {
        if (std::bitset<8>(m_errors).count() >= 3)
            failed(tr("rig not answering: %1").arg(hamlibError(ret != hl::OK ? ret : mret)));
        return;
    }
    const double khz = toKhz(f);
    if (khz != m_lastKhz) {
        m_lastKhz = khz;
        emit frequencyChanged(khz);
    }
    if (m_cfg.followMode && mret == hl::OK) {
        const QString mode = QString::fromLatin1(g_api.rig_strrmode(m));
        if (mode != m_lastMode) {
            m_lastMode = mode;
            emit modeChanged(mode);
        }
    }
}

void HamlibWorker::setFrequency(double khz)
{
    if (!m_rig || khz <= 0)
        return;
    const int ret = g_api.rig_set_freq(m_rig, hl::VFO_CURR, hl::freq_t(std::llround(khz * 1000.0)));
    if (ret != hl::OK)
        emit connectionChanged(true, tr("QSY to %1 kHz refused by the rig: %2").arg(khz).arg(hamlibError(ret)));
    poll();
}
