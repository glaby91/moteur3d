#include "moteur3d_v17_window.h"

using namespace m3d;

bool V17Window::vulkanDirectAvailable() const
{
#if defined(_WIN32) && M3D_V17_VULKAN_ENABLED
    return true;
#else
    return false;
#endif
}

bool V17Window::nativeDirectSelected() const
{
#ifdef _WIN32
    // Each native backend presents into its own backend-specific top-level
    // ImageView HWND; DXGI and Vulkan never share a target window.
    if (!backendBox_ || !directDisplayBox_ || !directDisplayBox_->isChecked())
        return false;
    const int index = backendBox_->currentIndex();
    return (index == 4 || index == 6 || (index == 5 && vulkanDirectAvailable())) && !nativeDirectFailed_[size_t(index)];
#else
    return false;
#endif
}

void V17Window::focusRenderViewport()
{
    if (canvas_)
    {
        canvas_->setFocus(Qt::OtherFocusReason);
        canvas_->raise();
        canvas_->activateWindow();
    }
}

void V17Window::restoreExplorationViewportFocus()
{
    if (!exploring_)
        return;
    // Changing the backend or direct-display mode moves focus between
    // independent top-level views. Rebind mouse-look after Qt activates
    // the newly selected output window.
    releaseMouseLook();
    QTimer::singleShot(0, this, [this] {
        if (!exploring_ || closingRequested_)
            return;
        focusRenderViewport();
        captureMouseLook();
    });
}

void V17Window::arrangeExplorationWindows()
{
    if (explorationWindowLayoutActive_ || !canvas_)
        return;
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    if (area.width() < 900 || area.height() < 500)
        return;

    explorationSavedMainGeometry_ = geometry();
    explorationSavedRenderGeometry_ = canvas_->geometry();
    explorationSavedMinimumSize_ = minimumSize();
    explorationSavedMainMaximized_ = isMaximized();
    explorationSavedMainFullScreen_ = isFullScreen();
    explorationSavedRenderMaximized_ = canvas_->isMaximized();
    explorationSavedRenderFullScreen_ = canvas_->isFullScreen();
    explorationSavedSceneDockVisible_ = sceneExplorerDock_ && sceneExplorerDock_->isVisible();
    explorationSavedControlsDockVisible_ = renderControlsDock_ && renderControlsDock_->isVisible();
    explorationSavedReviewDockVisible_ = anomalyReviewDock_ && anomalyReviewDock_->isVisible();
    explorationSavedMenuVisible_ = menuBar() && menuBar()->isVisible();
    explorationWindowLayoutActive_ = true;

    // Compact the controls into the right-hand band; hide secondary panels
    // and keep the renderer in the larger left window.
    if (sceneExplorerDock_)
        sceneExplorerDock_->hide();
    if (anomalyReviewDock_)
        anomalyReviewDock_->hide();
    if (renderControlsDock_)
    {
        renderControlsDock_->show();
        renderControlsDock_->raise();
    }
    if (centralWidget())
        centralWidget()->hide();
    if (menuBar())
        menuBar()->hide();
    setMinimumSize(360, 500);
    const int bandWidth = std::clamp(area.width() / 4, 380, 480);
    const int renderWidth = area.width() - bandWidth;
    const QRect renderRect(area.left(), area.top(), renderWidth, area.height());
    const QRect bandRect(area.left() + renderWidth, area.top(), bandWidth, area.height());
    // setGeometry() does not leave Qt's maximized state. Normalize both
    // top-level windows first so the requested left/right layout takes effect.
    if (canvas_->isMinimized() || canvas_->isMaximized() || canvas_->isFullScreen())
        canvas_->showNormal();
    canvas_->setGeometry(renderRect);
    if (isMinimized() || isMaximized() || isFullScreen())
        showNormal();
    setGeometry(bandRect);
    show();
    raise();
    canvas_->show();
    focusRenderViewport();
    updateRendererStamp();
    QTimer::singleShot(0, this, [this] {
        if (exploring_ && !closingRequested_)
        {
            focusRenderViewport();
            captureMouseLook();
        }
    });
}

void V17Window::restoreExplorationWindows()
{
    if (!explorationWindowLayoutActive_)
        return;
    explorationWindowLayoutActive_ = false;
    setMinimumSize(explorationSavedMinimumSize_);
    if (centralWidget())
        centralWidget()->show();
    if (menuBar())
        menuBar()->setVisible(explorationSavedMenuVisible_);
    if (sceneExplorerDock_)
        sceneExplorerDock_->setVisible(explorationSavedSceneDockVisible_);
    if (renderControlsDock_)
        renderControlsDock_->setVisible(explorationSavedControlsDockVisible_);
    if (anomalyReviewDock_)
        anomalyReviewDock_->setVisible(explorationSavedReviewDockVisible_);
    if (!explorationSavedMainGeometry_.isEmpty())
        setGeometry(explorationSavedMainGeometry_);
    if (canvas_ && !explorationSavedRenderGeometry_.isEmpty())
        canvas_->setGeometry(explorationSavedRenderGeometry_);
    if (explorationSavedMainFullScreen_)
        showFullScreen();
    else if (explorationSavedMainMaximized_)
        showMaximized();
    if (canvas_)
    {
        if (explorationSavedRenderFullScreen_)
            canvas_->showFullScreen();
        else if (explorationSavedRenderMaximized_)
            canvas_->showMaximized();
    }
    raise();
    activateWindow();
    updateRendererStamp();
}

QString V17Window::describeNativeWindow(quintptr nativeHandle, QSize requested, qreal dpr) const
{
#if defined(_WIN32)
    const HWND hwnd = reinterpret_cast<HWND>(nativeHandle);
    DWORD ownerProcess = 0;
    const DWORD ownerThread = hwnd ? GetWindowThreadProcessId(hwnd, &ownerProcess) : 0;
    RECT client{};
    const bool hasClient = hwnd && GetClientRect(hwnd, &client);
    wchar_t className[128]{};
    const int classLength = hwnd ? GetClassNameW(hwnd, className, 128) : 0;
    const QString classText =
        classLength > 0 ? QString::fromWCharArray(className, classLength) : QStringLiteral("<indisponible>");
    const auto hinstance = hwnd ? GetWindowLongPtrW(hwnd, GWLP_HINSTANCE) : 0;
    const auto style = hwnd ? GetWindowLongPtrW(hwnd, GWL_STYLE) : 0;
    const auto exStyle = hwnd ? GetWindowLongPtrW(hwnd, GWL_EXSTYLE) : 0;
    return QStringLiteral("HWND=0x%1 IsWindow=%2 ownerThread=%3 process=%4 HINSTANCE=0x%5 "
                          "style=0x%6 exStyle=0x%7 class=%8 client=%9x%10 target=%11x%12 DPR=%13")
        .arg(QString::number(qulonglong(nativeHandle), 16))
        .arg(hwnd && IsWindow(hwnd) ? 1 : 0)
        .arg(ownerThread)
        .arg(ownerProcess)
        .arg(QString::number(qulonglong(hinstance), 16))
        .arg(QString::number(qulonglong(style), 16))
        .arg(QString::number(qulonglong(exStyle), 16))
        .arg(classText)
        .arg(hasClient ? client.right - client.left : 0)
        .arg(hasClient ? client.bottom - client.top : 0)
        .arg(requested.width())
        .arg(requested.height())
        .arg(dpr, 0, 'f', 3);
#else
    Q_UNUSED(nativeHandle);
    return QStringLiteral("Win32 window diagnostics unavailable; target=%1x%2 DPR=%3")
        .arg(requested.width())
        .arg(requested.height())
        .arg(dpr, 0, 'f', 3);
#endif
}

QString V17Window::nativeDiagnosticLogPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
           QStringLiteral("/CSE WARP/Diagnostics/V17.523-native-presentation.jsonl");
}

void V17Window::saveNativePresentationDiagnostic(const QString &backend, const QString &reason,
                                                 const QString &windowDetails)
{
    const QString path = nativeDiagnosticLogPath();
    const QString directory = QFileInfo(path).absolutePath();
    QDir().mkpath(directory);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        const QJsonObject record{{QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
                                 {QStringLiteral("version"), QStringLiteral("17.523")},
                                 {QStringLiteral("backend"), backend},
                                 {QStringLiteral("reason"), reason},
                                 {QStringLiteral("window"), windowDetails}};
        file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
        file.write("\n");
    }
    const QString summary = reason.isEmpty() ? QStringLiteral("cause native non fournie par le backend") : reason;
    statusBar()->showMessage(QStringLiteral("%1 direct indisponible ; QImage actif. %2 — diagnostic détaillé : %3. "
                                            "Décochez puis recochez l’option pour retenter.")
                                 .arg(backend, summary, path),
                             30000);
}

QString V17Window::requestedRendererStamp() const
{
    if (!backendBox_)
        return QStringLiteral("Rendu en attente");
    const int index = backendBox_->currentIndex();
    QString name = backendBox_->currentText();
    if (index == 4)
        name = QStringLiteral("V17 D3D11 Raster");
    else if (index == 5)
        name = QStringLiteral("V17 Vulkan Raster");
    else if (index == 6)
        name = QStringLiteral("V17 LumaTrace BVH4");
    QSize requestedSize = qualityBox_ ? qualityBox_->currentData().toSize() : QSize{};
    if (!requestedSize.isValid() || requestedSize.isEmpty())
    {
        requestedSize = canvas_ ? canvas_->size().boundedTo(QSize(1600, 900)) : QSize(1000, 680);
        requestedSize.setWidth(std::max(64, requestedSize.width()));
        requestedSize.setHeight(std::max(64, requestedSize.height()));
    }
    const bool direct = (index == 4 || index == 5 || index == 6) && directDisplayBox_ && directDisplayBox_->isChecked();
    if (direct && canvas_)
    {
        const qreal dpr = canvas_->devicePixelRatioF();
        requestedSize = QSize(std::max(320, int(canvas_->width() * dpr)), std::max(200, int(canvas_->height() * dpr)));
    }
    if (direct && index >= 0 && index < int(nativeDirectFailed_.size()) && nativeDirectFailed_[size_t(index)])
        name += QStringLiteral(" — direct demandé, QImage actif après échec");
    else
        name += direct ? QStringLiteral(" — Direct GPU (swapchain)") : QStringLiteral(" — affichage Qt (QImage)");
    name += QStringLiteral(" — %1×%2 demandé").arg(requestedSize.width()).arg(requestedSize.height());
    return name;
}

void V17Window::updateRendererStamp()
{
    const QString requested = requestedRendererStamp();
    const int index = backendBox_ ? std::clamp(backendBox_->currentIndex(), 0, int(backendWindows_.size()) - 1) : 0;
    const QString displayed = backendLastRenderer_[size_t(index)];
    const bool pending = requested != backendLastRequest_[size_t(index)];
    if (canvas_)
    {
        canvas_->setRendererStamp(displayed, pending ? requested : QString{});
        canvas_->setHudVisible(rendererHudVisible_);
    }
    if (nativeRendererOverlay_)
    {
        const QString shown = displayed.isEmpty() ? QStringLiteral("Affiché : aucun rendu pour l’instant")
                                                  : QStringLiteral("Affiché : %1").arg(displayed);
        const QString waiting = pending ? QStringLiteral("Demandé : %1").arg(requested) : QString{};
        // Direct swapchain presentation bypasses ImageView::paintEvent,
        // including its camera HUD. Keep the live camera readout on the
        // native output window as well as in the controls status bar.
        const QString camera =
            rendererHudVisible_ && cameraHudEnabled_ && nativeHudLabel_ && !nativeHudLabel_->text().isEmpty()
                ? nativeHudLabel_->text()
                : QString{};
        QStringList overlayLines;
        overlayLines << shown;
        if (!waiting.isEmpty())
            overlayLines << waiting;
        if (!camera.isEmpty())
            overlayLines << camera;
        const QString overlay = overlayLines.join(QStringLiteral("   |   "));
        if (canvas_)
        {
            nativeRendererOverlay_->move(canvas_->mapToGlobal(QPoint(12, 12)));
            const int width = std::max(220, canvas_->width() - 24);
            nativeRendererOverlay_->setFixedSize(width, 34);
            const QFontMetrics metrics(nativeRendererOverlay_->font());
            nativeRendererOverlay_->setText(metrics.elidedText(overlay, Qt::ElideRight, width - 20));
        }
        nativeRendererOverlay_->setVisible(rendererHudVisible_ && nativeDirectSelected() && canvas_ &&
                                           canvas_->isVisible());
        if (nativeDirectSelected() && canvas_ && canvas_->isVisible())
            nativeRendererOverlay_->raise();
    }
}

void V17Window::selectBackendOutputWindow()
{
    if (!backendBox_)
        return;
    const int index = std::clamp(backendBox_->currentIndex(), 0, int(backendWindows_.size()) - 1);
    const bool backendChanged = outputBackendIndex_ != index;
    const QRect explorationRenderGeometry = canvas_ ? canvas_->geometry() : QRect{};
    outputBackendIndex_ = index;
    canvas_ = backendWindows_[size_t(index)];
    // Keep the native status overlay owned and stacked with the renderer
    // HWND, not with the controls window on the other side of the screen.
    if (nativeRendererOverlay_ && nativeRendererOverlay_->parentWidget() != canvas_)
        nativeRendererOverlay_->setParent(canvas_, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    canvas_->setWindowTitle(QStringLiteral("CSE WARP V%1 — %2 — fenêtre dédiée")
                                .arg(CSE_WARP_PRODUCT_VERSION)
                                .arg(backendBox_->itemText(index)));
    const bool direct = nativeDirectSelected();
    if (backendChanged || direct)
    {
        // The global export/sky state belongs to the selected output.
        // Never carry another backend's QImage into this window.
        lastImage_ = QImage{};
        hasRenderedPose_ = false;
        canvas_->setImage(QImage{});
        canvas_->enableSkyCompositing(false);
        backendLastRenderer_[size_t(index)].clear();
        backendLastRequest_[size_t(index)].clear();
    }
    for (int i = 0; i < int(backendWindows_.size()); ++i)
    {
        auto *view = backendWindows_[size_t(i)];
        if (!view)
            continue;
        const bool preserveInFlightPresent = watcher_.isRunning() && activeRenderDirect_ && i == activeRenderBackend_;
        view->setNativePresentation((i == index && direct) || preserveInFlightPresent);
        if (i == index)
        {
            if (isVisible())
            {
                // Preserve the established left render pane when the
                // user changes backend during an active exploration.
                if (exploring_ && explorationWindowLayoutActive_ && !explorationRenderGeometry.isEmpty())
                    view->setGeometry(explorationRenderGeometry);
                if (view->isMinimized())
                    view->showNormal();
                else
                    view->show();
                view->raise();
            }
        }
        else
            view->hide();
    }
    updateRendererStamp();
    if (direct)
    {
        statusBar()->showMessage(
            QStringLiteral("Fenêtre backend dédiée — affichage GPU direct (swapchain), sans QImage ; "
                           "décocher pour les captures PNG"),
            7000);
    }
}

void V17Window::resetPacing()
{
    pacingSamples_.clear();
    pacingRejected_ = 0;
    pacingQueued_ = 0;
    lastPresentMs_ = -1;
    lastCompletedRenderMs_ = 0.0;
    fpsFiltered_ = 0.0;
    pacingClock_.restart();
}

void V17Window::recordImagePresented()
{
    if (!pacingClock_.isValid())
        pacingClock_.start();
    const qint64 ms = pacingClock_.elapsed();
    if (exploring_)
    {
        cse::pacing::Sample sample{};
        sample.timestampMs = ms;
        sample.intervalMs = lastPresentMs_ >= 0 ? double(ms - lastPresentMs_) : 0.0;
        sample.renderMs = lastCompletedRenderMs_;
        sample.pendingRequests = unsigned(pacingQueued_);
        if (pacingSamples_.size() >= pacingCapacity_)
            pacingSamples_.erase(pacingSamples_.begin());
        pacingSamples_.push_back(sample);
        const double interval = lastPresentMs_ >= 0 ? double(ms - lastPresentMs_) : 0.0;
        if (interval >= 1.0)
        {
            const double fps = 1000.0 / interval;
            fpsFiltered_ = fpsFiltered_ == 0.0 ? fps : (fpsFiltered_ * .72 + fps * .28);
            canvas_->setFpsOverlay(fpsFiltered_, true);
            if (nativeFpsLabel_)
                nativeFpsLabel_->setText(QStringLiteral("%1 FPS").arg(fpsFiltered_, 0, 'f', 1));
        }
    }
    lastPresentMs_ = ms;
}

void V17Window::exportPacing()
{
    if (pacingSamples_.empty())
    {
        QMessageBox::information(this, QStringLiteral("Benchmark fluidité"),
                                 QStringLiteral("Démarrer l'exploration et se déplacer avant d'exporter."));
        return;
    }
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/CSE WARP/Benchmarks");
    QDir().mkpath(root);
    const QString name = QStringLiteral("V%1-PACING-").arg(CSE_WARP_PRODUCT_VERSION) +
                         QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    const auto stats = cse::pacing::summarize(pacingSamples_, pacingRejected_, pacingQueued_);
    QJsonArray records;
    QString csv = QStringLiteral("timestamp_ms,present_interval_ms,render_ms,requests_queued\n");
    for (const auto &sample : pacingSamples_)
    {
        records.push_back(QJsonObject{{"timestampMs", double(sample.timestampMs)},
                                      {"presentIntervalMs", sample.intervalMs},
                                      {"renderMs", sample.renderMs},
                                      {"queuedRequests", int(sample.pendingRequests)}});
        csv += QStringLiteral("%1,%2,%3,%4\n")
                   .arg(sample.timestampMs)
                   .arg(sample.intervalMs, 0, 'f', 3)
                   .arg(sample.renderMs, 0, 'f', 3)
                   .arg(sample.pendingRequests);
    }
    const QJsonObject summary{
        {"version", QStringLiteral(CSE_WARP_PRODUCT_VERSION)},
        {"scene", scenePath_},
        {"backend", backendBox_->currentText()},
        {"frameCount", int(stats.frames)},
        {"medianPresentMs", stats.medianPresentMs},
        {"meanPresentMs", stats.meanPresentMs},
        {"p95PresentMs", stats.p95PresentMs},
        {"p99PresentMs", stats.p99PresentMs},
        {"worstPresentMs", stats.worstPresentMs},
        {"medianRenderMs", stats.medianRenderMs},
        {"stutterPercent", stats.stutterPercent},
        {"queuedRequests", double(stats.queued)},
        {"invalidatedFrames", double(stats.invalidated)},
        {"zPrepass", backendBox_->currentIndex() >= 4 &&
                         qEnvironmentVariable("M3D_V17_DISABLE_Z_PREPASS") != QStringLiteral("1") &&
                         qEnvironmentVariable("M3D_V17_EXPERIMENT_Z_PREPASS") != QStringLiteral("0")},
        {"lightAabbCull", qEnvironmentVariable("M3D_V17_DISABLE_LIGHT_AABB_CULL") != QStringLiteral("1") &&
                              qEnvironmentVariable("M3D_V17_EXPERIMENT_LIGHT_AABB_CULL") != QStringLiteral("0")}};
    QFile jsonFile(root + QStringLiteral("/") + name + QStringLiteral(".json"));
    QFile csvFile(root + QStringLiteral("/") + name + QStringLiteral(".csv"));
    if (!jsonFile.open(QIODevice::WriteOnly) || !csvFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, QStringLiteral("Benchmark"), QStringLiteral("Impossible d'écrire les résultats."));
        return;
    }
    jsonFile.write(QJsonDocument(QJsonObject{{"summary", summary}, {"frames", records}}).toJson());
    csvFile.write(csv.toUtf8());
    statusBar()->showMessage(QStringLiteral("Benchmark fluidité exporté : %1").arg(root), 12000);
}

void V17Window::requestRender()
{
    if (closingRequested_)
        return;
    updateRendererStamp();
    if (sceneLoading_ || !runtime_ || runtime_->cameras.isEmpty())
        return;
    // Select the persistent top-level output HWND dedicated to this backend
    // before creating its DXGI/Vulkan surface.
    if (backendBox_)
    {
        const int selected = std::clamp(backendBox_->currentIndex(), 0, int(backendWindows_.size()) - 1);
        if (canvas_ != backendWindows_[size_t(selected)] || !canvas_->isVisible() ||
            canvas_->nativePresentation() != nativeDirectSelected())
            selectBackendOutputWindow();
    }
    if (nativeDirectSelected())
    {
        if (!canvas_->isVisible())
            selectBackendOutputWindow();
        const QSize target = canvas_->size();
        const bool ready = canvas_->isVisible() && target.width() >= 320 && target.height() >= 200 &&
                           canvas_->testAttribute(Qt::WA_NativeWindow);
        if (!ready)
        {
            if (++nativeReadyAttempts_ > 40)
            {
                nativeReadyAttempts_ = 0;
                // Keep the requested mode checked, latch this backend to
                // QImage, and allow the user to retry by toggling it.
                const int failedBackend = backendBox_->currentIndex();
                if (failedBackend >= 0 && failedBackend < int(nativeDirectFailed_.size()))
                {
                    nativeDirectFailed_[size_t(failedBackend)] = true;
                    nativeDirectFailureReason_[size_t(failedBackend)] =
                        QStringLiteral("La fenêtre n’a pas fourni un HWND visible aux dimensions requises.");
                }
                const QString backend =
                    failedBackend == 5 ? QStringLiteral("Vulkan")
                                       : (failedBackend == 6 ? QStringLiteral("LumaTrace") : QStringLiteral("DX11"));
                const QString windowDetails =
                    describeNativeWindow(quintptr(canvas_->winId()), target, canvas_->devicePixelRatioF());
                if (failedBackend >= 0 && failedBackend < int(nativeDirectFailureReason_.size()))
                    saveNativePresentationDiagnostic(backend, nativeDirectFailureReason_[size_t(failedBackend)],
                                                     windowDetails);
                selectBackendOutputWindow();
                QTimer::singleShot(0, this, [this] {
                    if (!closingRequested_ && !sceneLoading_ && runtime_)
                        requestRender();
                });
            }
            else if (!nativeReadyRetryQueued_)
            {
                nativeReadyRetryQueued_ = true;
                QTimer::singleShot(50, this, [this] {
                    nativeReadyRetryQueued_ = false;
                    if (!closingRequested_ && !sceneLoading_ && runtime_)
                        requestRender();
                });
            }
            return;
        }
        nativeReadyAttempts_ = 0;
    }
    const auto serial = renderSerial_;
    if (watcher_.isRunning())
    {
        pending_ = true; // Camera movement must not invalidate a finished image.
        ++pacingQueued_;
        return;
    }
    const uint64_t token = serial->fetch_add(1) + 1;
    auto snapshot = runtime_;
    ImageView *const outputView = canvas_;
    const int requestedBackend = backendBox_->currentIndex();
    const bool directPresent = nativeDirectSelected();
    activeRenderBackend_ = requestedBackend;
    activeRenderDirect_ = directPresent;
    // V17.493 uses an independent 30 Hz Qt sky compositor. Vulkan WSI
    // bypasses that QWidget, so sample the SAME monotonic solar clock on
    // each native frame rather than the slower 1.5 s UI spinbox timer.
    // Never modify the approved solarMotion() or walking controller.
    const int backend = backendBox_->currentIndex();
    const float renderHour =
        (directPresent && dayNightCycleBox_ && dayNightCycleBox_->isChecked() && dayNightClock_.isValid())
            ? float(cse::weather::cycleHour(dayNightOriginHour_, double(dayNightClock_.elapsed()) / 1000.0))
            : snapshot->daylightHour;
    // Keep lighting smooth, but rebuild the expensive 1024² CPU shadow map
    // only once per simulated hour during DirectX day/night. At 24h/6min,
    // this cuts rebuild stalls from every 3.75s to every 15s without lowering map quality.
    // Keep the high-cost CPU shadow map fixed while the sun animates. Rebuilding
    // it during the cycle creates periodic multi-second stalls in both raster APIs.
    // The map is invalidated once when the cycle stops so the final sun position
    // receives a matching shadow.
    const bool animatedDayNight = dayNightCycleBox_ && dayNightCycleBox_->isChecked();
    const float shadowStepHours = animatedDayNight ? 24.f : .25f;
    const float shadowHour = std::floor(renderHour / shadowStepHours) * shadowStepHours;
    auto preset = activeCameraPose();
    updateCameraHud();
    QSize renderSize = qualityBox_->currentData().toSize();
    if (!renderSize.isValid() || renderSize.isEmpty())
    {
        // Render to the viewport's *actual* aspect ratio, not the monitor
        // resolution; that used to letterbox the 3D image in a tiny strip.
        // Keep a bounded pixel budget on integrated GPUs.
        renderSize = canvas_->size().boundedTo(QSize(1600, 900));
        renderSize.setWidth(std::max(64, renderSize.width()));
        renderSize.setHeight(std::max(64, renderSize.height()));
    }
    int w = renderSize.width();
    int h = renderSize.height();
    // A swapchain always targets this backend's visible top-level HWND
    // extent; the offscreen quality resolution is irrelevant in this mode.
    if (directPresent && outputView)
    {
        const qreal dpr = outputView->devicePixelRatioF();
        const QSize logicalSize = outputView->size();
        renderSize =
            QSize(std::max(320, int(logicalSize.width() * dpr)), std::max(200, int(logicalSize.height() * dpr)));
        // The camera and renderer MUST use the native viewport dimensions.
        // Previously only renderSize changed: w/h still held the small Qt
        // canvas dimensions, stretching the Vulkan image on F11 fullscreen.
        w = renderSize.width();
        h = renderSize.height();
    }
    const quintptr nativeHandle = directPresent ? quintptr(outputView->winId()) : 0;
    const qreal nativeDpr = directPresent && outputView ? outputView->devicePixelRatioF() : 1.0;
    const QString nativeWindowDetails =
        directPresent ? describeNativeWindow(nativeHandle, QSize(w, h), nativeDpr) : QString{};
    const bool wantGpu = backend == 1 || backend == 4 || backend == 5 || backend == 6;
    const bool raster = backend == 4 || backend == 5 || backend == 6;
    const bool vulkan = backend == 5;
    const bool lumaTrace = backend == 6;
    void *sharedVulkanInstance = nullptr;
    uintptr_t sharedVulkanSurface = 0;
#if defined(_WIN32) && M3D_V17_VULKAN_ENABLED
    // Leave shared Vulkan handles null. VulkanRaster creates its own instance
    // and Win32 surface from nativeHandle, as in the passing hardware diagnostic.
#endif
    const bool globalIllumination = backend == 2 || backend == 3;
    const bool edgeAA = edgeAa_->isChecked();
    const unsigned sampleCount = globalIllumination ? unsigned(giSamples_->currentData().toInt())
                                                    : (wantGpu ? 1u : unsigned(directSamples_->currentData().toInt()));
    const unsigned maxBounces = backend == 2 ? 2u : 4u;
    const bool progressive = globalIllumination && progressiveGi_->isChecked();
    const bool denoise = globalIllumination && denoiseGi_->isChecked();
    const bool denoiseDirect = !globalIllumination && !wantGpu && denoiseDirect_->isChecked();
    const QString requestStamp = requestedRendererStamp();
    statusBar()->showMessage(QStringLiteral("Calcul de l'image…"));
    // Explain an abnormal delay rather than displaying "Calcul..." forever.
    // This watchdog does not claim to cancel a driver-blocked D3D call.
    if (raster)
    {
        QTimer::singleShot(30000, this, [this, serial, token, vulkan] {
            if (serial->load() == token && watcher_.isRunning() && !closingRequested_)
                statusBar()->showMessage(
                    QStringLiteral("Rendu %1 actif depuis plus de 30 secondes. GPU ou pilote possiblement saturé ; "
                                   "aucun basculement CPU.")
                        .arg(vulkan ? QStringLiteral("Vulkan") : QStringLiteral("DirectX 11")));
        });
    }
    const auto shadowCache = cpuShadows_;
    watcher_.setFuture(QtConcurrent::run(&renderPool_, [this, serial, token, snapshot, preset, w, h, wantGpu,
                                                        globalIllumination, raster, vulkan, lumaTrace, directPresent,
                                                        nativeHandle, sharedVulkanInstance, sharedVulkanSurface, edgeAA,
                                                        sampleCount, maxBounces, progressive, denoise, outputView,
                                                        requestedBackend, nativeWindowDetails, denoiseDirect,
                                                        shadowCache, renderHour, shadowHour, requestStamp] {
        Result result;
        result.requestStamp = requestStamp;
        result.cameraPose = preset;
        result.generation = token;
        result.backendIndex = requestedBackend;
        result.nativeWindowDetails = nativeWindowDetails;
        result.skyCompositing = raster;
        result.renderThread = reinterpret_cast<quintptr>(QThread::currentThreadId());
#if defined(_WIN32)
        if (!result.nativeWindowDetails.isEmpty())
            result.nativeWindowDetails += QStringLiteral(" renderThreadWin32=%1 renderThreadQt=0x%2")
                                              .arg(GetCurrentThreadId())
                                              .arg(QString::number(qulonglong(result.renderThread), 16));
#endif
        if (serial->load(std::memory_order_relaxed) != token)
            return result;
        updateLighting(renderHour);
        const auto camera = v16::makeCamera(preset, w, h);
        std::vector<uint32_t> pixels;
        Stats stats;
        double shadowBuildMs = 0.0;
        bool ok = false;
        if (wantGpu)
        {
            std::wstring reason;
            if (!raster)
            {
                releaseDirectX11NativeForCurrentThread();
                releaseVulkanDirectForCurrentThread();
                releaseVulkanPreviewForCurrentThread();
            }
            if (raster)
            {
                std::shared_ptr<const ShadowMap> shadow;
                {
                    std::lock_guard<std::mutex> lock(shadowCache->mutex);
                    if (shadowCache->scene.lock() == snapshot && shadowCache->hour == shadowHour && shadowCache->shadow)
                        shadow = shadowCache->shadow;
                    else
                    {
                        Stats shadowStats;
                        const auto shadowStart = std::chrono::steady_clock::now();
                        auto builtShadow = buildShadow(snapshot->renderScene, shadowStats, serial.get(), token);
                        if (serial->load(std::memory_order_relaxed) != token)
                            return result;
                        shadow = std::make_shared<ShadowMap>(std::move(builtShadow));
                        shadowBuildMs =
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shadowStart)
                                .count();
                        // Optional diagnostic of the 1,024 x 1,024 CPU ray
                        // shadow pass. This cost occurs outside VulkanRaster
                        // and cannot appear in its GPU-only CSV timings.
                        const QByteArray shadowProfile = qgetenv("CSE_WARP_VK_SHADOW_PROFILE");
                        if (!shadowProfile.isEmpty())
                        {
                            QFile log(QString::fromLocal8Bit(shadowProfile));
                            if (log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
                            {
                                const QString line =
                                    QStringLiteral("%1,shadow_build_ms=%2,solar_hour=%3,shadow_bin=%4,backend=%5\n")
                                        .arg(QDateTime::currentMSecsSinceEpoch())
                                        .arg(shadowBuildMs, 0, 'f', 3)
                                        .arg(renderHour, 0, 'f', 5)
                                        .arg(shadowHour, 0, 'f', 2)
                                        .arg(vulkan ? QStringLiteral("Vulkan") : QStringLiteral("DX11"));
                                log.write(line.toUtf8());
                            }
                        }
                        shadowCache->scene = snapshot;
                        shadowCache->hour = shadowHour;
                        shadowCache->shadow = shadow;
                    }
                }
                if (lumaTrace)
                {
                    releaseVulkanDirectForCurrentThread();
                    releaseVulkanPreviewForCurrentThread();
                    if (!snapshot->lumaTraceBvhError.isEmpty())
                    {
                        reason = snapshot->lumaTraceBvhError.toStdWString();
                    }
                    else if (snapshot->lumaTraceBvh4.nodes.empty())
                    {
                        reason = L"Le BVH4 LumaTrace n’a pas été construit pour cette scène.";
                    }
                    else if (directPresent)
                    {
                        ok = renderLumaTraceNative(snapshot->renderScene, snapshot->lumaTraceBvh4, *shadow, camera, w,
                                                   h, reinterpret_cast<void *>(nativeHandle), stats, reason);
                        if (!ok)
                        {
                            const std::wstring nativeReason = reason;
                            result.nativeFailure = QString::fromStdWString(nativeReason);
                            releaseDirectX11NativeForCurrentThread();
                            reason.clear();
                            ok = renderLumaTracePreview(snapshot->renderScene, snapshot->lumaTraceBvh4, *shadow, camera,
                                                        w, h, pixels, stats, reason);
                            result.nativeFallback = ok;
                            if (!ok)
                            {
                                const std::wstring previewReason = reason;
                                reason = L"LumaTrace DXGI natif en échec : " + nativeReason +
                                         L" ; aperçu LumaTrace également en échec : " + previewReason;
                            }
                        }
                    }
                    else
                    {
                        releaseDirectX11NativeForCurrentThread();
                        ok = renderLumaTracePreview(snapshot->renderScene, snapshot->lumaTraceBvh4, *shadow, camera, w,
                                                    h, pixels, stats, reason);
                    }
                }
                else if (vulkan)
                {
                    releaseDirectX11NativeForCurrentThread(); // DXGI belongs to a different backend HWND.
                    // Keep one Vulkan device/resource set resident for the selected path.
                    if (directPresent)
                        releaseVulkanPreviewForCurrentThread();
                    else
                        releaseVulkanDirectForCurrentThread();
                    if (directPresent)
                    {
                        ok = renderVulkanDirectPresent(snapshot->renderScene, *shadow, camera, w, h,
                                                       reinterpret_cast<void *>(nativeHandle), stats, reason,
                                                       sharedVulkanInstance, sharedVulkanSurface);
                        if (!ok)
                        {
                            const std::wstring nativeReason = reason;
                            result.nativeFailure = QString::fromStdWString(nativeReason);
                            releaseVulkanDirectForCurrentThread();
                            reason.clear();
                            ok = renderVulkanPreview(snapshot->renderScene, *shadow, camera, w, h, pixels, stats,
                                                     reason);
                            result.nativeFallback = ok;
                            if (!ok)
                            {
                                const std::wstring previewReason = reason;
                                reason = L"Vulkan native presentation failed: " + nativeReason +
                                         L"; Vulkan image preview also failed: " + previewReason;
                            }
                        }
                    }
                    else
                    {
                        ok = renderVulkanPreview(snapshot->renderScene, *shadow, camera, w, h, pixels, stats, reason);
                    }
                }
                else
                {
                    // Vulkan WSI and DXGI must never own swapchains for the
                    // same backend HWND at the same time. Also release the
                    // offscreen renderer when leaving Vulkan.
                    releaseVulkanDirectForCurrentThread();
                    releaseVulkanPreviewForCurrentThread();
                    ok = directPresent ? renderDirectX11Native(snapshot->renderScene, *shadow, camera, w, h,
                                                               reinterpret_cast<void *>(nativeHandle), stats, reason)
                                       : renderDirectX11Preview(snapshot->renderScene, *shadow, camera, w, h, pixels,
                                                                stats, reason);
                    if (!ok && directPresent)
                    {
                        // DXGI can be unavailable on remote desktop / certain drivers:
                        // recover the original full-quality CPU/QImage preview.
                        const std::wstring nativeReason = reason;
                        result.nativeFailure = QString::fromStdWString(nativeReason);
                        releaseDirectX11NativeForCurrentThread();
                        reason.clear();
                        ok =
                            renderDirectX11Preview(snapshot->renderScene, *shadow, camera, w, h, pixels, stats, reason);
                        result.nativeFallback = ok;
                        if (!ok)
                        {
                            const std::wstring previewReason = reason;
                            reason = L"DXGI native presentation failed: " + nativeReason +
                                     L"; DirectX 11 image preview also failed: " + previewReason;
                        }
                    }
                }
                result.directPresented = ok && directPresent && !result.nativeFallback;
                if (ok)
                {
                    result.renderer =
                        lumaTrace
                            ? (result.directPresented ? QStringLiteral("V17 LumaTrace BVH4 — Direct GPU (swapchain)")
                                                      : QStringLiteral("V17 LumaTrace BVH4 — affichage Qt (QImage)"))
                        : vulkan
                            ? (result.directPresented ? QStringLiteral("V17 Vulkan Raster — Direct GPU (swapchain)")
                                                      : QStringLiteral("V17 Vulkan Raster — affichage Qt (QImage)"))
                            : (result.directPresented ? QStringLiteral("V17 D3D11 Raster — Direct GPU (swapchain)")
                                                      : QStringLiteral("V17 D3D11 Raster — affichage Qt (QImage)"));
                    if (!vulkan && !lumaTrace && rasterHybridLightingEnabled())
                        result.renderer += rasterGiCacheLoaded()
                                               ? QStringLiteral(" — GI transport pré-calculée")
                                               : QStringLiteral(" — sondes approximatives (sans table GI)");
                }
            }
            else
            {
                ok = renderCompute11Preview(snapshot->renderScene, camera, w, h, pixels, stats, reason);
                if (ok)
                    result.renderer = QStringLiteral("V16 D3D11 Compute — GI adaptative");
            }
            if (!ok)
            {
                // Never send hundreds of thousands of triangles through
                // CPU ray tracing as an invisible fallback. That used to
                // look like an indefinitely stalled D3D11 render.
                result.error = QStringLiteral("Échec du rendu GPU %1 : %2. "
                                              "Aucun basculement automatique vers le CPU.")
                                   .arg(lumaTrace ? QStringLiteral("LumaTrace BVH4")
                                                  : (vulkan ? QStringLiteral("Vulkan") : QStringLiteral("DirectX11")))
                                   .arg(QString::fromStdWString(reason));
                result.renderer = lumaTrace ? QStringLiteral("V17 LumaTrace BVH4 — erreur")
                                            : (vulkan ? QStringLiteral("V17 Vulkan — erreur")
                                                      : QStringLiteral("V17 GPU D3D11 — erreur"));
                return result;
            }
        }
        if (!wantGpu)
        {
            releaseDirectX11NativeForCurrentThread();
            releaseVulkanDirectForCurrentThread();
            releaseVulkanPreviewForCurrentThread();
        }
        if (!ok)
        {
            releaseDirectX11NativeForCurrentThread();
            pixels.resize(size_t(w) * size_t(h));
            std::shared_ptr<const ShadowMap> shadow;
            bool shadowReused = false;
            {
                std::lock_guard<std::mutex> lock(shadowCache->mutex);
                if (shadowCache->scene.lock() == snapshot && shadowCache->hour == shadowHour && shadowCache->shadow)
                {
                    shadow = shadowCache->shadow;
                    shadowReused = true;
                }
                else
                {
                    Stats shadowStats;
                    auto builtShadow = buildShadow(snapshot->renderScene, shadowStats, serial.get(), token);
                    if (serial->load(std::memory_order_relaxed) != token)
                        return result;
                    shadow = std::make_shared<ShadowMap>(std::move(builtShadow));
                    shadowCache->scene = snapshot;
                    shadowCache->hour = shadowHour;
                    shadowCache->shadow = shadow;
                }
            }
            const unsigned workers = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
            if (globalIllumination)
            {
                std::vector<V> hdrAccum;
                unsigned completed = 0;
                double totalMs = 0;
                while (completed < sampleCount)
                {
                    if (serial->load() != token)
                        return result;
                    const unsigned batch = progressive ? std::min(std::max(1u, completed), sampleCount - completed)
                                                       : sampleCount - completed;
                    const auto pass =
                        renderGiAccumulatePass(snapshot->renderScene, *shadow, camera, w, h, hdrAccum, pixels,
                                               completed, batch, workers, *serial, token, maxBounces);
                    if (serial->load() != token)
                        return result;
                    completed += batch;
                    totalMs += pass.ms;
                    std::vector<uint32_t> display = pixels;
                    // Filtering is separate from HDR accumulation, so the
                    // next batch never feeds on blurred/tonemapped pixels.
                    if (denoise && completed >= 4)
                        display = denoiseBilateral(display, w, h, workers);
                    if (edgeAA)
                        display = antialiasEdges4(display, w, h, 4, false);
                    QImage image(w, h, QImage::Format_ARGB32);
                    for (int y = 0; y < h; ++y)
                        std::memcpy(image.scanLine(y), display.data() + size_t(y) * w, size_t(w) * 4);
                    if (progressive && completed < sampleCount)
                    {
                        QMetaObject::invokeMethod(
                            this,
                            [this, serial, token, outputView, count = completed, target = sampleCount, w, h,
                             image = std::move(image)]() mutable {
                                if (serial->load() != token)
                                    return;
                                if (outputView == canvas_)
                                    lastImage_ = image;
                                outputView->setImage(std::move(image));
                                statusBar()->showMessage(QStringLiteral("GI progressive — %1 / %2 échantillons (%3×%4)")
                                                             .arg(count)
                                                             .arg(target)
                                                             .arg(w)
                                                             .arg(h));
                            },
                            Qt::QueuedConnection);
                    }
                    else
                    {
                        result.image = std::move(image);
                    }
                }
                result.renderer = QStringLiteral("CPU GI — %1 rebonds, %2 spp, HDR%3 — %4×%5")
                                      .arg(maxBounces)
                                      .arg(sampleCount)
                                      .arg(denoise ? QStringLiteral(", débruité") : QString())
                                      .arg(w)
                                      .arg(h);
                result.milliseconds = totalMs;
                return result;
            }
            // Reuse the active render generation so CPU fallback/direct
            // rays stop promptly when the scene or camera is changed.
            stats = m3d::render(snapshot->renderScene, *shadow, camera, w, h, pixels, workers, false, *serial, token,
                                sampleCount, false, false, false, 3);
            if (serial->load() != token)
                return result;
            if (denoiseDirect)
                pixels = denoiseBilateral(pixels, w, h, workers);
            result.renderer = shadowReused
                                  ? QStringLiteral("CPU Ray Tracing — %1 spp, ombres en cache").arg(sampleCount)
                                  : QStringLiteral("CPU Ray Tracing — %1 spp, ombres calculées").arg(sampleCount);
        }
        else
            result.gpu = true;
        result.milliseconds = stats.ms + shadowBuildMs;
        result.renderer += QStringLiteral(" — %1×%2").arg(w).arg(h);
        if (result.nativeFallback)
        {
            result.renderer += vulkan ? QStringLiteral(" — secours image Vulkan via QImage (WSI indisponible)")
                                      : QStringLiteral(" — secours QImage (DXGI indisponible)");
            if (!result.nativeFailure.isEmpty())
                result.renderer += QStringLiteral(" — échec présentation native : %1").arg(result.nativeFailure);
        }
        if (result.directPresented)
        {
            result.renderer += QStringLiteral(" — écran GPU direct (swapchain)");
            return result; // No 4-byte-per-pixel CPU/QImage allocation.
        }
        if (edgeAA && !raster && pixels.size() == size_t(w) * size_t(h))
            pixels = antialiasEdges4(pixels, w, h, 4, false);
        if (pixels.size() != size_t(w) * h)
        {
            result.error += QStringLiteral("Image incomplète");
            return result;
        }
        QImage image(w, h, QImage::Format_ARGB32);
        for (int y = 0; y < h; y++)
            std::memcpy(image.scanLine(y), pixels.data() + size_t(y) * w, size_t(w) * 4);
        result.image = std::move(image);
        return result;
    }));
}
