#pragma once

#include "Types.h"

#include <QHash>
#include <QString>
#include <QVector>

class ContestRules;

namespace Adif {
// Writes an ADIF 3.1.8 file, ADI format (https://adif.org/318/ADIF_318.htm).
//   header : ADIF_VER, CREATED_TIMESTAMP, PROGRAMID, PROGRAMVERSION, <EOH>
//   record : CALL QSO_DATE TIME_ON BAND FREQ MODE [SUBMODE] RST_SENT RST_RCVD
//            STX | STX_STRING   SRX | SRX_STRING   CONTEST_ID STATION_CALLSIGN [MY_GRIDSQUARE]
//            [COMMENT] APP_QTLF_STATUS  <EOR>
// ADI strings are printable ASCII only: accents are removed (é -> e), other characters dropped.
// status: qso id -> status shown in the log (OK, DUPE, ...), written as APP_QTLF_STATUS.
bool write(const QString &path, const ContestInfo &contest, const ContestRules &rules,
           const QVector<Qso> &qsos, const QHash<int, QString> &status, const QString &myGrid,
           QString *error);

// ADIF MODE / SUBMODE for a qTLF mode (CW, SSB, USB, FT4…). submode "" = none.
void mode(const QString &qtlfMode, QString *mode, QString *submode);
// ADIF band name for a frequency in kHz ("" if out of the ADIF bands)
QString bandForKhz(double khz);
}
