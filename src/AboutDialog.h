#pragma once

#include <QDialog>

// Help > About qTLF
class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    AboutDialog(const QString &rulesDir, const QString &databasePath, QWidget *parent = nullptr);
};
