#include "TrxConfig.h"

#include <QSettings>

TrxConfig TrxConfig::load()
{
    TrxConfig c;
    QSettings s;
    s.beginGroup("trx");
    c.type = s.value("type", c.type).toString();
    c.rigModel = s.value("rig_model", c.rigModel).toInt();
    c.rigSpeed = s.value("rig_speed", c.rigSpeed).toInt();
    c.rigPort = s.value("rig_port", c.rigPort).toString();
    c.pollMs = qBound(100, s.value("poll_ms", c.pollMs).toInt(), 10000);
    c.setFreq = s.value("set_freq", c.setFreq).toBool();
    c.followMode = s.value("follow_mode", c.followMode).toBool();
    c.hamlibLibrary = s.value("hamlib_library").toString();
    s.endGroup();
    return c;
}

void TrxConfig::save() const
{
    QSettings s;
    s.beginGroup("trx");
    s.setValue("type", type);
    s.setValue("rig_model", rigModel);
    s.setValue("rig_speed", rigSpeed);
    s.setValue("rig_port", rigPort);
    s.setValue("poll_ms", pollMs);
    s.setValue("set_freq", setFreq);
    s.setValue("follow_mode", followMode);
    s.setValue("hamlib_library", hamlibLibrary);
    s.endGroup();
}
