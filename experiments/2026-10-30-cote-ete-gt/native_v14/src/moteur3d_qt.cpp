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
#include <QFile>
#include <QFormLayout>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
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
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>

namespace {
using namespace m3d;

struct RenderSettings {
    int outW = 1920, outH = 1080;
    int renderW = 1920, renderH = 1080;
    float distance = 12.f, angle = -43.f, elevation = 7.f, fov = 50.f;
    float targetX = 0.f, targetY = 1.f, targetZ = -8.f;
    bool navigationPreview = false;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency()), samples = 1;
    bool avx2 = false;
    int hourStep = 12;
    bool fog = false, dof = false, edgeAa = true, videoPreview = false;
};

struct CameraPose { float distance, angle, elevation; };

struct BenchmarkRow {
    unsigned threads = 1;
    bool avx2 = false;
    double renderMs = 0, postMs = 0, totalMs = 0;
    uint64_t rays = 0, tests = 0, nodes = 0, aabb = 0, hits = 0, texels = 0;
};

class BenchmarkDialog final : public QDialog {
public:
    explicit BenchmarkDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Benchmark — moteur 3D CPU"));
        resize(1040, 680);
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
        auto* threads = new QHBoxLayout;
        threads->addWidget(new QLabel(QStringLiteral("Comparer les threads :"), this));
        for (unsigned n : {1u, 2u, 4u, 8u, 16u, 32u}) {
            auto* check = new QCheckBox(QString::number(n), this); check->setChecked(n <= 8);
            threadChecks_.push_back({n, check}); threads->addWidget(check);
        }
        threads->addStretch(); root->addLayout(threads);
        progress_ = new QProgressBar(this); progress_->setRange(0, 100); progress_->setValue(0);
        status_ = new QLabel(QStringLiteral("Prêt — chaque mode garde les mêmes scène, caméra, qualité et effets."), this);
        root->addWidget(status_); root->addWidget(progress_);
        table_ = new QTableWidget(0, 7, this);
        table_->setHorizontalHeaderLabels({QStringLiteral("Mode SIMD"), QStringLiteral("Threads"), QStringLiteral("Rendu médian (ms)"), QStringLiteral("Post (ms)"), QStringLiteral("Total (ms)"), QStringLiteral("Gain SIMD"), QStringLiteral("Débit relatif (× réf.)")});
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
    int requestedSimdMode() const { return simdMode_->currentIndex(); }
    void setRunning(bool running) {
        running_ = running; run_->setEnabled(!running); export_->setEnabled(!running && !rows_.empty());
        cancel_->setEnabled(running);
        resolution_->setEnabled(!running); repeats_->setEnabled(!running); simdMode_->setEnabled(!running);
        for (const auto& item : threadChecks_) item.second->setEnabled(!running);
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
        const auto baseIt = std::find_if(rows_.begin(), rows_.end(), [](const BenchmarkRow& r) { return !r.avx2; });
        const BenchmarkRow& referenceRow = (baseIt == rows_.end() ? rows_.front() : *baseIt);
        const double referenceMs = referenceRow.totalMs;
        double maxRelative = 1.0;
        for (const auto& row : rows_) if (row.totalMs > 0) maxRelative = std::max(maxRelative, referenceMs / row.totalMs);
        QStringList modesMeasured;
        for (const auto& row : rows_) { const QString name = row.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard"); if (!modesMeasured.contains(name)) modesMeasured << name; }
        for (int i = 0; i < int(rows_.size()); ++i) {
            const auto& row = rows_[size_t(i)];
            const QString modeName = row.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard");
            const auto standardIt = std::find_if(rows_.begin(), rows_.end(), [&row](const BenchmarkRow& r) { return !r.avx2 && r.threads == row.threads; });
            const QString simdGain = row.avx2 && standardIt != rows_.end()
                ? QStringLiteral("%1×").arg(standardIt->totalMs / std::max(.001, row.totalMs), 0, 'f', 2)
                : QStringLiteral("—");
            const double relative = referenceMs / std::max(.001, row.totalMs);
            const QStringList values{modeName, QString::number(row.threads), QString::number(row.renderMs, 'f', 1),
                QString::number(row.postMs, 'f', 1), QString::number(row.totalMs, 'f', 1), simdGain, QString()};
            for (int c = 0; c < 6; ++c) table_->setItem(i, c, new QTableWidgetItem(values[c]));
            auto* bar = new QProgressBar(table_); bar->setRange(0, 100);
            bar->setValue(int(std::lround(100.0 * relative / maxRelative)));
            bar->setFormat(QStringLiteral("%1×").arg(relative, 0, 'f', 2));
            bar->setToolTip(QStringLiteral("Référence : %1, %2 thread(s)").arg(referenceRow.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard")).arg(referenceRow.threads));
            bar->setStyleSheet("QProgressBar{border:1px solid #426276;border-radius:3px;text-align:center;color:#f1f6f9;background:#102330;min-width:145px;} QProgressBar::chunk{background:#12a86b;}");
            table_->setCellWidget(i, 6, bar);
        }
        QStringList advice{QStringLiteral("%1 répétitions mesurées par configuration, médiane affichée; échauffement exclu. Ombres : %2 ms pour la passe unique.")
            .arg(repetitionsMeasured_).arg(shadowMs_, 0, 'f', 1)};
        if (modesMeasured.size() > 1) {
            for (const auto& row : rows_) if (row.avx2) {
                const auto ref = std::find_if(rows_.begin(), rows_.end(), [&row](const BenchmarkRow& r) { return !r.avx2 && r.threads == row.threads; });
                if (ref != rows_.end()) advice << QStringLiteral("À %1 threads, AVX2 + FMA3 : %2× par rapport à x64 standard (%3 ms contre %4 ms).")
                    .arg(row.threads).arg(ref->totalMs / std::max(.001, row.totalMs), 0, 'f', 2).arg(row.totalMs, 0, 'f', 1).arg(ref->totalMs, 0, 'f', 1);
            }
        }
        for (const QString& modeName : modesMeasured) {
            std::vector<const BenchmarkRow*> group;
            for (const auto& row : rows_) if ((row.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard")) == modeName) group.push_back(&row);
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
    QComboBox *resolution_ = nullptr, *simdMode_ = nullptr; QSpinBox* repeats_ = nullptr;
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
        out << "simd;threads;render_median_ms;post_median_ms;total_median_ms;speedup_vs_reference;simd_gain_vs_x64_same_threads;rays;BVH_tests;BVH_nodes;AABB_tests;hits;texels\n";
        const auto baseIt = std::find_if(rows_.begin(), rows_.end(), [](const BenchmarkRow& r) { return !r.avx2; });
        const double referenceMs = rows_.empty() ? 0 : (baseIt == rows_.end() ? rows_.front() : *baseIt).totalMs;
        for (const auto& r : rows_) {
            const QString modeName = r.avx2 ? QStringLiteral("avx2_fma3") : QStringLiteral("x64");
            const double relative = referenceMs / std::max(.001, r.totalMs);
            const auto standard = std::find_if(rows_.begin(), rows_.end(), [&r](const BenchmarkRow& x) { return !x.avx2 && x.threads == r.threads; });
            const QString simdGain = r.avx2 && standard != rows_.end() ? QString::number(standard->totalMs / std::max(.001, r.totalMs), 'f', 4) : QString();
            out << modeName << ';' << r.threads << ';' << QString::number(r.renderMs, 'f', 3) << ';'
                << QString::number(r.postMs, 'f', 3) << ';' << QString::number(r.totalMs, 'f', 3) << ';'
                << QString::number(relative, 'f', 4) << ';' << simdGain << ';' << r.rays << ';' << r.tests << ';' << r.nodes << ';'
                << r.aabb << ';' << r.hits << ';' << r.texels << '\n';
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

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(Scene loadedScene, QString scenePath) : scene_(std::move(loadedScene)), currentScenePath_(std::move(scenePath)) {
        setWindowTitle(QStringLiteral("Moteur 3D natif x64 — ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE));
        resize(1480, 900);
        setMinimumSize(1000, 620);
        buildUi();
        applySceneCameraPreset();
        readSettings();
        auto* fileMenu = menuBar()->addMenu(QStringLiteral("Scène"));
        auto* openScene = fileMenu->addAction(QStringLiteral("Ouvrir une scène…"));
        connect(openScene, &QAction::triggered, this, [this] { openSceneFile(); });
        auto* toolsMenu = menuBar()->addMenu(QStringLiteral("Outils"));
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
    QLabel *status_ = nullptr, *stats_ = nullptr, *hourLabel_ = nullptr, *threadLabel_ = nullptr;
    QComboBox *resolution_ = nullptr, *quality_ = nullptr, *simdMode_ = nullptr;
    QSlider *distance_ = nullptr, *angle_ = nullptr, *elevation_ = nullptr, *fov_ = nullptr;
    QSlider *threads_ = nullptr, *hour_ = nullptr;
    QCheckBox *fog_ = nullptr, *dof_ = nullptr, *edgeAa_ = nullptr, *videoPreview_ = nullptr;
    QPushButton *renderButton_ = nullptr, *homeButton_ = nullptr, *frontButton_ = nullptr, *rearButton_ = nullptr;
    VideoPreviewWindow* videoWindow_ = nullptr;
    QAction* benchmarkAction_ = nullptr;
    QTimer* videoPlaybackTimer_ = nullptr;
    std::vector<QImage> videoFrames_;
    size_t videoFrameIndex_ = 0, videoPlaybackIndex_ = 0;
    bool videoBuilding_ = false, videoPlaying_ = false;
    float videoAngle_ = 0.f;
    V cameraTarget_{0.f,1.f,-8.f};
    bool navigationActive_ = false;
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

    void setCameraSliders(int distanceHalf,int angle,int elevation,int fov) {
        const QSignalBlocker b0(distance_),b1(angle_),b2(elevation_),b3(fov_);
        distance_->setValue(distanceHalf); angle_->setValue(angle); elevation_->setValue(elevation); fov_->setValue(fov);
    }

    void applySceneCameraPreset() {
        navigationActive_=false;
        if(navigationPreviewTimer_) navigationPreviewTimer_->stop();
        if(navigationIdleTimer_) navigationIdleTimer_->stop();
        if(isRaptureScene()) { cameraTarget_={0.f,4.4f,-35.f}; setCameraSliders(28,180,10,52); }
        else { cameraTarget_={0.f,1.f,-8.f}; setCameraSliders(24,-43,7,50); }
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
            cameraTarget_=cameraTarget_+right*(-float(dx)*scale)+up*(float(dy)*scale);
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
        switch(key) {
            case Qt::Key_Z: cameraTarget_=cameraTarget_+forward*step; break;
            case Qt::Key_S: cameraTarget_=cameraTarget_-forward*step; break;
            case Qt::Key_Q: cameraTarget_=cameraTarget_-right*step; break;
            case Qt::Key_D: cameraTarget_=cameraTarget_+right*step; break;
            case Qt::Key_R: cameraTarget_.y+=step; break;
            case Qt::Key_X: cameraTarget_.y-=step; break;
            default: return false;
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
        auto* info = new QLabel(QStringLiteral("Scène V13.14  |  Git ") + QString::fromUtf8(M3D_GIT_REVISION_ANSI) + QStringLiteral("  |  branche ") + QString::fromUtf8(M3D_GIT_BRANCH_ANSI) + QStringLiteral("  |  rendu CPU parallèle"), left);
        info->setStyleSheet("color:#d4e1eb");
        image_ = new ImageView(left);
        auto* navigationInfo = new QLabel(QStringLiteral("Navigation : glisser gauche pour tourner, glisser droit pour se déplacer, molette pour zoomer. AZERTY : Z/Q/S/D pour avancer/gauche/reculer/droite, R/X pour monter/descendre."), left);
        navigationInfo->setWordWrap(true);
        navigationInfo->setStyleSheet("color:#b8cbd8;font-size:11px;");
        hourLabel_ = new QLabel(QStringLiteral("Heure : 12:00 — Midi"), left);
        hour_ = slider(Qt::Horizontal, 0, 32, 12, left);
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
        resolution_->addItems({QStringLiteral("Full HD — 1920 × 1080"), QStringLiteral("1600 × 900"), QStringLiteral("1280 × 720"), QStringLiteral("4K UHD — 3840 × 2160")});
        quality_ = new QComboBox(panel);
        quality_->addItems({QStringLiteral("Natif — 100 %"), QStringLiteral("Équilibré — 87,5 % par axe"), QStringLiteral("Détails — 75 % par axe"), QStringLiteral("Rapide — 66,7 % par axe"), QStringLiteral("Très rapide — 50 % par axe"), QStringLiteral("FSAA 16× — motif stratifié 4×4")});
        quality_->setCurrentIndex(0);
        controls->addWidget(new QLabel(QStringLiteral("Résolution de sortie"), panel)); controls->addWidget(resolution_);
        controls->addWidget(new QLabel(QStringLiteral("Précision du calcul interne"), panel)); controls->addWidget(quality_);

        auto* form = new QFormLayout;
        distance_ = slider(Qt::Horizontal, 6, 80, 24, panel);
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
        edgeAa_ = new QCheckBox(QStringLiteral("AA contours"), panel); edgeAa_->setChecked(true);
        checks->addWidget(fog_); checks->addWidget(dof_); checks->addWidget(edgeAa_); checks->addStretch();
        controls->addLayout(checks);
        videoPreview_ = new QCheckBox(QStringLiteral("Mode vidéo — aperçu rotatif 800 × 600"), panel);
        controls->addWidget(videoPreview_);
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
        connect(quality_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
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
        connect(edgeAa_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(simdMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        connect(videoPreview_, &QCheckBox::toggled, this, [this](bool enabled) {
            setVideoControlsEnabled(!enabled);
            if (enabled) {
                videoFrames_.clear();
                videoFrames_.reserve(180);
                videoFrameIndex_ = videoPlaybackIndex_ = 0;
                videoAngle_ = 0.f;
                videoBuilding_ = true;
                videoPlaying_ = false;
                videoWindow_->setStatus(QStringLiteral("Calcul vidéo — image 1 / 180 — caméra 0° / 360°"));
                videoWindow_->show(); videoWindow_->raise();
            } else {
                videoPlaybackTimer_->stop();
                videoBuilding_ = videoPlaying_ = false;
                videoFrameIndex_ = videoPlaybackIndex_ = 0;
                std::vector<QImage>().swap(videoFrames_);
                videoAngle_ = 0.f;
                videoWindow_->hide();
            }
            readSettings();
            requestRender();
            if (enabled) status_->setText(QStringLiteral("Calcul vidéo — image 1 / 180 — caméra 0° / 360°"));
        });
        connect(renderButton_, &QPushButton::clicked, this, [this] { readSettings(); requestRender(); });
        connect(homeButton_, &QPushButton::clicked, this, [this] { applySceneCameraPreset(); readSettings(); requestRender(); });
        connect(frontButton_, &QPushButton::clicked, this, [this] {
            if (isRaptureScene()) setCameraSliders(16,180,7,48); else setCameraSliders(20,-10,4,50);
            readSettings(); requestRender();
        });
        connect(rearButton_, &QPushButton::clicked, this, [this] {
            if (isRaptureScene()) setCameraSliders(24,0,9,55); else setCameraSliders(24,-145,7,50);
            readSettings(); requestRender();
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
            if (threadCounts.empty()) { QMessageBox::information(&dialog, QStringLiteral("Benchmark"), QStringLiteral("Sélectionnez au moins un nombre de threads.")); return; }
            const int repeats = dialog.repeats();
            const bool avxAvailable = avx2FmaAvailable();
            const int selectedMode = dialog.requestedSimdMode();
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
            const QString modeSummary = selectedMode == 3 ? QStringLiteral("Comparaison complète x64 / AVX2 + FMA3")
                : selectedMode == 2 ? QStringLiteral("AVX2 + FMA3")
                : selectedMode == 1 ? QStringLiteral("x64 standard")
                : QStringLiteral("Mode choisi dans le moteur (%1)").arg(cfg.avx2 && avxAvailable ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard"));
            const QString settingsSummary = QStringLiteral("output=%1x%2; internal=%3x%4; samples=%5; threads=%6; hour=%7; fog=%8; dof=%9; edge_aa=%10; camera=%11/%12/%13; fov=%14; arch=%15; logical_processors=%16")
                .arg(cfg.outW).arg(cfg.outH).arg(cfg.renderW).arg(cfg.renderH).arg(cfg.samples)
                .arg([&] { QStringList v; for (unsigned n : threadCounts) v << QString::number(n); return v.join(','); }())
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
            benchmarkThread_ = std::thread([this, &dialog, cfg, threadCounts, simdModes, repeats, token, modeSummary, settingsSummary] {
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
                const int totalPasses = int(simdModes.size() * threadCounts.size()) * (repeats + 1);
                int done = 0;
                for (bool useAvx2 : simdModes) {
                    for (unsigned threadCount : threadCounts) {
                        if (benchmarkGeneration_.load() != token) break;
                        std::vector<double> renderTimes, postTimes, totalTimes;
                        BenchmarkRow row; row.threads = threadCount; row.avx2 = useAvx2;
                        for (int repetition = -1; repetition < repeats; ++repetition) {
                            if (benchmarkGeneration_.load() != token) break;
                            std::vector<uint32_t> pixels(size_t(cfg.renderW) * cfg.renderH);
                            Camera cam = camera(cfg.angle, cfg.elevation, cfg.distance, cfg.renderW, cfg.renderH, V{cfg.targetX,cfg.targetY,cfg.targetZ});
                            cam.tanHalf = std::tan(cfg.fov * PI / 360.f); cam.focusDistance = cfg.distance;
                            cam.apertureRadius = cfg.distance * .018f;
                            cam.pixelAngle = 2 * cam.tanHalf / (cfg.renderW * (cfg.samples == 16 ? 4.f : cfg.samples == 4 ? 2.f : 1.f));
                            unsigned samples = cfg.samples; if (cfg.dof && samples < 4) samples = 4;
                            const auto start = std::chrono::steady_clock::now();
                            Stats st = m3d::render(scene_, benchShadows, cam, cfg.renderW, cfg.renderH, pixels, threadCount, cfg.fog, benchmarkGeneration_, token, samples, cfg.dof, useAvx2);
                            if (benchmarkGeneration_.load() != token) break;
                            const double renderMs = st.ms;
                            const auto postStart = std::chrono::steady_clock::now();
                            if (cfg.renderW == cfg.outW * 2 && cfg.renderH == cfg.outH * 2) pixels = downsample2x(pixels, cfg.renderW, cfg.renderH, threadCount);
                            else if (cfg.renderW != cfg.outW || cfg.renderH != cfg.outH) pixels = upscaleBilinear(pixels, cfg.renderW, cfg.renderH, cfg.outW, cfg.outH, threadCount);
                            if (cfg.edgeAa) pixels = antialiasEdges4(pixels, cfg.outW, cfg.outH, threadCount, useAvx2);
                            const double postMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postStart).count();
                            const double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                            if (repetition >= 0) {
                                renderTimes.push_back(renderMs); postTimes.push_back(postMs); totalTimes.push_back(totalMs);
                                row.rays += st.rays; row.tests += st.tests; row.nodes += st.nodes; row.aabb += st.aabb; row.hits += st.hits; row.texels += st.texels;
                            }
                            ++done;
                            const int progress = int(100.0 * done / std::max(1, totalPasses));
                            const QString modeName = useAvx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard");
                            QMetaObject::invokeMethod(&dialog, [&dialog, progress, threadCount, repetition, repeats, modeName] {
                                const QString phase = repetition < 0 ? QStringLiteral("Échauffement") : QStringLiteral("Mesure %1/%2").arg(repetition + 1).arg(repeats);
                                dialog.setProgress(progress, QStringLiteral("%1 — %2 thread(s) — %3 — buffer séparé").arg(modeName).arg(threadCount).arg(phase));
                            }, Qt::QueuedConnection);
                        }
                        if (renderTimes.empty()) break;
                        auto median = [](std::vector<double>& values) {
                            std::sort(values.begin(), values.end());
                            const size_t mid = values.size() / 2;
                            return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) * .5;
                        };
                        row.renderMs = median(renderTimes); row.postMs = median(postTimes); row.totalMs = median(totalTimes);
                        row.rays /= uint64_t(renderTimes.size()); row.tests /= uint64_t(renderTimes.size()); row.nodes /= uint64_t(renderTimes.size());
                        row.aabb /= uint64_t(renderTimes.size()); row.hits /= uint64_t(renderTimes.size()); row.texels /= uint64_t(renderTimes.size());
                        rows.push_back(row);
                    }
                    if (benchmarkGeneration_.load() != token) break;
                }
                sceneLock.unlock();
                { std::lock_guard<std::mutex> lock(requestMutex_); benchmarking_ = false; }
                const bool cancelled = benchmarkGeneration_.load() != token;
                QMetaObject::invokeMethod(&dialog, [this, &dialog, rows = std::move(rows), shadowMs, cfg, repeats, modeSummary, settingsSummary, cancelled] () mutable {
                    dialog.setRows(std::move(rows), shadowMs, cfg.outW, cfg.outH, cfg.samples, unsigned(repeats), modeSummary, settingsSummary);
                    if (cancelled) dialog.setProgress(0, QStringLiteral("Benchmark annulé — résultats partiels conservés. Ombres : %1 ms.").arg(shadowMs, 0, 'f', 1));
                    else dialog.setProgress(100, QStringLiteral("Terminé — médianes par mode et threads. Ombres (passe unique) : %1 ms.").arg(shadowMs, 0, 'f', 1));
                    dialog.setRunning(false);
                    requestCv_.notify_one();
                }, Qt::QueuedConnection);
            });
        });
        dialog.exec();
    }

    void setVideoControlsEnabled(bool enabled) {
        resolution_->setEnabled(enabled); quality_->setEnabled(enabled);
        distance_->setEnabled(enabled); angle_->setEnabled(enabled); elevation_->setEnabled(enabled); fov_->setEnabled(enabled);
        threads_->setEnabled(enabled); hour_->setEnabled(enabled); simdMode_->setEnabled(enabled); fog_->setEnabled(enabled); dof_->setEnabled(enabled); edgeAa_->setEnabled(enabled);
        renderButton_->setEnabled(enabled); homeButton_->setEnabled(enabled); frontButton_->setEnabled(enabled); rearButton_->setEnabled(enabled);
        image_->setEnabled(enabled);
        menuBar()->setEnabled(enabled);
    }

    void readSettings() {
        std::lock_guard<std::mutex> lock(requestMutex_);
        const int ri = resolution_->currentIndex();
        const bool video = videoPreview_->isChecked();
        settings_.outW = video ? 800 : ri == 0 ? 1920 : ri == 1 ? 1600 : ri == 2 ? 1280 : 3840;
        settings_.outH = video ? 600 : ri == 0 ? 1080 : ri == 1 ? 900 : ri == 2 ? 720 : 2160;
        const int qi = quality_->currentIndex();
        const float scale = qi == 0 ? 1.f : qi == 1 ? .875f : qi == 2 ? .75f : qi == 3 ? (2.f / 3.f) : qi == 4 ? .5f : 1.f;
        settings_.renderW = int(std::lround(settings_.outW * scale));
        settings_.renderH = int(std::lround(settings_.outH * scale));
        settings_.samples = qi == 5 ? 16u : 1u;
        settings_.edgeAa = edgeAa_->isChecked();
        settings_.videoPreview = video;
        settings_.targetX=cameraTarget_.x; settings_.targetY=cameraTarget_.y; settings_.targetZ=cameraTarget_.z;
        settings_.navigationPreview=navigationActive_;
        if(settings_.navigationPreview) {
            const float scale=std::min(1.f,std::min(480.f/std::max(1,settings_.outW),270.f/std::max(1,settings_.outH)));
            settings_.renderW=std::max(1,int(std::lround(settings_.outW*scale)));
            settings_.renderH=std::max(1,int(std::lround(settings_.outH*scale)));
            settings_.samples=1u; settings_.edgeAa=false;
        }
        settings_.distance = distance_->value() * .5f;
        settings_.angle = float(angle_->value());
        settings_.elevation = float(elevation_->value());
        if (video) {
            // Keep the Rapture turntable outside the bathysphere and nearby walls.
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
        settings_.hourStep = hour_->value();
        settings_.fog = fog_->isChecked();
        settings_.dof = dof_->isChecked() && !navigationActive_;
        threadLabel_->setText(QStringLiteral("Threads CPU : %1 — SIMD : %2").arg(settings_.threads).arg(settings_.avx2 ? QStringLiteral("AVX2 + FMA3") : QStringLiteral("x64 standard")));
    }

    void queueNextVideoFrame() {
        if (!videoPreview_->isChecked() || !videoBuilding_) return;
        if (videoFrameIndex_ >= 180) {
            videoBuilding_ = false;
            videoPlaying_ = true;
            videoPlaybackIndex_ = 0;
            videoWindow_->setFrame(videoFrames_.front());
            status_->setText(QStringLiteral("Mode vidéo — boucle de 360° pré-calculée"));
            videoPlaybackTimer_->start();
            return;
        }
        videoAngle_ = float(videoFrameIndex_ * 2);
        const size_t imageNumber = videoFrameIndex_ + 1;
        const QString progress = QStringLiteral("Calcul vidéo — image %1 / 180 — caméra %2° / 360°")
            .arg(qulonglong(imageNumber)).arg(int(videoAngle_));
        videoWindow_->setStatus(progress);
        readSettings();
        requestRender();
        status_->setText(progress);
    }

    void requestRender() {
        generation_.fetch_add(1);
        { std::lock_guard<std::mutex> lock(requestMutex_); pending_ = true; }
        status_->setText(QStringLiteral("Rendu CPU en cours…"));
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
            Stats st = m3d::render(scene_, shadows_, cam, cfg.renderW, cfg.renderH, pixels, cfg.threads, cfg.fog, generation_, token, samples, cfg.dof, cfg.avx2);
            if (generation_.load() != token) continue;
            const auto postStart = std::chrono::steady_clock::now();
            if (cfg.renderW == cfg.outW * 2 && cfg.renderH == cfg.outH * 2) pixels = downsample2x(pixels, cfg.renderW, cfg.renderH, cfg.threads);
            else if (cfg.renderW != cfg.outW || cfg.renderH != cfg.outH) pixels = upscaleBilinear(pixels, cfg.renderW, cfg.renderH, cfg.outW, cfg.outH, cfg.threads);
            if (cfg.edgeAa) pixels = antialiasEdges4(pixels, cfg.outW, cfg.outH, cfg.threads, cfg.avx2);
            st.postMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postStart).count();
            st.shadowMs = shadowMs;
            st.ms += st.postMs + shadowMs;
            if (generation_.load() != token) continue;

            QImage image(reinterpret_cast<const uchar*>(pixels.data()), cfg.outW, cfg.outH, cfg.outW * int(sizeof(uint32_t)), QImage::Format_ARGB32);
            image = image.copy();
            const auto count = std::count_if(scene_.obj.begin(), scene_.obj.end(), [](const Obj& o) { return o.kind == Kind::Triangle; });
            const auto objectCount = scene_.obj.size();
            sceneLock.unlock();
            const QString diag = QStringLiteral("CPU %1 | rendu %2 ms | ombres %3 ms | post %4 ms | total %5 ms\nRayons : %6 | impacts : %7 | ciel : %8\nTests : %9 | nœuds BVH : %10 | AABB : %11\nObjets : %12 | triangles : %13 | texels : %14")
                .arg(st.threads).arg(st.ms - st.postMs - st.shadowMs, 0, 'f', 1).arg(st.shadowMs, 0, 'f', 1).arg(st.postMs, 0, 'f', 1).arg(st.ms, 0, 'f', 1)
                .arg(qulonglong(st.rays)).arg(qulonglong(st.hits)).arg(qulonglong(st.sky)).arg(qulonglong(st.tests)).arg(qulonglong(st.nodes)).arg(qulonglong(st.aabb))
                .arg(qulonglong(objectCount)).arg(qulonglong(count)).arg(qulonglong(st.texels));
            QMetaObject::invokeMethod(this, [this, image = std::move(image), diag, hourValue, token, videoFrame = cfg.videoPreview] {
                if (generation_.load() != token || videoPreview_->isChecked() != videoFrame) return;
                stats_->setText(diag);
                if (videoFrame) {
                    if (!videoBuilding_) return;
                    videoFrames_.push_back(image);
                    ++videoFrameIndex_;
                    queueNextVideoFrame();
                } else {
                    image_->setFrame(image);
                    status_->setText(QStringLiteral("Terminé — ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE) + QStringLiteral(" — ") + QString::number(hourValue) + QStringLiteral(" h"));
                }
            }, Qt::QueuedConnection);
        }
    }
};
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString defaultScenePath = QCoreApplication::applicationDirPath() + QStringLiteral("/moteur3d_v1314_scene.bin");
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
