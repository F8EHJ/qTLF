#include "MainWindow.h"

#include "AboutDialog.h"
#include "BandSummaryWidget.h"
#include "CwPanel.h"
#include "Adif.h"
#include "Cabrillo.h"
#include "CabrilloFieldsForm.h"
#include "ContestBridge.h"
#include "Database.h"
#include "EntryBar.h"
#include "LogFilterProxy.h"
#include "QsoChecks.h"
#include "QsoLogModel.h"
#include "RemoteFiles.h"
#include "SettingsDialog.h"
#include "Trx.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSettings>
#include <QGuiApplication>
#include <QScreen>
#include <QShortcut>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(Database *db, const ContestInfo &contest, const ContestRules &rules, QWidget *parent)
    : QMainWindow(parent), m_db(db), m_contest(contest), m_rules(rules)
{
    updateWindowTitle();
    setDockNestingEnabled(true);
    resize(1280, 800);

    m_bridge = new ContestBridge(this);
    m_model = new QsoLogModel(m_db, &m_rules, this);
    m_model->load(m_contest.id);
    m_proxy = new LogFilterProxy(this);
    m_proxy->setSourceModel(m_model);

    // ---------- central area
    auto *central = new QWidget(this);
    auto *vl = new QVBoxLayout(central);
    vl->setContentsMargins(6, 6, 6, 6);

    auto *top = new QHBoxLayout;
    m_entry = new EntryBar(m_rules, central);
    auto *entryFrame = new QFrame(central);
    entryFrame->setFrameShape(QFrame::StyledPanel);
    auto *efl = new QVBoxLayout(entryFrame);
    efl->setContentsMargins(0, 0, 0, 0);
    efl->addWidget(m_entry);
    top->addWidget(entryFrame, 1, Qt::AlignTop);
    m_bandSummary = new BandSummaryWidget(m_rules.bandNames(), central);
    top->addWidget(m_bandSummary, 0, Qt::AlignTop | Qt::AlignRight);
    m_cwPanel = new CwPanel(central);
    efl->addWidget(m_cwPanel);
    efl->setContentsMargins(0, 0, 6, 6);
    vl->addLayout(top);

    auto *searchRow = new QHBoxLayout;
    m_search = new QLineEdit(central);
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(tr("Search (Ctrl+F):  HA6   call:DL*   band:40   freq>7020   time>=06:30   "
                                    "exch:YT7MM   tagged   is:dupe   -band:80m"));
    connect(m_search, &QLineEdit::textChanged, m_proxy, &LogFilterProxy::setQuery);
    connect(m_search, &QLineEdit::textChanged, this, [this] {
        statusBar()->showMessage(tr("%1 of %2 QSOs shown").arg(m_proxy->rowCount()).arg(m_model->rowCount()), 3000);
    });
    auto *searchLabel = new QLabel(tr("Find:"), central);
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_search, 1);
    vl->addLayout(searchRow);
    m_search->installEventFilter(this);           // ESC clears the search, TAB goes back to Call

    m_view = new QTableView(central);
    m_view->setModel(m_proxy);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(QsoLogModel::ColDate, Qt::AscendingOrder);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                            | QAbstractItemView::AnyKeyPressed);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->hide();
    m_view->verticalHeader()->setDefaultSectionSize(QFontMetrics(m_view->font()).height() + 8);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_view->horizontalHeader()->setStretchLastSection(false);
    m_view->horizontalHeader()->setSectionResizeMode(QsoLogModel::ColComment, QHeaderView::Stretch);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    const int widths[] = {48, 92, 60, 52, 72, 52, 120, 56, 56, 110, 110, 130, 40, 200, 32};
    // at least the width of the (translated) column title
    const QFontMetrics hfm(m_view->horizontalHeader()->font());
    for (int c = 0; c < QsoLogModel::ColCount; ++c)
        m_view->setColumnWidth(c, qMax(widths[c], hfm.horizontalAdvance(
                                                      m_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()) + 18));
    if (m_rules.modes.size() < 2)
        m_view->hideColumn(QsoLogModel::ColMode);
    vl->addWidget(m_view, 1);
    setCentralWidget(central);
    m_view->installEventFilter(this);             // TAB in the log goes back to Call

    connect(m_view, &QTableView::clicked, this, [this](const QModelIndex &idx) {
        if (idx.column() == QsoLogModel::ColDelete)
            deleteRow(m_proxy->mapToSource(idx).row());
    });
    connect(m_view, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex idx = m_view->indexAt(pos);
        if (!idx.isValid())
            return;
        const int row = m_proxy->mapToSource(idx).row();
        QMenu menu(this);
        menu.addAction(tr("Tag / comment…"), this, [this, row] { tagRow(row); });
        if (m_model->qsoAt(row).tagged)
            menu.addAction(tr("Remove tag"), this, [this, row] { m_model->setTag(row, false, QString()); });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("user-trash"), tr("Delete QSO…"), this, [this, row] { deleteRow(row); });
        menu.exec(m_view->viewport()->mapToGlobal(pos));
    });
    auto *delKey = new QShortcut(QKeySequence::Delete, m_view, nullptr, nullptr, Qt::WidgetShortcut);
    connect(delKey, &QShortcut::activated, this, [this] {
        if (int r = currentSourceRow(); r >= 0) deleteRow(r);
    });

    // ---------- status bar
    m_scoreLabel = new QLabel(this);
    m_clockLabel = new QLabel(this);
    m_keyerLabel = new QLabel(this);
    m_trxLabel = new QLabel(this);
    m_bandLockLabel = new QLabel(this);
    // permanent warning while the country file of the contest is not loaded (click = download)
    m_ctyLabel = new QLabel(this);
    m_ctyLabel->hide();
    m_ctyLabel->setTextFormat(Qt::RichText);
    connect(m_ctyLabel, &QLabel::linkActivated, this, [this] { updateRemoteFiles(true); });
    statusBar()->addPermanentWidget(m_ctyLabel);
    m_bandLockLabel->hide();
    statusBar()->addPermanentWidget(m_bandLockLabel);
    statusBar()->addPermanentWidget(m_trxLabel);
    statusBar()->addPermanentWidget(m_keyerLabel);
    statusBar()->addPermanentWidget(m_clockLabel);
    statusBar()->addPermanentWidget(m_scoreLabel);

    // ---------- signals
    connect(m_entry, &EntryBar::submitRequested, this, [this] { logQso(); });
    connect(m_entry, &EntryBar::commandEntered, this, &MainWindow::runCommand);
    connect(m_entry, &EntryBar::callEdited, this, &MainWindow::onCallEdited);
    connect(m_entry, &EntryBar::bandChanged, this, [this](const QString &b) {
        m_bandSummary->setCurrentBand(b);
        m_bridge->setCurrentBand(b);
        onCallEdited(m_entry->call());
        QSettings().setValue("last/band/" + m_rules.id, b);
        updateBandLock();
    });
    connect(m_entry, &EntryBar::frequencyEntered, this, [this](double khz) {   // typed in the Freq field
        if (m_trx && m_trxCfg.setFreq)
            m_trx->setFrequency(khz);
    });
    connect(m_model, &QsoLogModel::logChanged, this, &MainWindow::recompute);
    connect(m_model, &QsoLogModel::dataChanged, this, [this] {
        if (!m_model->lastError().isEmpty())
            statusBar()->showMessage(tr("Database error: %1").arg(m_model->lastError()), 5000);
    });

    // ---------- scoring + custom windows
    QString err;
    if (!m_rules.scoringScript.isEmpty() && !m_scoring.load(m_rules.filePath(m_rules.scoringScript), &err))
        QTimer::singleShot(0, this, [this, err] { QMessageBox::warning(this, tr("Scoring script"), err); });

    setupCw();
    applyTrxConfig();
    buildMenus();
    buildCustomWindows();

    QSettings s;
    m_entry->setBand(s.value("last/band/" + m_rules.id, m_rules.bandNames().value(0)).toString());
    m_bandSummary->setCurrentBand(m_entry->band());
    m_bridge->setCurrentBand(m_entry->band());
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value(windowStateKey()).toByteArray());
    QTimer::singleShot(0, this, &MainWindow::fitToScreens);   // once shown (frame size known)

    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, &MainWindow::updateContestClock);
    clock->start(1000);
    updateContestClock();

    m_remote = new RemoteFiles(this);
    connect(m_remote, &RemoteFiles::finished, this,
            [this](const QStringList &updated, const QStringList &unchanged, const QStringList &failed) {
        if (!updated.isEmpty())
            reloadDataFiles();
        // failed = "file: reason": with or without a local copy to fall back on
        QStringList kept, missing;
        for (const QString &f : failed)
            (QFileInfo::exists(m_rules.dataFilePath(f.section(':', 0, 0).trimmed())) ? kept : missing) << f;
        QStringList msg;
        if (!updated.isEmpty()) msg << tr("updated: %1").arg(updated.join(", "));
        if (!unchanged.isEmpty() && m_remoteUserRequest) msg << tr("up to date: %1").arg(unchanged.join(", "));
        if (!kept.isEmpty()) msg << tr("kept local copy — %1").arg(kept.join("; "));
        if (!missing.isEmpty()) msg << tr("NOT downloaded, no local copy — %1").arg(missing.join("; "));
        if (!msg.isEmpty())
            statusBar()->showMessage(tr("Data files %1").arg(msg.join("  |  ")), failed.isEmpty() ? 6000 : 15000);
        if (m_remoteUserRequest && !failed.isEmpty()) {
            QString text;
            if (!missing.isEmpty())
                text += tr("Could not be downloaded, and there is no local copy:\n%1\n\n").arg(missing.join("\n"));
            if (!kept.isEmpty())
                text += tr("Could not be downloaded; the local copy is used:\n%1").arg(kept.join("\n"));
            QMessageBox::warning(this, tr("Update data files"), text.trimmed());
        }
    });
    if (m_rules.remoteUpdate != "manual")
        QTimer::singleShot(500, this, [this] { updateRemoteFiles(false); });

    loadReferenceFiles();
    recompute();
    updateExchSent();
    m_view->scrollToBottom();
    QTimer::singleShot(0, m_entry, &EntryBar::focusCall);
}

MainWindow::~MainWindow() = default;

QString MainWindow::windowStateKey() const { return "windowState/" + m_rules.id; }

void MainWindow::closeEvent(QCloseEvent *e)
{
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue(windowStateKey(), saveState());
    stopAutoCq();
    QMainWindow::closeEvent(e);
}

void MainWindow::buildMenus()
{
    QMenu *contest = menuBar()->addMenu(tr("&Contest"));
    contest->addAction(tr("&Open / new contest…"), QKeySequence::Open, this, &MainWindow::switchContestRequested);
    contest->addAction(tr("Export &Cabrillo…"), QKeySequence("Ctrl+Shift+E"), this, &MainWindow::exportCabrillo);
    contest->addAction(tr("Export &ADIF…"), QKeySequence("Ctrl+Shift+A"), this, &MainWindow::exportAdif);
    contest->addAction(tr("&Update data files from network"), QKeySequence("Ctrl+U"), this,
                       [this] { updateRemoteFiles(true); });
    contest->addSeparator();
    contest->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *cfg = menuBar()->addMenu(tr("C&onfiguration"));
    cfg->addAction(tr("&Contest options…"), this, &MainWindow::editContest);
    cfg->addAction(tr("&Station…"), QKeySequence("Ctrl+,"), this, [this] { openSettings(SettingsDialog::StationTab); });
    cfg->addAction(tr("&Keyer…"), this, [this] { openSettings(SettingsDialog::KeyerTab); });
    cfg->addAction(tr("CW &messages…"), this, [this] { openSettings(SettingsDialog::MessagesTab); });
    cfg->addAction(tr("&TRX control…"), this, [this] { openSettings(SettingsDialog::TrxTab); });
    cfg->addAction(tr("&Paths…"), this, [this] { openSettings(SettingsDialog::PathsTab); });
    cfg->addAction(tr("&Language…"), this, [this] { openSettings(SettingsDialog::LanguageTab); });

    QMenu *log = menuBar()->addMenu(tr("&Log"));
    log->addAction(tr("&Find…"), QKeySequence::Find, this, [this] { m_search->setFocus(); m_search->selectAll(); });
    log->addAction(tr("C&lear entry line and search"), QKeySequence("Ctrl+L"), this, &MainWindow::clearEntryLine);
    log->addAction(tr("&Tag last QSO…"), QKeySequence("Ctrl+T"), this, [this] {
        if (m_model->rowCount() > 0) tagRow(m_model->rowCount() - 1);
    });
    log->addAction(tr("Band &up"), QKeySequence("Ctrl+Up"), this, [this] {
        const QStringList b = m_rules.bandNames();
        m_entry->setBand(b.value((b.indexOf(m_entry->band()) + 1) % b.size()));
    });
    log->addAction(tr("Band &down"), QKeySequence("Ctrl+Down"), this, [this] {
        const QStringList b = m_rules.bandNames();
        m_entry->setBand(b.value((b.indexOf(m_entry->band()) - 1 + b.size()) % b.size()));
    });

    m_windowsMenu = menuBar()->addMenu(tr("&Windows"));

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&Keys and commands"), QKeySequence("Ctrl+F1"), this, &MainWindow::showHelp);
    help->addAction(tr("&About qTLF"), this, [this] {
        AboutDialog(m_rules.dir, m_db->path(), this).exec();
        m_entry->focusCall();
    });
}

static Qt::DockWidgetArea dockArea(const QString &a)
{
    if (a == "left") return Qt::LeftDockWidgetArea;
    if (a == "top") return Qt::TopDockWidgetArea;
    if (a == "bottom") return Qt::BottomDockWidgetArea;
    return Qt::RightDockWidgetArea;
}

void MainWindow::buildCustomWindows()
{
    m_windowsMenu->clear();
    for (const WindowDef &w : m_rules.windows) {
        auto *dock = new QDockWidget(w.title, this);
        dock->setObjectName("dock_" + m_rules.id + "_" + w.id);
        auto *qw = new QQuickWidget(dock);
        qw->setResizeMode(QQuickWidget::SizeRootObjectToView);
        qw->rootContext()->setContextProperty("contest", m_bridge);
        qw->setSource(QUrl::fromLocalFile(m_rules.filePath(w.qml)));
        if (qw->status() == QQuickWidget::Error) {
            QStringList errs;
            for (const QQmlError &e : qw->errors())
                errs << e.toString();
            qWarning("%s", qPrintable(errs.join('\n')));
            statusBar()->showMessage(tr("Error in %1 (see console)").arg(w.qml), 8000);
        } else if (QQuickItem *root = qw->rootObject()) {
            // implicit size = preferred size; the dock may be made smaller (small screens)
            const QSize pref(int(root->implicitWidth()), int(root->implicitHeight()));
            qw->setMinimumSize(qMin(pref.width(), 180), qMin(pref.height(), 120));
            qw->resize(pref);
        }
        dock->setWidget(qw);
        dock->setFocusPolicy(Qt::NoFocus);
        qw->setFocusPolicy(Qt::NoFocus);
        if (w.area == "float") {
            addDockWidget(Qt::RightDockWidgetArea, dock);
            dock->setFloating(true);
        } else {
            addDockWidget(dockArea(w.area), dock);
        }
        dock->setVisible(w.visible);
        m_windowsMenu->addAction(dock->toggleViewAction());
        m_qmlDocks << qMakePair(dock, qw);
    }
    if (!m_rules.windows.isEmpty())
        m_windowsMenu->addSeparator();
    m_windowsMenu->addAction(tr("&Reload rules, scoring and windows"), QKeySequence("Ctrl+R"), this,
                             &MainWindow::reloadCustomization);
}

void MainWindow::reloadCustomization()
{
    ContestRules fresh;
    QString err;
    if (!fresh.load(m_rules.dir, &err)) {
        QMessageBox::warning(this, tr("Reload rules"), err);
        return;
    }
    const QByteArray state = saveState();
    for (auto &p : m_qmlDocks) {
        removeDockWidget(p.first);
        p.first->deleteLater();
    }
    m_qmlDocks.clear();
    // everything reloads live; only the layout of the entry line (bands list, field
    // labels) needs the contest to be reopened
    m_rules = fresh;
    loadReferenceFiles();
    if (!m_rules.scoringScript.isEmpty() && !m_scoring.load(m_rules.filePath(m_rules.scoringScript), &err))
        QMessageBox::warning(this, tr("Scoring script"), err);
    buildCustomWindows();
    restoreState(state);
    recompute();
    statusBar()->showMessage(tr("Rules reloaded from %1").arg(m_rules.dir), 4000);
}

bool MainWindow::checkEntry()
{
    if (m_entry->call().isEmpty()) {
        m_entry->setMessage(tr("Enter a callsign"), "warn");
        m_entry->focusCall();
        return false;
    }
    if (m_rules.exchRcvdRequired && m_entry->exchRcvd().isEmpty()) {
        QApplication::beep();
        m_entry->setMessage(tr("%1 is required").arg(m_rules.exchRcvdLabel), "error");
        m_entry->focusExchRcvd();
        return false;
    }
    const QString band = m_entry->band();
    if (band.isEmpty() || !m_rules.band(band)) {
        m_entry->setMessage(tr("No valid band: enter a frequency or choose a band"), "error");
        return false;
    }
    return true;
}

void MainWindow::updateRemoteFiles(bool userRequest)
{
    if (m_rules.remoteFiles.isEmpty()) {
        if (userRequest)
            QMessageBox::information(this, tr("Update data files"),
                                     tr("No data file of this contest has a \"url\" in %1/contest.json.").arg(m_rules.dir));
        return;
    }
    if (m_remote->isRunning())
        return;
    m_remoteUserRequest = userRequest;
    statusBar()->showMessage(tr("Downloading data files: %1…").arg(m_rules.remoteFiles.keys().join(", ")), 15000);
    m_remote->update(m_rules);
}

void MainWindow::reloadDataFiles()
{
    loadReferenceFiles();
    recompute();
    updateEntryInfo(m_entry->call());
}

// call_exchange (CSV) and country file (cty.dat)
void MainWindow::loadReferenceFiles()
{
    QStringList problems;
    m_callDb.clear();
    if (!m_rules.callDb.file.isEmpty()) {
        QString err;
        if (!m_callDb.load(m_rules.dataFilePath(m_rules.callDb.file), m_rules.callDb, m_rules.callDbExchangeField, &err))
            problems << err;
    }
    m_cty = CtyDat();
    QString ctyError;
    if (!m_rules.ctyFile.isEmpty()) {
        QString err;
        if (!m_cty.load(m_rules.dataFilePath(m_rules.ctyFile), &err)) {
            problems << tr("%1 (continents unknown — Contest › Update data files)").arg(err);
            ctyError = err;
        }
    }
    if (m_ctyLabel) {
        const bool missing = !m_rules.ctyFile.isEmpty() && !m_cty.isLoaded();
        m_ctyLabel->setVisible(missing);
        if (missing) {
            const bool canDownload = m_rules.remoteFiles.contains(m_rules.ctyFile);
            m_ctyLabel->setText(canDownload
                ? tr("<a href='update' style='color:#dc3545; font-weight:bold'>%1 missing</a>").arg(m_rules.ctyFile.toHtmlEscaped())
                : tr("<b style='color:#dc3545'>%1 missing</b>").arg(m_rules.ctyFile.toHtmlEscaped()));
            m_ctyLabel->setToolTip(tr("%1\n\nWithout it: no country, continent, DXCC or zones (scores that use them "
                                      "may be wrong, no pre-fill from it).\n%2")
                                       .arg(ctyError,
                                            canDownload ? tr("Click (or Contest › Update data files, Ctrl+U) to download it.")
                                                        : tr("Copy %1 into %2.").arg(m_rules.ctyFile, m_rules.dir)));
        }
    }
    m_refFilesError = problems.join("; ");
    if (!problems.isEmpty())
        statusBar()->showMessage(m_refFilesError, 10000);

    updateCompletions();
    // TAB from Call pre-fills the exchange: 1. this log (station already worked), 2. call_exchange
    // or a cty.dat field (exchange.received.prefill) — station call: /P /M /MM ignored
    m_entry->setExchangeLookup([this](const QString &call) { return exchangeFor(call); });
}

QVariantMap MainWindow::ctyVariant(const QString &call) const
{
    return m_cty.isLoaded() ? m_cty.lookup(call).toVariant() : QVariantMap{{"valid", false}};
}

// what the QML windows know about the call being typed (contest.entryCall / contest.callInfo)
void MainWindow::updateEntryInfo(const QString &call)
{
    if (call.isEmpty()) {
        m_bridge->setEntry(QString(), QVariantMap());
        return;
    }
    QVariantMap info = m_callDb.record(call);
    info["call"] = call;
    info["known"] = m_callDb.contains(call);
    info["cty"] = ctyVariant(call);
    QStringList worked;
    for (const Qso &q : m_model->qsos())
        if (QsoChecks::stationCall(q.call) == QsoChecks::stationCall(call) && !worked.contains(q.band))
            worked << q.band;
    info["workedBands"] = worked;
    info["dupe"] = isDupe(call);
    m_bridge->setEntry(call, info);
}

// force = Ctrl+ENTER: logged even if the exchange is missing or invalid (only an empty Call is refused)
void MainWindow::logQso(bool force)
{
    if (force) {
        if (m_entry->call().isEmpty()) {
            m_entry->setMessage(tr("Enter a callsign"), "warn");
            m_entry->focusCall();
            return;
        }
    } else if (!checkEntry()) {
        return;
    }
    const QString call = m_entry->call();
    const QString band = m_entry->band();

    Qso q;
    QDateTime now = QDateTime::currentDateTimeUtc();
    now.setTime(QTime(now.time().hour(), now.time().minute(), now.time().second()));
    q.utc = now;                                   // time of ENTER
    q.band = band;
    q.freqKhz = m_entry->frequency();
    q.mode = m_entry->mode();
    q.call = call;
    q.rstSent = m_entry->rstSent();
    q.rstRcvd = m_entry->rstRcvd();
    q.exchSent = m_entry->exchSent();
    q.exchRcvd = m_entry->exchRcvd();

    if (!m_model->addQso(q)) {
        QMessageBox::critical(this, tr("Database error"), m_model->lastError());
        return;
    }
    m_entry->clearForNext();
    updateExchSent();
    const QString status = m_model->statusOf(m_model->lastAddedId());   // not the last row: the log is in time order
    m_entry->setMessage(tr("Logged %1 on %2 at %3  %4")
                            .arg(q.call, q.band, q.utc.time().toString("HH:mm"),
                                 status == "OK" || status.isEmpty() ? QString() : "(" + status + ")"),
                        status == "OK" || status.isEmpty() ? "ok" : "warn");
    m_view->scrollToBottom();
}

bool MainWindow::isDupe(const QString &call) const
{
    if (call.isEmpty() || m_rules.dupeRule == "none")
        return false;
    const QString band = m_entry->band();
    const QString mode = m_entry->mode();
    const QString station = QsoChecks::stationCall(call);
    for (const Qso &q : m_model->qsos()) {
        if (QsoChecks::stationCall(q.call) != station)
            continue;
        if (m_rules.dupeRule == "contest" || (m_rules.dupeRule == "band" && q.band == band)
            || (m_rules.dupeRule == "band_mode" && q.band == band && q.mode == mode))
            return true;
    }
    return false;
}

void MainWindow::onCallEdited(const QString &call)
{
    updateEntryInfo(call);
    if (call.size() < 2) {
        if (call.isEmpty())
            m_entry->setMessage(QString());
        return;
    }
    const QString band = m_entry->band();
    QStringList bandsWorked;
    bool dupe = false;
    auto sameSlot = [&](const Qso &q) {
        return m_rules.dupeRule == "contest" || (m_rules.dupeRule == "band" && q.band == band)
               || (m_rules.dupeRule == "band_mode" && q.band == band && q.mode == m_entry->mode());
    };
    const QString station = QsoChecks::stationCall(call);
    for (const Qso &q : m_model->qsos()) {
        if (QsoChecks::stationCall(q.call) == station) {
            if (!bandsWorked.contains(q.band))
                bandsWorked << q.band;
            if (sameSlot(q))
                dupe = true;
        }
    }
    if (dupe) {
        m_entry->setMessage(tr("DUPE — %1 already worked on %2").arg(call, band), "error");
        return;
    }
    // partial calls are in the drop-down list of the Call field: only "worked on" here
    m_entry->setMessage(bandsWorked.isEmpty() ? QString() : tr("Worked on %1").arg(bandsWorked.join(", ")), "info");
}

void MainWindow::recompute()
{
    updateCompletions();
    QVariantMap info = ScoringEngine::contestToVariant(m_contest, m_rules);
    const QVariantMap myCty = ctyVariant(m_contest.myCall);
    info["my"] = myCty;                       // my station: {valid, cont, country, prefix, cqz, ituz}
    info["ctyLoaded"] = m_cty.isLoaded();

    // QSOs for scoring.js:
    //   error: result of the generic checks of qTLF ("" = valid, else OUT OF PERIOD, DUPE...)
    //   cont, country, dxcc (primary prefix), cqz, ituz: country file of the station worked
    //   db: call database record ({} if unknown)
    const QHash<int, QString> errors = QsoChecks::run(m_rules, m_contest, m_model->qsos());
    m_bandRun = QsoChecks::bandRun(m_rules, m_contest, m_model->qsos());
    m_bandWarned.clear();
    updateBandLock();
    QVariantList qsoVars;
    qsoVars.reserve(m_model->qsos().size());
    for (const Qso &q : m_model->qsos()) {
        QVariantMap v = qsoToVariant(q);
        v["error"] = errors.value(q.id);
        if (m_cty.isLoaded()) {
            const CtyDat::Info c = m_cty.lookup(q.call);
            v["cont"] = c.continent;
            v["country"] = c.country;
            v["dxcc"] = c.prefix;
            v["cqz"] = c.cqZone;
            v["ituz"] = c.ituZone;
        }
        v["db"] = m_callDb.record(q.call);
        qsoVars << v;
    }
    m_score = m_scoring.isLoaded() ? m_scoring.compute(qsoVars, info) : QVariantMap();

    // status of each QSO: generic error, else the status given by the script, else OK
    const QVariantMap st = m_score.value("status").toMap();
    QHash<int, QString> status;
    for (const Qso &q : m_model->qsos()) {
        QString s = errors.value(q.id);
        if (s.isEmpty())
            s = st.value(QString::number(q.id), "OK").toString();
        status.insert(q.id, s);
    }
    m_model->setStatuses(status);

    QMap<QString, int> counts;
    QVariantList qsoList;
    for (int i = 0; i < m_model->qsos().size(); ++i) {
        const Qso &q = m_model->qsos().at(i);
        counts[q.band]++;
        QVariantMap v = qsoVars.at(i).toMap();
        v["status"] = status.value(q.id);
        qsoList << v;
    }
    m_bandSummary->setCounts(counts);
    QVariantList bandCounts;
    for (const QString &b : m_rules.bandNames())
        bandCounts << QVariantMap{{"band", b}, {"qsos", counts.value(b)}};
    m_bridge->update(m_score, info, qsoList, bandCounts, myCty, m_cty.isLoaded());

    if (m_score.contains("error")) {
        m_scoreLabel->setText(tr("Scoring error"));
        m_scoreLabel->setToolTip(m_score.value("error").toString());
        m_scoreLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
    } else {
        QString t = tr("QSOs: %1").arg(m_model->rowCount());
        if (m_score.contains("summary"))
            t += "   " + m_score.value("summary").toString();
        else if (m_score.contains("points") && m_score.contains("multipliers"))
            t += "   " + tr("%1 pts x %2 mults").arg(m_score.value("points").toLongLong())
                             .arg(m_score.value("multipliers").toLongLong());
        else if (m_score.contains("points"))
            t += "   " + tr("%1 pts").arg(m_score.value("points").toLongLong());
        if (m_score.contains("total"))
            t += "   " + tr("Score: %1").arg(qRound64(m_score.value("total").toDouble()));
        m_scoreLabel->setText(t);
        m_scoreLabel->setToolTip(QString());
        m_scoreLabel->setStyleSheet("font-weight: bold;");
    }
}

int MainWindow::currentSourceRow() const
{
    const QModelIndex idx = m_view->currentIndex();
    return idx.isValid() ? m_proxy->mapToSource(idx).row() : -1;
}

void MainWindow::tagRow(int row)
{
    if (row < 0 || row >= m_model->rowCount())
        return;
    const Qso q = m_model->qsoAt(row);
    bool ok = false;
    const QString c = QInputDialog::getText(this, tr("Tag QSO"),
                                            tr("Comment for %1 (%2 %3):").arg(q.call, q.band, q.utc.time().toString("HH:mm")),
                                            QLineEdit::Normal, q.comment, &ok);
    if (ok)
        m_model->setTag(row, true, c);
    m_entry->focusCall();
}

void MainWindow::deleteRow(int row)
{
    if (row < 0 || row >= m_model->rowCount())
        return;
    const Qso q = m_model->qsoAt(row);
    if (QMessageBox::question(this, tr("Delete QSO"),
                              tr("Delete QSO with %1 on %2 at %3 UTC?")
                                  .arg(q.call, q.band, q.utc.toString("yyyy-MM-dd HH:mm")),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!m_model->removeQsoAt(row))
        QMessageBox::critical(this, tr("Database error"), m_model->lastError());
    updateExchSent();
    m_entry->focusCall();
}

void MainWindow::updateExchSent()
{
    m_entry->setExchSent(m_rules.exchSentType == "serial" ? m_rules.formatSerial(m_model->nextSerial())
                                                          : m_contest.exchSent);
}

void MainWindow::setFrequencyFromUser(double khz)
{
    m_entry->setFrequency(khz);
    if (m_trx && m_trxCfg.setFreq)
        m_trx->setFrequency(khz);
    const QString b = m_rules.bandForFrequency(khz);
    m_entry->setMessage(b.isEmpty() ? tr("%1 kHz is outside the contest bands").arg(khz)
                                    : tr("QSY %1 kHz (%2)").arg(khz).arg(b),
                        b.isEmpty() ? "warn" : "info");
}

void MainWindow::runCommand(const QString &text)
{
    QString t = text.trimmed();
    if (!t.startsWith(':')) {               // a number typed in the call field = frequency
        double f = QString(t).replace(',', '.').toDouble();
        if (f < 100) f *= 1000.0;
        setFrequencyFromUser(f);
        return;
    }
    const QString cmd = t.mid(1).section(' ', 0, 0).toLower();
    const QString arg = t.section(' ', 1).trimmed();

    if (cmd == "band" || cmd == "b") {
        QString b = arg.toLower();
        if (!b.isEmpty() && !b.endsWith('m') && !b.endsWith("cm")) b += "m";
        if (m_rules.band(b)) { m_entry->setBand(b); m_entry->setMessage(tr("Band %1").arg(b)); }
        else m_entry->setMessage(tr("Unknown band '%1'. Bands: %2").arg(arg, m_rules.bandNames().join(" ")), "error");
    } else if (cmd == "freq" || cmd == "f" || cmd == "qsy") {
        double f = QString(arg).replace(',', '.').toDouble();
        if (f < 100) f *= 1000.0;
        setFrequencyFromUser(f);
    } else if (cmd == "mode" || cmd == "m") {
        if (m_rules.modes.contains(arg.toUpper())) m_entry->setMode(arg.toUpper());
        else m_entry->setMessage(tr("Modes: %1").arg(m_rules.modes.join(" ")), "error");
    } else if (cmd == "cab" || cmd == "write" || cmd == "wri" || cmd == "cabrillo") {
        exportCabrillo();
    } else if (cmd == "adif" || cmd == "adi") {
        exportAdif();
    } else if (cmd == "find" || cmd == "search" || cmd == "s") {
        m_search->setText(arg);
        if (arg.isEmpty()) m_search->setFocus();
    } else if (cmd == "tag" || cmd == "note") {
        if (m_model->rowCount() == 0) return;
        if (arg.isEmpty()) tagRow(m_model->rowCount() - 1);
        else { m_model->setTag(m_model->rowCount() - 1, true, arg); m_entry->setMessage(tr("Last QSO tagged")); }
    } else if (cmd == "del" || cmd == "delete") {
        deleteRow(m_model->rowCount() - 1);
    } else if (cmd == "score" || cmd == "sco") {
        for (auto &p : m_qmlDocks) { p.first->show(); p.first->raise(); }
        m_entry->setMessage(m_scoreLabel->text());
    } else if (cmd == "cfg" || cmd == "set" || cmd == "settings") {
        openSettings();
    } else if (cmd == "update" || cmd == "upd") {
        updateRemoteFiles(true);
    } else if (cmd == "reload") {
        reloadCustomization();
    } else if (cmd == "contest" || cmd == "open") {
        emit switchContestRequested();
    } else if (cmd == "help" || cmd == "h" || cmd == "?") {
        showHelp();
    } else if (cmd == "exit" || cmd == "quit" || cmd == "q") {
        close();
    } else {
        m_entry->setMessage(tr("Unknown command '%1' — type :help").arg(t), "error");
    }
    m_entry->focusCall();
}

void MainWindow::exportCabrillo()
{
    const QString def = QString("%1_%2_%3.log")
                            .arg(QString(m_contest.myCall).replace(QChar(0x2F), QChar(0x2D)), m_rules.shortName, m_contest.date.toString("yyyy"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Cabrillo"),
                                                      QSettings().value("last/exportDir", QDir::homePath()).toString() + "/" + def,
                                                      tr("Cabrillo log (*.log *.cbr);;All files (*)"));
    if (path.isEmpty())
        return;
    QSettings().setValue("last/exportDir", QFileInfo(path).absolutePath());

    if (!m_rules.cabrilloFields.isEmpty()) {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Cabrillo header — %1").arg(m_contest.name));
        auto *lay = new QVBoxLayout(&dlg);
        QSettings s;
        auto *who = new QLabel(tr("CALLSIGN: <b>%1</b><br>NAME: %2<br>EMAIL: %3<br>ADDRESS: %4<br>"
                                  "<i>(Configuration › Station)</i>")
                                   .arg(m_contest.myCall.toHtmlEscaped(),
                                        s.value("station/name").toString().toHtmlEscaped(),
                                        s.value("station/email").toString().toHtmlEscaped(),
                                        s.value("station/address").toString().toHtmlEscaped().replace("\n", ", ")),
                               &dlg);
        lay->addWidget(who);
        auto *form = new CabrilloFieldsForm(&dlg);
        form->setFields(m_rules.cabrilloFields, m_contest.fields);
        lay->addWidget(form);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(bb);
        if (dlg.exec() != QDialog::Accepted)
            return;
        m_contest.fields = form->values();
        if (!m_db->updateContestFields(m_contest.id, m_contest.fields))
            statusBar()->showMessage(tr("Database error: %1").arg(m_db->lastError()), 6000);
        recompute();                      // scoring may use contest.fields
    }
    QString err;
    const qint64 score = m_score.contains("total") ? qRound64(m_score.value("total").toDouble()) : -1;
    if (!Cabrillo::write(path, m_contest, m_rules, m_model->qsos(), score, &err))
        QMessageBox::critical(this, tr("Export Cabrillo"), err);
    else
        statusBar()->showMessage(tr("Cabrillo written to %1").arg(path), 6000);
}

// ADIF 3.1.8 (ADI) export of the whole log, for the general logbook (LoTW, QRZ, Log4OM…)
void MainWindow::exportAdif()
{
    const QString def = QString("%1_%2_%3.adi")
                            .arg(QString(m_contest.myCall).replace(QChar(0x2F), QChar(0x2D)), m_rules.shortName,
                                 m_contest.date.toString("yyyy"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export ADIF"),
                                                      QSettings().value("last/exportDir", QDir::homePath()).toString() + "/" + def,
                                                      tr("ADIF file (*.adi);;All files (*)"));
    if (path.isEmpty())
        return;
    QSettings().setValue("last/exportDir", QFileInfo(path).absolutePath());
    QHash<int, QString> status;
    for (const Qso &q : m_model->qsos())
        status.insert(q.id, m_model->statusOf(q.id));
    QString err;
    if (!Adif::write(path, m_contest, m_rules, m_model->qsos(), status,
                     QSettings().value("station/locator").toString(), &err))
        QMessageBox::critical(this, tr("Export ADIF"), err);
    else
        statusBar()->showMessage(tr("ADIF written to %1 (%n QSO(s))", nullptr, int(m_model->qsos().size())).arg(path), 6000);
}

void MainWindow::openSettings(int tab)
{
    stopAutoCq();
    delete m_trx;               // frees the serial port for the dialog's "Test connection"
    m_trx = nullptr;
    SettingsDialog dlg(this, tab, &m_rules, m_contest.fields);
    if (dlg.exec() == QDialog::Accepted) {
        applyCwConfig();
        // Contest options tab: only the Cabrillo header options of the contest are saved
        const QVariantMap f = dlg.contestFields();
        if (!m_rules.cabrilloFields.isEmpty() && f != m_contest.fields) {
            m_contest.fields = f;
            if (!m_db->updateContestFields(m_contest.id, m_contest.fields))
                statusBar()->showMessage(tr("Database error: %1").arg(m_db->lastError()), 6000);
            recompute();                       // scoring.js and QML get contest.fields
            m_entry->setMessage(tr("Contest options saved"), "ok");
        }
    }
    applyTrxConfig();
    m_entry->focusCall();
}

void MainWindow::updateContestClock()
{
    updateBandLock();
    const auto periods = m_rules.periodsUtc(m_contest.date);
    if (periods.isEmpty()) {
        m_clockLabel->clear();
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto fmt = [](qint64 secs) {
        const qint64 d = secs / 86400;
        const QString hms = QTime(0, 0).addSecs(int(secs % 86400)).toString("HH:mm:ss");
        return d > 0 ? QString("%1d %2").arg(d).arg(hms) : hms;
    };
    const int n = periods.size();
    for (int i = 0; i < n; ++i) {
        const auto &p = periods[i];
        const QString which = n > 1 ? tr(" (period %1/%2)").arg(i + 1).arg(n) : QString();
        if (now < p.first) {
            m_clockLabel->setText((i == 0 ? tr("Contest starts in %1") : tr("Next period in %1"))
                                      .arg(fmt(now.secsTo(p.first))) + which);
            return;
        }
        if (now < p.second) {
            m_clockLabel->setText(tr("<b style='color:#28a745'>Contest running</b>%1 — %2 left")
                                      .arg(which, fmt(now.secsTo(p.second))));
            return;
        }
    }
    m_clockLabel->setText(tr("Contest over"));
}

void MainWindow::showHelp()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("qTLF — keys and commands"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr(
        "<h3>Entry line</h3>"
        "<table cellspacing=4>"
        "<tr><td><b>TAB</b></td><td>Call ⇄ %1 (pre-filled from the log, then call_exchange or cty.dat)</td></tr>"
        "<tr><td><b>ENTER</b></td><td>Smart key: sends the CW message of the current step and logs "
        "the QSO at the end of the sequence (date/time taken at that moment)</td></tr>"
        "<tr><td><b>ESC</b></td><td>Always stops CW / auto-CQ, then: Call empty → clear Call + exchange; "
        "Call filled → clear Call; exchange empty → back to Call; exchange filled → clear exchange</td></tr>"
        "<tr><td><b>+</b></td><td>Toggle CQ / S&amp;P mode</td></tr>"
        "<tr><td><b>F1…F12</b></td><td>Send CW message (F12 = auto-CQ on/off). Right-click a key to modify it</td></tr>"
        "<tr><td><b>PgUp / PgDn</b></td><td>Call empty: CW speed ±1 WPM. Call filled: 2nd digit of the RST "
        "(cursor in Call = RST sent, cursor in exchange = RST received)</td></tr>"
        "<tr><td><b>Ctrl+ENTER</b></td><td>Log the QSO without sending CW</td></tr>"
        "<tr><td><b>TAB</b></td><td>Call ⇄ exchange received (Log button → exchange received)</td></tr>"
        "<tr><td><b>Ctrl+↑ / Ctrl+↓</b></td><td>Next / previous band</td></tr>"
        "<tr><td><b>Ctrl+F</b></td><td>Search the log (ESC clears the search)</td></tr>"
        "<tr><td><b>Ctrl+L</b></td><td>Clear Call, exchange received and search, cursor in Call</td></tr>"
        "<tr><td><b>Ctrl+T</b></td><td>Tag the last QSO with a comment</td></tr>"
        "<tr><td><b>Ctrl+R</b></td><td>Reload scoring.js, data files and QML windows</td></tr>"
        "</table>"
        "<h3>ENTER sequence</h3>"
        "<table cellspacing=4>"
        "<tr><th></th><th align=left>CQ mode</th><th align=left>S&amp;P mode</th></tr>"
        "<tr><td>Call empty</td><td>F1 (CQ)</td><td>my call (%)</td></tr>"
        "<tr><td>Call filled</td><td>F3, go to exchange</td><td>F2, go to exchange</td></tr>"
        "<tr><td>Exchange empty</td><td>F3</td><td>F2</td></tr>"
        "<tr><td>Exchange filled</td><td>CQ_TU_MSG (13) + log</td><td>S&amp;P_TU_MSG (14), go to Log button</td></tr>"
        "<tr><td>Log button</td><td>CQ_TU_MSG (13) + log</td><td>log (no CW) — TAB: back to exchange</td></tr>"
        "<tr><td>Call is a DUPE</td><td>F7 (QSO B4)</td><td>F7 (QSO B4)</td></tr>"
        "</table>"
        "<h3>Commands typed in the Call field</h3>"
        "<table cellspacing=4>"
        "<tr><td><b>7025</b> or <b>3.535</b></td><td>QSY (kHz or MHz)</td></tr>"
        "<tr><td><b>:band 40</b></td><td>change band</td></tr>"
        "<tr><td><b>:mode CW</b></td><td>change mode</td></tr>"
        "<tr><td><b>:tag text</b></td><td>tag the last QSO with a comment</td></tr>"
        "<tr><td><b>:del</b></td><td>delete the last QSO</td></tr>"
        "<tr><td><b>:find text</b></td><td>search the log</td></tr>"
        "<tr><td><b>:cab</b> / <b>:write</b></td><td>export Cabrillo</td></tr>"
        "<tr><td><b>:adif</b></td><td>export ADIF</td></tr>"
        "<tr><td><b>:score</b>, <b>:cfg</b>, <b>:reload</b>, <b>:open</b>, <b>:quit</b></td><td></td></tr>"
        "</table>"
        "<h3>Search syntax</h3>"
        "<p>Words match any field. <code>call:HA*</code> <code>band:40</code> <code>freq&gt;7020</code> "
        "<code>time&gt;=06:30</code> <code>date=2026-11-01</code> <code>exch:YT7MM</code> <code>comment:check</code> "
        "<code>tagged</code> <code>is:dupe</code> <code>is:invalid</code>; prefix with <code>-</code> to exclude.</p>"
        "<p>Edit any cell of the log by double-clicking it. Click 🗑 to delete a QSO.</p>")
                    .arg(m_rules.exchRcvdLabel.toHtmlEscaped()));
    box.exec();
    m_entry->focusCall();
}

// ===================================================================== TRX control

void MainWindow::applyTrxConfig()
{
    delete m_trx;
    m_trx = nullptr;
    m_trxCfg = TrxConfig::load();
    m_trx = Trx::create(m_trxCfg, this);
    if (!m_trx) {
        m_trxLabel->setText(m_trxCfg.enabled() ? tr("TRX: <span style='color:#dc3545'>%1 not available</span>")
                                                      .arg(m_trxCfg.type)
                                               : QString());
        m_trxLabel->setVisible(m_trxCfg.enabled());
        return;
    }
    m_trxLabel->setVisible(true);
    m_trxLabel->setText(tr("TRX: %1 …").arg(m_trx->description().toHtmlEscaped()));
    connect(m_trx, &Trx::frequencyChanged, this, [this](double khz) {
        m_entry->setFrequency(khz);         // band follows; not while the Freq field is being edited
    });
    connect(m_trx, &Trx::modeChanged, this, [this](const QString &rigMode) {
        const QString m = contestModeForRigMode(rigMode, m_rules.modes);
        if (!m.isEmpty() && m != m_entry->mode())
            m_entry->setMode(m);
    });
    connect(m_trx, &Trx::connectionChanged, this, &MainWindow::onTrxConnection);
    m_trx->start();
}

void MainWindow::onTrxConnection(bool connected, const QString &message)
{
    if (!m_trx)
        return;
    const QString rig = m_trx->description().toHtmlEscaped();
    if (connected && message.isEmpty()) {
        m_trxLabel->setText(tr("TRX: <span style='color:#28a745'>●</span> %1").arg(rig));
        m_trxLabel->setToolTip(tr("Rig connected"));
        statusBar()->showMessage(tr("TRX connected: %1").arg(m_trx->description()), 4000);
    } else if (connected) {
        statusBar()->showMessage(message, 6000);                     // e.g. QSY refused
    } else {
        m_trxLabel->setText(tr("TRX: <span style='color:#dc3545'>●</span> %1").arg(rig));
        m_trxLabel->setToolTip(message + "\n" + tr("qTLF retries every 5 s — Configuration › TRX"));
        statusBar()->showMessage(tr("TRX: %1").arg(message), 10000);
    }
}

// ===================================================================== band_change rule

void MainWindow::updateBandLock()
{
    if (!m_bandLockLabel)
        return;
    const QDateTime allowed = m_bandRun.changeAllowedAt(m_rules);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    // locked only while the contest runs, in the period of the band run (a new period frees the
    // band choice), and never for a clock before the last QSO (wrong PC clock, test logs…)
    bool locked = allowed.isValid() && now < allowed && now >= m_bandRun.last;
    const auto periods = m_rules.periodsUtc(m_contest.date);
    if (locked && !periods.isEmpty()) {
        int current = -1;
        for (int i = 0; i < periods.size(); ++i)
            if (now >= periods[i].first && now < periods[i].second)
                current = i;
        locked = current >= 0 && current == m_bandRun.period;
    }
    if (!locked) {
        m_bandLockLabel->hide();
        m_bandWarned.clear();
        return;
    }
    const qint64 left = now.secsTo(allowed);
    const QString mmss = QString("%1:%2").arg(left / 60).arg(left % 60, 2, 10, QChar('0'));
    const bool wrongBand = m_entry->band() != m_bandRun.band;
    m_bandLockLabel->setText(
        wrongBand ? tr("<b style='color:#dc3545'>Band locked on %1 — %2</b>").arg(m_bandRun.band, mmss)
                  : tr("<span style='color:#b8860b'>%1 locked %2</span>").arg(m_bandRun.band, mmss));
    m_bandLockLabel->setToolTip(tr("Contest rule: at least %1 min on a band, counted from the first QSO on the band (%2 UTC).\n"
                                   "QSOs on another band before %3 UTC are marked BAND CHANGE.")
                                    .arg(m_rules.bandChangeMinutes)
                                    .arg(m_bandRun.first.toString("HH:mm"))
                                    .arg(allowed.toString("HH:mm:ss")));
    m_bandLockLabel->show();
    if (wrongBand && m_bandWarned != m_entry->band()) {
        m_bandWarned = m_entry->band();
        m_entry->setMessage(tr("Band change too early: QSOs on %1 before %2 UTC are invalid (BAND CHANGE). "
                               "Stay on %3 for %4 more.")
                                .arg(m_entry->band(), allowed.toString("HH:mm"), m_bandRun.band, mmss),
                            "warn");
    } else if (!wrongBand) {
        m_bandWarned.clear();
    }
}

// ===================================================================== entry line helpers

// Drop-down lists of the entry line, rebuilt when the log changes:
//   Call      = calls of the log of this contest (newest first), then the calls of call_exchange
//   Exch Rcvd = (exchange.received.completion) exchanges of the log, then of call_exchange
void MainWindow::updateCompletions()
{
    QStringList calls, exchs;
    const QVector<Qso> &qsos = m_model->qsos();
    for (int i = qsos.size() - 1; i >= 0; --i) {
        calls << qsos.at(i).call.trimmed().toUpper();
        exchs << qsos.at(i).exchRcvd.trimmed().toUpper();
    }
    calls << m_callDb.calls();
    calls.removeAll(QString());
    calls.removeDuplicates();                  // keeps the first one: log before file
    m_entry->setCallCompletion(calls);
    if (m_rules.exchRcvdCompletion) {
        exchs << m_callDb.exchangeValues();
        exchs.removeAll(QString());
        exchs.removeDuplicates();
        m_entry->setExchCompletion(exchs);
    } else {
        m_entry->setExchCompletion({});
    }
}

// TAB pre-fill of the exchange received (exchange.received.prefill):
//   1. the log of this contest: newest QSO with this station (F5ABC = F5ABC/P = F5ABC/MM)
//   2. "call_exchange": the exchange column of the call_exchange file
//      "cqz" | "ituz" | "cont" | "dxcc" | "country": that field of cty.dat for this call
//   "none": no pre-fill at all (serial numbers…)
QString MainWindow::exchangeFor(const QString &call) const
{
    const QString mode = m_rules.exchRcvdPrefill;
    if (mode == "none")
        return QString();
    const QString station = QsoChecks::stationCall(call);
    // 1. this contest: the newest QSO with this station
    const Qso *best = nullptr;
    for (const Qso &q : m_model->qsos())
        if (!q.exchRcvd.trimmed().isEmpty() && QsoChecks::stationCall(q.call) == station
            && (!best || q.utc >= best->utc))
            best = &q;
    if (best)
        return best->exchRcvd.trimmed().toUpper();
    // 2. call_exchange file, or a field of the country file
    if (mode == "call_exchange")
        return m_callDb.exchange(call);
    if (!m_cty.isLoaded())
        return QString();
    const CtyDat::Info c = m_cty.lookup(call);
    if (!c.valid)
        return QString();
    if (mode == "cqz") return c.cqZone > 0 ? QString::number(c.cqZone) : QString();
    if (mode == "ituz") return c.ituZone > 0 ? QString::number(c.ituZone) : QString();
    if (mode == "cont") return c.continent;
    if (mode == "dxcc") return c.prefix;
    if (mode == "country") return c.country.toUpper();
    return QString();
}

void MainWindow::clearEntryLine()
{
    m_search->clear();
    m_entry->clearEntry();
}

// TAB pressed in a widget that does not handle it (F-key buttons, CQ button, log, docks…):
// back to Call — TAB never wanders to the search field or the buttons
bool MainWindow::focusNextPrevChild(bool)
{
    m_entry->focusCall();
    return true;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::KeyPress && (obj == m_search || obj == m_view)) {
        const int key = static_cast<QKeyEvent *>(ev)->key();
        if (obj == m_search && key == Qt::Key_Escape) {
            m_search->clear();
            return true;
        }
        if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
            m_entry->focusCall();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

// ===================================================================== Configuration › Contest

void MainWindow::updateWindowTitle()
{
    setWindowTitle(QString("qTLF — %1 — %2 — %3")
                       .arg(m_contest.name, m_contest.myCall, m_contest.date.toString("yyyy-MM-dd")));
}

// Configuration › Contest options = first tab of the configuration dialog
void MainWindow::editContest()
{
    openSettings(SettingsDialog::ContestTab);
}

// ===================================================================== screens

// The saved size / positions may come from another screen setup (bigger screen, monitor
// unplugged, other resolution): the main window is brought inside the available area of its
// screen (shrunk if needed), and a floating dock that is on no screen comes back next to it.
void MainWindow::fitToScreens()
{
    QScreen *sc = QGuiApplication::screenAt(frameGeometry().center());
    if (!sc)
        sc = QGuiApplication::primaryScreen();
    if (!sc)
        return;
    const QRect avail = sc->availableGeometry();
    const QRect fg = frameGeometry();
    const int dw = fg.width() - width(), dh = fg.height() - height();   // window frame
    if (!isMaximized() && !isFullScreen() && !avail.contains(fg)) {
        const int w = qMax(minimumSizeHint().width(), qMin(width(), avail.width() - dw));
        const int h = qMax(minimumSizeHint().height(), qMin(height(), avail.height() - dh));
        resize(w, h);
        const int x = qBound(avail.left(), fg.left(), qMax(avail.left(), avail.right() - (w + dw) + 1));
        const int y = qBound(avail.top(), fg.top(), qMax(avail.top(), avail.bottom() - (h + dh) + 1));
        move(x, y);
    }
    int n = 0;
    for (QDockWidget *d : findChildren<QDockWidget *>()) {
        if (!d->isFloating() || !d->isVisible())
            continue;
        if (QGuiApplication::screenAt(d->frameGeometry().center()))
            continue;                                     // on a screen: keep it where it was
        d->resize(qMin(d->width(), avail.width() / 2), qMin(d->height(), avail.height() / 2));
        d->move(avail.left() + 40 + 30 * n, avail.top() + 40 + 30 * n);
        ++n;
    }
}
