#include "m3d_engine.h"
#include "version.h"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QMetaObject>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

namespace {
using namespace m3d;

struct RenderSettings {
    int outW = 1920, outH = 1080;
    int renderW = 1920, renderH = 1080;
    float distance = 12.f, angle = -43.f, elevation = 7.f, fov = 50.f;
    unsigned threads = 16, samples = 1;
    int hourStep = 12;
    bool fog = false, dof = false, edgeAa = true;
};

class ImageView final : public QWidget {
public:
    explicit ImageView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(480, 320);
        setAutoFillBackground(false);
    }
    void setFrame(QImage image) { image_ = std::move(image); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(4, 12, 17));
        if (image_.isNull()) return;
        QSize fit = image_.size();
        fit.scale(size(), Qt::KeepAspectRatio);
        QRect target(QPoint((width() - fit.width()) / 2, (height() - fit.height()) / 2), fit);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(target, image_);
    }
private:
    QImage image_;
};

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(Scene loadedScene) : scene_(std::move(loadedScene)) {
        setWindowTitle(QStringLiteral("Moteur 3D natif x64 — ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE));
        resize(1480, 900);
        buildUi();
        updateLighting(12.f);
        Stats initialShadowStats;
        shadows_ = buildShadow(scene_, initialShadowStats);
        shadowHour_ = 12;
        coordinator_ = std::thread([this] { renderLoop(); });
        requestRender();
    }

    ~MainWindow() override {
        generation_.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(requestMutex_);
            stopping_ = true;
        }
        requestCv_.notify_one();
        if (coordinator_.joinable()) coordinator_.join();
    }

private:
    Scene scene_;
    ShadowMap shadows_;
    int shadowHour_ = -1;
    RenderSettings settings_;
    std::mutex requestMutex_;
    std::condition_variable requestCv_;
    std::atomic<uint64_t> generation_{1};
    std::thread coordinator_;
    bool stopping_ = false, pending_ = false;
    ImageView* image_ = nullptr;
    QLabel *status_ = nullptr, *stats_ = nullptr, *hourLabel_ = nullptr, *threadLabel_ = nullptr;
    QComboBox *resolution_ = nullptr, *quality_ = nullptr;
    QSlider *distance_ = nullptr, *angle_ = nullptr, *elevation_ = nullptr, *fov_ = nullptr;
    QSlider *threads_ = nullptr, *hour_ = nullptr;
    QCheckBox *fog_ = nullptr, *dof_ = nullptr;

    static QSlider* slider(Qt::Orientation orientation, int min, int max, int value, QWidget* parent) {
        auto* s = new QSlider(orientation, parent);
        s->setRange(min, max);
        s->setValue(value);
        return s;
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
        hourLabel_ = new QLabel(QStringLiteral("Heure : 12:00 — Midi"), left);
        hour_ = slider(Qt::Horizontal, 0, 32, 12, left);
        auto* hourMarks = new QHBoxLayout;
        for (const auto& label : {QStringLiteral("Matin"), QStringLiteral("Midi"), QStringLiteral("Coucher"), QStringLiteral("Nuit")}) {
            auto* l = new QLabel(label, left); l->setStyleSheet("color:#d4e1eb"); hourMarks->addWidget(l);
        }
        leftLayout->addWidget(title);
        leftLayout->addWidget(info);
        leftLayout->addWidget(image_, 1);
        leftLayout->addWidget(hourLabel_);
        leftLayout->addWidget(hour_);
        leftLayout->addLayout(hourMarks);

        auto* panel = new QWidget(split);
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
        quality_->addItems({QStringLiteral("Natif — sans lissage"), QStringLiteral("Équilibré — 87,5 % par axe"), QStringLiteral("Détails — 75 % par axe"), QStringLiteral("Rapide — 66,7 % par axe"), QStringLiteral("Très rapide — 50 % par axe"), QStringLiteral("Lissage des contours — 4 voisins"), QStringLiteral("FSAA 16× — motif stratifié 4×4")});
        quality_->setCurrentIndex(5);
        controls->addWidget(new QLabel(QStringLiteral("Résolution de sortie"), panel)); controls->addWidget(resolution_);
        controls->addWidget(new QLabel(QStringLiteral("Précision du calcul interne"), panel)); controls->addWidget(quality_);

        auto* form = new QFormLayout;
        distance_ = slider(Qt::Horizontal, 9, 48, 24, panel);
        angle_ = slider(Qt::Horizontal, -180, 180, -43, panel);
        elevation_ = slider(Qt::Horizontal, -5, 50, 7, panel);
        fov_ = slider(Qt::Horizontal, 35, 105, 50, panel);
        threads_ = slider(Qt::Horizontal, 1, 32, 16, panel);
        form->addRow(QStringLiteral("Distance caméra"), distance_);
        form->addRow(QStringLiteral("Rotation caméra"), angle_);
        form->addRow(QStringLiteral("Élévation caméra"), elevation_);
        form->addRow(QStringLiteral("Champ horizontal"), fov_);
        threadLabel_ = new QLabel(QStringLiteral("Threads CPU : 16"), panel);
        form->addRow(threadLabel_, threads_);
        controls->addLayout(form);
        auto* checks = new QHBoxLayout;
        fog_ = new QCheckBox(QStringLiteral("Brouillard atmosphérique"), panel);
        dof_ = new QCheckBox(QStringLiteral("DOF"), panel);
        checks->addWidget(fog_); checks->addWidget(dof_); checks->addStretch();
        controls->addLayout(checks);
        auto* renderButton = new QPushButton(QStringLiteral("Calculer l’image"), panel);
        controls->addWidget(renderButton);
        auto* views = new QHBoxLayout;
        auto* home = new QPushButton(QStringLiteral("Vue initiale"), panel);
        auto* front = new QPushButton(QStringLiteral("Vue avant"), panel);
        auto* rear = new QPushButton(QStringLiteral("Vue arrière"), panel);
        views->addWidget(home); views->addWidget(front);
        controls->addLayout(views); controls->addWidget(rear);
        status_ = new QLabel(QStringLiteral("Prêt"), panel);
        auto* diagTitle = new QLabel(QStringLiteral("DIAGNOSTICS MESURÉS"), panel);
        diagTitle->setStyleSheet("font-size:15px;font-weight:700");
        stats_ = new QLabel(QStringLiteral("En attente du premier rendu"), panel);
        stats_->setWordWrap(true);
        controls->addWidget(status_); controls->addWidget(diagTitle); controls->addWidget(stats_); controls->addStretch();

        split->addWidget(left); split->addWidget(panel); split->setStretchFactor(0, 1); split->setStretchFactor(1, 0);
        root->addWidget(split);
        setCentralWidget(central);

        connect(resolution_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        connect(quality_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { readSettings(); requestRender(); });
        for (auto* s : {distance_, angle_, elevation_, fov_, threads_, hour_}) {
            connect(s, &QSlider::sliderReleased, this, [this] { readSettings(); requestRender(); });
            connect(s, &QSlider::valueChanged, this, [this, s](int) { if (!s->isSliderDown()) { readSettings(); requestRender(); } });
        }
        connect(fog_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(dof_, &QCheckBox::toggled, this, [this] { readSettings(); requestRender(); });
        connect(renderButton, &QPushButton::clicked, this, [this] { readSettings(); requestRender(); });
        connect(home, &QPushButton::clicked, this, [this] { distance_->setValue(24); angle_->setValue(-43); elevation_->setValue(7); fov_->setValue(50); readSettings(); requestRender(); });
        connect(front, &QPushButton::clicked, this, [this] { distance_->setValue(20); angle_->setValue(-10); elevation_->setValue(4); readSettings(); requestRender(); });
        connect(rear, &QPushButton::clicked, this, [this] { distance_->setValue(24); angle_->setValue(-145); elevation_->setValue(7); readSettings(); requestRender(); });
        connect(hour_, &QSlider::valueChanged, this, [this](int step) {
            float h = 6.f + step * .5f;
            QString mode = h < 9 ? QStringLiteral("Matin") : h < 16 ? QStringLiteral("Midi") : h < 20 ? QStringLiteral("Coucher de soleil") : QStringLiteral("Nuit");
            hourLabel_->setText(QStringLiteral("Heure : %1:%2 — %3").arg(int(h), 2, 10, QLatin1Char('0')).arg((step % 2) ? QStringLiteral("30") : QStringLiteral("00"), mode));
        });
    }

    void readSettings() {
        std::lock_guard<std::mutex> lock(requestMutex_);
        const int ri = resolution_->currentIndex();
        settings_.outW = ri == 0 ? 1920 : ri == 1 ? 1600 : ri == 2 ? 1280 : 3840;
        settings_.outH = ri == 0 ? 1080 : ri == 1 ? 900 : ri == 2 ? 720 : 2160;
        const int qi = quality_->currentIndex();
        const float scale = qi == 0 ? 1.f : qi == 1 ? .875f : qi == 2 ? .75f : qi == 3 ? (2.f / 3.f) : qi == 4 ? .5f : 1.f;
        settings_.renderW = int(std::lround(settings_.outW * scale));
        settings_.renderH = int(std::lround(settings_.outH * scale));
        settings_.samples = qi == 6 ? 16u : 1u;
        settings_.edgeAa = qi == 5;
        settings_.distance = distance_->value() * .5f;
        settings_.angle = float(angle_->value());
        settings_.elevation = float(elevation_->value());
        settings_.fov = float(fov_->value());
        settings_.threads = unsigned(threads_->value());
        settings_.hourStep = hour_->value();
        settings_.fog = fog_->isChecked();
        settings_.dof = dof_->isChecked();
        threadLabel_->setText(QStringLiteral("Threads CPU : %1").arg(settings_.threads));
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
            requestCv_.wait(lock, [this] { return stopping_ || pending_; });
            if (stopping_) return;
            pending_ = false;
            const uint64_t token = generation_.load();
            const RenderSettings cfg = settings_;
            lock.unlock();

            const int hourValue = 6 + cfg.hourStep / 2;
            const float hour = 6.f + cfg.hourStep * .5f;
            double shadowMs = 0;
            if (cfg.hourStep != shadowHour_) {
                updateLighting(hour);
                Stats lightStats;
                const auto start = std::chrono::steady_clock::now();
                shadows_ = buildShadow(scene_, lightStats);
                shadowMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                shadowHour_ = cfg.hourStep;
            }

            std::vector<uint32_t> pixels(size_t(cfg.renderW) * cfg.renderH);
            Camera cam = camera(cfg.angle, cfg.elevation, cfg.distance, cfg.renderW, cfg.renderH);
            cam.tanHalf = std::tan(cfg.fov * PI / 360.f);
            cam.focusDistance = cfg.distance;
            cam.apertureRadius = cfg.distance * .018f;
            cam.pixelAngle = 2 * cam.tanHalf / (cfg.renderW * (cfg.samples == 16 ? 4.f : cfg.samples == 4 ? 2.f : 1.f));
            unsigned samples = cfg.samples;
            if (cfg.dof && samples < 4) samples = 4;
            Stats st = m3d::render(scene_, shadows_, cam, cfg.renderW, cfg.renderH, pixels, cfg.threads, cfg.fog, generation_, token, samples, cfg.dof);
            if (generation_.load() != token) continue;
            const auto postStart = std::chrono::steady_clock::now();
            if (cfg.renderW == cfg.outW * 2 && cfg.renderH == cfg.outH * 2) pixels = downsample2x(pixels, cfg.renderW, cfg.renderH, cfg.threads);
            else if (cfg.renderW != cfg.outW || cfg.renderH != cfg.outH) pixels = upscaleBilinear(pixels, cfg.renderW, cfg.renderH, cfg.outW, cfg.outH, cfg.threads);
            if (cfg.edgeAa) pixels = antialiasEdges4(pixels, cfg.outW, cfg.outH, cfg.threads);
            st.postMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postStart).count();
            st.shadowMs = shadowMs;
            st.ms += st.postMs + shadowMs;
            if (generation_.load() != token) continue;

            QImage image(reinterpret_cast<const uchar*>(pixels.data()), cfg.outW, cfg.outH, cfg.outW * int(sizeof(uint32_t)), QImage::Format_ARGB32);
            image = image.copy();
            const auto count = std::count_if(scene_.obj.begin(), scene_.obj.end(), [](const Obj& o) { return o.kind == Kind::Triangle; });
            const QString diag = QStringLiteral("CPU %1 | rendu %2 ms | ombres %3 ms | post %4 ms | total %5 ms\nRayons : %6 | impacts : %7 | ciel : %8\nTests : %9 | nœuds BVH : %10 | AABB : %11\nObjets : %12 | triangles : %13 | texels : %14")
                .arg(st.threads).arg(st.ms - st.postMs - st.shadowMs, 0, 'f', 1).arg(st.shadowMs, 0, 'f', 1).arg(st.postMs, 0, 'f', 1).arg(st.ms, 0, 'f', 1)
                .arg(qulonglong(st.rays)).arg(qulonglong(st.hits)).arg(qulonglong(st.sky)).arg(qulonglong(st.tests)).arg(qulonglong(st.nodes)).arg(qulonglong(st.aabb))
                .arg(qulonglong(scene_.obj.size())).arg(qulonglong(count)).arg(qulonglong(st.texels));
            QMetaObject::invokeMethod(this, [this, image = std::move(image), diag, hourValue] {
                image_->setFrame(image);
                stats_->setText(diag);
                status_->setText(QStringLiteral("Terminé — ") + QString::fromWCharArray(M3D_RELEASE_ID_WIDE) + QStringLiteral(" — ") + QString::number(hourValue) + QStringLiteral(" h"));
            }, Qt::QueuedConnection);
        }
    }
};
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString scenePath = QCoreApplication::applicationDirPath() + QStringLiteral("/moteur3d_v1314_scene.bin");
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
    MainWindow window(std::move(scene));
    window.show();
    return app.exec();
}
