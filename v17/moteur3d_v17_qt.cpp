#include "m3d_engine.h"
#include "src/m3d_walk_controller.h"
#include "exploration_motion.h"
#include "scope_zoom.h"
#include "camera_diagnostic.h"
#include "anomaly_catalog.h"
#include "frame_pacing.h"
#include "pool_ladder.h"
#include "radio_audio.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
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
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QSysInfo>
#include <QFontMetrics>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <memory>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dxgi1_2.h>
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
namespace {
struct Result {
    QImage image;
    v16::CameraPreset cameraPose;
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
    void setImage(QImage im){image_=std::move(im);paintPending_=true;update();}
    void onPresented(std::function<void()> callback){presented_=std::move(callback);}
    void setPlaceholder(const QString& message){placeholder_=message;update();}
    void setCameraHud(QStringList lines,bool enabled){
        if(hudLines_==lines&&hudEnabled_==enabled)return;
        hudLines_=std::move(lines);hudEnabled_=enabled;update();
    }
    void setScopeOverlay(float level){
        level=std::clamp(level,0.f,1.f);
        if(std::abs(scopeLevel_-level)<.002f)return;
        scopeLevel_=level;
        update();
    }
    void setFpsOverlay(double fps,bool visible){
        const QString next=visible&&fps>0.0
            ?QStringLiteral("%1 FPS").arg(fps,0,'f',1)
            :QStringLiteral("FPS —");
        if(fpsVisible_==visible&&fpsText_==next)return;
        fpsVisible_=visible;
        fpsText_=next;
        update();
    }
    static void drawCameraHud(QPainter& painter,const QRect& bounds,const QStringList& lines){
        if(lines.isEmpty()||bounds.width()<240||bounds.height()<110)return;
        painter.save();
        QFont font=painter.font();font.setFamily(QStringLiteral("Consolas"));font.setPointSize(9);
        painter.setFont(font);
        const QFontMetrics fm(font);
        int longest=0;
        for(const auto& t:lines)longest=std::max(longest,fm.horizontalAdvance(t));
        const int pad=12,lh=fm.height()+3;
        const QRect panel(bounds.left()+12,bounds.top()+12,
                          std::min(bounds.width()-24,longest+pad*2),lh*lines.size()+pad*2);
        painter.setPen(Qt::NoPen);painter.setBrush(QColor(10,18,27,208));
        painter.drawRoundedRect(panel,8,8);
        for(int i=0;i<lines.size();++i){
            painter.setPen(i==0?QColor(140,224,245):QColor(236,243,249));
            painter.drawText(panel.left()+pad,panel.top()+pad+fm.ascent()+i*lh,
                fm.elidedText(lines[i],Qt::ElideRight,panel.width()-pad*2));
        }
        painter.restore();
    }
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
        s.scale(size(),Qt::KeepAspectRatio);
        QRect dst(QPoint((width()-s.width())/2,(height()-s.height())/2),s);
        p.setRenderHint(QPainter::SmoothPixmapTransform,false);
        p.drawImage(dst,image_);
        // Scoped reticle is only a 2D Qt overlay, with no effect on physical
        // position, collision rays or the raster/CPU renderer.
        if(scopeLevel_>.005f){
            p.save();
            p.setRenderHint(QPainter::Antialiasing,true);
            const QPointF center=dst.center();
            const qreal radius=qreal(std::min(dst.width(),dst.height()))*.405;
            QPainterPath outside;
            outside.setFillRule(Qt::OddEvenFill);
            outside.addRect(dst);
            outside.addEllipse(center,radius,radius);
            p.fillPath(outside,QColor(0,0,0,int(135.f*scopeLevel_)));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(228,236,231,int(195.f*scopeLevel_)),1.25));
            p.drawEllipse(center,radius,radius);
            const qreal line=qreal(std::max(12,int(radius*.14)));
            const qreal gap=5.;
            p.setPen(QPen(QColor(16,24,22,int(245.f*scopeLevel_)),1.8));
            p.drawLine(QPointF(center.x()-line,center.y()),QPointF(center.x()-gap,center.y()));
            p.drawLine(QPointF(center.x()+gap,center.y()),QPointF(center.x()+line,center.y()));
            p.drawLine(QPointF(center.x(),center.y()-line),QPointF(center.x(),center.y()-gap));
            p.drawLine(QPointF(center.x(),center.y()+gap),QPointF(center.x(),center.y()+line));
            p.setBrush(QColor(230,238,235,int(190.f*scopeLevel_)));
            p.setPen(Qt::NoPen);
            p.drawEllipse(center,1.75,1.75);
            p.restore();
        }
        if(hudEnabled_)drawCameraHud(p,dst,hudLines_);
        if(fpsVisible_){
            // Count completed presentations, not a theoretical GPU FPS claim.
            p.save();
            QFont f=p.font();f.setFamily(QStringLiteral("Consolas"));f.setPointSize(10);
            f.setBold(true);p.setFont(f);
            const int width=std::max(87,p.fontMetrics().horizontalAdvance(fpsText_)+18);
            const QRect badge(dst.right()-width-10,dst.top()+10,width,28);
            p.setPen(Qt::NoPen);p.setBrush(QColor(9,15,22,213));
            p.drawRoundedRect(badge,6,6);
            p.setPen(QColor(178,238,222));
            p.drawText(badge,Qt::AlignCenter,fpsText_);
            p.restore();
        }
        if(paintPending_){
            paintPending_=false;
            if(presented_)presented_();
        }
    }
private:
    QImage image_;
    bool paintPending_=false;
    std::function<void()> presented_;
    QString placeholder_;
    QStringList hudLines_;
    bool hudEnabled_=true;
    float scopeLevel_=0.f;
    bool fpsVisible_=false;
    QString fpsText_;
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
        // A stationary camera must always display the native full-resolution
        // raster, regardless of the experimental launcher environment.
        peripheralExperimentEnabled_=
            qEnvironmentVariable("M3D_V17_PERIPHERAL_MULTIREZ")==QStringLiteral("1");
        setRasterPeripheralMultiresOverride(0);
        pacingClock_.start();
        canvas_->onPresented([this]{recordImagePresented();});
        audioTimer_.setInterval(50); // 20 Hz spatial updates, independent from GPU images.
        connect(&audioTimer_,&QTimer::timeout,this,[this]{
            if(!radioPlaying_ || !runtime_)return;
            const auto pose=activeCameraPose();
            const float eye[3]{pose.eye.x,pose.eye.y,pose.eye.z};
            const float target[3]{pose.target.x,pose.target.y,pose.target.z};
            radioAudio_.update(eye,target);
        });
        audioTimer_.start();
        // A permanent status-bar widget outlives temporary render/progress
        // messages. QStatusBar::showMessage(..., 15000) used to erase the
        // completed render time after 15 seconds.
        renderTimeLabel_=new QLabel(QStringLiteral("Dernier rendu : —"),this);
        renderTimeLabel_->setObjectName(QStringLiteral("v16LastRenderTime"));
        statusBar()->addPermanentWidget(renderTimeLabel_);
        explorationSpeedLabel_=new QLabel(this);
        explorationSpeedLabel_->setObjectName(QStringLiteral("v17ExplorationSpeed"));
        statusBar()->addPermanentWidget(explorationSpeedLabel_);
        explorationSpeedLabel_->hide();
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
                ++pacingRejected_;
                if(pending_){pending_=false;requestRender();}
                return;
            }
            if(!outcome.image.isNull()){
                lastImage_=outcome.image;
                lastRenderedPose_=outcome.cameraPose;
                hasRenderedPose_=true;
                lastCompletedRenderMs_=outcome.milliseconds;
                canvas_->setImage(outcome.image);
            }
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
            else if(exploring_ && !sceneLoading_ && runtime_)requestRender();
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
    // Regression: recalling an anomaly must not switch the user's walking
    // preference to free flight. Previously that disabling was persisted in
    // QSettings and removed ALL collision checks on subsequent launches.
    void smokeWalkingCollisionPreference(){
        auto* poll=new QTimer(this);
        poll->setInterval(100);
        connect(poll,&QTimer::timeout,this,[this,poll]{
            if(sceneLoading_||!runtime_||!walkingModeBox_)return;
            poll->stop();
            QSettings prefs(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            const QVariant previous=prefs.value(QStringLiteral("Exploration/Walking"));
            walkingModeBox_->setChecked(true);
            prefs.setValue(QStringLiteral("Exploration/Walking"),true);
            prefs.sync();
            selectAnomaly(0); // ANO-005 has a precisely measured camera pose.
            const bool okAfterSelect=walkingModeBox_->isChecked()
                &&walkingCollisionSyncPending_
                &&prefs.value(QStringLiteral("Exploration/Walking")).toBool();
            returnBeforeAnomaly();
            prefs.sync();
            const bool okAfterReturn=walkingModeBox_->isChecked()
                &&walkingCollisionSyncPending_
                &&prefs.value(QStringLiteral("Exploration/Walking")).toBool();
            // Restore the prior preference after the smoke test.
            {
                QSignalBlocker block(walkingModeBox_);
                walkingModeBox_->setChecked(previous.isValid()?previous.toBool():true);
            }
            if(previous.isValid())prefs.setValue(QStringLiteral("Exploration/Walking"),previous);
            else prefs.remove(QStringLiteral("Exploration/Walking"));
            prefs.sync();
            if(!okAfterSelect||!okAfterReturn){
                std::fprintf(stderr,
                    "V17 COLLISION PREFERENCE FAILED: afterSelect=%d afterReturn=%d\\n",
                    int(okAfterSelect),int(okAfterReturn));
                QCoreApplication::exit(38);
                return;
            }
            std::puts("V17 COLLISION PREFERENCE PASS: anomaly pose + return retain walking preference");
            QCoreApplication::exit(0);
        });
        poll->start();
        QTimer::singleShot(120000,this,[]{
            std::fprintf(stderr,"V17 COLLISION PREFERENCE: timeout\\n");
            QCoreApplication::exit(39);
        });
    }
    // GUI integration smoke: actual Qt RMB events on the canvas, FOV narrows
    // while aiming, and release/stop restores the original camera exactly.
    // Does not change walking mode or camera position.
    void smokeScopedAim(){
        auto* poll=new QTimer(this);
        poll->setInterval(100);
        connect(poll,&QTimer::timeout,this,[this,poll]{
            if(sceneLoading_||!runtime_||runtime_->cameras.isEmpty())return;
            poll->stop();
            if(!freeCameraActive_)activateFreeCamera();
            exploring_=true;
            const float originalFov=freeCamera_.fov;
            const V originalEye=freeCamera_.eye;
            QMouseEvent down(QEvent::MouseButtonPress,QPointF(40,40),
                             Qt::RightButton,Qt::RightButton,Qt::NoModifier);
            QCoreApplication::sendEvent(canvas_,&down);
            const bool activated=rightMouseHeld_;
            scopeBlend_=cse::scope::advance(scopeBlend_,rightMouseHeld_,.08f);
            freeCamera_.fov=cse::scope::effectiveFov(scopeBaseFov_,scopeBlend_);
            const bool magnified=freeCamera_.fov<originalFov-1.f;
            QMouseEvent up(QEvent::MouseButtonRelease,QPointF(40,40),
                           Qt::RightButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(canvas_,&up);
            const bool released=!rightMouseHeld_;
            stopExploration();
            const bool restored=std::abs(freeCamera_.fov-originalFov)<.0005f;
            const bool unchanged=length(freeCamera_.eye-originalEye)<.0005f;
            if(!(activated&&magnified&&released&&restored&&unchanged)){
                std::fprintf(stderr,
                    "V17 SCOPE MOUSE FAIL: down=%d zoom=%d up=%d fov=%d eye=%d\\n",
                    int(activated),int(magnified),int(released),int(restored),int(unchanged));
                QCoreApplication::exit(44);return;
            }
            std::puts("V17 SCOPE MOUSE PASS: press/release and original FOV, position preserved");
            QCoreApplication::exit(0);
        });
        poll->start();
        QTimer::singleShot(120000,this,[]{
            std::fprintf(stderr,"V17 SCOPE MOUSE TIMEOUT\\n");
            QCoreApplication::exit(45);
        });
    }
    // Real Qt event regression for free-look, true F11 fullscreen and return.
    // Test uses a synthetic motion delta; no physical mouse is required on CI.
    void smokeImmersiveMouse(){
        auto* poll=new QTimer(this);
        poll->setInterval(100);
        connect(poll,&QTimer::timeout,this,[this,poll]{
            if(sceneLoading_||!runtime_||runtime_->cameras.isEmpty())return;
            poll->stop();
            activateFreeCamera();
            exploring_=true;
            mouseCaptured_=true;
            canvas_->setCursor(Qt::BlankCursor);
            const float originalYaw=yaw_;
            const V originalEye=freeCamera_.eye;
            const QPoint center=canvas_->rect().center();
            QMouseEvent move(QEvent::MouseMove,QPointF(center+QPoint(16,-9)),
                Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(canvas_,&move);
            const bool freeLookWorked=std::abs(yaw_-originalYaw)>.01f;
            const bool eyeUnchanged=length(freeCamera_.eye-originalEye)<.0005f;
            const bool fpsReady=canvas_->isVisible();
            immersiveAction_->setChecked(true);
            QCoreApplication::processEvents();
            const bool full=QWidget::isFullScreen()
                &&!menuBar()->isVisible()&&!statusBar()->isVisible();
            immersiveAction_->setChecked(false);
            QCoreApplication::processEvents();
            const bool back=!QWidget::isFullScreen()
                &&menuBar()->isVisible()&&statusBar()->isVisible();
            stopExploration();
            if(!(freeLookWorked&&eyeUnchanged&&full&&back&&fpsReady)){
                std::fprintf(stderr,
                    "IMMERSIVE MOUSE FAILED: move=%d eye=%d full=%d back=%d canvas=%d\\n",
                    int(freeLookWorked),int(eyeUnchanged),int(full),int(back),int(fpsReady));
                QCoreApplication::exit(49);return;
            }
            std::puts("IMMERSIVE MOUSE PASS: no-button look, true F11 fullscreen and restored editor");
            QCoreApplication::exit(0);
        });
        poll->start();
        QTimer::singleShot(120000,this,[]{
            std::fprintf(stderr,"IMMERSIVE MOUSE TIMEOUT\\n");
            QCoreApplication::exit(50);
        });
    }
    ~V17Window() override {
        audioTimer_.stop();
        stopRadio();
        renderSerial_->fetch_add(1);
        // All async tasks hold independent snapshots. Wait before QObject
        // destruction so no worker can post into the destroyed window.
        loadWatcher_.waitForFinished();
        watcher_.waitForFinished();
    }
private:
    int textureResolution_=2048;
    QPushButton* radioToggleButton_=nullptr;
    QLabel* radioStatusLabel_=nullptr;
    bool radioPlaying_=false;
    bool radioAutoplayEnabled_=true;
    cse::RadioAudio radioAudio_;
    QTimer audioTimer_;
    std::array<cse::RadioSpeaker,4> radioSpeakers_{};
    QSlider* radioVolumeSlider_=nullptr;

    ImageView* canvas_=nullptr;
    QTimer frameTimer_;
    QTimer skyAnimationTimer_;
    QTimer dayNightTimer_;
    QElapsedTimer dayNightClock_;
    QElapsedTimer frameClock_;
    QElapsedTimer motionClock_;
    QSet<int> keysDown_;
    QPoint previousMouse_;
    v16::CameraPreset freeCamera_;
    walk::State walkingState_{};
    cse::pool_ladder::Motion poolLadderMotion_{};
    QCheckBox* walkingModeBox_=nullptr;
    bool exploring_=false,freeCameraActive_=false,leftMouseHeld_=false,rightMouseHeld_=false;
    bool mouseCaptured_=false;
    bool peripheralExperimentEnabled_=false,peripheralDuringMotion_=false;
    QElapsedTimer lastMouseLookMotion_;
    QAction* immersiveAction_=nullptr;
    std::vector<std::pair<QWidget*,bool>> fullscreenRestore_;
    double fpsFiltered_=0.0;
    float scopeBaseFov_=65.f,scopeBlend_=0.f;
    // Exact anomaly/replayed camera is kept until the next walking input.
    // This does not disable or persistently alter the user's collision mode.
    bool walkingCollisionSyncPending_=false;
    bool explorationQualityOverride_=false;
    RasterQualitySettings explorationOriginalQuality_{};
    float yaw_=0.f,pitch_=0.f;
    bool cameraHudEnabled_=true;
    bool hasRenderedPose_=false;
    v16::CameraPreset lastRenderedPose_;
    QPushButton* explorationButton_=nullptr;
    QLabel* explorationHelp_=nullptr;
    QLabel* explorationSpeedLabel_=nullptr;
    v16::CameraPreset activeCameraPose() const {
        if(freeCameraActive_)return freeCamera_;
        v16::CameraPreset pose;
        if(!runtime_||runtime_->cameras.isEmpty())return pose;
        const int idx=std::clamp(cameraBox_->currentIndex(),0,int(runtime_->cameras.size())-1);
        pose=runtime_->cameras[idx];
        const V dir=pose.eye-pose.target;
        const float angle=float(azimuth_->value())*PI/180.f;
        pose.eye={pose.target.x+dir.x*std::cos(angle)+dir.z*std::sin(angle),
                  pose.eye.y,
                  pose.target.z-dir.x*std::sin(angle)+dir.z*std::cos(angle)};
        return pose;
    }
    void updateCameraHud(){
        if(!canvas_)return;
        if(!runtime_||runtime_->cameras.isEmpty()){
            canvas_->setCameraHud({},false);return;
        }
        const auto pose=activeCameraPose();
        const QString label=QStringLiteral("CAM %1 | %2")
            .arg(freeCameraActive_?QStringLiteral("Libre"):QStringLiteral("Préréglée"),
                 pose.name.isEmpty()?pose.id:pose.name);
        canvas_->setCameraHud(cse::camera_diagnostic::hudLines(pose,label),cameraHudEnabled_);
    }
    static QString gpuAdapterLabel(){
#if defined(_WIN32)
        IDXGIFactory1* factory=nullptr;
        if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))){
            IDXGIAdapter1* adapter=nullptr;
            if(SUCCEEDED(factory->EnumAdapters1(0,&adapter))){
                DXGI_ADAPTER_DESC1 d{};
                const HRESULT hr=adapter->GetDesc1(&d);
                adapter->Release();factory->Release();
                if(SUCCEEDED(hr))return QString::fromWCharArray(d.Description);
            }else factory->Release();
        }
#endif
        return QStringLiteral("Non détecté");
    }
    QString githubRevision() const {
        QFile f(QCoreApplication::applicationDirPath()+QStringLiteral("/REVISION.txt"));
        return f.open(QIODevice::ReadOnly|QIODevice::Text)
            ?QString::fromUtf8(f.readAll()).trimmed().left(12)
            :QStringLiteral("développement local");
    }
    static QJsonObject sceneCameraReport(const v16::CameraPreset& pose,
        const QString& manifest,const QString& title,const QString& image,
        const QSize imageSize,const QString& revision) {
        return {
            {"format",QString::fromLatin1(cse::camera_diagnostic::formatId)},
            {"createdAtUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"engine",QStringLiteral("CSE WARP Beta X")},
            {"version",QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER)},
            {"revision",revision},
            {"scene",QJsonObject{{"manifestFile",QFileInfo(manifest).fileName()},
                                 {"title",title},{"originalPath",manifest}}},
            {"camera",cse::camera_diagnostic::toJson(pose)},
            {"image",image},
            {"resolution",QJsonObject{{"width",imageSize.width()},{"height",imageSize.height()}}},
            {"units",QStringLiteral("metres")},
            {"coordinateConvention",QStringLiteral("Y vertical; yaw 0 degrees points toward -Z; positive yaw rotates toward +X")}
        };
    }
    void copyLiveCamera(){
        if(!runtime_||runtime_->cameras.isEmpty())return;
        const auto report=sceneCameraReport(activeCameraPose(),scenePath_,runtime_->title,
                                             QString(),QSize(),githubRevision());
        QGuiApplication::clipboard()->setText(
            QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        statusBar()->showMessage(QStringLiteral("Position caméra et identité de la scène copiées."),5000);
    }
    void captureAnomaly(){
        if(!runtime_||lastImage_.isNull()||!hasRenderedPose_){
            QMessageBox::information(this,QStringLiteral("Capture d'anomalie"),
                QStringLiteral("Une image doit être rendue avant d'enregistrer l'anomalie."));
            return;
        }
        // During navigation the CPU-readback image may lag behind the live camera.
        // Always export the exact pose associated with the accepted rendered frame.
        const QString documents=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const QString folder=QDir(documents.isEmpty()?QDir::homePath():documents)
            .filePath(QStringLiteral("CSE WARP/Anomalies"));
        if(!QDir().mkpath(folder)){
            QMessageBox::warning(this,QStringLiteral("Capture d'anomalie"),
                                 QStringLiteral("Dossier inaccessible : %1").arg(folder));
            return;
        }
        const QString stamp=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        const QString base=QDir(folder).filePath(QStringLiteral("ANOMALIE_%1").arg(stamp));
        const QString imageFile=base+QStringLiteral(".png");
        const QString jsonFile=base+QStringLiteral(".json");
        QImage annotated=lastImage_.copy();
        {
            QPainter painter(&annotated);
            ImageView::drawCameraHud(painter,annotated.rect(),
                cse::camera_diagnostic::hudLines(lastRenderedPose_,
                    QStringLiteral("CAMÉRA DU RENDU / V17.%1").arg(CSE_WARP_BUILD_NUMBER)));
        }
        if(!annotated.save(imageFile,"PNG")){
            QMessageBox::warning(this,QStringLiteral("Capture d'anomalie"),
                                 QStringLiteral("Échec de l'enregistrement PNG."));
            return;
        }
        QJsonObject report=sceneCameraReport(lastRenderedPose_,scenePath_,runtime_->title,
            QFileInfo(imageFile).fileName(),lastImage_.size(),githubRevision());
        report.insert("renderInProgressAtCapture",watcher_.isRunning());
        report.insert("liveCameraAtCapture",cse::camera_diagnostic::toJson(activeCameraPose()));
        report.insert("note",QStringLiteral(
            "camera correspond précisément au PNG enregistré; liveCameraAtCapture est informative."));
        QSaveFile file(jsonFile);
        if(!file.open(QIODevice::WriteOnly) ||
           file.write(QJsonDocument(report).toJson(QJsonDocument::Indented))<0 ||
           !file.commit()){
            QFile::remove(imageFile);
            QMessageBox::warning(this,QStringLiteral("Capture d'anomalie"),
                                 QStringLiteral("Échec de l'enregistrement JSON."));
            return;
        }
        statusBar()->showMessage(QStringLiteral("Anomalie sauvegardée : %1").arg(base),15000);
        QMessageBox::information(this,QStringLiteral("Anomalie enregistrée"),
            QStringLiteral("Capture PNG et coordonnées exactes disponibles dans :\n%1").arg(folder));
    }
    void restoreAnomalyCamera(){
        if(!runtime_||runtime_->cameras.isEmpty()){
            QMessageBox::information(this,QStringLiteral("Restaurer une caméra"),
                                     QStringLiteral("Charge d'abord la scène concernée."));
            return;
        }
        const QString path=QFileDialog::getOpenFileName(this,
            QStringLiteral("Restaurer une caméra d'anomalie"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                +QStringLiteral("/CSE WARP/Anomalies"),
            QStringLiteral("Rapport caméra CSE WARP (*.json)"));
        if(path.isEmpty())return;
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)){
            QMessageBox::warning(this,QStringLiteral("Restaurer une caméra"),
                                 QStringLiteral("Impossible d'ouvrir le rapport."));return;
        }
        QJsonParseError parsing;
        const auto doc=QJsonDocument::fromJson(file.readAll(),&parsing);
        const auto root=doc.object();
        if(parsing.error!=QJsonParseError::NoError ||
           root.value("format").toString()!=QString::fromLatin1(cse::camera_diagnostic::formatId)){
            QMessageBox::warning(this,QStringLiteral("Restaurer une caméra"),
                                 QStringLiteral("Format incompatible ou JSON invalide."));return;
        }
        const auto scene=root.value("scene").toObject();
        if(scene.value("manifestFile").toString()!=QFileInfo(scenePath_).fileName() ||
           scene.value("title").toString()!=runtime_->title){
            QMessageBox::warning(this,QStringLiteral("Scène différente"),
                QStringLiteral("La capture appartient à une autre scène. Charge d'abord cette scène pour éviter toute erreur architecturale."));
            return;
        }
        v16::CameraPreset pose;
        QString error;
        if(!cse::camera_diagnostic::fromJson(root.value("camera").toObject(),pose,error)){
            QMessageBox::warning(this,QStringLiteral("Restaurer une caméra"),error);return;
        }
        keysDown_.clear();
        // Preserve the exact screenshot pose without switching the
        // collision mode (which previously persisted "Vol libre" in QSettings).
        // On the next user movement, resynchronize feet with the real scene.
        freeCamera_=pose;
        resetScopeForCamera();
        walkingCollisionSyncPending_=true;
        const V dir=norm(pose.target-pose.eye);
        yaw_=std::atan2(dir.x,-dir.z);
        pitch_=std::asin(clampf(dir.y,-.99f,.99f));
        freeCameraActive_=true;
        updateExplorationSpeedIndicator();
        updateCameraHud();
        requestRender();
        canvas_->setFocus();
        statusBar()->showMessage(QStringLiteral("Point de vue d'anomalie restauré depuis %1").arg(path),12000);
    }
    void updateExplorationSpeedIndicator(){
        if(!explorationSpeedLabel_)return;
        if(!exploring_){
            explorationSpeedLabel_->hide();
            return;
        }
        const bool fast=keysDown_.contains(Qt::Key_Shift);
        const bool walking=walkingModeBox_&&walkingModeBox_->isChecked();
        explorationSpeedLabel_->setText(
            QStringLiteral("%1 · %2")
                .arg(walking?QStringLiteral("Marche"):QStringLiteral("Vol libre"))
                .arg(fast?QStringLiteral("Vitesse ×3 (Maj)")
                         :QStringLiteral("Vitesse ×1")));
        explorationSpeedLabel_->show();
    }
    void resetScopeForCamera(){
        rightMouseHeld_=false;
        scopeBlend_=0.f;
        scopeBaseFov_=freeCamera_.fov;
        if(canvas_)canvas_->setScopeOverlay(0.f);
    }
    void activateFreeCamera(){
        if(!runtime_||runtime_->cameras.isEmpty())return;
        const int idx=std::clamp(cameraBox_->currentIndex(),0,int(runtime_->cameras.size())-1);
        freeCamera_=runtime_->cameras[idx];
        resetScopeForCamera();
        const V dir=norm(freeCamera_.target-freeCamera_.eye);
        yaw_=std::atan2(dir.x,-dir.z);
        pitch_=std::asin(clampf(dir.y,-.99f,.99f));
        freeCameraActive_=true;
        walkingCollisionSyncPending_=false;
        if(walkingModeBox_&&walkingModeBox_->isChecked()){
            walk::spawn(runtime_->renderScene,freeCamera_.eye,walkingState_);
            freeCamera_.eye=walk::eye(walkingState_);
            updateLook();
        }
        updateCameraHud();
    }
    void updateLook(){
        const V forward{std::sin(yaw_)*std::cos(pitch_),std::sin(pitch_),
                        -std::cos(yaw_)*std::cos(pitch_)};
        freeCamera_.target=freeCamera_.eye+forward;
        updateCameraHud();
    }
    void captureMouseLook(){
        if(!exploring_||mouseCaptured_||!canvas_||!isActiveWindow())return;
        mouseCaptured_=true;
        canvas_->setCursor(Qt::BlankCursor);
        canvas_->setFocus(Qt::OtherFocusReason);
        QCursor::setPos(canvas_->mapToGlobal(canvas_->rect().center()));
    }
    void releaseMouseLook(){
        if(!mouseCaptured_)return;
        mouseCaptured_=false;
        if(canvas_)canvas_->unsetCursor();
    }
    void setMotionResolution(bool active){
        const bool next=peripheralExperimentEnabled_&&active;
        if(peripheralDuringMotion_==next)return;
        peripheralDuringMotion_=next;
        setRasterPeripheralMultiresOverride(next?1:0);
    }
    void stopExploration(){
        releaseMouseLook();
        setMotionResolution(false);
        exploring_=false;
        if(canvas_)canvas_->setFpsOverlay(0.0,false);
        if(freeCameraActive_)freeCamera_.fov=scopeBaseFov_;
        resetScopeForCamera();
        if(explorationQualityOverride_){
            std::wstring error;
            if(setRasterQualitySettings(explorationOriginalQuality_,error))
                displayRasterSettings(explorationOriginalQuality_);
            explorationQualityOverride_=false;
        }
        frameTimer_.stop();
        keysDown_.clear();
        poolLadderMotion_={};
        leftMouseHeld_=false;
        rightMouseHeld_=false;
        lastMouseLookMotion_.invalidate();
        updateExplorationSpeedIndicator();
        if(explorationButton_)explorationButton_->setText(QStringLiteral("Démarrer exploration (V17)"));
        if(explorationHelp_)explorationHelp_->setText(QStringLiteral(
            "ZQSD / flèches : marcher · souris : regarder librement · clic droit : lunette ×3\n"
            "F11 : plein écran · Échap : arrêter · E : échelle · Maj : accélérer · Vol libre : Espace / Ctrl."));
        // A delayed multires GPU frame could still be on screen when Esc
        // stops exploration. Queue a final guaranteed full-resolution image.
        if(!closingRequested_&&!sceneLoading_&&runtime_)
            QTimer::singleShot(0,this,[this]{
                if(!exploring_&&!closingRequested_&&runtime_)requestRender();
            });
    }
    void toggleExploration(){
        if(exploring_){stopExploration();return;}
        if(!runtime_||runtime_->cameras.isEmpty())return;
        if(backendBox_->currentIndex()!=4)backendBox_->setCurrentIndex(4);
        if(!freeCameraActive_)activateFreeCamera();
        // An Intel HD 620 cannot reasonably sustain 8x MSAA during continuous
        // navigation of the large V8 scene. Temporarily lower AA to 2x ONLY
        // while moving. Restore the user's precise settings on exit.
        const auto adapter=gpuAdapterLabel();
        if(adapter.contains(QStringLiteral("HD Graphics 620"),Qt::CaseInsensitive)
           ||adapter.contains(QStringLiteral("UHD Graphics 620"),Qt::CaseInsensitive)){
            const auto q=rasterQualitySettings();
            if(q.msaaSamples>2){
                auto interactive=q;
                interactive.msaaSamples=2;
                interactive.anisotropy=std::min<unsigned>(q.anisotropy,8);
                std::wstring error;
                if(setRasterQualitySettings(interactive,error)){
                    explorationOriginalQuality_=q;
                    explorationQualityOverride_=true;
                    displayRasterSettings(interactive);
                    statusBar()->showMessage(QStringLiteral(
                        "HD 620 : exploration MSAA 2x temporaire ; qualité initiale restaurée à l'arrêt"),7000);
                }
            }
        }
        exploring_=true;
        setMotionResolution(false);
        resetPacing();
        canvas_->setFpsOverlay(0.0,true);
        if(walkingModeBox_&&!walkingModeBox_->isChecked()){
            statusBar()->showMessage(QStringLiteral(
                "Vol libre : collisions désactivées. Cochez « Marcher au sol » pour bloquer murs et obstacles."),10000);
        }
        poolLadderMotion_={};
        motionClock_.start();
        frameClock_.invalidate();
        lastMouseLookMotion_.invalidate();
        if(explorationButton_)explorationButton_->setText(QStringLiteral("Arrêter exploration"));
        updateCameraHud();
        frameTimer_.start();
        updateExplorationSpeedIndicator();
        canvas_->setFocus();
        captureMouseLook();
    }
    void frameTick(){
        if(!exploring_||!runtime_||!freeCameraActive_)return;
        const float dt=std::min(0.08f,float(motionClock_.restart())/1000.f);
        const float nextScope=cse::scope::advance(scopeBlend_,rightMouseHeld_,dt);
        if(nextScope!=scopeBlend_){
            scopeBlend_=nextScope;
            freeCamera_.fov=cse::scope::effectiveFov(scopeBaseFov_,scopeBlend_);
            canvas_->setScopeOverlay(scopeBlend_);
            updateCameraHud();
        }
        V forward{std::sin(yaw_)*std::cos(pitch_),std::sin(pitch_),
                  -std::cos(yaw_)*std::cos(pitch_)};
        V horizontal=norm(V{forward.x,0,forward.z});
        V right=norm(cross(horizontal,V{0,1,0}));
        V move{};
        if(keysDown_.contains(Qt::Key_Z)||keysDown_.contains(Qt::Key_W)||keysDown_.contains(Qt::Key_Up))move=move+horizontal;
        if(keysDown_.contains(Qt::Key_S)||keysDown_.contains(Qt::Key_Down))move=move-horizontal;
        if(keysDown_.contains(Qt::Key_D)||keysDown_.contains(Qt::Key_Right))move=move+right;
        if(keysDown_.contains(Qt::Key_Q)||keysDown_.contains(Qt::Key_A)||keysDown_.contains(Qt::Key_Left))move=move-right;
        const bool mouseRecentlyMoved=lastMouseLookMotion_.isValid()&&
            lastMouseLookMotion_.elapsed()<180;
        const bool moving=length(move)>.01f||poolLadderMotion_.active();
        // The scoped sight remains fully sharp. On stopping, raster returns
        // to full resolution within 180ms, without any temporal reprojection.
        setMotionResolution((moving||mouseRecentlyMoved)&&
            !rightMouseHeld_&&scopeBlend_<.05f);
        if(walkingModeBox_&&walkingModeBox_->isChecked()){
            // A recalled anomaly camera can be above a floor or behind a wall.
            // Keep its precise eye position until the user actually navigates.
            // Never reuse stale feet from before camera teleportation.
            if(walkingCollisionSyncPending_ && length(move)>.01f){
                walk::spawn(runtime_->renderScene,freeCamera_.eye,walkingState_);
                freeCamera_.eye=walk::eye(walkingState_);
                walkingCollisionSyncPending_=false;
                updateLook();
            }
            if(poolLadderMotion_.active()){
                const bool climbing=poolLadderMotion_.up;
                const auto p=poolLadderMotion_.advance(dt);
                walkingState_.feet={p.x,p.y,p.z};
                freeCamera_.eye=walk::eye(walkingState_);
                updateLook();
                if(!poolLadderMotion_.active() && climbing){
                    // Re-establish dry ground contact after climbing over coping.
                    const V eye=walk::eye(walkingState_);
                    walk::spawn(runtime_->renderScene,eye,walkingState_);
                    freeCamera_.eye=walk::eye(walkingState_);
                    updateLook();
                }
                if(!watcher_.isRunning()){frameClock_.start();requestRender();}
                return;
            }
            const V currentFeet=walkingState_.feet;
            if(scenePath_.contains(QStringLiteral("benchmark-civic"),Qt::CaseInsensitive) &&
               cse::pool_ladder::inWater({currentFeet.x,currentFeet.y,currentFeet.z})){
                const float speed=cse::exploration::speedMetersPerSecond(false)*.55f;
                const V horizontalMotion=length(move)>.01f?norm(move)*(dt*speed):V{};
                const auto p=cse::pool_ladder::walkInWater(
                    {currentFeet.x,currentFeet.y,currentFeet.z},horizontalMotion.x,horizontalMotion.z);
                walkingState_.feet={p.x,p.y,p.z};
                freeCamera_.eye=walk::eye(walkingState_);
                updateLook();
                if(!watcher_.isRunning()){frameClock_.start();requestRender();}
                return;
            }
            // Walking is always horizontal input. Height follows geometry:
            // floor, stairs, landings, slopes and gravity on unsupported areas.
            // Shift is an exact 3× multiplier; the deterministic walker still
            // subdivides large motion into <= 8.5cm steps for collision safety.
            const float speed=cse::exploration::speedMetersPerSecond(
                keysDown_.contains(Qt::Key_Shift));
            const V displacement=length(move)>.01f?norm(move)*(dt*speed):V{};
            const V oldEye=freeCamera_.eye;
            const walk::State beforeStep=walkingState_;
            walk::advance(runtime_->renderScene,walkingState_,displacement,dt);
            // The water volume is reached ONLY through the actual ladder
            // interaction. Don't let numerical walker tunnelling bypass the
            // tiled coping (a V17.437 user-found regression).
            if(scenePath_.contains(QStringLiteral("benchmark-civic"),Qt::CaseInsensitive)
                && beforeStep.feet.y<1.0f && walkingState_.feet.y<1.0f){
                const auto dry=[](const V& feet){
                    return !(std::abs(feet.x)<24.92f &&
                        feet.z>-44.45f && feet.z<-19.55f);
                };
                if(dry(beforeStep.feet)&&!dry(walkingState_.feet))
                    walkingState_=beforeStep;
            }
            freeCamera_.eye=walk::eye(walkingState_);
            if(length(freeCamera_.eye-oldEye)>.00001f)updateLook();
        }else{
            if(keysDown_.contains(Qt::Key_Space))move.y+=1.f;
            if(keysDown_.contains(Qt::Key_Control))move.y-=1.f;
            if(length(move)>.01f){
                freeCamera_.eye=freeCamera_.eye+norm(move)*(dt*cse::exploration::speedMetersPerSecond(keysDown_.contains(Qt::Key_Shift)));
                updateLook();
            }
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
    bool event(QEvent* e) override {
        if(e->type()==QEvent::WindowActivate&&exploring_){
            QTimer::singleShot(0,this,[this]{
                if(exploring_&&!closingRequested_)captureMouseLook();
            });
        }
        if(e->type()==QEvent::WindowDeactivate){
            // Alt+Tab / focus loss must never leave the pointer hidden or
            // the rifle scope button logically stuck in the pressed state.
            releaseMouseLook();
            rightMouseHeld_=false;
        }
        return QMainWindow::event(e);
    }
    bool eventFilter(QObject* watched,QEvent* event) override{
        if(watched==canvas_ && exploring_){
            if(event->type()==QEvent::MouseButtonPress){
                auto* e=static_cast<QMouseEvent*>(event);
                if(!mouseCaptured_)captureMouseLook();
                if(e->button()==Qt::RightButton){
                    rightMouseHeld_=true;
                    canvas_->setFocus();
                    return true;
                }
                if(e->button()==Qt::LeftButton){
                    leftMouseHeld_=true;
                    canvas_->setFocus();
                    return true;
                }
            }
            if(event->type()==QEvent::MouseButtonRelease){
                auto* e=static_cast<QMouseEvent*>(event);
                if(e->button()==Qt::RightButton){rightMouseHeld_=false;return true;}
                if(e->button()==Qt::LeftButton){leftMouseHeld_=false;return true;}
            }
            if(event->type()==QEvent::MouseMove && mouseCaptured_){
                auto* e=static_cast<QMouseEvent*>(event);
                const QPoint center=canvas_->rect().center();
                const QPoint delta=e->pos()-center;
                // Qt generates a synthetic move when the cursor is recentered.
                // Ignore it. No click is required to look around.
                if(delta.isNull())return true;
                const float dx=clampf(float(delta.x()),-160.f,160.f);
                const float dy=clampf(float(delta.y()),-160.f,160.f);
                const float sensitivity=.005f*cse::scope::mouseSensitivity(scopeBlend_);
                yaw_+=dx*sensitivity;
                pitch_=clampf(pitch_-dy*sensitivity,-1.48f,1.48f);
                lastMouseLookMotion_.restart();
                updateLook();
                QCursor::setPos(canvas_->mapToGlobal(center));
                return true;
            }
        }
        return QMainWindow::eventFilter(watched,event);
    }
    void keyPressEvent(QKeyEvent* e) override{
        if(e->key()==Qt::Key_F11 && immersiveAction_){
            immersiveAction_->setChecked(!immersiveAction_->isChecked());
            e->accept();return;
        }
        if(exploring_){
            if(e->key()==Qt::Key_Escape){
                stopExploration();
                if(immersiveAction_&&immersiveAction_->isChecked())
                    immersiveAction_->setChecked(false);
                e->accept();return;
            }
            if(e->key()==Qt::Key_Tab){
                if(mouseCaptured_)releaseMouseLook();else captureMouseLook();
                e->accept();return;
            }
            if(e->key()==Qt::Key_E && !e->isAutoRepeat() &&
               walkingModeBox_ && walkingModeBox_->isChecked() &&
               scenePath_.contains(QStringLiteral("benchmark-civic"),Qt::CaseInsensitive)){
                if(walkingCollisionSyncPending_&&runtime_){
                    walk::spawn(runtime_->renderScene,freeCamera_.eye,walkingState_);
                    freeCamera_.eye=walk::eye(walkingState_);
                    walkingCollisionSyncPending_=false;
                    updateLook();
                }
                const V feet=walkingState_.feet;
                if(poolLadderMotion_.begin({feet.x,feet.y,feet.z})){
                    keysDown_.clear();
                    statusBar()->showMessage(poolLadderMotion_.up
                        ?QStringLiteral("Piscine : montée de l'échelle en cours…")
                        :QStringLiteral("Piscine : descente de l'échelle en cours…"),3000);
                    e->accept();return;
                }
            }
            if(!e->isAutoRepeat()){
                keysDown_.insert(e->key());
                if(e->key()==Qt::Key_Shift)updateExplorationSpeedIndicator();
            }
            e->accept();return;
        }
        QMainWindow::keyPressEvent(e);
    }
    void keyReleaseEvent(QKeyEvent* e) override{
        if(exploring_){if(!e->isAutoRepeat()){keysDown_.remove(e->key());if(e->key()==Qt::Key_Shift)updateExplorationSpeedIndicator();}e->accept();return;}
        QMainWindow::keyReleaseEvent(e);
    }
private:
    QTreeWidget* sceneTree_=nullptr;
    QTreeWidget* resourceTree_=nullptr;
    QComboBox* cameraBox_=nullptr;
    QVector<QAction*> quickCameraActions_;
    QComboBox* backendBox_=nullptr;
    QComboBox* qualityBox_=nullptr;
    QComboBox* rasterPresetBox_=nullptr;
    QComboBox* rasterMsaaBox_=nullptr;
    QComboBox* rasterAnisoBox_=nullptr;
    QComboBox* rasterLodBox_=nullptr;
    QCheckBox* rasterCullBox_=nullptr;
    QCheckBox* rasterDynamicLodBox_=nullptr;
    QComboBox* giSamples_=nullptr;
    QComboBox* directSamples_=nullptr;
    QCheckBox* progressiveGi_=nullptr;
    QCheckBox* denoiseGi_=nullptr;
    QCheckBox* denoiseDirect_=nullptr;
    QCheckBox* edgeAa_=nullptr;
    QSlider* azimuth_=nullptr;
    QDoubleSpinBox* sunHour_=nullptr;
    QCheckBox* cloudAnimationBox_=nullptr;
    QCheckBox* dayNightCycleBox_=nullptr;
    QCheckBox* hybridLightBox_=nullptr;
    QCheckBox* filmicToneBox_=nullptr;
    QCheckBox* colorEnergyBox_=nullptr;
    QDoubleSpinBox* colorExposureBox_=nullptr;
    QPushButton* bakeGiButton_=nullptr;
    QProcess* bakeGiProcess_=nullptr;
    QString bakingScenePath_;
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
    QElapsedTimer pacingClock_;
    qint64 lastPresentMs_=-1;
    double lastCompletedRenderMs_=0.0;
    uint64_t pacingRejected_=0, pacingQueued_=0;
    std::vector<cse::pacing::Sample> pacingSamples_;
    static constexpr int pacingCapacity_=5000;
    int selectedMaterial_=-1;
    QString scenePath_;
    QString materialsPath_;
    int currentAnomaly_=-1;
    v16::CameraPreset beforeAnomalyCamera_{};
    bool hasBeforeAnomalyCamera_=false;
    QLabel* anomalyPanelLabel_=nullptr;

    void displayRasterSettings(const RasterQualitySettings& q){
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
    void applyRasterSettings(bool userCustom){
        if(!rasterPresetBox_||!rasterMsaaBox_)return;
        if(userCustom){
            const QSignalBlocker blocker(rasterPresetBox_);
            rasterPresetBox_->setCurrentIndex(4);
        }
        RasterQualitySettings q;
        q.msaaSamples=rasterMsaaBox_->currentData().toUInt();
        q.anisotropy=rasterAnisoBox_->currentData().toUInt();
        q.lodQualityPercent=rasterLodBox_->currentData().toUInt();
        q.frustumCulling=rasterCullBox_->isChecked();
        q.dynamicLod=rasterDynamicLodBox_->isChecked();
        rasterLodBox_->setEnabled(q.dynamicLod);
        std::wstring error;
        if(!setRasterQualitySettings(q,error)){
            statusBar()->showMessage(QString::fromStdWString(error),7000);
            return;
        }
        QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
        settings.setValue(QStringLiteral("Raster/Preset"),rasterPresetBox_->currentIndex());
        settings.setValue(QStringLiteral("Raster/MSAA"),q.msaaSamples);
        settings.setValue(QStringLiteral("Raster/Anisotropy"),q.anisotropy);
        settings.setValue(QStringLiteral("Raster/LODPercent"),q.lodQualityPercent);
        settings.setValue(QStringLiteral("Raster/Culling"),q.frustumCulling);
        settings.setValue(QStringLiteral("Raster/DynamicLOD"),q.dynamicLod);
        if(runtime_&&backendBox_&&backendBox_->currentIndex()==4)requestRender();
    }
    void stopRadio(){
        radioAudio_.stop();
        radioPlaying_=false;
        if(radioToggleButton_)radioToggleButton_->setText(QStringLiteral("Reprendre la radio"));
        if(radioStatusLabel_)radioStatusLabel_->setText(QStringLiteral("Radio arrêtée"));
    }
    bool loadSpeakerLayout(){
        const QString path=QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("audio/speaker_layout.json"));
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly))return false;
        const auto doc=QJsonDocument::fromJson(file.readAll());
        if(!doc.isObject())return false;
        const QJsonArray sources=doc.object().value(QStringLiteral("loudspeakers")).toArray();
        if(sources.size()!=4)return false;
        for(int i=0;i<4;++i){
            if(!sources[i].isObject())return false;
            const auto obj=sources[i].toObject();
            const auto pos=obj.value(QStringLiteral("position")).toArray();
            const auto front=obj.value(QStringLiteral("front")).toArray();
            if(pos.size()!=3||front.size()!=3)return false;
            for(int axis=0;axis<3;++axis){
                const double p=pos[axis].toDouble(std::numeric_limits<double>::quiet_NaN());
                const double f=front[axis].toDouble(std::numeric_limits<double>::quiet_NaN());
                if(!std::isfinite(p)||!std::isfinite(f))return false;
                radioSpeakers_[i].position[axis]=float(p);
                radioSpeakers_[i].front[axis]=float(f);
            }
        }
        return true;
    }
    void playRadio(){
        const QString path=QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("audio/pripyat_radio.wav"));
        if(!QFileInfo::exists(path)){
            if(radioStatusLabel_)radioStatusLabel_->setText(
                QStringLiteral("Fichier WAV absent : audio/pripyat_radio.wav"));
            return;
        }
        if(!loadSpeakerLayout()){
            if(radioStatusLabel_)radioStatusLabel_->setText(
                QStringLiteral("Implantation des 4 HP introuvable ou invalide"));
            return;
        }
        radioAudio_.setVolume(radioVolumeSlider_?float(radioVolumeSlider_->value())/500.f:.20f);
        radioPlaying_=radioAudio_.start(path.toStdWString(),radioSpeakers_);
        if(radioToggleButton_)radioToggleButton_->setText(radioPlaying_
            ?QStringLiteral("Arrêter la radio"):QStringLiteral("Reprendre la radio"));
        if(radioStatusLabel_)radioStatusLabel_->setText(radioPlaying_
            ?QStringLiteral("4 HP spatialisés · XAudio2 / X3DAudio · WAV de test")
            :QStringLiteral("Audio 3D indisponible (pilote ou WAV incorrect)"));
    }
    void refreshRadioForScene(const QString& path){
        stopRadio();
        const QString lower=path.toLower();
        const bool isSquare=lower.contains(QStringLiteral("benchmark-civic"))
            ||lower.contains(QStringLiteral("pripyat"))
            ||lower.contains(QStringLiteral("energetik"));
        if(isSquare&&radioAutoplayEnabled_)playRadio();
    }

    void selectAnomaly(int index){
        if(index<0||index>=int(cse::anomalies::records.size()))return;
        if(!runtime_||runtime_->cameras.isEmpty()){
            QMessageBox::information(this,QStringLiteral("Anomalies"),
                QStringLiteral("Chargez la scene du palais avant de consulter le registre."));
            return;
        }
        if(!scenePath_.contains(QStringLiteral("benchmark-civic"),Qt::CaseInsensitive)){
            QMessageBox::warning(this,QStringLiteral("Anomalies"),
                QStringLiteral("Les positions sont relevees dans le palais benchmark-civic. Ouvrez cette scene avant de les utiliser."));
            return;
        }
        const auto& a=cse::anomalies::records[size_t(index)];
        if(!hasBeforeAnomalyCamera_){
            beforeAnomalyCamera_=activeCameraPose();
            hasBeforeAnomalyCamera_=true;
        }
        currentAnomaly_=index;
        if(a.cameraKnown){
            // Camera recalls preserve a precise reference view. Do not
            // silently disable walking/collisions or overwrite QSettings.
            // Only resynchronize the walking state on the next movement.
            keysDown_.clear();
            poolLadderMotion_={};
            const float yr=a.yaw*PI/180.f;
            const float pr=a.pitch*PI/180.f;
            freeCamera_.eye={a.x,a.y,a.z};
            walkingCollisionSyncPending_=true;
            freeCamera_.fov=a.fov;
            resetScopeForCamera();
            freeCamera_.id=QString::fromLatin1(a.id);
            freeCamera_.name=QString::fromUtf8(a.title);
            yaw_=yr;pitch_=pr;
            freeCameraActive_=true;
            updateLook();
            requestRender();
            canvas_->setFocus();
        }else{
            // ANO-014 has no measured coordinates; keep the current pose.
            // The user can select exterior camera 01 or walk to any paving joint.
            statusBar()->showMessage(QStringLiteral(
                "ANO-014 : pas de coordonnees mesurees. Selectionnez une vue exterieure puis marchez sur les joints."),13000);
        }
        updateAnomalyPanel();
    }
    void updateAnomalyPanel(){
        if(!anomalyPanelLabel_)return;
        if(currentAnomaly_<0){
            anomalyPanelLabel_->setText(QStringLiteral(
                "Choisissez ANO-005 a ANO-014 depuis le menu Anomalies."));
            return;
        }
        const auto& a=cse::anomalies::records[size_t(currentAnomaly_)];
        anomalyPanelLabel_->setText(
            QStringLiteral("%1 - %2\n%3\n%4")
              .arg(QString::fromLatin1(a.id),
                   QString::fromUtf8(a.title),
                   QString::fromUtf8(a.check),
                   a.correctedCandidate?QStringLiteral("Modification codee : a valider")
                                       :QStringLiteral("Anomalie encore ouverte")));
        anomalyPanelLabel_->setWordWrap(true);
        statusBar()->showMessage(
            QStringLiteral("%1 : %2").arg(QString::fromLatin1(a.id),QString::fromUtf8(a.check)),
            12000);
    }
    void saveAnomalyVerdict(const QString& verdict){
        if(currentAnomaly_<0)return;
        const auto& a=cse::anomalies::records[size_t(currentAnomaly_)];
        // Separate file per attempt: do not overwrite the evidence from another
        // version or previous review. No external storage or telemetry.
        const QString root=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            +QStringLiteral("/CSE-WARP-Anomalies");
        if(!QDir().mkpath(root)){
            QMessageBox::warning(this,QStringLiteral("Anomalies"),
                QStringLiteral("Impossible de creer le dossier : %1").arg(root));
            return;
        }
        const QString timestamp=QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
        const QString path=root+QStringLiteral("/")
            +QString::fromLatin1(a.id)+QStringLiteral("-V17.%1-").arg(CSE_WARP_BUILD_NUMBER)
            +timestamp+QStringLiteral(".json");
        const QJsonObject record{
            {QStringLiteral("format"),QStringLiteral("cse-warp-anomaly-review-v1")},
            {QStringLiteral("version"),QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER)},
            {QStringLiteral("anomalyId"),QString::fromLatin1(a.id)},
            {QStringLiteral("title"),QString::fromUtf8(a.title)},
            {QStringLiteral("expected"),QString::fromUtf8(a.check)},
            {QStringLiteral("verdict"),verdict},
            {QStringLiteral("scene"),QFileInfo(scenePath_).fileName()},
            {QStringLiteral("timestamp"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("referencePoseKnown"),a.cameraKnown},
            {QStringLiteral("camera"),cse::camera_diagnostic::toJson(activeCameraPose())}
        };
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)||
            file.write(QJsonDocument(record).toJson(QJsonDocument::Indented))<0||
            !file.commit()){
            QMessageBox::warning(this,QStringLiteral("Anomalies"),
               QStringLiteral("Echec d'enregistrement : %1").arg(path));
            return;
        }
        statusBar()->showMessage(QStringLiteral("%1 : verdict %2 sauvegarde : %3")
            .arg(QString::fromLatin1(a.id),verdict,path),14000);
    }
    void returnBeforeAnomaly(){
        if(!hasBeforeAnomalyCamera_)return;
        const auto pose=beforeAnomalyCamera_;
        // Return to the old camera without changing the selected navigation
        // mode or disabling obstacle collisions in saved preferences.
        freeCamera_=pose;
        resetScopeForCamera();
        walkingCollisionSyncPending_=true;
        const V dir=norm(pose.target-pose.eye);
        yaw_=std::atan2(dir.x,-dir.z);
        pitch_=std::asin(clampf(dir.y,-.99f,.99f));
        freeCameraActive_=true;
        hasBeforeAnomalyCamera_=false;
        currentAnomaly_=-1;
        updateAnomalyPanel();
        updateCameraHud();
        requestRender();
        canvas_->setFocus();
    }

    void createUi(){
        auto* file=menuBar()->addMenu(QStringLiteral("Fichier"));
        auto* captureAction=file->addAction(QStringLiteral("Capturer une anomalie (PNG + caméra JSON)…"));
        captureAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
        connect(captureAction,&QAction::triggered,this,[this]{captureAnomaly();});
        auto* restoreAction=file->addAction(QStringLiteral("Restaurer une caméra d'anomalie…"));
        restoreAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
        connect(restoreAction,&QAction::triggered,this,[this]{restoreAnomalyCamera();});
        auto* copyPoseAction=file->addAction(QStringLiteral("Copier la position caméra (JSON)"));
        copyPoseAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
        connect(copyPoseAction,&QAction::triggered,this,[this]{copyLiveCamera();});
        file->addSeparator();
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
        // V17.443 - one-click regression review at the EXACT user-reported
        // viewpoints, independent of the six artistic camera presets.
        auto* anomaliesMenu=menuBar()->addMenu(QStringLiteral("Anomalies"));
        for(int i=0;i<int(cse::anomalies::records.size());++i){
            const auto& a=cse::anomalies::records[size_t(i)];
            const QString name=QStringLiteral("%1 - %2%3")
                .arg(QString::fromLatin1(a.id),
                     QString::fromUtf8(a.title),
                     a.cameraKnown?QString():QStringLiteral(" (position a definir)"));
            auto* act=anomaliesMenu->addAction(name);
            connect(act,&QAction::triggered,this,[this,i]{selectAnomaly(i);});
        }
        anomaliesMenu->addSeparator();
        auto* prevAnomaly=anomaliesMenu->addAction(QStringLiteral("Anomalie precedente"));
        prevAnomaly->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Left")));
        connect(prevAnomaly,&QAction::triggered,this,[this]{
            selectAnomaly(currentAnomaly_<0?0:std::max(0,currentAnomaly_-1));
        });
        auto* nextAnomaly=anomaliesMenu->addAction(QStringLiteral("Anomalie suivante"));
        nextAnomaly->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Right")));
        connect(nextAnomaly,&QAction::triggered,this,[this]{
            selectAnomaly(currentAnomaly_<0?0:std::min(int(cse::anomalies::records.size())-1,currentAnomaly_+1));
        });
        auto* restoreAnomaly=anomaliesMenu->addAction(QStringLiteral("Retour a la position initiale"));
        connect(restoreAnomaly,&QAction::triggered,this,[this]{returnBeforeAnomaly();});
        auto* reviewDock=new QDockWidget(QStringLiteral("Validation des anomalies"),this);
        auto* reviewWidget=new QWidget(reviewDock);
        auto* reviewLayout=new QVBoxLayout(reviewWidget);
        anomalyPanelLabel_=new QLabel(QStringLiteral(
            "Choisissez ANO-005 a ANO-014 depuis le menu Anomalies."),reviewWidget);
        anomalyPanelLabel_->setWordWrap(true);
        reviewLayout->addWidget(anomalyPanelLabel_);
        auto* reviewButtons=new QHBoxLayout;
        for(const auto& verdict:QVector<QPair<QString,QString>>{
            {QStringLiteral("Conforme"),QStringLiteral("conforme")},
            {QStringLiteral("Presente"),QStringLiteral("toujours_presente")},
            {QStringLiteral("A revoir"),QStringLiteral("a_revoir")}}){
            auto* button=new QPushButton(verdict.first,reviewWidget);
            connect(button,&QPushButton::clicked,this,[this,verdict]{
                saveAnomalyVerdict(verdict.second);
            });
            reviewButtons->addWidget(button);
        }
        reviewLayout->addLayout(reviewButtons);
        reviewDock->setWidget(reviewWidget);
        addDockWidget(Qt::BottomDockWidgetArea,reviewDock);
        reviewDock->setVisible(false);
        connect(anomaliesMenu,&QMenu::aboutToShow,reviewDock,[reviewDock]{
            reviewDock->show();
        });

        auto* toolsMenu=menuBar()->addMenu(QStringLiteral("Outils"));
        auto* pacingAction=toolsMenu->addAction(QStringLiteral("Exporter benchmark fluidité (CSV + JSON)"));
        connect(pacingAction,&QAction::triggered,this,[this]{exportPacing();});
        auto* benchmarkAction=toolsMenu->addAction(
            QStringLiteral("Benchmark V17.%1 — performances…").arg(CSE_WARP_BUILD_NUMBER));
        connect(benchmarkAction,&QAction::triggered,this,[this]{
            if(scenePath_.isEmpty()){
                QMessageBox::information(this,QStringLiteral("Benchmark"),QStringLiteral("Charger une scène avant de lancer le benchmark."));
                return;
            }
            const QString path=QCoreApplication::applicationDirPath()
                +QStringLiteral("/outils/benchmark/moteur3d_v17_benchui.exe");
            if(!QFileInfo::exists(path)){
                QMessageBox::warning(this,QStringLiteral("Benchmark indisponible"),
                                     QStringLiteral("Programme de benchmark introuvable : %1").arg(path));
                return;
            }
            // The benchmark is another process: the DLL's atomic settings
            // cannot be shared implicitly. Transfer the active Raster quality
            // explicitly so all image/performance comparisons use the exact
            // same MSAA, anisotropy, LOD and culling configuration.
            const auto q=rasterQualitySettings();
            const QStringList arguments{
                scenePath_,
                QStringLiteral("--raster-msaa"),QString::number(q.msaaSamples),
                QStringLiteral("--raster-anisotropy"),QString::number(q.anisotropy),
                QStringLiteral("--raster-lod-percent"),QString::number(q.lodQualityPercent),
                QStringLiteral("--raster-culling"),QString::number(q.frustumCulling?1:0),
                QStringLiteral("--raster-lod-enabled"),QString::number(q.dynamicLod?1:0)
            };
            if(!QProcess::startDetached(path,arguments,QCoreApplication::applicationDirPath()))
                QMessageBox::critical(this,QStringLiteral("Benchmark"),QStringLiteral("Impossible d'ouvrir la fenêtre de benchmark."));
        });
        auto* helpMenu=menuBar()->addMenu(QStringLiteral("Aide"));
        auto* aboutAction=helpMenu->addAction(
            QStringLiteral("À propos de CSE WARP V17.%1…").arg(CSE_WARP_BUILD_NUMBER));
        connect(aboutAction,&QAction::triggered,this,[this]{
            const QString gpu=gpuAdapterLabel();
            const QString revision=githubRevision();
            const QString compileDate=QString::fromLatin1(__DATE__ " " __TIME__);
            const auto quality=rasterQualitySettings();
            const bool zEnabled=qEnvironmentVariable("M3D_V17_EXPERIMENT_Z_PREPASS")==QStringLiteral("1");
            const bool lightEnabled=qEnvironmentVariable("M3D_V17_EXPERIMENT_LIGHT_AABB_CULL")==QStringLiteral("1");
            const QString backend=backendBox_?backendBox_->currentText():QStringLiteral("indéterminé");
            const QString details=QStringLiteral(
                "CSE WARP Beta X / V17.%1\\n"
                "Révision GitHub : %2\\n"
                "Compilation : %3\\n"
                "Système : %4\\n"
                "Adaptateur principal DXGI : %5\\n"
                "Rendu sélectionné : %6\\n"
                "Qt : %7\\n"
                "MSAA : %8x | Anisotropie : %9x | PBR : %10 px\\n"
                "Z-prepass expérimental : %11 | Rejet AABB lampes : %12\\n"
                "HUD caméra / rapports d'anomalies PNG+JSON / restauration : disponibles")
                .arg(CSE_WARP_BUILD_NUMBER)
                .arg(revision).arg(compileDate).arg(QSysInfo::prettyProductName())
                .arg(gpu).arg(backend).arg(QString::fromLatin1(qVersion()))
                .arg(quality.msaaSamples).arg(quality.anisotropy)
                .arg(textureResolution_).arg(zEnabled?QStringLiteral("ON"):QStringLiteral("OFF"))
                .arg(lightEnabled?QStringLiteral("ON"):QStringLiteral("OFF"));
            QMessageBox message(this);
            message.setWindowTitle(QStringLiteral("À propos de CSE WARP V17.%1").arg(CSE_WARP_BUILD_NUMBER));
            message.setIcon(QMessageBox::Information);
            message.setTextFormat(Qt::RichText);
            message.setText(QStringLiteral(
                "<h3>CSE WARP Beta X — V17.%1</h3>"
                "<p><b>Révision :</b> %2<br/><b>Compilation :</b> %3</p>"
                "<p><b>GPU principal (DXGI) :</b> %4<br/><b>Rendu :</b> %5</p>"
                "<p><b>Navigation :</b> HUD position X/Y/Z, cap, inclinaison et FOV.<br/>"
                "<b>Diagnostic architectural :</b> capture PNG + position JSON restaurable.<br/>"
                "<b>Raster :</b> Z-prepass %6, rejet AABB lumières %7 (expérimentaux).</p>"
                "<p><b>Configuration :</b> Qt %8 ; MSAA %9x ; PBR %10 px.</p>")
                .arg(CSE_WARP_BUILD_NUMBER).arg(revision.toHtmlEscaped())
                .arg(compileDate.toHtmlEscaped()).arg(gpu.toHtmlEscaped())
                .arg(backend.toHtmlEscaped())
                .arg(zEnabled?QStringLiteral("ON"):QStringLiteral("OFF"))
                .arg(lightEnabled?QStringLiteral("ON"):QStringLiteral("OFF"))
                .arg(QString::fromLatin1(qVersion()))
                .arg(quality.msaaSamples).arg(textureResolution_));
            auto* copy=message.addButton(QStringLiteral("Copier les informations système"),QMessageBox::ActionRole);
            message.addButton(QMessageBox::Ok);
            message.exec();
            if(message.clickedButton()==copy)QGuiApplication::clipboard()->setText(details);
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
        radioToggleButton_=new QPushButton(QStringLiteral("Reprendre la radio"),controls);
        radioStatusLabel_=new QLabel(QStringLiteral("Radio arrêtée"),controls);
        radioStatusLabel_->setWordWrap(true);
        vertical->addWidget(new QLabel(QStringLiteral("SON AMBIANT — RADIO UKRAINIENNE"),controls));
        vertical->addWidget(radioToggleButton_);
        vertical->addWidget(radioStatusLabel_);
        radioVolumeSlider_=new QSlider(Qt::Horizontal,controls);
        radioVolumeSlider_->setRange(0,100);
        radioVolumeSlider_->setValue(65);
        radioVolumeSlider_->setToolTip(QStringLiteral("Volume de la radio 3D (0 à 100 %)"));
        vertical->addWidget(new QLabel(QStringLiteral("Volume radio"),controls));
        vertical->addWidget(radioVolumeSlider_);
        connect(radioVolumeSlider_,&QSlider::valueChanged,this,[this](int value){
            radioAudio_.setVolume(float(value)/500.f);
        });
        connect(radioToggleButton_,&QPushButton::clicked,this,[this]{
            if(radioPlaying_){radioAutoplayEnabled_=false;stopRadio();}
            else{radioAutoplayEnabled_=true;playRadio();}
        });
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
        qualityBox_->addItem(QStringLiteral("Viewport adaptatif — taille du panneau 3D"),QSize());
        qualityBox_->addItem(QStringLiteral("Dell natif 1366 × 768"),QSize(1366,768));
        qualityBox_->addItem(QStringLiteral("Full HD 1920 × 1080"),QSize(1920,1080));
        qualityBox_->addItem(QStringLiteral("QHD 2560 × 1440"),QSize(2560,1440));
        qualityBox_->addItem(QStringLiteral("4K UHD 3840 × 2160"),QSize(3840,2160));
        qualityBox_->setCurrentIndex(0); // Full-panel display by default; Dell 1366x768 remains available.
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
        sunHour_->setRange(0.0,23.99);
        sunHour_->setDecimals(2);
        sunHour_->setSingleStep(.25);
        sunHour_->setSuffix(QStringLiteral(" h"));
        sunHour_->setValue(14.75);
        sunHour_->setToolTip(QStringLiteral("Soleil et éclairage de 00 h à 24 h — aube, jour, crépuscule, nuit"));
        cloudAnimationBox_=new QCheckBox(QStringLiteral("Nuages animés sur le ciel"));
        cloudAnimationBox_->setChecked(true);
        cloudAnimationBox_->setToolTip(QStringLiteral(
            "Véritables cirrus calculés par le shader DirectX11. La scène reste nette."));
        dayNightCycleBox_=new QCheckBox(QStringLiteral("Simulation jour/nuit (24 h en 6 min)"));
        dayNightCycleBox_->setChecked(false);
        dayNightCycleBox_->setToolTip(QStringLiteral(
            "Fait progresser le soleil, le ciel, les ombres et les éclairages du bâtiment."));
        // Two independent controls avoid coupling global illumination and
        // output tone mapping. Preserve the existing hybrid preference, while
        // the new filmic shoulder defaults OFF to avoid V17.309 wash-out.
        hybridLightBox_=new QCheckBox(QStringLiteral("Rebonds indirects 3D (bêta)"));
        hybridLightBox_->setToolTip(QStringLiteral(
            "Sondes précalculées, ciel plus froid, rebonds colorés et zones abritées.\n"
            "Ne change pas le traitement tonal."));
        // Calibrated linear-light energy path. Does not modify sRGB textures
        // or flatten dielectric reflections; legacy lighting remains A/B.
        colorEnergyBox_=new QCheckBox(QStringLiteral("Couleurs fidèles et lumière équilibrée"));
        colorEnergyBox_->setToolTip(QStringLiteral(
            "Équilibre l'éclairage ambiant, le soleil et les reflets du béton\n"
            "dans l'espace linéaire, sans filtre de saturation ni flou PBR."));
        colorExposureBox_=new QDoubleSpinBox;
        colorExposureBox_->setRange(.65,1.45);
        colorExposureBox_->setDecimals(2);
        colorExposureBox_->setSingleStep(.05);
        colorExposureBox_->setToolTip(QStringLiteral(
            "Réglage fin de l'exposition du nouveau modèle. 1,00 = neutre.\n"
            "Préserve la teinte et la saturation des matériaux."));
        filmicToneBox_=new QCheckBox(QStringLiteral("Compression tonale filmique (optionnelle)"));
        filmicToneBox_->setToolTip(QStringLiteral(
            "Adoucit les hautes lumières sans remonter les demi-teintes.\n"
            "Indépendant des sondes indirectes."));
        QSettings hybridPrefs(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
        hybridLightBox_->setChecked(hybridPrefs.value(QStringLiteral("Raster/HybridLighting"),true).toBool());
        filmicToneBox_->setChecked(hybridPrefs.value(QStringLiteral("Raster/FilmicTonemap"),false).toBool());
        colorEnergyBox_->setChecked(hybridPrefs.value(QStringLiteral("Raster/ColorEnergyBalance"),true).toBool());
        colorExposureBox_->setValue(hybridPrefs.value(QStringLiteral("Raster/ColorExposure"),1.0).toDouble());
        colorExposureBox_->setEnabled(colorEnergyBox_->isChecked());
        m3d::setRasterHybridLightingEnabled(hybridLightBox_->isChecked());
        m3d::setRasterFilmicTonemapEnabled(filmicToneBox_->isChecked());
        m3d::setRasterColorEnergyBalanceEnabled(colorEnergyBox_->isChecked());
        m3d::setRasterColorExposure(float(colorExposureBox_->value()));
        azimuth_=new QSlider(Qt::Horizontal);
        azimuth_->setRange(-180,180);
        azimuth_->setValue(0);
        form->addRow(QStringLiteral("Caméra"),cameraBox_);
        form->addRow(QStringLiteral("Méthode de rendu"),backendBox_);
        form->addRow(QStringLiteral("Résolution"),qualityBox_);
        form->addRow(QStringLiteral("Échantillons CPU direct"),directSamples_);
        form->addRow(QStringLiteral("Échantillons GI"),giSamples_);
        form->addRow(QStringLiteral("Heure solaire"),sunHour_);
        form->addRow(cloudAnimationBox_);
        form->addRow(dayNightCycleBox_);
        form->addRow(hybridLightBox_);
        form->addRow(colorEnergyBox_);
        form->addRow(QStringLiteral("Exposition linéaire"),colorExposureBox_);
        form->addRow(filmicToneBox_);
        form->addRow(QStringLiteral("Rotation caméra"),azimuth_);
        vertical->addLayout(form);
        vertical->addWidget(progressiveGi_);
        vertical->addWidget(denoiseGi_);
        vertical->addWidget(denoiseDirect_);
        edgeAa_=new QCheckBox(QStringLiteral("Antialiasing des contours"));
        edgeAa_->setChecked(true);
        vertical->addWidget(edgeAa_);
        // One quality source for Qt and embedding host applications.
        // All controls update an atomic engine snapshot; the GPU worker
        // applies the new settings only at the next frame boundary.
        auto* rasterQuality=new QGroupBox(QStringLiteral("Qualité Raster DirectX 11"));
        auto* rasterForm=new QFormLayout(rasterQuality);
        rasterPresetBox_=new QComboBox;
        rasterPresetBox_->addItems({
            QStringLiteral("Performance"),
            QStringLiteral("Équilibrée"),
            QStringLiteral("Élevée — qualité historique"),
            QStringLiteral("Ultra — finesse maximale"),
            QStringLiteral("Personnalisée")
        });
        rasterForm->addRow(QStringLiteral("Préréglage"),rasterPresetBox_);
        rasterMsaaBox_=new QComboBox;
        for(int v:{1,2,4,8})
            rasterMsaaBox_->addItem(QStringLiteral("MSAA %1×").arg(v),v);
        rasterForm->addRow(QStringLiteral("Anti-aliasing GPU"),rasterMsaaBox_);
        rasterAnisoBox_=new QComboBox;
        for(int v:{1,2,4,8,16})
            rasterAnisoBox_->addItem(QStringLiteral("Anisotropie %1×").arg(v),v);
        rasterForm->addRow(QStringLiteral("Textures PBR"),rasterAnisoBox_);
        rasterLodBox_=new QComboBox;
        for(int v:{50,60,75,85,100,125,150,180,200})
            rasterLodBox_->addItem(QStringLiteral("Qualité LOD %1 %").arg(v),v);
        rasterForm->addRow(QStringLiteral("Distance détails"),rasterLodBox_);
        rasterCullBox_=new QCheckBox(QStringLiteral("Ne pas calculer les objets hors caméra"));
        rasterForm->addRow(rasterCullBox_);
        rasterDynamicLodBox_=new QCheckBox(QStringLiteral("LOD adaptatif des objets éloignés"));
        rasterForm->addRow(rasterDynamicLodBox_);
        vertical->addWidget(rasterQuality);
        QSettings stored(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
        int initialPreset=std::clamp(stored.value(QStringLiteral("Raster/Preset"),2).toInt(),0,4);
        // An embedding host may configure the raster API BEFORE launching
        // Qt. Respect it unless the user already saved UI preferences.
        auto initialQuality=stored.contains(QStringLiteral("Raster/Preset"))
            ?rasterQualityProfile(RasterQualityProfile(std::min(initialPreset,3)))
            :rasterQualitySettings();
        if(!stored.contains(QStringLiteral("Raster/Preset"))){
            initialPreset=4;
            for(int candidate=0;candidate<4;++candidate){
                const auto q=rasterQualityProfile(RasterQualityProfile(candidate));
                if(q.msaaSamples==initialQuality.msaaSamples &&
                   q.anisotropy==initialQuality.anisotropy &&
                   q.lodQualityPercent==initialQuality.lodQualityPercent &&
                   q.frustumCulling==initialQuality.frustumCulling &&
                   q.dynamicLod==initialQuality.dynamicLod){
                    initialPreset=candidate;break;
                }
            }
        }
        if(initialPreset==4){
            initialQuality.msaaSamples=stored.value(QStringLiteral("Raster/MSAA"),4).toUInt();
            initialQuality.anisotropy=stored.value(QStringLiteral("Raster/Anisotropy"),16).toUInt();
            initialQuality.lodQualityPercent=stored.value(QStringLiteral("Raster/LODPercent"),100).toUInt();
            initialQuality.frustumCulling=stored.value(QStringLiteral("Raster/Culling"),true).toBool();
            initialQuality.dynamicLod=stored.value(QStringLiteral("Raster/DynamicLOD"),true).toBool();
        }
        if(rasterMsaaBox_->findData(int(initialQuality.msaaSamples))<0||
           rasterAnisoBox_->findData(int(initialQuality.anisotropy))<0||
           rasterLodBox_->findData(int(initialQuality.lodQualityPercent))<0){
            initialPreset=2;initialQuality=rasterQualityProfile(RasterQualityProfile::High);
        }
        rasterPresetBox_->setCurrentIndex(initialPreset);
        displayRasterSettings(initialQuality);
        std::wstring qualityError;
        setRasterQualitySettings(initialQuality,qualityError);
        connect(rasterPresetBox_,qOverload<int>(&QComboBox::currentIndexChanged),
                this,[this](int selected){
            if(selected>=0&&selected<4)
                displayRasterSettings(rasterQualityProfile(RasterQualityProfile(selected)));
            applyRasterSettings(false);
        });
        for(auto* combo:{rasterMsaaBox_,rasterAnisoBox_,rasterLodBox_}){
            connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),
                    this,[this](int){applyRasterSettings(true);});
        }
        connect(rasterCullBox_,&QCheckBox::toggled,this,
                [this](bool){applyRasterSettings(true);});
        connect(rasterDynamicLodBox_,&QCheckBox::toggled,this,
                [this](bool){applyRasterSettings(true);});
        explorationButton_=new QPushButton(QStringLiteral("Démarrer exploration (V17)"));
        vertical->addWidget(explorationButton_);
        walkingModeBox_=new QCheckBox(QStringLiteral("Marcher au sol (yeux à 1,70 m, escaliers et obstacles)"));
        walkingModeBox_->setToolTip(QStringLiteral(
            "Marche à hauteur d'homme, monte les marches de 34 cm au maximum,\n"
            "descend les paliers et bloque les murs. Décochez pour voler librement."));
        QSettings walkPreferences(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
        walkingModeBox_->setChecked(walkPreferences.value(QStringLiteral("Exploration/Walking"),true).toBool());
        vertical->addWidget(walkingModeBox_);
        explorationHelp_=new QLabel(QStringLiteral(
            "ZQSD / flèches : marcher · souris libre : orienter la caméra · clic droit : zoom ×3\\n"
            "F11 : plein écran · Échap : arrêter · E : échelle · Ctrl+Maj+A : anomalie PNG+JSON."));
        explorationHelp_->setWordWrap(true);
        vertical->addWidget(explorationHelp_);
        connect(explorationButton_,&QPushButton::clicked,this,[this]{toggleExploration();});
        bakeGiButton_=new QPushButton(QStringLiteral("Précalculer la table GI (ray tracing CPU)"));
        bakeGiButton_->setToolTip(QStringLiteral(
            "Calcule les vrais rebonds sur les surfaces et écrit une table GI pour 25 heures solaires.\n"
            "Opération hors ligne dans un programme séparé, sans ralentir chaque image."));
        bakeGiButton_->setEnabled(false);
        vertical->addWidget(bakeGiButton_);
        bakeGiProcess_=new QProcess(this);
        connect(bakeGiButton_,&QPushButton::clicked,this,[this]{
            if(scenePath_.isEmpty()||bakeGiProcess_->state()!=QProcess::NotRunning)return;
            const QString exe=QCoreApplication::applicationDirPath()
                +QStringLiteral("/outils/gi/moteur3d_v17_gibake.exe");
            if(!QFileInfo::exists(exe)){
                QMessageBox::warning(this,QStringLiteral("Précalcul GI"),
                    QStringLiteral("Exécutable moteur3d_v17_gibake.exe absent du dossier de l'application."));
                return;
            }
            bakingScenePath_=scenePath_;
            bakeGiButton_->setEnabled(false);
            bakeGiButton_->setText(QStringLiteral("Calcul de la table GI en cours…"));
            statusBar()->showMessage(QStringLiteral(
                "Précalcul hors ligne : rayons et rebonds sur les surfaces de la scène…"));
            bakeGiProcess_->setProgram(exe);
            bakeGiProcess_->setArguments({bakingScenePath_,
                                         bakingScenePath_+QStringLiteral(".m3dgi")});
            bakeGiProcess_->start();
        });
        connect(bakeGiProcess_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),
                this,[this](int exitCode,QProcess::ExitStatus exitStatus){
            bakeGiButton_->setText(QStringLiteral("Précalculer la table GI (ray tracing CPU)"));
            bakeGiButton_->setEnabled(bool(runtime_));
            if(exitStatus!=QProcess::NormalExit||exitCode!=0){
                statusBar()->showMessage(QStringLiteral(
                    "Échec précalcul GI : %1").arg(QString::fromUtf8(
                        bakeGiProcess_->readAllStandardError()).left(200)),15000);
                return;
            }
            if(bakingScenePath_!=scenePath_)return; // scene switched during bake
            setRasterGiCachePath(std::filesystem::path(
                (scenePath_+QStringLiteral(".m3dgi")).toStdWString()));
            statusBar()->showMessage(QStringLiteral(
                "Table GI enregistrée pour %1 — rebonds chargés au prochain rendu.")
                .arg(QFileInfo(scenePath_).fileName()),16000);
            requestRender();
        });
        connect(walkingModeBox_,&QCheckBox::toggled,this,[this](bool enabled){
            updateExplorationSpeedIndicator();
            QSettings preferences(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            preferences.setValue(QStringLiteral("Exploration/Walking"),enabled);
            if(exploring_&&freeCameraActive_&&runtime_){
                if(enabled){
                    walk::spawn(runtime_->renderScene,freeCamera_.eye,walkingState_);
                    walkingCollisionSyncPending_=false;
                    freeCamera_.eye=walk::eye(walkingState_);
                    updateLook();
                }
                requestRender();
            }
        });
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
        auto* rightScroll=new QScrollArea;
        rightScroll->setWidgetResizable(true);
        rightScroll->setWidget(controls);
        right->setWidget(rightScroll);
        addDockWidget(Qt::RightDockWidgetArea,right);
        right->setMinimumWidth(295);
        // True fullscreen (not just hidden docks), preserving the editor
        // layout for a reversible F11 return.
        auto* viewMenu=menuBar()->addMenu(QStringLiteral("Affichage"));
        auto* hudToggle=viewMenu->addAction(QStringLiteral("Afficher les coordonnées caméra (HUD)"));
        hudToggle->setCheckable(true);
        QSettings hudSettings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
        cameraHudEnabled_=hudSettings.value(QStringLiteral("Camera/ShowHUD"),true).toBool();
        hudToggle->setChecked(cameraHudEnabled_);
        connect(hudToggle,&QAction::toggled,this,[this](bool enabled){
            cameraHudEnabled_=enabled;
            QSettings cfg(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            cfg.setValue(QStringLiteral("Camera/ShowHUD"),enabled);
            updateCameraHud();
        });
        immersiveAction_=viewMenu->addAction(QStringLiteral("Plein écran 3D (F11)"));
        immersiveAction_->setCheckable(true);
        immersiveAction_->setShortcut(QKeySequence(Qt::Key_F11));
        immersiveAction_->setShortcutContext(Qt::ApplicationShortcut);
        // F11 must remain available even while the menu bar is hidden.
        addAction(immersiveAction_);
        connect(immersiveAction_,&QAction::toggled,this,[this](bool enabled){
            if(enabled){
                fullscreenRestore_.clear();
                const auto hideRemembered=[this](QWidget* w){
                    if(!w)return;
                    fullscreenRestore_.emplace_back(w,w->isVisible());
                    w->hide();
                };
                for(auto* dock:findChildren<QDockWidget*>())hideRemembered(dock);
                for(auto* bar:findChildren<QToolBar*>())hideRemembered(bar);
                hideRemembered(menuBar());
                hideRemembered(statusBar());
                showFullScreen();
            }else{
                showNormal();
                for(const auto& entry:fullscreenRestore_)
                    if(entry.first)entry.first->setVisible(entry.second);
                fullscreenRestore_.clear();
            }
            QTimer::singleShot(0,this,[this]{
                if(exploring_){
                    releaseMouseLook();
                    canvas_->setFocus();
                    captureMouseLook();
                }
                if(qualityBox_&&qualityBox_->currentIndex()==0)requestRender();
            });
        });

        connect(renderButton,&QPushButton::clicked,this,[this]{requestRender();});
        connect(cameraBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
            azimuth_->setValue(0);
            if(exploring_)activateFreeCamera();
            else freeCameraActive_=false;
            updateCameraHud();
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
        connect(azimuth_,&QSlider::sliderReleased,this,[this]{updateCameraHud();requestRender();});
        connect(hybridLightBox_,&QCheckBox::toggled,this,[this](bool enabled){
            m3d::setRasterHybridLightingEnabled(enabled);
            QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            settings.setValue(QStringLiteral("Raster/HybridLighting"),enabled);
            if(backendBox_&&backendBox_->currentIndex()==4)requestRender();
        });
        connect(colorEnergyBox_,&QCheckBox::toggled,this,[this](bool enabled){
            m3d::setRasterColorEnergyBalanceEnabled(enabled);
            colorExposureBox_->setEnabled(enabled);
            QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            settings.setValue(QStringLiteral("Raster/ColorEnergyBalance"),enabled);
            if(backendBox_&&backendBox_->currentIndex()==4)requestRender();
        });
        connect(colorExposureBox_,qOverload<double>(&QDoubleSpinBox::valueChanged),
                this,[this](double value){
            m3d::setRasterColorExposure(float(value));
            QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            settings.setValue(QStringLiteral("Raster/ColorExposure"),value);
            if(backendBox_&&backendBox_->currentIndex()==4&&colorEnergyBox_->isChecked())
                requestRender();
        });
        connect(filmicToneBox_,&QCheckBox::toggled,this,[this](bool enabled){
            m3d::setRasterFilmicTonemapEnabled(enabled);
            QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            settings.setValue(QStringLiteral("Raster/FilmicTonemap"),enabled);
            if(backendBox_&&backendBox_->currentIndex()==4)requestRender();
        });
        // Separate, rate-limited sky updates. Do not queue renders when the
        // integrated GPU is already working; preserve HD620 responsiveness.
        skyAnimationTimer_.setInterval(1200);
        connect(&skyAnimationTimer_,&QTimer::timeout,this,[this]{
            if(!cloudAnimationBox_->isChecked() || dayNightCycleBox_->isChecked() ||
               exploring_ || sceneLoading_ || !runtime_ ||
               !backendBox_ || backendBox_->currentIndex()!=4 ||
               watcher_.isRunning())return;
            requestRender();
        });
        skyAnimationTimer_.start();
        dayNightTimer_.setInterval(750);
        connect(&dayNightTimer_,&QTimer::timeout,this,[this]{
            const auto elapsed=dayNightClock_.restart();
            if(!runtime_ || sceneLoading_ || watcher_.isRunning())return;
            const double seconds=std::min<qint64>(elapsed,1500)/1000.0;
            if(seconds<=0)return;
            const double next=std::fmod(sunHour_->value()+seconds*24.0/360.0,24.0);
            sunHour_->setValue(next);
        });
        connect(dayNightCycleBox_,&QCheckBox::toggled,this,[this](bool enabled){
            if(enabled){dayNightClock_.start();dayNightTimer_.start();}
            else dayNightTimer_.stop();
        });
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
        canvas_->setCameraHud({},false);
        hasRenderedPose_=false;
        freeCameraActive_=false;
        canvas_->setPlaceholder(QStringLiteral("CSE WARP V17\nChargement de la scène…"));
        cameraBox_->setEnabled(false);
        if(explorationButton_)explorationButton_->setEnabled(false);
        if(bakeGiButton_)bakeGiButton_->setEnabled(false);
        if(bakeGiProcess_&&bakeGiProcess_->state()!=QProcess::NotRunning)
            bakeGiProcess_->kill();
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
        refreshRadioForScene(scenePath_);
        setRasterGiCachePath(std::filesystem::path(
            (scenePath_+QStringLiteral(".m3dgi")).toStdWString()));
        if(bakeGiButton_)bakeGiButton_->setEnabled(true);
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
    void resetPacing(){
        pacingSamples_.clear();
        pacingRejected_=0;
        pacingQueued_=0;
        lastPresentMs_=-1;
        lastCompletedRenderMs_=0.0;
        fpsFiltered_=0.0;
        pacingClock_.restart();
    }
    void recordImagePresented(){
        if(!pacingClock_.isValid())pacingClock_.start();
        const qint64 ms=pacingClock_.elapsed();
        if(exploring_){
            cse::pacing::Sample sample{};
            sample.timestampMs=ms;
            sample.intervalMs=lastPresentMs_>=0?double(ms-lastPresentMs_):0.0;
            sample.renderMs=lastCompletedRenderMs_;
            sample.pendingRequests=unsigned(pacingQueued_);
            if(pacingSamples_.size()>=pacingCapacity_)
                pacingSamples_.erase(pacingSamples_.begin());
            pacingSamples_.push_back(sample);
            const double interval=lastPresentMs_>=0?double(ms-lastPresentMs_):0.0;
            if(interval>=1.0){
                const double fps=1000.0/interval;
                fpsFiltered_=fpsFiltered_==0.0?fps:(fpsFiltered_*.72+fps*.28);
                canvas_->setFpsOverlay(fpsFiltered_,true);
            }
        }
        lastPresentMs_=ms;
    }
    void exportPacing(){
        if(pacingSamples_.empty()){
            QMessageBox::information(this,QStringLiteral("Benchmark fluidité"),
                QStringLiteral("Démarrer l'exploration et se déplacer avant d'exporter."));
            return;
        }
        const QString root=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            +QStringLiteral("/CSE WARP/Benchmarks");
        QDir().mkpath(root);
        const QString name=QStringLiteral("V17.%1-PACING-").arg(CSE_WARP_BUILD_NUMBER)
            +QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
        const auto stats=cse::pacing::summarize(pacingSamples_,pacingRejected_,pacingQueued_);
        QJsonArray records;
        QString csv=QStringLiteral("timestamp_ms,present_interval_ms,render_ms,requests_queued\n");
        for(const auto& sample:pacingSamples_){
            records.push_back(QJsonObject{
                {"timestampMs",double(sample.timestampMs)},
                {"presentIntervalMs",sample.intervalMs},
                {"renderMs",sample.renderMs},
                {"queuedRequests",int(sample.pendingRequests)}
            });
            csv+=QStringLiteral("%1,%2,%3,%4\n")
                .arg(sample.timestampMs).arg(sample.intervalMs,0,'f',3)
                .arg(sample.renderMs,0,'f',3).arg(sample.pendingRequests);
        }
        const QJsonObject summary{
            {"version",QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER)},
            {"scene",scenePath_},
            {"backend",backendBox_->currentText()},
            {"frameCount",int(stats.frames)},
            {"medianPresentMs",stats.medianPresentMs},
            {"meanPresentMs",stats.meanPresentMs},
            {"p95PresentMs",stats.p95PresentMs},
            {"p99PresentMs",stats.p99PresentMs},
            {"worstPresentMs",stats.worstPresentMs},
            {"medianRenderMs",stats.medianRenderMs},
            {"stutterPercent",stats.stutterPercent},
            {"queuedRequests",double(stats.queued)},
            {"invalidatedFrames",double(stats.invalidated)},
            {"zPrepass",qEnvironmentVariable("M3D_V17_EXPERIMENT_Z_PREPASS")==QStringLiteral("1")},
            {"lightAabbCull",qEnvironmentVariable("M3D_V17_EXPERIMENT_LIGHT_AABB_CULL")==QStringLiteral("1")}
        };
        QFile jsonFile(root+QStringLiteral("/")+name+QStringLiteral(".json"));
        QFile csvFile(root+QStringLiteral("/")+name+QStringLiteral(".csv"));
        if(!jsonFile.open(QIODevice::WriteOnly) || !csvFile.open(QIODevice::WriteOnly)){
            QMessageBox::warning(this,QStringLiteral("Benchmark"),QStringLiteral("Impossible d'écrire les résultats."));
            return;
        }
        jsonFile.write(QJsonDocument(QJsonObject{{"summary",summary},{"frames",records}}).toJson());
        csvFile.write(csv.toUtf8());
        statusBar()->showMessage(QStringLiteral("Benchmark fluidité exporté : %1").arg(root),12000);
    }
    void requestRender(){
        if(sceneLoading_||!runtime_||runtime_->cameras.isEmpty())return;
        const auto serial=renderSerial_;
        if(watcher_.isRunning()){
            pending_=true; // Camera movement must not invalidate a finished image.
            ++pacingQueued_;
            return;
        }
        const uint64_t token=serial->fetch_add(1)+1;
        auto snapshot=runtime_;
        auto preset=activeCameraPose();
        updateCameraHud();
        QSize renderSize=qualityBox_->currentData().toSize();
        if(!renderSize.isValid() || renderSize.isEmpty()) {
            // Render to the viewport's *actual* aspect ratio, not the monitor
            // resolution; that used to letterbox the 3D image in a tiny strip.
            // Keep a bounded pixel budget on integrated GPUs.
            renderSize=canvas_->size().boundedTo(QSize(1600,900));
            renderSize.setWidth(std::max(64,renderSize.width()));
            renderSize.setHeight(std::max(64,renderSize.height()));
        }
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
            result.cameraPose=preset;
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
                    if(ok){
                        result.renderer=QStringLiteral("V17 D3D11 Raster — géométrie matérielle");
                        if(rasterHybridLightingEnabled())
                            result.renderer+=rasterGiCacheLoaded()
                              ?QStringLiteral(" — GI transport pré-calculée")
                              :QStringLiteral(" — sondes approximatives (sans table GI)");
                    }
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
        // The clean package always opens the latest validated V8/V440 scene.
        // Explicit CLI scene arguments still override the default.
        app+QStringLiteral("/benchmark-civic/scene.json"),
        app+QStringLiteral("/benchmark-civic/scene-v440-audit.json"),
        app+QStringLiteral("/scenes/v16/benchmark-civic/scene-v4.json"),
        QDir::currentPath()+QStringLiteral("/scenes/v16/benchmark-civic/scene-v4.json"),
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
    // Separate benchmark utilities need access to Qt and the V16 renderer
    // DLL in this installation root, even when launched from a subdirectory.
    const QByteArray installRoot=QCoreApplication::applicationDirPath().toLocal8Bit();
    qputenv("PATH",installRoot+";"+qgetenv("PATH"));
    qputenv("QT_PLUGIN_PATH",installRoot);
    // GUI animation is intentionally opt-in; offline GPU benchmarks remain
    // deterministic for strict RGB parity and regression comparisons.
    qputenv("M3D_V17_ANIMATE_CLOUDS","1");
    QCoreApplication::setApplicationName(QStringLiteral("CSE WARP Beta X V17.%1")
        .arg(CSE_WARP_BUILD_NUMBER));
    QCoreApplication::setApplicationVersion(QStringLiteral("17.%1")
        .arg(CSE_WARP_BUILD_NUMBER));
    const bool switchSmoke=argc>=4
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-scene-switch");
    const bool repeatSmoke=argc>=3
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-repeat-raster");
    const bool walkingSmoke=argc>=3
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-walking-collisions");
    const bool scopeSmoke=argc>=3
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-scope-mouse");
    const bool immersiveSmoke=argc>=3
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-immersive-mouse");
    // Use the same Windows V17 brand icon for Qt titlebar/Alt-Tab/taskbar.
    // The ICO is also embedded in the EXE's native Win32 resources.
    const QString iconPath=QCoreApplication::applicationDirPath()
        +QStringLiteral("/cse_warp_v17.ico");
    if(QFileInfo::exists(iconPath))app.setWindowIcon(QIcon(iconPath));
    const QString path=switchSmoke||repeatSmoke||walkingSmoke||scopeSmoke||immersiveSmoke
        ?QString::fromLocal8Bit(argv[2])
        :(argc>1?QString::fromLocal8Bit(argv[1]):defaultPath());
    // The smoke test uses reduced texture resolution to stay viable on
    // CI's software-only WARP device. Normal V17 keeps full 2048 PBR.
    V17Window window(path,(switchSmoke||repeatSmoke||walkingSmoke||scopeSmoke||immersiveSmoke)?256:2048);
    if(switchSmoke)window.smokeSceneSwitch(QString::fromLocal8Bit(argv[3]));
    if(repeatSmoke)window.smokeRepeatRaster();
    if(walkingSmoke)window.smokeWalkingCollisionPreference();
    if(scopeSmoke)window.smokeScopedAim();
    if(immersiveSmoke)window.smokeImmersiveMouse();
    if(!app.windowIcon().isNull())window.setWindowIcon(app.windowIcon());
    window.show();
    return app.exec();
}
