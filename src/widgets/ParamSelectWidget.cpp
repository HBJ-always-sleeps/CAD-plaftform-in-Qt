#include "ParamSelectWidget.h"

ParamSelectWidget::ParamSelectWidget(const QString &label, const QVector<Option> &options, const QString &defaultValue, QWidget *parent)
    : QWidget(parent)
{
    setupUi(label, options, defaultValue);
}

void ParamSelectWidget::setupUi(const QString &label, const QVector<Option> &options, const QString &defaultValue)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(10);

    // 标签
    QLabel *labelWidget = new QLabel(label.toUpper(), this);
    labelWidget->setStyleSheet("font-size: 13px; font-family: 'Consolas'; color: #808080;");
    layout->addWidget(labelWidget);

    // 下拉框
    m_combo = new QComboBox(this);
    m_combo->setMinimumHeight(44);
    m_combo->setStyleSheet(
        "QComboBox { background-color: #1A1A1A; border: 1px solid #2A2A2A; border-radius: 8px; "
        "padding: 12px 16px; font-size: 14px; color: #E4E3E0; }"
        "QComboBox:focus { border: 1px solid #E4E3E0; }"
        "QComboBox::drop-down { border: none; width: 30px; }"
        "QComboBox QAbstractItemView { background-color: #1A1A1A; border: 1px solid #2A2A2A; "
        "selection-background-color: #2A2A2A; color: #E4E3E0; }"
    );

    for (const Option &opt : options) {
        m_combo->addItem(opt.text, opt.value);
    }

    // 设置默认值
    setValue(defaultValue);

    layout->addWidget(m_combo);
}

void ParamSelectWidget::setValue(const QString &value)
{
    int idx = m_combo->findData(value);
    if (idx >= 0) {
        m_combo->setCurrentIndex(idx);
    }
}