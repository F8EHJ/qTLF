#pragma once

#include "Types.h"

#include <QString>
#include <QVector>

class ContestRules;

namespace Cabrillo {
// Writes a Cabrillo 3.0 log. The QSO line layout comes from rules "cabrillo.qso_template":
// tokens {freq} {mode} {date} {time} {mycall} {rst_s} {exch_s} {call} {rst_r} {exch_r}
// with optional width: {call:13} = left aligned on 13 chars, {freq:>5} = right aligned.
bool write(const QString &path, const ContestInfo &contest, const ContestRules &rules,
           const QVector<Qso> &qsos, qint64 claimedScore, QString *error);
}
