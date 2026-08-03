#include "PythonBridge.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSaveFile>
#include <QJsonDocument>
#include <QFile>

static const QString kInfo  = QStringLiteral("info");
static const QString kError = QStringLiteral("error");
static const QString kWarn  = QStringLiteral("warning");

static QString extractEmbeddedScript(const QString &resourcePath, const QString &fileName) {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList outputDirs = {
        QDir(appDir).filePath(QStringLiteral("scripts")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("HydraulicCADPlatform/scripts"))
    };

    QFile source(resourcePath);
    if (!source.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray content = source.readAll();

    for (const QString &outputDir : outputDirs) {
        if (!QDir().mkpath(outputDir))
            continue;
        const QString outputPath = QDir(outputDir).filePath(fileName);
        QSaveFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly))
            continue;
        if (output.write(content) == content.size() && output.commit())
            return outputPath;
    }
    return QString();
}

static QString locatePythonExecutable() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("runtime/python/python.exe")),
        QDir(appDir).filePath(QStringLiteral("python/python.exe"))
    };
    for (const QString &candidate : candidates) {
        const QString cleanPath = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanPath))
            return cleanPath;
    }
    return QStringLiteral("python");
}

static QString locateAutosectionScript() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("scripts/autosection_compute.py")),
        QDir(appDir).filePath(QStringLiteral("../scripts/autosection_compute.py"))
    };
    for (const QString &candidate : candidates) {
        const QString cleanPath = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleanPath))
            return cleanPath;
    }
    return extractEmbeddedScript(
        QStringLiteral(":/scripts/autosection_compute.py"),
        QStringLiteral("autosection_compute.py"));
}

QJsonArray PythonBridge::pointToJson(const Point2D &pt) {
    QJsonArray a; a.append(pt.x); a.append(pt.y); return a;
}

QJsonArray PythonBridge::pointsToJsonArray(const QVector<Point2D> &pts) {
    QJsonArray a;
    for (const Point2D &pt : pts) a.append(pointToJson(pt));
    return a;
}

QJsonObject PythonBridge::lineToJson(const Line2D &line) {
    QJsonObject o;
    o[QStringLiteral("points")] = pointsToJsonArray(line.points);
    return o;
}

PythonBridge::Result PythonBridge::run(const QString &inputPath, const QString &outputDxfPath,
                                        const QString &outputXlsxPath, const QJsonObject &jsonData,
                                        LogCallback log)
{
    Result result;

    QString jsonPath = outputDxfPath + ".json";
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        log(QStringLiteral("[ERROR] 无法创建JSON文件"), kError);
        return result;
    }
    jsonFile.write(QJsonDocument(jsonData).toJson());
    jsonFile.close();

    QString dumpPath = qEnvironmentVariable("QT_CAD_DUMP_JSON");
    if (!dumpPath.isEmpty()) {
        QFile::remove(dumpPath);
        if (!QFile::copy(jsonPath, dumpPath))
            log(QStringLiteral("[WARN] 无法写入JSON调试副本: %1").arg(dumpPath), kWarn);
    }

    const QString scriptPath = locateAutosectionScript();
    if (scriptPath.isEmpty()) {
        log(QStringLiteral("[ERROR] 未找到 scripts/autosection_compute.py"), kError);
        QFile::remove(jsonPath);
        return result;
    }

    log(QStringLiteral("[INFO] 调用Python计算: %1").arg(scriptPath), kInfo);

    const QString pythonExecutable = locatePythonExecutable();
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    environment.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    if (QFileInfo(pythonExecutable).isAbsolute()) {
        environment.insert(QStringLiteral("PYTHONHOME"), QFileInfo(pythonExecutable).absolutePath());
        environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
        log(QStringLiteral("[INFO] 使用内置 Python: %1").arg(pythonExecutable), kInfo);
    }
    process.setProcessEnvironment(environment);
    process.start(pythonExecutable, {scriptPath, jsonPath});
    if (!process.waitForStarted(10000)) {
        log(QStringLiteral("[ERROR] 无法启动Python: %1").arg(process.errorString()), kError);
        QFile::remove(jsonPath);
        return result;
    }
    if (!process.waitForFinished(900000)) {
        process.kill();
        process.waitForFinished();
        log(QStringLiteral("[ERROR] Python计算超过15分钟"), kError);
        QFile::remove(jsonPath);
        return result;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QString error = QString::fromUtf8(process.readAllStandardError());

    bool foundResult = false;
    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith(QStringLiteral("__RESULT__:"))) {
            QJsonDocument rd = QJsonDocument::fromJson(line.mid(11).trimmed().toUtf8());
            if (!rd.isNull() && rd.isObject()) {
                QJsonObject obj = rd.object();
                if (!obj.contains(QStringLiteral("totalArea"))) {
                    log(QStringLiteral("[ERROR] Python结果缺少totalArea"), kError);
                    continue;
                }
                result.totalArea = obj[QStringLiteral("totalArea")].toDouble();
                result.backfillArea = obj[QStringLiteral("backfillArea")].toDouble();
                foundResult = true;
            } else {
                log(QStringLiteral("[ERROR] Python结果JSON解析失败"), kError);
            }
        } else {
            log(line.trimmed(), kInfo);
        }
    }
    if (!error.isEmpty()) {
        for (const QString &line : error.split('\n', Qt::SkipEmptyParts))
            log(QStringLiteral("[PY] %1").arg(line.trimmed()), kWarn);
    }

    QFile::remove(jsonPath);
    if (process.exitCode() != 0) {
        log(QStringLiteral("[ERROR] Python计算失败"), kError);
        return result;
    }

    if (!foundResult) {
        log(QStringLiteral("[ERROR] Python未返回面积统计结果"), kError);
        return result;
    }

    result.success = true;
    return result;
}
