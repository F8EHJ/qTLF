#pragma once

#include <QString>

// Transceiver (TRX) control configuration, stored in the qTLF INI file, group [trx]:
//
//   [trx]
//   type=hamlib            ; none | hamlib   (other backends can be added later)
//   rig_model=3073         ; RIGMODEL  - Hamlib model number (rigctl -l), 2 = rigctld over the network
//   rig_speed=57600        ; RIGSPEED  - serial speed (baud), 0 = backend default
//   rig_port=/dev/ttyUSB0  ; RIGPORT   - serial port (or host:port for model 2)
//   poll_ms=500            ; how often the frequency is read
//   set_freq=true          ; a frequency typed in qTLF is sent to the rig (QSY)
//   follow_mode=true       ; the rig mode selects the qTLF mode (CW, SSB…) when the contest has it
//   hamlib_library=        ; Hamlib library file, empty = automatic
//                          ; (Windows: libhamlib-4.dll next to qtlf.exe, C:\Program Files\hamlib-w64-*\bin, PATH)
struct TrxConfig
{
    QString type = "none";
    int rigModel = 1;           // 1 = Hamlib dummy rig
    int rigSpeed = 57600;
#ifdef Q_OS_WIN
    QString rigPort = "COM3";
#else
    QString rigPort = "/dev/ttyUSB0";
#endif
    int pollMs = 500;
    bool setFreq = true;
    bool followMode = true;
    QString hamlibLibrary;      // "" = automatic

    static TrxConfig load();
    void save() const;
    bool enabled() const { return type != "none"; }
};
