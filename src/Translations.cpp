#include "Translations.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTranslator>

namespace Translations {

// folders searched for qtlf_<code>.qm, external ones first (a newer file overrides the built-in one)
static QStringList qmFolders()
{
    const QString app = QCoreApplication::applicationDirPath();
    return {app + "/translations", app + "/../share/qtlf/translations",
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/translations",
            ":/i18n"};
}

QList<Language> available()
{
    QList<Language> out;
    out.append(Language{QString(), QObject::tr("Automatic (language of the system)")});
    out.append(Language{"en", "English"});
    QStringList seen = {"en"};
    for (const QString &dir : qmFolders()) {
        for (const QString &f : QDir(dir).entryList({"qtlf_*.qm"}, QDir::Files, QDir::Name)) {
            const QString code = QFileInfo(f).completeBaseName().mid(5);       // qtlf_fr -> fr
            if (code.isEmpty() || seen.contains(code))
                continue;
            seen << code;
            QString name = QLocale(code).nativeLanguageName();
            if (!name.isEmpty())
                name[0] = name[0].toUpper();
            out.append(Language{code, name.isEmpty() ? code : QString("%1 (%2)").arg(name, code)});
        }
    }
    return out;
}

QString configured()
{
    return QSettings().value("ui/language").toString();
}

void setConfigured(const QString &code)
{
    QSettings().setValue("ui/language", code);
}

static bool loadInto(QTranslator *t, const QString &base, const QString &code, const QStringList &dirs)
{
    for (const QString &d : dirs)
        if (t->load(base + code, d))      // tries base_fr_FR.qm, base_fr.qm…
            return true;
    return false;
}

QString install(QApplication &app)
{
    QString code = configured();
    if (code.isEmpty())
        code = QLocale::system().name();          // e.g. "fr_FR"
    if (code == "C" || code.startsWith("en"))
        return "en";

    auto *qtlf = new QTranslator(&app);
    if (!loadInto(qtlf, "qtlf_", code, qmFolders())) {
        delete qtlf;
        return "en";                               // no translation for this language: English
    }
    app.installTranslator(qtlf);

    // Qt's own texts (OK / Cancel buttons, file dialog…), when Qt's translations are installed
    auto *qt = new QTranslator(&app);
    const QStringList qtDirs = {QLibraryInfo::path(QLibraryInfo::TranslationsPath),
                                QCoreApplication::applicationDirPath() + "/translations"};
    if (loadInto(qt, "qtbase_", code, qtDirs))
        app.installTranslator(qt);
    else
        delete qt;
    return code.left(2);
}

}   // namespace Translations
