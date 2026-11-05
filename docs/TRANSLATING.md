# Translating qTLF

Available: English (sources), French `fr`, Italian `it`, German `de`, Spanish `es`.

English is the language of the sources. Every other language is one Qt Linguist file:
`translations/qtlf_<code>.ts` (code ISO 639-1: `fr`, `it`, `de`, `es`…). Only the **user interface**
is translated: menus, dialogs, messages, the online help (Help › Keys and commands). The contest
`rules/` folders, the QML windows, the comments of the code and the status codes of the QSO checks
(`DUPE`, `OUT OF PERIOD`, `BAD EXCH`…) stay in English. Acronyms are not translated (CW, CQ, S&P,
QSO, RST, DUPE, TRX, UTC, WPM, Cabrillo, ADIF…).

## Add a language (e.g. Italian)

1. Copy `translations/qtlf_fr.ts` to `translations/qtlf_it.ts` and change `language="fr_FR"` to
   `language="it_IT"` on the 2nd line.
2. Translate it with **Qt Linguist** (`linguist translations/qtlf_it.ts`, package `qt6-tools-dev-tools`
   on Debian/Ubuntu, included in the Qt installer on Windows): each English text, its translation.
   Keep `%1`, `%2`, `%n`, the HTML tags and the `&` of the menu shortcuts (one per menu, different letters).
3. Rebuild: CMake finds every `translations/qtlf_*.ts`, compiles it to `qtlf_it.qm` and puts it in the
   program. *Configuration › Language* then shows "Italiano (it)".

Without rebuilding: `lrelease translations/qtlf_it.ts` gives `qtlf_it.qm`; copy it to
`translations/` next to the program (or `~/.local/share/qTLF/qTLF/translations/`). qTLF lists it at once
(a file there wins over the built-in one, handy to test a translation).

## After a change of the program

New or changed English texts: `cmake --build build --target update_translations` (or
`lupdate src -ts translations/qtlf_*.ts`) adds them to every `.ts` file as *unfinished*; translate them
in Qt Linguist. An untranslated text is shown in English.

## Choice of the language

*Configuration › Language* (INI file, `[ui] language=`): *Automatic* uses the language of the system if
a translation exists, otherwise English. The change takes effect when qTLF is restarted.
