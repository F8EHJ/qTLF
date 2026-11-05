#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace AppPaths {

QString rulesRoot()
{
    QStringList candidates;
    candidates << qEnvironmentVariable("QTLF_RULES");
    candidates << QSettings().value("paths/rules").toString();
    const QString app = QCoreApplication::applicationDirPath();
    candidates << app + "/rules" << app + "/../rules" << app + "/../share/qtlf/rules";
#ifdef QTLF_DEV_RULES_DIR
    candidates << QString::fromUtf8(QTLF_DEV_RULES_DIR);
#endif
    candidates << QDir::currentPath() + "/rules";
    candidates << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rules";

    for (const QString &d : candidates)
        if (!d.isEmpty() && QDir(d).exists())
            return QDir(d).absolutePath();
    return QDir(app + "/rules").absolutePath();
}

QString databasePath()
{
    QString p = QSettings().value("database/path").toString();
    if (p.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        p = dir + "/qtlf.sqlite";
    }
    return p;
}

} // namespace AppPaths
