#include "ContestSelectDialog.h"

#include "CabrilloFieldsForm.h"
#include "Database.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

ContestSelectDialog::ContestSelectDialog(Database *db, const QString &rulesRoot, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(tr("qTLF — Select contest"));
    resize(760, 460);
    QStringList errors;
    m_rules = ContestRules::discover(rulesRoot, &errors);

    auto *lay = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    lay->addWidget(m_tabs);

    // ---- existing contests
    auto *openPage = new QWidget;
    auto *ol = new QVBoxLayout(openPage);
    m_table = new QTableWidget(0, 6, openPage);
    m_table->setHorizontalHeaderLabels({tr("Contest"), tr("Rules"), tr("Date"), tr("Call"), tr("Category"), tr("QSOs")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ContestSelectDialog::openSelected);
    ol->addWidget(m_table);
    auto *ob = new QHBoxLayout;
    auto *del = new QPushButton(tr("Delete…"), openPage);
    auto *open = new QPushButton(tr("Open"), openPage);
    open->setDefault(true);
    del->setAutoDefault(false);
    connect(m_table, &QTableWidget::activated, this, &ContestSelectDialog::openSelected);
    connect(del, &QPushButton::clicked, this, &ContestSelectDialog::deleteSelected);
    connect(open, &QPushButton::clicked, this, &ContestSelectDialog::openSelected);
    ob->addWidget(del);
    ob->addStretch();
    ob->addWidget(open);
    ol->addLayout(ob);
    m_tabs->addTab(openPage, tr("Open existing contest"));

    // ---- new contest
    auto *newPage = new QWidget;
    auto *nl = new QVBoxLayout(newPage);
    auto *form = new QFormLayout;
    m_rulesCombo = new QComboBox(newPage);
    for (const ContestRules &r : m_rules)
        m_rulesCombo->addItem(QString("%1  (%2)").arg(r.name, r.id), r.id);
    m_rulesInfo = new QLabel(newPage);
    m_rulesInfo->setWordWrap(true);
    m_rulesInfo->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_rulesInfo->setOpenExternalLinks(true);
    m_date = new QDateEdit(QDate::currentDate(), newPage);
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat("yyyy-MM-dd");
    m_name = new QLineEdit(newPage);
    m_call = new QLineEdit(QSettings().value("station/call").toString(), newPage);
    m_exch = new QLineEdit(newPage);
    m_exchLabel = new QLabel(newPage);
    for (QLineEdit *e : {m_call, m_exch}) {
        connect(e, &QLineEdit::textEdited, e, [e](const QString &t) {
            const int p = e->cursorPosition();
            e->setText(t.toUpper());
            e->setCursorPosition(p);
        });
    }
    form->addRow(tr("Contest rules"), m_rulesCombo);
    form->addRow(QString(), m_rulesInfo);
    form->addRow(tr("Contest date"), m_date);
    form->addRow(tr("Log name"), m_name);
    form->addRow(tr("My callsign"), m_call);
    form->addRow(m_exchLabel, m_exch);
    m_fields = new CabrilloFieldsForm(form, newPage);  // Cabrillo fields of the contest (cabrillo.fields)
    nl->addLayout(form);
    nl->addStretch();
    if (!errors.isEmpty()) {
        auto *err = new QLabel(tr("Some rules could not be loaded:\n%1").arg(errors.join('\n')), newPage);
        err->setStyleSheet("color: #dc3545;");
        err->setWordWrap(true);
        nl->addWidget(err);
    }
    auto *nb = new QHBoxLayout;
    auto *rulesDir = new QLabel(tr("Rules folder: %1").arg(rulesRoot), newPage);
    rulesDir->setEnabled(false);
    auto *create = new QPushButton(tr("Create && open"), newPage);
    connect(create, &QPushButton::clicked, this, &ContestSelectDialog::createContest);
    nb->addWidget(rulesDir);
    nb->addStretch();
    nb->addWidget(create);
    nl->addLayout(nb);
    m_tabs->addTab(newPage, tr("New contest"));

    connect(m_rulesCombo, &QComboBox::currentIndexChanged, this, &ContestSelectDialog::onRulesChanged);
    connect(m_date, &QDateEdit::dateChanged, this, [this] {
        if (const ContestRules *r = currentRules())
            m_name->setText(QString("%1 %2").arg(r->shortName).arg(m_date->date().year()));
    });

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);

    refreshExisting();
    onRulesChanged();
    m_tabs->setCurrentIndex(m_table->rowCount() > 0 ? 0 : 1);
    if (m_table->rowCount() > 0)
        m_table->setFocus();
    else
        m_call->setFocus();
}

const ContestRules *ContestSelectDialog::currentRules() const
{
    const QString id = m_rulesCombo->currentData().toString();
    for (const ContestRules &r : m_rules)
        if (r.id == id)
            return &r;
    return nullptr;
}

void ContestSelectDialog::refreshExisting()
{
    const auto list = m_db->contests();
    m_table->setRowCount(list.size());
    int row = 0;
    for (const ContestInfo &c : list) {
        const QStringList cells = {c.name, c.rulesId, c.date.toString("yyyy-MM-dd"), c.myCall, c.category,
                                   QString::number(c.qsoCount)};
        for (int col = 0; col < cells.size(); ++col) {
            auto *it = new QTableWidgetItem(cells[col]);
            it->setData(Qt::UserRole, c.id);
            m_table->setItem(row, col, it);
        }
        ++row;
    }
    if (row > 0)
        m_table->selectRow(0);
}

void ContestSelectDialog::onRulesChanged()
{
    const ContestRules *r = currentRules();
    if (!r) {
        m_rulesInfo->setText(tr("No contest definition found in the rules folder."));
        return;
    }
    QString info = r->description.toHtmlEscaped();
    if (!r->url.isEmpty())
        info += QString("<br><a href=\"%1\">%1</a>").arg(r->url.toHtmlEscaped());
    m_rulesInfo->setText(info);
    m_fields->setFields(r->cabrilloFields);
    m_date->setDate(r->nextDefaultDate(QDate::currentDate()));
    m_name->setText(QString("%1 %2").arg(r->shortName).arg(m_date->date().year()));
    const bool fixed = r->exchSentType == "fixed";
    m_exchLabel->setText(fixed ? r->exchSentLabel : tr("Exchange sent"));
    m_exch->setEnabled(fixed);
    m_exch->setPlaceholderText(fixed ? r->exchSentPrompt
                                     : tr("serial number starting at %1").arg(r->formatSerial(r->serialStart)));
    if (!fixed)
        m_exch->clear();
}

void ContestSelectDialog::openSelected()
{
    const auto items = m_table->selectedItems();
    if (items.isEmpty())
        return;
    m_selected = items.first()->data(Qt::UserRole).toInt();
    accept();
}

void ContestSelectDialog::deleteSelected()
{
    const auto items = m_table->selectedItems();
    if (items.isEmpty())
        return;
    const int id = items.first()->data(Qt::UserRole).toInt();
    const QString name = m_table->item(items.first()->row(), 0)->text();
    const QString n = m_table->item(items.first()->row(), 5)->text();
    if (QMessageBox::warning(this, tr("Delete contest"),
                             tr("Delete contest \"%1\" and its %2 QSO(s)?\nThis cannot be undone.").arg(name, n),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!m_db->deleteContest(id))
        QMessageBox::critical(this, tr("Database error"), m_db->lastError());
    refreshExisting();
}

void ContestSelectDialog::createContest()
{
    const ContestRules *r = currentRules();
    if (!r)
        return;
    ContestInfo c;
    c.rulesId = r->id;
    c.name = m_name->text().trimmed();
    c.date = m_date->date();
    c.myCall = m_call->text().trimmed().toUpper();
    c.fields = m_fields->values();
    QStringList cat;                                   // short summary for the contest list
    for (auto it = c.fields.cbegin(); it != c.fields.cend(); ++it)
        if (it.key().startsWith("CATEGORY-") && !it.value().toString().isEmpty())
            cat << it.value().toString();
    c.category = cat.join(' ');
    c.exchSent = m_exch->text().trimmed().toUpper();
    if (c.myCall.isEmpty()) {
        QMessageBox::warning(this, tr("New contest"), tr("Please enter your callsign."));
        m_call->setFocus();
        return;
    }
    if (r->exchSentType == "fixed" && c.exchSent.isEmpty()) {
        QMessageBox::warning(this, tr("New contest"), tr("Please enter: %1").arg(r->exchSentLabel));
        m_exch->setFocus();
        return;
    }
    if (c.name.isEmpty())
        c.name = QString("%1 %2").arg(r->shortName).arg(c.date.year());
    const int id = m_db->createContest(c);
    if (id <= 0) {
        QMessageBox::critical(this, tr("Database error"), m_db->lastError());
        return;
    }
    QSettings().setValue("station/call", c.myCall);
    m_selected = id;
    accept();
}
