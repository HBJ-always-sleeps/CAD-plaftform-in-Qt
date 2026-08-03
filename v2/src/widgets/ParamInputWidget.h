#ifndef PARAM_INPUT_WIDGET_H
#define PARAM_INPUT_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

/**
 * 参数输入组件
 */
class ParamInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ParamInputWidget(const QString &label, const QString &value = "", bool isPath = false, QWidget *parent = nullptr);

    QString getValue() const { return m_lineEdit->text(); }
    void setValue(const QString &value) { m_lineEdit->setText(value); }

private slots:
    void onBrowseClicked();

private:
    void setupUi(const QString &label, const QString &value);

    bool m_isPath;
    QLineEdit *m_lineEdit;
};

#endif // PARAM_INPUT_WIDGET_H