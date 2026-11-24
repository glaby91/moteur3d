#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSize>
#include <QStringList>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace m3d;

namespace {

struct BenchmarkOptions {
    QString scenePath;
    QString outputRoot;
    bool quick = false;
    bool cpuOnly = false;
};

struct PreparedScene {
    v16::BuildResult build;
    ShadowMap shadows;
    double buildSceneMs = 0.0;
    double buildShadowMs = 0.0;
};

QString csvField(QString value) {
    value.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

bool writeJson(const QString& path, const QJsonObject& value) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QByteArray json = QJsonDocument(value).toJson(QJsonDocument::Indented);
    qsizetype offset = 0;
    while (offset < json.size()) {
        const qint64 remaining = static_cast<qint64>(json.size() - offset);
        const qint64 written = file.write(json.constData() + offset, remaining);
        if (written <= 0) {
            return false;
        }
        offset += static_cast<qsizetype>(written);
    }

    return file.flush();
}

bool writePng(
    const QString& path,
    const std::vector<uint32_t>& pixels,
    int width,
    int height) {
    const size_t expectedPixels = size_t(width) * size_t(height);
    if (pixels.size() != expectedPixels) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        const size_t rowOffset = size_t(y) * size_t(width);
        std::memcpy(image.scanLine(y), pixels.data() + rowOffset, size_t(width) * 4);
    }

    return image.save(path, "PNG");
}

QJsonObject statsToJson(const Stats& stats) {
    return {
        {"rays", double(stats.rays)},
        {"triangleTests", double(stats.tests)},
        {"aabb", double(stats.aabb)},
        {"bvhNodes", double(stats.nodes)},
        {"hits", double(stats.hits)},
        {"sky", double(stats.sky)},
        {"texels", double(stats.texels)},
        {"shadowRays", double(stats.shadowRays)},
        {"engineMs", stats.ms},
        {"postMs", stats.postMs}
    };
}

QString makeCaseId(
    const QString& backend,
    const QString& camera,
    int width,
    int height,
    int threads,
    const QString& mode) {
    return QStringLiteral("%1_%2_%3x%4_t%5_%6")
        .arg(backend, camera)
        .arg(width)
        .arg(height)
        .arg(threads)
        .arg(mode);
}

bool parseOptions(
    int argc,
    char** argv,
    const QCoreApplication& app,
    BenchmarkOptions& options,
    QTextStream& errorOutput) {
    if (argc < 3) {
        errorOutput
            << "Usage: moteur3d_v16_benchmark.exe scene.json dossier_sortie "
               "[--quick] [--cpu-only]\n";
        return false;
    }

    options.scenePath =
        QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    options.outputRoot = QString::fromLocal8Bit(argv[2]);

    const QStringList arguments = app.arguments();
    options.quick = arguments.contains(QStringLiteral("--quick"));
    options.cpuOnly =
        arguments.contains(QStringLiteral("--cpu-only")) || options.quick;
    return true;
}

class BenchmarkReporter {
public:
    BenchmarkReporter()
        : csv_(&csvFile_),
          progress_(&progressFile_) {
    }

    bool open(
        const QString& outputRoot,
        const QString& runName,
        QString& errorMessage) {
        const QDir outputDirectory(outputRoot);
        if (!outputDirectory.mkpath(runName)) {
            errorMessage = QStringLiteral(
                "Cannot create benchmark output directory");
            return false;
        }

        directory_ = QDir(outputDirectory.absoluteFilePath(runName));
        if (!directory_.mkpath(QStringLiteral("images"))) {
            errorMessage = QStringLiteral("Cannot create images directory");
            return false;
        }

        csvFile_.setFileName(directory_.filePath(QStringLiteral("results.csv")));
        progressFile_.setFileName(
            directory_.filePath(QStringLiteral("progress.jsonl")));
        if (!csvFile_.open(QIODevice::WriteOnly | QIODevice::Text)
            || !progressFile_.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errorMessage = QStringLiteral("Cannot write benchmark reports");
            return false;
        }

        csv_ << "id,backend,mode,camera,width,height,threads,samples,status,"
                "wall_ms,engine_ms,rays,tests,aabb,hits,texels,shadow_rays,"
                "error,image\n";
        return true;
    }

    QString filePath(const QString& fileName) const {
        return directory_.filePath(fileName);
    }

    void record(const QJsonObject& result, QTextStream& output) {
        const QString id = result.value(QStringLiteral("id")).toString();
        const QString image = result.value(QStringLiteral("image")).toString();
        const QString status = result.value(QStringLiteral("status")).toString();
        const QString error = result.value(QStringLiteral("error")).toString();

        csv_ << csvField(id) << ','
             << csvField(result.value(QStringLiteral("backend")).toString()) << ','
             << csvField(result.value(QStringLiteral("mode")).toString()) << ','
             << csvField(result.value(QStringLiteral("camera")).toString()) << ','
             << result.value(QStringLiteral("width")).toInt() << ','
             << result.value(QStringLiteral("height")).toInt() << ','
             << result.value(QStringLiteral("threads")).toInt() << ','
             << result.value(QStringLiteral("samples")).toInt() << ','
             << csvField(status) << ','
             << QString::number(
                    result.value(QStringLiteral("wallMs")).toDouble(), 'f', 3) << ','
             << QString::number(
                    result.value(QStringLiteral("engineMs")).toDouble(), 'f', 3) << ','
             << QString::number(
                    result.value(QStringLiteral("rays")).toDouble(), 'f', 0) << ','
             << QString::number(
                    result.value(QStringLiteral("triangleTests")).toDouble(), 'f', 0) << ','
             << QString::number(
                    result.value(QStringLiteral("aabb")).toDouble(), 'f', 0) << ','
             << QString::number(
                    result.value(QStringLiteral("hits")).toDouble(), 'f', 0) << ','
             << QString::number(
                    result.value(QStringLiteral("texels")).toDouble(), 'f', 0) << ','
             << QString::number(
                    result.value(QStringLiteral("shadowRays")).toDouble(), 'f', 0) << ','
             << csvField(result.value(QStringLiteral("error")).toString()) << ','
             << csvField(image) << '\n';
        csv_.flush();

        progress_ << QString::fromUtf8(
            QJsonDocument(result).toJson(QJsonDocument::Compact)) << '\n';
        progress_.flush();

        records_.append(result);
        if (status == QStringLiteral("ok")) {
            ++successful_;
        } else if (status == QStringLiteral("skipped")) {
            ++skipped_;
        } else {
            ++failed_;
        }

        output << QStringLiteral("[%1] %2 — %3 ms\n")
                      .arg(status, id)
                      .arg(result.value(QStringLiteral("wallMs")).toDouble(), 0, 'f', 1);
        if (!error.isEmpty()) {
            output << "  " << error << '\n';
        }
        output.flush();
    }

    bool writeSummary(
        const QString& runName,
        const QString& startedAtUtc,
        const QJsonObject& systemManifest) const {
        const QJsonObject summary{
            {"run", runName},
            {"startedUtc", startedAtUtc},
            {"completedUtc",
             QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"successful", successful_},
            {"skipped", skipped_},
            {"failed", failed_},
            {"tests", records_},
            {"system", systemManifest}
        };
        return writeJson(filePath(QStringLiteral("summary.json")), summary);
    }

    int successfulCount() const {
        return successful_;
    }

    int skippedCount() const {
        return skipped_;
    }

    int failedCount() const {
        return failed_;
    }

private:
    QDir directory_;
    QFile csvFile_;
    QFile progressFile_;
    QTextStream csv_;
    QTextStream progress_;
    QJsonArray records_;
    int successful_ = 0;
    int skipped_ = 0;
    int failed_ = 0;
};

struct BenchmarkRun {
    const Scene& scene;
    const ShadowMap& shadows;
    BenchmarkReporter& reporter;
    QTextStream& output;
    bool quick;
    bool cpuOnly;
    std::atomic<uint64_t> generation{1};
};

void addCaseDetails(
    QJsonObject& result,
    const QString& id,
    const QString& backend,
    const QString& mode,
    const v16::CameraPreset& camera,
    const QSize& size,
    int threads,
    int samples,
    double wallMs,
    const QString& status,
    const QString& image) {
    result["id"] = id;
    result["backend"] = backend;
    result["mode"] = mode;
    result["camera"] = camera.id;
    result["width"] = size.width();
    result["height"] = size.height();
    result["threads"] = threads;
    result["samples"] = samples;
    result["wallMs"] = wallMs;
    result["status"] = status;
    result["image"] = image;
}

void runCpuResolutionTests(
    BenchmarkRun& run,
    const v16::CameraPreset& cameraPreset,
    const QVector<QSize>& resolutions,
    const QVector<int>& threadCounts) {
    for (const QSize& size : resolutions) {
        const auto camera = v16::makeCamera(
            cameraPreset, size.width(), size.height());

        for (const int threadCount : threadCounts) {
            std::vector<uint32_t> pixels(
                size_t(size.width()) * size_t(size.height()));
            const auto renderStart = std::chrono::steady_clock::now();
            const Stats stats = render(
                run.scene,
                run.shadows,
                camera,
                size.width(),
                size.height(),
                pixels,
                static_cast<unsigned>(threadCount),
                false,
                run.generation,
                1,
                1,
                false,
                false,
                false,
                3);
            const double wallMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - renderStart).count();

            const QString id = makeCaseId(
                QStringLiteral("cpu"),
                cameraPreset.id,
                size.width(),
                size.height(),
                threadCount,
                QStringLiteral("pbr"));
            const bool saveImage = run.quick
                || (threadCount == threadCounts.back()
                    && size == resolutions.back());

            QString image;
            QString imageError;
            if (saveImage) {
                image = QStringLiteral("images/%1.png").arg(id);
                if (!writePng(
                        run.reporter.filePath(image),
                        pixels,
                        size.width(),
                        size.height())) {
                    imageError = QStringLiteral(
                        "Could not write PNG image: %1").arg(image);
                    image.clear();
                }
            }

            QJsonObject result = statsToJson(stats);
            addCaseDetails(
                result,
                id,
                QStringLiteral("cpu"),
                QStringLiteral("raytrace_pbr"),
                cameraPreset,
                size,
                threadCount,
                1,
                wallMs,
                imageError.isEmpty()
                    ? QStringLiteral("ok")
                    : QStringLiteral("failed"),
                image);
            if (!imageError.isEmpty()) {
                result["error"] = imageError;
            }
            run.reporter.record(result, run.output);
        }
    }
}

void runCpuFourSampleTest(
    BenchmarkRun& run,
    const v16::CameraPreset& cameraPreset) {
    const QSize size(1920, 1080);
    const auto camera = v16::makeCamera(
        cameraPreset, size.width(), size.height());
    std::vector<uint32_t> pixels(
        size_t(size.width()) * size_t(size.height()));

    const auto renderStart = std::chrono::steady_clock::now();
    const int idealThreads = QThread::idealThreadCount();
    const Stats stats = render(
        run.scene,
        run.shadows,
        camera,
        size.width(),
        size.height(),
        pixels,
        static_cast<unsigned>(std::max(1, idealThreads)),
        false,
        run.generation,
        1,
        4,
        false,
        false,
        false,
        3);
    const double wallMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - renderStart).count();

    const QString id = makeCaseId(
        QStringLiteral("cpu"),
        cameraPreset.id,
        size.width(),
        size.height(),
        idealThreads,
        QStringLiteral("4spp"));
    QString image = QStringLiteral("images/%1.png").arg(id);
    QString imageError;
    if (!writePng(
            run.reporter.filePath(image),
            pixels,
            size.width(),
            size.height())) {
        imageError = QStringLiteral(
            "Could not write PNG image: %1").arg(image);
        image.clear();
    }

    QJsonObject result = statsToJson(stats);
    addCaseDetails(
        result,
        id,
        QStringLiteral("cpu"),
        QStringLiteral("raytrace_pbr_4spp"),
        cameraPreset,
        size,
        idealThreads,
        4,
        wallMs,
        imageError.isEmpty()
            ? QStringLiteral("ok")
            : QStringLiteral("failed"),
        image);
    if (!imageError.isEmpty()) {
        result["error"] = imageError;
    }
    run.reporter.record(result, run.output);
}

void runGpuPreviewTest(
    BenchmarkRun& run,
    const v16::CameraPreset& cameraPreset) {
    const QSize size(1920, 1080);
    const auto camera = v16::makeCamera(
        cameraPreset, size.width(), size.height());

    Stats stats;
    std::wstring error;
    std::vector<uint32_t> pixels;
    const auto renderStart = std::chrono::steady_clock::now();
    const bool succeeded = renderCompute11Preview(
        run.scene,
        camera,
        size.width(),
        size.height(),
        pixels,
        stats,
        error);
    const double wallMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - renderStart).count();

    const QString id = makeCaseId(
        QStringLiteral("gpu"),
        cameraPreset.id,
        size.width(),
        size.height(),
        0,
        QStringLiteral("compute"));
    QString image;
    QString errorMessage = succeeded ? QString() : QString::fromStdWString(error);
    if (succeeded) {
        image = QStringLiteral("images/%1.png").arg(id);
        if (!writePng(
                run.reporter.filePath(image),
                pixels,
                size.width(),
                size.height())) {
            errorMessage = QStringLiteral(
                "Could not write PNG image: %1").arg(image);
            image.clear();
        }
    }

    QJsonObject result = statsToJson(stats);
    addCaseDetails(
        result,
        id,
        QStringLiteral("gpu"),
        QStringLiteral("compute_pbr_shadow_preview"),
        cameraPreset,
        size,
        0,
        1,
        wallMs,
        !succeeded
            ? QStringLiteral("skipped")
            : errorMessage.isEmpty()
                ? QStringLiteral("ok")
                : QStringLiteral("failed"),
        image);
    result["error"] = errorMessage;
    run.reporter.record(result, run.output);
}

void runCameraBenchmarks(
    BenchmarkRun& run,
    const v16::CameraPreset& cameraPreset,
    const QVector<QSize>& resolutions,
    const QVector<int>& threadCounts) {
    runCpuResolutionTests(run, cameraPreset, resolutions, threadCounts);

    if (!run.quick) {
        runCpuFourSampleTest(run, cameraPreset);
    }

    if (!run.cpuOnly) {
        runGpuPreviewTest(run, cameraPreset);
    }
}

void runBenchmarks(
    const PreparedScene& preparedScene,
    const BenchmarkOptions& options,
    BenchmarkReporter& reporter,
    QTextStream& output) {
    const QVector<QSize> resolutions = options.quick
        ? QVector<QSize>{QSize(1920, 1080)}
        : QVector<QSize>{
              QSize(1920, 1080),
              QSize(2560, 1440),
              QSize(3840, 2160)
          };
    const QVector<int> threadCounts = options.quick
        ? QVector<int>{1}
        : QVector<int>{1, 2, 4, 8, 16};
    const int cameraCount = options.quick
        ? 1
        : int(preparedScene.build.runtime.cameras.size());

    BenchmarkRun run{
        preparedScene.build.runtime.renderScene,
        preparedScene.shadows,
        reporter,
        output,
        options.quick,
        options.cpuOnly
    };

    for (int cameraIndex = 0; cameraIndex < cameraCount; ++cameraIndex) {
        const auto& cameraPreset =
            preparedScene.build.runtime.cameras[cameraIndex];
        runCameraBenchmarks(
            run, cameraPreset, resolutions, threadCounts);
    }
}

QJsonObject makeSystemManifest(
    const BenchmarkOptions& options,
    const PreparedScene& preparedScene,
    const QString& timestampUtc) {
    const auto& runtime = preparedScene.build.runtime;
    const auto& scene = runtime.renderScene;

    return {
        {"scene", options.scenePath},
        {"sceneName", runtime.title},
        {"timestampUtc", timestampUtc},
        {"os", QSysInfo::prettyProductName()},
        {"cpuArch", QSysInfo::currentCpuArchitecture()},
        {"idealThreads", QThread::idealThreadCount()},
        {"qtVersion", QStringLiteral(QT_VERSION_STR)},
        {"geometryObjects", int(runtime.objects.size())},
        {"triangles", int(scene.obj.size())},
        {"bvhNodes", int(scene.node.size())},
        {"materials", int(scene.mat.size())},
        {"textureResolution", runtime.textureResolution},
        {"daylightHour", runtime.daylightHour},
        {"buildSceneMs", preparedScene.buildSceneMs},
        {"buildShadowMs", preparedScene.buildShadowMs},
        {"quick", options.quick},
        {"cpuOnly", options.cpuOnly},
        {"note",
         "GPU Compute uses procedural PBR mip textures, hard sun shadows and "
         "straight-through transparency; no physical reflections, GI or CPU "
         "quality parity."}
    };
}

bool prepareScene(
    const BenchmarkOptions& options,
    PreparedScene& preparedScene,
    QTextStream& errorOutput) {
    const auto loadedScene = v16::loadScene(options.scenePath);
    if (!loadedScene) {
        errorOutput << "Invalid scene: " << loadedScene.error << '\n';
        return false;
    }

    const auto buildStart = std::chrono::steady_clock::now();
    preparedScene.build = v16::buildRuntime(
        loadedScene.scene, options.quick ? 128 : 1024);
    preparedScene.buildSceneMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - buildStart).count();
    if (!preparedScene.build) {
        errorOutput << "Scene runtime error: "
                    << preparedScene.build.error << '\n';
        return false;
    }

    const auto& runtime = preparedScene.build.runtime;
    if (runtime.cameras.isEmpty()) {
        errorOutput << "No camera\n";
        return false;
    }

    updateLighting(runtime.daylightHour);

    Stats shadowStats;
    const auto shadowStart = std::chrono::steady_clock::now();
    preparedScene.shadows = buildShadow(runtime.renderScene, shadowStats);
    preparedScene.buildShadowMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - shadowStart).count();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTextStream output(stdout);
    QTextStream errorOutput(stderr);

    BenchmarkOptions options;
    if (!parseOptions(argc, argv, app, options, errorOutput)) {
        return 2;
    }

    const QString runName = QStringLiteral("Pavillon01-V16-%1").arg(
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));

    BenchmarkReporter reporter;
    QString reportError;
    if (!reporter.open(options.outputRoot, runName, reportError)) {
        errorOutput << reportError << '\n';
        return 3;
    }

    PreparedScene preparedScene;
    if (!prepareScene(options, preparedScene, errorOutput)) {
        return 4;
    }

    const QString timestampUtc =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QJsonObject systemManifest =
        makeSystemManifest(options, preparedScene, timestampUtc);
    if (!writeJson(
            reporter.filePath(QStringLiteral("system.json")), systemManifest)) {
        errorOutput << "Cannot write system metadata\n";
        return 3;
    }

    runBenchmarks(preparedScene, options, reporter, output);

    if (!reporter.writeSummary(runName, timestampUtc, systemManifest)) {
        errorOutput << "Could not write summary.json\n";
        return 5;
    }

    output << "Benchmark saved to "
           << QFileInfo(reporter.filePath(QStringLiteral("summary.json")))
                  .absolutePath()
           << '\n';
    output << reporter.successfulCount() << " completed, "
           << reporter.skippedCount() << " skipped, "
           << reporter.failedCount() << " failed.\n";
    return reporter.failedCount() ? 6 : 0;
}
