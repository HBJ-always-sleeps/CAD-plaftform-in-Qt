#include "ParamInputWidget.h"
#include <QFileDialog>

ParamInputWidget::ParamInputWidget(const QString &label, const QString &value, bool isPath, QWidget *parent)
    : QWidget(parent)
    , m_isPath(isPath)
{
    setupUi(label, value);
}

void ParamInputWidget::setupUi(const QString &label, const QString &value)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(10);

    // 标签
    QLabel *labelWidget = new QLabel(label.toUpper(), this);
    labelWidget->setStyleSheet("font-size: 13px; font-family: 'Consolas'; color: #808080;");
    layout->addWidget(labelWidget);

    // 输入框布局
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(8);

    m_lineEdit = new QLineEdit(value, this);
    m_lineEdit->setMinimumHeight(44);
    m_lineEdit->setStyleSheet(
        "QLineEdit { background-color: #1A1A1A; border: 1px solid #2A2A2A; border-radius: 8px; "
        "padding: 12px 16px; font-size: 14px; color: #E4E3E0; }"
        "QLineEdit:focus { border: 1px solid #E4E3E0; }"
    );
    inputLayout->addWidget(m_lineEdit, 1);

    // 如果是路径类型，添加浏览按钮
    if (m_isPath) {
        QPushButton *browseBtn = new QPushButton("...", this);
        browseBtn->setFixedSize(40, 44);
        browseBtn->setStyleSheet(
            "QPushButton { background-color: #1A1A1A; border: 1px solid #2A2A2A; border-radius: 8px; }"
            "QPushButton:hover { border-color: #E4E3E0; }"
        );
        connect(browseBtn, &QPushButton::clicked, this, &ParamInputWidget::onBrowseClicked);
        inputLayout->addWidget(browseBtn);
    }

    layout->addLayout(inputLayout);
}

void ParamInputWidget::onBrowseClicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择输出目录");
    if (!dirPath.isEmpty()) {
        m_lineEdit->setText(dirPath);
    }
}