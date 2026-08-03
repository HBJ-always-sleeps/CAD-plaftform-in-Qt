#include "MainWindow.h"
#include "Config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QInputMethod>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentTask("autoline")
    , m_executing(false)
    , m_worker(nullptr)
{
    setupUi();
    applyStyleSheet();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle("航道断面算量自动化平台 v4.2");
    setMinimumSize(1280, 800);
    resize(1280, 800);

    // 主容器
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("MainContainer");
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部导航栏
    mainLayout->addWidget(createHeader());

    // 内容区域
    QWidget *contentWidget = new QWidget(this);
    QHBoxLayout *contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    contentLayout->addWidget(createLeftPanel(), 6);
    contentLayout->addWidget(createRightPanel(), 4);

    mainLayout->addWidget(contentWidget, 1);

    // 状态栏
    m_statusBar = new QStatusBar(this);
    m_statusBar->showMessage("引擎版本: Qt C++ v1.0 | 整合版");
    setStatusBar(m_statusBar);
}

QWidget* MainWindow::createHeader()
{
    QWidget *header = new QWidget(this);
    header->setObjectName("Header");
    header->setFixedHeight(80);

    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(24, 16, 24, 16);

    // 标题
    QLabel *titleLabel = new QLabel("航道断面算量自动化平台 Qt版", header);
    titleLabel->setObjectName("TitleLabel");
    titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; font-style: italic; color: #E4E3E0;");
    layout->addWidget(titleLabel);

    layout->addStretch();

    // 版本信息
    QLabel *versionLabel = new QLabel("v4.2 | 四色厚度分层俯视图版", header);
    versionLabel->setStyleSheet("font-size: 11px; color: #707070; font-family: 'Consolas';");
    layout->addWidget(versionLabel);

    return header;
}

QWidget* MainWindow::createLeftPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setStyleSheet("background-color: #141414;");

    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 标签页
    m_tabWidget = new QTabWidget(panel);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; background-color: #141414; }"
        "QTabBar::tab { background-color: #1A1A1A; color: #707070; padding: 16px 24px; "
        "font-size: 14px; font-weight: 500; border: none; border-bottom: 2px solid transparent; }"
        "QTabBar::tab:selected { color: #E4E3E0; background-color: #141414; border-bottom: 2px solid #E4E3E0; }"
        "QTabBar::tab:hover:!selected { color: #E4E3E0; }"
    );

    QStringList tabs = {
        "autoline", "autopaste", "autohatch", "autosection", "backfill", "autosection_backfill",
        "geology_topview", "excel_migrate"
    };
    QStringList tabNames = {
        "断面合并", "批量粘贴", "快速填充", "分层算量", "回淤计算", "分层+回淤",
        "四色俯视图", "数据迁移"
    };

    for (int i = 0; i < tabs.size(); ++i) {
        QWidget *tabContent = createTabContent(tabs[i]);
        m_tabWidget->addTab(tabContent, tabNames[i]);
    }

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    layout->addWidget(m_tabWidget, 1);

    // 执行按钮区域
    QWidget *buttonContainer = new QWidget(panel);
    buttonContainer->setFixedHeight(100);
    buttonContainer->setStyleSheet(
        "QWidget { background-color: rgba(26, 26, 26, 0.5); border-top: 1px solid #2A2A2A; }"
    );

    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(24, 20, 24, 24);

    m_executeBtn = new QPushButton("执行任务", buttonContainer);
    m_executeBtn->setObjectName("PrimaryButton");
    m_executeBtn->setMinimumHeight(56);
    m_executeBtn->setStyleSheet(
        "QPushButton { background-color: #E4E3E0; color: #0F0F0F; font-size: 18px; "
        "font-weight: bold; padding: 16px 32px; border-radius: 12px; }"
        "QPushButton:hover { background-color: #FFFFFF; }"
        "QPushButton:disabled { background-color: #2A2A2A; color: #555555; }"
    );
    connect(m_executeBtn, &QPushButton::clicked, this, &MainWindow::onExecuteClicked);
    buttonLayout->addWidget(m_executeBtn);

    layout->addWidget(buttonContainer);

    return panel;
}

QWidget* MainWindow::createTabContent(const QString &taskType)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // 滚动区域
    QScrollArea *scrollArea = new QScrollArea(container);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");

    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(24, 24, 24, 24);
    scrollLayout->setSpacing(24);

    // 文件选择区域
    QLabel *fileTitle = new QLabel("文件选择", scrollContent);
    fileTitle->setStyleSheet("font-size: 12px; font-family: 'Consolas'; color: #707070;");
    scrollLayout->addWidget(fileTitle);

    if (taskType == "geology_topview") {
        m_topviewFileRow = new FileRowWidget(
            "断面 DXF", scrollContent, "选择带分层与回淤的断面 DXF",
            "DXF 文件 (*.dxf);;所有文件 (*.*)");
        connect(m_topviewFileRow, &FileRowWidget::fileSelected,
                this, &MainWindow::onTopviewFileSelected);
        connect(m_topviewFileRow, &FileRowWidget::fileCleared,
                this, &MainWindow::onTopviewFileCleared);
        scrollLayout->addWidget(m_topviewFileRow);

        m_topviewSpineFileRow = new FileRowWidget(
            "脊梁点 JSON", scrollContent, "选择脊梁点匹配结果 JSON",
            "JSON 文件 (*.json);;所有文件 (*.*)");
        connect(m_topviewSpineFileRow, &FileRowWidget::fileSelected,
                this, &MainWindow::onTopviewSpineFileSelected);
        connect(m_topviewSpineFileRow, &FileRowWidget::fileCleared,
                this, &MainWindow::onTopviewSpineFileCleared);
        scrollLayout->addWidget(m_topviewSpineFileRow);
    } else if (taskType == "excel_migrate") {
        m_migrateSourceFileRow = new FileRowWidget(
            "算量结果", scrollContent, "选择分层算量结果",
            "Excel 文件 (*.xlsx);;所有文件 (*.*)");
        connect(m_migrateSourceFileRow, &FileRowWidget::fileSelected,
                this, &MainWindow::onMigrateSourceFileSelected);
        connect(m_migrateSourceFileRow, &FileRowWidget::fileCleared,
                this, &MainWindow::onMigrateSourceFileCleared);
        scrollLayout->addWidget(m_migrateSourceFileRow);

        m_migrateTargetFileRow = new FileRowWidget(
            "月进度表", scrollContent, "选择月进度工程量表模板",
            "Excel 文件 (*.xlsx);;所有文件 (*.*)");
        connect(m_migrateTargetFileRow, &FileRowWidget::fileSelected,
                this, &MainWindow::onMigrateTargetFileSelected);
        connect(m_migrateTargetFileRow, &FileRowWidget::fileCleared,
                this, &MainWindow::onMigrateTargetFileCleared);
        scrollLayout->addWidget(m_migrateTargetFileRow);
    } else if (taskType == "autopaste") {
        m_sourceFileRow = new FileRowWidget("源文件", scrollContent);
        connect(m_sourceFileRow, &FileRowWidget::fileSelected, this, &MainWindow::onSourceFileSelected);
        connect(m_sourceFileRow, &FileRowWidget::fileCleared, this, &MainWindow::onSourceFileCleared);
        scrollLayout->addWidget(m_sourceFileRow);

        m_targetFileRow = new FileRowWidget("目标文件", scrollContent);
        connect(m_targetFileRow, &FileRowWidget::fileSelected, this, &MainWindow::onTargetFileSelected);
        connect(m_targetFileRow, &FileRowWidget::fileCleared, this, &MainWindow::onTargetFileCleared);
        scrollLayout->addWidget(m_targetFileRow);
    } else {
        m_fileRow = new FileRowWidget("待处理 DXF", scrollContent);
        connect(m_fileRow, &FileRowWidget::fileSelected, this, &MainWindow::onFileSelected);
        connect(m_fileRow, &FileRowWidget::fileCleared, this, &MainWindow::onFileCleared);
        scrollLayout->addWidget(m_fileRow);
    }

    // 参数设置区域
    QLabel *paramTitle = new QLabel("参数设置", scrollContent);
    paramTitle->setStyleSheet("font-size: 12px; font-family: 'Consolas'; color: #707070;");
    scrollLayout->addWidget(paramTitle);

    QWidget *paramWidget = new QWidget(scrollContent);
    QGridLayout *paramGrid = new QGridLayout(paramWidget);
    paramGrid->setSpacing(12);

    if (taskType == "autoline") {
        createAutolineParams(paramGrid);
    } else if (taskType == "autopaste") {
        createAutopasteParams(paramGrid);
    } else if (taskType == "autohatch") {
        createAutohatchParams(paramGrid);
    } else if (taskType == "autosection") {
        createAutosectionParams(paramGrid);
    } else if (taskType == "backfill") {
        createBackfillParams(paramGrid);
    } else if (taskType == "autosection_backfill") {
        createAutosectionBackfillParams(paramGrid);
    } else if (taskType == "excel_migrate") {
        createExcelMigrationParams(paramGrid);
    } else if (taskType == "geology_topview") {
        createGeologyTopviewParams(paramGrid);
    }

    scrollLayout->addWidget(paramWidget);
    scrollLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    containerLayout->addWidget(scrollArea, 1);

    return container;
}

void MainWindow::createAutolineParams(QGridLayout *layout)
{
    m_paramLayerA = new ParamInputWidget("图层 A 名称", "", false, this);
    m_paramLayerB = new ParamInputWidget("图层 B 名称", "", false, this);

    QVector<ParamSelectWidget::Option> envelopeOptions = {
        {"lower", "下包络线（取最小Y）"},
        {"upper", "上包络线（取最大Y）"}
    };
    m_paramEnvelopeType = new ParamSelectWidget("包络线类型", envelopeOptions, "lower", this);

    // 自定义输出图层名 - 修复了源文件的bug
    m_paramOutputLayer = new ParamInputWidget("输出图层名", Config::DEFAULT_OUTPUT_LAYER, false, this);
    m_paramOutputDir = new ParamInputWidget("输出目录", "", true, this);

    layout->addWidget(m_paramLayerA, 0, 0);
    layout->addWidget(m_paramLayerB, 0, 1);
    layout->addWidget(m_paramEnvelopeType, 1, 0, 1, 2);
    layout->addWidget(m_paramOutputLayer, 2, 0);
    layout->addWidget(m_paramOutputDir, 2, 1);
}

void MainWindow::createAutopasteParams(QGridLayout *layout)
{
    QLabel *hintLabel = new QLabel("自动检测源文件小矩形+断面曲线，目标文件L1脊梁线基点", this);
    hintLabel->setStyleSheet("font-size: 12px; color: #50FA7B; padding: 8px; "
                             "background-color: rgba(80, 250, 123, 0.1); border-radius: 4px;");
    layout->addWidget(hintLabel, 0, 0, 1, 2);

    // 自定义输出图层名
    m_paramPasteLayer = new ParamInputWidget("输出图层名", "0-已粘贴断面", false, this);
    m_paramPasteOutputDir = new ParamInputWidget("输出目录", "", true, this);

    layout->addWidget(m_paramPasteLayer, 1, 0);
    layout->addWidget(m_paramPasteOutputDir, 1, 1);
}

void MainWindow::createAutohatchParams(QGridLayout *layout)
{
    // 自定义填充图层名
    m_paramHatchLayer = new ParamInputWidget("填充图层名", Config::DEFAULT_HATCH_LAYER, false, this);
    m_paramTextHeight = new ParamInputWidget("标注字高", "3.0", false, this);
    m_paramHatchOutputDir = new ParamInputWidget("输出目录", "", true, this);

    layout->addWidget(m_paramHatchLayer, 0, 0);
    layout->addWidget(m_paramTextHeight, 0, 1);
    layout->addWidget(m_paramHatchOutputDir, 1, 0, 1, 2);
}

void MainWindow::createAutosectionParams(QGridLayout *layout)
{
    m_paramElevation = new ParamInputWidget("目标高程 (m)", "", false, this);
    m_paramSectionLayer = new ParamInputWidget("断面线图层", "DMX", false, this);
    m_paramPileLayer = new ParamInputWidget("桩号图层", "0-桩号", false, this);
    m_paramMergeSection = new ParamCheckboxWidget("合并断面线", true, this);
    m_paramAuxLayers = new ParamInputWidget("辅助断面图层", "", false, this);

    QVector<ParamSelectWidget::Option> calcOptions = {
        {"below", "高程线以下"},
        {"above", "高程线以上"}
    };
    m_paramCalcMode = new ParamSelectWidget("计算模式", calcOptions, "below", this);

    m_paramDistinguishDesign = new ParamCheckboxWidget("区分设计/超挖量", false, this);
    m_paramSectionOutputDir = new ParamInputWidget("输出目录", "", true, this);

    layout->addWidget(m_paramElevation, 0, 0);
    layout->addWidget(m_paramSectionLayer, 0, 1);
    layout->addWidget(m_paramPileLayer, 1, 0);
    layout->addWidget(m_paramMergeSection, 1, 1);
    layout->addWidget(m_paramAuxLayers, 2, 0);
    layout->addWidget(m_paramCalcMode, 2, 1);
    layout->addWidget(m_paramDistinguishDesign, 3, 0, 1, 2);
    layout->addWidget(m_paramSectionOutputDir, 4, 0, 1, 2);
}

void MainWindow::createBackfillParams(QGridLayout *layout)
{
    m_paramDesignLayer = new ParamInputWidget("设计断面线图层", "", false, this);
    m_paramBackfillSectionLayer = new ParamInputWidget("断面线图层", "DMX", false, this);
    m_paramBackfillOutputDir = new ParamInputWidget("输出目录", "", true, this);

    layout->addWidget(m_paramDesignLayer, 0, 0);
    layout->addWidget(m_paramBackfillSectionLayer, 0, 1);
    layout->addWidget(m_paramBackfillOutputDir, 1, 0, 1, 2);
}

void MainWindow::createAutosectionBackfillParams(QGridLayout *layout)
{
    m_paramCombinedElevation = new ParamInputWidget("目标高程 (m)", "", false, this);
    m_paramCombinedPileLayer = new ParamInputWidget("桩号图层", "0-桩号", false, this);
    m_paramCombinedDesignLayer = new ParamInputWidget("设计断面线图层 (DMX)", "DMX", false, this);
    m_paramCombinedUpdateLayer = new ParamInputWidget("更新断面线图层", "", false, this);
    m_paramCombinedMergeSection = new ParamCheckboxWidget("合并断面线", false, this);
    m_paramCombinedExtendOverexc = new ParamCheckboxWidget("延长超挖线", false, this);

    QVector<ParamSelectWidget::Option> calcOptions = {
        {"below", "高程线以下"},
        {"above", "高程线以上"}
    };
    m_paramCombinedCalcMode = new ParamSelectWidget("计算模式", calcOptions, "below", this);

    m_paramCombinedDistinguishDesign = new ParamCheckboxWidget("区分设计/超挖量", false, this);
    m_paramCombinedOutputDir = new ParamInputWidget("输出目录", "", true, this);

    layout->addWidget(m_paramCombinedElevation, 0, 0);
    layout->addWidget(m_paramCombinedPileLayer, 0, 1);
    layout->addWidget(m_paramCombinedDesignLayer, 1, 0);
    layout->addWidget(m_paramCombinedUpdateLayer, 1, 1);
    layout->addWidget(m_paramCombinedMergeSection, 2, 0);
    layout->addWidget(m_paramCombinedExtendOverexc, 2, 1);
    layout->addWidget(m_paramCombinedCalcMode, 3, 0);
    layout->addWidget(m_paramCombinedDistinguishDesign, 3, 1);
    layout->addWidget(m_paramCombinedOutputDir, 4, 0, 1, 2);
}

void MainWindow::createExcelMigrationParams(QGridLayout *layout)
{
    QLabel *hintLabel = new QLabel(
        "算量结果面积 × 0.6 写入本期 L/N；原本期 L:O 自动迁至上期 H:K；M/O 写入梯形公式。",
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("font-size: 12px; color: #50FA7B; padding: 8px; "
                             "background-color: rgba(80, 250, 123, 0.1); border-radius: 4px;");
    layout->addWidget(hintLabel, 0, 0, 1, 2);

    m_paramMigrationCoefficient = new ParamInputWidget("面积系数", "0.6", false, this);
    m_paramMigrationOutputDir = new ParamInputWidget("输出目录", "", true, this);
    layout->addWidget(m_paramMigrationCoefficient, 1, 0);
    layout->addWidget(m_paramMigrationOutputDir, 1, 1);
}

void MainWindow::createGeologyTopviewParams(QGridLayout *layout)
{
    QLabel *hintLabel = new QLabel(
        "按平台DMX规则严格归属断面，逐25米断面连接；2.5米精细网格，"
        "不膨胀、不限制面积。填土归入淤泥，碎石归入砂；"
        "仅回淤按最小厚度过滤，并执行全段漏传审计。",
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("font-size: 12px; color: #50FA7B; padding: 8px; "
                             "background-color: rgba(80, 250, 123, 0.1); border-radius: 4px;");
    layout->addWidget(hintLabel, 0, 0, 1, 2);

    m_paramTopviewBackfillThickness =
        new ParamInputWidget("回淤最小厚度 (m)", "0.20", false, this);
    m_paramTopviewOutputDir =
        new ParamInputWidget("输出目录", "", true, this);
    layout->addWidget(m_paramTopviewBackfillThickness, 1, 0);
    layout->addWidget(m_paramTopviewOutputDir, 1, 1);
}

QWidget* MainWindow::createRightPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setStyleSheet("background-color: #0D0D0D;");
    panel->setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 成果输出区域
    QWidget *resultHeader = new QWidget(panel);
    resultHeader->setFixedHeight(48);
    resultHeader->setStyleSheet("background-color: #1A1A1A; border-bottom: 1px solid #2A2A2A;");

    QHBoxLayout *resultHeaderLayout = new QHBoxLayout(resultHeader);
    resultHeaderLayout->setContentsMargins(16, 0, 16, 0);

    QLabel *resultTitle = new QLabel("成果输出", resultHeader);
    resultTitle->setStyleSheet("font-size: 12px; font-family: 'Consolas'; color: #808080;");
    resultHeaderLayout->addWidget(resultTitle);
    layout->addWidget(resultHeader);

    m_resultArea = new QTextEdit(panel);
    m_resultArea->setReadOnly(true);
    m_resultArea->setPlaceholderText("等待任务完成...");
    m_resultArea->setFixedHeight(250);
    m_resultArea->setStyleSheet(
        "QTextEdit { background-color: rgba(0, 0, 0, 0.3); border: none; padding: 16px; "
        "font-size: 14px; color: #E4E3E0; }"
    );
    layout->addWidget(m_resultArea);

    // 分隔线
    QFrame *separator = new QFrame(panel);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background-color: #2A2A2A; border: none; max-height: 1px;");
    layout->addWidget(separator);

    // 日志区域
    QWidget *logHeader = new QWidget(panel);
    logHeader->setFixedHeight(48);
    logHeader->setStyleSheet("background-color: #1A1A1A; border-bottom: 1px solid #2A2A2A;");

    QHBoxLayout *logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(16, 0, 16, 0);

    QLabel *logTitle = new QLabel("运行控制台", logHeader);
    logTitle->setStyleSheet("font-size: 12px; font-family: 'Consolas'; color: #808080;");
    logHeaderLayout->addWidget(logTitle);

    QPushButton *clearBtn = new QPushButton("清除", logHeader);
    clearBtn->setStyleSheet("font-size: 11px; color: #707070; background: transparent; border: none;");
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearLog);
    logHeaderLayout->addWidget(clearBtn);
    layout->addWidget(logHeader);

    m_logArea = new QTextEdit(panel);
    m_logArea->setReadOnly(true);
    m_logArea->setStyleSheet(
        "QTextEdit { background-color: rgba(0, 0, 0, 0.5); border: none; padding: 16px; "
        "font-family: 'Consolas'; font-size: 13px; color: #E4E3E0; }"
    );
    layout->addWidget(m_logArea, 1);

    return panel;
}

void MainWindow::applyStyleSheet()
{
    // 完整深色主题样式表 - 复刻Python platform_ui_v3.py STYLESHEET
    QString stylesheet =
        // 全局样式
        "QMainWindow, QWidget {"
        "   background-color: #0F0F0F;"
        "   color: #E4E3E0;"
        "   font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "}"
        ""
        // 主容器
        "#MainContainer {"
        "   background-color: #141414;"
        "   border: 1px solid #2A2A2A;"
        "   border-radius: 16px;"
        "}"
        ""
        // 顶部导航栏
        "#Header {"
        "   background-color: #1A1A1A;"
        "   border-bottom: 1px solid #2A2A2A;"
        "   border-top-left-radius: 16px;"
        "   border-top-right-radius: 16px;"
        "}"
        ""
        // 标题
        "#TitleLabel {"
        "   font-size: 22px;"
        "   font-weight: bold;"
        "   font-style: italic;"
        "}"
        ""
        // 标签页
        "QTabWidget::pane {"
        "   border: none;"
        "   background-color: #141414;"
        "}"
        ""
        "QTabBar::tab {"
        "   background-color: #1A1A1A;"
        "   color: #707070;"
        "   padding: 16px 24px;"
        "   font-size: 14px;"
        "   font-weight: 500;"
        "   border: none;"
        "   border-bottom: 2px solid transparent;"
        "}"
        ""
        "QTabBar::tab:selected {"
        "   color: #E4E3E0;"
        "   background-color: #141414;"
        "   border-bottom: 2px solid #E4E3E0;"
        "}"
        ""
        "QTabBar::tab:hover:!selected {"
        "   color: #E4E3E0;"
        "}"
        ""
        // 输入框
        "QLineEdit {"
        "   background-color: #1A1A1A;"
        "   border: 1px solid #2A2A2A;"
        "   border-radius: 8px;"
        "   padding: 12px 16px;"
        "   font-size: 14px;"
        "   font-family: 'Consolas', 'Microsoft YaHei UI';"
        "   color: #E4E3E0;"
        "   selection-background-color: #E4E3E0;"
        "   selection-color: #0F0F0F;"
        "}"
        ""
        "QLineEdit:focus {"
        "   border: 1px solid #E4E3E0;"
        "}"
        ""
        "QLineEdit:disabled {"
        "   background-color: #0D0D0D;"
        "   color: #555555;"
        "}"
        ""
        // 下拉框
        "QComboBox {"
        "   background-color: #1A1A1A;"
        "   border: 1px solid #2A2A2A;"
        "   border-radius: 8px;"
        "   padding: 12px 16px;"
        "   font-size: 14px;"
        "   color: #E4E3E0;"
        "}"
        ""
        "QComboBox:focus {"
        "   border: 1px solid #E4E3E0;"
        "}"
        ""
        "QComboBox::drop-down {"
        "   border: none;"
        "   width: 30px;"
        "}"
        ""
        "QComboBox::down-arrow {"
        "   image: none;"
        "   border-left: 5px solid transparent;"
        "   border-right: 5px solid transparent;"
        "   border-top: 6px solid #E4E3E0;"
        "   margin-right: 10px;"
        "}"
        ""
        "QComboBox QAbstractItemView {"
        "   background-color: #1A1A1A;"
        "   border: 1px solid #2A2A2A;"
        "   selection-background-color: #2A2A2A;"
        "   color: #E4E3E0;"
        "}"
        ""
        // 按钮
        "QPushButton {"
        "   background-color: #2A2A2A;"
        "   color: #E4E3E0;"
        "   border: none;"
        "   border-radius: 8px;"
        "   padding: 10px 20px;"
        "   font-size: 13px;"
        "   font-weight: 500;"
        "}"
        ""
        "QPushButton:hover {"
        "   background-color: #333333;"
        "}"
        ""
        "QPushButton:pressed {"
        "   background-color: #1A1A1A;"
        "}"
        ""
        "QPushButton:disabled {"
        "   background-color: #2A2A2A;"
        "   color: #555555;"
        "}"
        ""
        // 主按钮
        "#PrimaryButton {"
        "   background-color: #E4E3E0;"
        "   color: #0F0F0F;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "   padding: 16px 32px;"
        "   border-radius: 12px;"
        "}"
        ""
        "#PrimaryButton:hover {"
        "   background-color: #FFFFFF;"
        "}"
        ""
        "#PrimaryButton:disabled {"
        "   background-color: #2A2A2A;"
        "   color: #555555;"
        "}"
        ""
        // 文件行
        "#FileRow {"
        "   background-color: #1A1A1A;"
        "   border: 1px solid #2A2A2A;"
        "   border-radius: 8px;"
        "}"
        ""
        "#FileRow:hover {"
        "   border-color: #333333;"
        "}"
        ""
        // 日志区域
        "QTextEdit {"
        "   background-color: rgba(0, 0, 0, 0.4);"
        "   border: none;"
        "   border-radius: 8px;"
        "   padding: 8px;"
        "   font-family: 'Consolas', 'Microsoft YaHei UI';"
        "   font-size: 13px;"
        "   color: #E4E3E0;"
        "}"
        ""
        // 复选框
        "QCheckBox {"
        "   spacing: 8px;"
        "   font-size: 14px;"
        "}"
        ""
        "QCheckBox::indicator {"
        "   width: 20px;"
        "   height: 20px;"
        "   border-radius: 4px;"
        "   border: 2px solid #333333;"
        "   background-color: #0F0F0F;"
        "}"
        ""
        "QCheckBox::indicator:checked {"
        "   background-color: #E4E3E0;"
        "   border-color: #E4E3E0;"
        "}"
        ""
        "QCheckBox::indicator:hover {"
        "   border-color: #E4E3E0;"
        "}"
        ""
        // 滚动条
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 6px;"
        "   border-radius: 3px;"
        "}"
        ""
        "QScrollBar::handle:vertical {"
        "   background: #2A2A2A;"
        "   border-radius: 3px;"
        "   min-height: 20px;"
        "}"
        ""
        "QScrollBar::handle:vertical:hover {"
        "   background: #333333;"
        "}"
        ""
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        ""
        // 状态栏
        "QStatusBar {"
        "   background-color: #1A1A1A;"
        "   border-top: 1px solid #2A2A2A;"
        "   color: #707070;"
        "   font-size: 11px;"
        "   font-family: 'Consolas';"
        "}"
        ""
        // 进度条
        "QProgressBar {"
        "   background-color: #2A2A2A;"
        "   border: none;"
        "   border-radius: 4px;"
        "   height: 8px;"
        "   text-align: center;"
        "}"
        ""
        "QProgressBar::chunk {"
        "   background-color: #E4E3E0;"
        "   border-radius: 4px;"
        "}"
        ""
        // 滚动区域
        "QScrollArea {"
        "   background-color: transparent;"
        "   border: none;"
        "}"
        ""
        "QScrollArea > QWidget > QWidget {"
        "   background-color: transparent;"
        "}"
        ""
        // 分隔线
        "QFrame[frameShape=4] {"
        "   background-color: #2A2A2A;"
        "   border: none;"
        "   max-height: 1px;"
        "}"
        "";

    setStyleSheet(stylesheet);
}

void MainWindow::onTabChanged(int index)
{
    QStringList tasks = {"autoline", "autopaste", "autohatch", "autosection", "backfill",
                         "autosection_backfill", "geology_topview", "excel_migrate"};
    if (index >= 0 && index < tasks.size()) {
        m_currentTask = tasks[index];
    }
}

void MainWindow::onExecuteClicked()
{
    if (m_executing) {
        return;
    }

    // 检查文件选择
    if (m_currentTask == "geology_topview") {
        if (!m_topviewFileRow->hasFile() || !m_topviewSpineFileRow->hasFile()) {
            QMessageBox::warning(this, "警告", "请选择断面 DXF 和脊梁点匹配 JSON");
            return;
        }
    } else if (m_currentTask == "excel_migrate") {
        if (!m_migrateSourceFileRow->hasFile() || !m_migrateTargetFileRow->hasFile()) {
            QMessageBox::warning(this, "警告", "请选择算量结果和月进度表模板");
            return;
        }
    } else if (m_currentTask == "autopaste") {
        if (!m_sourceFileRow->hasFile() || !m_targetFileRow->hasFile()) {
            QMessageBox::warning(this, "警告", "请选择源文件和目标文件");
            return;
        }
    } else {
        if (!m_fileRow->hasFile()) {
            QMessageBox::warning(this, "警告", "请选择要处理的 DXF 文件");
            return;
        }
    }

    // Commit any active Chinese IME composition before reading parameters.
    // Otherwise clicking "执行任务" immediately after typing a layer name can
    // occasionally submit the previous default value.
    if (QApplication::inputMethod())
        QApplication::inputMethod()->commit();

    m_executing = true;
    m_executeBtn->setText("计算中...");
    m_executeBtn->setEnabled(false);

    // 收集参数
    QMap<QString, QString> params = collectParams();

    // 创建任务线程
    m_worker = new TaskWorker(m_currentTask, params, this);
    connect(m_worker, &TaskWorker::logMessage, this, &MainWindow::onLogMessage);
    connect(m_worker, &TaskWorker::taskResult, this, &MainWindow::onTaskResult);
    connect(m_worker, &TaskWorker::finished, this, &MainWindow::onTaskFinished);
    m_worker->start();
}

QMap<QString, QString> MainWindow::collectParams()
{
    QMap<QString, QString> params;

    if (m_currentTask == "autoline") {
        params["图层A名称"] = m_paramLayerA->getValue();
        params["图层B名称"] = m_paramLayerB->getValue();
        params["包络线类型"] = m_paramEnvelopeType->getValue();
        params["输出图层名"] = m_paramOutputLayer->getValue();  // 自定义图层名
        params["输出目录"] = m_paramOutputDir->getValue();
        params["files"] = m_selectedFile["path"].toString();
    } else if (m_currentTask == "autopaste") {
        params["源文件名"] = m_sourceFile["path"].toString();
        params["目标文件名"] = m_targetFile["path"].toString();
        params["输出图层名"] = m_paramPasteLayer->getValue();  // 自定义图层名
        params["输出目录"] = m_paramPasteOutputDir->getValue();
    } else if (m_currentTask == "autohatch") {
        params["填充层名称"] = m_paramHatchLayer->getValue();  // 自定义图层名
        params["标注字高"] = m_paramTextHeight->getValue();
        params["输出目录"] = m_paramHatchOutputDir->getValue();
        params["files"] = m_selectedFile["path"].toString();
    } else if (m_currentTask == "autosection") {
        params["目标高程"] = m_paramElevation->getValue();
        params["断面线图层"] = m_paramSectionLayer->getValue();
        params["桩号图层"] = m_paramPileLayer->getValue();
        params["合并断面线"] = m_paramMergeSection->isChecked() ? "true" : "false";
        params["辅助断面图层"] = m_paramAuxLayers->getValue();
        params["计算模式"] = m_paramCalcMode->getValue();
        params["区分设计超挖"] = m_paramDistinguishDesign->isChecked() ? "true" : "false";
        params["输出目录"] = m_paramSectionOutputDir->getValue();
        params["files"] = m_selectedFile["path"].toString();
    } else if (m_currentTask == "backfill") {
        params["断面线图层"] = m_paramBackfillSectionLayer->getValue();
        params["设计断面线图层"] = m_paramDesignLayer->getValue();
        params["输出目录"] = m_paramBackfillOutputDir->getValue();
        params["files"] = m_selectedFile["path"].toString();
    } else if (m_currentTask == "autosection_backfill") {
        params["目标高程"] = m_paramCombinedElevation->getValue();
        params["桩号图层"] = m_paramCombinedPileLayer->getValue();
        params["设计断面线图层"] = m_paramCombinedDesignLayer->getValue();
        params["更新断面线图层"] = m_paramCombinedUpdateLayer->getValue();
        params["合并断面线"] = m_paramCombinedMergeSection->isChecked() ? "true" : "false";
        params["延长超挖线"] = m_paramCombinedExtendOverexc->isChecked() ? "true" : "false";
        params["计算模式"] = m_paramCombinedCalcMode->getValue();
        params["区分设计超挖"] = m_paramCombinedDistinguishDesign->isChecked() ? "true" : "false";
        params["输出目录"] = m_paramCombinedOutputDir->getValue();
        params["files"] = m_selectedFile["path"].toString();
    } else if (m_currentTask == "excel_migrate") {
        params["源文件名"] = m_migrateSourceFile["path"].toString();
        params["目标文件名"] = m_migrateTargetFile["path"].toString();
        params["面积系数"] = m_paramMigrationCoefficient->getValue();
        params["输出目录"] = m_paramMigrationOutputDir->getValue();
    } else if (m_currentTask == "geology_topview") {
        params["files"] = m_topviewFile["path"].toString();
        params["脊梁点JSON"] = m_topviewSpineFile["path"].toString();
        params["回淤最小厚度"] = m_paramTopviewBackfillThickness->getValue();
        params["输出目录"] = m_paramTopviewOutputDir->getValue();
    }

    return params;
}

void MainWindow::onLogMessage(const QString &message, const QString &level)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");

    QString color;
    if (level == "success") {
        color = "#50FA7B";
    } else if (level == "error") {
        color = "#FF5555";
    } else if (level == "warning") {
        color = "#FFFF00";
    } else {
        color = "#E4E3E0";
    }

    QString html = QString("<span style='color: #707070;'>[%1]</span> <span style='color: %2;'>%3</span>")
                   .arg(timestamp, color, message);

    m_logArea->append(html);
}

void MainWindow::onTaskResult(const QJsonObject &result)
{
    if (result["success"].toBool()) {
        m_resultArea->clear();
        QString outputPath = result["outputPath"].toString();
        if (!outputPath.isEmpty()) {
            m_resultArea->append(QString("输出文件:\n%1").arg(outputPath));
        }
        if (result.contains("matchedSheets")) {
            m_resultArea->append(QString("\n匹配地层: %1\n匹配桩号: %2\n公式单元格: %3")
                                     .arg(result["matchedSheets"].toInt())
                                     .arg(result["matchedStations"].toInt())
                                     .arg(result["formulaCells"].toInt()));
        }
        if (result.contains("mudCount")) {
            m_resultArea->append(
                QString("\n淤泥（含填土）: %1 块 / %2 m²"
                        "\n黏土: %3 块 / %4 m²"
                        "\n砂（含碎石）: %5 块 / %6 m²"
                        "\n回淤: %7 块 / %8 m²"
                        "\n回淤最小厚度: %9 m"
                        "\n严格显示断面: 淤泥 %10 / 黏土 %11 / 砂 %12 / 回淤 %13"
                        "\n25米桩号线: %14 条 / 双侧标注 %15 个 / 统一长度 %16 m"
                        "\n传导审计: 上游漏传 %17 / 俯视图漏传 %18"
                        "\n源回淤为空 %19 个断面 / 厚度筛除 %20 个断面"
                        "\n诊断文件: %21")
                    .arg(result["mudCount"].toInt())
                    .arg(result["mudArea"].toDouble(), 0, 'f', 2)
                    .arg(result["clayCount"].toInt())
                    .arg(result["clayArea"].toDouble(), 0, 'f', 2)
                    .arg(result["sandCount"].toInt())
                    .arg(result["sandArea"].toDouble(), 0, 'f', 2)
                    .arg(result["backfillCount"].toInt())
                    .arg(result["backfillArea"].toDouble(), 0, 'f', 2)
                    .arg(result["minBackfillThickness"].toDouble(), 0, 'f', 3)
                    .arg(result["mudVisibleStations"].toInt())
                    .arg(result["clayVisibleStations"].toInt())
                    .arg(result["sandVisibleStations"].toInt())
                    .arg(result["backfillVisibleStations"].toInt())
                    .arg(result["stationLineCount"].toInt())
                    .arg(result["stationTextCount"].toInt())
                    .arg(result["stationLineLength"].toDouble(), 0, 'f', 1)
                    .arg(result["upstreamLineToHatchLossCount"].toInt())
                    .arg(result["transmissionLossCount"].toInt())
                    .arg(result["sourceHatchEmptySectionCount"].toInt())
                    .arg(result["thresholdFilteredSectionCount"].toInt())
                    .arg(result["debugPath"].toString()));
        }
    } else {
        QString error = result["error"].toString();
        addLog(QString("错误: %1").arg(error), "error");
    }
}

void MainWindow::onTaskFinished()
{
    m_executing = false;
    m_executeBtn->setText("执行任务");
    m_executeBtn->setEnabled(true);

    if (m_worker) {
        m_worker->deleteLater();
        m_worker = nullptr;
    }
}

void MainWindow::onFileSelected(const QJsonObject &fileInfo)
{
    m_selectedFile = fileInfo;
    addLog(QString("已选择文件: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onFileCleared()
{
    m_selectedFile = QJsonObject();
}

void MainWindow::onSourceFileSelected(const QJsonObject &fileInfo)
{
    m_sourceFile = fileInfo;
    addLog(QString("已选择源文件: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onSourceFileCleared()
{
    m_sourceFile = QJsonObject();
}

void MainWindow::onTargetFileSelected(const QJsonObject &fileInfo)
{
    m_targetFile = fileInfo;
    addLog(QString("已选择目标文件: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onTargetFileCleared()
{
    m_targetFile = QJsonObject();
}

void MainWindow::onMigrateSourceFileSelected(const QJsonObject &fileInfo)
{
    m_migrateSourceFile = fileInfo;
    addLog(QString("已选择算量结果: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onMigrateSourceFileCleared()
{
    m_migrateSourceFile = QJsonObject();
}

void MainWindow::onMigrateTargetFileSelected(const QJsonObject &fileInfo)
{
    m_migrateTargetFile = fileInfo;
    addLog(QString("已选择月进度表: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onMigrateTargetFileCleared()
{
    m_migrateTargetFile = QJsonObject();
}

void MainWindow::onTopviewFileSelected(const QJsonObject &fileInfo)
{
    m_topviewFile = fileInfo;
    addLog(QString("已选择俯视图断面文件: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onTopviewFileCleared()
{
    m_topviewFile = QJsonObject();
}

void MainWindow::onTopviewSpineFileSelected(const QJsonObject &fileInfo)
{
    m_topviewSpineFile = fileInfo;
    addLog(QString("已选择脊梁点匹配文件: %1").arg(fileInfo["name"].toString()), "info");
}

void MainWindow::onTopviewSpineFileCleared()
{
    m_topviewSpineFile = QJsonObject();
}

void MainWindow::addLog(const QString &message, const QString &level)
{
    onLogMessage(message, level);
}

void MainWindow::clearLog()
{
    m_logArea->clear();
}
