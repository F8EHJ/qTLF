#pragma once

#include "Types.h"

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <memory>

class QJSEngine;
class ContestRules;

// Runs the contest's scoring.js:  score(qsos, contest) -> object
class ScoringEngine
{
public:
    ScoringEngine();
    ~ScoringEngine();

    bool load(const QString &scriptPath, QString *error = nullptr);
    bool isLoaded() const { return m_loaded; }

    // Returns the object built by the script. On failure: {"error": "..."}.
    // qsos: list of QSO objects (qsoToVariant + cty fields), chronological
    QVariantMap compute(const QVariantList &qsos, const QVariantMap &contest);

    static QVariantMap contestToVariant(const ContestInfo &c, const ContestRules &r);

private:
    std::unique_ptr<QJSEngine> m_engine;
    bool m_loaded = false;
};
