#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QJsonObject>
#include <QMap>

#include "widgets/FileRowWidget.h"
#include "widgets/ParamInputWidget.h"
#include "widgets/ParamSelectWidget.h"
#include "widgets/ParamCheckboxWidget.h"
#include "workers/TaskWorker.h"

/**
 * 主窗口类 - 航道断面算量自动化平台 Qt C++版本
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onTabChanged(int index);
    void onExecuteClicked();
    void onLogMessage(const QString &message, const QString &level);
    void onTaskResult(const QJsonObject &result);
    void onTaskFinished();

    // 文件选择回调
    void onFileSelected(const QJsonObject &fileInfo);
    void onFileCleared();
    void onSourceFileSelected(const QJsonObject &fileInfo);
    void onSourceFileCleared();
    void onTargetFileSelected(const QJsonObject &fileInfo);
    void onTargetFileCleared();

private:
    void setupUi();
    void applyStyleSheet();

    // 创建UI组件
    QWidget* createHeader();
    QWidget* createLeftPanel();
    QWidget* createRightPanel();
    QWidget* createTabContent(const QString &taskType);

    // 创建各任务的参数面板
    void createAutolineParams(QGridLayout *layout);
    void createAutopasteParams(QGridLayout *layout);
    void createAutohatchParams(QGridLayout *layout);
    void createAutosectionParams(QGridLayout *layout);
    void createBackfillParams(QGridLayout *layout);
    void createAutosectionBackfillParams(QGridLayout *layout);

    // 收集参数
    QMap<QString, QString> collectParams();

    // 日志和状态
    void addLog(const QString &message, const QString &level = "info");
    void clearLog();

    // UI组件
    QTabWidget *m_tabWidget;
    QTextEdit *m_logArea;
    QTextEdit *m_resultArea;
    QPushButton *m_executeBtn;
    QStatusBar *m_statusBar;

    // 当前任务
    QString m_currentTask;

    // 文件信息
    QJsonObject m_selectedFile;
    QJsonObject m_sourceFile;
    QJsonObject m_targetFile;

    // 执行状态
    bool m_executing;
    TaskWorker *m_worker;

    // 参数组件指针（各任务）
    // autoline
    ParamInputWidget *m_paramLayerA;
    ParamInputWidget *m_paramLayerB;
    ParamSelectWidget *m_paramEnvelopeType;
    ParamInputWidget *m_paramOutputLayer;
    ParamInputWidget *m_paramOutputDir;

    // autopaste
    ParamInputWidget *m_paramPasteLayer;
    ParamInputWidget *m_paramPasteOutputDir;

    // autohatch
    ParamInputWidget *m_paramHatchLayer;
    ParamInputWidget *m_paramTextHeight;
    ParamInputWidget *m_paramHatchOutputDir;

    // autosection
    ParamInputWidget *m_paramElevation;
    ParamInputWidget *m_paramSectionLayer;
    ParamInputWidget *m_paramPileLayer;
    ParamCheckboxWidget *m_paramMergeSection;
    ParamInputWidget *m_paramAuxLayers;
    ParamSelectWidget *m_paramCalcMode;
    ParamCheckboxWidget *m_paramDistinguishDesign;
    ParamInputWidget *m_paramSectionOutputDir;

    // backfill
    ParamInputWidget *m_paramDesignLayer;
    ParamInputWidget *m_paramBackfillSectionLayer;
    ParamInputWidget *m_paramBackfillOutputDir;

    // autosection_backfill
    ParamInputWidget *m_paramCombinedElevation;
    ParamInputWidget *m_paramCombinedPileLayer;
    ParamInputWidget *m_paramCombinedDesignLayer;
    ParamInputWidget *m_paramCombinedUpdateLayer;
    ParamCheckboxWidget *m_paramCombinedMergeSection;
    ParamSelectWidget *m_paramCombinedCalcMode;
    ParamCheckboxWidget *m_paramCombinedDistinguishDesign;
    ParamInputWidget *m_paramCombinedOutputDir;

    // 文件选择组件
    FileRowWidget *m_fileRow;
    FileRowWidget *m_sourceFileRow;
    FileRowWidget *m_targetFileRow;
};

#endif // MAINWINDOW_H