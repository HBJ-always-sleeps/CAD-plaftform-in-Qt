#include "ParamCheckboxWidget.h"

ParamCheckboxWidget::ParamCheckboxWidget(const QString &label, bool checked, QWidget *parent)
    : QWidget(parent)
{
    setupUi(label, checked);
}

void ParamCheckboxWidget::setupUi(const QString &label, bool checked)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 12);
    layout->setSpacing(0);

    m_checkbox = new QCheckBox(label, this);
    m_checkbox->setChecked(checked);
    m_checkbox->setStyleSheet(
        "QCheckBox { spacing: 8px; font-size: 14px; color: #E4E3E0; }"
        "QCheckBox::indicator { width: 20px; height: 20px; border-radius: 4px; "
        "border: 2px solid #333333; background-color: #0F0F0F; }"
        "QCheckBox::indicator:checked { background-color: #E4E3E0; border-color: #E4E3E0; }"
        "QCheckBox::indicator:hover { border-color: #E4E3E0; }"
    );

    layout->addWidget(m_checkbox);
}