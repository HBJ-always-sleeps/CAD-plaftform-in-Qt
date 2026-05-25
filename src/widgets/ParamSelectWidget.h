#ifndef PARAM_SELECT_WIDGET_H
#define PARAM_SELECT_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QVector>

/**
 * 下拉选择组件
 */
class ParamSelectWidget : public QWidget
{
    Q_OBJECT

public:
    struct Option {
        QString value;
        QString text;
    };

    explicit ParamSelectWidget(const QString &label, const QVector<Option> &options, const QString &defaultValue = "", QWidget *parent = nullptr);

    QString getValue() const { return m_combo->currentData().toString(); }
    void setValue(const QString &value);

private:
    void setupUi(const QString &label, const QVector<Option> &options, const QString &defaultValue);

    QComboBox *m_combo;
};

#endif // PARAM_SELECT_WIDGET_H