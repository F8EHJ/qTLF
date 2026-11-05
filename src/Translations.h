#pragma once

#include <QList>
#include <QString>

class QApplication;

// User-interface language. English is the language of the sources (no file needed); every other
// language is a Qt translation file qtlf_<code>.qm (code = ISO 639-1: fr, it, de, es…):
//   * built into the program from translations/qtlf_<code>.ts (CMake, qt_add_translations);
//   * or dropped, without rebuilding, in one of these folders (they win over the built-in file):
//       <program folder>/translations/   <program folder>/../share/qtlf/translations/
//       <user data folder>/translations/ (Linux ~/.local/share/qTLF/qTLF/translations)
// The choice is stored in the INI file, [ui] language= ("" = language of the system).
// Only the user interface is translated: rules folders, code comments and the status codes of
// the QSO checks (DUPE, OUT OF PERIOD…) stay in English.
namespace Translations {

struct Language {
    QString code;     // "" = automatic, "en", "fr"…
    QString name;     // shown in Configuration › Language
};

QList<Language> available();          // automatic, English, then every qtlf_<code>.qm found
QString configured();                 // INI value ("" = automatic)
void setConfigured(const QString &code);
QString install(QApplication &app);   // loads the translators; returns the language used ("en", "fr"…)

}
