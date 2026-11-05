#include "AppPaths.h"
#include "ContestRules.h"
#include "ContestSelectDialog.h"
#include "Database.h"
#include "MainWindow.h"
#include "Translations.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QNetworkProxyFactory>
#include <QPointer>
#include <QSettings>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("qTLF");
    QApplication::setApplicationName("qTLF");
    QApplication::setApplicationVersion(QTLF_VERSION);
    // All the configuration goes to one INI file:
    //   Linux ~/.config/qTLF/qTLF.ini   Windows %APPDATA%\qTLF\qTLF.ini   macOS ~/.config/qTLF/qTLF.ini
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // user-interface language: Configuration › Language ([ui] language=, "" = system language)
    Translations::install(app);

    // window + taskbar icon (all sizes, Qt picks the best one)
    QIcon icon;
    for (int s : {16, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QString(":/icons/qtlf-%1.png").arg(s), QSize(s, s));
    QApplication::setWindowIcon(icon);
    QGuiApplication::setDesktopFileName("qtlf");   // Linux/Wayland: matches qtlf.desktop
    QNetworkProxyFactory::setUseSystemConfiguration(true);   // remote data files: honour the OS proxy

    Database db;
    QString err;
    if (!db.open(AppPaths::databasePath(), &err)) {
        QMessageBox::critical(nullptr, "qTLF", QObject::tr("Cannot open database %1:\n%2")
                                                   .arg(AppPaths::databasePath(), err));
        return 1;
    }

    QPointer<MainWindow> current;

    // Shows the contest selection dialog and opens the chosen contest.
    std::function<bool()> openContest = [&]() -> bool {
        const QString root = AppPaths::rulesRoot();
        ContestSelectDialog dlg(&db, root, current);
        if (dlg.exec() != QDialog::Accepted)
            return false;
        ContestInfo info;
        if (!db.contest(dlg.selectedContestId(), info)) {
            QMessageBox::critical(nullptr, "qTLF", QObject::tr("Contest not found in the database."));
            return false;
        }
        ContestRules rules;
        if (!rules.load(root + "/" + info.rulesId, &err)) {
            QMessageBox::critical(nullptr, "qTLF",
                                  QObject::tr("Cannot load the rules '%1':\n%2").arg(info.rulesId, err));
            return openContest();               // back to the contest list (fix the rules, or pick another)
        }
        auto *w = new MainWindow(&db, info, rules);
        w->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(w, &MainWindow::switchContestRequested, w, [&, w] {
            QTimer::singleShot(0, [&, w] {
                QPointer<MainWindow> old = w;
                if (openContest() && old)
                    old->close();
            });
        });
        if (current)
            current->close();
        current = w;
        w->show();
        return true;
    };

    if (!openContest())
        return 0;
    return app.exec();
}
