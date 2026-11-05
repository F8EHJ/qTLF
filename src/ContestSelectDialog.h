#pragma once

#include "ContestRules.h"

#include <QDialog>

class Database;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QTabWidget;
class QTableWidget;
class CabrilloFieldsForm;

// Start-up dialog: open an existing contest from the database, or create a new one
// from one of the contest definitions found in rules/.
class ContestSelectDialog : public QDialog
{
    Q_OBJECT
public:
    ContestSelectDialog(Database *db, const QString &rulesRoot, QWidget *parent = nullptr);
    int selectedContestId() const { return m_selected; }

private:
    void refreshExisting();
    void onRulesChanged();
    void openSelected();
    void deleteSelected();
    void createContest();
    const ContestRules *currentRules() const;

    Database *m_db;
    QList<ContestRules> m_rules;
    int m_selected = 0;

    QTabWidget *m_tabs;
    QTableWidget *m_table;
    QComboBox *m_rulesCombo;
    CabrilloFieldsForm *m_fields = nullptr;
    QLabel *m_rulesInfo, *m_exchLabel;
    QDateEdit *m_date;
    QLineEdit *m_name, *m_call, *m_exch;
};
