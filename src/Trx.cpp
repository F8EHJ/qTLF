#include "Trx.h"

#ifdef QTLF_HAVE_HAMLIB
#include "HamlibTrx.h"
#endif

QList<Trx::Backend> Trx::backends()
{
    QList<Backend> l;
    l.append({"none", tr("None"), true});
#ifdef QTLF_HAVE_HAMLIB
    l.append({"hamlib", tr("Hamlib (serial / USB port or rigctld)"), true});
#else
    l.append({"hamlib", tr("Hamlib — not available in this build (-DQTLF_WITH_HAMLIB=OFF)"), false});
#endif
    return l;
}

Trx *Trx::create(const TrxConfig &cfg, QObject *parent)
{
#ifdef QTLF_HAVE_HAMLIB
    if (cfg.type == "hamlib")
        return new HamlibTrx(cfg, parent);
#else
    Q_UNUSED(cfg)
    Q_UNUSED(parent)
#endif
    return nullptr;
}

QString contestModeForRigMode(const QString &rigMode, const QStringList &contestModes)
{
    const QString m = rigMode.toUpper();
    QStringList wanted;
    if (m == "CW" || m == "CWR")
        wanted = {"CW"};
    else if (m == "USB" || m == "LSB")
        wanted = {"SSB", "PH", "PHONE"};
    else if (m == "AM" || m == "SAM")
        wanted = {"AM", "PH", "PHONE", "SSB"};
    else if (m == "FM" || m == "WFM" || m == "FMN")
        wanted = {"FM", "PH", "PHONE"};
    else if (m == "RTTY" || m == "RTTYR")
        wanted = {"RTTY", "RY", "DIGI", "DG"};
    else if (m.startsWith("PKT") || m.startsWith("DATA"))
        wanted = {"DIGI", "DG", "DATA", "FT8", "FT4", "PSK", "RTTY"};
    for (const QString &w : wanted)
        for (const QString &c : contestModes)
            if (c.compare(w, Qt::CaseInsensitive) == 0)
                return c;
    return QString();
}
