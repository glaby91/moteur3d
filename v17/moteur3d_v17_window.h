#pragma once
#include "anomaly_catalog.h"
#include "camera_diagnostic.h"
#include "exploration_motion.h"
#include "frame_pacing.h"
#include "m3d_engine.h"
#include "pool_ladder.h"
#include "radio_audio.h"
#include "scope_zoom.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include "src/m3d_walk_controller.h"
#include "v17_build_info.h"
#include "weather/sky_compositor.h"
#include "weather/solar_cycle.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSysInfo>
#include <QTabWidget>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dxgi1_2.h>
#include <windows.h>
// Some Windows SDK headers still expose min/max macros; the native renderer
// already uses std::min/std::max extensively and must not be macro-expanded.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

using namespace m3d;

inline int systemMemoryLoadPercent()
{
#if defined(_WIN32)
    MEMORYSTATUSEX state{};
    state.dwLength = sizeof(state);
    return GlobalMemoryStatusEx(&state) ? int(state.dwMemoryLoad) : -1;
#else
    return -1;
#endif
}
struct Result
{
    QImage image;
    v16::CameraPreset cameraPose;
    QString renderer;
    QString error;
    double milliseconds = 0;
    bool gpu = false;
    bool skyCompositing = false;  // DX11 sky pixels are transparent for Qt compositor
    bool directPresented = false; // DXGI or Vulkan native swapchain, no QImage
    bool nativeFallback = false;  // Native DXGI/WSI unavailable: show the QImage preview
    QString nativeFailure;        // Preserve exact Vulkan/DXGI presentation error after image fallback
    QString nativeWindowDetails;  // HWND, owning thread, client size and requested extent
    QString requestStamp;
    // Reject finished frames after a camera, material, backend or scene change.
    uint64_t generation = 0;
    int backendIndex = -1;
    quintptr renderThread = 0;
};
struct SceneLoadOutcome
{
    QString path;
    QString error;
    v16::SceneDatabase database;
    std::shared_ptr<v16::Runtime> runtime;
    uint64_t generation = 0;
};
class ImageView final : public QWidget
{
  public:
    explicit ImageView(QWidget *owner = nullptr) : QWidget(owner)
    {
    }
    void setNativePresentation(bool enabled)
    {
        if (nativePresentation_ == enabled)
            return;
        nativePresentation_ = enabled;
        if (enabled)
            paintPending_ = false;
        setAttribute(Qt::WA_OpaquePaintEvent, enabled);
        setAttribute(Qt::WA_NoSystemBackground, enabled);
        if (!enabled)
            update();
    }
    bool nativePresentation() const
    {
        return nativePresentation_;
    }
    void setImage(QImage im)
    {
        image_ = std::move(im);
        paintPending_ = true;
        scheduleQtPaint();
    }
    void enableSkyCompositing(bool enabled)
    {
        skyCompositing_ = enabled;
        if (!enabled)
            skyImage_ = QImage{};
        else
            skyDirty_ = true;
        scheduleQtPaint();
    }
    bool hasSkyFrame() const
    {
        return !skyImage_.isNull() && !skyDirty_;
    }
    void invalidateSky()
    {
        skyDirty_ = true;
    }
    void drawSky(const m3d::Camera &camera, float hour, double seconds, bool cloudsEnabled, bool fastWeather)
    {
        if (!skyCompositing_)
            return;
        skyComposer_.render(skyImage_, camera, hour, seconds, cloudsEnabled, fastWeather);
        skyDirty_ = false;
        scheduleQtPaint();
    }
    void onPresented(std::function<void()> callback)
    {
        presented_ = std::move(callback);
    }
    void setPlaceholder(const QString &message)
    {
        placeholder_ = message;
        scheduleQtPaint();
    }
    void setRendererStamp(QString displayed, QString requested)
    {
        if (displayedRenderer_ == displayed && requestedRenderer_ == requested)
            return;
        displayedRenderer_ = std::move(displayed);
        requestedRenderer_ = std::move(requested);
        scheduleQtPaint();
    }
    void setHudVisible(bool visible)
    {
        if (hudVisible_ == visible)
            return;
        hudVisible_ = visible;
        scheduleQtPaint();
    }
    void setCameraHud(QStringList lines, bool enabled)
    {
        if (hudLines_ == lines && hudEnabled_ == enabled)
            return;
        hudLines_ = std::move(lines);
        hudEnabled_ = enabled;
        scheduleQtPaint();
    }
    void setScopeOverlay(float level)
    {
        level = std::clamp(level, 0.f, 1.f);
        if (std::abs(scopeLevel_ - level) < .002f)
            return;
        scopeLevel_ = level;
        scheduleQtPaint();
    }
    void setFpsOverlay(double fps, bool visible)
    {
        const QString next =
            visible && fps > 0.0 ? QStringLiteral("%1 FPS").arg(fps, 0, 'f', 1) : QStringLiteral("FPS —");
        if (fpsVisible_ == visible && fpsText_ == next)
            return;
        fpsVisible_ = visible;
        fpsText_ = next;
        scheduleQtPaint();
    }
    static void drawCameraHud(QPainter &painter, const QRect &bounds, const QStringList &lines, int rightInset = 0)
    {
        if (lines.isEmpty() || bounds.width() < 240 || bounds.height() < 110)
            return;
        painter.save();
        QFont font = painter.font();
        font.setFamily(QStringLiteral("Consolas"));
        font.setPointSize(9);
        painter.setFont(font);
        const QFontMetrics fm(font);
        const int pad = 10;
        const QRect panel(bounds.left() + 8, bounds.top() + 8,
                          std::max(120, bounds.width() - 16 - std::max(0, rightInset)), fm.height() + pad * 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(10, 18, 27, 208));
        painter.drawRoundedRect(panel, 6, 6);
        painter.setPen(QColor(226, 239, 247));
        painter.drawText(panel.adjusted(pad, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         fm.elidedText(lines.join(QStringLiteral("   |   ")), Qt::ElideRight, panel.width() - pad * 2));
        painter.restore();
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        // In direct mode this top-level HWND belongs to DXGI or Vulkan WSI.
        // Qt must not repaint over a swapchain present.
        if (nativePresentation_)
            return;
        QPainter p(this);
        p.fillRect(rect(), QColor(22, 26, 31));
        if (image_.isNull())
        {
            p.setPen(QColor(180, 191, 204));
            p.drawText(rect().adjusted(30, 20, -30, -20), Qt::AlignCenter | Qt::TextWordWrap,
                       placeholder_.isEmpty() ? QStringLiteral("Ouvrir une scène V16 dans le menu Fichier")
                                              : placeholder_);
            if (hudVisible_)
                drawRendererStamp(p, rect());
            return;
        }
        QSize s = image_.size();
        s.scale(size(), Qt::KeepAspectRatio);
        QRect dst(QPoint((width() - s.width()) / 2, (height() - s.height()) / 2), s);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        if (skyCompositing_ && !skyImage_.isNull())
        {
            // The 512x288 independent weather layer is updated at 30Hz.
            // The DX11 architectural foreground stays at its last good frame.
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(dst, skyImage_);
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        }
        p.drawImage(dst, image_);
        if (hudVisible_)
            drawRendererStamp(p, dst);
        // Scoped reticle is only a 2D Qt overlay, with no effect on physical
        // position, collision rays or the raster/CPU renderer.
        if (hudVisible_ && scopeLevel_ > .005f)
        {
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            const QPointF center = dst.center();
            const qreal radius = qreal(std::min(dst.width(), dst.height())) * .405;
            QPainterPath outside;
            outside.setFillRule(Qt::OddEvenFill);
            outside.addRect(dst);
            outside.addEllipse(center, radius, radius);
            p.fillPath(outside, QColor(0, 0, 0, int(135.f * scopeLevel_)));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(228, 236, 231, int(195.f * scopeLevel_)), 1.25));
            p.drawEllipse(center, radius, radius);
            const qreal line = qreal(std::max(12, int(radius * .14)));
            const qreal gap = 5.;
            p.setPen(QPen(QColor(16, 24, 22, int(245.f * scopeLevel_)), 1.8));
            p.drawLine(QPointF(center.x() - line, center.y()), QPointF(center.x() - gap, center.y()));
            p.drawLine(QPointF(center.x() + gap, center.y()), QPointF(center.x() + line, center.y()));
            p.drawLine(QPointF(center.x(), center.y() - line), QPointF(center.x(), center.y() - gap));
            p.drawLine(QPointF(center.x(), center.y() + gap), QPointF(center.x(), center.y() + line));
            p.setBrush(QColor(230, 238, 235, int(190.f * scopeLevel_)));
            p.setPen(Qt::NoPen);
            p.drawEllipse(center, 1.75, 1.75);
            p.restore();
        }
        if (hudVisible_ && hudEnabled_)
            drawCameraHud(p, dst, hudLines_, fpsVisible_ ? 105 : 0);
        if (hudVisible_ && fpsVisible_)
        {
            // Count completed presentations, not a theoretical GPU FPS claim.
            p.save();
            QFont f = p.font();
            f.setFamily(QStringLiteral("Consolas"));
            f.setPointSize(10);
            f.setBold(true);
            p.setFont(f);
            const int width = std::max(87, p.fontMetrics().horizontalAdvance(fpsText_) + 18);
            const QRect badge(dst.right() - width - 10, dst.top() + 10, width, 28);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(9, 15, 22, 213));
            p.drawRoundedRect(badge, 6, 6);
            p.setPen(QColor(178, 238, 222));
            p.drawText(badge, Qt::AlignCenter, fpsText_);
            p.restore();
        }
        if (paintPending_)
        {
            paintPending_ = false;
            if (presented_)
                presented_();
        }
    }

  private:
    // In native WSI mode Qt does not own the pixels in this HWND. Camera HUD,
    // FPS and renderer-stamp changes must not schedule a Qt repaint that can
    // clear the just-presented Vulkan swapchain image to the widget background.
    void scheduleQtPaint()
    {
        if (!nativePresentation_)
            update();
    }
    QImage image_;
    QImage skyImage_;
    cse::weather::SkyCompositor skyComposer_;
    bool skyCompositing_ = false, skyDirty_ = true;
    bool paintPending_ = false;
    bool nativePresentation_ = false;
    std::function<void()> presented_;
    QString placeholder_;
    QStringList hudLines_;
    bool hudEnabled_ = true;
    bool hudVisible_ = true;
    float scopeLevel_ = 0.f;
    bool fpsVisible_ = false;
    QString fpsText_;
    QString displayedRenderer_;
    QString requestedRenderer_;
    void drawRendererStamp(QPainter &p, const QRect &bounds)
    {
        if (bounds.width() < 240 || bounds.height() < 80)
            return;
        QStringList lines;
        lines << QStringLiteral("Affiché : %1")
                     .arg(displayedRenderer_.isEmpty() ? QStringLiteral("aucun rendu pour l’instant")
                                                       : displayedRenderer_);
        if (!requestedRenderer_.isEmpty())
            lines << QStringLiteral("Demandé : %1").arg(requestedRenderer_);
        p.save();
        QFont f = p.font();
        f.setFamily(QStringLiteral("Consolas"));
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        int longest = 0;
        for (const auto &line : lines)
            longest = std::max(longest, fm.horizontalAdvance(line));
        const int pad = 9, lh = fm.height() + 3;
        const int panelWidth = std::min(bounds.width() - 24, longest + pad * 2);
        const int panelHeight = lh * lines.size() + pad * 2;
        const QRect panel(bounds.left() + 12, bounds.bottom() - panelHeight - 12, panelWidth, panelHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(9, 15, 22, 220));
        p.drawRoundedRect(panel, 6, 6);
        for (int i = 0; i < lines.size(); ++i)
        {
            p.setPen(i == 0 ? QColor(143, 232, 255) : QColor(255, 218, 132));
            p.drawText(panel.left() + pad, panel.top() + pad + fm.ascent() + i * lh,
                       fm.elidedText(lines[i], Qt::ElideRight, panel.width() - pad * 2));
        }
        p.restore();
    }
};
class V17Window final : public QMainWindow
{
  public:
    explicit V17Window(QString scenePath, int textureResolution, std::shared_ptr<std::atomic_bool> shutdownComplete);
    // UI contract smoke: each renderer owns a unique top-level HWND.
    void smokeNativeUi();
    // GUI smoke: first scene must load asynchronously before we request
    // another scene; the replacement must render a new (non-stale) image.
    void smokeSceneSwitch(const QString &secondScene);
    // Dedicated GPU regression: exercise two independent button-equivalent
    // render requests on the same scene and assert worker/cache affinity.
    // The first render starts normally after asynchronous scene loading.
    void smokeRepeatRaster();
    // Regression: recalling an anomaly must not switch the user's walking
    // preference to free flight. Previously that disabling was persisted in
    // QSettings and removed ALL collision checks on subsequent launches.
    void smokeWalkingCollisionPreference();
    // GUI integration smoke: actual Qt RMB events on the canvas, FOV narrows
    // while aiming, and release/stop restores the original camera exactly.
    // Does not change walking mode or camera position.
    void smokeScopedAim();
    // Real Qt event regression for free-look, true F11 fullscreen and return.
    // Test uses a synthetic motion delta; no physical mouse is required on CI.
    void smokeImmersiveMouse();
    ~V17Window() override;

  private:
    int textureResolution_ = 2048;
    QPushButton *radioToggleButton_ = nullptr;
    QLabel *radioStatusLabel_ = nullptr;
    bool radioPlaying_ = false;
    bool radioAutoplayEnabled_ = false;
    bool rendererHudVisible_ = true;
    cse::RadioAudio radioAudio_;
    QTimer audioTimer_;
    std::array<cse::RadioSpeaker, 4> radioSpeakers_{};
    QSlider *radioVolumeSlider_ = nullptr;

    ImageView *canvas_ = nullptr;
    std::array<ImageView *, 7> backendWindows_{};
    std::array<QString, 7> backendLastRenderer_{};
    std::array<QString, 7> backendLastRequest_{};
    std::array<bool, 7> nativeDirectFailed_{};
    std::array<QString, 7> nativeDirectFailureReason_{};
    int outputBackendIndex_ = -1;
    int activeRenderBackend_ = -1;
    bool activeRenderDirect_ = false;
    QLabel *nativeHudLabel_ = nullptr;
    QLabel *nativeFpsLabel_ = nullptr;
    QLabel *nativeRendererOverlay_ = nullptr;
    QString lastPresentedRenderer_;
    QCheckBox *directDisplayBox_ = nullptr;
    bool nativeReadyRetryQueued_ = false;
    int nativeReadyAttempts_ = 0;
    QDockWidget *sceneExplorerDock_ = nullptr;
    QDockWidget *renderControlsDock_ = nullptr;
    QDockWidget *anomalyReviewDock_ = nullptr;
    QRect explorationSavedMainGeometry_;
    QRect explorationSavedRenderGeometry_;
    QSize explorationSavedMinimumSize_;
    bool explorationSavedSceneDockVisible_ = false;
    bool explorationSavedControlsDockVisible_ = false;
    bool explorationSavedReviewDockVisible_ = false;
    bool explorationSavedMenuVisible_ = true;
    bool explorationSavedMainMaximized_ = false;
    bool explorationSavedMainFullScreen_ = false;
    bool explorationSavedRenderMaximized_ = false;
    bool explorationSavedRenderFullScreen_ = false;
    bool explorationWindowLayoutActive_ = false;
    QTimer frameTimer_;
    QTimer skyAnimationTimer_;
    QElapsedTimer skyMotionClock_;
    qint64 lastVulkanIdleSkyRequestMs_ = -1000;
    QTimer dayNightTimer_;
    QElapsedTimer dayNightClock_;
    double dayNightOriginHour_ = 14.75;
    bool applyingDayNightTick_ = false;
    qint64 lastDayNightRenderMs_ = -5000;
    QElapsedTimer frameClock_;
    QElapsedTimer motionClock_;
    QSet<int> keysDown_;
    bool leftShiftHeld_ = false;
    bool rightShiftHeld_ = false;
    bool leftControlHeld_ = false;
    bool rightControlHeld_ = false;
    bool jumpRequested_ = false;
    V jumpHorizontalVelocity_{};
    QPoint previousMouse_;
    v16::CameraPreset freeCamera_;
    walk::State walkingState_{};
    cse::pool_ladder::Motion poolLadderMotion_{};
    QCheckBox *walkingModeBox_ = nullptr;
    bool exploring_ = false, freeCameraActive_ = false, leftMouseHeld_ = false, rightMouseHeld_ = false;
    bool mouseCaptured_ = false;
    bool peripheralExperimentEnabled_ = false, peripheralDuringMotion_ = false;
    QElapsedTimer lastMouseLookMotion_;
    QAction *immersiveAction_ = nullptr;
    double fpsFiltered_ = 0.0;
    float scopeBaseFov_ = 65.f, scopeBlend_ = 0.f;
    // Exact anomaly/replayed camera is kept until the next walking input.
    // This does not disable or persistently alter the user's collision mode.
    bool walkingCollisionSyncPending_ = false;
    bool explorationQualityOverride_ = false;
    RasterQualitySettings explorationOriginalQuality_{};
    float yaw_ = 0.f, pitch_ = 0.f;
    bool cameraHudEnabled_ = true;
    bool hasRenderedPose_ = false;
    v16::CameraPreset lastRenderedPose_;
    int selectedSceneCameraIndex() const;
    QPushButton *explorationButton_ = nullptr;
    QLabel *explorationHelp_ = nullptr;
    QLabel *explorationSpeedLabel_ = nullptr;
    v16::CameraPreset activeCameraPose() const;
    void updateCameraHud();
    static QString gpuAdapterLabel();
    QString githubRevision() const;
    static QJsonObject sceneCameraReport(const v16::CameraPreset &pose, const QString &manifest, const QString &title,
                                         const QString &image, const QSize imageSize, const QString &revision);
    void copyLiveCamera();
    void captureAnomaly();
    void restoreAnomalyCamera();
    void updateExplorationSpeedIndicator();
    void resetScopeForCamera();
    void activateFreeCamera();
    void updateLook();
    void captureMouseLook();
    void releaseMouseLook();
    void setMotionResolution(bool active);
    void stopExploration();
    void toggleExploration();
    void frameTick();
    static bool supportsExplorationBackend(int index);

  protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

  private:
    QTreeWidget *sceneTree_ = nullptr;
    QTreeWidget *resourceTree_ = nullptr;
    QComboBox *cameraBox_ = nullptr;
    QVector<QAction *> quickCameraActions_;
    QComboBox *backendBox_ = nullptr;
    QComboBox *qualityBox_ = nullptr;
    QComboBox *rasterPresetBox_ = nullptr;
    QComboBox *rasterMsaaBox_ = nullptr;
    QComboBox *rasterAnisoBox_ = nullptr;
    QComboBox *rasterLodBox_ = nullptr;
    QCheckBox *rasterCullBox_ = nullptr;
    QCheckBox *rasterDynamicLodBox_ = nullptr;
    QComboBox *giSamples_ = nullptr;
    QComboBox *directSamples_ = nullptr;
    QCheckBox *progressiveGi_ = nullptr;
    QCheckBox *denoiseGi_ = nullptr;
    QCheckBox *denoiseDirect_ = nullptr;
    QCheckBox *edgeAa_ = nullptr;
    QSlider *azimuth_ = nullptr;
    QDoubleSpinBox *sunHour_ = nullptr;
    QCheckBox *cloudAnimationBox_ = nullptr;
    bool lumaTraceCloudDefaultApplied_ = false;
    QCheckBox *dayNightCycleBox_ = nullptr;
    QCheckBox *exteriorLightsBox_ = nullptr;
    QCheckBox *hybridLightBox_ = nullptr;
    QCheckBox *filmicToneBox_ = nullptr;
    QCheckBox *colorEnergyBox_ = nullptr;
    QDoubleSpinBox *colorExposureBox_ = nullptr;
    QPushButton *bakeGiButton_ = nullptr;
    QProcess *bakeGiProcess_ = nullptr;
    QString bakingScenePath_;
    QDoubleSpinBox *roughness_ = nullptr;
    QDoubleSpinBox *metallic_ = nullptr;
    QLabel *materialLabel_ = nullptr;
    QLabel *renderTimeLabel_ = nullptr;
    QFutureWatcher<Result> watcher_;
    QThreadPool renderPool_;
    QFutureWatcher<SceneLoadOutcome> loadWatcher_;
    QLabel *sceneLoadLabel_ = nullptr;
    QProgressBar *sceneLoadProgress_ = nullptr;
    bool sceneLoading_ = false;
    uint64_t sceneLoadGeneration_ = 0;
    QString queuedScenePath_;
    std::shared_ptr<std::atomic<uint64_t>> renderSerial_ = std::make_shared<std::atomic<uint64_t>>(0);
    std::shared_ptr<std::atomic_bool> shutdownComplete_;
    std::shared_ptr<v16::Runtime> runtime_;
    // The sun and scene geometry do not change when navigating cameras.
    // Reuse an identical shadow map across CPU renders of the same runtime.
    struct CpuShadowCache
    {
        std::mutex mutex;
        std::weak_ptr<v16::Runtime> scene;
        std::shared_ptr<const ShadowMap> shadow;
        float hour = -1.f;
    };
    std::shared_ptr<CpuShadowCache> cpuShadows_ = std::make_shared<CpuShadowCache>();
    bool pending_ = false, updatingControls_ = false, closingRequested_ = false;
    QElapsedTimer pacingClock_;
    qint64 lastPresentMs_ = -1;
    double lastCompletedRenderMs_ = 0.0;
    uint64_t pacingRejected_ = 0, pacingQueued_ = 0;
    std::vector<cse::pacing::Sample> pacingSamples_;
    static constexpr int pacingCapacity_ = 5000;
    int selectedMaterial_ = -1;
    QString scenePath_;
    QString materialsPath_;
    int currentAnomaly_ = -1;
    v16::CameraPreset beforeAnomalyCamera_{};
    bool hasBeforeAnomalyCamera_ = false;
    QLabel *anomalyPanelLabel_ = nullptr;
    QLabel *sceneRevisionLabel_ = nullptr;
    QLabel *systemMemoryLabel_ = nullptr;
    QProgressBar *systemMemoryBar_ = nullptr;
    QTimer *memoryRefreshTimer_ = nullptr;

    void updateMemoryBanner();

    void displayRasterSettings(const RasterQualitySettings &q);
    void applyRasterSettings(bool userCustom);
    void stopRadio();
    bool loadSpeakerLayout();
    void playRadio();
    void refreshRadioForScene(const QString &path);

    void selectAnomaly(int index);
    void updateAnomalyPanel();
    void saveAnomalyVerdict(const QString &verdict);
    void returnBeforeAnomaly();

    void createUi();
    QImage lastImage_;
    void chooseMaterial(int id, const QString &name);
    void modifyMaterial();
    void saveMaterials();
    void openScene(const QString &path);
    void beginSceneLoad();
    void finishSceneLoad();
    void populateTrees(const v16::SceneDatabase &database);

    bool vulkanDirectAvailable() const;
    bool nativeDirectSelected() const;
    void focusRenderViewport();
    void restoreExplorationViewportFocus();
    void arrangeExplorationWindows();
    void restoreExplorationWindows();
    QString describeNativeWindow(quintptr nativeHandle, QSize requested, qreal dpr) const;
    QString nativeDiagnosticLogPath() const;
    void saveNativePresentationDiagnostic(const QString &backend, const QString &reason, const QString &windowDetails);
    QString requestedRendererStamp() const;
    void updateRendererStamp();
    void selectBackendOutputWindow();
    void resetPacing();
    void recordImagePresented();
    void exportPacing();
    void requestRender();
};
