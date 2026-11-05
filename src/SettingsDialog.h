#pragma once

#include <QDialog>
#include <QList>
#include <QVariantMap>

#include "TrxConfig.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
class CabrilloFieldsForm;
class ContestRules;

// Configuration dialog (Contest options, Station, Keyer, CW messages, TRX, Paths).
// Contest options = the Cabrillo header options of the open contest (cabrillo.fields of its rules),
// returned by contestFields() and saved with the contest by MainWindow; everything else is
// stored in the qTLF INI file (QSettings, IniFormat).
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr, int initialTab = 0, const ContestRules *rules = nullptr,
                            const QVariantMap &contestFields = {});
    void accept() override;
    QVariantMap contestFields() const;   // Contest options tab (empty map if no option)

    enum Tab { ContestTab, StationTab, KeyerTab, MessagesTab, TrxTab, PathsTab, LanguageTab };

private:
    void updateKeyerFields();
    void buildTrxTab(QTabWidget *tabs);
    void updateTrxFields();
    void fillPorts();
    void reloadRigModels();
    void testTrx();
    TrxConfig trxFromFields(bool *ok) const;

    CabrilloFieldsForm *m_contestForm = nullptr;
    QComboBox *m_language = nullptr;
    QLineEdit *m_call, *m_name, *m_email, *m_locator, *m_rulesPath, *m_dbPath;
    QPlainTextEdit *m_address;
    QComboBox *m_keyerType;
    QLineEdit *m_keyerAddress, *m_initHex;
    QSpinBox *m_port, *m_speed, *m_autoCq;
    QList<QLineEdit *> m_messages;

    QComboBox *m_trxType, *m_rigModel, *m_rigSpeed, *m_rigPort;
    QLineEdit *m_hamlibLib;
    QLabel *m_hamlibStatus;
    QSpinBox *m_pollMs;
    QCheckBox *m_setFreq, *m_followMode;
    QPushButton *m_trxTest;
    QLabel *m_trxResult;
};
