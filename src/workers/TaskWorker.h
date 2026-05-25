#ifndef TASK_WORKER_H
#define TASK_WORKER_H

#include <QThread>
#include <QString>
#include <QJsonObject>
#include <QMap>
#include <functional>

/**
 * 后台任务执行线程
 * 
 * 调用EngineCad执行六大任务
 */
class TaskWorker : public QThread
{
    Q_OBJECT

public:
    typedef std::function<void(const QString&, const QString&)> LogCallback;

    explicit TaskWorker(const QString &taskType, const QMap<QString, QString> &params, QObject *parent = nullptr);

    void run() override;

signals:
    void logMessage(const QString &message, const QString &level);
    void taskResult(const QJsonObject &result);

private:
    QString m_taskType;
    QMap<QString, QString> m_params;
};

#endif // TASK_WORKER_H