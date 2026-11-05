#include "CabrilloFieldsForm.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

CabrilloFieldsForm::CabrilloFieldsForm(QWidget *parent) : QWidget(parent), m_form(new QFormLayout(this))
{
    m_form->setContentsMargins(0, 0, 0, 0);
}

CabrilloFieldsForm::CabrilloFieldsForm(QFormLayout *target, QWidget *parent) : QWidget(parent), m_form(target)
{
    hide();                                    // only holds the editors, the rows live in 'target'
}

void CabrilloFieldsForm::setFields(const QList<CabrilloFieldDef> &defs, const QVariantMap &values)
{
    for (QWidget *w : m_editors)
        m_form->removeRow(w);                  // deletes the editor and its label
    m_editors.clear();
    m_defs = defs;
    for (const CabrilloFieldDef &d : defs) {
        const QString current = values.value(d.tag, d.defaultValue).toString();
        QWidget *w;
        if (!d.values.isEmpty()) {
            auto *cb = new QComboBox(parentWidget());
            cb->addItems(d.values);
            const int i = cb->findText(current);
            cb->setCurrentIndex(i >= 0 ? i : 0);
            w = cb;
        } else {
            auto *le = new QLineEdit(current, parentWidget());
            le->setPlaceholderText(d.tag);
            w = le;
        }
        w->setToolTip(QString("Cabrillo: %1").arg(d.tag));
        m_form->addRow(d.label, w);
        m_editors << w;
    }
}

QVariantMap CabrilloFieldsForm::values() const
{
    QVariantMap m;
    for (int i = 0; i < m_defs.size(); ++i) {
        QString v;
        if (auto *cb = qobject_cast<QComboBox *>(m_editors[i]))
            v = cb->currentText();
        else if (auto *le = qobject_cast<QLineEdit *>(m_editors[i]))
            v = le->text().trimmed().toUpper();
        m.insert(m_defs[i].tag, v);
    }
    return m;
}
