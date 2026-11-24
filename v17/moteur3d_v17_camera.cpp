#include "moteur3d_v17_window.h"

using namespace m3d;

int V17Window::selectedSceneCameraIndex() const
{
    if (!runtime_ || runtime_->cameras.isEmpty() || !cameraBox_)
        return 0;
    const int source = cameraBox_->currentData().toInt();
    return std::clamp(source, 0, int(runtime_->cameras.size()) - 1);
}

v16::CameraPreset V17Window::activeCameraPose() const
{
    if (freeCameraActive_)
        return freeCamera_;
    v16::CameraPreset pose;
    if (!runtime_ || runtime_->cameras.isEmpty())
        return pose;
    const int idx = selectedSceneCameraIndex();
    pose = runtime_->cameras[idx];
    const V dir = pose.eye - pose.target;
    const float angle = float(azimuth_->value()) * PI / 180.f;
    pose.eye = {pose.target.x + dir.x * std::cos(angle) + dir.z * std::sin(angle), pose.eye.y,
                pose.target.z - dir.x * std::sin(angle) + dir.z * std::cos(angle)};
    return pose;
}

void V17Window::updateCameraHud()
{
    if (!canvas_)
        return;
    if (!runtime_ || runtime_->cameras.isEmpty())
    {
        canvas_->setCameraHud({}, false);
        return;
    }
    const auto pose = activeCameraPose();
    if (nativeHudLabel_)
        nativeHudLabel_->setText(QStringLiteral("CAM X:%1 Y:%2 Z:%3")
                                     .arg(pose.eye.x, 0, 'f', 1)
                                     .arg(pose.eye.y, 0, 'f', 1)
                                     .arg(pose.eye.z, 0, 'f', 1));
    const QString label = QStringLiteral("CAM %1 | %2")
                              .arg(freeCameraActive_ ? QStringLiteral("Libre") : QStringLiteral("Préréglée"),
                                   pose.name.isEmpty() ? pose.id : pose.name);
    canvas_->setCameraHud(cse::camera_diagnostic::hudLines(pose, label), cameraHudEnabled_);
}

QString V17Window::gpuAdapterLabel()
{
#if defined(_WIN32)
    IDXGIFactory1 *factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
    {
        IDXGIAdapter1 *adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter)))
        {
            DXGI_ADAPTER_DESC1 d{};
            const HRESULT hr = adapter->GetDesc1(&d);
            adapter->Release();
            factory->Release();
            if (SUCCEEDED(hr))
                return QString::fromWCharArray(d.Description);
        }
        else
            factory->Release();
    }
#endif
    return QStringLiteral("Non détecté");
}

QString V17Window::githubRevision() const
{
    QFile f(QCoreApplication::applicationDirPath() + QStringLiteral("/REVISION.txt"));
    return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()).trimmed().left(12)
                                                         : QStringLiteral("développement local");
}

QJsonObject V17Window::sceneCameraReport(const v16::CameraPreset &pose, const QString &manifest, const QString &title,
                                         const QString &image, const QSize imageSize, const QString &revision)
{
    return {
        {"format", QString::fromLatin1(cse::camera_diagnostic::formatId)},
        {"createdAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"engine", QStringLiteral("CSE WARP Beta X")},
        {"version", QStringLiteral(CSE_WARP_PRODUCT_VERSION)},
        {"revision", revision},
        {"scene",
         QJsonObject{{"manifestFile", QFileInfo(manifest).fileName()}, {"title", title}, {"originalPath", manifest}}},
        {"camera", cse::camera_diagnostic::toJson(pose)},
        {"image", image},
        {"resolution", QJsonObject{{"width", imageSize.width()}, {"height", imageSize.height()}}},
        {"units", QStringLiteral("metres")},
        {"coordinateConvention",
         QStringLiteral("Y vertical; yaw 0 degrees points toward -Z; positive yaw rotates toward +X")}};
}

void V17Window::copyLiveCamera()
{
    if (!runtime_ || runtime_->cameras.isEmpty())
        return;
    const auto report =
        sceneCameraReport(activeCameraPose(), scenePath_, runtime_->title, QString(), QSize(), githubRevision());
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
    statusBar()->showMessage(QStringLiteral("Position caméra et identité de la scène copiées."), 5000);
}

void V17Window::captureAnomaly()
{
    if (!runtime_ || lastImage_.isNull() || !hasRenderedPose_)
    {
        QMessageBox::information(this, QStringLiteral("Capture d'anomalie"),
                                 QStringLiteral("Une image doit être rendue avant d'enregistrer l'anomalie."));
        return;
    }
    // During navigation the CPU-readback image may lag behind the live camera.
    // Always export the exact pose associated with the accepted rendered frame.
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString folder =
        QDir(documents.isEmpty() ? QDir::homePath() : documents).filePath(QStringLiteral("CSE WARP/Anomalies"));
    if (!QDir().mkpath(folder))
    {
        QMessageBox::warning(this, QStringLiteral("Capture d'anomalie"),
                             QStringLiteral("Dossier inaccessible : %1").arg(folder));
        return;
    }
    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
    const QString base = QDir(folder).filePath(QStringLiteral("ANOMALIE_%1").arg(stamp));
    const QString imageFile = base + QStringLiteral(".png");
    const QString jsonFile = base + QStringLiteral(".json");
    QImage annotated = lastImage_.copy();
    {
        QPainter painter(&annotated);
        ImageView::drawCameraHud(
            painter, annotated.rect(),
            cse::camera_diagnostic::hudLines(lastRenderedPose_,
                                             QStringLiteral("CAMÉRA DU RENDU / V%1").arg(CSE_WARP_PRODUCT_VERSION)));
    }
    if (!annotated.save(imageFile, "PNG"))
    {
        QMessageBox::warning(this, QStringLiteral("Capture d'anomalie"),
                             QStringLiteral("Échec de l'enregistrement PNG."));
        return;
    }
    QJsonObject report = sceneCameraReport(lastRenderedPose_, scenePath_, runtime_->title,
                                           QFileInfo(imageFile).fileName(), lastImage_.size(), githubRevision());
    report.insert("renderInProgressAtCapture", watcher_.isRunning());
    report.insert("liveCameraAtCapture", cse::camera_diagnostic::toJson(activeCameraPose()));
    report.insert("note", QStringLiteral(
                              "camera correspond précisément au PNG enregistré; liveCameraAtCapture est informative."));
    QSaveFile file(jsonFile);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit())
    {
        QFile::remove(imageFile);
        QMessageBox::warning(this, QStringLiteral("Capture d'anomalie"),
                             QStringLiteral("Échec de l'enregistrement JSON."));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Anomalie sauvegardée : %1").arg(base), 15000);
    QMessageBox::information(this, QStringLiteral("Anomalie enregistrée"),
                             QStringLiteral("Capture PNG et coordonnées exactes disponibles dans :\n%1").arg(folder));
}

void V17Window::restoreAnomalyCamera()
{
    if (!runtime_ || runtime_->cameras.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("Restaurer une caméra"),
                                 QStringLiteral("Charge d'abord la scène concernée."));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Restaurer une caméra d'anomalie"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/CSE WARP/Anomalies"),
        QStringLiteral("Rapport caméra CSE WARP (*.json)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, QStringLiteral("Restaurer une caméra"),
                             QStringLiteral("Impossible d'ouvrir le rapport."));
        return;
    }
    QJsonParseError parsing;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parsing);
    const auto root = doc.object();
    if (parsing.error != QJsonParseError::NoError ||
        root.value("format").toString() != QString::fromLatin1(cse::camera_diagnostic::formatId))
    {
        QMessageBox::warning(this, QStringLiteral("Restaurer une caméra"),
                             QStringLiteral("Format incompatible ou JSON invalide."));
        return;
    }
    const auto scene = root.value("scene").toObject();
    if (scene.value("manifestFile").toString() != QFileInfo(scenePath_).fileName() ||
        scene.value("title").toString() != runtime_->title)
    {
        QMessageBox::warning(this, QStringLiteral("Scène différente"),
                             QStringLiteral("La capture appartient à une autre scène. Charge d'abord cette scène "
                                            "pour éviter toute erreur architecturale."));
        return;
    }
    v16::CameraPreset pose;
    QString error;
    if (!cse::camera_diagnostic::fromJson(root.value("camera").toObject(), pose, error))
    {
        QMessageBox::warning(this, QStringLiteral("Restaurer une caméra"), error);
        return;
    }
    keysDown_.clear();
    // Preserve the exact screenshot pose without switching the
    // collision mode (which previously persisted "Vol libre" in QSettings).
    // On the next user movement, resynchronize feet with the real scene.
    freeCamera_ = pose;
    resetScopeForCamera();
    walkingCollisionSyncPending_ = true;
    const V dir = norm(pose.target - pose.eye);
    yaw_ = std::atan2(dir.x, -dir.z);
    pitch_ = std::asin(clampf(dir.y, -.99f, .99f));
    freeCameraActive_ = true;
    updateExplorationSpeedIndicator();
    updateCameraHud();
    requestRender();
    canvas_->setFocus();
    statusBar()->showMessage(QStringLiteral("Point de vue d'anomalie restauré depuis %1").arg(path), 12000);
}
