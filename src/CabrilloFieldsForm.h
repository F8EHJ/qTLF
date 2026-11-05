#pragma once

#include "ContestRules.h"

#include <QVariantMap>
#include <QWidget>

class QFormLayout;

// Form for the contest-specific Cabrillo fields (cabrillo.fields of contest.json):
// a combo box for a list of values, a line edit for free text.
class CabrilloFieldsForm : public QWidget
{
    Q_OBJECT
public:
    explicit CabrilloFieldsForm(QWidget *parent = nullptr);
    // rows are added to an existing form layout (same column of labels as the other fields)
    CabrilloFieldsForm(QFormLayout *target, QWidget *parent);
    void setFields(const QList<CabrilloFieldDef> &defs, const QVariantMap &values = {});
    QVariantMap values() const;
    bool isEmpty() const { return m_defs.isEmpty(); }

private:
    QFormLayout *m_form;
    QList<CabrilloFieldDef> m_defs;
    QList<QWidget *> m_editors;
};
