#pragma once

#include "CallDatabase.h"
#include "ContestRules.h"
#include "CtyDat.h"
#include "CwConfig.h"
#include "TrxConfig.h"
#include "EntryBar.h"
#include "QsoChecks.h"
#include "ScoringEngine.h"
#include "Types.h"

#include <QMainWindow>
#include <QVariantMap>

class BandSummaryWidget;
class ContestBridge;
class Database;
class CwPanel;
class Keyer;
class Trx;
class LogFilterProxy;
class QDockWidget;
class QLabel;
class QLineEdit;
class QQuickWidget;
class QTableView;
class QsoLogModel;
class RemoteFiles;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(Database *db, const ContestInfo &contest, const ContestRules &rules, QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    void switchContestRequested();

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;   // search field and log: ESC / TAB
    bool focusNextPrevChild(bool next) override;            // TAB anywhere else: back to Call

private:
    void buildMenus();
    void buildCustomWindows();
    void reloadCustomization();
    void updateRemoteFiles(bool userRequest);
    void reloadDataFiles();
    void loadReferenceFiles();      // call database + country file
    void updateEntryInfo(const QString &call);
    QVariantMap ctyVariant(const QString &call) const;
    bool checkEntry();          // validates the entry line before logging (shows the reason)
    void logQso(bool force = false);   // force: Ctrl+ENTER, no exchange check
    void updateExchSent();
    void runCommand(const QString &cmd);
    void onCallEdited(const QString &call);
    bool isDupe(const QString &call) const;   // per the rules' dupe setting, current band/mode
    void recompute();
    void tagRow(int sourceRow);
    void deleteRow(int sourceRow);
    void exportCabrillo();
    void exportAdif();
    void openSettings(int tab = 0);
    void showHelp();
    void updateContestClock();
    void setFrequencyFromUser(double khz);
    int currentSourceRow() const;
    QString windowStateKey() const;

    // ---- CW keyer (MainWindowCw.cpp)
    void setupCw();
    void applyCwConfig();       // (re)creates the keyer from the INI configuration
    void applyTrxConfig();      // (re)creates the TRX control from the INI configuration
    void onTrxConnection(bool connected, const QString &message);
    QString exchangeFor(const QString &call) const;   // TAB pre-fill: this log, then call_exchange or cty.dat
    void updateCompletions();   // drop-down lists of Call and Exch Rcvd
    void clearEntryLine();      // Ctrl+L
    void editContest();         // Configuration › Contest options: Cabrillo header options only
    void updateWindowTitle();
    void fitToScreens();        // saved geometry from another screen setup: back inside the screens
    void updateBandLock();      // band_change rule: status-bar countdown + warning
    void sendCwMessage(int index);
    void sendCwText(const QString &name, const QString &message);
    void onFunctionKey(int index);
    void onEnter(EntryBar::Field field);
    void onEscape(EntryBar::Field field);
    void onSpeedKey(int delta, EntryBar::Field field);
    void changeSpeed(int delta);
    void toggleCqMode();
    void startAutoCq();
    void stopAutoCq();
    void autoCqTick();
    void editCwMessage(int index);
    void editAutoCqDelay();

    Database *m_db;
    ContestInfo m_contest;
    ContestRules m_rules;
    ScoringEngine m_scoring;
    QVariantMap m_score;

    QsoLogModel *m_model;
    LogFilterProxy *m_proxy;
    EntryBar *m_entry;
    BandSummaryWidget *m_bandSummary;
    QTableView *m_view;
    QLineEdit *m_search;
    QLabel *m_scoreLabel, *m_clockLabel;
    ContestBridge *m_bridge;
    QList<QPair<QDockWidget *, QQuickWidget *>> m_qmlDocks;
    QMenu *m_windowsMenu = nullptr;
    RemoteFiles *m_remote = nullptr;
    CtyDat m_cty;
    CallDatabase m_callDb;
    QString m_refFilesError;
    bool m_remoteUserRequest = false;

    CwConfig m_cw;
    Keyer *m_keyer = nullptr;
    CwPanel *m_cwPanel = nullptr;
    QLabel *m_keyerLabel = nullptr;

    TrxConfig m_trxCfg;
    Trx *m_trx = nullptr;
    QLabel *m_trxLabel = nullptr;
    QLabel *m_ctyLabel = nullptr;     // "cty.dat missing" while the country file is not loaded

    QsoChecks::BandRun m_bandRun;
    QLabel *m_bandLockLabel = nullptr;
    QString m_bandWarned;       // band already warned about (no repeat every second)
    QTimer *m_autoCqTimer = nullptr;
    bool m_cqMode = true;
    bool m_autoCq = false;
    int m_speed = 20;
};
