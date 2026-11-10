#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QToolBar>
#include <QProcess>
#include <QProgressBar>
#include <QDateTime>
#include <QStandardPaths>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QIcon>
#include <QLabel>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSet>
#include <QTimer>
#include <QThreadPool>
#include <QThread>
#include <QMainWindow>
#include <QMetaObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace m3d;
namespace {
struct Result {
    QImage image;
    QString renderer;
    QString error;
    double milliseconds=0;
    bool gpu=false;
    // Reject finished frames after a camera, material, backend or scene change.
    uint64_t generation=0;
    quintptr renderThread=0;
};
struct SceneLoadOutcome {
    QString path;
    QString error;
    v16::SceneDatabase database;
    std::shared_ptr<v16::Runtime> runtime;
    uint64_t generation=0;
};
class ImageView final : public QWidget {
public:
    void setImage(QImage im){image_=std::move(im);update();}
    void setPlaceholder(const QString& message){placeholder_=message;update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(),QColor(22,26,31));
        if(image_.isNull()) {
            p.setPen(QColor(180,191,204));
            p.drawText(rect().adjusted(30,20,-30,-20),Qt::AlignCenter|Qt::TextWordWrap,
                       placeholder_.isEmpty()?QStringLiteral("Ouvrir une scène V16 dans le menu Fichier")
                                             :placeholder_);
            return;
        }
        QSize s=image_.size();
        s.scale(size()-QSize(20,20),Qt::KeepAspectRatio);
        QRect dst(QPoint((width()-s.width())/2,(height()-s.height())/2),s);
        p.setRenderHint(QPainter::SmoothPixmapTransform,false);
        p.drawImage(dst,image_);
    }
private:
    QImage image_;
    QString placeholder_;
};
class V17Window final : public QMainWindow {
public:
    explicit V17Window(QString scenePath,int textureResolution=2048)
        :textureResolution_(textureResolution){
        setWindowTitle(QStringLiteral("CSE WARP Beta X — V17.%1 Exploration")
            .arg(CSE_WARP_BUILD_NUMBER));
        resize(1510,900);
        setMinimumSize(1100,670);
        canvas_=new ImageView;
        canvas_->setFocusPolicy(Qt::StrongFocus);
        canvas_->setMouseTracking(true);
        canvas_->installEventFilter(this);
        setCentralWidget(canvas_);
        frameTimer_.setInterval(16);
        connect(&frameTimer_, &QTimer::timeout, this, [this]{ frameTick(); });
        // The D3D11 preview owns a thread_local device and texture cache.
        // A single persistent QtConcurrent worker keeps GPU resources on
        // the SAME thread across all subsequent renders and camera changes.
        // Using Qt's global pool can build a new device for every worker.
        renderPool_.setMaxThreadCount(1);
        renderPool_.setExpiryTimeout(-1);
        createUi();
        // A permanent status-bar widget outlives temporary render/progress
        // messages. QStatusBar::showMessage(..., 15000) used to erase the
        // completed render time after 15 seconds.
        renderTimeLabel_=new QLabel(QStringLiteral("Dernier rendu : —"),this);
        renderTimeLabel_->setObjectName(QStringLiteral("v16LastRenderTime"));
        statusBar()->addPermanentWidget(renderTimeLabel_);
        sceneLoadLabel_=new QLabel(this);
        sceneLoadLabel_->setText(QStringLiteral("Chargement de la scène…"));
        sceneLoadProgress_=new QProgressBar(this);
        sceneLoadProgress_->setRange(0,100);
        sceneLoadProgress_->setValue(0);
        sceneLoadProgress_->setMaximumWidth(255);
        sceneLoadProgress_->setTextVisible(true);
        statusBar()->addWidget(sceneLoadLabel_,1);
        statusBar()->addWidget(sceneLoadProgress_);
        sceneLoadLabel_->hide();
        sceneLoadProgress_->hide();
        connect(&loadWatcher_,&QFutureWatcher<SceneLoadOutcome>::finished,this,
                [this]{
                    if(closingRequested_){
                        if(!loadWatcher_.isRunning())QTimer::singleShot(0,this,[this]{close();});
                        return;
                    }
                    finishSceneLoad();
                });
        connect(&watcher_,&QFutureWatcher<Result>::finished,this,[this]{
            // A stale finished notification can arrive after setFuture()
            // already started another render. result() would block the GUI
            // waiting for that new render: never call it while still running.
            if(watcher_.isRunning())return;
            if(closingRequested_){
                QTimer::singleShot(0,this,[this]{close();});
                return;
            }
            const auto outcome=watcher_.result();
            if(sceneLoading_)return;
            // A previous scene may finish after the user has opened a new one.
            // Never show its image or overwrite the current status bar.
            if(outcome.generation!=renderSerial_->load()){
                if(pending_){pending_=false;requestRender();}
                return;
            }
            if(!outcome.image.isNull()){lastImage_=outcome.image;canvas_->setImage(outcome.image);}
            if(outcome.error.isEmpty()&&!outcome.image.isNull()){
                const QString milliseconds=QString::number(outcome.milliseconds,'f',1);
                const QString seconds=QString::number(outcome.milliseconds/1000.0,'f',1);
                renderTimeLabel_->setText(QStringLiteral("Dernier rendu : %1 ms (%2 s)")
                                              .arg(milliseconds,seconds));
                renderTimeLabel_->setToolTip(
                    QStringLiteral("%1\nTemps de calcul : %2 ms (%3 s). "
                                   "Cette valeur exclut certains traitements de post-production.")
                        .arg(outcome.renderer,milliseconds,seconds));
            }
            // The completion message itself also remains until another action.
            // Errors leave the last successful render time visible.
            statusBar()->showMessage(outcome.error.isEmpty()
                ?QStringLiteral("%1 — %2 ms")
                      .arg(outcome.renderer).arg(outcome.milliseconds,0,'f',1)
                :outcome.error);
            if(exploring_ && outcome.error.isEmpty() && !outcome.image.isNull()){
                if(frameClock_.isValid()){
                    const qint64 frameMs=frameClock_.elapsed();
                    if(frameMs>0)renderTimeLabel_->setText(
                        QStringLiteral("V17 : %1 ms · %2 FPS calculés")
                            .arg(frameMs).arg(1000.0/double(frameMs),0,'f',1));
                }
            } else if(exploring_ && !outcome.error.isEmpty()){
                stopExploration();
            }
            if(pending_){pending_=false;requestRender();}
        });
        // Return from the constructor immediately. The first scene is built
        // on a worker after show(), so Windows always shows the main window.
        canvas_->setPlaceholder(QStringLiteral("CSE WARP V17\nChargement de la scène…"));
        if(!scenePath.isEmpty())
            QTimer::singleShot(0,this,[this,scenePath]{openScene(scenePath);});
    }
    // GUI smoke: first scene must load asynchronously before we request
    // another scene; the replacement must render a new (non-stale) image.
    void smokeSceneSwitch(const QString& secondScene){
        auto* poll=new QTimer(this);
        auto phase=std::make_shared<int>(0);
        poll->setInterval(100);
        connect(poll,&QTimer::timeout,this,[this,secondScene,poll,phase]{
            if(*phase==0){
                if(sceneLoading_ || !runtime_)return;
                *phase=1;
                openScene(secondScene);
                return;
            }
            if(sceneLoading_||!runtime_||watcher_.isRunning()||pending_
               ||lastImage_.isNull())return;
            if(QFileInfo(scenePath_).absoluteFilePath()
                !=QFileInfo(secondScene).absoluteFilePath()){
                std::fprintf(stderr,"V17 SCENE SWITCH: wrong scene loaded\\n");
                QCoreApplication::exit(22);
                return;
            }
            std::fprintf(stderr,"V17 SCENE SWITCH: new scene rendered %dx%d\\n",
                lastImage_.width(),lastImage_.height());
            poll->stop();
            QCoreApplication::exit(0);
        });
        poll->start();
        QTimer::singleShot(120000,this,[]{
            std::fprintf(stderr,"V17 SCENE SWITCH: timed out\\n");
            QCoreApplication::exit(23);
        });
    }
    // Dedicated GPU regression: exercise two independent button-equivalent
    // render requests on the same scene and assert worker/cache affinity.
    // The first render starts normally after asynchronous scene loading.
    void smokeRepeatRaster(){
        backendBox_->setCurrentIndex(4);
        auto count=std::make_shared<int>(0);
        auto firstThread=std::make_shared<quintptr>(0);
        connect(&watcher_,&QFutureWatcher<Result>::finished,this,
            [this,count,firstThread]{
                if(watcher_.isRunning()||sceneLoading_||!runtime_)return;
                const Result result=watcher_.result();
                if(result.generation!=renderSerial_->load())return;
                if(!result.gpu||result.image.isNull()||!result.error.isEmpty()){
                    std::fprintf(stderr,"V17 REPEAT RASTER: failed render %d: %s\n",
                        *count+1,result.error.toUtf8().constData());
                    QCoreApplication::exit(31);
                    return;
                }
                if(*count==0){
                    *firstThread=result.renderThread;
                    std::fprintf(stderr,
                        "V17 REPEAT RASTER: first GPU frame %.2f ms, worker %llu\n",
                        result.milliseconds,
                        static_cast<unsigned long long>(result.renderThread));
                    ++*count;
                    QTimer::singleShot(0,this,[this]{requestRender();});
                    return;
                }
                if(result.renderThread!=*firstThread){
                    std::fprintf(stderr,"V17 REPEAT RASTER: GPU worker changed!\n");
                    QCoreApplication::exit(32);
                    return;
                }
                std::fprintf(stderr,
                    "V17 REPEAT RASTER: second GPU frame %.2f ms, same worker %llu\n",
                    result.milliseconds,
                    static_cast<unsigned long long>(result.renderThread));
                QCoreApplication::exit(0);
            });
        QTimer::singleShot(120000,this,[]{
            std::fprintf(stderr,"V17 REPEAT RASTER: timed out\n");
            QCoreApplication::exit(33);
        });
    }
    ~V17Window() override {
        renderSerial_->fetch_add(1);
        // All async tasks hold independent snapshots. Wait before QObject
        // destruction so no worker can post into the destroyed window.
        loadWatcher_.waitForFinished();
        watcher_.waitForFinished();
    }
private:
    int textureResolution_=2048;
    ImageView* canvas_=nullptr;
    QTimer frameTimer_;
    QElapsedTimer frameClock_;
    QElapsedTimer motionClock_;
    QSet<int> keysDown_;
    QPoint previousMouse_;
    v16::CameraPreset freeCamera_;
    bool exploring_=false,freeCameraActive_=false,leftMouseHeld_=false;
    float yaw_=0.f,pitch_=0.f;
    QPushButton* explorationButton_=nullptr;
    QLabel* explorationHelp_=nullptr;
    void activateFreeCamera(){
        if(!runtime_||runtime_->cameras.isEmpty())return;
        const int idx=std::clamp(cameraBox_->currentIndex(),0,int(runtime_->cameras.size())-1);
        freeCamera_=runtime_->cameras[idx];
        const V dir=norm(freeCamera_.target-freeCamera_.eye);
        yaw_=std::atan2(dir.x,-dir.z);
        pitch_=std::asin(clampf(dir.y,-.99f,.99f));
        freeCameraActive_=true;
    }
    void updateLook(){
        const V forward{std::sin(yaw_)*std::cos(pitch_),std::sin(pitch_),
                        -std::cos(yaw_)*std::cos(pitch_)};
        freeCamera_.target=freeCamera_.eye+forward;
    }
    void stopExploration(){
        exploring_=false;
        frameTimer_.stop();
        keysDown_.clear();
        leftMouseHeld_=false;
        if(explorationButton_)explorationButton_->setText(QStringLiteral("Démarrer exploration (V17)"));
        if(explorationHelp_)explorationHelp_->setText(QStringLiteral(
            "ZQSD / flèches : mouvement · clic gauche + souris : regarder · Maj : accélérer · Échap : arrêter"));
    }
    void toggleExploration(){
        if(exploring_){stopExploration();return;}
        if(!runtime_||runtime_->cameras.isEmpty())return;
        if(backendBox_->currentIndex()!=4)backendBox_->setCurrentIndex(4);
        activateFreeCamera();
        exploring_=true;
        motionClock_.start();
        frameClock_.invalidate();
        if(explorationButton_)explorationButton_->setText(QStringLiteral("Arrêter exploration"));
        frameTimer_.start();
        canvas_->setFocus();
    }
    void frameTick(){
        if(!exploring_||!runtime_||!freeCameraActive_)return;
        const float dt=std::min(0.08f,float(motionClock_.restart())/1000.f);
        V forward{std::sin(yaw_)*std::cos(pitch_),std::sin(pitch_),
                  -std::cos(yaw_)*std::cos(pitch_)};
        V horizontal=norm(V{forward.x,0,forward.z});
        V right=norm(cross(horizontal,V{0,1,0}));
        V move{};
        if(keysDown_.contains(Qt::Key_Z)||keysDown_.contains(Qt::Key_W)||keysDown_.contains(Qt::Key_Up))move=move+horizontal;
        if(keysDown_.contains(Qt::Key_S)||keysDown_.contains(Qt::Key_Down))move=move-horizontal;
        if(keysDown_.contains(Qt::Key_D)||keysDown_.contains(Qt::Key_Right))move=move+right;
        if(keysDown_.contains(Qt::Key_Q)||keysDown_.contains(Qt::Key_A)||keysDown_.contains(Qt::Key_Left))move=move-right;
        if(keysDown_.contains(Qt::Key_Space))move.y+=1.f;
        if(keysDown_.contains(Qt::Key_Control))move.y-=1.f;
        if(length(move)>.01f){
            freeCamera_.eye=freeCamera_.eye+norm(move)*(dt*(keysDown_.contains(Qt::Key_Shift)?7.f:2.5f));
            updateLook();
        }
        // Avoid queued stale frames. CPU draw/readback is still a V17-1
        // limitation and cannot yet guarantee the 25 ms target.
        if(!watcher_.isRunning()){frameClock_.start();requestRender();}
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        // A synchronous D3D11 Map/readback cannot be forcibly cancelled.
        // Keep the UI visible while a worker is winding down instead of
        // leaving a hidden "zombie" process in Task Manager.
        if(watcher_.isRunning()||loadWatcher_.isRunning()){
            closingRequested_=true;
            stopExploration();
            renderSerial_->fetch_add(1);
            statusBar()->showMessage(QStringLiteral(
                "Fermeture demandée : fin du calcul en cours… "
                "Si le pilote GPU est bloqué, utilisez le Gestionnaire des tâches."));
            event->ignore();
            return;
        }
        QMainWindow::closeEvent(event);
    }
    bool eventFilter(QObject* watched,QEvent* event) override{
        if(watched==canvas_ && exploring_){
            if(event->type()==QEvent::MouseButtonPress){
                auto* e=static_cast<QMouseEvent*>(event);
                if(e->button()==Qt::LeftButton){
                    leftMouseHeld_=true;previousMouse_=e->pos();canvas_->setFocus();return true;
                }
            }
            if(event->type()==QEvent::MouseButtonRelease){
                auto* e=static_cast<QMouseEvent*>(event);
                if(e->button()==Qt::LeftButton){leftMouseHeld_=false;return true;}
            }
            if(event->type()==QEvent::MouseMove && leftMouseHeld_){
                auto* e=static_cast<QMouseEvent*>(event);
                const QPoint d=e->pos()-previousMouse_;previousMouse_=e->pos();
                yaw_+=float(d.x())*.005f;
                pitch_=clampf(pitch_-float(d.y())*.005f,-1.48f,1.48f);
                updateLook();return true;
            }
        }
        return QMainWindow::eventFilter(watched,event);
    }
    void keyPressEvent(QKeyEvent* e) override{
        if(exploring_){
            if(e->key()==Qt::Key_Escape){stopExploration();e->accept();return;}
            if(!e->isAutoRepeat()){keysDown_.insert(e->key());}
            e->accept();return;
        }
        QMainWindow::keyPressEvent(e);
    }
    void keyReleaseEvent(QKeyEvent* e) override{
        if(exploring_){if(!e->isAutoRepeat())keysDown_.remove(e->key());e->accept();return;}
        QMainWindow::keyReleaseEvent(e);
    }
private:
    QTreeWidget* sceneTree_=nullptr;
    QTreeWidget* resourceTree_=nullptr;
    QComboBox* cameraBox_=nullptr;
    QVector<QAction*> quickCameraActions_;
    QComboBox* backendBox_=nullptr;
    QComboBox* qualityBox_=nullptr;
    QComboBox* giSamples_=nullptr;
    QComboBox* directSamples_=nullptr;
    QCheckBox* progressiveGi_=nullptr;
    QCheckBox* denoiseGi_=nullptr;
    QCheckBox* denoiseDirect_=nullptr;
    QCheckBox* edgeAa_=nullptr;
    QSlider* azimuth_=nullptr;
    QDoubleSpinBox* sunHour_=nullptr;
    QDoubleSpinBox* roughness_=nullptr;
    QDoubleSpinBox* metallic_=nullptr;
    QLabel* materialLabel_=nullptr;
    QLabel* renderTimeLabel_=nullptr;
    QFutureWatcher<Result> watcher_;
    QThreadPool renderPool_;
    QFutureWatcher<SceneLoadOutcome> loadWatcher_;
    QLabel* sceneLoadLabel_=nullptr;
    QProgressBar* sceneLoadProgress_=nullptr;
    bool sceneLoading_=false;
    uint64_t sceneLoadGeneration_=0;
    QString queuedScenePath_;
    std::shared_ptr<std::atomic<uint64_t>> renderSerial_=std::make_shared<std::atomic<uint64_t>>(0);
    std::shared_ptr<v16::Runtime> runtime_;
    // The sun and scene geometry do not change when navigating cameras.
    // Reuse an identical shadow map across CPU renders of the same runtime.
    struct CpuShadowCache {
        std::mutex mutex;
        std::weak_ptr<v16::Runtime> scene;
        std::shared_ptr<const ShadowMap> shadow;
        float hour=-1.f;
    };
    std::shared_ptr<CpuShadowCache> cpuShadows_=std::make_shared<CpuShadowCache>();
    bool pending_=false, updatingControls_=false, closingRequested_=false;
    int selectedMaterial_=-1;
    QString scenePath_;
    QString materialsPath_;

    void createUi(){
        auto* file=menuBar()->addMenu(QStringLiteral("Fichier"));
        auto* open=file->addAction(QStringLiteral("Ouvrir une scène V16 (scene.json)…"));
        connect(open,&QAction::triggered,this,[this]{
            const QString chosen=QFileDialog::getOpenFileName(
                this,QStringLiteral("Scène V16"),QFileInfo(scenePath_).absolutePath(),
                QStringLiteral("Manifeste V16 (scene.json);;JSON (*.json)"));
            if(!chosen.isEmpty())openScene(chosen);
        });
        auto* benchmarkScenes=file->addMenu(QStringLiteral("Scènes benchmark architecturales"));
        for(const auto& profile:QVector<QPair<QString,QString>>{
            {QStringLiteral("Léger — 20 000 triangles"),QStringLiteral("scene.json")},
            {QStringLiteral("Lourd — 150 000 triangles"),QStringLiteral("scene-lourd.json")},
            {QStringLiteral("Extrême — 500 000+ triangles"),QStringLiteral("scene-extreme.json")}}){
            auto* action=benchmarkScenes->addAction(profile.first);
            const QString sceneFile=profile.second;
            connect(action,&QAction::triggered,this,[this,sceneFile]{
                const QString target=QCoreApplication::applicationDirPath()
                    +QStringLiteral("/benchmark-civic/")+sceneFile;
                if(!QFileInfo::exists(target)){
                    QMessageBox::warning(this,QStringLiteral("Scène benchmark manquante"),
                        QStringLiteral("La scène n'est pas installée : %1").arg(target));
                    return;
                }
                if(sceneFile==QStringLiteral("scene-extreme.json")&&
                    QMessageBox::question(this,QStringLiteral("Profil extrême"),
                        QStringLiteral("Ce niveau contient au moins 500 000 triangles. "
                                       "Il utilise beaucoup de RAM et peut être lent sur Intel HD 620. "
                                       "Charger la scène sans lancer automatiquement son rendu ?"))
                          !=QMessageBox::Yes)return;
                openScene(target);
            });
        }
        auto* saveMaterialsAction=file->addAction(QStringLiteral("Enregistrer les matériaux PBR…"));
        connect(saveMaterialsAction,&QAction::triggered,this,[this]{saveMaterials();});
        auto* save=file->addAction(QStringLiteral("Exporter l'image PNG…"));
        connect(save,&QAction::triggered,this,[this]{
            if(lastImage_.isNull()){QMessageBox::information(this,"PNG","Aucune image calculée.");return;}
            const QString path=QFileDialog::getSaveFileName(this,"Exporter PNG","pavillon01-v16.png","PNG (*.png)");
            if(!path.isEmpty()&&!lastImage_.save(path,"PNG"))
                QMessageBox::warning(this,"PNG","Échec de l'enregistrement.");
        });
        auto* quit=file->addAction(QStringLiteral("Quitter"));
        connect(quit,&QAction::triggered,this,&QWidget::close);

        // One-click camera presets are independent from the detailed settings panel.
        auto* camerasMenu=menuBar()->addMenu(QStringLiteral("Caméras"));
        auto* camerasToolbar=addToolBar(QStringLiteral("Caméras · 1 à 6"));
        camerasToolbar->setMovable(false);
        camerasToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
        for(int i=0;i<6;++i){
            auto* action=new QAction(QString::number(i+1),this);
            action->setShortcut(QKeySequence(QString::number(i+1)));
            action->setShortcutContext(Qt::ApplicationShortcut);
            action->setEnabled(false);
            const int index=i;
            connect(action,&QAction::triggered,this,[this,index]{
                if(!runtime_||index>=runtime_->cameras.size())return;
                const bool changed=cameraBox_->currentIndex()!=index;
                cameraBox_->setCurrentIndex(index);
                azimuth_->setValue(0);
                if(!changed)requestRender();
            });
            quickCameraActions_.append(action);
            camerasToolbar->addAction(action);
            camerasMenu->addAction(action);
        }
        auto* toolsMenu=menuBar()->addMenu(QStringLiteral("Outils"));
        auto* benchmarkAction=toolsMenu->addAction(
            QStringLiteral("Benchmark V17.%1 — performances…").arg(CSE_WARP_BUILD_NUMBER));
        connect(benchmarkAction,&QAction::triggered,this,[this]{
            if(scenePath_.isEmpty()){
                QMessageBox::information(this,QStringLiteral("Benchmark"),QStringLiteral("Charger une scène avant de lancer le benchmark."));
                return;
            }
            const QString path=QCoreApplication::applicationDirPath()
                +QStringLiteral("/moteur3d_v17_benchui.exe");
            if(!QFileInfo::exists(path)){
                QMessageBox::warning(this,QStringLiteral("Benchmark indisponible"),
                                     QStringLiteral("Programme de benchmark introuvable : %1").arg(path));
                return;
            }
            if(!QProcess::startDetached(path,{scenePath_},QCoreApplication::applicationDirPath()))
                QMessageBox::critical(this,QStringLiteral("Benchmark"),QStringLiteral("Impossible d'ouvrir la fenêtre de benchmark."));
        });
        auto* helpMenu=menuBar()->addMenu(QStringLiteral("Aide"));
        auto* aboutAction=helpMenu->addAction(
            QStringLiteral("À propos de CSE WARP V17.%1…").arg(CSE_WARP_BUILD_NUMBER));
        connect(aboutAction,&QAction::triggered,this,[this]{
            const QString revisionPath=QCoreApplication::applicationDirPath()
                +QStringLiteral("/REVISION.txt");
            QString revision=QStringLiteral("développement");
            QFile revisionFile(revisionPath);
            if(revisionFile.open(QIODevice::ReadOnly|QIODevice::Text))
                revision=QString::fromUtf8(revisionFile.readAll()).trimmed().left(12);
            QMessageBox::about(this,
                QStringLiteral("À propos de CSE WARP V17.%1").arg(CSE_WARP_BUILD_NUMBER),
                QStringLiteral("<h3>CSE WARP Beta X — V17.%1</h3>"
                    "<p><b>Version :</b> 17.%1<br/><b>Build GitHub :</b> %1<br/>"
                    "<b>Révision GitHub :</b> %2<br/><b>Qt :</b> %3</p>"
                    "<p><b>Rendu :</b> Direct3D 11 Raster, GPU Compute HLSL, "
                    "Ray Tracing CPU et illumination globale CPU.</p>"
                    "<p><b>Nouveautés :</b> thread GPU permanent, chargement asynchrone, "
                    "frustum culling de lots, correction du filtrage PBR/mipmaps, "
                    "benchmark CSV/JSON avec triangles soumis et trois profils de charge.</p>"
                    "<p><b>En développement :</b> instancing natif, LOD et éclairage PBR "
                    "amélioré. Objectifs de FPS non garantis.</p>")
                    .arg(QString::number(CSE_WARP_BUILD_NUMBER),
                         revision.toHtmlEscaped(),
                         QString::fromLatin1(qVersion()).toHtmlEscaped()));
        });

        auto* left=new QDockWidget(QStringLiteral("Explorateur de scène"),this);
        auto* tabs=new QTabWidget;
        sceneTree_=new QTreeWidget;
        sceneTree_->setHeaderLabels({QStringLiteral("Hiérarchie"),QStringLiteral("Matériau")});
        resourceTree_=new QTreeWidget;
        resourceTree_->setHeaderLabels({QStringLiteral("Ressource"),QStringLiteral("Origine")});
        tabs->addTab(sceneTree_,QStringLiteral("Objets"));
        tabs->addTab(resourceTree_,QStringLiteral("Ressources"));
        left->setWidget(tabs);
        addDockWidget(Qt::LeftDockWidgetArea,left);
        left->resize(360,720);

        auto* right=new QDockWidget(QStringLiteral("Rendu et matériaux"),this);
        auto* controls=new QWidget;
        auto* vertical=new QVBoxLayout(controls);
        auto* form=new QFormLayout;
        cameraBox_=new QComboBox;
        backendBox_=new QComboBox;
        backendBox_->addItems({
            QStringLiteral("CPU Ray Tracing PBR — direct"),
            QStringLiteral("GPU D3D11 Compute — rapide, ombres + verre"),
            QStringLiteral("CPU Éclairage global — 2 rebonds (qualité réglable)"),
            QStringLiteral("CPU Éclairage global — 4 rebonds (qualité réglable)"),
            QStringLiteral("V17 GPU D3D11 Raster — exploration rapide")
        });
#ifndef _WIN32
        backendBox_->setItemData(1,0,Qt::UserRole-1);
        backendBox_->setItemData(4,0,Qt::UserRole-1);
#endif
        qualityBox_=new QComboBox;
        // Keep native and Full HD side by side. Store explicit width/height
        // instead of deriving them from an index or assuming exact 16:9.
        qualityBox_->addItem(QStringLiteral("Dell natif 1366 × 768"),QSize(1366,768));
        qualityBox_->addItem(QStringLiteral("Full HD 1920 × 1080"),QSize(1920,1080));
        qualityBox_->addItem(QStringLiteral("QHD 2560 × 1440"),QSize(2560,1440));
        qualityBox_->addItem(QStringLiteral("4K UHD 3840 × 2160"),QSize(3840,2160));
        qualityBox_->setCurrentIndex(0); // Native Dell resolution for GPU comparison.
        directSamples_=new QComboBox;
        for(int n:{1,4,16})
            directSamples_->addItem(QStringLiteral("%1 échantillon(s) / pixel").arg(n),n);
        directSamples_->setCurrentIndex(0);
        giSamples_=new QComboBox;
        for(int n:{4,8,16,32,64})
            giSamples_->addItem(QStringLiteral("%1 échantillons / pixel").arg(n),n);
        giSamples_->setCurrentIndex(2); // Native Full HD quality baseline: 16 spp.
        giSamples_->setEnabled(false);
        progressiveGi_=new QCheckBox(QStringLiteral("GI progressive (afficher les passes)"));
        progressiveGi_->setChecked(true);
        progressiveGi_->setEnabled(false);
        denoiseGi_=new QCheckBox(QStringLiteral("Débruitage GI bilatéral (optionnel)"));
        denoiseGi_->setChecked(true);
        denoiseGi_->setEnabled(false);
        denoiseDirect_=new QCheckBox(QStringLiteral("Débruitage CPU direct (bilatéral)"));
        denoiseDirect_->setChecked(false); // avoid blurring PBR details by default
        sunHour_=new QDoubleSpinBox;
        sunHour_->setRange(7.0,19.0);
        sunHour_->setDecimals(2);
        sunHour_->setSingleStep(.25);
        sunHour_->setSuffix(QStringLiteral(" h"));
        sunHour_->setValue(14.75);
        sunHour_->setToolTip(QStringLiteral("Direction et hauteur du soleil — lumière rasante vers 17 h"));
        azimuth_=new QSlider(Qt::Horizontal);
        azimuth_->setRange(-180,180);
        azimuth_->setValue(0);
        form->addRow(QStringLiteral("Caméra"),cameraBox_);
        form->addRow(QStringLiteral("Méthode de rendu"),backendBox_);
        form->addRow(QStringLiteral("Résolution"),qualityBox_);
        form->addRow(QStringLiteral("Échantillons CPU direct"),directSamples_);
        form->addRow(QStringLiteral("Échantillons GI"),giSamples_);
        form->addRow(QStringLiteral("Heure solaire"),sunHour_);
        form->addRow(QStringLiteral("Rotation caméra"),azimuth_);
        vertical->addLayout(form);
        vertical->addWidget(progressiveGi_);
        vertical->addWidget(denoiseGi_);
        vertical->addWidget(denoiseDirect_);
        edgeAa_=new QCheckBox(QStringLiteral("Antialiasing des contours"));
        edgeAa_->setChecked(true);
        vertical->addWidget(edgeAa_);
        explorationButton_=new QPushButton(QStringLiteral("Démarrer exploration (V17)"));
        vertical->addWidget(explorationButton_);
        explorationHelp_=new QLabel(QStringLiteral(
            "ZQSD / flèches : mouvement · clic gauche + souris : regarder · Maj : accélérer · Échap : arrêter"));
        explorationHelp_->setWordWrap(true);
        vertical->addWidget(explorationHelp_);
        connect(explorationButton_,&QPushButton::clicked,this,[this]{toggleExploration();});
        auto* renderButton=new QPushButton(QStringLiteral("Calculer l'image"));
        vertical->addWidget(renderButton);
        auto* materials=new QGroupBox(QStringLiteral("Matériau sélectionné"));
        auto* matForm=new QFormLayout(materials);
        materialLabel_=new QLabel(QStringLiteral("Sélectionner un matériau dans le catalogue"));
        materialLabel_->setWordWrap(true);
        roughness_=new QDoubleSpinBox;
        roughness_->setRange(.01,1.0);roughness_->setSingleStep(.05);roughness_->setDecimals(2);
        metallic_=new QDoubleSpinBox;
        metallic_->setRange(0,1);metallic_->setSingleStep(.05);metallic_->setDecimals(2);
        matForm->addRow(materialLabel_);
        matForm->addRow(QStringLiteral("Rugosité ×"),roughness_);
        matForm->addRow(QStringLiteral("Métallicité"),metallic_);
        vertical->addWidget(materials);
        vertical->addStretch(1);
        right->setWidget(controls);
        addDockWidget(Qt::RightDockWidgetArea,right);
        right->setMinimumWidth(295);

        connect(renderButton,&QPushButton::clicked,this,[this]{requestRender();});
        connect(cameraBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
            azimuth_->setValue(0);
            if(exploring_)activateFreeCamera();
            requestRender();
        });
        connect(backendBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){
            if(exploring_ && index!=4)stopExploration();
            const bool gi=index==2||index==3;
            directSamples_->setEnabled(index==0);
            denoiseDirect_->setEnabled(index==0);
            giSamples_->setEnabled(gi);
            progressiveGi_->setEnabled(gi);
            denoiseGi_->setEnabled(gi);
            requestRender();
        });
        connect(qualityBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){requestRender();});
        connect(directSamples_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){if(backendBox_->currentIndex()==0)requestRender();});
        connect(giSamples_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){if(backendBox_->currentIndex()==2||backendBox_->currentIndex()==3)requestRender();});
        connect(progressiveGi_,&QCheckBox::toggled,this,[this](bool){if(backendBox_->currentIndex()==2||backendBox_->currentIndex()==3)requestRender();});
        connect(denoiseGi_,&QCheckBox::toggled,this,[this](bool){if(backendBox_->currentIndex()==2||backendBox_->currentIndex()==3)requestRender();});
        connect(denoiseDirect_,&QCheckBox::toggled,this,[this](bool){if(backendBox_->currentIndex()==0)requestRender();});
        connect(edgeAa_,&QCheckBox::toggled,this,[this](bool){requestRender();});
        connect(azimuth_,&QSlider::sliderReleased,this,[this]{requestRender();});
        connect(sunHour_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double hour){
            if(!runtime_)return;
            // No render thread may observe a partially updated daylight hour.
            renderSerial_->fetch_add(1);
            if(watcher_.isRunning())watcher_.waitForFinished();
            runtime_->daylightHour=float(hour);
            requestRender();
        });
        connect(roughness_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double){modifyMaterial();});
        connect(metallic_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double){modifyMaterial();});
        connect(resourceTree_,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem* item,int){
            const QString key=item->data(0,Qt::UserRole).toString();
            if(runtime_&&runtime_->materialIndex.contains(key))chooseMaterial(runtime_->materialIndex.value(key),key);
        });
        connect(sceneTree_,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem* item,int){
            const QString key=item->data(0,Qt::UserRole).toString();
            if(runtime_&&runtime_->materialIndex.contains(key))chooseMaterial(runtime_->materialIndex.value(key),key);
        });
    }
    QImage lastImage_;
    void chooseMaterial(int id,const QString& name){
        if(!runtime_||id<0||id>=int(runtime_->renderScene.mat.size()))return;
        selectedMaterial_=id;updatingControls_=true;
        materialLabel_->setText(name);
        const auto& m=runtime_->renderScene.mat[size_t(id)];
        roughness_->setValue(m.roughnessScale);
        metallic_->setValue(m.metallic);
        updatingControls_=false;
    }
    void modifyMaterial(){
        if(updatingControls_||!runtime_||selectedMaterial_<0)return;
        // Only mutate scene resources once the previous render no longer reads them.
        renderSerial_->fetch_add(1);
        if(watcher_.isRunning())watcher_.waitForFinished();
        auto& m=runtime_->renderScene.mat[size_t(selectedMaterial_)];
        m.roughnessScale=float(roughness_->value());
        m.metallic=float(metallic_->value());
        // Required for GPU fast-path cache invalidation. The shadow map itself
        // depends only on geometry/sun, not on metallicity or roughness.
        runtime_->renderScene.markGpuContentChanged();
        requestRender();
    }
    void saveMaterials(){
        if(materialsPath_.isEmpty()||!runtime_){QMessageBox::warning(this,"Matériaux","Aucun catalogue chargé.");return;}
        if(watcher_.isRunning())watcher_.waitForFinished();
        QFile file(materialsPath_);
        if(!file.open(QIODevice::ReadOnly)){QMessageBox::critical(this,"Matériaux",file.errorString());return;}
        const auto document=QJsonDocument::fromJson(file.readAll());
        file.close();
        if(!document.isObject()){QMessageBox::critical(this,"Matériaux","Fichier JSON invalide.");return;}
        QJsonObject root=document.object();
        QJsonArray materials=root.value("materials").toArray();
        for(int i=0;i<materials.size();++i){
            auto j=materials[i].toObject();
            const QString id=j.value("id").toString();
            if(!runtime_->materialIndex.contains(id))continue;
            const auto& m=runtime_->renderScene.mat[size_t(runtime_->materialIndex.value(id))];
            j.insert("roughnessScale",double(m.roughnessScale));
            j.insert("metallic",double(m.metallic));
            materials[i]=j;
        }
        root.insert("materials",materials);
        QSaveFile output(materialsPath_);
        if(!output.open(QIODevice::WriteOnly)){
            QMessageBox::critical(this,"Matériaux",output.errorString());return;
        }
        const auto content=QJsonDocument(root).toJson(QJsonDocument::Indented);
        if(output.write(content)!=content.size()||!output.commit()){
            QMessageBox::critical(this,"Matériaux","Écriture du catalogue impossible.");return;
        }
        statusBar()->showMessage(QStringLiteral("Matériaux enregistrés dans %1").arg(materialsPath_),15000);
    }
    void openScene(const QString& path){
        if(path.isEmpty())return;
        if(exploring_)stopExploration();
        freeCameraActive_=false;
        // Invalidates previous scene/camera frame without waiting on its GPU.
        renderSerial_->fetch_add(1);
        pending_=false;
        queuedScenePath_=path;
        ++sceneLoadGeneration_;
        sceneLoading_=true;
        sceneLoadProgress_->setValue(0);
        sceneLoadLabel_->setText(QStringLiteral("Chargement de la scène…"));
        sceneLoadLabel_->show();
        sceneLoadProgress_->show();
        canvas_->setImage(QImage{});
        canvas_->setPlaceholder(QStringLiteral("CSE WARP V17\nChargement de la scène…"));
        cameraBox_->setEnabled(false);
        if(explorationButton_)explorationButton_->setEnabled(false);
        if(!loadWatcher_.isRunning())beginSceneLoad();
    }
    void beginSceneLoad(){
        if(queuedScenePath_.isEmpty())return;
        const QString path=std::exchange(queuedScenePath_,QString{});
        const uint64_t token=sceneLoadGeneration_;
        const int resolution=textureResolution_;
        sceneLoadProgress_->setValue(5);
        sceneLoadLabel_->setText(QStringLiteral("Lecture JSON et ressources…"));
        loadWatcher_.setFuture(QtConcurrent::run([this,path,token,resolution]{
            SceneLoadOutcome outcome;
            outcome.path=path;
            outcome.generation=token;
            const auto loaded=v16::loadScene(path);
            if(!loaded){outcome.error=loaded.error;return outcome;}
            outcome.database=loaded.scene;
            auto build=v16::buildRuntime(loaded.scene,resolution,
                [this,token](int percent,const QString& description){
                    QMetaObject::invokeMethod(this,[this,token,percent,description]{
                        if(!sceneLoading_||token!=sceneLoadGeneration_)return;
                        sceneLoadProgress_->setValue(percent);
                        sceneLoadLabel_->setText(description);
                        canvas_->setPlaceholder(
                            QStringLiteral("CSE WARP V17\nChargement de la scène…\n%1")
                                .arg(description));
                    },Qt::QueuedConnection);
                });
            if(!build){outcome.error=build.error;return outcome;}
            outcome.runtime=std::make_shared<v16::Runtime>(std::move(build.runtime));
            return outcome;
        }));
    }
    void finishSceneLoad(){
        if(loadWatcher_.isRunning())return; // an older completion was queued
        const SceneLoadOutcome outcome=loadWatcher_.result();
        if(outcome.generation!=sceneLoadGeneration_ || !queuedScenePath_.isEmpty()){
            if(!queuedScenePath_.isEmpty())beginSceneLoad();
            return; // obsolete load; never apply its resources to new scene
        }
        if(!outcome.error.isEmpty()||!outcome.runtime){
            sceneLoading_=false;
            sceneLoadLabel_->hide();
            sceneLoadProgress_->hide();
            cameraBox_->setEnabled(bool(runtime_));
            if(explorationButton_)explorationButton_->setEnabled(bool(runtime_));
            canvas_->setPlaceholder(QStringLiteral("Chargement impossible : %1").arg(outcome.error));
            statusBar()->showMessage(QStringLiteral("Erreur de chargement : %1").arg(outcome.error));
            QMessageBox::warning(this,QStringLiteral("Chargement de la scène"),outcome.error);
            return;
        }
        const auto replacement=outcome.runtime;
        // Apply a complete, validated runtime in one GUI-thread transaction.
        renderSerial_->fetch_add(1);
        pending_=false;
        runtime_=replacement;
        lastImage_=QImage{};
        canvas_->setImage(QImage{});  // Do not keep displaying an old scene.
        renderTimeLabel_->setText(QStringLiteral("Dernier rendu : —"));
        materialLabel_->setText(QStringLiteral("Sélectionner un matériau dans le catalogue"));
        setWindowTitle(QStringLiteral("CSE WARP Beta X V17.%1 — %2")
            .arg(CSE_WARP_BUILD_NUMBER).arg(runtime_->title));
        sunHour_->blockSignals(true);
        sunHour_->setValue(double(runtime_->daylightHour));
        sunHour_->blockSignals(false);
        scenePath_=outcome.path;
        materialsPath_.clear();
        for(const auto& sector:outcome.database.sectors){
            const auto entries=sector.resources.value(QStringLiteral("materials"));
            if(!entries.isEmpty()){
                materialsPath_=QDir(QFileInfo(outcome.path).absolutePath()).absoluteFilePath(entries.front().path);
                break;
            }
        }
        selectedMaterial_=-1;
        cameraBox_->blockSignals(true);
        cameraBox_->clear();
        for(const auto& c:runtime_->cameras)cameraBox_->addItem(c.name);
        for(int i=0;i<quickCameraActions_.size();++i){
            const bool valid=i<runtime_->cameras.size();
            quickCameraActions_[i]->setEnabled(valid);
            quickCameraActions_[i]->setText(valid ? QStringLiteral("%1 · %2").arg(i+1).arg(runtime_->cameras[i].name) : QString::number(i+1));
            quickCameraActions_[i]->setToolTip(valid ? QStringLiteral("Vue %1 — touche %2").arg(runtime_->cameras[i].name).arg(i+1) : QString());
        }
        cameraBox_->blockSignals(false);
        populateTrees(outcome.database);
        statusBar()->showMessage(QStringLiteral("%1 — %2 objets, %3 triangles, %4 matériaux, PBR %5 px")
            .arg(runtime_->title)
            .arg(runtime_->objects.size())
            .arg(runtime_->renderScene.obj.size())
            .arg(runtime_->renderScene.mat.size())
            .arg(runtime_->textureResolution));
        // The stress scene must start with the actual V17 DX11 Raster backend
        // instead of CPU ray tracing, which scales poorly with 500k triangles.
        const bool isStressScene=QFileInfo(outcome.path).absolutePath().endsWith(
            QStringLiteral("/benchmark-civic"),Qt::CaseInsensitive);
        const bool extremeStress=QFileInfo(outcome.path).fileName()
            ==QStringLiteral("scene-extreme.json");
        if(isStressScene)backendBox_->setCurrentIndex(4);
        sceneLoading_=false;
        canvas_->setPlaceholder(extremeStress
            ?QStringLiteral("Scène extrême chargée. Cliquez sur Calculer l'image pour lancer le rendu.")
            :QStringLiteral("Scène chargée. Calcul de la première image…"));
        sceneLoadLabel_->hide();
        sceneLoadProgress_->hide();
        cameraBox_->setEnabled(true);
        if(explorationButton_)explorationButton_->setEnabled(true);
        if(!extremeStress)requestRender();
        else statusBar()->showMessage(QStringLiteral(
            "Profil extrême : rendu manuel conseillé pour limiter la consommation mémoire."));
    }
    void populateTrees(const v16::SceneDatabase& database){
        sceneTree_->clear();resourceTree_->clear();
        QHash<QString,QTreeWidgetItem*> nodes;
        auto* root=new QTreeWidgetItem(sceneTree_,{database.name,QStringLiteral("V16")});
        nodes.insert(QString(),root);
        for(const auto& object:runtime_->objects){
            QString walk;
            QTreeWidgetItem* parent=root;
            for(const auto& part:object.parent.split('/',Qt::SkipEmptyParts)){
                walk+=QStringLiteral("/")+part;
                if(!nodes.contains(walk))nodes.insert(walk,new QTreeWidgetItem(parent,{part,QStringLiteral("Groupe")}));
                parent=nodes.value(walk);
            }
            auto* leaf=new QTreeWidgetItem(parent,{object.id,object.material});
            leaf->setData(0,Qt::UserRole,object.material);
        }
        for(const auto& sector:database.sectors){
            for(auto it=sector.resources.constBegin();it!=sector.resources.constEnd();++it){
                auto* category=new QTreeWidgetItem(resourceTree_,{it.key(),sector.id});
                for(const auto& resource:it.value())
                    new QTreeWidgetItem(category,{QFileInfo(resource.path).fileName(),resource.path});
            }
        }
        auto* catalog=new QTreeWidgetItem(resourceTree_,{QStringLiteral("Catalogue des matériaux"),QStringLiteral("partagés")});
        for(auto it=runtime_->materialIndex.cbegin();it!=runtime_->materialIndex.cend();++it){
            const auto& m=runtime_->renderScene.mat[size_t(it.value())];
            auto* leaf=new QTreeWidgetItem(catalog,{it.key(),QStringLiteral("PBR partagé")});
            leaf->setData(0,Qt::UserRole,it.key());
            auto addChannel=[&](const QString& label,size_t count,int size){
                auto* channel=new QTreeWidgetItem(leaf,{label,
                    QStringLiteral("%1² → 1² / %2 mipmaps").arg(size).arg(count)});
                channel->setData(0,Qt::UserRole,it.key());
            };
            if(!m.colorMip.empty())addChannel(QStringLiteral("Albedo — sRGB"),m.colorMip.size(),int(m.colorMip.front().size));
            if(!m.normalMip.empty())addChannel(QStringLiteral("Normal — linéaire"),m.normalMip.size(),int(m.normalMip.front().size));
            if(!m.roughnessMip.empty())addChannel(QStringLiteral("Rugosité — linéaire"),m.roughnessMip.size(),int(m.roughnessMip.front().size));
            new QTreeWidgetItem(leaf,{QStringLiteral("Métallicité"),QString::number(m.metallic,'f',2)});
            new QTreeWidgetItem(leaf,{QStringLiteral("Transmission"),QString::number(m.transmissionPercent)});
        }
        sceneTree_->expandToDepth(2);
        resourceTree_->expandToDepth(0);
        catalog->setExpanded(true);
    }
    void requestRender(){
        if(sceneLoading_||!runtime_||runtime_->cameras.isEmpty())return;
        const auto serial=renderSerial_;
        const uint64_t token=serial->fetch_add(1)+1;
        if(watcher_.isRunning()){
            pending_=true;
            statusBar()->showMessage(QStringLiteral("Nouveau rendu en attente de la fin du calcul précédent…"));
            return;
        }
        auto snapshot=runtime_;
        const int presetId=std::clamp(cameraBox_->currentIndex(),0,int(snapshot->cameras.size())-1);
        auto preset=snapshot->cameras[presetId];
        if(exploring_ && freeCameraActive_)preset=freeCamera_;
        else {
            const auto dir=preset.eye-preset.target;
            const float angle=float(azimuth_->value())*PI/180;
            preset.eye={preset.target.x+dir.x*std::cos(angle)+dir.z*std::sin(angle),
                        preset.eye.y,
                        preset.target.z-dir.x*std::sin(angle)+dir.z*std::cos(angle)};
        }
        const QSize renderSize=qualityBox_->currentData().toSize();
        const int w=renderSize.width();
        const int h=renderSize.height();
        const int backend=backendBox_->currentIndex();
        const bool wantGpu=backend==1||backend==4;
        const bool raster=backend==4;
        const bool globalIllumination=backend==2||backend==3;
        const bool edgeAA=edgeAa_->isChecked();
        const unsigned sampleCount=globalIllumination?
            unsigned(giSamples_->currentData().toInt()):
            (wantGpu?1u:unsigned(directSamples_->currentData().toInt()));
        const unsigned maxBounces=backend==2?2u:4u;
        const bool progressive=globalIllumination&&progressiveGi_->isChecked();
        const bool denoise=globalIllumination&&denoiseGi_->isChecked();
        const bool denoiseDirect=!globalIllumination&&!wantGpu&&denoiseDirect_->isChecked();
        statusBar()->showMessage(QStringLiteral("Calcul de l'image…"));
        // Explain an abnormal delay rather than displaying "Calcul..." forever.
        // This watchdog does not claim to cancel a driver-blocked D3D call.
        if(raster){
            QTimer::singleShot(30000,this,[this,serial,token]{
                if(serial->load()==token && watcher_.isRunning() && !closingRequested_)
                    statusBar()->showMessage(QStringLiteral(
                        "Rendu DirectX 11 actif depuis plus de 30 secondes. "
                        "GPU ou pilote possiblement saturé ; aucun basculement CPU."));
            });
        }
        const auto shadowCache=cpuShadows_;
        watcher_.setFuture(QtConcurrent::run(&renderPool_,[this,serial,token,snapshot,preset,w,h,wantGpu,
            globalIllumination,raster,edgeAA,sampleCount,maxBounces,progressive,denoise,denoiseDirect,shadowCache]{
            Result result;
            result.generation=token;
            result.renderThread=reinterpret_cast<quintptr>(QThread::currentThreadId());
            updateLighting(snapshot->daylightHour);
            const auto camera=v16::makeCamera(preset,w,h);
            std::vector<uint32_t> pixels;
            Stats stats;
            bool ok=false;
            if(wantGpu){
                std::wstring reason;
                if(raster){
                    std::shared_ptr<const ShadowMap> shadow;
                    {
                        std::lock_guard<std::mutex> lock(shadowCache->mutex);
                        if(shadowCache->scene.lock()==snapshot &&
                           shadowCache->hour==snapshot->daylightHour&&shadowCache->shadow)
                            shadow=shadowCache->shadow;
                        else {
                            Stats shadowStats;
                            shadow=std::make_shared<ShadowMap>(
                                buildShadow(snapshot->renderScene,shadowStats));
                            shadowCache->scene=snapshot;shadowCache->hour=snapshot->daylightHour;
                            shadowCache->shadow=shadow;
                        }
                    }
                    ok=renderDirectX11Preview(snapshot->renderScene,*shadow,camera,w,h,pixels,stats,reason);
                    if(ok)result.renderer=QStringLiteral("V17 D3D11 Raster — géométrie matérielle");
                }else{
                    ok=renderCompute11Preview(snapshot->renderScene,camera,w,h,pixels,stats,reason);
                    if(ok)result.renderer=QStringLiteral("V16 D3D11 Compute — GI adaptative");
                }
                if(!ok){
                    // Never send hundreds of thousands of triangles through
                    // CPU ray tracing as an invisible fallback. That used to
                    // look like an indefinitely stalled D3D11 render.
                    result.error=QStringLiteral(
                        "Échec du rendu GPU DirectX 11 : %1. "
                        "Aucun basculement automatique vers le CPU.")
                        .arg(QString::fromStdWString(reason));
                    result.renderer=QStringLiteral("V17 GPU D3D11 — erreur");
                    return result;
                }
            }
            if(!ok){
                pixels.resize(size_t(w)*size_t(h));
                std::shared_ptr<const ShadowMap> shadow;
                bool shadowReused=false;
                {
                    std::lock_guard<std::mutex> lock(shadowCache->mutex);
                    if(shadowCache->scene.lock()==snapshot &&
                       shadowCache->hour==snapshot->daylightHour && shadowCache->shadow){
                        shadow=shadowCache->shadow;
                        shadowReused=true;
                    }else{
                        Stats shadowStats;
                        shadow=std::make_shared<ShadowMap>(
                            buildShadow(snapshot->renderScene,shadowStats));
                        shadowCache->scene=snapshot;
                        shadowCache->hour=snapshot->daylightHour;
                        shadowCache->shadow=shadow;
                    }
                }
                const unsigned workers=std::max(1u,std::min(8u,std::thread::hardware_concurrency()));
                if(globalIllumination){
                    std::vector<V> hdrAccum;
                    unsigned completed=0;
                    double totalMs=0;
                    while(completed<sampleCount){
                        if(serial->load()!=token)return result;
                        const unsigned batch=progressive?
                            std::min(std::max(1u,completed),sampleCount-completed):
                            sampleCount-completed;
                        const auto pass=renderGiAccumulatePass(
                            snapshot->renderScene,*shadow,camera,w,h,hdrAccum,pixels,
                            completed,batch,workers,*serial,token,maxBounces);
                        if(serial->load()!=token)return result;
                        completed+=batch;
                        totalMs+=pass.ms;
                        std::vector<uint32_t> display=pixels;
                        // Filtering is separate from HDR accumulation, so the
                        // next batch never feeds on blurred/tonemapped pixels.
                        if(denoise&&completed>=4)
                            display=denoiseBilateral(display,w,h,workers);
                        if(edgeAA)display=antialiasEdges4(display,w,h,4,false);
                        QImage image(w,h,QImage::Format_ARGB32);
                        for(int y=0;y<h;++y)
                            std::memcpy(image.scanLine(y),display.data()+size_t(y)*w,size_t(w)*4);
                        if(progressive&&completed<sampleCount){
                            QMetaObject::invokeMethod(this,
                                [this,serial,token,count=completed,target=sampleCount,w,h,image=std::move(image)]() mutable {
                                    if(serial->load()!=token)return;
                                    lastImage_=std::move(image);
                                    canvas_->setImage(lastImage_);
                                    statusBar()->showMessage(
                                        QStringLiteral("GI progressive — %1 / %2 échantillons (%3×%4)")
                                            .arg(count).arg(target).arg(w).arg(h));
                                },Qt::QueuedConnection);
                        }else{
                            result.image=std::move(image);
                        }
                    }
                    result.renderer=QStringLiteral("CPU GI — %1 rebonds, %2 spp, HDR%3 — %4×%5")
                        .arg(maxBounces).arg(sampleCount)
                        .arg(denoise?QStringLiteral(", débruité"):QString())
                        .arg(w).arg(h);
                    result.milliseconds=totalMs;
                    return result;
                }
                // Reuse the active render generation so CPU fallback/direct
                // rays stop promptly when the scene or camera is changed.
                stats=m3d::render(snapshot->renderScene,*shadow,camera,w,h,pixels,
                                  workers,false,*serial,token,sampleCount,false,false,false,3);
                if(serial->load()!=token)return result;
                if(denoiseDirect) pixels=denoiseBilateral(pixels,w,h,workers);
                result.renderer=shadowReused
                    ?QStringLiteral("CPU Ray Tracing — %1 spp, ombres en cache").arg(sampleCount)
                    :QStringLiteral("CPU Ray Tracing — %1 spp, ombres calculées").arg(sampleCount);
            }else result.gpu=true;
            result.milliseconds=stats.ms;
            result.renderer+=QStringLiteral(" — %1×%2").arg(w).arg(h);
            if(edgeAA && !raster && pixels.size()==size_t(w)*size_t(h))
                pixels=antialiasEdges4(pixels,w,h,4,false);
            if(pixels.size()!=size_t(w)*h){
                result.error+=QStringLiteral("Image incomplète");
                return result;
            }
            QImage image(w,h,QImage::Format_ARGB32);
            for(int y=0;y<h;y++)std::memcpy(image.scanLine(y),pixels.data()+size_t(y)*w,size_t(w)*4);
            result.image=std::move(image);
            return result;
        }));
    }
};
QString defaultPath(){
    const QString app=QCoreApplication::applicationDirPath();
    const QStringList candidates{
        app+QStringLiteral("/pavillon01/scene.json"),
        app+QStringLiteral("/scenes/v16/pavillon01/scene.json"),
        QDir::currentPath()+QStringLiteral("/scenes/v16/pavillon01/scene.json")
    };
    for(const auto& path:candidates)if(QFileInfo::exists(path))return path;
    return QString();
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    QCoreApplication::setApplicationName(QStringLiteral("CSE WARP Beta X V17.%1")
        .arg(CSE_WARP_BUILD_NUMBER));
    QCoreApplication::setApplicationVersion(QStringLiteral("17.%1")
        .arg(CSE_WARP_BUILD_NUMBER));
    const bool switchSmoke=argc>=4
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-scene-switch");
    const bool repeatSmoke=argc>=3
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-repeat-raster");
    // Use the same Windows V17 brand icon for Qt titlebar/Alt-Tab/taskbar.
    // The ICO is also embedded in the EXE's native Win32 resources.
    const QString iconPath=QCoreApplication::applicationDirPath()
        +QStringLiteral("/cse_warp_v17.ico");
    if(QFileInfo::exists(iconPath))app.setWindowIcon(QIcon(iconPath));
    const QString path=switchSmoke||repeatSmoke
        ?QString::fromLocal8Bit(argv[2])
        :(argc>1?QString::fromLocal8Bit(argv[1]):defaultPath());
    // The smoke test uses reduced texture resolution to stay viable on
    // CI's software-only WARP device. Normal V17 keeps full 2048 PBR.
    V17Window window(path,(switchSmoke||repeatSmoke)?256:2048);
    if(switchSmoke)window.smokeSceneSwitch(QString::fromLocal8Bit(argv[3]));
    if(repeatSmoke)window.smokeRepeatRaster();
    if(!app.windowIcon().isNull())window.setWindowIcon(app.windowIcon());
    window.show();
    return app.exec();
}
