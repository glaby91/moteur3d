#include "moteur3d_v17_window.h"

using namespace m3d;

void V17Window::updateMemoryBanner()
{
    if (!systemMemoryLabel_ || !systemMemoryBar_)
        return;
    const int percent = systemMemoryLoadPercent();
    if (percent < 0)
    {
        systemMemoryLabel_->setText(QStringLiteral("RAM n/d"));
        systemMemoryBar_->setValue(0);
        return;
    }
    systemMemoryLabel_->setText(QStringLiteral("RAM %1 %").arg(percent));
    systemMemoryBar_->setValue(percent);
}

void V17Window::displayRasterSettings(const RasterQualitySettings &q)
{
    const QSignalBlocker msaaBlocker(rasterMsaaBox_);
    const QSignalBlocker anisoBlocker(rasterAnisoBox_);
    const QSignalBlocker lodBlocker(rasterLodBox_);
    const QSignalBlocker cullBlocker(rasterCullBox_);
    const QSignalBlocker dynamicBlocker(rasterDynamicLodBox_);
    rasterMsaaBox_->setCurrentIndex(rasterMsaaBox_->findData(int(q.msaaSamples)));
    rasterAnisoBox_->setCurrentIndex(rasterAnisoBox_->findData(int(q.anisotropy)));
    rasterLodBox_->setCurrentIndex(rasterLodBox_->findData(int(q.lodQualityPercent)));
    rasterCullBox_->setChecked(q.frustumCulling);
    rasterDynamicLodBox_->setChecked(q.dynamicLod);
    rasterLodBox_->setEnabled(q.dynamicLod);
}

void V17Window::applyRasterSettings(bool userCustom)
{
    if (!rasterPresetBox_ || !rasterMsaaBox_)
        return;
    if (userCustom)
    {
        const QSignalBlocker blocker(rasterPresetBox_);
        rasterPresetBox_->setCurrentIndex(4);
    }
    RasterQualitySettings q;
    q.msaaSamples = rasterMsaaBox_->currentData().toUInt();
    q.anisotropy = rasterAnisoBox_->currentData().toUInt();
    q.lodQualityPercent = rasterLodBox_->currentData().toUInt();
    q.frustumCulling = rasterCullBox_->isChecked();
    q.dynamicLod = rasterDynamicLodBox_->isChecked();
    rasterLodBox_->setEnabled(q.dynamicLod);
    std::wstring error;
    if (!setRasterQualitySettings(q, error))
    {
        statusBar()->showMessage(QString::fromStdWString(error), 7000);
        return;
    }
    QSettings settings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    settings.setValue(QStringLiteral("Raster/Preset"), rasterPresetBox_->currentIndex());
    settings.setValue(QStringLiteral("Raster/MSAA"), q.msaaSamples);
    settings.setValue(QStringLiteral("Raster/Anisotropy"), q.anisotropy);
    settings.setValue(QStringLiteral("Raster/LODPercent"), q.lodQualityPercent);
    settings.setValue(QStringLiteral("Raster/Culling"), q.frustumCulling);
    settings.setValue(QStringLiteral("Raster/DynamicLOD"), q.dynamicLod);
    if (runtime_ && backendBox_ &&
        (backendBox_->currentIndex() == 4 || backendBox_->currentIndex() == 5 || backendBox_->currentIndex() == 6))
        requestRender();
}

void V17Window::stopRadio()
{
    radioAudio_.stop();
    radioPlaying_ = false;
    if (radioToggleButton_)
        radioToggleButton_->setText(QStringLiteral("Activer la radio"));
    if (radioStatusLabel_)
        radioStatusLabel_->setText(QStringLiteral("Radio arrêtée"));
}

bool V17Window::loadSpeakerLayout()
{
    const QString path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("audio/speaker_layout.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const QJsonArray sources = doc.object().value(QStringLiteral("loudspeakers")).toArray();
    if (sources.size() != 4)
        return false;
    for (int i = 0; i < 4; ++i)
    {
        if (!sources[i].isObject())
            return false;
        const auto obj = sources[i].toObject();
        const auto pos = obj.value(QStringLiteral("position")).toArray();
        const auto front = obj.value(QStringLiteral("front")).toArray();
        if (pos.size() != 3 || front.size() != 3)
            return false;
        for (int axis = 0; axis < 3; ++axis)
        {
            const double p = pos[axis].toDouble(std::numeric_limits<double>::quiet_NaN());
            const double f = front[axis].toDouble(std::numeric_limits<double>::quiet_NaN());
            if (!std::isfinite(p) || !std::isfinite(f))
                return false;
            radioSpeakers_[i].position[axis] = float(p);
            radioSpeakers_[i].front[axis] = float(f);
        }
    }
    return true;
}

void V17Window::playRadio()
{
    const QString path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("audio/pripyat_radio.wav"));
    if (!QFileInfo::exists(path))
    {
        if (radioStatusLabel_)
            radioStatusLabel_->setText(QStringLiteral("Fichier WAV absent : audio/pripyat_radio.wav"));
        return;
    }
    if (!loadSpeakerLayout())
    {
        if (radioStatusLabel_)
            radioStatusLabel_->setText(QStringLiteral("Implantation des 4 HP introuvable ou invalide"));
        return;
    }
    radioAudio_.setVolume(radioVolumeSlider_ ? float(radioVolumeSlider_->value()) / 500.f : .20f);
    radioPlaying_ = radioAudio_.start(path.toStdWString(), radioSpeakers_);
    if (radioToggleButton_)
        radioToggleButton_->setText(radioPlaying_ ? QStringLiteral("Arrêter la radio")
                                                  : QStringLiteral("Activer la radio"));
    if (radioStatusLabel_)
        radioStatusLabel_->setText(radioPlaying_ ? QStringLiteral("4 HP spatialisés · XAudio2 / X3DAudio · WAV de test")
                                                 : QStringLiteral("Audio 3D indisponible (pilote ou WAV incorrect)"));
}

void V17Window::refreshRadioForScene(const QString &path)
{
    stopRadio();
    const QString lower = path.toLower();
    const bool isSquare = lower.contains(QStringLiteral("benchmark-civic")) ||
                          lower.contains(QStringLiteral("pripyat")) || lower.contains(QStringLiteral("energetik"));
    if (isSquare && radioAutoplayEnabled_)
        playRadio();
}

void V17Window::selectAnomaly(int index)
{
    if (index < 0 || index >= int(cse::anomalies::records.size()))
        return;
    if (!runtime_ || runtime_->cameras.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("Anomalies"),
                                 QStringLiteral("Chargez la scene du palais avant de consulter le registre."));
        return;
    }
    if (!scenePath_.contains(QStringLiteral("benchmark-civic"), Qt::CaseInsensitive))
    {
        QMessageBox::warning(this, QStringLiteral("Anomalies"),
                             QStringLiteral("Les positions sont relevees dans le palais benchmark-civic. Ouvrez "
                                            "cette scene avant de les utiliser."));
        return;
    }
    const auto &a = cse::anomalies::records[size_t(index)];
    if (!hasBeforeAnomalyCamera_)
    {
        beforeAnomalyCamera_ = activeCameraPose();
        hasBeforeAnomalyCamera_ = true;
    }
    currentAnomaly_ = index;
    if (a.cameraKnown)
    {
        // Camera recalls preserve a precise reference view. Do not
        // silently disable walking/collisions or overwrite QSettings.
        // Only resynchronize the walking state on the next movement.
        keysDown_.clear();
        poolLadderMotion_ = {};
        const float yr = a.yaw * PI / 180.f;
        const float pr = a.pitch * PI / 180.f;
        freeCamera_.eye = {a.x, a.y, a.z};
        walkingCollisionSyncPending_ = true;
        freeCamera_.fov = a.fov;
        resetScopeForCamera();
        freeCamera_.id = QString::fromLatin1(a.id);
        freeCamera_.name = QString::fromUtf8(a.title);
        yaw_ = yr;
        pitch_ = pr;
        freeCameraActive_ = true;
        updateLook();
        requestRender();
        canvas_->setFocus();
    }
    else
    {
        // ANO-014 has no measured coordinates; keep the current pose.
        // The user can select exterior camera 01 or walk to any paving joint.
        statusBar()->showMessage(QStringLiteral("ANO-014 : pas de coordonnees mesurees. Selectionnez une vue "
                                                "exterieure puis marchez sur les joints."),
                                 13000);
    }
    updateAnomalyPanel();
}

void V17Window::updateAnomalyPanel()
{
    if (!anomalyPanelLabel_)
        return;
    if (currentAnomaly_ < 0)
    {
        anomalyPanelLabel_->setText(QStringLiteral("Choisissez ANO-005 a ANO-014 depuis le menu Anomalies."));
        return;
    }
    const auto &a = cse::anomalies::records[size_t(currentAnomaly_)];
    anomalyPanelLabel_->setText(QStringLiteral("%1 - %2\n%3\n%4")
                                    .arg(QString::fromLatin1(a.id), QString::fromUtf8(a.title),
                                         QString::fromUtf8(a.check),
                                         a.correctedCandidate ? QStringLiteral("Modification codee : a valider")
                                                              : QStringLiteral("Anomalie encore ouverte")));
    anomalyPanelLabel_->setWordWrap(true);
    statusBar()->showMessage(QStringLiteral("%1 : %2").arg(QString::fromLatin1(a.id), QString::fromUtf8(a.check)),
                             12000);
}

void V17Window::saveAnomalyVerdict(const QString &verdict)
{
    if (currentAnomaly_ < 0)
        return;
    const auto &a = cse::anomalies::records[size_t(currentAnomaly_)];
    // Separate file per attempt: do not overwrite the evidence from another
    // version or previous review. No external storage or telemetry.
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/CSE-WARP-Anomalies");
    if (!QDir().mkpath(root))
    {
        QMessageBox::warning(this, QStringLiteral("Anomalies"),
                             QStringLiteral("Impossible de creer le dossier : %1").arg(root));
        return;
    }
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString path = root + QStringLiteral("/") + QString::fromLatin1(a.id) +
                         QStringLiteral("-V%1-").arg(CSE_WARP_PRODUCT_VERSION) + timestamp + QStringLiteral(".json");
    const QJsonObject record{{QStringLiteral("format"), QStringLiteral("cse-warp-anomaly-review-v1")},
                             {QStringLiteral("version"), QStringLiteral(CSE_WARP_PRODUCT_VERSION)},
                             {QStringLiteral("anomalyId"), QString::fromLatin1(a.id)},
                             {QStringLiteral("title"), QString::fromUtf8(a.title)},
                             {QStringLiteral("expected"), QString::fromUtf8(a.check)},
                             {QStringLiteral("verdict"), verdict},
                             {QStringLiteral("scene"), QFileInfo(scenePath_).fileName()},
                             {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                             {QStringLiteral("referencePoseKnown"), a.cameraKnown},
                             {QStringLiteral("camera"), cse::camera_diagnostic::toJson(activeCameraPose())}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(record).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit())
    {
        QMessageBox::warning(this, QStringLiteral("Anomalies"),
                             QStringLiteral("Echec d'enregistrement : %1").arg(path));
        return;
    }
    statusBar()->showMessage(
        QStringLiteral("%1 : verdict %2 sauvegarde : %3").arg(QString::fromLatin1(a.id), verdict, path), 14000);
}

void V17Window::returnBeforeAnomaly()
{
    if (!hasBeforeAnomalyCamera_)
        return;
    const auto pose = beforeAnomalyCamera_;
    // Return to the old camera without changing the selected navigation
    // mode or disabling obstacle collisions in saved preferences.
    freeCamera_ = pose;
    resetScopeForCamera();
    walkingCollisionSyncPending_ = true;
    const V dir = norm(pose.target - pose.eye);
    yaw_ = std::atan2(dir.x, -dir.z);
    pitch_ = std::asin(clampf(dir.y, -.99f, .99f));
    freeCameraActive_ = true;
    hasBeforeAnomalyCamera_ = false;
    currentAnomaly_ = -1;
    updateAnomalyPanel();
    updateCameraHud();
    requestRender();
    canvas_->setFocus();
}

void V17Window::createUi()
{
    auto *file = menuBar()->addMenu(QStringLiteral("Fichier"));
    auto *captureAction = file->addAction(QStringLiteral("Capturer une anomalie (PNG + caméra JSON)…"));
    captureAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    connect(captureAction, &QAction::triggered, this, [this] { captureAnomaly(); });
    auto *restoreAction = file->addAction(QStringLiteral("Restaurer une caméra d'anomalie…"));
    restoreAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(restoreAction, &QAction::triggered, this, [this] { restoreAnomalyCamera(); });
    auto *copyPoseAction = file->addAction(QStringLiteral("Copier la position caméra (JSON)"));
    copyPoseAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    connect(copyPoseAction, &QAction::triggered, this, [this] { copyLiveCamera(); });
    file->addSeparator();
    auto *open = file->addAction(QStringLiteral("Ouvrir une scène V16 (scene.json)…"));
    connect(open, &QAction::triggered, this, [this] {
        const QString chosen =
            QFileDialog::getOpenFileName(this, QStringLiteral("Scène V16"), QFileInfo(scenePath_).absolutePath(),
                                         QStringLiteral("Manifeste V16 (scene.json);;JSON (*.json)"));
        if (!chosen.isEmpty())
            openScene(chosen);
    });
    auto *benchmarkScenes = file->addMenu(QStringLiteral("Scènes benchmark architecturales"));
    for (const auto &profile : QVector<QPair<QString, QString>>{
             {QStringLiteral("Léger — 20 000 triangles"), QStringLiteral("scene.json")},
             {QStringLiteral("Lourd — 150 000 triangles"), QStringLiteral("scene-lourd.json")},
             {QStringLiteral("Extrême — 500 000+ triangles"), QStringLiteral("scene-extreme.json")}})
    {
        auto *action = benchmarkScenes->addAction(profile.first);
        const QString sceneFile = profile.second;
        connect(action, &QAction::triggered, this, [this, sceneFile] {
            const QString target =
                QCoreApplication::applicationDirPath() + QStringLiteral("/benchmark-civic/") + sceneFile;
            if (!QFileInfo::exists(target))
            {
                QMessageBox::warning(this, QStringLiteral("Scène benchmark manquante"),
                                     QStringLiteral("La scène n'est pas installée : %1").arg(target));
                return;
            }
            if (sceneFile == QStringLiteral("scene-extreme.json") &&
                QMessageBox::question(this, QStringLiteral("Profil extrême"),
                                      QStringLiteral("Ce niveau contient au moins 500 000 triangles. "
                                                     "Il utilise beaucoup de RAM et peut être lent sur Intel HD 620. "
                                                     "Charger la scène sans lancer automatiquement son rendu ?")) !=
                    QMessageBox::Yes)
                return;
            openScene(target);
        });
    }
    benchmarkScenes->menuAction()->setVisible(false);
    auto *saveMaterialsAction = file->addAction(QStringLiteral("Enregistrer les matériaux PBR…"));
    connect(saveMaterialsAction, &QAction::triggered, this, [this] { saveMaterials(); });
    auto *save = file->addAction(QStringLiteral("Exporter l'image PNG…"));
    connect(save, &QAction::triggered, this, [this] {
        if (lastImage_.isNull())
        {
            QMessageBox::information(this, "PNG", "Aucune image calculée.");
            return;
        }
        const QString path = QFileDialog::getSaveFileName(this, "Exporter PNG", "pavillon01-v16.png", "PNG (*.png)");
        if (!path.isEmpty() && !lastImage_.save(path, "PNG"))
            QMessageBox::warning(this, "PNG", "Échec de l'enregistrement.");
    });
    auto *quit = file->addAction(QStringLiteral("Quitter"));
    connect(quit, &QAction::triggered, this, &QWidget::close);

    // One-click camera presets are independent from the detailed settings panel.
    auto *camerasMenu = menuBar()->addMenu(QStringLiteral("Caméras"));
    auto *camerasToolbar = addToolBar(QStringLiteral("Caméras · 1 à 6"));
    camerasToolbar->setMovable(false);
    camerasToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    for (int i = 0; i < 6; ++i)
    {
        auto *action = new QAction(QString::number(i + 1), this);
        action->setShortcut(QKeySequence(QString::number(i + 1)));
        action->setShortcutContext(Qt::ApplicationShortcut);
        action->setEnabled(false);
        const int index = i;
        connect(action, &QAction::triggered, this, [this, index] {
            if (!runtime_ || index >= cameraBox_->count())
                return;
            const bool changed = cameraBox_->currentIndex() != index;
            cameraBox_->setCurrentIndex(index);
            azimuth_->setValue(0);
            if (!changed)
                requestRender();
        });
        quickCameraActions_.append(action);
        camerasToolbar->addAction(action);
        camerasMenu->addAction(action);
    }
    camerasToolbar->addSeparator();
    sceneRevisionLabel_ = new QLabel(QStringLiteral("Scène —"), camerasToolbar);
    sceneRevisionLabel_->setMinimumWidth(108);
    sceneRevisionLabel_->setToolTip(QStringLiteral("Révision du manifeste de la scène chargée"));
    camerasToolbar->addWidget(sceneRevisionLabel_);
    systemMemoryLabel_ = new QLabel(QStringLiteral("RAM —"), camerasToolbar);
    systemMemoryLabel_->setToolTip(QStringLiteral("Charge de la mémoire physique système"));
    camerasToolbar->addWidget(systemMemoryLabel_);
    systemMemoryBar_ = new QProgressBar(camerasToolbar);
    systemMemoryBar_->setRange(0, 100);
    systemMemoryBar_->setTextVisible(false);
    systemMemoryBar_->setFixedSize(68, 12);
    camerasToolbar->addWidget(systemMemoryBar_);
    memoryRefreshTimer_ = new QTimer(this);
    connect(memoryRefreshTimer_, &QTimer::timeout, this, [this] { updateMemoryBanner(); });
    memoryRefreshTimer_->start(1500);
    updateMemoryBanner();
    // V17.443 - one-click regression review at the EXACT user-reported
    // viewpoints, independent of the six artistic camera presets.
    auto *anomaliesMenu = menuBar()->addMenu(QStringLiteral("Anomalies"));
    for (int i = 0; i < int(cse::anomalies::records.size()); ++i)
    {
        const auto &a = cse::anomalies::records[size_t(i)];
        const QString name = QStringLiteral("%1 - %2%3")
                                 .arg(QString::fromLatin1(a.id), QString::fromUtf8(a.title),
                                      a.cameraKnown ? QString() : QStringLiteral(" (position a definir)"));
        auto *act = anomaliesMenu->addAction(name);
        connect(act, &QAction::triggered, this, [this, i] { selectAnomaly(i); });
    }
    anomaliesMenu->addSeparator();
    auto *prevAnomaly = anomaliesMenu->addAction(QStringLiteral("Anomalie precedente"));
    prevAnomaly->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Left")));
    connect(prevAnomaly, &QAction::triggered, this,
            [this] { selectAnomaly(currentAnomaly_ < 0 ? 0 : std::max(0, currentAnomaly_ - 1)); });
    auto *nextAnomaly = anomaliesMenu->addAction(QStringLiteral("Anomalie suivante"));
    nextAnomaly->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Right")));
    connect(nextAnomaly, &QAction::triggered, this, [this] {
        selectAnomaly(currentAnomaly_ < 0 ? 0 : std::min(int(cse::anomalies::records.size()) - 1, currentAnomaly_ + 1));
    });
    auto *restoreAnomaly = anomaliesMenu->addAction(QStringLiteral("Retour a la position initiale"));
    connect(restoreAnomaly, &QAction::triggered, this, [this] { returnBeforeAnomaly(); });
    auto *reviewDock = new QDockWidget(QStringLiteral("Validation des anomalies"), this);
    anomalyReviewDock_ = reviewDock;
    auto *reviewWidget = new QWidget(reviewDock);
    auto *reviewLayout = new QVBoxLayout(reviewWidget);
    anomalyPanelLabel_ =
        new QLabel(QStringLiteral("Choisissez ANO-005 a ANO-014 depuis le menu Anomalies."), reviewWidget);
    anomalyPanelLabel_->setWordWrap(true);
    reviewLayout->addWidget(anomalyPanelLabel_);
    auto *reviewButtons = new QHBoxLayout;
    for (const auto &verdict :
         QVector<QPair<QString, QString>>{{QStringLiteral("Conforme"), QStringLiteral("conforme")},
                                          {QStringLiteral("Presente"), QStringLiteral("toujours_presente")},
                                          {QStringLiteral("A revoir"), QStringLiteral("a_revoir")}})
    {
        auto *button = new QPushButton(verdict.first, reviewWidget);
        connect(button, &QPushButton::clicked, this, [this, verdict] { saveAnomalyVerdict(verdict.second); });
        reviewButtons->addWidget(button);
    }
    reviewLayout->addLayout(reviewButtons);
    reviewDock->setWidget(reviewWidget);
    addDockWidget(Qt::BottomDockWidgetArea, reviewDock);
    reviewDock->setVisible(false);
    connect(anomaliesMenu, &QMenu::aboutToShow, reviewDock, [reviewDock] { reviewDock->show(); });

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("Outils"));
    auto *pacingAction = toolsMenu->addAction(QStringLiteral("Exporter benchmark fluidité (CSV + JSON)"));
    connect(pacingAction, &QAction::triggered, this, [this] { exportPacing(); });
    auto *benchmarkAction =
        toolsMenu->addAction(QStringLiteral("Benchmark V%1 — performances…").arg(CSE_WARP_PRODUCT_VERSION));
    connect(benchmarkAction, &QAction::triggered, this, [this] {
        if (scenePath_.isEmpty())
        {
            QMessageBox::information(this, QStringLiteral("Benchmark"),
                                     QStringLiteral("Charger une scène avant de lancer le benchmark."));
            return;
        }
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates{appDir + QStringLiteral("/outils/benchmark/moteur3d_v17_benchui.exe"),
                                     appDir + QStringLiteral("/tools/benchmark/moteur3d_v17_benchui.exe"),
                                     appDir + QStringLiteral("/moteur3d_v17_benchui.exe")};
        QString path;
        for (const QString &candidate : candidates)
        {
            if (QFileInfo::exists(candidate))
            {
                path = candidate;
                break;
            }
        }
        if (path.isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("Benchmark indisponible"),
                                 QStringLiteral("Programme de benchmark introuvable. Chemins vérifiés : %1")
                                     .arg(candidates.join(QStringLiteral("\n"))));
            return;
        }
        // The benchmark is another process: the DLL's atomic settings
        // cannot be shared implicitly. Transfer the active Raster quality
        // explicitly so all image/performance comparisons use the exact
        // same MSAA, anisotropy, LOD and culling configuration.
        const auto q = rasterQualitySettings();
        const QStringList arguments{scenePath_,
                                    QStringLiteral("--raster-msaa"),
                                    QString::number(q.msaaSamples),
                                    QStringLiteral("--raster-anisotropy"),
                                    QString::number(q.anisotropy),
                                    QStringLiteral("--raster-lod-percent"),
                                    QString::number(q.lodQualityPercent),
                                    QStringLiteral("--raster-culling"),
                                    QString::number(q.frustumCulling ? 1 : 0),
                                    QStringLiteral("--raster-lod-enabled"),
                                    QString::number(q.dynamicLod ? 1 : 0)};
        if (!QProcess::startDetached(path, arguments, QCoreApplication::applicationDirPath()))
            QMessageBox::critical(this, QStringLiteral("Benchmark"),
                                  QStringLiteral("Impossible d'ouvrir la fenêtre de benchmark."));
    });
    toolsMenu->menuAction()->setVisible(false);
    auto *helpMenu = menuBar()->addMenu(QStringLiteral("Aide"));
    auto *aboutAction = helpMenu->addAction(QStringLiteral("À propos de CSE WARP V%1…").arg(CSE_WARP_PRODUCT_VERSION));
    connect(aboutAction, &QAction::triggered, this, [this] {
        const QString gpu = gpuAdapterLabel();
        const QString revision = githubRevision();
        const QString compileDate = QString::fromLatin1(__DATE__ " " __TIME__);
        const auto quality = rasterQualitySettings();
        const bool zEnabled = backendBox_->currentIndex() >= 4 &&
                              qEnvironmentVariable("M3D_V17_DISABLE_Z_PREPASS") != QStringLiteral("1") &&
                              qEnvironmentVariable("M3D_V17_EXPERIMENT_Z_PREPASS") != QStringLiteral("0");
        const bool lightEnabled = qEnvironmentVariable("M3D_V17_DISABLE_LIGHT_AABB_CULL") != QStringLiteral("1") &&
                                  qEnvironmentVariable("M3D_V17_EXPERIMENT_LIGHT_AABB_CULL") != QStringLiteral("0");
        const QString backend = backendBox_ ? backendBox_->currentText() : QStringLiteral("indéterminé");
        const QString sceneRevision = sceneRevisionLabel_ ? sceneRevisionLabel_->text() : QStringLiteral("Scène —");
        const QString details =
            QStringLiteral("CSE WARP Beta X / V%1\\n"
                           "Révision GitHub : %2\\n"
                           "Compilation : %3\\n"
                           "Système : %4\\n"
                           "Adaptateur principal DXGI : %5\\n"
                           "Rendu sélectionné : %6\\n"
                           "Qt : %7\\n"
                           "MSAA : %8x | Anisotropie : %9x | PBR : %10 px\\n"
                           "Z-prepass GPU : %11 | Rejet de volume des lampes : %12\\n"
                           "Build GitHub Actions : %13\\nScène : %14\\n"
                           "Composants : PBR-%15, CPU-%16, DX11-%17, Vulkan-%18\\n"
                           "HUD caméra / rapports d'anomalies PNG+JSON / restauration : disponibles")
                .arg(CSE_WARP_PRODUCT_VERSION)
                .arg(revision)
                .arg(compileDate)
                .arg(QSysInfo::prettyProductName())
                .arg(gpu)
                .arg(backend)
                .arg(QString::fromLatin1(qVersion()))
                .arg(quality.msaaSamples)
                .arg(quality.anisotropy)
                .arg(textureResolution_)
                .arg(zEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                .arg(lightEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                .arg(QStringLiteral(CSE_WARP_CI_BUILD))
                .arg(sceneRevision)
                .arg(QStringLiteral(CSE_WARP_COMPONENT_PBR))
                .arg(QStringLiteral(CSE_WARP_COMPONENT_CPU_LIGHTING))
                .arg(QStringLiteral(CSE_WARP_COMPONENT_DIRECTX11))
                .arg(QStringLiteral(CSE_WARP_COMPONENT_VULKAN));
        QMessageBox message(this);
        message.setWindowTitle(QStringLiteral("À propos de CSE WARP V%1").arg(CSE_WARP_PRODUCT_VERSION));
        message.setIcon(QMessageBox::Information);
        message.setTextFormat(Qt::RichText);
        message.setText(QStringLiteral("<h3>CSE WARP Beta X — V%1</h3>"
                                       "<p><b>Révision :</b> %2<br/><b>Compilation :</b> %3</p>"
                                       "<p><b>GPU principal (DXGI) :</b> %4<br/><b>Rendu :</b> %5</p>"
                                       "<p><b>Navigation :</b> HUD position X/Y/Z, cap, inclinaison et FOV.<br/>"
                                       "<b>Diagnostic architectural :</b> capture PNG + position JSON restaurable.<br/>"
                                       "<b>Raster :</b> Z-prepass %6, rejet des lumières hors portée %7.</p>"
                                       "<p><b>Configuration :</b> Qt %8 ; MSAA %9x ; PBR %10 px.</p>"
                                       "<p><b>Build GitHub Actions :</b> %11<br/><b>Scène :</b> %12</p>"
                                       "<p><b>Révisions moteur :</b> PBR-%13 ; CPU-%14 ; DX11-%15 ; Vulkan-%16</p>")
                            .arg(CSE_WARP_PRODUCT_VERSION)
                            .arg(revision.toHtmlEscaped())
                            .arg(compileDate.toHtmlEscaped())
                            .arg(gpu.toHtmlEscaped())
                            .arg(backend.toHtmlEscaped())
                            .arg(zEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                            .arg(lightEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                            .arg(QString::fromLatin1(qVersion()))
                            .arg(quality.msaaSamples)
                            .arg(textureResolution_)
                            .arg(QStringLiteral(CSE_WARP_CI_BUILD))
                            .arg(sceneRevision.toHtmlEscaped())
                            .arg(QStringLiteral(CSE_WARP_COMPONENT_PBR))
                            .arg(QStringLiteral(CSE_WARP_COMPONENT_CPU_LIGHTING))
                            .arg(QStringLiteral(CSE_WARP_COMPONENT_DIRECTX11))
                            .arg(QStringLiteral(CSE_WARP_COMPONENT_VULKAN)));
        auto *copy = message.addButton(QStringLiteral("Copier les informations système"), QMessageBox::ActionRole);
        message.addButton(QMessageBox::Ok);
        message.exec();
        if (message.clickedButton() == copy)
            QGuiApplication::clipboard()->setText(details);
    });

    auto *left = new QDockWidget(QStringLiteral("Explorateur de scène"), this);
    sceneExplorerDock_ = left;
    auto *tabs = new QTabWidget;
    sceneTree_ = new QTreeWidget;
    sceneTree_->setHeaderLabels({QStringLiteral("Hiérarchie"), QStringLiteral("Matériau")});
    resourceTree_ = new QTreeWidget;
    resourceTree_->setHeaderLabels({QStringLiteral("Ressource"), QStringLiteral("Origine")});
    tabs->addTab(sceneTree_, QStringLiteral("Objets"));
    tabs->addTab(resourceTree_, QStringLiteral("Ressources"));
    left->setWidget(tabs);
    addDockWidget(Qt::LeftDockWidgetArea, left);
    left->resize(360, 720);

    auto *right = new QDockWidget(QStringLiteral("Rendu et matériaux"), this);
    renderControlsDock_ = right;
    auto *controls = new QWidget;
    auto *vertical = new QVBoxLayout(controls);
    radioToggleButton_ = new QPushButton(QStringLiteral("Activer la radio"), controls);
    radioStatusLabel_ = new QLabel(QStringLiteral("Radio arrêtée"), controls);
    radioStatusLabel_->setWordWrap(true);
    vertical->addWidget(new QLabel(QStringLiteral("SON AMBIANT — RADIO UKRAINIENNE"), controls));
    vertical->addWidget(radioToggleButton_);
    vertical->addWidget(radioStatusLabel_);
    radioVolumeSlider_ = new QSlider(Qt::Horizontal, controls);
    radioVolumeSlider_->setRange(0, 100);
    radioVolumeSlider_->setValue(65);
    radioVolumeSlider_->setToolTip(QStringLiteral("Volume de la radio 3D (0 à 100 %)"));
    vertical->addWidget(new QLabel(QStringLiteral("Volume radio"), controls));
    vertical->addWidget(radioVolumeSlider_);
    connect(radioVolumeSlider_, &QSlider::valueChanged, this,
            [this](int value) { radioAudio_.setVolume(float(value) / 500.f); });
    connect(radioToggleButton_, &QPushButton::clicked, this, [this] {
        if (radioPlaying_)
        {
            radioAutoplayEnabled_ = false;
            stopRadio();
        }
        else
        {
            radioAutoplayEnabled_ = true;
            playRadio();
        }
    });
    auto *form = new QFormLayout;
    cameraBox_ = new QComboBox;
    backendBox_ = new QComboBox;
    directDisplayBox_ = new QCheckBox(QStringLiteral("Affichage GPU direct natif (DX11 / Vulkan / LumaTrace)"));
#ifdef _WIN32
    // Vulkan direct presentation uses an engine-owned Win32 surface on the
    // private top-level HWND, isolated from every other renderer window.
    directDisplayBox_->setCheckState(Qt::Checked);
    directDisplayBox_->setToolTip(
        QStringLiteral("DX11, Vulkan et LumaTrace présentent dans leur HWND dédié. Décocher pour afficher "
                       "le rendu de ce backend via QImage."));
#else
    directDisplayBox_->setChecked(false);
    directDisplayBox_->setEnabled(false);
#endif
    backendBox_->addItems({QStringLiteral("CPU Ray Tracing PBR — direct"),
                           QStringLiteral("GPU D3D11 Compute — rapide, ombres + verre"),
                           QStringLiteral("CPU Éclairage global — 2 rebonds (qualité réglable)"),
                           QStringLiteral("CPU Éclairage global — 4 rebonds (qualité réglable)"),
                           QStringLiteral("V17 GPU D3D11 Raster — exploration rapide"),
                           QStringLiteral("V17 Vulkan Raster — expérimental (GPU matériel)"),
                           QStringLiteral("LumaTrace BVH4 — test (éclairage GPU)")});
#ifndef _WIN32
    backendBox_->setItemData(1, 0, Qt::UserRole - 1);
    backendBox_->setItemData(4, 0, Qt::UserRole - 1);
    backendBox_->setItemData(5, 0, Qt::UserRole - 1);
    backendBox_->setItemData(6, 0, Qt::UserRole - 1);
#endif
#if !M3D_V17_VULKAN_ENABLED
    backendBox_->setItemData(5, 0, Qt::UserRole - 1);
    backendBox_->setItemData(5, QStringLiteral("Vulkan SDK absent lors de la compilation"), Qt::ToolTipRole);
#endif
#ifdef _WIN32
    // Default Windows user path: native D3D11 raster with DXGI presentation.
    // Vulkan and all CPU/compute options remain selectable.
    backendBox_->setCurrentIndex(4);
#endif
    // Keep the backend choice visible at 1366px: renderer names
    // previously consumed the entire top toolbar and hid the renderer.
    auto *fastBackend = new QComboBox(camerasToolbar);
    fastBackend->setObjectName(QStringLiteral("v17QuickRenderer"));
    fastBackend->setMinimumWidth(220);
    fastBackend->setMaximumWidth(330);
    for (int i = 0; i < backendBox_->count(); ++i)
    {
        fastBackend->addItem(i == 4
                                 ? QStringLiteral("DirectX 11 Raster")
                                 : (i == 5 ? QStringLiteral("Vulkan Raster")
                                           : (i == 6 ? QStringLiteral("LumaTrace BVH4") : backendBox_->itemText(i))));
        fastBackend->setItemData(i, backendBox_->itemData(i, Qt::UserRole - 1), Qt::UserRole - 1);
        fastBackend->setItemData(i, backendBox_->itemText(i), Qt::ToolTipRole);
    }
    fastBackend->setCurrentIndex(backendBox_->currentIndex());
    camerasToolbar->addSeparator();
    camerasToolbar->addWidget(new QLabel(QStringLiteral("Moteur : "), camerasToolbar));
    camerasToolbar->addWidget(fastBackend);
    auto *quickRender = camerasToolbar->addAction(QStringLiteral("Actualiser"));
    connect(quickRender, &QAction::triggered, this, [this] { requestRender(); });
    connect(fastBackend, qOverload<int>(&QComboBox::currentIndexChanged), backendBox_, &QComboBox::setCurrentIndex);
    connect(backendBox_, qOverload<int>(&QComboBox::currentIndexChanged), fastBackend, &QComboBox::setCurrentIndex);
    qualityBox_ = new QComboBox;
    // Keep native and Full HD side by side. Store explicit width/height
    // instead of deriving them from an index or assuming exact 16:9.
    qualityBox_->addItem(QStringLiteral("Viewport adaptatif — taille de la fenêtre moteur"), QSize());
    qualityBox_->addItem(QStringLiteral("Dell natif 1366 × 768"), QSize(1366, 768));
    qualityBox_->addItem(QStringLiteral("Full HD 1920 × 1080"), QSize(1920, 1080));
    qualityBox_->addItem(QStringLiteral("QHD 2560 × 1440"), QSize(2560, 1440));
    qualityBox_->addItem(QStringLiteral("4K UHD 3840 × 2160"), QSize(3840, 2160));
    qualityBox_->setCurrentIndex(0); // Full-panel display by default; Dell 1366x768 remains available.
    directSamples_ = new QComboBox;
    for (int n : {1, 4, 16})
        directSamples_->addItem(QStringLiteral("%1 échantillon(s) / pixel").arg(n), n);
    directSamples_->setCurrentIndex(0);
    giSamples_ = new QComboBox;
    for (int n : {4, 8, 16, 32, 64})
        giSamples_->addItem(QStringLiteral("%1 échantillons / pixel").arg(n), n);
    giSamples_->setCurrentIndex(2); // Native Full HD quality baseline: 16 spp.
    giSamples_->setEnabled(false);
    progressiveGi_ = new QCheckBox(QStringLiteral("GI progressive (afficher les passes)"));
    progressiveGi_->setChecked(true);
    progressiveGi_->setEnabled(false);
    denoiseGi_ = new QCheckBox(QStringLiteral("Débruitage GI bilatéral (optionnel)"));
    denoiseGi_->setChecked(true);
    denoiseGi_->setEnabled(false);
    denoiseDirect_ = new QCheckBox(QStringLiteral("Débruitage CPU direct (bilatéral)"));
    denoiseDirect_->setChecked(false); // avoid blurring PBR details by default
    sunHour_ = new QDoubleSpinBox;
    sunHour_->setRange(0.0, 23.99);
    sunHour_->setDecimals(2);
    sunHour_->setSingleStep(.25);
    sunHour_->setSuffix(QStringLiteral(" h"));
    sunHour_->setValue(14.75);
    sunHour_->setToolTip(QStringLiteral("Soleil et éclairage de 00 h à 24 h — aube, jour, crépuscule, nuit"));
    cloudAnimationBox_ = new QCheckBox(QStringLiteral("Nuages animés sur le ciel"));
    cloudAnimationBox_->setChecked(true);
    cloudAnimationBox_->setToolTip(
        QStringLiteral("Véritables cirrus calculés par le shader DirectX11. La scène reste nette."));
    dayNightCycleBox_ = new QCheckBox(QStringLiteral("Simulation jour/nuit (24 h en 6 min)"));
    dayNightCycleBox_->setChecked(false);
    dayNightCycleBox_->setToolTip(
        QStringLiteral("Fait progresser le soleil, le ciel, les ombres et les éclairages du bâtiment."));
    exteriorLightsBox_ = new QCheckBox(QStringLiteral("Éclairage extérieur automatique (façade + allée)"));
    exteriorLightsBox_->setToolTip(
        QStringLiteral("Le capteur solaire active les lumières sous le seuil du soir et les éteint le jour.\n"
                       "Hystérésis intégrée pour éviter les clignotements au crépuscule."));
    // Two independent controls avoid coupling global illumination and
    // output tone mapping. Preserve the existing hybrid preference, while
    // the new filmic shoulder defaults OFF to avoid V17.309 wash-out.
    hybridLightBox_ = new QCheckBox(QStringLiteral("Rebonds indirects 3D (bêta)"));
    hybridLightBox_->setToolTip(
        QStringLiteral("Sondes précalculées, ciel plus froid, rebonds colorés et zones abritées.\n"
                       "Ne change pas le traitement tonal."));
    // Calibrated linear-light energy path. Does not modify sRGB textures
    // or flatten dielectric reflections; legacy lighting remains A/B.
    colorEnergyBox_ = new QCheckBox(QStringLiteral("Couleurs fidèles et lumière équilibrée"));
    colorEnergyBox_->setToolTip(QStringLiteral("Équilibre l'éclairage ambiant, le soleil et les reflets du béton\n"
                                               "dans l'espace linéaire, sans filtre de saturation ni flou PBR."));
    colorExposureBox_ = new QDoubleSpinBox;
    colorExposureBox_->setRange(.65, 1.45);
    colorExposureBox_->setDecimals(2);
    colorExposureBox_->setSingleStep(.05);
    colorExposureBox_->setToolTip(QStringLiteral("Réglage fin de l'exposition du nouveau modèle. 1,00 = neutre.\n"
                                                 "Préserve la teinte et la saturation des matériaux."));
    filmicToneBox_ = new QCheckBox(QStringLiteral("Compression tonale filmique (optionnelle)"));
    filmicToneBox_->setToolTip(QStringLiteral("Adoucit les hautes lumières sans remonter les demi-teintes.\n"
                                              "Indépendant des sondes indirectes."));
    QSettings hybridPrefs(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    hybridLightBox_->setChecked(hybridPrefs.value(QStringLiteral("Raster/HybridLighting"), true).toBool());
    filmicToneBox_->setChecked(hybridPrefs.value(QStringLiteral("Raster/FilmicTonemap"), false).toBool());
    colorEnergyBox_->setChecked(hybridPrefs.value(QStringLiteral("Raster/ColorEnergyBalance"), true).toBool());
    colorExposureBox_->setValue(hybridPrefs.value(QStringLiteral("Raster/ColorExposure"), 1.0).toDouble());
    colorExposureBox_->setEnabled(colorEnergyBox_->isChecked());
    m3d::setRasterHybridLightingEnabled(hybridLightBox_->isChecked());
    m3d::setRasterFilmicTonemapEnabled(filmicToneBox_->isChecked());
    m3d::setRasterColorEnergyBalanceEnabled(colorEnergyBox_->isChecked());
    m3d::setRasterColorExposure(float(colorExposureBox_->value()));
    azimuth_ = new QSlider(Qt::Horizontal);
    azimuth_->setRange(-180, 180);
    azimuth_->setValue(0);
    form->addRow(QStringLiteral("Caméra"), cameraBox_);
    form->addRow(QStringLiteral("Méthode de rendu"), backendBox_);
    form->addRow(directDisplayBox_);
    form->addRow(QStringLiteral("Résolution"), qualityBox_);
    form->addRow(QStringLiteral("Échantillons CPU direct"), directSamples_);
    form->addRow(QStringLiteral("Échantillons GI"), giSamples_);
    form->addRow(QStringLiteral("Heure solaire"), sunHour_);
    form->addRow(cloudAnimationBox_);
    form->addRow(dayNightCycleBox_);
    QSettings exteriorPrefs(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    exteriorLightsBox_->setChecked(exteriorPrefs.value(QStringLiteral("Lighting/ExteriorAuto"), true).toBool());
    m3d::setExteriorLightingEnabled(exteriorLightsBox_->isChecked());
    form->addRow(exteriorLightsBox_);
    form->addRow(hybridLightBox_);
    form->addRow(colorEnergyBox_);
    form->addRow(QStringLiteral("Exposition linéaire"), colorExposureBox_);
    form->addRow(filmicToneBox_);
    form->addRow(QStringLiteral("Rotation caméra"), azimuth_);
    vertical->addLayout(form);
    vertical->addWidget(progressiveGi_);
    vertical->addWidget(denoiseGi_);
    vertical->addWidget(denoiseDirect_);
    edgeAa_ = new QCheckBox(QStringLiteral("Antialiasing des contours"));
    edgeAa_->setChecked(true);
    vertical->addWidget(edgeAa_);
    // One quality source for Qt and embedding host applications.
    // All controls update an atomic engine snapshot; the GPU worker
    // applies the new settings only at the next frame boundary.
    auto *rasterQuality = new QGroupBox(QStringLiteral("Qualité Raster DirectX 11"));
    auto *rasterForm = new QFormLayout(rasterQuality);
    rasterPresetBox_ = new QComboBox;
    rasterPresetBox_->addItems({QStringLiteral("Performance"), QStringLiteral("Équilibrée"),
                                QStringLiteral("Élevée — qualité historique"),
                                QStringLiteral("Ultra — finesse maximale"), QStringLiteral("Personnalisée")});
    rasterForm->addRow(QStringLiteral("Préréglage"), rasterPresetBox_);
    rasterMsaaBox_ = new QComboBox;
    for (int v : {1, 2, 4, 8})
        rasterMsaaBox_->addItem(QStringLiteral("MSAA %1×").arg(v), v);
    rasterForm->addRow(QStringLiteral("Anti-aliasing GPU"), rasterMsaaBox_);
    rasterAnisoBox_ = new QComboBox;
    for (int v : {1, 2, 4, 8, 16})
        rasterAnisoBox_->addItem(QStringLiteral("Anisotropie %1×").arg(v), v);
    rasterForm->addRow(QStringLiteral("Textures PBR"), rasterAnisoBox_);
    rasterLodBox_ = new QComboBox;
    for (int v : {50, 60, 75, 85, 100, 125, 150, 180, 200})
        rasterLodBox_->addItem(QStringLiteral("Qualité LOD %1 %").arg(v), v);
    rasterForm->addRow(QStringLiteral("Distance détails"), rasterLodBox_);
    rasterCullBox_ = new QCheckBox(QStringLiteral("Ne pas calculer les objets hors caméra"));
    rasterForm->addRow(rasterCullBox_);
    rasterDynamicLodBox_ = new QCheckBox(QStringLiteral("LOD adaptatif des objets éloignés"));
    rasterForm->addRow(rasterDynamicLodBox_);
    vertical->addWidget(rasterQuality);
    QSettings stored(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    int initialPreset = std::clamp(stored.value(QStringLiteral("Raster/Preset"), 2).toInt(), 0, 4);
    // An embedding host may configure the raster API BEFORE launching
    // Qt. Respect it unless the user already saved UI preferences.
    auto initialQuality = stored.contains(QStringLiteral("Raster/Preset"))
                              ? rasterQualityProfile(RasterQualityProfile(std::min(initialPreset, 3)))
                              : rasterQualitySettings();
    if (!stored.contains(QStringLiteral("Raster/Preset")))
    {
        initialPreset = 4;
        for (int candidate = 0; candidate < 4; ++candidate)
        {
            const auto q = rasterQualityProfile(RasterQualityProfile(candidate));
            if (q.msaaSamples == initialQuality.msaaSamples && q.anisotropy == initialQuality.anisotropy &&
                q.lodQualityPercent == initialQuality.lodQualityPercent &&
                q.frustumCulling == initialQuality.frustumCulling && q.dynamicLod == initialQuality.dynamicLod)
            {
                initialPreset = candidate;
                break;
            }
        }
    }
    if (initialPreset == 4)
    {
        initialQuality.msaaSamples = stored.value(QStringLiteral("Raster/MSAA"), 4).toUInt();
        initialQuality.anisotropy = stored.value(QStringLiteral("Raster/Anisotropy"), 16).toUInt();
        initialQuality.lodQualityPercent = stored.value(QStringLiteral("Raster/LODPercent"), 100).toUInt();
        initialQuality.frustumCulling = stored.value(QStringLiteral("Raster/Culling"), true).toBool();
        initialQuality.dynamicLod = stored.value(QStringLiteral("Raster/DynamicLOD"), true).toBool();
    }
    if (rasterMsaaBox_->findData(int(initialQuality.msaaSamples)) < 0 ||
        rasterAnisoBox_->findData(int(initialQuality.anisotropy)) < 0 ||
        rasterLodBox_->findData(int(initialQuality.lodQualityPercent)) < 0)
    {
        initialPreset = 2;
        initialQuality = rasterQualityProfile(RasterQualityProfile::High);
    }
    rasterPresetBox_->setCurrentIndex(initialPreset);
    displayRasterSettings(initialQuality);
    std::wstring qualityError;
    setRasterQualitySettings(initialQuality, qualityError);
    connect(rasterPresetBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int selected) {
        if (selected >= 0 && selected < 4)
            displayRasterSettings(rasterQualityProfile(RasterQualityProfile(selected)));
        applyRasterSettings(false);
    });
    for (auto *combo : {rasterMsaaBox_, rasterAnisoBox_, rasterLodBox_})
    {
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int) { applyRasterSettings(true); });
    }
    connect(rasterCullBox_, &QCheckBox::toggled, this, [this](bool) { applyRasterSettings(true); });
    connect(rasterDynamicLodBox_, &QCheckBox::toggled, this, [this](bool) { applyRasterSettings(true); });
    explorationButton_ = new QPushButton(QStringLiteral("Démarrer exploration (V17)"));
    vertical->addWidget(explorationButton_);
    walkingModeBox_ = new QCheckBox(QStringLiteral("Marcher au sol (saut et accroupissement inclus)"));
    walkingModeBox_->setToolTip(
        QStringLiteral("Marche à hauteur d'homme, monte les marches de 34 cm au maximum,\n"
                       "saute avec inertie, s'accroupit sous les plafonds et bloque les murs.\n"
                       "Ctrl gauche : s'accroupir · Maj : turbo ×3. Décochez pour voler librement."));
    QSettings walkPreferences(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    walkingModeBox_->setChecked(walkPreferences.value(QStringLiteral("Exploration/WalkingV17_600"), true).toBool());
    vertical->addWidget(walkingModeBox_);
    explorationHelp_ = new QLabel(
        QStringLiteral("ZQSD / flèches : marcher · Espace : sauter · Ctrl gauche : s'accroupir · Maj : turbo\\n"
                       "Souris : regarder librement · clic droit : zoom ×3 · F11 : plein écran · Échap : arrêter · "
                       "E : échelle · Ctrl+Maj+A : anomalie."));
    explorationHelp_->setWordWrap(true);
    vertical->addWidget(explorationHelp_);
    connect(explorationButton_, &QPushButton::clicked, this, [this] { toggleExploration(); });
    bakeGiButton_ = new QPushButton(QStringLiteral("Précalculer la table GI (ray tracing CPU)"));
    bakeGiButton_->setToolTip(
        QStringLiteral("Calcule les vrais rebonds sur les surfaces et écrit une table GI pour 25 heures solaires.\n"
                       "Opération hors ligne dans un programme séparé, sans ralentir chaque image."));
    bakeGiButton_->setEnabled(false);
    vertical->addWidget(bakeGiButton_);
    bakeGiProcess_ = new QProcess(this);
    connect(bakeGiButton_, &QPushButton::clicked, this, [this] {
        if (scenePath_.isEmpty() || bakeGiProcess_->state() != QProcess::NotRunning)
            return;
        const QString appDir = QCoreApplication::applicationDirPath();
        QString exe = appDir + QStringLiteral("/outils/gi/moteur3d_v17_gibake.exe");
        // V17 distributables historically placed auxiliary tools beside
        // the main executable. Prefer the organized path, then accept that
        // existing package layout so GI bake works with either archive.
        if (!QFileInfo::exists(exe))
            exe = appDir + QStringLiteral("/moteur3d_v17_gibake.exe");
        if (!QFileInfo::exists(exe))
        {
            QMessageBox::warning(
                this, QStringLiteral("Précalcul GI"),
                QStringLiteral(
                    "Exécutable moteur3d_v17_gibake.exe introuvable dans outils/gi ni à côté de l'application."));
            return;
        }
        bakingScenePath_ = scenePath_;
        bakeGiButton_->setEnabled(false);
        bakeGiButton_->setText(QStringLiteral("Calcul de la table GI en cours…"));
        statusBar()->showMessage(
            QStringLiteral("Précalcul hors ligne : rayons et rebonds sur les surfaces de la scène…"));
        bakeGiProcess_->setProgram(exe);
        bakeGiProcess_->setArguments({bakingScenePath_, bakingScenePath_ + QStringLiteral(".m3dgi")});
        bakeGiProcess_->start();
    });
    connect(
        bakeGiProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            bakeGiButton_->setText(QStringLiteral("Précalculer la table GI (ray tracing CPU)"));
            bakeGiButton_->setEnabled(bool(runtime_));
            if (exitStatus != QProcess::NormalExit || exitCode != 0)
            {
                statusBar()->showMessage(QStringLiteral("Échec précalcul GI : %1")
                                             .arg(QString::fromUtf8(bakeGiProcess_->readAllStandardError()).left(200)),
                                         15000);
                return;
            }
            if (bakingScenePath_ != scenePath_)
                return; // scene switched during bake
            setRasterGiCachePath(std::filesystem::path((scenePath_ + QStringLiteral(".m3dgi")).toStdWString()));
            statusBar()->showMessage(QStringLiteral("Table GI enregistrée pour %1 — rebonds chargés au prochain rendu.")
                                         .arg(QFileInfo(scenePath_).fileName()),
                                     16000);
            requestRender();
        });
    connect(walkingModeBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        updateExplorationSpeedIndicator();
        QSettings preferences(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        preferences.setValue(QStringLiteral("Exploration/WalkingV17_600"), enabled);
        if (exploring_ && freeCameraActive_ && runtime_)
        {
            if (enabled)
            {
                walk::spawn(runtime_->renderScene, freeCamera_.eye, walkingState_);
                walkingCollisionSyncPending_ = false;
                freeCamera_.eye = walk::eye(walkingState_);
                updateLook();
            }
            requestRender();
        }
    });
    auto *renderButton = new QPushButton(QStringLiteral("Calculer l'image"));
    vertical->addWidget(renderButton);
    auto *materials = new QGroupBox(QStringLiteral("Matériau sélectionné"));
    auto *matForm = new QFormLayout(materials);
    materialLabel_ = new QLabel(QStringLiteral("Sélectionner un matériau dans le catalogue"));
    materialLabel_->setWordWrap(true);
    roughness_ = new QDoubleSpinBox;
    roughness_->setRange(.01, 1.0);
    roughness_->setSingleStep(.05);
    roughness_->setDecimals(2);
    metallic_ = new QDoubleSpinBox;
    metallic_->setRange(0, 1);
    metallic_->setSingleStep(.05);
    metallic_->setDecimals(2);
    matForm->addRow(materialLabel_);
    matForm->addRow(QStringLiteral("Rugosité ×"), roughness_);
    matForm->addRow(QStringLiteral("Métallicité"), metallic_);
    vertical->addWidget(materials);
    vertical->addStretch(1);
    auto *rightScroll = new QScrollArea;
    rightScroll->setWidgetResizable(true);
    rightScroll->setWidget(controls);
    right->setWidget(rightScroll);
    addDockWidget(Qt::RightDockWidgetArea, right);
    right->setMinimumWidth(295);
    // Organize Scene / Rendering as tabs on one right sidebar.
    // Both panels remain accessible; the 3D viewport gets the former
    // left-dock width back (important at 1366x768 on the Dell).
    setDockOptions(dockOptions() | QMainWindow::AllowTabbedDocks);
    removeDockWidget(left);
    addDockWidget(Qt::RightDockWidgetArea, left);
    tabifyDockWidget(right, left);
    right->raise();
    // F11 controls the dedicated render window, leaving these controls
    // available behind it.
    auto *viewMenu = menuBar()->addMenu(QStringLiteral("Affichage"));
    auto *rendererHudAction = viewMenu->addAction(QStringLiteral("Afficher le HUD de la fenêtre de rendu (F2)"));
    rendererHudAction->setCheckable(true);
    QSettings rendererHudSettings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    rendererHudVisible_ = rendererHudSettings.value(QStringLiteral("Render/ShowHUD"), true).toBool();
    rendererHudAction->setChecked(rendererHudVisible_);
    rendererHudAction->setShortcut(QKeySequence(Qt::Key_F2));
    rendererHudAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(rendererHudAction);
    for (auto *view : backendWindows_)
        if (view)
            view->addAction(rendererHudAction);
    connect(rendererHudAction, &QAction::toggled, this, [this](bool visible) {
        rendererHudVisible_ = visible;
        QSettings cfg(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        cfg.setValue(QStringLiteral("Render/ShowHUD"), visible);
        updateRendererStamp();
        if (canvas_)
            canvas_->setHudVisible(visible);
    });
    auto *hudToggle = viewMenu->addAction(QStringLiteral("Afficher les coordonnées caméra (HUD)"));
    hudToggle->setCheckable(true);
    QSettings hudSettings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
    cameraHudEnabled_ = hudSettings.value(QStringLiteral("Camera/ShowHUD"), true).toBool();
    hudToggle->setChecked(cameraHudEnabled_);
    connect(hudToggle, &QAction::toggled, this, [this](bool enabled) {
        cameraHudEnabled_ = enabled;
        QSettings cfg(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        cfg.setValue(QStringLiteral("Camera/ShowHUD"), enabled);
        updateCameraHud();
        updateRendererStamp();
    });
    immersiveAction_ = viewMenu->addAction(QStringLiteral("Plein écran 3D (F11)"));
    immersiveAction_->setCheckable(true);
    immersiveAction_->setShortcut(QKeySequence(Qt::Key_F11));
    immersiveAction_->setShortcutContext(Qt::ApplicationShortcut);
    // Application shortcut keeps F11 available while a render window has focus.
    addAction(immersiveAction_);
    for (auto *view : backendWindows_)
        if (view)
            view->addAction(immersiveAction_);
    connect(immersiveAction_, &QAction::toggled, this, [this](bool enabled) {
        if (!canvas_)
            return;
        if (enabled)
        {
            canvas_->showFullScreen();
            canvas_->raise();
        }
        else
            canvas_->showNormal();
        QTimer::singleShot(0, this, [this] {
            if (exploring_)
            {
                releaseMouseLook();
                canvas_->setFocus();
                captureMouseLook();
            }
            if (nativeDirectSelected() || (qualityBox_ && qualityBox_->currentIndex() == 0))
                requestRender();
        });
    });

    connect(renderButton, &QPushButton::clicked, this, [this] { requestRender(); });
    connect(cameraBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        azimuth_->setValue(0);
        if (exploring_)
            activateFreeCamera();
        else
            freeCameraActive_ = false;
        updateCameraHud();
        requestRender();
    });
    connect(backendBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (exploring_ && !supportsExplorationBackend(index))
            stopExploration();
        if (index == 6 && !lumaTraceCloudDefaultApplied_)
        {
            lumaTraceCloudDefaultApplied_ = true;
            cloudAnimationBox_->setChecked(false);
        }
        const bool gi = index == 2 || index == 3;
        directDisplayBox_->setEnabled(index == 4 || index == 6 || (index == 5 && vulkanDirectAvailable()));
        directDisplayBox_->setToolTip(
            index == 6
                ? QStringLiteral("LumaTrace : présentation DX11 native. Les nuages sont coupés par défaut ; "
                                 "réactivez la case Nuages pour comparer.")
                : (index == 5 ? (vulkanDirectAvailable()
                                     ? QStringLiteral("Vulkan : swapchain Win32 dans sa fenêtre dédiée.")
                                     : QStringLiteral("Vulkan Win32 WSI indisponible dans cette compilation."))
                              : QStringLiteral(
                                    "DX11 : swapchain dans sa fenêtre dédiée. Décochez pour afficher l’image Qt.")));
        directSamples_->setEnabled(index == 0);
        denoiseDirect_->setEnabled(index == 0);
        giSamples_->setEnabled(gi);
        progressiveGi_->setEnabled(gi);
        denoiseGi_->setEnabled(gi);
        selectBackendOutputWindow();
        restoreExplorationViewportFocus();
        requestRender();
    });
    connect(directDisplayBox_, &QCheckBox::toggled, this, [this](bool) {
        if (backendBox_)
        {
            const int index = backendBox_->currentIndex();
            if (index >= 0 && index < int(nativeDirectFailed_.size()))
            {
                nativeDirectFailed_[size_t(index)] = false;
                nativeDirectFailureReason_[size_t(index)].clear();
            }
        }
        nativeReadyAttempts_ = 0;
        selectBackendOutputWindow();
        updateRendererStamp();
        restoreExplorationViewportFocus();
        requestRender();
    });
    connect(qualityBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { requestRender(); });
    connect(directSamples_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (backendBox_->currentIndex() == 0)
            requestRender();
    });
    connect(giSamples_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (backendBox_->currentIndex() == 2 || backendBox_->currentIndex() == 3)
            requestRender();
    });
    connect(progressiveGi_, &QCheckBox::toggled, this, [this](bool) {
        if (backendBox_->currentIndex() == 2 || backendBox_->currentIndex() == 3)
            requestRender();
    });
    connect(denoiseGi_, &QCheckBox::toggled, this, [this](bool) {
        if (backendBox_->currentIndex() == 2 || backendBox_->currentIndex() == 3)
            requestRender();
    });
    connect(denoiseDirect_, &QCheckBox::toggled, this, [this](bool) {
        if (backendBox_->currentIndex() == 0)
            requestRender();
    });
    connect(edgeAa_, &QCheckBox::toggled, this, [this](bool) { requestRender(); });
    connect(azimuth_, &QSlider::sliderReleased, this, [this] {
        updateCameraHud();
        requestRender();
    });
    connect(exteriorLightsBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        m3d::setExteriorLightingEnabled(enabled);
        QSettings settings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        settings.setValue(QStringLiteral("Lighting/ExteriorAuto"), enabled);
        if (runtime_)
            requestRender();
    });
    connect(hybridLightBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        m3d::setRasterHybridLightingEnabled(enabled);
        QSettings settings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        settings.setValue(QStringLiteral("Raster/HybridLighting"), enabled);
        if (backendBox_ && (backendBox_->currentIndex() == 4 || backendBox_->currentIndex() == 6))
            requestRender();
    });
    connect(colorEnergyBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        m3d::setRasterColorEnergyBalanceEnabled(enabled);
        colorExposureBox_->setEnabled(enabled);
        QSettings settings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        settings.setValue(QStringLiteral("Raster/ColorEnergyBalance"), enabled);
        if (backendBox_ && (backendBox_->currentIndex() == 4 || backendBox_->currentIndex() == 6))
            requestRender();
    });
    connect(colorExposureBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m3d::setRasterColorExposure(float(value));
        QSettings settings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        settings.setValue(QStringLiteral("Raster/ColorExposure"), value);
        if (backendBox_ && (backendBox_->currentIndex() == 4 || backendBox_->currentIndex() == 6) &&
            colorEnergyBox_->isChecked())
            requestRender();
    });
    connect(filmicToneBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        m3d::setRasterFilmicTonemapEnabled(enabled);
        QSettings settings(QStringLiteral("CSE WARP"), QStringLiteral("CSE WARP V17"));
        settings.setValue(QStringLiteral("Raster/FilmicTonemap"), enabled);
        if (backendBox_ && (backendBox_->currentIndex() == 4 || backendBox_->currentIndex() == 6))
            requestRender();
    });
    // DX11/QImage: lightweight, independent 30 Hz sky compositor.
    // Native Vulkan has no QImage at all: present a new GPU frame when
    // stationary so clouds and the solar sky do not freeze outside explore.
    // Coalesce idle refreshes (20 Hz cap); exploring already renders
    // continuously, and the in-flight guard prevents render queue buildup.
    skyAnimationTimer_.setInterval(33);
    connect(&skyAnimationTimer_, &QTimer::timeout, this, [this] {
        if (sceneLoading_ || !runtime_ || !backendBox_)
            return;
        if (nativeDirectSelected())
        {
            if (!exploring_ && (cloudAnimationBox_->isChecked() || dayNightCycleBox_->isChecked()) &&
                !watcher_.isRunning() && !pending_)
            {
                const qint64 now = skyMotionClock_.elapsed();
                if (now - lastVulkanIdleSkyRequestMs_ >= 50)
                {
                    lastVulkanIdleSkyRequestMs_ = now;
                    requestRender();
                }
            }
            return;
        }
        if (!hasRenderedPose_ || lastImage_.isNull() ||
            (backendBox_->currentIndex() != 4 && backendBox_->currentIndex() != 5 && backendBox_->currentIndex() != 6))
            return;
        if (!cloudAnimationBox_->isChecked() && !dayNightCycleBox_->isChecked() && canvas_->hasSkyFrame())
            return;
        const auto camera = v16::makeCamera(lastRenderedPose_, lastImage_.width(), lastImage_.height());
        const float hour =
            dayNightCycleBox_->isChecked()
                ? float(cse::weather::cycleHour(dayNightOriginHour_, double(dayNightClock_.elapsed()) / 1000.0))
                : float(sunHour_->value());
        const double seconds = double(skyMotionClock_.elapsed()) / 1000.0;
        canvas_->drawSky(camera, hour, seconds, cloudAnimationBox_->isChecked(), dayNightCycleBox_->isChecked());
    });
    skyAnimationTimer_.start();
    // Weather animation deliberately faster than the old almost-static sky.
    m3d::setRasterCloudTimeScale(24.f);
    connect(cloudAnimationBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        m3d::setRasterCloudsEnabled(enabled);
        m3d::setRasterCloudTimeScale(!enabled ? 0.f : (dayNightCycleBox_->isChecked() ? 240.f : 24.f));
        canvas_->invalidateSky();
        if (backendBox_ &&
            (backendBox_->currentIndex() == 4 || backendBox_->currentIndex() == 5 || backendBox_->currentIndex() == 6))
            requestRender();
    });
    dayNightTimer_.setInterval(1500); // Update simulated time without queuing a render every tick
    connect(&dayNightTimer_, &QTimer::timeout, this, [this] {
        if (!runtime_ || sceneLoading_)
            return;
        // Simulated time advances independently of GPU frame duration.
        // Elapsed time is absolute since the start, never accumulated
        // from successful frames. Long HD620 GPU frames cannot freeze
        // or stretch the 6-minute day/night cycle.
        const double seconds = double(dayNightClock_.elapsed()) / 1000.0;
        const double next = cse::weather::cycleHour(dayNightOriginHour_, seconds);
        if (std::abs(next - sunHour_->value()) < .01)
            return;
        applyingDayNightTick_ = true;
        sunHour_->setValue(next);
        applyingDayNightTick_ = false;
    });
    connect(dayNightCycleBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled)
        {
            dayNightOriginHour_ = sunHour_->value();
            dayNightClock_.start();
            dayNightTimer_.start();
        }
        else
            dayNightTimer_.stop();
        m3d::setRasterCloudTimeScale(!cloudAnimationBox_->isChecked() ? 0.f : (enabled ? 240.f : 24.f));
        if (!enabled && runtime_)
        {
            {
                std::lock_guard<std::mutex> lock(cpuShadows_->mutex);
                cpuShadows_->hour = -1.f;
            }
            const int backend = backendBox_ ? backendBox_->currentIndex() : -1;
            if (backend == 4 || backend == 5 || backend == 6)
                requestRender();
        }
    });
    connect(sunHour_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double hour) {
        // A deliberate manual time change reanchors the running clock.
        if (dayNightCycleBox_ && dayNightCycleBox_->isChecked() && !applyingDayNightTick_)
        {
            dayNightOriginHour_ = hour;
            dayNightClock_.restart();
        }
        if (!runtime_)
            return;
        // No UI-thread wait for a slow DX11 frame. Only the latest hour
        // is queued, and in-flight frames retain their captured hour.
        runtime_->daylightHour = float(hour);
        canvas_->invalidateSky();
        // Automatic cycle renders are restricted to the two live raster
        // backends. CPU GI and CPU direct can take seconds or minutes per
        // image; queuing those on every solar tick looks like a frozen UI.
        if (applyingDayNightTick_)
        {
            const int backend = backendBox_ ? backendBox_->currentIndex() : -1;
            if (backend != 4 && backend != 5 && backend != 6)
                return;
            const qint64 now = dayNightClock_.elapsed();
            if (watcher_.isRunning() || now - lastDayNightRenderMs_ < 5000)
                return;
            lastDayNightRenderMs_ = now;
        }
        requestRender();
    });
    connect(roughness_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { modifyMaterial(); });
    connect(metallic_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { modifyMaterial(); });
    connect(resourceTree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        const QString key = item->data(0, Qt::UserRole).toString();
        if (runtime_ && runtime_->materialIndex.contains(key))
            chooseMaterial(runtime_->materialIndex.value(key), key);
    });
    connect(sceneTree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        const QString key = item->data(0, Qt::UserRole).toString();
        if (runtime_ && runtime_->materialIndex.contains(key))
            chooseMaterial(runtime_->materialIndex.value(key), key);
    });
}
