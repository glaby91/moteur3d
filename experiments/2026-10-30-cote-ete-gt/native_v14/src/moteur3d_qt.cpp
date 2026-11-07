#include "m3d_engine.h"
#include "version.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFormLayout>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextStream>
#include <QSpinBox>
#include <QSlider>
#include <QScrollArea>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QSysInfo>

#include <algorithm>
#include <cmath>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>

namespace {
using namespace m3d;

struct RenderSettings {
    int outW = 1366, outH = 768;
    int renderW = 1366, renderH = 768;
    float distance = 12.f, angle = -43.f, elevation = 7.f, fov = 50.f;
    float targetX = 0.f, targetY = 1.f, targetZ = -8.f;
    bool navigationPreview = false;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency()), samples = 1;
    bool avx2 = false;
    bool directX11 = false, compute11 = false, offlinePathTrace = false;
    unsigned pathBounces = 3;
    int hourStep = 4;
    bool fog = false, dof = false, edgeAa = true, dust = true, videoPreview = false;
};

struct CameraPose { float distance, angle, elevation; };
struct SceneViewPreset {
    QString name;
    V target{};
    float distance=8.f, angle=0.f, elevation=0.f, fov=50.f;
    int hourStep=12;
};

static void applyDustOverlay(const Scene& scene, const Camera& cam, int w, int h,
                             int hourStep, std::vector<uint32_t>& pixels) {
    if (w < 2 || h < 2 || pixels.size() < size_t(w) * size_t(h) || daylight < .12f) return;
    auto mixPixel = [&](int x, int y, int r, int g, int b, int alpha) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        uint32_t& p = pixels[size_t(y) * size_t(w) + size_t(x)];
        const int inv = 255 - alpha;
        const int pr = int((p >> 16) & 255), pg = int((p >> 8) & 255), pb = int(p & 255);
        p = 0xff000000u | (uint32_t((pr * inv + r * alpha) / 255) << 16)
            | (uint32_t((pg * inv + g * alpha) / 255) << 8)
            | uint32_t((pb * inv + b * alpha) / 255);
    };
    // Fixed world-space motes remain stable while navigating. A ray test keeps
    // them behind opaque walls while allowing light shafts through the glazing.
    uint32_t rng = 0x9e3779b9u ^ uint32_t(hourStep * 7919);
    auto random01 = [&]() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(rng & 0x00ffffffu) / float(0x01000000u);
    };
    Stats visibilityStats;
    const int count = 92;
    for (int i = 0; i < count; ++i) {
        const V point{-14.f + random01() * 24.f, .72f + random01() * 7.0f,
                      -31.2f + random01() * 10.0f};
        const V delta = point - cam.eye;
        const float distance = length(delta);
        if (distance < .6f) continue;
        const V direction = delta / distance;
        Hit blocker;
        closest(scene, Ray{cam.eye, direction}, blocker, &visibilityStats, false, -1, true);
        if (blocker.id >= 0 && blocker.t < distance - .12f) continue;
        const float depth = dot(delta, cam.f);
        if (depth <= .35f) continue;
        const float nx = dot(delta, cam.r) / (depth * cam.tanHalf * cam.aspect);
        const float ny = dot(delta, cam.u) / (depth * cam.tanHalf);
        if (std::abs(nx) > .99f || std::abs(ny) > .99f) continue;
        const int x = int((nx * .5f + .5f) * float(w));
        const int y = int((.5f - ny * .5f) * float(h));
        const int radius = depth < 5.f ? 2 : 1;
        const int alpha = int((18.f + random01() * 32.f) * std::clamp(daylight * 1.4f, .35f, 1.f));
        const int green = int(205.f + random01() * 40.f);
        const int blue = int(152.f + random01() * 48.f);
        mixPixel(x, y, 255, green, blue, alpha);
        if (radius == 2) {
            mixPixel(x-1,y,255,green,blue,alpha/3); mixPixel(x+1,y,255,green,blue,alpha/3);
            mixPixel(x,y-1,255,green,blue,alpha/3); mixPixel(x,y+1,255,green,blue,alpha/3);
        }
    }
}

static V clampSceneMotion(const Scene& scene, V from, V to) {
    const V delta=to-from;
    const float distance=length(delta);
    if(distance<.015f)return to;
    const V direction=delta/distance;
    Hit obstacle;
    closest(scene,Ray{from+direction*.02f,direction},obstacle,nullptr,false,-1,true);
    if(obstacle.id>=0&&obstacle.t<distance-.10f)
        return from+direction*std::max(0.f,obstacle.t-.12f);
    return to;
}

static V cameraEyeOffset(float angleDeg,float elevationDeg,float distance) {
    const float a=angleDeg*PI/180.f,e=elevationDeg*PI/180.f;
    return {distance*std::sin(a)*std::cos(e),distance*std::sin(e),distance*std::cos(a)*std::cos(e)};
}

static V clampCameraMotion(const Scene& scene,V from,V to,float angleDeg,float elevationDeg,float distance) {
    const V delta=to-from;
    const float length2=dot(delta,delta);
    if(length2<.000225f)return to;
    const V eyeOffset=cameraEyeOffset(angleDeg,elevationDeg,distance);
    V allowed=clampSceneMotion(scene,from,to);
    V eyeAllowed=clampSceneMotion(scene,from+eyeOffset,to+eyeOffset)-eyeOffset;
    float t=std::min(dot(allowed-from,delta)/length2,dot(eyeAllowed-from,delta)/length2);
    // Sweep two shoulder points as well as the eye centre so a narrow gap
    // between stair flights cannot admit the camera body.
    const float a=angleDeg*PI/180.f;
    const V right{std::cos(a),0.f,-std::sin(a)};
    for(float side:{-.18f,.18f}) {
        const V offset=eyeOffset+right*side;
        const V sideAllowed=clampSceneMotion(scene,from+offset,to+offset)-offset;
        t=std::min(t,dot(sideAllowed-from,delta)/length2);
    }
    return from+delta*std::clamp(t,0.f,1.f);
}

struct BenchmarkRow {
    unsigned threads = 1;
    bool avx2 = false, gpu = false;
    double renderMs = 0, postMs = 0, totalMs = 0;
    uint64_t rays = 0, tests = 0, nodes = 0, aabb = 0, hits = 0, texels = 0;
    uint64_t simdRayPackets = 0, simdTrianglePackets = 0, simdTextureSamples = 0, simdLightBatches = 0;
};

struct BenchmarkPass { unsigned threads = 1; bool avx2 = false, gpu = false; };

class BenchmarkDialog final : public QDialog {
public:
    explicit BenchmarkDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Benchmark — moteur 3D CPU / GPU"));
        resize(1220, 690);
        setStyleSheet("QDialog{background:#0b1923;color:#e0ebf3;} QLabel{color:#e0ebf3;} QGroupBox{border:1px solid #345267;margin-top:10px;padding:8px;color:#e0ebf3;} QComboBox,QSpinBox,QPushButton{padding:5px;}");
        auto* root = new QVBoxLayout(this);
        auto* title = new QLabel(QStringLiteral("BENCHMARK DU MOTEUR"), this);
        title->setStyleSheet("font-size:18px;font-weight:700;color:#f0a45d");
        root->addWidget(title);
        sceneLabel_ = new QLabel(this); root->addWidget(sceneLabel_);
        auto* settings = new QHBoxLayout;
        resolution_ = new QComboBox(this);
        resolution_->addItems({QStringLiteral("Réglage courant"), QStringLiteral("1280 × 720"), QStringLiteral("Full HD — 1920 × 1080"), QStringLiteral("4K UHD — 3840 × 2160")});
        repeats_ = new QSpinBox(this); repeats_->setRange(1, 7); repeats_->setValue(3);
        simdMode_ = new QComboBox(this);
        simdMode_->addItems({QStringLiteral("Mode choisi dans le moteur"), QStringLiteral("x64 standard"), QStringLiteral("AVX2 + FMA3"), QStringLiteral("Comparaison complète : x64 / AVX2 + FMA3")});
        const bool avxAvailable = avx2FmaAvailable();
        if (!avxAvailable) {
            simdMode_->setItemData(2, 0, Qt::UserRole - 1);
            simdMode_->setItemData(3, 0, Qt::UserRole - 1);
            simdMode_->setCurrentIndex(1);
        } else {
            simdMode_->setCurrentIndex(3);
        }
        settings->addWidget(new QLabel(QStringLiteral("Résolution :"), this)); settings->addWidget(resolution_);
        settings->addSpacing(12); settings->addWidget(new QLabel(QStringLiteral("SIMD :"), this)); settings->addWidget(simdMode_, 1);
        settings->addSpacing(12); settings->addWidget(new QLabel(QStringLiteral("Répétitions :"), this)); settings->addWidget(repeats_);
        settings->addWidget(new QLabel(QStringLiteral("+ 1 échauffement"), this)); root->addLayout(settings);
        auto* engine = new QHBoxLayout;
        engine->addWidget(new QLabel(QStringLiteral("Moteur à mesurer :"), this));
        engineMode_ = new QComboBox(this);
        engineMode_->addItems({QStringLiteral("CPU — ray tracing complet"), QStringLiteral("GPU — DirectX 11"), QStringLiteral("Comparaison CPU x64 / GPU DirectX 11")});
        engineMode_->setToolTip(QStringLiteral("La comparaison CPU/GPU mesure deux chemins de rendu de qualité différente : le CPU trace les rayons, tandis que DirectX rasterise et ne calcule pas les mêmes réflexions/réfraction. La lecture GPU→CPU est incluse. Ce n’est pas un gain à qualité égale."));
#ifndef _WIN32
        engineMode_->setItemData(1, 0, Qt::UserRole - 1); engineMode_->setItemData(2, 0, Qt::UserRole - 1);
        engineMode_->setItemData(1, QStringLiteral("DirectX 11 est disponible uniquement sous Windows."), Qt::ToolTipRole);
        engineMode_->setItemData(2, QStringLiteral("DirectX 11 est disponible uniquement sous Windows."), Qt::ToolTipRole);
#endif
        engine->addWidget(engineMode_, 1); root->addLayout(engine);
        auto* threads = new QHBoxLayout;
        threads->addWidget(new QLabel(QStringLiteral("Comparer les threads :"), this));
        for (unsigned n : {1u, 2u, 4u, 8u, 16u, 32u}) {
            auto* check = new QCheckBox(QString::number(n), this); check->setChecked(n <= 8);
            threadChecks_.push_back({n, check}); threads->addWidget(check);
        }
        connect(engineMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
            simdMode_->setEnabled(!running_ && engineMode_->currentIndex()==0);
            for(const auto& item:threadChecks_)item.second->setEnabled(!running_ && engineMode_->currentIndex()!=1);
        });
        threads->addStretch(); root->addLayout(threads);
        progress_ = new QProgressBar(this); progress_->setRange(0, 100); progress_->setValue(0);
        status_ = new QLabel(QStringLiteral("Prêt — choisissez CPU, DirectX 11 ou une comparaison CPU/GPU."), this);
        root->addWidget(status_); root->addWidget(progress_);
        table_ = new QTableWidget(0, 8, this);
        table_->setHorizontalHeaderLabels({QStringLiteral("Moteur / mode"), QStringLiteral("Threads"), QStringLiteral("Rendu médian (ms)"), QStringLiteral("Post (ms)"), QStringLiteral("Total (ms)"), QStringLiteral("Gain SIMD"), QStringLiteral("Débit relatif*"), QStringLiteral("Dispatch AVX2 : rayon / triangle / texture / lumière")});
        table_->horizontalHeaderItem(7)->setToolTip(QStringLiteral("Paquets de 8 rayons, groupes de 8 triangles, échantillons de texture et lots de 8 lumières réellement passés par les noyaux AVX2."));
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setStretchLastSection(true);
        table_->setAlternatingRowColors(true); root->addWidget(table_, 1);
        diagnosis_ = new QLabel(QStringLiteral("Le diagnostic apparaîtra après la première série."), this);
        diagnosis_->setWordWrap(true); diagnosis_->setStyleSheet("color:#9fd5c5;padding:6px;background:#142735");
        root->addWidget(diagnosis_);
        auto* actions = new QHBoxLayout;
        run_ = new QPushButton(QStringLiteral("Lancer le benchmark"), this);
        export_ = new QPushButton(QStringLiteral("Exporter CSV…"), this); export_->setEnabled(false);
        cancel_ = new QPushButton(QStringLiteral("Annuler"), this); cancel_->setEnabled(false);
        close_ = new QPushButton(QStringLiteral("Fermer"), this);
        actions->addWidget(run_); actions->addWidget(export_); actions->addWidget(cancel_); actions->addStretch(); actions->addWidget(close_); root->addLayout(actions);
        connect(close_, &QPushButton::clicked, this, &QDialog::reject);
        connect(export_, &QPushButton::clicked, this, [this] { exportCsv(); });
    }
    void setScene(const QString& name, size_t objects, size_t triangles) {
        sceneLabel_->setText(QStringLiteral("Scène : %1   |   %2 objets   |   %3 triangles").arg(name).arg(qulonglong(objects)).arg(qulonglong(triangles)));
    }
    QPushButton* runButton() const { return run_; }
    QPushButton* cancelButton() const { return cancel_; }
    std::vector<unsigned> selectedThreads() const {
        std::vector<unsigned> out; for (const auto& item : threadChecks_) if (item.second->isChecked()) out.push_back(item.first); return out;
    }
    int repeats() const { return repeats_->value(); }
    int resolutionIndex() const { return resolution_->currentIndex(); }
    int requestedEngine() const { return engineMode_->currentIndex(); }
    int requestedSimdMode() const { return simdMode_->currentIndex(); }
    void setRunning(bool running) {
        running_ = running; run_->setEnabled(!running); export_->setEnabled(!running && !rows_.empty());
        cancel_->setEnabled(running);
        resolution_->setEnabled(!running); repeats_->setEnabled(!running); engineMode_->setEnabled(!running);
        simdMode_->setEnabled(!running && engineMode_->currentIndex()==0);
        for (const auto& item : threadChecks_) item.second->setEnabled(!running && engineMode_->currentIndex()!=1);
        close_->setText(running ? QStringLiteral("Benchmark en cours…") : QStringLiteral("Fermer"));
        close_->setEnabled(!running);
    }
    void setCancelling() { cancel_->setEnabled(false); status_->setText(QStringLiteral("Annulation demandée — arrêt au prochain point d’interruption…")); }
    void setProgress(int value, const QString& text) { progress_->setValue(value); status_->setText(text); }
    void setRows(std::vector<BenchmarkRow> rows, double shadowMs, int width, int height, unsigned samples,
                 unsigned repetitions = 0, QString modeSummary = {}, QString settingsSummary = {}) {
        rows_ = std::move(rows); shadowMs_ = shadowMs; width_ = width; height_ = height; samples_ = samples;
        repetitionsMeasured_ = repetitions; modeSummary_ = std::move(modeSummary); settingsSummary_ = std::move(settingsSummary);
        table_->clearContents(); table_->setRowCount(int(rows_.size()));
        if (rows_.empty()) { diagnosis_->setText(QStringLiteral("Aucune mesure disponible.")); export_->setEnabled(false); return; }
        const auto baseIt = std::find_if(rows_.begin(), rows_.end(), [](const BenchmarkRow& r) { return !r.gpu && !r.avx2; });
        const BenchmarkRow& referenceRow = (baseIt == rows_.end() ? rows_.front() : *baseIt);
        const double referenceMs = referenceRow.totalMs;
        double maxRelative = 1.0;
        for (const auto& row : rows_) if (row.totalMs > 0) maxRelative = std::max(maxRelative, referenceMs / row.totalMs);
        QStringList modesMeasured;
        auto modeLabel=[](const BenchmarkRow& row){return row.gpu?QStringLiteral("DirectX 11 GPU"):row.avx2?QStringLiteral("AVX2 + FMA3"):QStringLiteral("x64 standard");};
        for (const auto& row : rows_) { const QString name = modeLabel(row); if (!modesMeasured.contains(name)) modesMeasured << name; }
        for (int i = 0; i < int(rows_.size()); ++i) {
            const auto& row = rows_[size_t(i)];
            const QString modeName = modeLabel(row);
            const auto standardIt = std::find_if(rows_.begin(), rows_.end(), [&row](const BenchmarkRow& r) { return !r.gpu && !r.avx2 && r.threads == row.threads; });
            const QString simdGain = !row.gpu && row.avx2 && standardIt != rows_.end()
                ? QStringLiteral("%1×").arg(standardIt->totalMs / std::max(.001, row.totalMs), 0, 'f', 2)
                : QStringLiteral("—");
            const double relative = referenceMs / std::max(.001, row.totalMs);
            const QStringList values{modeName, row.gpu?QStringLiteral("GPU"):QString::number(row.threads), QString::number(row.renderMs, 'f', 1),
                QString::number(row.postMs, 'f', 1), QString::number(row.totalMs, 'f', 1), simdGain, QString()};
            for (int c = 0; c < 6; ++c) table_->setItem(i, c, new QTableWidgetItem(values[c]));
            auto compactCount = [](uint64_t n) { return n >= 1000000 ? QString::number(double(n) / 1000000.0, 'f', 1) + QStringLiteral("M") : n >= 1000 ? QString::number(double(n) / 1000.0, 'f', 1) + QStringLiteral("k") : QString::number(qulonglong(n)); };
            const QString dispatch = !row.gpu && row.avx2 ? QStringLiteral("R %1 · T %2 · Tex %3 · L %4").arg(compactCount(row.simdRayPackets), compactCount(row.simdTrianglePackets), compactCount(row.simdTextureSamples), compactCount(row.simdLightBatches)) : QStringLiteral("—");
            table_->setItem(i, 7, new QTableWidgetItem(dispatch));
            auto* bar = new QProgressBar(table_); bar->setRange(0, 100);
            bar->setValue(int(std::lround(100.0 * relative / maxRelative)));
            bar->setFormat(QStringLiteral("%1×").arg(relative, 0, 'f', 2));
            bar->setToolTip(referenceRow.gpu?QStringLiteral("Référence : DirectX 11 GPU"):
                QStringLiteral("Référence : %1, %2 thread(s)").arg(referenceRow.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard")).arg(referenceRow.threads));
            bar->setStyleSheet("QProgressBar{border:1px solid #426276;border-radius:3px;text-align:center;color:#f1f6f9;background:#102330;min-width:145px;} QProgressBar::chunk{background:#12a86b;}");
            table_->setCellWidget(i, 6, bar);
        }
        QStringList advice{QStringLiteral("%1 répétitions mesurées par configuration, médiane affichée; échauffement exclu. Ombres : %2 ms pour la passe unique.")
            .arg(repetitionsMeasured_).arg(shadowMs_, 0, 'f', 1)};
        if(std::any_of(rows_.begin(),rows_.end(),[](const BenchmarkRow& row){return row.gpu;}))
            advice << QStringLiteral("* Débit relatif indicatif : CPU ray tracing complet et GPU raster ne rendent pas les mêmes effets. Réflexions/réfraction et lecture GPU→CPU diffèrent; ce n’est pas un gain à qualité égale.");
        if (modesMeasured.size() > 1) {
            for (const auto& row : rows_) if (!row.gpu && row.avx2) {
                const auto ref = std::find_if(rows_.begin(), rows_.end(), [&row](const BenchmarkRow& r) { return !r.gpu && !r.avx2 && r.threads == row.threads; });
                if (ref != rows_.end()) advice << QStringLiteral("À %1 threads, AVX2 + FMA3 : %2× par rapport à x64 standard (%3 ms contre %4 ms).")
                    .arg(row.threads).arg(ref->totalMs / std::max(.001, row.totalMs), 0, 'f', 2).arg(row.totalMs, 0, 'f', 1).arg(ref->totalMs, 0, 'f', 1);
            }
        }
        for (const QString& modeName : modesMeasured) {
            std::vector<const BenchmarkRow*> group;
            for (const auto& row : rows_) if (modeLabel(row) == modeName) group.push_back(&row);
            if (group.size() >= 2) {
                const double scaling = group.front()->totalMs / std::max(.001, group.back()->totalMs);
                advice << QStringLiteral("%1 : %2× entre %3 et %4 threads.")
                    .arg(modeName).arg(scaling, 0, 'f', 2).arg(group.front()->threads).arg(group.back()->threads);
            }
        }
        if (!settingsSummary_.isEmpty()) advice << QStringLiteral("Réglages : %1.").arg(settingsSummary_);
        diagnosis_->setText(advice.join(QStringLiteral(" ")));
        export_->setEnabled(true);
    }
    void closeEvent(QCloseEvent* event) override { if (running_) { event->ignore(); return; } QDialog::closeEvent(event); }
private:
    QLabel *sceneLabel_ = nullptr, *status_ = nullptr, *diagnosis_ = nullptr;
    QComboBox *resolution_ = nullptr, *simdMode_ = nullptr, *engineMode_ = nullptr; QSpinBox* repeats_ = nullptr;
    QProgressBar* progress_ = nullptr; QTableWidget* table_ = nullptr;
    QPushButton *run_ = nullptr, *export_ = nullptr, *cancel_ = nullptr, *close_ = nullptr;
    std::vector<std::pair<unsigned, QCheckBox*>> threadChecks_;
    std::vector<BenchmarkRow> rows_; double shadowMs_ = 0; int width_ = 0, height_ = 0;
    unsigned samples_ = 1, repetitionsMeasured_ = 0; bool running_ = false; QString settingsSummary_, modeSummary_;
    void exportCsv() {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Exporter les résultats du benchmark"), QStringLiteral("benchmark_moteur3d.csv"), QStringLiteral("CSV (*.csv)"));
        if (path.isEmpty()) return;
        QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::warning(this, QStringLiteral("Export CSV"), QStringLiteral("Impossible d’écrire ce fichier.")); return; }
        QTextStream out(&file); out.setEncoding(QStringConverter::Utf8);
        out << "# build=" << M3D_BUILD_ID_ANSI << "; scene=" << sceneLabel_->text() << "\n";
        out << "# cpu_arch=" << QSysInfo::currentCpuArchitecture() << "; os=" << QSysInfo::productType() << " " << QSysInfo::productVersion()
            << "; logical_processors=" << std::thread::hardware_concurrency() << "; avx2_fma_available=" << (avx2FmaAvailable() ? 1 : 0) << "\n";
        out << "# simd_selection=" << modeSummary_ << "; repetitions=" << repetitionsMeasured_ << "; warmup=1\n";
        out << "# settings=" << settingsSummary_ << "\n";
        out << "# resolution=" << width_ << "x" << height_ << "; samples_per_pixel=" << samples_ << "; shadow_build_ms=" << QString::number(shadowMs_, 'f', 3) << "\n";
        const bool hasGpu=std::any_of(rows_.begin(),rows_.end(),[](const BenchmarkRow& row){return row.gpu;});
        if(hasGpu)out << "# comparison=non-equivalent paths; CPU full ray tracing vs DirectX raster preview; relative throughput is indicative only\n";
        out << "engine;simd;threads;render_median_ms;post_median_ms;total_median_ms;speedup_vs_reference;quality_comparable;simd_gain_vs_x64_same_threads;simd_ray_packets;simd_triangle_packets;simd_texture_samples;simd_light_batches;rays;BVH_tests;BVH_nodes;AABB_tests;hits;texels\n";
        const auto baseIt = std::find_if(rows_.begin(), rows_.end(), [](const BenchmarkRow& r) { return !r.gpu && !r.avx2; });
        const double referenceMs = rows_.empty() ? 0 : (baseIt == rows_.end() ? rows_.front() : *baseIt).totalMs;
        for (const auto& r : rows_) {
            const QString engineName = r.gpu ? QStringLiteral("directx11") : QStringLiteral("cpu");
            const QString modeName = r.gpu ? QString() : r.avx2 ? QStringLiteral("avx2_fma3") : QStringLiteral("x64");
            const double relative = referenceMs / std::max(.001, r.totalMs);
            const auto standard = std::find_if(rows_.begin(), rows_.end(), [&r](const BenchmarkRow& x) { return !x.gpu && !x.avx2 && x.threads == r.threads; });
            const QString simdGain = !r.gpu && r.avx2 && standard != rows_.end() ? QString::number(standard->totalMs / std::max(.001, r.totalMs), 'f', 4) : QString();
            out << engineName << ';' << modeName << ';' << (r.gpu?QStringLiteral("GPU"):QString::number(r.threads)) << ';' << QString::number(r.renderMs, 'f', 3) << ';'
                << QString::number(r.postMs, 'f', 3) << ';' << QString::number(r.totalMs, 'f', 3) << ';'
                << QString::number(relative, 'f', 4) << ';' << (hasGpu?QStringLiteral("no"):QStringLiteral("yes")) << ';' << simdGain << ';'
                << r.simdRayPackets << ';' << r.simdTrianglePackets << ';' << r.simdTextureSamples << ';' << r.simdLightBatches << ';'
                << r.rays << ';' << r.tests << ';' << r.nodes << ';' << r.aabb << ';' << r.hits << ';' << r.texels << '\n';
        }
    }
};
static CameraPose ellipseOrbitPose(float distance, float angleDeg, float elevationDeg, float phaseDeg) {
    const float a0 = angleDeg * PI / 180.f;
    const float e0 = elevationDeg * PI / 180.f;
    const float horizontal0 = distance * std::cos(e0);
    const float height0 = distance * std::sin(e0);
    const float theta = phaseDeg * PI / 180.f;
    const float tangentRadius = horizontal0 * .9f;
    const float x = std::sin(a0) * horizontal0 * std::cos(theta) + std::cos(a0) * tangentRadius * std::sin(theta);
    const float z = std::cos(a0) * horizontal0 * std::cos(theta) - std::sin(a0) * tangentRadius * std::sin(theta);
    const float horizontal = std::sqrt(x * x + z * z);
    const float lift = horizontal0 * .22f * (1.f - std::cos(theta)) * .5f;
    const float height = height0 + lift;
    return {std::sqrt(horizontal * horizontal + height * height), std::atan2(x, z) * 180.f / PI, std::atan2(height, horizontal) * 180.f / PI};
}

static CameraPose guidedPripyatTour(float progress, V& look) {
    // 90-second guided tour: exterior, foyer, rooms on all three levels,
    // the hollow stair tower, the roof walk, then a short rooftop hold.
    if (progress >= .88f) {
        look={0.f,11.35f,-23.5f};
        return {3.0f,0.f,0.f};
    }
    progress=std::clamp(progress/.88f,0.f,.999999f);
    static constexpr V route[] = {
        { 0.f,1.6f,48.f }, { 0.f,1.6f,25.f }, { 0.f,1.6f,2.f },
        { 0.f,1.6f,-17.f }, { 0.f,1.6f,-23.5f },
        { 12.9f,1.6f,-23.5f }, { 12.9f,1.6f,-30.5f },
        { 0.f,1.6f,-30.5f }, { -21.4f,1.6f,-30.5f },
        { -32.5f,1.6f,-30.5f }, { -43.35f,1.6f,-30.5f },
        { -43.35f,1.6f,-38.0f }, { -32.5f,1.6f,-38.0f },
        { -43.35f,1.6f,-30.5f }, { -32.5f,1.6f,-30.5f },
        { -21.4f,1.6f,-30.5f }, { 0.f,1.6f,-30.5f },
        { 12.9f,1.6f,-30.5f }, { 19.0f,1.6f,-30.5f },
        { 23.5f,1.6f,-40.5f }, { 23.5f,2.4f,-36.0f },
        { 23.5f,3.3f,-31.0f }, { 23.5f,4.6f,-25.0f },
        { 23.5f,4.7f,-30.5f }, { 19.0f,4.7f,-30.5f },
        { 12.9f,4.7f,-30.5f }, { 0.f,4.7f,-30.5f },
        { -21.4f,4.7f,-30.5f }, { -32.5f,4.7f,-30.5f },
        { -43.35f,4.7f,-30.5f }, { -32.5f,4.7f,-30.5f },
        { -21.4f,4.7f,-30.5f }, { 0.f,4.7f,-30.5f },
        { 12.9f,4.7f,-30.5f }, { 19.0f,4.7f,-30.5f },
        { 23.5f,4.7f,-30.5f }, { 23.5f,4.7f,-40.1f },
        { 23.5f,5.6f,-35.0f }, { 23.5f,6.6f,-30.0f },
        { 23.5f,7.7f,-25.0f }, { 23.5f,7.8f,-30.5f },
        { 19.0f,7.8f,-30.5f }, { 12.9f,7.8f,-30.5f },
        { 0.f,7.8f,-30.5f }, { -21.4f,7.8f,-30.5f },
        { -32.5f,7.8f,-30.5f }, { -43.35f,7.8f,-30.5f },
        { -32.5f,7.8f,-30.5f }, { -21.4f,7.8f,-30.5f },
        { 0.f,7.8f,-30.5f }, { 12.9f,7.8f,-30.5f },
        { 19.0f,7.8f,-30.5f }, { 23.5f,7.8f,-30.5f },
        { 23.5f,7.8f,-40.5f }, { 23.5f,8.7f,-35.5f },
        { 23.5f,9.7f,-30.0f }, { 23.5f,10.65f,-25.0f },
        { 23.5f,11.15f,-21.6f }, { 23.5f,11.35f,-20.3f },
        { 16.0f,11.35f,-20.3f }, { 8.0f,11.35f,-20.3f },
        { 0.0f,11.35f,-20.3f }, { 0.0f,11.35f,-23.5f }
    };
    constexpr int count=int(sizeof(route)/sizeof(route[0]));
    float totalLength=0.f;
    for(int i=0;i<count-1;i++) totalLength+=length(route[i+1]-route[i]);
    float remaining=progress*totalLength;
    int i=0;
    while(i<count-2&&remaining>length(route[i+1]-route[i])){remaining-=length(route[i+1]-route[i]);i++;}
    const float segmentLength=length(route[i+1]-route[i]);
    const float t=segmentLength>1e-5f?std::clamp(remaining/segmentLength,0.f,1.f):0.f;
    const V& p1=route[i];
    const V& p2=route[i+1];
    const V cameraPath=p1+(p2-p1)*t;
    // Keep the path on its authored corridor, but turn the view gradually
    // before corners. The old tangent switched instantly at every waypoint,
    // which looked like a video jump (most noticeably at the foyer entrance).
    const float lookAhead=1.35f;
    float aheadDistance=progress*totalLength+lookAhead;
    aheadDistance=std::clamp(aheadDistance,0.f,totalLength);
    int ai=0;
    while(ai<count-2 && aheadDistance>length(route[ai+1]-route[ai])) {
        aheadDistance-=length(route[ai+1]-route[ai]);
        ++ai;
    }
    const float al=length(route[ai+1]-route[ai]);
    const float at=al>1e-5f?std::clamp(aheadDistance/al,0.f,1.f):0.f;
    const V ahead=route[ai]+(route[ai+1]-route[ai])*at;
    const float dx=ahead.x-cameraPath.x,dz=ahead.z-cameraPath.z;
    const float speed=std::max(.001f,std::sqrt(dx*dx+dz*dz));
    const float angle=std::atan2(-dx/speed,-dz/speed)*180.f/PI;
    const float cameraDistance=cameraPath.z>-16.f?3.0f:1.55f;
    // Route samples describe the camera's physical position. The renderer's
    // camera API stores a look target and places the eye one distance behind
    // it; compensate here so the eye, rather than the look target, passes
    // through the authored door and foyer openings.
    look=cameraPath-cameraEyeOffset(angle,0.f,cameraDistance);
    return {cameraDistance,angle,0.f};
}

class ImageView final : public QWidget {
public:
    explicit ImageView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(480, 320); setAutoFillBackground(false);
        setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
    }
    void setFrame(QImage image) { image_ = std::move(image); update(); }
    void setNavigationHandlers(std::function<void(int,int,bool)> drag, std::function<void(int)> zoom,
                               std::function<bool(int)> move, std::function<void()> release) {
        dragHandler_ = std::move(drag); zoomHandler_ = std::move(zoom);
        moveHandler_ = std::move(move); releaseHandler_ = std::move(release);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.fillRect(rect(), QColor(4, 12, 17));
        if (image_.isNull()) return;
        QSize fit=image_.size(); fit.scale(size(), Qt::KeepAspectRatio);
        QRect target(QPoint((width()-fit.width())/2,(height()-fit.height())/2),fit);
        p.setRenderHint(QPainter::SmoothPixmapTransform,true); p.drawImage(target,image_);
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()==Qt::LeftButton || event->button()==Qt::RightButton) {
            pressedButton_=event->button(); lastMouse_=event->position().toPoint(); dragged_=false;
            setCursor(Qt::ClosedHandCursor); setFocus(Qt::MouseFocusReason); event->accept(); return;
        }
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(pressedButton_!=Qt::NoButton && (event->buttons() & pressedButton_)) {
            const QPoint current=event->position().toPoint(), delta=current-lastMouse_; lastMouse_=current;
            if(!delta.isNull() && dragHandler_) { dragged_=true; dragHandler_(delta.x(),delta.y(),pressedButton_==Qt::RightButton); }
            event->accept(); return;
        }
        QWidget::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()==pressedButton_) {
            pressedButton_=Qt::NoButton; unsetCursor();
            if(dragged_ && releaseHandler_) releaseHandler_();
            dragged_=false; event->accept(); return;
        }
        QWidget::mouseReleaseEvent(event);
    }
    void wheelEvent(QWheelEvent* event) override {
        if(zoomHandler_) zoomHandler_(event->angleDelta().y()); event->accept();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if(moveHandler_ && moveHandler_(event->key())) { event->accept(); return; }
        QWidget::keyPressEvent(event);
    }
private:
    QImage image_; QPoint lastMouse_; Qt::MouseButton pressedButton_=Qt::NoButton; bool dragged_=false;
    std::function<void(int,int,bool)> dragHandler_;
    std::function<void(int)> zoomHandler_;
    std::function<bool(int)> moveHandler_;
    std::function<void()> releaseHandler_;
};
class VideoPreviewWindow final : public QDialog {
public:
    explicit VideoPreviewWindow(QWidget* parent = nullptr) : QDialog(parent, Qt::Window) {
        setWindowTitle(QStringLiteral("Aperçu rotatif — 800 × 600"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        image_ = new ImageView(this);
        image_->setMinimumSize(800, 600);
        layout->addWidget(image_);
        status_ = new QLabel(QStringLiteral("En attente du pré-calcul"), this);
        layout->addWidget(status_);
        resize(824, 654);
    }
    void setFrame(const QImage& image) { image_->setFrame(image); status_->setText(QStringLiteral("Lecture des images pré-calculées")); }
    void setStatus(const QString& text) { status_->setText(text); }
private:
    ImageView* image_ = nullptr;
    QLabel* status_ = nullptr;
};


// Independent, non-modal render viewport. Main window keeps all controls and
// its embedded preview; the second window receives exactly the same CPU/GPU
// frame without starting a second render or copying scene geometry.
class DetachedRenderWindow final : public QDialog {
public:
    explicit DetachedRenderWindow(QWidget* parent=nullptr):QDialog(parent,Qt::Window){
        setWindowTitle(QStringLiteral("Moteur3D — Rendu indépendant"));
        auto* layout=new QVBoxLayout(this);
        layout->setContentsMargins(6,6,6,6);
        view_=new ImageView(this);
        view_->setMinimumSize(640,360);
        layout->addWidget(view_,1);
        resize(1280,760);
    }
    void setFrame(const QImage& frame){if(view_)view_->setFrame(frame);}
private:
    ImageView* view_=nullptr;
};

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(Scene loadedScene, QString scenePath) : scene_(std::move(loadedScene)), currentScenePath_(std::move(scenePath)) {
        setWindowTitle(QStringLiteral("Moteur 3D natif x64 — ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE));
        resize(1480, 900);
        setMinimumSize(1000, 620);
        buildUi();
        rendererCapabilities_ = detectRendererCapabilities();
        if (rendererCapabilities_.dxrHardware && !rendererCapabilities_.dxrBackend)
            rendererStatus_->setText(QStringLiteral("GPU compatible DXR détecté. Backend DXR du moteur absent pour l’instant : Auto rend sur CPU."));
        else if (!rendererCapabilities_.dxrHardware)
            rendererStatus_->setText(QStringLiteral("DXR non disponible : le rendu complet s’exécute sur CPU."));
        else
            rendererStatus_->setText(QStringLiteral("DXR disponible : sélection automatique au rendu."));
        loadSceneViewPresets();
        applySceneCameraPreset();
        readSettings();
        auto* fileMenu = menuBar()->addMenu(QStringLiteral("Scène"));
        auto* openScene = fileMenu->addAction(QStringLiteral("Ouvrir une scène…"));
        connect(openScene, &QAction::triggered, this, [this] { openSceneFile(); });
        auto* toolsMenu = menuBar()->addMenu(QStringLiteral("Outils"));
        auto* separateView = toolsMenu->addAction(QStringLiteral("Ouvrir la deuxième fenêtre de rendu"));
        connect(separateView,&QAction::triggered,this,[this]{
            if(!detachedRender_)detachedRender_=new DetachedRenderWindow(this);
            if(!lastRenderedFrame_.isNull())detachedRender_->setFrame(lastRenderedFrame_);
            detachedRender_->show();detachedRender_->raise();detachedRender_->activateWindow();
        });
        benchmarkAction_ = toolsMenu->addAction(QStringLiteral("Benchmark du moteur…"));
        connect(benchmarkAction_, &QAction::triggered, this, [this] { openBenchmark(); });
        updateLighting(12.f);
        Stats initialShadowStats; initialShadowStats.avx2 = settings_.avx2;
        shadows_ = buildShadow(scene_, initialShadowStats);
        shadowHour_ = 12;
        coordinator_ = std::thread([this] { renderLoop(); });
        requestRender();
    }

    ~MainWindow() override {
        generation_.fetch_add(1);
        benchmarkGeneration_.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            stopping_ = true;
        }
        requestCv_.notify_one();
        if (benchmarkThread_.joinable()) benchmarkThread_.join();
        if (coordinator_.joinable()) coordinator_.join();
    }

private:
    Scene scene_;
    ShadowMap shadows_;
    std::mutex sceneMutex_;
    QString currentScenePath_;
    int shadowHour_ = -1;
    RenderSettings settings_;
    std::mutex requestMutex_;
    std::condition_variable requestCv_;
    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> benchmarkGeneration_{1};
    std::thread coordinator_;
    std::thread benchmarkThread_;
    bool stopping_ = false, pending_ = false, benchmarking_ = false;
    ImageView* image_ = nullptr;
    DetachedRenderWindow* detachedRender_ = nullptr;
    QImage lastRenderedFrame_;
    QLabel *status_ = nullptr, *stats_ = nullptr, *hourLabel_ = nullptr, *threadLabel_ = nullptr;
    QComboBox *resolution_ = nullptr, *renderer_ = nullptr, *quality_ = nullptr, *simdMode_ = nullptr;
    QLabel* rendererStatus_ = nullptr;
    RendererCapabilities rendererCapabilities_{};
    QSlider *distance_ = nullptr, *angle_ = nullptr, *elevation_ = nullptr, *fov_ = nullptr;
    QSlider *threads_ = nullptr, *hour_ = nullptr;
    QCheckBox *fog_ = nullptr, *dof_ = nullptr, *dust_ = nullptr, *edgeAa_ = nullptr, *videoPreview_ = nullptr, *fastNavigation_ = nullptr;
    QPushButton *renderButton_ = nullptr, *homeButton_ = nullptr, *frontButton_ = nullptr, *rearButton_ = nullptr, *nightButton_ = nullptr;
    QComboBox* sceneViewSelector_ = nullptr;
    QLabel* sceneViewLabel_ = nullptr;
    std::vector<SceneViewPreset> sceneViewPresets_;
    VideoPreviewWindow* videoWindow_ = nullptr;
    QAction* benchmarkAction_ = nullptr;
    QTimer* videoPlaybackTimer_ = nullptr;
    QElapsedTimer tourTimer_;
    std::vector<QImage> videoFrames_;
    size_t videoFrameIndex_ = 0, videoPlaybackIndex_ = 0;
    bool videoBuilding_ = false, videoPlaying_ = false;
    float videoAngle_ = 0.f;
    V cameraTarget_{0.f,1.f,-8.f};
    bool navigationActive_ = false;
    bool forceFullQuality_ = false;
    QTimer* navigationPreviewTimer_ = nullptr;
    QTimer* navigationIdleTimer_ = nullptr;

    static QSlider* slider(Qt::Orientation orientation, int min, int max, int value, QWidget* parent) {
        auto* s = new QSlider(orientation, parent);
        s->setRange(min, max);
        s->setValue(value);
        return s;
    }

    bool isRaptureScene() const {
        return QFileInfo(currentScenePath_).fileName().contains(QStringLiteral("rapture"),Qt::CaseInsensitive);
    }

    bool isPripyatScene() const {
        return QFileInfo(currentScenePath_).fileName().contains(QStringLiteral("pripyat"),Qt::CaseInsensitive);
    }

    void loadSceneViewPresets() {
        if(!sceneViewSelector_) return;
        const QSignalBlocker blocker(sceneViewSelector_);
        sceneViewPresets_.clear();
        sceneViewSelector_->clear();
        sceneViewSelector_->setVisible(isPripyatScene());
        sceneViewSelector_->setEnabled(false);
        if(!isPripyatScene()) return;
        QFile file(QCoreApplication::applicationDirPath()+QStringLiteral("/scene_pripyat_views.json"));
        if(!file.open(QIODevice::ReadOnly)) {
            sceneViewSelector_->setToolTip(QStringLiteral("Annexe des vues introuvable près du moteur."));
            return;
        }
        QJsonParseError parseError{};
        const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),&parseError);
        if(parseError.error!=QJsonParseError::NoError || !doc.isObject()) {
            sceneViewSelector_->setToolTip(QStringLiteral("Annexe des vues illisible."));
            return;
        }
        const QJsonArray views=doc.object().value(QStringLiteral("views")).toArray();
        for(const QJsonValue& value:views) {
            if(sceneViewPresets_.size()>=13 || !value.isObject()) break;
            const QJsonObject o=value.toObject();
            const QJsonArray target=o.value(QStringLiteral("target")).toArray();
            if(target.size()!=3) continue;
            SceneViewPreset p;
            p.name=o.value(QStringLiteral("name")).toString().trimmed();
            p.target={float(target[0].toDouble()),float(target[1].toDouble()),float(target[2].toDouble())};
            p.distance=float(o.value(QStringLiteral("distance")).toDouble(8.0));
            p.angle=float(o.value(QStringLiteral("angle")).toDouble());
            p.elevation=float(o.value(QStringLiteral("elevation")).toDouble());
            p.fov=float(o.value(QStringLiteral("fov")).toDouble(50.0));
            p.hourStep=o.value(QStringLiteral("hour_step")).toInt(12);
            if(p.name.isEmpty() || p.distance<3.f || p.distance>130.f || p.fov<35.f || p.fov>105.f) continue;
            sceneViewPresets_.push_back(p);
            sceneViewSelector_->addItem(p.name);
        }
        sceneViewSelector_->setEnabled(!sceneViewPresets_.empty());
        sceneViewSelector_->setCurrentIndex(-1);
        sceneViewSelector_->setToolTip(QStringLiteral("Treize vues documentées : Energetik, grande roue, Polissya et galerie."));
    }

    void applySceneViewPreset(int index) {
        if(!isPripyatScene() || index<0 || size_t(index)>=sceneViewPresets_.size()) return;
        const SceneViewPreset& p=sceneViewPresets_[size_t(index)];
        navigationActive_=false;
        if(navigationPreviewTimer_) navigationPreviewTimer_->stop();
        if(navigationIdleTimer_) navigationIdleTimer_->stop();
        cameraTarget_=p.target;
        setCameraSliders(int(std::lround(p.distance*2.f)),int(std::lround(p.angle)),
                         int(std::lround(p.elevation)),int(std::lround(p.fov)));
        hour_->setValue(p.hourStep);
        forceFullQuality_=true;
        readSettings();
        requestRender();
    }

    int videoFrameCount() const { return isPripyatScene() ? 360 : 180; }

    void setCameraSliders(int distanceHalf,int angle,int elevation,int fov) {
        const QSignalBlocker b0(distance_),b1(angle_),b2(elevation_),b3(fov_);
        distance_->setValue(distanceHalf); angle_->setValue(angle); elevation_->setValue(elevation); fov_->setValue(fov);
    }

    void applySceneCameraPreset() {
        navigationActive_=false;
        if(navigationPreviewTimer_) navigationPreviewTimer_->stop();
        if(navigationIdleTimer_) navigationIdleTimer_->stop();
        if(fastNavigation_) {
            fastNavigation_->setEnabled(isPripyatScene());
            fastNavigation_->setText(QStringLiteral("Visite fluide — aperçu léger après déplacement"));
            fastNavigation_->setChecked(isPripyatScene());
        }
        if(isPripyatScene()) {
            if(sceneViewSelector_) {
                const QSignalBlocker viewBlock(sceneViewSelector_);
                sceneViewSelector_->setCurrentIndex(-1);
                sceneViewSelector_->setVisible(true);
            }
            if(sceneViewLabel_) sceneViewLabel_->setVisible(true);
            cameraTarget_={0.f,1.6f,48.f}; setCameraSliders(17,0,0,55);
            if(videoPreview_) {
                videoPreview_->setText(QStringLiteral("Visite guidée — 800 × 600"));
                videoPreview_->setToolTip(QStringLiteral("Rendu à 800 × 600 sur CPU et DirectX 11; sur CPU, la visite prend davantage de temps."));
            }
            if(videoWindow_) videoWindow_->setWindowTitle(QStringLiteral("Parcours automatique — Pripiat"));
        } else if(isRaptureScene()) {
            if(sceneViewSelector_) sceneViewSelector_->setVisible(false);
            if(sceneViewLabel_) sceneViewLabel_->setVisible(false);
            cameraTarget_={0.f,4.4f,-35.f}; setCameraSliders(28,180,10,52);
            if(videoPreview_) videoPreview_->setText(QStringLiteral("Mode vidéo — aperçu rotatif 800 × 600"));
            if(videoWindow_) videoWindow_->setWindowTitle(QStringLiteral("Aperçu rotatif — 800 × 600"));
        } else {
            if(sceneViewSelector_) sceneViewSelector_->setVisible(false);
            if(sceneViewLabel_) sceneViewLabel_->setVisible(false);
            cameraTarget_={0.f,1.f,-8.f}; setCameraSliders(24,-43,7,50);
            if(videoPreview_) videoPreview_->setText(QStringLiteral("Mode vidéo — aperçu rotatif 800 × 600"));
            if(videoWindow_) videoWindow_->setWindowTitle(QStringLiteral("Aperçu rotatif — 800 × 600"));
        }
    }

    void beginNavigation() {
        navigationActive_=true;
        if(navigationPreviewTimer_ && !navigationPreviewTimer_->isActive()) navigationPreviewTimer_->start();
        if(navigationIdleTimer_) navigationIdleTimer_->start(450);
    }

    void finishNavigation() {
        if(!navigationActive_) return;
        navigationActive_=false;
        if(navigationPreviewTimer_) navigationPreviewTimer_->stop();
        readSettings(); requestRender();
        if(fastNavigation_ && fastNavigation_->isChecked())
            status_->setText(QStringLiteral("Visite fluide — aperçu léger. Calculer l’image lance le rendu complet."));
    }

    void handleCameraDrag(int dx,int dy,bool pan) {
        if(!dx && !dy) return;
        beginNavigation();
        if(pan) {
            const float distance=distance_->value()*.5f, angle=angle_->value()*PI/180.f;
            const float elevation=elevation_->value()*PI/180.f, fov=fov_->value()*PI/180.f;
            const float scale=std::max(.001f,distance*2.f*std::tan(fov*.5f)/std::max(1,image_->height()));
            const V right{std::cos(angle),0.f,-std::sin(angle)};
            const V up{-std::sin(angle)*std::sin(elevation),std::cos(elevation),-std::cos(angle)*std::sin(elevation)};
            const V previous=cameraTarget_;
            cameraTarget_=cameraTarget_+right*(-float(dx)*scale)+up*(float(dy)*scale);
            if(isPripyatScene()) {
                std::lock_guard<std::mutex> guard(sceneMutex_);
                cameraTarget_=clampCameraMotion(scene_,previous,cameraTarget_,angle_->value(),elevation_->value(),distance);
            }
        } else {
            setCameraSliders(distance_->value(),angle_->value()-int(std::lround(dx*.32f)),
                std::max(-10,std::min(65,elevation_->value()+int(std::lround(dy*.24f)))),fov_->value());
        }
        readSettings(); navigationIdleTimer_->start(450);
    }

    void handleCameraZoom(int delta) {
        if(!delta) return;
        beginNavigation();
        const int increments=std::max(1,int(std::lround(std::abs(delta)/120.f)))*2*(delta>0?-1:1);
        distance_->setValue(distance_->value()+increments);
        readSettings(); navigationIdleTimer_->start(450);
    }

    bool handleCameraKey(int key) {
        const float distance=distance_->value()*.5f,angle=angle_->value()*PI/180.f;
        const float step=std::max(.15f,distance*.055f);
        const V forward{-std::sin(angle),0.f,-std::cos(angle)},right{std::cos(angle),0.f,-std::sin(angle)};
        const V previous=cameraTarget_;
        switch(key) {
            case Qt::Key_Z: cameraTarget_=cameraTarget_+forward*step; break;
            case Qt::Key_S: cameraTarget_=cameraTarget_-forward*step; break;
            case Qt::Key_Q: cameraTarget_=cameraTarget_-right*step; break;
            case Qt::Key_D: cameraTarget_=cameraTarget_+right*step; break;
            case Qt::Key_R: cameraTarget_.y+=step; break;
            case Qt::Key_X: cameraTarget_.y-=step; break;
            default: return false;
        }
        if(isPripyatScene()){
            std::lock_guard<std::mutex> guard(sceneMutex_);
            cameraTarget_=clampCameraMotion(scene_,previous,cameraTarget_,angle_->value(),elevation_->value(),distance);
        }
        beginNavigation(); readSettings(); navigationIdleTimer_->start(450); return true;
    }

    void buildUi() {
        auto* central = new QWidget(this);
        auto* root = new QHBoxLayout(central);
        root->setContentsMargins(12, 12, 12, 12);
        auto* split = new QSplitter(Qt::Horizontal, central);
        auto* left = new QWidget(split);
        auto* leftLayout = new QVBoxLayout(left);
        auto* title = new QLabel(QStringLiteral("MOTEUR 3D NATIF x64  |  ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE), left);
        title->setStyleSheet("font-size:18px;font-weight:700;color:#e0ebf3");
        auto* info = new QLabel(QStringLiteral("Scène V13.14  |  Git ") + QString::fromUtf8(M3D_GIT_REVISION_ANSI) + QStringLiteral("  |  branche ") + QString::fromUtf8(M3D_GIT_BRANCH_ANSI) + QStringLiteral("  |  rendu CPU / GPU DirectX"), left);
        info->setStyleSheet("color:#d4e1eb");
        image_ = new ImageView(left);
        auto* navigationInfo = new QLabel(QStringLiteral("Navigation : glisser gauche pour tourner, glisser droit pour se déplacer, molette pour zoomer. AZERTY : Z/Q/S/D pour avancer/gauche/reculer/droite, R/X pour monter/descendre."), left);
        navigationInfo->setWordWrap(true);
        navigationInfo->setStyleSheet("color:#b8cbd8;font-size:11px;");
        hourLabel_ = new QLabel(QStringLiteral("Heure : 08:00 — Matin"), left);
        hour_ = slider(Qt::Horizontal, 0, 32, 4, left);
        auto* hourMarks = new QHBoxLayout;
        for (const auto& label : {QStringLiteral("Matin"), QStringLiteral("Midi"), QStringLiteral("Coucher"), QStringLiteral("Nuit")}) {
            auto* l = new QLabel(label, left); l->setStyleSheet("color:#d4e1eb"); hourMarks->addWidget(l);
        }
        leftLayout->addWidget(title);
        leftLayout->addWidget(info);
        leftLayout->addWidget(image_, 1);
        leftLayout->addWidget(navigationInfo);
        leftLayout->addWidget(hourLabel_);
        leftLayout->addWidget(hour_);
        leftLayout->addLayout(hourMarks);

        auto* panel = new QWidget;
        panel->setMinimumWidth(380);
        panel->setMaximumWidth(520);
        panel->setStyleSheet("QWidget{background:#182b3c;color:#e0ebf3;} QLabel{background:transparent;} QComboBox,QPushButton{background:#f0f0f0;color:#111;padding:5px;} QSlider{background:transparent;}");
        auto* controls = new QVBoxLayout(panel);
        auto* heading = new QLabel(QStringLiteral("PILOTAGE DU RENDU"), panel);
        heading->setStyleSheet("font-size:17px;font-weight:700");
        controls->addWidget(heading);
        resolution_ = new QComboBox(panel);
        resolution_->addItems({QStringLiteral("Vostro — 1366 × 768"), QStringLiteral("Full HD — 1920 × 1080"), QStringLiteral("1600 × 900"), QStringLiteral("1280 × 720"), QStringLiteral("4K UHD — 3840 × 2160")});
        renderer_ = new QComboBox(panel);
        renderer_->addItems({QStringLiteral("Auto — DXR si renderer disponible, sinon CPU"), QStringLiteral("CPU — ray tracing logiciel complet"), QStringLiteral("GPU — DirectX 11 (aperçu raster)"), QStringLiteral("GPU — Compute D3D11 (ray tracing expérimental)")});
        renderer_->setCurrentIndex(0);
        renderer_->setToolTip(QStringLiteral("Auto : CPU tant que DXR n’est pas disponible. GPU DirectX 11 : raster. GPU Compute D3D11 : ray tracing BVH sans DXR, aperçu avec albédo moyen et éclairage simplifié. Repli CPU si erreur."));
#ifndef _WIN32
        renderer_->setItemData(2, 0, Qt::UserRole - 1);
        renderer_->setItemData(2, QStringLiteral("DirectX 11 est disponible uniquement sous Windows."), Qt::ToolTipRole);
        renderer_->setItemData(3, 0, Qt::UserRole - 1);
        renderer_->setItemData(3, QStringLiteral("GPU Compute D3D11 disponible uniquement sous Windows."), Qt::ToolTipRole);
#endif
        quality_ = new QComboBox(panel);
        quality_->addItems({QStringLiteral("Natif — 100 %"), QStringLiteral("Équilibré — 87,5 % par axe"), QStringLiteral("Détails — 75 % par axe"), QStringLiteral("Rapide — 66,7 % par axe"), QStringLiteral("Très rapide — 50 % par axe"), QStringLiteral("FSAA 16× — motif stratifié 4×4"), QStringLiteral("Haute définition — 2× interne (SSAA CPU)"), QStringLiteral("CPU hors-ligne PBR/HDR — 64 spp / 3 rebonds")});
        quality_->setToolTip(QStringLiteral("Le mode hors-ligne utilise un BRDF PBR GGX en lumière linéaire, un tone mapping ACES et un débruitage bilatéral CPU. Le temps de calcul augmente fortement avec la résolution."));
        quality_->setCurrentIndex(6);
        controls->addWidget(new QLabel(QStringLiteral("Résolution de sortie"), panel)); controls->addWidget(resolution_);
        controls->addWidget(new QLabel(QStringLiteral("Moteur de rendu"), panel)); controls->addWidget(renderer_);
        rendererStatus_ = new QLabel(panel); rendererStatus_->setWordWrap(true); controls->addWidget(rendererStatus_);
        controls->addWidget(new QLabel(QStringLiteral("Précision du calcul interne"), panel)); controls->addWidget(quality_);
        sceneViewLabel_=new QLabel(QStringLiteral("Vues du Palais Energetik"),panel);
        sceneViewSelector_=new QComboBox(panel);
        sceneViewSelector_->setToolTip(QStringLiteral("Vues documentées dans l’annexe de la scène Pripiat."));
        controls->addWidget(sceneViewLabel_); controls->addWidget(sceneViewSelector_);

        auto* form = new QFormLayout;
        distance_ = slider(Qt::Horizontal, 6, 260, 24, panel);
        angle_ = slider(Qt::Horizontal, -180, 180, -43, panel);
        elevation_ = slider(Qt::Horizontal, -5, 50, 7, panel);
        fov_ = slider(Qt::Horizontal, 35, 105, 50, panel);
        const unsigned detectedThreads = std::max(1u, std::thread::hardware_concurrency());
        const int defaultThreads = int(std::min(32u, detectedThreads));
        threads_ = slider(Qt::Horizontal, 1, 32, defaultThreads, panel);
        form->addRow(QStringLiteral("Distance caméra"), distance_);
        form->addRow(QStringLiteral("Rotation caméra"), angle_);
        form->addRow(QStringLiteral("Élévation caméra"), elevation_);
        form->addRow(QStringLiteral("Champ horizontal"), fov_);
        threadLabel_ = new QLabel(QStringLiteral("Threads CPU : 16"), panel);
        form->addRow(threadLabel_, threads_);
        simdMode_ = new QComboBox(panel);
        simdMode_->addItems({QStringLiteral("Automatique (AVX2 + FMA3 si disponibles)"), QStringLiteral("AVX2 + FMA3 activé"), QStringLiteral("Désactivé — x64 standard")});
        simdMode_->setCurrentIndex(2);
        simdMode_->setToolTip(QStringLiteral("AVX2 + FMA3 requiert le support CPU et système. Les tests précédents ont montré un ralentissement sur i5-7200U; le x64 standard reste le réglage initial en attendant une comparaison médiane après ces corrections."));
        if (!avx2FmaAvailable()) simdMode_->setItemData(1, 0, Qt::UserRole - 1);
        form->addRow(QStringLiteral("Optimisation SIMD"), simdMode_);
        controls->addLayout(form);
        auto* checks = new QHBoxLayout;
        fog_ = new QCheckBox(QStringLiteral("Brouillard atmosphérique"), panel);
        dof_ = new QCheckBox(QStringLiteral("DOF"), panel);
        dust_ = new QCheckBox(QStringLiteral("Poussière dans les rais de lumière"), panel); dust_->setChecked(true);
        edgeAa_ = new QCheckBox(QStringLiteral("AA contours"), panel); edgeAa_->setChecked(true);
        checks->addWidget(fog_); checks->addWidget(dof_); checks->addWidget(dust_); checks->addWidget(edgeAa_); checks->addStretch();
        controls->addLayout(checks);
        videoPreview_ = new QCheckBox(QStringLiteral("Mode vidéo — aperçu rotatif 800 × 600"), panel);
        controls->addWidget(videoPreview_);
        fastNavigation_ = new QCheckBox(QStringLiteral("Visite fluide — aperçu léger après déplacement"), panel);
        fastNavigation_->setToolTip(QStringLiteral("Garde un aperçu rapide pendant l’exploration. Le bouton Calculer l’image produit toujours un rendu complet."));
        fastNavigation_->setChecked(false);
        controls->addWidget(fastNavigation_);
        videoWindow_ = new VideoPreviewWindow(this);
        videoPlaybackTimer_ = new QTimer(this);
        videoPlaybackTimer_->setInterval(50);
        connect(videoPlaybackTimer_, &QTimer::timeout, this, [this] {
            if (!videoPreview_->isChecked() || !videoPlaying_ || videoFrames_.empty()) { videoPlaybackTimer_->stop(); return; }
            videoPlaybackIndex_ = (videoPlaybackIndex_ + 1) % videoFrames_.size();
            videoWindow_->setFrame(videoFrames_[videoPlaybackIndex_]);
        });
        connect(videoWindow_, &QDialog::finished, this, [this](int) {
            if (videoPreview_->isChecked()) videoPreview_->setChecked(false);
        });
        renderButton_ = new QPushButton(QStringLiteral("Calculer l’image"), panel);
        controls->addWidget(renderButton_);
        auto* views = new QHBoxLayout;
        homeButton_ = new QPushButton(QStringLiteral("Vue initiale"), panel);
        frontButton_ = new QPushButton(QStringLiteral("Vue avant"), panel);
        rearButton_ = new QPushButton(QStringLiteral("Vue arrière"), panel);
        views->addWidget(homeButton_); views->addWidget(frontButton_);
        controls->addLayout(views); controls->addWidget(rearButton_);
        nightButton_ = new QPushButton(QStringLiteral("Vue de nuit — 22 h"), panel);
        nightButton_->setToolTip(QStringLiteral("Place la caméra devant la façade et règle l’heure à 22 h pour voir les éclairages intérieurs."));
        controls->addWidget(nightButton_);
        // Metric survey panel: scene-space coordinates, 1 unit = 1 metre.
        // Input is explicit until depth-buffer picking is implemented.
        auto* surveyTitle = new QLabel(QStringLiteral("MESURES DE LA SCÈNE (m)"), panel);
        surveyTitle->setStyleSheet("font-size:15px;font-weight:700");
        controls->addWidget(surveyTitle);
        auto* surveyForm = new QFormLayout;
        QDoubleSpinBox* surveyCoordinates[6] = {};
        const QString surveyNames[6] = {
            QStringLiteral("Point A — X"), QStringLiteral("Point A — Y"), QStringLiteral("Point A — Z"),
            QStringLiteral("Point B — X"), QStringLiteral("Point B — Y"), QStringLiteral("Point B — Z")
        };
        for(int i=0;i<6;++i) {
            auto* value = new QDoubleSpinBox(panel);
            value->setRange(-1000000.0,1000000.0);
            value->setDecimals(3);
            value->setSingleStep(0.5);
            value->setSuffix(QStringLiteral(" m"));
            surveyCoordinates[i]=value;
            surveyForm->addRow(surveyNames[i],value);
        }
        controls->addLayout(surveyForm);
        auto* surveyResult = new QLabel(QStringLiteral("Saisissez les coordonnées de deux points."),panel);
        surveyResult->setWordWrap(true);
        auto* measureButton = new QPushButton(QStringLiteral("Mesurer A → B"),panel);
        controls->addWidget(measureButton);
        controls->addWidget(surveyResult);
        connect(measureButton,&QPushButton::clicked,this,[surveyCoordinates,surveyResult] {
            const double dx=surveyCoordinates[3]->value()-surveyCoordinates[0]->value();
            const double dy=surveyCoordinates[4]->value()-surveyCoordinates[1]->value();
            const double dz=surveyCoordinates[5]->value()-surveyCoordinates[2]->value();
            const double horizontal=std::hypot(dx,dz);
            const double spatial=std::hypot(horizontal,dy);
            surveyResult->setText(QStringLiteral("Horizontale : %1 m | 3D : %2 m | Dénivelé B−A : %3 m")
                .arg(horizontal,0,'f',3).arg(spatial,0,'f',3).arg(dy,0,'f',3));
        });
        auto* surveyCaution = new QLabel(QStringLiteral("Coordonnées de scène uniquement : géoréférencement historique non validé. Sélection par clic 3D à venir."),panel);
        surveyCaution->setWordWrap(true);
        controls->addWidget(surveyCaution);
        status_ = new QLabel(QStringLiteral("Prêt"), panel);
        auto* diagTitle = new QLabel(QStringLiteral("DIAGNOSTICS MESURÉS"), panel);
        diagTitle->setStyleSheet("font-size:15px;font-weight:700");
        stats_ = new QLabel(QStringLiteral("En attente du premier rendu"), panel);
        stats_->setWordWrap(true);
        controls->addWidget(status_); controls->addWidget(diagTitle); controls->addWidget(stats_); controls->addStretch();

        auto* panelScroll = new QScrollArea(split);
        panelScroll->setWidgetResizable(true);
        panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        panelScroll->setMinimumWidth(380);
        panelScroll->setMaximumWidth(520);
        panelScroll->setFrameShape(QFrame::NoFrame);
        panelScroll->setWidget(panel);
        split->addWidget(left); split->addWidget(panelScroll); split->setStretchFactor(0, 1); split->setStretchFactor(1, 0);
        root->addWidget(split);
        setCentralWidget(central);

        connect(resolution_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        connect(renderer_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        connect(quality_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        connect(sceneViewSelector_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int index) { applySceneViewPreset(index); });
        for (auto* s : {distance_, angle_, elevation_, fov_}) {
            connect(s, &QSlider::sliderReleased, this, [this] { beginNavigation(); });
            connect(s, &QSlider::valueChanged, this, [this, s](int) {
                if (s->isSliderDown()) { beginNavigation(); readSettings(); }
                else if (!navigationActive_) { readSettings(); requestRender(); }
            });
        }
        for (auto* s : {threads_, hour_}) {
            connect(s, &QSlider::sliderReleased, this, [this] { readSettings(); requestRender(); });
            connect(s, &QSlider::valueChanged, this, [this, s](int) { if (!s->isSliderDown()) { readSettings(); requestRender(); } });
        }
        connect(fog_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(dof_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(dust_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(edgeAa_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(simdMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        connect(videoPreview_, &QCheckBox::toggled, this, [this](bool enabled) {
            setVideoControlsEnabled(!enabled);
            if (enabled) {
                videoFrames_.clear();
                if (!isPripyatScene()) videoFrames_.reserve(size_t(videoFrameCount()));
                videoFrameIndex_ = videoPlaybackIndex_ = 0;
                videoAngle_ = 0.f;
                videoBuilding_ = true;
                if (isPripyatScene()) tourTimer_.start();
                videoPlaying_ = false;
                videoWindow_->setStatus(isPripyatScene()
                    ? QStringLiteral("Visite Energetik — départ extérieur, pièces, étages puis toit · 1 min 30")
                    : QStringLiteral("Calcul vidéo — image 1 / 180 — caméra 0° / 360°"));
                videoWindow_->show(); videoWindow_->raise();
            } else {
                videoPlaybackTimer_->stop();
                tourTimer_.invalidate();
                videoBuilding_ = videoPlaying_ = false;
                videoFrameIndex_ = videoPlaybackIndex_ = 0;
                std::vector<QImage>().swap(videoFrames_);
                videoAngle_ = 0.f;
                videoWindow_->hide();
                if (isPripyatScene()) applySceneCameraPreset();
            }
            readSettings();
            requestRender();
            if (enabled) status_->setText(isPripyatScene()
                ? QStringLiteral("Visite Energetik — départ extérieur, pièces, étages puis toit · 1 min 30")
                : QStringLiteral("Calcul vidéo — image 1 / 180 — caméra 0° / 360°"));
        });
        connect(fastNavigation_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(renderButton_, &QPushButton::clicked, this, [this] { forceFullQuality_=true; readSettings(); requestRender(); });
        connect(homeButton_, &QPushButton::clicked, this, [this] { applySceneCameraPreset(); readSettings(); requestRender(); });
        connect(frontButton_, &QPushButton::clicked, this, [this] {
            if (isRaptureScene()) setCameraSliders(16,180,7,48); else setCameraSliders(20,-10,4,50);
            readSettings(); requestRender();
        });
        connect(rearButton_, &QPushButton::clicked, this, [this] {
            if (isRaptureScene()) setCameraSliders(24,0,9,55); else setCameraSliders(24,-145,7,50);
            readSettings(); requestRender();
        });
        connect(nightButton_, &QPushButton::clicked, this, [this] {
            if(isPripyatScene()) {
                cameraTarget_={0.f,4.2f,-19.5f};
                setCameraSliders(72,0,1,55);
                { const QSignalBlocker hourBlock(hour_); hour_->setValue(32); }
                hourLabel_->setText(QStringLiteral("Heure : 22:00 — Nuit"));
                readSettings(); requestRender();
            }
        });
        connect(hour_, &QSlider::valueChanged, this, [this](int step) {
            float h = 6.f + step * .5f;
            QString mode = h < 9 ? QStringLiteral("Matin") : h < 16 ? QStringLiteral("Midi") : h < 20 ? QStringLiteral("Coucher de soleil") : QStringLiteral("Nuit");
            hourLabel_->setText(QStringLiteral("Heure : %1:%2 — %3").arg(int(h), 2, 10, QLatin1Char('0')).arg((step % 2) ? QStringLiteral("30") : QStringLiteral("00"), mode));
        });
        navigationPreviewTimer_ = new QTimer(this); navigationPreviewTimer_->setInterval(350);
        connect(navigationPreviewTimer_, &QTimer::timeout, this, [this] { if (navigationActive_) { readSettings(); requestRender(); } });
        navigationIdleTimer_ = new QTimer(this); navigationIdleTimer_->setSingleShot(true);
        connect(navigationIdleTimer_, &QTimer::timeout, this, [this] { finishNavigation(); });
        image_->setNavigationHandlers(
            [this](int dx,int dy,bool pan) { handleCameraDrag(dx,dy,pan); },
            [this](int delta) { handleCameraZoom(delta); },
            [this](int key) { return handleCameraKey(key); },
            [this] { navigationIdleTimer_->start(450); });
    }

    void openSceneFile() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Ouvrir une scène"), currentScenePath_, QStringLiteral("Scènes Moteur 3D (*.bin);;Tous les fichiers (*.*)"));
        if (path.isEmpty()) return;
        Scene loaded;
        std::wstring error;
#ifdef _WIN32
        const std::filesystem::path nativePath(path.toStdWString());
#else
        const QByteArray encodedPath = path.toUtf8();
        const std::filesystem::path nativePath(std::string(encodedPath.constData(), size_t(encodedPath.size())));
#endif
        if (!loadScene(nativePath, loaded, error)) {
            QMessageBox::critical(this, QStringLiteral("Ouverture de scène impossible"), QString::fromWCharArray(error.c_str()));
            return;
        }
        generation_.fetch_add(1);
        {
            std::lock_guard<std::mutex> guard(sceneMutex_);
            scene_ = std::move(loaded);
            const int hourStep = hour_->value();
            const float hour = 6.f + hourStep * .5f;
            updateLighting(hour);
            Stats build;
            shadows_ = buildShadow(scene_, build);
            shadowHour_ = hourStep;
        }
        currentScenePath_ = path;
        loadSceneViewPresets();
        applySceneCameraPreset();
        readSettings();
        requestRender();
    }

    void openBenchmark() {
        if (videoPreview_->isChecked()) return;
        if (benchmarkThread_.joinable()) benchmarkThread_.join();
        const auto objects = scene_.obj.size();
        const auto triangles = std::count_if(scene_.obj.begin(), scene_.obj.end(), [](const Obj& o) { return o.kind == Kind::Triangle; });
        BenchmarkDialog dialog(this);
        dialog.setScene(currentScenePath_, objects, size_t(triangles));
        connect(dialog.cancelButton(), &QPushButton::clicked, &dialog, [this, &dialog] {
            benchmarkGeneration_.fetch_add(1);
            dialog.setCancelling();
        });
        connect(dialog.runButton(), &QPushButton::clicked, &dialog, [this, &dialog] {
            if (benchmarkThread_.joinable()) benchmarkThread_.join();
            readSettings();
            RenderSettings cfg;
            { std::lock_guard<std::mutex> lock(requestMutex_); cfg = settings_; }
            const int baseOutW = cfg.outW, baseOutH = cfg.outH;
            switch (dialog.resolutionIndex()) {
                case 1: cfg.outW = 1280; cfg.outH = 720; break;
                case 2: cfg.outW = 1920; cfg.outH = 1080; break;
                case 3: cfg.outW = 3840; cfg.outH = 2160; break;
                default: break;
            }
            const float scaleX = float(cfg.renderW) / std::max(1, baseOutW);
            const float scaleY = float(cfg.renderH) / std::max(1, baseOutH);
            cfg.renderW = std::max(1, int(std::lround(cfg.outW * scaleX)));
            cfg.renderH = std::max(1, int(std::lround(cfg.outH * scaleY)));
            cfg.videoPreview = false;
            auto threadCounts = dialog.selectedThreads();
            std::sort(threadCounts.begin(), threadCounts.end());
            const int engineMode = dialog.requestedEngine();
            if(engineMode!=0){cfg.samples=1;cfg.dof=false;cfg.fog=false;}
            if (engineMode == 0 && threadCounts.empty()) { QMessageBox::information(&dialog, QStringLiteral("Benchmark"), QStringLiteral("Sélectionnez au moins un nombre de threads.")); return; }
            if (threadCounts.empty()) threadCounts.push_back(1);
            const int repeats = dialog.repeats();
            const bool avxAvailable = avx2FmaAvailable();
            const int selectedMode = engineMode==0 ? dialog.requestedSimdMode() : 1;
            std::vector<bool> simdModes;
            switch (selectedMode) {
                case 0: simdModes.push_back(cfg.avx2 && avxAvailable); break;
                case 1: simdModes.push_back(false); break;
                case 2:
                    if (!avxAvailable) { QMessageBox::information(&dialog, QStringLiteral("Benchmark"), QStringLiteral("AVX2 + FMA3 n’est pas disponible sur ce processeur.")); return; }
                    simdModes.push_back(true); break;
                case 3:
                    if (!avxAvailable) { QMessageBox::information(&dialog, QStringLiteral("Benchmark"), QStringLiteral("La comparaison complète nécessite AVX2 + FMA3.")); return; }
                    simdModes = {false, true}; break;
                default: simdModes.push_back(false); break;
            }
            if(engineMode!=0)simdModes={false};
            std::vector<BenchmarkPass> passes;
            if(engineMode==1)passes.push_back({0,false,true});
            else if(engineMode==2){for(unsigned n:threadCounts)passes.push_back({n,false,false});passes.push_back({0,false,true});}
            else for(unsigned n:threadCounts)for(bool avx:simdModes)passes.push_back({n,avx,false});
            const QString modeSummary = engineMode==1 ? QStringLiteral("DirectX 11 GPU")
                : engineMode==2 ? QStringLiteral("Chemins CPU / GPU (qualités différentes)")
                : selectedMode == 3 ? QStringLiteral("Comparaison complète x64 / AVX2 + FMA3")
                : selectedMode == 2 ? QStringLiteral("AVX2 + FMA3")
                : selectedMode == 1 ? QStringLiteral("x64 standard")
                : QStringLiteral("Mode choisi dans le moteur (%1)").arg(cfg.avx2 && avxAvailable ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard"));
            const QString threadSummary=engineMode==1?QStringLiteral("GPU"):([&] { QStringList v; for (unsigned n : threadCounts) v << QString::number(n); return v.join(','); }());
            const QString settingsSummary = QStringLiteral("engine=%1; output=%2x%3; internal=%4x%5; samples=%6; threads=%7; hour=%8; fog=%9; dof=%10; edge_aa=%11; camera=%12/%13/%14; fov=%15; arch=%16; logical_processors=%17")
                .arg(engineMode==1?QStringLiteral("DirectX 11"):engineMode==2?QStringLiteral("CPU/GPU non équivalent"):QStringLiteral("CPU"))
                .arg(cfg.outW).arg(cfg.outH).arg(cfg.renderW).arg(cfg.renderH).arg(cfg.samples).arg(threadSummary)
                .arg(cfg.hourStep).arg(cfg.fog ? 1 : 0).arg(cfg.dof ? 1 : 0).arg(cfg.edgeAa ? 1 : 0)
                .arg(cfg.distance, 0, 'f', 2).arg(cfg.angle, 0, 'f', 1).arg(cfg.elevation, 0, 'f', 1).arg(cfg.fov, 0, 'f', 1)
                .arg(QSysInfo::currentCpuArchitecture()).arg(std::thread::hardware_concurrency());
            dialog.setRows({}, 0, cfg.outW, cfg.outH, cfg.samples, 0, modeSummary, settingsSummary);
            dialog.setRunning(true); dialog.setProgress(0, QStringLiteral("Préparation du benchmark…"));
            uint64_t token = 0;
            {
                std::lock_guard<std::mutex> lock(requestMutex_);
                benchmarking_ = true;
                pending_ = false;
                // Publish the render cancellation and the benchmark gate atomically with
                // respect to renderLoop's pending-work check.
                generation_.fetch_add(1);
                token = benchmarkGeneration_.fetch_add(1) + 1;
            }
            requestCv_.notify_one();
            benchmarkThread_ = std::thread([this, &dialog, cfg, threadCounts, passes, repeats, token, modeSummary, settingsSummary] {
                std::vector<BenchmarkRow> rows;
                double shadowMs = 0;
                std::unique_lock<std::mutex> sceneLock(sceneMutex_);
                if (benchmarkGeneration_.load() != token) {
                    sceneLock.unlock();
                    { std::lock_guard<std::mutex> lock(requestMutex_); benchmarking_ = false; }
                    QMetaObject::invokeMethod(&dialog, [&dialog] {
                        dialog.setProgress(0, QStringLiteral("Benchmark annulé avant le début des mesures."));
                        dialog.setRunning(false);
                    }, Qt::QueuedConnection);
                    requestCv_.notify_one();
                    return;
                }
                updateLighting(6.f + cfg.hourStep * .5f);
                Stats shadowStats;
                auto shadowStart = std::chrono::steady_clock::now();
                ShadowMap benchShadows = buildShadow(scene_, shadowStats);
                shadowMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shadowStart).count();
                const int totalPasses = int(passes.size()) * (repeats + 1);
                int done = 0;
                QString benchmarkError;
                for (const BenchmarkPass& pass : passes) {
                    if (benchmarkGeneration_.load() != token || !benchmarkError.isEmpty()) break;
                    const unsigned postThreads = pass.gpu ? std::max(1u,cfg.threads) : pass.threads;
                    std::vector<double> renderTimes, postTimes, totalTimes;
                    BenchmarkRow row; row.threads = pass.threads; row.avx2 = pass.avx2; row.gpu = pass.gpu;
                    for (int repetition = -1; repetition < repeats; ++repetition) {
                        if (benchmarkGeneration_.load() != token) break;
                        std::vector<uint32_t> pixels(size_t(cfg.renderW) * cfg.renderH);
                        Camera cam = camera(cfg.angle, cfg.elevation, cfg.distance, cfg.renderW, cfg.renderH, V{cfg.targetX,cfg.targetY,cfg.targetZ});
                        cam.tanHalf = std::tan(cfg.fov * PI / 360.f); cam.focusDistance = cfg.distance;
                        cam.apertureRadius = cfg.distance * .018f;
                        cam.pixelAngle = 2 * cam.tanHalf / (cfg.renderW * (cfg.samples == 16 ? 4.f : cfg.samples == 4 ? 2.f : 1.f));
                        unsigned samples = cfg.samples; if (cfg.dof && samples < 4) samples = 4;
                        const auto start = std::chrono::steady_clock::now();
                        Stats st;
                        if(pass.gpu){std::wstring error;if(!renderDirectX11Preview(scene_,benchShadows,cam,cfg.renderW,cfg.renderH,pixels,st,error)){benchmarkError=QString::fromWCharArray(error.c_str());break;}}
                        else st=m3d::render(scene_, benchShadows, cam, cfg.renderW, cfg.renderH, pixels, pass.threads, cfg.fog, benchmarkGeneration_, token, samples, cfg.dof, pass.avx2);
                        if (benchmarkGeneration_.load() != token) break;
                        const double renderMs = st.ms;
                        const auto postStart = std::chrono::steady_clock::now();
                        if (cfg.renderW == cfg.outW * 2 && cfg.renderH == cfg.outH * 2) pixels = downsample2x(pixels, cfg.renderW, cfg.renderH, postThreads);
                        else if (cfg.renderW != cfg.outW || cfg.renderH != cfg.outH) pixels = upscaleBilinear(pixels, cfg.renderW, cfg.renderH, cfg.outW, cfg.outH, postThreads);
                        if (cfg.edgeAa) pixels = antialiasEdges4(pixels, cfg.outW, cfg.outH, postThreads, !pass.gpu && pass.avx2);
                        const double postMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postStart).count();
                        const double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                        if (repetition >= 0) {
                            renderTimes.push_back(renderMs); postTimes.push_back(postMs); totalTimes.push_back(totalMs);
                            row.rays += st.rays; row.tests += st.tests; row.nodes += st.nodes; row.aabb += st.aabb; row.hits += st.hits; row.texels += st.texels;
                            row.simdRayPackets += st.simdRayPackets; row.simdTrianglePackets += st.simdTrianglePackets; row.simdTextureSamples += st.simdTextureSamples; row.simdLightBatches += st.simdLightBatches;
                        }
                        ++done;
                        const int progress = int(100.0 * done / std::max(1, totalPasses));
                        const QString modeName = pass.gpu ? QStringLiteral("DirectX 11 GPU") : pass.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard");
                        const QString threadLabel = pass.gpu ? QStringLiteral("GPU") : QStringLiteral("%1 thread(s)").arg(pass.threads);
                        QMetaObject::invokeMethod(&dialog, [&dialog, progress, threadLabel, repetition, repeats, modeName] {
                            const QString phase = repetition < 0 ? QStringLiteral("Échauffement") : QStringLiteral("Mesure %1/%2").arg(repetition + 1).arg(repeats);
                            dialog.setProgress(progress, QStringLiteral("%1 — %2 — %3 — buffer séparé").arg(modeName,threadLabel,phase));
                        }, Qt::QueuedConnection);
                    }
                    if (renderTimes.empty()) { if(benchmarkGeneration_.load()==token&&benchmarkError.isEmpty())break; else continue; }
                    auto median = [](std::vector<double>& values) {
                        std::sort(values.begin(), values.end());
                        const size_t mid = values.size() / 2;
                        return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) * .5;
                    };
                    row.renderMs = median(renderTimes); row.postMs = median(postTimes); row.totalMs = median(totalTimes);
                    row.rays /= uint64_t(renderTimes.size()); row.tests /= uint64_t(renderTimes.size()); row.nodes /= uint64_t(renderTimes.size());
                    row.aabb /= uint64_t(renderTimes.size()); row.hits /= uint64_t(renderTimes.size()); row.texels /= uint64_t(renderTimes.size());
                    row.simdRayPackets /= uint64_t(renderTimes.size()); row.simdTrianglePackets /= uint64_t(renderTimes.size()); row.simdTextureSamples /= uint64_t(renderTimes.size()); row.simdLightBatches /= uint64_t(renderTimes.size());
                    rows.push_back(row);
                }
                sceneLock.unlock();
                { std::lock_guard<std::mutex> lock(requestMutex_); benchmarking_ = false; }
                const bool cancelled = benchmarkGeneration_.load() != token;
                QMetaObject::invokeMethod(&dialog, [this, &dialog, rows = std::move(rows), shadowMs, cfg, repeats, modeSummary, settingsSummary, cancelled, benchmarkError] () mutable {
                    dialog.setRows(std::move(rows), shadowMs, cfg.outW, cfg.outH, cfg.samples, unsigned(repeats), modeSummary, settingsSummary);
                    if (!benchmarkError.isEmpty()) dialog.setProgress(0, QStringLiteral("Échec du benchmark DirectX 11 : %1").arg(benchmarkError));
                    else if (cancelled) dialog.setProgress(0, QStringLiteral("Benchmark annulé — résultats partiels conservés. Ombres : %1 ms.").arg(shadowMs, 0, 'f', 1));
                    else dialog.setProgress(100, QStringLiteral("Terminé — médianes par mode et threads. Ombres (passe unique) : %1 ms.").arg(shadowMs, 0, 'f', 1));
                    dialog.setRunning(false);
                    requestCv_.notify_one();
                }, Qt::QueuedConnection);
            });
        });
        dialog.exec();
    }

    void setVideoControlsEnabled(bool enabled) {
        resolution_->setEnabled(enabled); renderer_->setEnabled(enabled); quality_->setEnabled(enabled);
        distance_->setEnabled(enabled); angle_->setEnabled(enabled); elevation_->setEnabled(enabled); fov_->setEnabled(enabled);
        threads_->setEnabled(enabled && !settings_.directX11); hour_->setEnabled(enabled); simdMode_->setEnabled(enabled && !settings_.directX11);
        fog_->setEnabled(enabled && !settings_.directX11); dof_->setEnabled(enabled && !settings_.directX11); edgeAa_->setEnabled(enabled);
        renderButton_->setEnabled(enabled); homeButton_->setEnabled(enabled); frontButton_->setEnabled(enabled); rearButton_->setEnabled(enabled);
        nightButton_->setEnabled(enabled && isPripyatScene());
        fastNavigation_->setEnabled(enabled && isPripyatScene());
        image_->setEnabled(enabled);
        menuBar()->setEnabled(enabled);
    }

    void readSettings() {
        std::lock_guard<std::mutex> lock(requestMutex_);
        const int ri = resolution_->currentIndex();
        const bool video = videoPreview_->isChecked();
        settings_.outW = video ? 800 : ri == 0 ? 1366 : ri == 1 ? 1920 : ri == 2 ? 1600 : ri == 3 ? 1280 : 3840;
        settings_.outH = video ? 600 : ri == 0 ? 768 : ri == 1 ? 1080 : ri == 2 ? 900 : ri == 3 ? 720 : 2160;
        const int qi = quality_->currentIndex();
        settings_.offlinePathTrace = qi == 7;
        settings_.directX11 = renderer_->currentIndex() == 2 && !settings_.offlinePathTrace;
        settings_.compute11 = renderer_->currentIndex() == 3 && !settings_.offlinePathTrace;
        if (rendererStatus_ && qi == 7) rendererStatus_->setText(QStringLiteral("CPU hors-ligne PBR/HDR — 64 spp, 3 rebonds et débruitage."));
        else if (rendererStatus_ && settings_.compute11) rendererStatus_->setText(QStringLiteral("GPU Compute D3D11 : ray tracing BVH, atlas PBR et ciel. GPU sans DXR, repli CPU si échec."));
        if (navigationPreviewTimer_) navigationPreviewTimer_->setInterval(settings_.directX11 ? 50 : 350);
        threads_->setEnabled(!(settings_.directX11 || settings_.compute11));
        simdMode_->setEnabled(!(settings_.directX11 || settings_.compute11));
        fog_->setEnabled(!(settings_.directX11 || settings_.compute11));
        dof_->setEnabled(!(settings_.directX11 || settings_.compute11));
        const float scale = qi == 7 ? 1.f : qi == 6 && !(settings_.directX11 || settings_.compute11) ? 2.f : qi == 0 ? 1.f : qi == 1 ? .875f : qi == 2 ? .75f : qi == 3 ? (2.f / 3.f) : qi == 4 ? .5f : 1.f;
        settings_.renderW = int(std::lround(settings_.outW * scale));
        settings_.renderH = int(std::lround(settings_.outH * scale));
        settings_.samples = qi == 7 ? 64u : qi == 5 ? 16u : 1u;
        settings_.edgeAa = edgeAa_->isChecked();
        settings_.videoPreview = video;
        settings_.targetX=cameraTarget_.x; settings_.targetY=cameraTarget_.y; settings_.targetZ=cameraTarget_.z;
        const bool explicitFullQuality=forceFullQuality_;
        forceFullQuality_=false;
        settings_.navigationPreview=!explicitFullQuality && (navigationActive_ || (fastNavigation_ && fastNavigation_->isChecked() && !video));
        if(settings_.navigationPreview) {
            const float previewW=settings_.directX11?1024.f:480.f,previewH=settings_.directX11?576.f:270.f;
            const float scale=std::min(1.f,std::min(previewW/std::max(1,settings_.outW),previewH/std::max(1,settings_.outH)));
            settings_.renderW=std::max(1,int(std::lround(settings_.outW*scale)));
            settings_.renderH=std::max(1,int(std::lround(settings_.outH*scale)));
            settings_.samples=1u; settings_.edgeAa=false; settings_.offlinePathTrace=false;
        }
        if(video && isPripyatScene()) {
            // Keep the guided preview at its advertised 800x600 resolution on
            // both backends. It is slower on CPU, but avoids enlarging a tiny
            // 320x240 image into the full video window.
            settings_.renderW=800;
            settings_.renderH=600;
            settings_.samples=1u; settings_.edgeAa=false; settings_.offlinePathTrace=false;
        }
        settings_.distance = distance_->value() * .5f;
        settings_.angle = float(angle_->value());
        settings_.elevation = float(elevation_->value());
        if (video && isPripyatScene()) {
            V tourTarget{};
            // Advance per completed frame, not by wall clock. A slow CPU must
            // not skip waypoints while it is still rendering the previous one.
            const float progress=std::clamp(float(videoFrameIndex_)/float(videoFrameCount()),0.f,1.f);
            const CameraPose pose=guidedPripyatTour(progress,tourTarget);
            settings_.distance=pose.distance;
            settings_.angle=pose.angle;
            settings_.elevation=pose.elevation;
            // This fixed route is laid through the clear doorway centres.
            // Sweeping the look target against walls misplaces the camera by
            // its orbit distance and strands it at the entrance partition.
            cameraTarget_=tourTarget;
        } else if (video) {
            // Preserve the established orbit preview for all existing scenes.
            const float orbitDistance=isRaptureScene()?std::max(settings_.distance,18.f):settings_.distance;
            const CameraPose pose = ellipseOrbitPose(orbitDistance, settings_.angle, settings_.elevation, videoAngle_);
            settings_.distance = pose.distance;
            settings_.angle = pose.angle;
            settings_.elevation = pose.elevation;
        }
        settings_.fov = float(fov_->value());
        settings_.threads = unsigned(threads_->value());
        const int simdSelection = simdMode_->currentIndex();
        settings_.avx2 = avx2FmaAvailable() && (simdSelection == 1 || simdSelection == 0);
        settings_.hourStep = (video && isPripyatScene()) ? 6 : hour_->value();
        settings_.fog = fog_->isChecked();
        settings_.dof = dof_->isChecked() && !settings_.navigationPreview && !video;
        settings_.dust = dust_->isChecked();
        threadLabel_->setText(settings_.compute11
            ? QStringLiteral("D3D11 Compute — ray tracing expérimental")
            : settings_.directX11
            ? QStringLiteral("DirectX 11 — affichage temps réel")
            : QStringLiteral("Threads CPU : %1 — SIMD : %2").arg(settings_.threads).arg(settings_.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard")));
    }

    void queueNextVideoFrame() {
        if (!videoPreview_->isChecked() || !videoBuilding_) return;
        const int frameCount=videoFrameCount();
        if(isPripyatScene()){
            const qint64 elapsed=std::max<qint64>(0,tourTimer_.elapsed());
            if(videoFrameIndex_>=size_t(frameCount)){
                videoBuilding_=false;
                videoWindow_->setStatus(QStringLiteral("Visite terminée — pause sur le toit"));
                status_->setText(QStringLiteral("Visite Energetik terminée — 1 min 30"));
                return;
            }
            // 360 route samples over 90 s gives a 250 ms target cadence. If a
            // frame takes longer, the tour slows down instead of jumping ahead.
            const qint64 targetElapsed=qint64(videoFrameIndex_)*90000/frameCount;
            if(elapsed<targetElapsed){
                QTimer::singleShot(int(targetElapsed-elapsed),this,[this]{queueNextVideoFrame();});
                return;
            }
            const int seconds=int(targetElapsed/1000);
            videoWindow_->setStatus(QStringLiteral("Energetik — extérieur → intérieur → toit — %1:%2 / 1:30")
                .arg(seconds/60).arg(seconds%60,2,10,QChar('0')));
        } else if(videoFrameIndex_>=size_t(frameCount)){
            videoBuilding_=false;
            videoPlaying_=true;
            videoPlaybackIndex_=0;
            videoWindow_->setFrame(videoFrames_.front());
            status_->setText(QStringLiteral("Mode vidéo — boucle de 360° pré-calculée"));
            videoPlaybackTimer_->start();
            return;
        }
        videoAngle_=isPripyatScene()?0.f:float(videoFrameIndex_*2);
        if(!isPripyatScene()){
            const QString progress=QStringLiteral("Calcul vidéo — image %1 / %2 — caméra %3° / 360°")
                .arg(qulonglong(videoFrameIndex_+1)).arg(frameCount).arg(int(videoAngle_));
            videoWindow_->setStatus(progress);
            status_->setText(progress);
        }
        readSettings();
        requestRender();
    }

    void requestRender() {
        generation_.fetch_add(1);
        bool gpu=false;
        { std::lock_guard<std::mutex> lock(requestMutex_); pending_ = true; gpu=settings_.directX11||settings_.compute11; }
        status_->setText(gpu ? QStringLiteral("Aperçu GPU DirectX 11 en cours…") : QStringLiteral("Rendu CPU en cours…"));
        requestCv_.notify_one();
    }

    void renderLoop() {
        for (;;) {
            std::unique_lock<std::mutex> lock(requestMutex_);
            requestCv_.wait(lock, [this] { return stopping_ || (pending_ && !benchmarking_); });
            if (stopping_) return;
            pending_ = false;
            const uint64_t token = generation_.load();
            const RenderSettings cfg = settings_;
            lock.unlock();
            std::unique_lock<std::mutex> sceneLock(sceneMutex_);

            const int hourValue = 6 + cfg.hourStep / 2;
            const float hour = 6.f + cfg.hourStep * .5f;
            double shadowMs = 0;
            if (cfg.hourStep != shadowHour_) {
                updateLighting(hour);
                Stats lightStats; lightStats.avx2 = cfg.avx2;
                const auto start = std::chrono::steady_clock::now();
                shadows_ = buildShadow(scene_, lightStats);
                shadowMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                shadowHour_ = cfg.hourStep;
            }

            std::vector<uint32_t> pixels(size_t(cfg.renderW) * cfg.renderH);
            Camera cam = camera(cfg.angle, cfg.elevation, cfg.distance, cfg.renderW, cfg.renderH, V{cfg.targetX,cfg.targetY,cfg.targetZ});
            cam.tanHalf = std::tan(cfg.fov * PI / 360.f);
            cam.focusDistance = cfg.distance;
            cam.apertureRadius = cfg.distance * .018f;
            cam.pixelAngle = 2 * cam.tanHalf / (cfg.renderW * (cfg.samples == 16 ? 4.f : cfg.samples == 4 ? 2.f : 1.f));
            unsigned samples = cfg.samples;
            if (cfg.dof && samples < 4) samples = 4;
            Stats st{};
            QString fallbackNotice;
            if (cfg.compute11) {
                std::wstring gpuError;
                if (!renderCompute11Preview(scene_, cam, cfg.renderW, cfg.renderH, pixels, st, gpuError)) {
                    fallbackNotice = QStringLiteral("Compute GPU indisponible — rendu CPU de secours : ") + QString::fromWCharArray(gpuError.c_str());
                    st = m3d::render(scene_, shadows_, cam, cfg.renderW, cfg.renderH, pixels, cfg.threads, cfg.fog, generation_, token, samples, cfg.dof, cfg.avx2, cfg.offlinePathTrace, cfg.pathBounces);
                }
            } else if (cfg.directX11) {
                std::wstring gpuError;
                if (!renderDirectX11Preview(scene_, shadows_, cam, cfg.renderW, cfg.renderH, pixels, st, gpuError)) {
                    fallbackNotice = QStringLiteral("DirectX indisponible — rendu CPU de secours : ") + QString::fromWCharArray(gpuError.c_str());
                    st = m3d::render(scene_, shadows_, cam, cfg.renderW, cfg.renderH, pixels, cfg.threads, cfg.fog, generation_, token, samples, cfg.dof, cfg.avx2, cfg.offlinePathTrace, cfg.pathBounces);
                }
            } else {
                st = m3d::render(scene_, shadows_, cam, cfg.renderW, cfg.renderH, pixels, cfg.threads, cfg.fog, generation_, token, samples, cfg.dof, cfg.avx2, cfg.offlinePathTrace, cfg.pathBounces);
            }
            if (generation_.load() != token) continue;
            const auto postStart = std::chrono::steady_clock::now();
            if (cfg.renderW == cfg.outW * 2 && cfg.renderH == cfg.outH * 2) pixels = downsample2x(pixels, cfg.renderW, cfg.renderH, cfg.threads);
            else if (cfg.renderW != cfg.outW || cfg.renderH != cfg.outH) pixels = upscaleBilinear(pixels, cfg.renderW, cfg.renderH, cfg.outW, cfg.outH, cfg.threads);
            if (cfg.offlinePathTrace) pixels = denoiseBilateral(pixels, cfg.outW, cfg.outH, cfg.threads);
            if (cfg.edgeAa) pixels = antialiasEdges4(pixels, cfg.outW, cfg.outH, cfg.threads, cfg.avx2);
            if (cfg.dust && isPripyatScene()) applyDustOverlay(scene_, cam, cfg.outW, cfg.outH, cfg.hourStep, pixels);
            st.postMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postStart).count();
            st.shadowMs = shadowMs;
            st.ms += st.postMs + shadowMs;
            if (generation_.load() != token) continue;

            QImage image(reinterpret_cast<const uchar*>(pixels.data()), cfg.outW, cfg.outH, cfg.outW * int(sizeof(uint32_t)), QImage::Format_ARGB32);
            image = image.copy();
            const auto count = std::count_if(scene_.obj.begin(), scene_.obj.end(), [](const Obj& o) { return o.kind == Kind::Triangle; });
            const auto objectCount = scene_.obj.size();
            sceneLock.unlock();
            QString diag;
            if (st.gpu && cfg.compute11) {
                diag = QStringLiteral("GPU Compute D3D11 — ray tracing expérimental | calcul %1 ms | total %2 ms\nAlbédo, normales et rugosité échantillonnés sur GPU; ombres, GI et réfraction en développement.").arg(st.ms-st.postMs,0,'f',1).arg(st.ms,0,'f',1);
            } else if (st.gpu) {
                diag = QStringLiteral("GPU DirectX 11 — aperçu raster | rendu %1 ms | post %2 ms | total %3 ms\nGéométrie rasterisée : %4 triangles\nCarte d’ombres CPU réutilisée; DOF, réflexions et réfraction restent simplifiées.")
                    .arg(st.ms - st.postMs, 0, 'f', 1).arg(st.postMs, 0, 'f', 1).arg(st.ms, 0, 'f', 1).arg(qulonglong(st.tests));
            } else {
                diag = QStringLiteral("CPU %1 | rendu %2 ms | ombres %3 ms | post %4 ms | total %5 ms\nRayons : %6 | impacts : %7 | ciel : %8\nTests : %9 | nœuds BVH : %10 | AABB : %11\nObjets : %12 | triangles : %13 | texels : %14")
                    .arg(st.threads).arg(st.ms - st.postMs - st.shadowMs, 0, 'f', 1).arg(st.shadowMs, 0, 'f', 1).arg(st.postMs, 0, 'f', 1).arg(st.ms, 0, 'f', 1)
                    .arg(qulonglong(st.rays)).arg(qulonglong(st.hits)).arg(qulonglong(st.sky)).arg(qulonglong(st.tests)).arg(qulonglong(st.nodes)).arg(qulonglong(st.aabb))
                    .arg(qulonglong(objectCount)).arg(qulonglong(count)).arg(qulonglong(st.texels));
            }
            if (!fallbackNotice.isEmpty()) diag.prepend(fallbackNotice + QStringLiteral("\n"));
            QMetaObject::invokeMethod(this, [this, image = std::move(image), diag, hourValue, token, videoFrame = cfg.videoPreview] {
                if (generation_.load() != token || videoPreview_->isChecked() != videoFrame) return;
                stats_->setText(diag);
                if (videoFrame) {
                    if (!videoBuilding_) return;
                    if (isPripyatScene()) {
                        // Display each newly rendered waypoint immediately and
                        // continue the guided camera path at the renderer's pace.
                        videoWindow_->setFrame(image);
                        ++videoFrameIndex_;
                        queueNextVideoFrame();
                    } else {
                        videoFrames_.push_back(image);
                        ++videoFrameIndex_;
                        queueNextVideoFrame();
                    }
                } else {
                    lastRenderedFrame_=image;
                    image_->setFrame(image);
                    if(detachedRender_ && detachedRender_->isVisible()) detachedRender_->setFrame(image);
                    status_->setText(QStringLiteral("Terminé — ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE) + QStringLiteral(" — ") + QString::number(hourValue) + QStringLiteral(" h"));
                }
            }, Qt::QueuedConnection);
        }
    }
};
}

int main(int argc, char** argv) {
    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--validate-compute-hlsl")) {
        std::wstring error;
        if (m3d::validateCompute11Shader(error)) {
            std::cout << "HLSL Compute Shader Model 5.0: compilation OK" << std::endl;
            return 0;
        }
        std::wcerr << L"Erreur de validation HLSL : " << error << std::endl;
        return 12;
    }
    // Execute the real D3D11 compute shader on a tiny synthetic BVH.
    // WARP is selected only when M3D_COMPUTE_WARP=1 (Windows CI).
    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--smoke-compute-hlsl")) {
        m3d::Scene minimal;
        m3d::Obj box{};
        box.kind=m3d::Kind::Box;
        box.mat=0;
        box.mn={-1.f,-.95f,-4.f};box.mx={1.f,.95f,-2.f};
        minimal.obj.push_back(box);
        m3d::Node leaf{};
        leaf.mn=box.mn;leaf.mx=box.mx;leaf.start=0;leaf.count=1;
        minimal.node.push_back(leaf);
        minimal.order.push_back(0);minimal.root=0;
        m3d::Material mat{};
        mat.name="concrete";
        m3d::ColorLevel albedo{};
        albedo.size=512;
        albedo.p.assign(512u*512u,0x00a3b27du);
        mat.colorMip.push_back(std::move(albedo));
        minimal.mat.push_back(std::move(mat));
        m3d::updateLighting(12.f);
        m3d::Camera cam=m3d::camera(0.f,0.f,5.f,128,72,{0.f,0.f,-3.f});
        std::vector<uint32_t> pixels;
        m3d::Stats stats{};
        std::wstring error;
        if(!m3d::renderCompute11Preview(minimal,cam,128,72,pixels,stats,error)) {
            std::wcerr << L"Compute execution smoke FAILED: "<<error<<std::endl;
            return 14;
        }
        if(pixels.size()!=128u*72u || !stats.gpu ||
           pixels[36u*128u+64u]==pixels[0] ||
           pixels[36u*128u+64u]==0xffff00ffu) {
            std::cerr << "Compute execution smoke produced incorrect framebuffer" << std::endl;
            return 15;
        }
        if(argc>=3) {
            QImage image(reinterpret_cast<const uchar*>(pixels.data()),128,72,
                         128*int(sizeof(uint32_t)),QImage::Format_ARGB32);
            if(!image.copy().save(QString::fromLocal8Bit(argv[2]),"PNG"))return 16;
        }
        std::cout << "D3D11 compute BVH + PBR textures + 8bit readback: OK (" << stats.ms << " ms)" << std::endl;
        return 0;
    }
    // Non-GUI CPU snapshots for reproducible GitHub Actions geometry inspection.
    // Usage: moteur3d_qt.exe --snapshot <view 1..13> <output.png>
    if (argc >= 4 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--snapshot")) {
        QCoreApplication app(argc, argv);
        bool ok = false;
        const int view = QString::fromLocal8Bit(argv[2]).toInt(&ok);
        QFile file(QCoreApplication::applicationDirPath()+QStringLiteral("/scene_pripyat_views.json"));
        if (!ok || view < 1 || view > 13 || !file.open(QIODevice::ReadOnly)) return 2;
        QJsonParseError parseError{};
        const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),&parseError);
        if(parseError.error!=QJsonParseError::NoError || !doc.isObject()) return 3;
        const QJsonArray views=doc.object().value(QStringLiteral("views")).toArray();
        if(view>views.size()) return 4;
        const QJsonObject p=views.at(view-1).toObject();
        const QJsonArray t=p.value(QStringLiteral("target")).toArray();
        if(t.size()!=3) return 5;
        Scene scene;
        std::wstring error;
        const std::filesystem::path path(QCoreApplication::applicationDirPath().toStdWString()
                                          + std::wstring(L"/scene_pripyat_central.bin"));
        if(!loadScene(path,scene,error)) return 6;
        updateLighting(6.0f+float(p.value(QStringLiteral("hour_step")).toInt(12))*0.5f);
        Stats shadowStats{};
        const ShadowMap shadow=buildShadow(scene,shadowStats);
        constexpr int width=640,height=360;
        const float angle=float(p.value(QStringLiteral("angle")).toDouble());
        const float elevation=float(p.value(QStringLiteral("elevation")).toDouble());
        const float distance=float(p.value(QStringLiteral("distance")).toDouble(9.0));
        const float fov=float(p.value(QStringLiteral("fov")).toDouble(60.0));
        const V target{float(t.at(0).toDouble()),float(t.at(1).toDouble()),
                       float(t.at(2).toDouble())};
        Camera cam=camera(angle,elevation,distance,width,height,target);
        cam.tanHalf=std::tan(fov*PI/360.f);
        cam.focusDistance=distance;
        cam.pixelAngle=2*cam.tanHalf/float(width);
        // m3d::render writes directly to pixels[index], so allocate the full
        // framebuffer before dispatching threads (same as the GUI render loop).
        std::vector<uint32_t> pixels(size_t(width)*size_t(height));
        const std::atomic<uint64_t> generation{1};
        m3d::render(scene,shadow,cam,width,height,pixels,4,false,generation,1,1,false,false);
        if(pixels.size()!=size_t(width)*size_t(height)) return 7;
        QImage image(reinterpret_cast<const uchar*>(pixels.data()),
                     width,height,width*int(sizeof(uint32_t)),QImage::Format_ARGB32);
        return image.copy().save(QString::fromLocal8Bit(argv[3]),"PNG")?0:8;
    }
    QApplication app(argc, argv);
    const QString defaultScenePath = QCoreApplication::applicationDirPath() + QStringLiteral("/scene_pripyat_central.bin");
    const QString scenePath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultScenePath;
    Scene scene;
    std::wstring error;
#ifdef _WIN32
    const std::filesystem::path nativePath(scenePath.toStdWString());
#else
    const QByteArray encodedPath = scenePath.toUtf8();
    const std::filesystem::path nativePath(std::string(encodedPath.constData(), size_t(encodedPath.size())));
#endif
    if (!loadScene(nativePath, scene, error)) {
        QMessageBox::critical(nullptr, QStringLiteral("Moteur 3D natif"), QString::fromWCharArray(error.c_str()));
        return 1;
    }
    MainWindow window(std::move(scene), scenePath);
    window.show();
    return app.exec();
}
