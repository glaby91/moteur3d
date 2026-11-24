#include "moteur3d_v17_window.h"

using namespace m3d;

void V17Window::updateExplorationSpeedIndicator()
{
    if (!explorationSpeedLabel_)
        return;
    if (!exploring_)
    {
        explorationSpeedLabel_->hide();
        return;
    }
    const bool fast = leftShiftHeld_ || rightShiftHeld_;
    const bool walking = walkingModeBox_ && walkingModeBox_->isChecked();
    const QString movement = !walking ? QStringLiteral("Vol libre")
                                      : (leftControlHeld_ ? QStringLiteral("Accroupi") : QStringLiteral("Marche"));
    const QString speed = fast ? QStringLiteral("Turbo ×3 (Maj)") : QStringLiteral("Vitesse ×1");
    explorationSpeedLabel_->setText(QStringLiteral("%1 · %2").arg(movement, speed));
    explorationSpeedLabel_->show();
}

void V17Window::resetScopeForCamera()
{
    rightMouseHeld_ = false;
    scopeBlend_ = 0.f;
    scopeBaseFov_ = freeCamera_.fov;
    if (canvas_)
        canvas_->setScopeOverlay(0.f);
}

void V17Window::activateFreeCamera()
{
    if (!runtime_ || runtime_->cameras.isEmpty())
        return;
    const int idx = selectedSceneCameraIndex();
    freeCamera_ = runtime_->cameras[idx];
    resetScopeForCamera();
    const V dir = norm(freeCamera_.target - freeCamera_.eye);
    yaw_ = std::atan2(dir.x, -dir.z);
    pitch_ = std::asin(clampf(dir.y, -.99f, .99f));
    freeCameraActive_ = true;
    walkingCollisionSyncPending_ = false;
    if (walkingModeBox_ && walkingModeBox_->isChecked())
    {
        bool grounded = walk::spawn(runtime_->renderScene, freeCamera_.eye, walkingState_);
        // Pavillon 01's overview cameras sit outside the 30 x 26 m
        // terrain. If no walkable surface lies below the selected view,
        // start on the front terrace beside the basin instead of leaving
        // the player suspended in the air.
        if (!grounded && scenePath_.contains(QStringLiteral("pavillon01"), Qt::CaseInsensitive))
        {
            grounded = walk::spawn(runtime_->renderScene, V{-5.f, 1.86f, 9.1f}, walkingState_);
            if (grounded)
            {
                statusBar()->showMessage(
                    QStringLiteral("Exploration au sol : départ sur la terrasse devant le bassin."), 6000);
            }
        }
        if (!grounded)
        {
            statusBar()->showMessage(QStringLiteral("Aucun sol détecté sous cette caméra. Choisissez une vue "
                                                    "extérieure ou décochez « Marcher au sol »."),
                                     9000);
        }
        freeCamera_.eye = walk::eye(walkingState_);
        updateLook();
    }
    updateCameraHud();
}

void V17Window::updateLook()
{
    const V forward{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), -std::cos(yaw_) * std::cos(pitch_)};
    freeCamera_.target = freeCamera_.eye + forward;
    updateCameraHud();
}

void V17Window::captureMouseLook()
{
    if (!exploring_ || mouseCaptured_ || !canvas_ || (!isActiveWindow() && !canvas_->isActiveWindow()))
        return;
    mouseCaptured_ = true;
    canvas_->setCursor(Qt::BlankCursor);
    focusRenderViewport();
    QCursor::setPos(canvas_->mapToGlobal(canvas_->rect().center()));
}

void V17Window::releaseMouseLook()
{
    if (!mouseCaptured_)
        return;
    mouseCaptured_ = false;
    if (canvas_)
        canvas_->unsetCursor();
    for (auto *view : backendWindows_)
        if (view)
            view->unsetCursor();
}

void V17Window::setMotionResolution(bool active)
{
    const bool next = peripheralExperimentEnabled_ && active;
    if (peripheralDuringMotion_ == next)
        return;
    peripheralDuringMotion_ = next;
    setRasterPeripheralMultiresOverride(next ? 1 : 0);
}

void V17Window::stopExploration()
{
    releaseMouseLook();
    setMotionResolution(false);
    exploring_ = false;
    if (canvas_)
        canvas_->setFpsOverlay(0.0, false);
    if (nativeFpsLabel_)
        nativeFpsLabel_->setText(QStringLiteral("FPS —"));
    if (freeCameraActive_)
        freeCamera_.fov = scopeBaseFov_;
    resetScopeForCamera();
    if (explorationQualityOverride_)
    {
        std::wstring error;
        if (setRasterQualitySettings(explorationOriginalQuality_, error))
            displayRasterSettings(explorationOriginalQuality_);
        explorationQualityOverride_ = false;
    }
    frameTimer_.stop();
    keysDown_.clear();
    leftShiftHeld_ = false;
    rightShiftHeld_ = false;
    leftControlHeld_ = false;
    rightControlHeld_ = false;
    jumpRequested_ = false;
    jumpHorizontalVelocity_ = {};
    walkingState_.crouched = false;
    if (freeCameraActive_ && walkingState_.hasSafeGround)
    {
        freeCamera_.eye = walk::eye(walkingState_);
        updateLook();
    }
    poolLadderMotion_ = {};
    leftMouseHeld_ = false;
    rightMouseHeld_ = false;
    lastMouseLookMotion_.invalidate();
    updateExplorationSpeedIndicator();
    if (!closingRequested_)
        restoreExplorationWindows();
    if (explorationButton_)
        explorationButton_->setText(QStringLiteral("Démarrer exploration (V17)"));
    if (explorationHelp_)
        explorationHelp_->setText(QStringLiteral(
            "ZQSD / flèches : marcher · Espace : sauter · Ctrl gauche : s'accroupir · Maj : turbo\n"
            "Souris : regarder librement · clic droit : lunette ×3 · F11 : plein écran · Échap : arrêter · E : "
            "échelle."));
    // A delayed multires GPU frame could still be on screen when Esc
    // stops exploration. Queue a final guaranteed full-resolution image.
    if (!closingRequested_ && !sceneLoading_ && runtime_)
        QTimer::singleShot(0, this, [this] {
            if (!exploring_ && !closingRequested_ && runtime_)
                requestRender();
        });
}

void V17Window::toggleExploration()
{
    if (exploring_)
    {
        stopExploration();
        return;
    }
    if (!runtime_ || runtime_->cameras.isEmpty())
        return;
    if (!supportsExplorationBackend(backendBox_->currentIndex()))
        backendBox_->setCurrentIndex(4);
    if (!freeCameraActive_)
        activateFreeCamera();
    // An Intel HD 620 cannot reasonably sustain 8x MSAA during continuous
    // navigation of the large V8 scene. Temporarily lower AA to 2x ONLY
    // while moving. Restore the user's precise settings on exit.
    const auto adapter = gpuAdapterLabel();
    if (adapter.contains(QStringLiteral("HD Graphics 620"), Qt::CaseInsensitive) ||
        adapter.contains(QStringLiteral("UHD Graphics 620"), Qt::CaseInsensitive))
    {
        const auto q = rasterQualitySettings();
        if (q.msaaSamples > 2)
        {
            auto interactive = q;
            interactive.msaaSamples = 2;
            interactive.anisotropy = std::min<unsigned>(q.anisotropy, 8);
            std::wstring error;
            if (setRasterQualitySettings(interactive, error))
            {
                explorationOriginalQuality_ = q;
                explorationQualityOverride_ = true;
                displayRasterSettings(interactive);
                statusBar()->showMessage(
                    QStringLiteral("HD 620 : exploration MSAA 2x temporaire ; qualité initiale restaurée à l'arrêt"),
                    7000);
            }
        }
    }
    exploring_ = true;
    setMotionResolution(false);
    resetPacing();
    canvas_->setFpsOverlay(0.0, true);
    if (walkingModeBox_ && !walkingModeBox_->isChecked())
    {
        statusBar()->showMessage(
            QStringLiteral(
                "Vol libre : collisions désactivées. Cochez « Marcher au sol » pour bloquer murs et obstacles."),
            10000);
    }
    poolLadderMotion_ = {};
    motionClock_.start();
    frameClock_.invalidate();
    lastMouseLookMotion_.invalidate();
    if (explorationButton_)
        explorationButton_->setText(QStringLiteral("Arrêter exploration"));
    updateCameraHud();
    frameTimer_.start();
    updateExplorationSpeedIndicator();
    arrangeExplorationWindows();
    captureMouseLook();
}

void V17Window::frameTick()
{
    if (!exploring_ || !runtime_ || !freeCameraActive_)
        return;
    const bool jumpPressed = std::exchange(jumpRequested_, false);
    const float dt = std::min(0.08f, float(motionClock_.restart()) / 1000.f);
    const float nextScope = cse::scope::advance(scopeBlend_, rightMouseHeld_, dt);
    if (nextScope != scopeBlend_)
    {
        scopeBlend_ = nextScope;
        freeCamera_.fov = cse::scope::effectiveFov(scopeBaseFov_, scopeBlend_);
        canvas_->setScopeOverlay(scopeBlend_);
        updateCameraHud();
    }
    V forward{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), -std::cos(yaw_) * std::cos(pitch_)};
    V horizontal = norm(V{forward.x, 0, forward.z});
    V right = norm(cross(horizontal, V{0, 1, 0}));
    V move{};
    if (keysDown_.contains(Qt::Key_Z) || keysDown_.contains(Qt::Key_W) || keysDown_.contains(Qt::Key_Up))
        move = move + horizontal;
    if (keysDown_.contains(Qt::Key_S) || keysDown_.contains(Qt::Key_Down))
        move = move - horizontal;
    if (keysDown_.contains(Qt::Key_D) || keysDown_.contains(Qt::Key_Right))
        move = move + right;
    if (keysDown_.contains(Qt::Key_Q) || keysDown_.contains(Qt::Key_A) || keysDown_.contains(Qt::Key_Left))
        move = move - right;
    const bool mouseRecentlyMoved = lastMouseLookMotion_.isValid() && lastMouseLookMotion_.elapsed() < 180;
    const bool moving = length(move) > .01f || poolLadderMotion_.active();
    // The scoped sight remains fully sharp. On stopping, raster returns
    // to full resolution within 180ms, without any temporal reprojection.
    setMotionResolution((moving || mouseRecentlyMoved) && !rightMouseHeld_ && scopeBlend_ < .05f);
    if (walkingModeBox_ && walkingModeBox_->isChecked())
    {
        // A recalled anomaly camera can be above a floor or behind a wall.
        // Keep its precise eye position until the user actually navigates.
        // Never reuse stale feet from before camera teleportation.
        if (walkingCollisionSyncPending_ && length(move) > .01f)
        {
            walk::spawn(runtime_->renderScene, freeCamera_.eye, walkingState_);
            freeCamera_.eye = walk::eye(walkingState_);
            walkingCollisionSyncPending_ = false;
            updateLook();
        }
        if (!poolLadderMotion_.active())
        {
            const bool wantsCrouch = leftControlHeld_;
            const bool canChangeCrouch =
                wantsCrouch || !walkingState_.crouched || walk::canStand(runtime_->renderScene, walkingState_);
            if (canChangeCrouch && walkingState_.crouched != wantsCrouch)
            {
                walkingState_.crouched = wantsCrouch;
                freeCamera_.eye = walk::eye(walkingState_);
                updateLook();
            }
        }
        if (poolLadderMotion_.active())
        {
            const bool climbing = poolLadderMotion_.up;
            const auto p = poolLadderMotion_.advance(dt);
            walkingState_.feet = {p.x, p.y, p.z};
            freeCamera_.eye = walk::eye(walkingState_);
            updateLook();
            if (!poolLadderMotion_.active() && climbing)
            {
                // Re-establish dry ground contact after climbing over coping.
                const V eye = walk::eye(walkingState_);
                walk::spawn(runtime_->renderScene, eye, walkingState_);
                freeCamera_.eye = walk::eye(walkingState_);
                updateLook();
            }
            if (!watcher_.isRunning())
            {
                frameClock_.start();
                requestRender();
            }
            return;
        }
        const V currentFeet = walkingState_.feet;
        if (scenePath_.contains(QStringLiteral("benchmark-civic"), Qt::CaseInsensitive) &&
            cse::pool_ladder::inWater({currentFeet.x, currentFeet.y, currentFeet.z}))
        {
            const float speed = cse::exploration::speedMetersPerSecond(false) * .55f;
            const V horizontalMotion = length(move) > .01f ? norm(move) * (dt * speed) : V{};
            const auto p = cse::pool_ladder::walkInWater({currentFeet.x, currentFeet.y, currentFeet.z},
                                                         horizontalMotion.x, horizontalMotion.z);
            walkingState_.feet = {p.x, p.y, p.z};
            freeCamera_.eye = walk::eye(walkingState_);
            updateLook();
            if (!watcher_.isRunning())
            {
                frameClock_.start();
                requestRender();
            }
            return;
        }
        // Walking is always horizontal input. Height follows geometry:
        // floor, stairs, landings, slopes and gravity on unsupported areas.
        // Shift is an exact 3× multiplier; the deterministic walker still
        // subdivides large motion into <= 8.5cm steps for collision safety.
        const float speed = cse::exploration::speedMetersPerSecond(leftShiftHeld_ || rightShiftHeld_);
        if (jumpPressed && walk::jump(runtime_->renderScene, walkingState_))
            jumpHorizontalVelocity_ = length(move) > .01f ? norm(move) * speed : V{};
        V displacement{};
        if (walkingState_.grounded)
        {
            jumpHorizontalVelocity_ = {};
            if (length(move) > .01f)
                displacement = norm(move) * (dt * speed);
        }
        else
        {
            const V desiredVelocity = length(move) > .01f ? norm(move) * speed : V{};
            const float airControl = length(move) > .01f ? 4.f : 1.5f;
            jumpHorizontalVelocity_ = jumpHorizontalVelocity_ + (desiredVelocity - jumpHorizontalVelocity_) *
                                                                    std::clamp(dt * airControl, 0.f, 1.f);
            displacement = jumpHorizontalVelocity_ * dt;
        }
        const V oldEye = freeCamera_.eye;
        const walk::State beforeStep = walkingState_;
        walk::advance(runtime_->renderScene, walkingState_, displacement, dt);
        if (walkingState_.grounded)
            jumpHorizontalVelocity_ = {};
        // The water volume is reached ONLY through the actual ladder
        // interaction. Don't let numerical walker tunnelling bypass the
        // tiled coping (a V17.437 user-found regression).
        if (scenePath_.contains(QStringLiteral("benchmark-civic"), Qt::CaseInsensitive) && beforeStep.feet.y < 1.0f &&
            walkingState_.feet.y < 1.0f)
        {
            const auto dry = [](const V &feet) {
                return !(std::abs(feet.x) < 24.92f && feet.z > -44.45f && feet.z < -19.55f);
            };
            if (dry(beforeStep.feet) && !dry(walkingState_.feet))
                walkingState_ = beforeStep;
        }
        freeCamera_.eye = walk::eye(walkingState_);
        if (length(freeCamera_.eye - oldEye) > .00001f)
            updateLook();
    }
    else
    {
        if (keysDown_.contains(Qt::Key_Space))
            move.y += 1.f;
        if (keysDown_.contains(Qt::Key_Control))
            move.y -= 1.f;
        if (length(move) > .01f)
        {
            freeCamera_.eye =
                freeCamera_.eye +
                norm(move) * (dt * cse::exploration::speedMetersPerSecond(leftShiftHeld_ || rightShiftHeld_));
            updateLook();
        }
    }
    // Avoid queued stale frames. CPU draw/readback is still a V17-1
    // limitation and cannot yet guarantee the 25 ms target.
    if (!watcher_.isRunning())
    {
        frameClock_.start();
        requestRender();
    }
}

bool V17Window::supportsExplorationBackend(int index)
{
    // HLSL compute is available for slower full-quality camera exploration
    // through the existing QImage output. V17 raster remains the fast path.
    return index == 1 || index == 4 || index == 5 || index == 6;
}
