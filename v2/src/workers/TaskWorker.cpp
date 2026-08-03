#include "TaskWorker.h"
#include "EngineCad.h"
#include <QDateTime>

TaskWorker::TaskWorker(const QString &taskType, const QMap<QString, QString> &params, QObject *parent)
    : QThread(parent)
    , m_taskType(taskType)
    , m_params(params)
{
}

void TaskWorker::run()
{
    // 创建日志回调函数
    LogCallback logFunc = [this](const QString &msg, const QString &level) {
        emit logMessage(msg, level);
    };

    emit logMessage(QString("开始执行任务: %1").arg(m_taskType), "info");

    // 根据任务类型执行对应函数
    EngineCad engine;

    QJsonObject result;
    bool success = false;

    if (m_taskType == "autoline") {
        success = engine.runAutoline(m_params, logFunc, result);
    } else if (m_taskType == "autopaste") {
        success = engine.runAutopaste(m_params, logFunc, result);
    } else if (m_taskType == "autohatch") {
        success = engine.runAutohatch(m_params, logFunc, result);
    } else if (m_taskType == "autosection") {
        success = engine.runAutosection(m_params, logFunc, result);
    } else if (m_taskType == "backfill") {
        success = engine.runBackfill(m_params, logFunc, result);
    } else if (m_taskType == "autosection_backfill") {
        success = engine.runAutosectionBackfill(m_params, logFunc, result);
    } else if (m_taskType == "excel_migrate") {
        success = engine.runExcelMigration(m_params, logFunc, result);
    } else if (m_taskType == "geology_topview") {
        success = engine.runGeologyTopview(m_params, logFunc, result);
    } else {
        emit logMessage(QString("未知任务类型: %1").arg(m_taskType), "error");
        result["success"] = false;
        result["error"] = "未知任务类型";
        emit taskResult(result);
        return;
    }

    result["success"] = success;
    emit taskResult(result);

    if (success) {
        emit logMessage("任务执行完成", "success");
    } else {
        emit logMessage("任务执行失败", "error");
    }
}
