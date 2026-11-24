#include "moteur3d_v17_window.h"

using namespace m3d;

void V17Window::chooseMaterial(int id, const QString &name)
{
    if (!runtime_ || id < 0 || id >= int(runtime_->renderScene.mat.size()))
        return;
    selectedMaterial_ = id;
    updatingControls_ = true;
    materialLabel_->setText(name);
    const auto &m = runtime_->renderScene.mat[size_t(id)];
    roughness_->setValue(m.roughnessScale);
    metallic_->setValue(m.metallic);
    updatingControls_ = false;
}

void V17Window::modifyMaterial()
{
    if (updatingControls_ || !runtime_ || selectedMaterial_ < 0)
        return;
    // Only mutate scene resources once the previous render no longer reads them.
    renderSerial_->fetch_add(1);
    if (watcher_.isRunning())
        watcher_.waitForFinished();
    auto &m = runtime_->renderScene.mat[size_t(selectedMaterial_)];
    m.roughnessScale = float(roughness_->value());
    m.metallic = float(metallic_->value());
    // Required for GPU fast-path cache invalidation. The shadow map itself
    // depends only on geometry/sun, not on metallicity or roughness.
    runtime_->renderScene.markGpuParametersChanged();
    requestRender();
}

void V17Window::saveMaterials()
{
    if (materialsPath_.isEmpty() || !runtime_)
    {
        QMessageBox::warning(this, "Matériaux", "Aucun catalogue chargé.");
        return;
    }
    if (watcher_.isRunning())
        watcher_.waitForFinished();
    QFile file(materialsPath_);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, "Matériaux", file.errorString());
        return;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject())
    {
        QMessageBox::critical(this, "Matériaux", "Fichier JSON invalide.");
        return;
    }
    QJsonObject root = document.object();
    QJsonArray materials = root.value("materials").toArray();
    for (int i = 0; i < materials.size(); ++i)
    {
        auto j = materials[i].toObject();
        const QString id = j.value("id").toString();
        if (!runtime_->materialIndex.contains(id))
            continue;
        const auto &m = runtime_->renderScene.mat[size_t(runtime_->materialIndex.value(id))];
        j.insert("roughnessScale", double(m.roughnessScale));
        j.insert("metallic", double(m.metallic));
        materials[i] = j;
    }
    root.insert("materials", materials);
    QSaveFile output(materialsPath_);
    if (!output.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, "Matériaux", output.errorString());
        return;
    }
    const auto content = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (output.write(content) != content.size() || !output.commit())
    {
        QMessageBox::critical(this, "Matériaux", "Écriture du catalogue impossible.");
        return;
    }
    statusBar()->showMessage(QStringLiteral("Matériaux enregistrés dans %1").arg(materialsPath_), 15000);
}

void V17Window::openScene(const QString &path)
{
    if (path.isEmpty())
        return;
    if (exploring_)
        stopExploration();
    freeCameraActive_ = false;
    // Invalidates previous scene/camera frame without waiting on its GPU.
    renderSerial_->fetch_add(1);
    pending_ = false;
    queuedScenePath_ = path;
    ++sceneLoadGeneration_;
    sceneLoading_ = true;
    sceneLoadProgress_->setValue(0);
    sceneLoadLabel_->setText(QStringLiteral("Chargement de la scène…"));
    sceneLoadLabel_->show();
    sceneLoadProgress_->show();
    lastImage_ = QImage{};
    hasRenderedPose_ = false;
    for (auto *view : backendWindows_)
    {
        if (!view)
            continue;
        // A native-present worker cannot be interrupted mid-Present. Keep
        // Qt from painting over its HWND until that frame leaves the GPU.
        const bool preserveInFlightPresent = watcher_.isRunning() && activeRenderDirect_ && activeRenderBackend_ >= 0 &&
                                             activeRenderBackend_ < int(backendWindows_.size()) &&
                                             view == backendWindows_[size_t(activeRenderBackend_)];
        view->setNativePresentation(preserveInFlightPresent);
        view->setImage(QImage{});
        view->setCameraHud({}, false);
        view->enableSkyCompositing(false);
        view->setPlaceholder(QStringLiteral("CSE WARP V17\nChargement de la scène…"));
    }
    backendLastRenderer_.fill(QString{});
    backendLastRequest_.fill(QString{});
    hasRenderedPose_ = false;
    freeCameraActive_ = false;
    cameraBox_->setEnabled(false);
    if (explorationButton_)
        explorationButton_->setEnabled(false);
    if (bakeGiButton_)
        bakeGiButton_->setEnabled(false);
    if (bakeGiProcess_ && bakeGiProcess_->state() != QProcess::NotRunning)
        bakeGiProcess_->kill();
    if (!loadWatcher_.isRunning())
        beginSceneLoad();
}

void V17Window::beginSceneLoad()
{
    if (queuedScenePath_.isEmpty())
        return;
    const QString path = std::exchange(queuedScenePath_, QString{});
    const uint64_t token = sceneLoadGeneration_;
    const int resolution = textureResolution_;
    sceneLoadProgress_->setValue(5);
    sceneLoadLabel_->setText(QStringLiteral("Lecture JSON et ressources…"));
    loadWatcher_.setFuture(QtConcurrent::run([this, path, token, resolution] {
        SceneLoadOutcome outcome;
        outcome.path = path;
        outcome.generation = token;
        const auto loaded = v16::loadScene(path);
        if (!loaded)
        {
            outcome.error = loaded.error;
            return outcome;
        }
        outcome.database = loaded.scene;
        auto build =
            v16::buildRuntime(loaded.scene, resolution, [this, token](int percent, const QString &description) {
                QMetaObject::invokeMethod(
                    this,
                    [this, token, percent, description] {
                        if (!sceneLoading_ || token != sceneLoadGeneration_)
                            return;
                        sceneLoadProgress_->setValue(percent);
                        sceneLoadLabel_->setText(description);
                        canvas_->setPlaceholder(
                            QStringLiteral("CSE WARP V17\nChargement de la scène…\n%1").arg(description));
                    },
                    Qt::QueuedConnection);
            });
        if (!build)
        {
            outcome.error = build.error;
            return outcome;
        }
        outcome.runtime = std::make_shared<v16::Runtime>(std::move(build.runtime));
        return outcome;
    }));
}

void V17Window::finishSceneLoad()
{
    if (loadWatcher_.isRunning())
        return; // an older completion was queued
    const SceneLoadOutcome outcome = loadWatcher_.result();
    if (outcome.generation != sceneLoadGeneration_ || !queuedScenePath_.isEmpty())
    {
        if (!queuedScenePath_.isEmpty())
            beginSceneLoad();
        return; // obsolete load; never apply its resources to new scene
    }
    if (!outcome.error.isEmpty() || !outcome.runtime)
    {
        sceneLoading_ = false;
        sceneLoadLabel_->hide();
        sceneLoadProgress_->hide();
        cameraBox_->setEnabled(bool(runtime_));
        if (explorationButton_)
            explorationButton_->setEnabled(bool(runtime_));
        canvas_->setPlaceholder(QStringLiteral("Chargement impossible : %1").arg(outcome.error));
        statusBar()->showMessage(QStringLiteral("Erreur de chargement : %1").arg(outcome.error));
        QMessageBox::warning(this, QStringLiteral("Chargement de la scène"), outcome.error);
        return;
    }
    const auto replacement = outcome.runtime;
    // Apply a complete, validated runtime in one GUI-thread transaction.
    renderSerial_->fetch_add(1);
    pending_ = false;
    runtime_ = replacement;
    lastImage_ = QImage{};
    canvas_->setImage(QImage{}); // Do not keep displaying an old scene.
    canvas_->enableSkyCompositing(false);
    renderTimeLabel_->setText(QStringLiteral("Dernier rendu : —"));
    materialLabel_->setText(QStringLiteral("Sélectionner un matériau dans le catalogue"));
    const QString sceneRevision = outcome.database.revision.trimmed();
    if (sceneRevisionLabel_)
        sceneRevisionLabel_->setText(sceneRevision.isEmpty() ? QStringLiteral("Scène —")
                                                             : QStringLiteral("Scène %1").arg(sceneRevision));
    const QString revisionSuffix =
        sceneRevision.isEmpty() ? QStringLiteral("Scène sans révision") : QStringLiteral("Scène %1").arg(sceneRevision);
    setWindowTitle(QStringLiteral("CSE WARP Beta X V%1 — %2 — %3")
                       .arg(CSE_WARP_PRODUCT_VERSION)
                       .arg(runtime_->title, revisionSuffix));
    sunHour_->blockSignals(true);
    sunHour_->setValue(double(runtime_->daylightHour));
    sunHour_->blockSignals(false);
    scenePath_ = outcome.path;
    refreshRadioForScene(scenePath_);
    setRasterGiCachePath(std::filesystem::path((scenePath_ + QStringLiteral(".m3dgi")).toStdWString()));
    if (bakeGiButton_)
        bakeGiButton_->setEnabled(true);
    materialsPath_.clear();
    for (const auto &sector : outcome.database.sectors)
    {
        const auto entries = sector.resources.value(QStringLiteral("materials"));
        if (!entries.isEmpty())
        {
            materialsPath_ = QDir(QFileInfo(outcome.path).absolutePath()).absoluteFilePath(entries.front().path);
            break;
        }
    }
    selectedMaterial_ = -1;
    cameraBox_->blockSignals(true);
    cameraBox_->clear();
    const QVector<QString> preferredCameraIds{QStringLiteral("v4_place"),
                                              QStringLiteral("v4_aerienne"),
                                              QStringLiteral("v6_monument_contrechamp"),
                                              QStringLiteral("v7_pool_fresco"),
                                              QStringLiteral("v4_hall"),
                                              QStringLiteral("v523_cupola_exterieure"),
                                              QStringLiteral("v523_cupola_interieure"),
                                              QStringLiteral("v523_cupola_arriere"),
                                              QStringLiteral("v600_garden_view")};
    QVector<int> visibleCameras;
    for (const QString &id : preferredCameraIds)
    {
        for (int i = 0; i < runtime_->cameras.size(); ++i)
        {
            if (runtime_->cameras[i].id == id)
            {
                visibleCameras.append(i);
                break;
            }
        }
    }
    if (visibleCameras.isEmpty())
    {
        for (int i = 0; i < std::min(9, int(runtime_->cameras.size())); ++i)
            visibleCameras.append(i);
    }
    for (int i : visibleCameras)
    {
        QString name = runtime_->cameras[i].name;
        name.remove(QRegularExpression(QStringLiteral("^V\\d+(?:\\.\\d+)?\\s*[·—-]\\s*"),
                                       QRegularExpression::CaseInsensitiveOption));
        cameraBox_->addItem(name, i);
    }
    for (int i = 0; i < quickCameraActions_.size(); ++i)
    {
        const bool valid = i < visibleCameras.size();
        quickCameraActions_[i]->setEnabled(valid);
        // Full camera names remain in the side selector and tooltip;
        // the toolbar uses compact numbers so DX11 / Vulkan stay visible.
        quickCameraActions_[i]->setText(QString::number(i + 1));
        quickCameraActions_[i]->setToolTip(
            valid ? QStringLiteral("Vue %1 — touche %2").arg(cameraBox_->itemText(i)).arg(i + 1) : QString());
    }
    cameraBox_->blockSignals(false);
    updateCameraHud();
    populateTrees(outcome.database);
    statusBar()->showMessage(QStringLiteral("%1 — %2 objets, %3 triangles, %4 matériaux, PBR %5 px")
                                 .arg(runtime_->title)
                                 .arg(runtime_->objects.size())
                                 .arg(runtime_->renderScene.obj.size())
                                 .arg(runtime_->renderScene.mat.size())
                                 .arg(runtime_->textureResolution));
    // The stress scene must start with the actual V17 DX11 Raster backend
    // instead of CPU ray tracing, which scales poorly with 500k triangles.
    const bool isStressScene =
        QFileInfo(outcome.path).absolutePath().endsWith(QStringLiteral("/benchmark-civic"), Qt::CaseInsensitive);
    const bool extremeStress = QFileInfo(outcome.path).fileName() == QStringLiteral("scene-extreme.json");
    // Explicit benchmark launcher override only: ordinary scene launches
    // keep their existing DX11 Raster default and user controls.
    const bool vulkanProfile = isStressScene && qEnvironmentVariable("CSE_WARP_VK_PROFILE_MODE") == QStringLiteral("1");
#if defined(_WIN32) && M3D_V17_VULKAN_ENABLED
    if (vulkanProfile)
    {
        directDisplayBox_->setChecked(true);
        backendBox_->setCurrentIndex(5);     // Vulkan Raster, native WSI
        dayNightCycleBox_->setChecked(true); // exercise solar/shadow updates
    }
    else
#endif
        if (isStressScene)
        backendBox_->setCurrentIndex(4);
    sceneLoading_ = false;
    selectBackendOutputWindow();
    canvas_->setPlaceholder(
        extremeStress ? QStringLiteral("Scène extrême chargée. Cliquez sur Calculer l'image pour lancer le rendu.")
                      : QStringLiteral("Scène chargée. Calcul de la première image…"));
    sceneLoadLabel_->hide();
    sceneLoadProgress_->hide();
    cameraBox_->setEnabled(true);
    if (explorationButton_)
        explorationButton_->setEnabled(true);
    if (!extremeStress)
        requestRender();
    else
        statusBar()->showMessage(
            QStringLiteral("Profil extrême : rendu manuel conseillé pour limiter la consommation mémoire."));
}

void V17Window::populateTrees(const v16::SceneDatabase &database)
{
    sceneTree_->clear();
    resourceTree_->clear();
    QHash<QString, QTreeWidgetItem *> nodes;
    auto *root = new QTreeWidgetItem(sceneTree_, {database.name, QStringLiteral("V16")});
    nodes.insert(QString(), root);
    for (const auto &object : runtime_->objects)
    {
        QString walk;
        QTreeWidgetItem *parent = root;
        for (const auto &part : object.parent.split('/', Qt::SkipEmptyParts))
        {
            walk += QStringLiteral("/") + part;
            if (!nodes.contains(walk))
                nodes.insert(walk, new QTreeWidgetItem(parent, {part, QStringLiteral("Groupe")}));
            parent = nodes.value(walk);
        }
        auto *leaf = new QTreeWidgetItem(parent, {object.id, object.material});
        leaf->setData(0, Qt::UserRole, object.material);
    }
    for (const auto &sector : database.sectors)
    {
        for (auto it = sector.resources.constBegin(); it != sector.resources.constEnd(); ++it)
        {
            auto *category = new QTreeWidgetItem(resourceTree_, {it.key(), sector.id});
            for (const auto &resource : it.value())
                new QTreeWidgetItem(category, {QFileInfo(resource.path).fileName(), resource.path});
        }
    }
    auto *catalog =
        new QTreeWidgetItem(resourceTree_, {QStringLiteral("Catalogue des matériaux"), QStringLiteral("partagés")});
    for (auto it = runtime_->materialIndex.cbegin(); it != runtime_->materialIndex.cend(); ++it)
    {
        const auto &m = runtime_->renderScene.mat[size_t(it.value())];
        auto *leaf = new QTreeWidgetItem(catalog, {it.key(), QStringLiteral("PBR partagé")});
        leaf->setData(0, Qt::UserRole, it.key());
        auto addChannel = [&](const QString &label, size_t count, int size) {
            auto *channel =
                new QTreeWidgetItem(leaf, {label, QStringLiteral("%1² → 1² / %2 mipmaps").arg(size).arg(count)});
            channel->setData(0, Qt::UserRole, it.key());
        };
        if (!m.colorMip.empty())
            addChannel(QStringLiteral("Albedo — sRGB"), m.colorMip.size(), int(m.colorMip.front().size));
        if (!m.normalMip.empty())
            addChannel(QStringLiteral("Normal — linéaire"), m.normalMip.size(), int(m.normalMip.front().size));
        if (!m.roughnessMip.empty())
            addChannel(QStringLiteral("Rugosité — linéaire"), m.roughnessMip.size(), int(m.roughnessMip.front().size));
        new QTreeWidgetItem(leaf, {QStringLiteral("Métallicité"), QString::number(m.metallic, 'f', 2)});
        new QTreeWidgetItem(leaf, {QStringLiteral("Transmission"), QString::number(m.transmissionPercent)});
    }
    sceneTree_->expandToDepth(2);
    resourceTree_->expandToDepth(0);
    catalog->setExpanded(true);
}
