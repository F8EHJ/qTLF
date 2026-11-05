#include "AboutDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

AboutDialog::AboutDialog(const QString &rulesDir, const QString &databasePath, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About qTLF"));

    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->setSpacing(18);

    auto *icon = new QLabel(this);
    icon->setPixmap(QPixmap(":/icons/qtlf-128.png"));
    icon->setAlignment(Qt::AlignTop);
    top->addWidget(icon, 0, Qt::AlignTop);

    auto *text = new QLabel(this);
    text->setTextFormat(Qt::RichText);
    text->setWordWrap(true);
    text->setOpenExternalLinks(true);
    text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    text->setMinimumWidth(420);
    text->setText(tr(
        "<h2 style='margin-bottom:0'>qTLF %1</h2>"
        "<p style='margin-top:4px'><b>User-customizable contest logging software</b><br>for radio amateurs</p>"
        "<p><b>Author:</b> F8EHJ<br>"
        "<i>(with Claude AI help for clean deployment)</i></p>"
        "<p><b>Inspired by</b> <a href='https://tlf.github.io/'>TLF</a>, the Linux contest logger "
        "by PA0R.</p>"
        "<p><b>Why \"TLF\"?</b> It's an old joke about someone sending bad CW (Morse code) over the "
        "radio, as if stomping on the key with their foot. TLF stands for <b>\"Try Left Foot instead.\"</b></p>"
        "<p>Written in C++ / Qt (built with Qt %2, running on Qt %3).</p>")
                      .arg(QApplication::applicationVersion(), QT_VERSION_STR, qVersion()));
    top->addWidget(text, 1);
    lay->addLayout(top);

    auto *paths = new QLabel(this);
    paths->setTextFormat(Qt::RichText);
    paths->setWordWrap(true);
    paths->setTextInteractionFlags(Qt::TextSelectableByMouse);
    paths->setEnabled(false);
    paths->setText(tr("Rules: %1<br>Database: %2<br>Configuration: %3")
                       .arg(rulesDir.toHtmlEscaped(), databasePath.toHtmlEscaped(),
                            QSettings().fileName().toHtmlEscaped()));
    lay->addWidget(paths);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    auto *aboutQt = bb->addButton(tr("About Qt"), QDialogButtonBox::HelpRole);
    connect(aboutQt, &QPushButton::clicked, qApp, &QApplication::aboutQt);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    lay->addWidget(bb);
    layout()->setSizeConstraint(QLayout::SetFixedSize);
}
