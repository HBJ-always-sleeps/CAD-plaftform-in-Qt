#ifndef PARAM_CHECKBOX_WIDGET_H
#define PARAM_CHECKBOX_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>

/**
 * 复选框参数组件
 */
class ParamCheckboxWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ParamCheckboxWidget(const QString &label, bool checked = false, QWidget *parent = nullptr);

    bool isChecked() const { return m_checkbox->isChecked(); }
    void setChecked(bool checked) { m_checkbox->setChecked(checked); }

private:
    void setupUi(const QString &label, bool checked);

    QCheckBox *m_checkbox;
};

#endif // PARAM_CHECKBOX_WIDGET_H