#ifndef FILE_ROW_WIDGET_H
#define FILE_ROW_WIDGET_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QJsonObject>

/**
 * 文件选择行组件
 */
class FileRowWidget : public QFrame
{
    Q_OBJECT

public:
    explicit FileRowWidget(const QString &label, QWidget *parent = nullptr,
                           const QString &dialogTitle = QStringLiteral("选择 DXF 文件"),
                           const QString &fileFilter = QStringLiteral("DXF Files (*.dxf)"));

    QJsonObject getFileInfo() const { return m_fileInfo; }
    bool hasFile() const { return !m_fileInfo.isEmpty(); }
    void clearFile();

signals:
    void fileSelected(const QJsonObject &fileInfo);
    void fileCleared();

private slots:
    void onSelectClicked();
    void onClearClicked();

private:
    void setupUi();

    QString m_labelText;
    QString m_dialogTitle;
    QString m_fileFilter;
    QJsonObject m_fileInfo;

    QLabel *m_label;
    QLabel *m_fileLabel;
    QPushButton *m_selectBtn;
    QPushButton *m_clearBtn;
};

#endif // FILE_ROW_WIDGET_H
