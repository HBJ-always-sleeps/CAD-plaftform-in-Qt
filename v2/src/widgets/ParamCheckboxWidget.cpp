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
    m_checkbox->setStyleSheet(
        "QCheckBox { spacing: 10px; padding: 6px 10px; border-radius: 5px; "
        "font-size: 14px; color: #A8A8A8; background-color: #151515; }"
        "QCheckBox:checked { color: #FFFFFF; background-color: #164C35; "
        "border: 1px solid #35D07F; font-weight: 600; }"
        "QCheckBox::indicator { width: 20px; height: 20px; border-radius: 3px; "
        "border: 2px solid #777777; background-color: #0B0B0B; }"
        "QCheckBox::indicator:checked { background-color: #35D07F; border-color: #9FFFC3; }"
        "QCheckBox::indicator:hover { border-color: #35D07F; }"
    );

    auto updateText = [this, label](bool enabled) {
        m_checkbox->setText(enabled
            ? QStringLiteral("✓ %1（已启用）").arg(label)
            : QStringLiteral("□ %1（未启用）").arg(label));
    };
    connect(m_checkbox, &QCheckBox::toggled, this, updateText);
    m_checkbox->setChecked(checked);
    updateText(checked);

    layout->addWidget(m_checkbox);
}
