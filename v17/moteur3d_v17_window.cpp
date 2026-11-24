#include "moteur3d_v17_window.h"

#include <cstdlib>

using namespace m3d;

namespace
{
bool isLeftShift(const QKeyEvent *event)
{
#if defined(_WIN32)
    return event->nativeVirtualKey() == VK_LSHIFT;
#else
    Q_UNUSED(event);
    return false;
#endif
}

bool isRightShift(const QKeyEvent *event)
{
#if defined(_WIN32)
    return event->nativeVirtualKey() == VK_RSHIFT;
#else
    Q_UNUSED(event);
    return false;
#endif
}

bool isLeftControl(const QKeyEvent *event)
{
#if defined(_WIN32)
    return event->nativeVirtualKey() == VK_LCONTROL;
#else
    Q_UNUSED(event);
    return false;
#endif
}

bool isRightControl(const QKeyEvent *event)
{
#if defined(_WIN32)
    return event->nativeVirtualKey() == VK_RCONTROL;
#else
    Q_UNUSED(event);
    return false;
#endif
}
} // namespace

V17Window::V17Window(QString scenePath, int textureResolution, std::shared_ptr<std::atomic_bool> shutdownComplete)
    : textureResolution_(textureResolution), shutdownComplete_(std::move(shutdownComplete))
{
    setAttribute(Qt::WA_QuitOnClose, true);
    setWindowTitle(QStringLiteral("CSE WARP Beta X - V%1 Exploration").arg(CSE_WARP_PRODUCT_VERSION));
    resize(1510, 900);
    setMinimumSize(1100, 670);
    // Every backend gets its own independent top-level HWND. Direct DX11
    // and Vulkan present into their own ImageView HWND; CPU/compute/QImage
    // modes paint into the matching window through the same view class.
    // None of the render targets is embedded in the controls window.
    for (int i = 0; i < int(backendWindows_.size()); ++i)
    {
        // No owner window: Windows must allow the controls and each
        // backend output to be raised independently.
        auto *view = new ImageView;
        view->setWindowFlag(Qt::Window, true);
        view->setAttribute(Qt::WA_QuitOnClose, false);
        view->setAttribute(Qt::WA_NativeWindow);
        view->setFocusPolicy(Qt::StrongFocus);
        view->setMouseTracking(true);
        view->resize(1000, 680);
        view->move(pos() + QPoint(36, 70));
        view->setWindowTitle(QStringLiteral("CSE WARP — rendu dédié — backend %1").arg(i + 1));
        view->installEventFilter(this);
        view->onPresented([this] { recordImagePresented(); });
        backendWindows_[size_t(i)] = view;
    }
    canvas_ = backendWindows_[4];
    nativeRendererOverlay_ = new QLabel(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    nativeRendererOverlay_->setObjectName(QStringLiteral("v17NativeRendererStamp"));
    nativeRendererOverlay_->setGeometry(12, 12, 470, 34);
    nativeRendererOverlay_->setWordWrap(false);
    nativeRendererOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    nativeRendererOverlay_->setStyleSheet(
        QStringLiteral("QLabel { color: #dff7ff; background: rgba(9,15,22,220); "
                       "border-radius: 6px; padding: 7px; font: bold 9pt \"Consolas\"; }"));
    nativeRendererOverlay_->raise();
    auto *controlsPlaceholder = new QLabel(QStringLiteral("Chaque backend possède sa fenêtre de rendu dédiée. "
                                                          "Sélectionnez un moteur puis actualisez la scène."),
                                           this);
    controlsPlaceholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(controlsPlaceholder);
    frameTimer_.setInterval(16);
    connect(&frameTimer_, &QTimer::timeout, this, [this] { frameTick(); });
    // The D3D11 preview owns a thread_local device and texture cache.
    // A single persistent QtConcurrent worker keeps GPU resources on
    // the SAME thread across all subsequent renders and camera changes.
    // Using Qt's global pool can build a new device for every worker.
    renderPool_.setMaxThreadCount(1);
    renderPool_.setExpiryTimeout(-1);
    createUi();
    updateRendererStamp();
    // The combo box selected DirectX BEFORE its currentIndexChanged signal
    // was connected. Select its dedicated output window now, before show() and
    // before asynchronous scene loading requests the first image.
    // Existing GUI smokes require the reference QImage path on CI's
    // offscreen Qt platform; the native-UI policy smoke is excluded.
    const QStringList args = QCoreApplication::arguments();
    if (args.contains(QStringLiteral("--smoke-scene-switch")) ||
        args.contains(QStringLiteral("--smoke-repeat-raster")) ||
        args.contains(QStringLiteral("--smoke-walking-collisions")) ||
        args.contains(QStringLiteral("--smoke-scope-mouse")) ||
        args.contains(QStringLiteral("--smoke-immersive-mouse")))
    {
        const QSignalBlocker blocked(directDisplayBox_);
        directDisplayBox_->setChecked(false);
    }
    selectBackendOutputWindow();
    // A stationary camera must always display the native full-resolution
    // raster, regardless of the experimental launcher environment.
    peripheralExperimentEnabled_ = qEnvironmentVariable("M3D_V17_PERIPHERAL_MULTIREZ") == QStringLiteral("1");
    setRasterPeripheralMultiresOverride(0);
    pacingClock_.start();
    skyMotionClock_.start();
    audioTimer_.setInterval(50); // 20 Hz spatial updates, independent from GPU images.
    connect(&audioTimer_, &QTimer::timeout, this, [this] {
        if (!radioPlaying_ || !runtime_)
            return;
        const auto pose = activeCameraPose();
        const float eye[3]{pose.eye.x, pose.eye.y, pose.eye.z};
        const float target[3]{pose.target.x, pose.target.y, pose.target.z};
        radioAudio_.update(eye, target);
    });
    audioTimer_.start();
    // A permanent status-bar widget outlives temporary render/progress
    // messages. QStatusBar::showMessage(..., 15000) used to erase the
    // completed render time after 15 seconds.
    nativeHudLabel_ = new QLabel(QStringLiteral("CAM —"), this);
    nativeHudLabel_->setMinimumWidth(215);
    nativeHudLabel_->hide(); // Camera details belong in the render-window HUD.
    nativeFpsLabel_ = new QLabel(QStringLiteral("FPS —"), this);
    nativeFpsLabel_->hide(); // Avoid duplicating the render-window FPS badge.
    renderTimeLabel_ = new QLabel(QStringLiteral("Dernier rendu : —"), this);
    renderTimeLabel_->setObjectName(QStringLiteral("v16LastRenderTime"));
    statusBar()->addPermanentWidget(renderTimeLabel_);
    explorationSpeedLabel_ = new QLabel(this);
    explorationSpeedLabel_->setObjectName(QStringLiteral("v17ExplorationSpeed"));
    statusBar()->addPermanentWidget(explorationSpeedLabel_);
    explorationSpeedLabel_->hide();
    sceneLoadLabel_ = new QLabel(this);
    sceneLoadLabel_->setText(QStringLiteral("Chargement de la scène…"));
    sceneLoadProgress_ = new QProgressBar(this);
    sceneLoadProgress_->setRange(0, 100);
    sceneLoadProgress_->setValue(0);
    sceneLoadProgress_->setMaximumWidth(255);
    sceneLoadProgress_->setTextVisible(true);
    statusBar()->addWidget(sceneLoadLabel_, 1);
    statusBar()->addWidget(sceneLoadProgress_);
    sceneLoadLabel_->hide();
    sceneLoadProgress_->hide();
    connect(&loadWatcher_, &QFutureWatcher<SceneLoadOutcome>::finished, this, [this] {
        if (closingRequested_)
        {
            if (!loadWatcher_.isRunning())
                QTimer::singleShot(0, this, [this] { close(); });
            return;
        }
        finishSceneLoad();
    });
    connect(&watcher_, &QFutureWatcher<Result>::finished, this, [this] {
        // A stale finished notification can arrive after setFuture()
        // already started another render. result() would block the GUI
        // waiting for that new render: never call it while still running.
        if (watcher_.isRunning())
            return;
        if (closingRequested_)
        {
            QTimer::singleShot(0, this, [this] { close(); });
            return;
        }
        const auto outcome = watcher_.result();
        if (sceneLoading_)
            return;
        // A previous scene may finish after the user has opened a new one.
        // Never show its image or overwrite the current status bar.
        if (outcome.generation != renderSerial_->load())
        {
            ++pacingRejected_;
            if (pending_)
            {
                pending_ = false;
                requestRender();
            }
            return;
        }
        const bool nativePresentationFailed = !outcome.nativeFailure.isEmpty();
        if (nativePresentationFailed)
        {
            // Retain failures even if the user switched backends while
            // the native WSI call was still in flight.
            const int failedBackend = outcome.backendIndex;
            if (failedBackend >= 0 && failedBackend < int(nativeDirectFailed_.size()))
            {
                nativeDirectFailed_[size_t(failedBackend)] = true;
                nativeDirectFailureReason_[size_t(failedBackend)] = outcome.nativeFailure;
            }
            const QString backend = failedBackend == 5
                                        ? QStringLiteral("Vulkan")
                                        : (failedBackend == 6 ? QStringLiteral("LumaTrace") : QStringLiteral("DX11"));
            const QString message = QStringLiteral("%1 direct a échoué : %2\n%3")
                                        .arg(backend, outcome.nativeFailure, outcome.nativeWindowDetails);
            qWarning().noquote() << message;
            saveNativePresentationDiagnostic(backend, outcome.nativeFailure, outcome.nativeWindowDetails);
        }
        if (outcome.backendIndex != backendBox_->currentIndex())
        {
            // A queued frame belongs to its original backend HWND. Never
            // paint or report it as if it came from the newly selected one.
            if (pending_)
            {
                pending_ = false;
                requestRender();
            }
            return;
        }
        if (!outcome.error.isEmpty())
        {
            // Keep the selected backend active and show its own failure in
            // its dedicated viewport instead of leaving a misleading blank.
            const int failedBackend = outcome.backendIndex;
            if (failedBackend >= 0 && failedBackend < int(backendLastRenderer_.size()))
            {
                backendLastRenderer_[size_t(failedBackend)] = outcome.renderer;
                backendLastRequest_[size_t(failedBackend)] = outcome.requestStamp;
            }
            if (canvas_)
            {
                canvas_->setNativePresentation(false);
                canvas_->setImage(QImage{});
                canvas_->setCameraHud({}, false);
                canvas_->enableSkyCompositing(false);
                canvas_->setPlaceholder(QStringLiteral("%1\n\n%2").arg(outcome.renderer, outcome.error));
            }
            updateRendererStamp();
        }
        if (outcome.error.isEmpty() && !outcome.nativeFallback && outcome.directPresented != nativeDirectSelected())
        {
            // Direct-display was toggled while this frame was in flight.
            // Keep the frame's original HWND mode until completion, then
            // immediately render again using the newly requested mode.
            pending_ = false;
            requestRender();
            return;
        }
        if (nativePresentationFailed)
        {
            // Keep the checked request, but stop retrying the unusable
            // swapchain every camera frame. Toggling off/on retries it.
            selectBackendOutputWindow();
        }
        if (outcome.error.isEmpty() && (outcome.directPresented || !outcome.image.isNull()))
        {
            lastPresentedRenderer_ = outcome.renderer;
            if (!lastPresentedRenderer_.contains(QStringLiteral("(swapchain)")) &&
                !lastPresentedRenderer_.contains(QStringLiteral("(QImage)")))
                lastPresentedRenderer_ += outcome.directPresented ? QStringLiteral(" — Direct GPU (swapchain)")
                                                                  : QStringLiteral(" — affichage Qt (QImage)");
            backendLastRenderer_[size_t(outcome.backendIndex)] = lastPresentedRenderer_;
            backendLastRequest_[size_t(outcome.backendIndex)] =
                nativePresentationFailed ? requestedRendererStamp() : outcome.requestStamp;
            updateRendererStamp();
        }
        if (outcome.directPresented)
        {
            // Vulkan submitted a swapchain image directly. No hidden
            // QImage and no per-frame GPU-to-CPU readback.
            lastRenderedPose_ = outcome.cameraPose;
            hasRenderedPose_ = true;
            lastCompletedRenderMs_ = outcome.milliseconds;
            recordImagePresented(); // count submitted/presented frames
        }
        if (!outcome.image.isNull())
        {
            lastImage_ = outcome.image;
            lastRenderedPose_ = outcome.cameraPose;
            hasRenderedPose_ = true;
            lastCompletedRenderMs_ = outcome.milliseconds;
            canvas_->setImage(outcome.image);
            canvas_->enableSkyCompositing(outcome.skyCompositing);
        }
        if (outcome.error.isEmpty() && (outcome.directPresented || !outcome.image.isNull()))
        {
            const QString milliseconds = QString::number(outcome.milliseconds, 'f', 1);
            const QString seconds = QString::number(outcome.milliseconds / 1000.0, 'f', 1);
            renderTimeLabel_->setText(QStringLiteral("Dernier rendu : %1 ms (%2 s)").arg(milliseconds, seconds));
            renderTimeLabel_->setToolTip(QStringLiteral("%1\nTemps de calcul : %2 ms (%3 s). "
                                                        "Cette valeur exclut certains traitements de post-production.")
                                             .arg(outcome.renderer, milliseconds, seconds));
        }
        // The completion message itself also remains until another action.
        // Errors leave the last successful render time visible.
        const int completedBackend = outcome.backendIndex;
        const bool directFallbackActive = directDisplayBox_ && directDisplayBox_->isChecked() &&
                                          completedBackend >= 0 && completedBackend < int(nativeDirectFailed_.size()) &&
                                          nativeDirectFailed_[size_t(completedBackend)];
        if (nativePresentationFailed || directFallbackActive)
        {
            const QString backend =
                completedBackend == 5 ? QStringLiteral("Vulkan")
                                      : (completedBackend == 6 ? QStringLiteral("LumaTrace") : QStringLiteral("DX11"));
            const QString detail = completedBackend >= 0 && completedBackend < int(nativeDirectFailureReason_.size()) &&
                                           !nativeDirectFailureReason_[size_t(completedBackend)].isEmpty()
                                       ? nativeDirectFailureReason_[size_t(completedBackend)]
                                       : QStringLiteral("cause native non disponible");
            const QString recovery = outcome.image.isNull() ? QStringLiteral("QImage de secours également indisponible")
                                                            : QStringLiteral("QImage actif après échec direct");
            statusBar()->showMessage(
                QStringLiteral("%1 — %2 : %3. Diagnostic : %4. Décochez puis recochez pour retenter.")
                    .arg(backend, recovery, detail, nativeDiagnosticLogPath()),
                30000);
        }
        else
            statusBar()->showMessage(
                outcome.error.isEmpty()
                    ? QStringLiteral("%1 — %2 ms").arg(outcome.renderer).arg(outcome.milliseconds, 0, 'f', 1)
                    : outcome.error);
        if (exploring_ && outcome.error.isEmpty() && (outcome.directPresented || !outcome.image.isNull()))
        {
            if (frameClock_.isValid())
            {
                const qint64 frameMs = frameClock_.elapsed();
                if (frameMs > 0)
                    renderTimeLabel_->setText(
                        QStringLiteral("%1 ms · %2 FPS").arg(frameMs).arg(1000.0 / double(frameMs), 0, 'f', 1));
            }
        }
        else if (exploring_ && !outcome.error.isEmpty())
        {
            stopExploration();
        }
        if (pending_)
        {
            pending_ = false;
            requestRender();
        }
        else if (exploring_ && !sceneLoading_ && runtime_)
            requestRender();
    });
    // Return from the constructor immediately. The first scene is built
    // on a worker after show(), so Windows always shows the main window.
    canvas_->setPlaceholder(QStringLiteral("CSE WARP V17\nChargement de la scène…"));
    if (!scenePath.isEmpty())
        QTimer::singleShot(0, this, [this, scenePath] { openScene(scenePath); });
}

void V17Window::smokeNativeUi()
{
    QTimer::singleShot(0, this, [this] {
#if defined(_WIN32)
        auto *quick = findChild<QComboBox *>(QStringLiteral("v17QuickRenderer"));
        QSet<quintptr> handles;
        bool allWindows = true;
        for (auto *view : backendWindows_)
        {
            if (!view || !view->isWindow())
                allWindows = false;
            else
                handles.insert(quintptr(view->winId()));
        }
        const bool dx11Ready = backendBox_ && backendBox_->count() == 7 && backendWindows_[4] &&
                               backendWindows_[4]->isWindow() && backendBox_->currentIndex() == 4 &&
                               directDisplayBox_->isChecked() &&
                               backendBox_->itemText(5).contains(QStringLiteral("Vulkan")) &&
                               (backendBox_->model()->flags(backendBox_->model()->index(5, 0)) & Qt::ItemIsEnabled) &&
                               quick && quick->count() == 7 && quick->itemText(5).contains(QStringLiteral("Vulkan")) &&
                               canvas_ == backendWindows_[4] && canvas_->testAttribute(Qt::WA_NativeWindow);
        backendBox_->setCurrentIndex(5);
        const bool vulkanVisiblePath = nativeDirectSelected() && directDisplayBox_->isEnabled() &&
                                       canvas_ == backendWindows_[5] && canvas_ != backendWindows_[4] &&
                                       backendWindows_[5]->isVisible() && !backendWindows_[4]->isVisible();
        backendBox_->setCurrentIndex(4);
        selectBackendOutputWindow();
        const bool ok = dx11Ready && vulkanVisiblePath && allWindows && handles.size() == 7;
        std::fprintf(stderr, "V17 BACKEND WINDOWS: %s, seven unique top-level HWNDs\n", ok ? "PASS" : "FAIL");
        QCoreApplication::exit(ok ? 0 : 46);
#else
            QCoreApplication::exit(0);
#endif
    });
}

void V17Window::smokeSceneSwitch(const QString &secondScene)
{
    auto *poll = new QTimer(this);
    auto phase = std::make_shared<int>(0);
    poll->setInterval(100);
    connect(poll, &QTimer::timeout, this, [this, secondScene, poll, phase] {
        if (*phase == 0)
        {
            if (sceneLoading_ || !runtime_)
                return;
            *phase = 1;
            openScene(secondScene);
            return;
        }
        if (sceneLoading_ || !runtime_ || watcher_.isRunning() || pending_ || lastImage_.isNull())
            return;
        if (QFileInfo(scenePath_).absoluteFilePath() != QFileInfo(secondScene).absoluteFilePath())
        {
            std::fprintf(stderr, "V17 SCENE SWITCH: wrong scene loaded\\n");
            QCoreApplication::exit(22);
            return;
        }
        std::fprintf(stderr, "V17 SCENE SWITCH: new scene rendered %dx%d\\n", lastImage_.width(), lastImage_.height());
        poll->stop();
        QCoreApplication::exit(0);
    });
    poll->start();
    QTimer::singleShot(120000, this, [] {
        std::fprintf(stderr, "V17 SCENE SWITCH: timed out\\n");
        QCoreApplication::exit(23);
    });
}

void V17Window::smokeRepeatRaster()
{
    backendBox_->setCurrentIndex(4);
    auto count = std::make_shared<int>(0);
    auto firstThread = std::make_shared<quintptr>(0);
    connect(&watcher_, &QFutureWatcher<Result>::finished, this, [this, count, firstThread] {
        if (watcher_.isRunning() || sceneLoading_ || !runtime_)
            return;
        const Result result = watcher_.result();
        if (result.generation != renderSerial_->load())
            return;
        if (!result.gpu || result.image.isNull() || !result.error.isEmpty())
        {
            std::fprintf(stderr, "V17 REPEAT RASTER: failed render %d: %s\n", *count + 1,
                         result.error.toUtf8().constData());
            QCoreApplication::exit(31);
            return;
        }
        if (*count == 0)
        {
            *firstThread = result.renderThread;
            std::fprintf(stderr, "V17 REPEAT RASTER: first GPU frame %.2f ms, worker %llu\n", result.milliseconds,
                         static_cast<unsigned long long>(result.renderThread));
            ++*count;
            QTimer::singleShot(0, this, [this] { requestRender(); });
            return;
        }
        if (result.renderThread != *firstThread)
        {
            std::fprintf(stderr, "V17 REPEAT RASTER: GPU worker changed!\n");
            QCoreApplication::exit(32);
            return;
        }
        std::fprintf(stderr, "V17 REPEAT RASTER: second GPU frame %.2f ms, same worker %llu\n", result.milliseconds,
                     static_cast<unsigned long long>(result.renderThread));
        QCoreApplication::exit(0);
    });
    QTimer::singleShot(120000, this, [] {
        std::fprintf(stderr, "V17 REPEAT RASTER: timed out\n");
        QCoreApplication::exit(33);
    });
}

void V17Window::smokeWalkingCollisionPreference()
{
    auto *poll = new QTimer(this);
    poll->setInterval(100);
    connect(poll, &QTimer::timeout, this, [this, poll] {
        if (sceneLoading_ || !runtime_ || !walkingModeBox_)
            return;
        poll->stop();
        QSettings prefs(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        const QVariant previous = prefs.value(QStringLiteral("Exploration/WalkingV17_600"));
        walkingModeBox_->setChecked(true);
        prefs.setValue(QStringLiteral("Exploration/WalkingV17_600"), true);
        prefs.sync();
        selectAnomaly(0); // ANO-005 has a precisely measured camera pose.
        const bool okAfterSelect = walkingModeBox_->isChecked() && walkingCollisionSyncPending_ &&
                                   prefs.value(QStringLiteral("Exploration/WalkingV17_600")).toBool();
        returnBeforeAnomaly();
        prefs.sync();
        const bool okAfterReturn = walkingModeBox_->isChecked() && walkingCollisionSyncPending_ &&
                                   prefs.value(QStringLiteral("Exploration/WalkingV17_600")).toBool();
        // Restore the prior preference after the smoke test.
        {
            QSignalBlocker block(walkingModeBox_);
            walkingModeBox_->setChecked(previous.isValid() ? previous.toBool() : true);
        }
        if (previous.isValid())
            prefs.setValue(QStringLiteral("Exploration/WalkingV17_600"), previous);
        else
            prefs.remove(QStringLiteral("Exploration/WalkingV17_600"));
        prefs.sync();
        if (!okAfterSelect || !okAfterReturn)
        {
            std::fprintf(stderr, "V17 COLLISION PREFERENCE FAILED: afterSelect=%d afterReturn=%d\\n",
                         int(okAfterSelect), int(okAfterReturn));
            QCoreApplication::exit(38);
            return;
        }
        std::puts("V17 COLLISION PREFERENCE PASS: anomaly pose + return retain walking preference");
        QCoreApplication::exit(0);
    });
    poll->start();
    QTimer::singleShot(120000, this, [] {
        std::fprintf(stderr, "V17 COLLISION PREFERENCE: timeout\\n");
        QCoreApplication::exit(39);
    });
}

void V17Window::smokeScopedAim()
{
    auto *poll = new QTimer(this);
    poll->setInterval(100);
    connect(poll, &QTimer::timeout, this, [this, poll] {
        if (sceneLoading_ || !runtime_ || runtime_->cameras.isEmpty())
            return;
        poll->stop();
        if (!freeCameraActive_)
            activateFreeCamera();
        exploring_ = true;
        const float originalFov = freeCamera_.fov;
        const V originalEye = freeCamera_.eye;
        QMouseEvent down(QEvent::MouseButtonPress, QPointF(40, 40), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &down);
        const bool activated = rightMouseHeld_;
        scopeBlend_ = cse::scope::advance(scopeBlend_, rightMouseHeld_, .08f);
        freeCamera_.fov = cse::scope::effectiveFov(scopeBaseFov_, scopeBlend_);
        const bool magnified = freeCamera_.fov < originalFov - 1.f;
        QMouseEvent up(QEvent::MouseButtonRelease, QPointF(40, 40), Qt::RightButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &up);
        const bool released = !rightMouseHeld_;
        stopExploration();
        const bool restored = std::abs(freeCamera_.fov - originalFov) < .0005f;
        const bool unchanged = length(freeCamera_.eye - originalEye) < .0005f;
        if (!(activated && magnified && released && restored && unchanged))
        {
            std::fprintf(stderr, "V17 SCOPE MOUSE FAIL: down=%d zoom=%d up=%d fov=%d eye=%d\\n", int(activated),
                         int(magnified), int(released), int(restored), int(unchanged));
            QCoreApplication::exit(44);
            return;
        }
        std::puts("V17 SCOPE MOUSE PASS: press/release and original FOV, position preserved");
        QCoreApplication::exit(0);
    });
    poll->start();
    QTimer::singleShot(120000, this, [] {
        std::fprintf(stderr, "V17 SCOPE MOUSE TIMEOUT\\n");
        QCoreApplication::exit(45);
    });
}

void V17Window::smokeImmersiveMouse()
{
    auto *poll = new QTimer(this);
    poll->setInterval(100);
    connect(poll, &QTimer::timeout, this, [this, poll] {
        if (sceneLoading_ || !runtime_ || runtime_->cameras.isEmpty())
            return;
        poll->stop();
        activateFreeCamera();
        exploring_ = true;
        mouseCaptured_ = true;
        canvas_->setCursor(Qt::BlankCursor);
        const float originalYaw = yaw_;
        const V originalEye = freeCamera_.eye;
        const QPoint center = canvas_->rect().center();
        QMouseEvent move(QEvent::MouseMove, QPointF(center + QPoint(16, -9)), Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QCoreApplication::sendEvent(canvas_, &move);
        const bool freeLookWorked = std::abs(yaw_ - originalYaw) > .01f;
        const bool eyeUnchanged = length(freeCamera_.eye - originalEye) < .0005f;
        const bool fpsReady = canvas_->isVisible();
        immersiveAction_->setChecked(true);
        QCoreApplication::processEvents();
        const bool full = canvas_->isFullScreen();
        immersiveAction_->setChecked(false);
        QCoreApplication::processEvents();
        const bool back = !canvas_->isFullScreen() && menuBar()->isVisible() && statusBar()->isVisible();
        stopExploration();
        if (!(freeLookWorked && eyeUnchanged && full && back && fpsReady))
        {
            std::fprintf(stderr, "IMMERSIVE MOUSE FAILED: move=%d eye=%d full=%d back=%d canvas=%d\\n",
                         int(freeLookWorked), int(eyeUnchanged), int(full), int(back), int(fpsReady));
            QCoreApplication::exit(49);
            return;
        }
        std::puts("IMMERSIVE MOUSE PASS: no-button look, true F11 fullscreen and restored editor");
        QCoreApplication::exit(0);
    });
    poll->start();
    QTimer::singleShot(120000, this, [] {
        std::fprintf(stderr, "IMMERSIVE MOUSE TIMEOUT\\n");
        QCoreApplication::exit(50);
    });
}

V17Window::~V17Window()
{
    audioTimer_.stop();
    stopRadio();
    renderSerial_->fetch_add(1);
    // Drain both producers before destroying backend output windows. Their
    // Win32 HWNDs must outlive DXGI swapchains and VkSurfaceKHR objects.
    loadWatcher_.waitForFinished();
    watcher_.waitForFinished();
#if defined(_WIN32)
    // Raster backends keep thread_local Vulkan/D3D11 contexts in the
    // dedicated one-worker pool. Destroy them on their owning thread
    // BEFORE their dedicated top-level HWNDs are destroyed.
    // This also covers closing directly after the first native frame.
    auto cleanup = QtConcurrent::run(&renderPool_, [] {
        releaseDirectX11NativeForCurrentThread();
        releaseVulkanDirectForCurrentThread();
        releaseVulkanPreviewForCurrentThread();
    });
    cleanup.waitForFinished();
#endif
    for (auto *view : backendWindows_)
    {
        if (view)
        {
            view->setNativePresentation(false);
            delete view;
        }
    }
}

void V17Window::closeEvent(QCloseEvent *event)
{
    if (closingRequested_)
    {
        if (watcher_.isRunning() || loadWatcher_.isRunning())
        {
            event->ignore();
            return;
        }
        for (auto *view : backendWindows_)
            if (view)
                view->hide();
        QMainWindow::closeEvent(event);
        if (event->isAccepted())
            QCoreApplication::quit();
        return;
    }
    closingRequested_ = true;
    const auto shutdownComplete = shutdownComplete_;
    std::thread([shutdownComplete] {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (shutdownComplete->load(std::memory_order_acquire))
            return;
        std::fprintf(stderr, "Shutdown timeout: renderer cleanup is still active; terminating process.\n");
#if defined(_WIN32)
        if (!TerminateProcess(GetCurrentProcess(), 2))
            std::fprintf(stderr, "TerminateProcess failed with error %lu; exiting immediately.\n", GetLastError());
#endif
        std::_Exit(2);
    }).detach();
    stopExploration();
    renderSerial_->fetch_add(1);
    pending_ = false;
    frameTimer_.stop();
    skyAnimationTimer_.stop();
    dayNightTimer_.stop();
    audioTimer_.stop();
    stopRadio();
    if (bakeGiProcess_ && bakeGiProcess_->state() != QProcess::NotRunning)
    {
        bakeGiProcess_->kill();
        bakeGiProcess_->waitForFinished(1000);
    }
    if (watcher_.isRunning() || loadWatcher_.isRunning())
    {
        statusBar()->showMessage(QStringLiteral("Fermeture : arrêt du chargement ou du rendu en cours…"));
        event->ignore();
        return;
    }
    for (auto *view : backendWindows_)
        if (view)
            view->hide();
    QMainWindow::closeEvent(event);
    if (event->isAccepted())
        QCoreApplication::quit();
}

bool V17Window::event(QEvent *e)
{
    if (e->type() == QEvent::WindowActivate && exploring_)
    {
        QTimer::singleShot(0, this, [this] {
            if (exploring_ && !closingRequested_)
                captureMouseLook();
        });
    }
    if (e->type() == QEvent::WindowDeactivate)
    {
        // Output views are separate top-level windows. Give Qt one event
        // turn to activate the renderer before treating this as Alt+Tab.
        QTimer::singleShot(0, this, [this] {
            if (!isActiveWindow() && (!canvas_ || !canvas_->isActiveWindow()))
            {
                releaseMouseLook();
                rightMouseHeld_ = false;
            }
        });
    }
    return QMainWindow::event(e);
}

bool V17Window::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != this && event->type() == QEvent::Close)
    {
        if (!closingRequested_)
            close();
        event->ignore();
        return true;
    }
    if (watched == canvas_ && event->type() == QEvent::WindowActivate && exploring_)
        QTimer::singleShot(0, this, [this] {
            if (exploring_)
                captureMouseLook();
        });
    if (watched == canvas_ && event->type() == QEvent::Resize && nativeDirectSelected() && runtime_ && !sceneLoading_)
    {
        // The backend's private swapchain must follow its top-level HWND.
        // Defer until Qt finishes the native resize transaction.
        QTimer::singleShot(0, this, [this] {
            if (nativeDirectSelected() && !closingRequested_)
                requestRender();
        });
    }
    if (watched == canvas_ && exploring_)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            if (event->type() == QEvent::KeyPress)
                keyPressEvent(static_cast<QKeyEvent *>(event));
            else
                keyReleaseEvent(static_cast<QKeyEvent *>(event));
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto *e = static_cast<QMouseEvent *>(event);
            if (!mouseCaptured_)
                captureMouseLook();
            if (e->button() == Qt::RightButton)
            {
                rightMouseHeld_ = true;
                focusRenderViewport();
                return true;
            }
            if (e->button() == Qt::LeftButton)
            {
                leftMouseHeld_ = true;
                focusRenderViewport();
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease)
        {
            auto *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::RightButton)
            {
                rightMouseHeld_ = false;
                return true;
            }
            if (e->button() == Qt::LeftButton)
            {
                leftMouseHeld_ = false;
                return true;
            }
        }
        if (event->type() == QEvent::MouseMove && mouseCaptured_)
        {
            auto *e = static_cast<QMouseEvent *>(event);
            const QPoint center = canvas_->rect().center();
            const QPoint delta = e->pos() - center;
            // Qt generates a synthetic move when the cursor is recentered.
            // Ignore it. No click is required to look around.
            if (delta.isNull())
                return true;
            const float dx = clampf(float(delta.x()), -160.f, 160.f);
            const float dy = clampf(float(delta.y()), -160.f, 160.f);
            const float sensitivity = .005f * cse::scope::mouseSensitivity(scopeBlend_);
            yaw_ += dx * sensitivity;
            pitch_ = clampf(pitch_ - dy * sensitivity, -1.48f, 1.48f);
            lastMouseLookMotion_.restart();
            updateLook();
            QCursor::setPos(canvas_->mapToGlobal(center));
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void V17Window::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_F11 && immersiveAction_)
    {
        immersiveAction_->setChecked(!immersiveAction_->isChecked());
        e->accept();
        return;
    }
    if (exploring_)
    {
        if (e->key() == Qt::Key_Escape)
        {
            stopExploration();
            if (immersiveAction_ && immersiveAction_->isChecked())
                immersiveAction_->setChecked(false);
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Tab)
        {
            if (mouseCaptured_)
                releaseMouseLook();
            else
                captureMouseLook();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_E && !e->isAutoRepeat() && walkingModeBox_ && walkingModeBox_->isChecked() &&
            scenePath_.contains(QStringLiteral("benchmark-civic"), Qt::CaseInsensitive))
        {
            if (walkingCollisionSyncPending_ && runtime_)
            {
                walk::spawn(runtime_->renderScene, freeCamera_.eye, walkingState_);
                freeCamera_.eye = walk::eye(walkingState_);
                walkingCollisionSyncPending_ = false;
                updateLook();
            }
            const V feet = walkingState_.feet;
            if (poolLadderMotion_.begin({feet.x, feet.y, feet.z}))
            {
                keysDown_.clear();
                statusBar()->showMessage(poolLadderMotion_.up
                                             ? QStringLiteral("Piscine : montée de l'échelle en cours…")
                                             : QStringLiteral("Piscine : descente de l'échelle en cours…"),
                                         3000);
                e->accept();
                return;
            }
        }
        if (!e->isAutoRepeat())
        {
            const bool wasDown = keysDown_.contains(e->key());
            keysDown_.insert(e->key());
            if (e->key() == Qt::Key_Space && !wasDown)
                jumpRequested_ = true;
            if (e->key() == Qt::Key_Shift)
            {
                if (isLeftShift(e) || (!isRightShift(e) && !rightShiftHeld_))
                    leftShiftHeld_ = true;
                if (isRightShift(e))
                    rightShiftHeld_ = true;
                updateExplorationSpeedIndicator();
            }
            if (e->key() == Qt::Key_Control)
            {
                if (isLeftControl(e) || (!isRightControl(e) && !rightControlHeld_))
                    leftControlHeld_ = true;
                if (isRightControl(e))
                    rightControlHeld_ = true;
                updateExplorationSpeedIndicator();
            }
        }
        e->accept();
        return;
    }
    QMainWindow::keyPressEvent(e);
}

void V17Window::keyReleaseEvent(QKeyEvent *e)
{
    if (exploring_)
    {
        if (!e->isAutoRepeat())
        {
            keysDown_.remove(e->key());
            if (e->key() == Qt::Key_Shift)
            {
                if (isLeftShift(e))
                    leftShiftHeld_ = false;
                else if (isRightShift(e))
                    rightShiftHeld_ = false;
                else
                {
                    leftShiftHeld_ = false;
                    rightShiftHeld_ = false;
                }
                updateExplorationSpeedIndicator();
            }
            if (e->key() == Qt::Key_Control)
            {
                if (isLeftControl(e))
                    leftControlHeld_ = false;
                else if (isRightControl(e))
                    rightControlHeld_ = false;
                else
                {
                    leftControlHeld_ = false;
                    rightControlHeld_ = false;
                }
                updateExplorationSpeedIndicator();
            }
        }
        e->accept();
        return;
    }
    QMainWindow::keyReleaseEvent(e);
}
