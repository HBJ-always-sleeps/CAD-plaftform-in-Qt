#include "FileRowWidget.h"
#include <QFileInfo>

FileRowWidget::FileRowWidget(const QString &label, QWidget *parent)
    : QFrame(parent)
    , m_labelText(label)
{
    setupUi();
}

void FileRowWidget::setupUi()
{
    setObjectName("FileRow");
    setFixedHeight(64);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    // 标签
    m_label = new QLabel(m_labelText, this);
    m_label->setFixedWidth(80);
    m_label->setStyleSheet("font-size: 12px; font-family: 'Consolas'; color: #707070;");
    layout->addWidget(m_label);

    // 文件信息标签
    m_fileLabel = new QLabel("未选择文件", this);
    m_fileLabel->setStyleSheet("font-size: 14px; font-family: 'Consolas'; color: #505050; font-style: italic;");
    layout->addWidget(m_fileLabel, 1);

    // 选择按钮
    m_selectBtn = new QPushButton("选择", this);
    m_selectBtn->setFixedSize(80, 36);
    m_selectBtn->setStyleSheet(
        "QPushButton { background-color: #E4E3E0; color: #0F0F0F; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background-color: #FFFFFF; }"
    );
    connect(m_selectBtn, &QPushButton::clicked, this, &FileRowWidget::onSelectClicked);
    layout->addWidget(m_selectBtn);

    // 清除按钮
    m_clearBtn = new QPushButton("清除", this);
    m_clearBtn->setFixedSize(80, 36);
    m_clearBtn->setStyleSheet(
        "QPushButton { background-color: transparent; color: #FF5555; border: 1px solid #FF5555; border-radius: 6px; }"
        "QPushButton:hover { background-color: rgba(255, 85, 85, 0.1); }"
    );
    connect(m_clearBtn, &QPushButton::clicked, this, &FileRowWidget::onClearClicked);
    layout->addWidget(m_clearBtn);
}

void FileRowWidget::onSelectClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择 DXF 文件", "", "DXF Files (*.dxf)");
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        m_fileInfo = QJsonObject{
            {"name", fileInfo.fileName()},
            {"path", filePath},
            {"size", fileInfo.size()}
        };

        m_fileLabel->setText(QString("✓ %1").arg(fileInfo.fileName()));
        m_fileLabel->setStyleSheet("font-size: 14px; font-family: 'Consolas'; color: #50FA7B;");

        emit fileSelected(m_fileInfo);
    }
}

void FileRowWidget::onClearClicked()
{
    clearFile();
    emit fileCleared();
}

void FileRowWidget::clearFile()
{
    m_fileInfo = QJsonObject();
    m_fileLabel->setText("未选择文件");
    m_fileLabel->setStyleSheet("font-size: 14px; font-family: 'Consolas'; color: #505050; font-style: italic;");
}