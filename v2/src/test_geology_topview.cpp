#include "EngineCad.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() < 5) {
        std::fprintf(
            stderr,
            "Usage: TestGeologyTopview <input.dxf> <spine.json> "
            "<min-backfill-thickness-m> <output-dir>\n");
        return 2;
    }

    QMap<QString, QString> params;
    params[QStringLiteral("files")] = arguments[1];
    params[QStringLiteral("脊梁点JSON")] = arguments[2];
    params[QStringLiteral("回淤最小厚度")] = arguments[3];
    params[QStringLiteral("输出目录")] = arguments[4];

    EngineCad engine;
    QJsonObject result;
    const auto logger = [](const QString &message, const QString &level) {
        const QByteArray line =
            QStringLiteral("[%1] %2\n").arg(level, message).toUtf8();
        std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
        std::fflush(stderr);
    };

    const bool success = engine.runGeologyTopview(params, logger, result);
    const QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
    std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
    std::fputc('\n', stdout);
    return success ? 0 : 1;
}
