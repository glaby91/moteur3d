Warning: truncated output (original token count: 50334)
Total output lines: 3759

#include "m3d_engine.h"
#include "src/m3d_walk_controller.h"
#include "exploration_motion.h"
#include "scope_zoom.h"
#include "camera_diagnostic.h"
#include "anomaly_catalog.h"
#include "frame_pacing.h"
#include "pool_ladder.h"
#include "weather/solar_cycle.h"
#include "weather/sky_compositor.h"
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
#include <QScreen>
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
#include <QStackedLayout>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <chrono>
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
#include <windows.h>
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
int systemMemoryLoadPercent(){
#if defined(_WIN32)
    MEMORYSTATUSEX state{}; state.dwLength=sizeof(state);
    return GlobalMemoryStatusEx(&state)?int(state.dwMemoryLoad):-1;
#else
    return -1;
#endif
}
struct Result {
    QImage image;
    v16::CameraPreset cameraPose;
    QString renderer;
    QString error;
    double milliseconds=0;
    bool gpu=false;
    bool skyCompositing=false; // DX11 sky pixels are transparent for Qt compositor
    bool directPresented=false; // DXGI or Vulkan native swapchain, no QImage
    bool nativeFallback=false; // Native DXGI/WSI unavailable: show the QImage preview
    QString nativeFailure; // Preserve exact Vulkan/DXGI presentation error after image fallback
    QString nativeWindowDetails; // HWND, owning thread, client size and requested extent
    QString requestStamp;
    // Reject finished frames after a camera, material, backend or scene change.
    uint64_t generation=0;
    int backendIndex=-1;
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
    explicit ImageView(QWidget* owner=nullptr):QWidget(owner){}
    void setNativePresentation(bool enabled){
        if(nativePresentation_==enabled)return;
        nativePresentation_=enabled;
        if(enabled)paintPending_=false;
        setAttribute(Qt::WA_OpaquePaintEvent,enabled);
        setAttribute(Qt::WA_NoSystemBackground,enabled);
        if(!enabled)update();
    }
    bool nativePresentation()const{return nativePresentation_;}
    void setImage(QImage im){image_=std::move(im);paintPending_=true;scheduleQtPaint();}
    void enableSkyCompositing(bool enabled){
        skyCompositing_=enabled;
        if(!enabled)skyImage_=QImage{};
        else skyDirty_=true;
        scheduleQtPaint();
    }
    bool hasSkyFrame()const{return !skyImage_.isNull()&&!skyDirty_;}
    void invalidateSky(){skyDirty_=true;}
    void drawSky(const m3d::Camera& camera,float hour,double seconds,
                 bool cloudsEnabled,bool fastWeather){
        if(!skyCompositing_)return;
        skyComposer_.render(skyImage_,camera,hour,seconds,cloudsEnabled,fastWeather);
        skyDirty_=false;
        scheduleQtPaint();
    }
    void onPresented(std::function<void()> callback){presented_=std::move(callback);}
    void setPlaceholder(const QString& message){placeholder_=message;scheduleQtPaint();}
    void setRendererStamp(QString displayed,QString requested){
        if(displayedRenderer_==displayed&&requestedRenderer_==requested)return;
        displayedRenderer_=std::move(displayed);requestedRenderer_=std::move(requested);scheduleQtPaint();
    }
    void setCameraHud(QStringList lines,bool enabled){
        if(hudLines_==lines&&hudEnabled_==enabled)return;
        hudLines_=std::move(lines);hudEnabled_=enabled;scheduleQtPaint();
    }
    void setScopeOverlay(float level){
        level=std::clamp(level,0.f,1.f);
        if(std::abs(scopeLevel_-level)<.002f)return;
        scopeLevel_=level;
        scheduleQtPaint();
    }
    void setFpsOverlay(double fps,bool visible){
        const QString next=visible&&fps>0.0
            ?QStringLiteral("%1 FPS").arg(fps,0,'f',1)
            :QStringLiteral("FPS —");
        if(fpsVisible_==visible&&fpsText_==next)return;
        fpsVisible_=visible;
        fpsText_=next;
        scheduleQtPaint();
    }
    static void drawCameraHud(QPainter& painter,const QRect& bounds,const QStringList& lines,
                              int rightInset=0){
        if(lines.isEmpty()||bounds.width()<240||bounds.height()<110)return;
        painter.save();
        QFont font=painter.font();font.setFamily(QStringLiteral("Consolas"));font.setPointSize(9);
        painter.setFont(font);
        const QFontMetrics fm(font);
        const int pad=10;
        const QRect panel(bounds.left()+8,bounds.top()+8,
                          std::max(120,bounds.width()-16-std::max(0,rightInset)),
                          fm.height()+pad*2);
        painter.setPen(Qt::NoPen);painter.setBrush(QColor(10,18,27,208));
        painter.drawRoundedRect(panel,6,6);
        painter.setPen(QColor(226,239,247));
        painter.drawText(panel.adjusted(pad,0,-pad,0),Qt::AlignVCenter|Qt::AlignLeft,
            fm.elidedText(lines.join(QStringLiteral("   |   ")),Qt::ElideRight,
                          panel.width()-pad*2));
        painter.restore();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        // In direct mode this top-level HWND belongs to DXGI or Vulkan WSI.
        // Qt must not repaint over a swapchain present.
        if(nativePresentation_)return;
        QPainter p(this);
        p.fillRect(rect(),QColor(22,26,31));
        if(image_.isNull()) {
            p.setPen(QColor(180,191,204));
            p.drawText(rect().adjusted(30,20,-30,-20),Qt::AlignCenter|Qt::TextWordWrap,
                       placeholder_.isEmpty()?QStringLiteral("Ouvrir une scène V16 dans le menu Fichier")
                                             :placeholder_);
            drawRendererStamp(p,rect());
            return;
        }
        QSize s=image_.size();
        s.scale(size(),Qt::KeepAspectRatio);
        QRect dst(QPoint((width()-s.width())/2,(height()-s.height())/2),s);
        p.setRenderHint(QPainter::SmoothPixmapTransform,false);
        if(skyCompositing_ && !skyImage_.isNull()) {
            // The 512x288 independent weather layer is updated at 30Hz.
            // The DX11 architectural foreground stays at its last good frame.
            p.setRenderHint(QPainter::SmoothPixmapTransform,true);
            p.drawImage(dst,skyImage_);
            p.setRenderHint(QPainter::SmoothPixmapTransform,false);
        }
        p.drawImage(dst,image_);
        drawRendererStamp(p,dst);
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
        if(hudEnabled_)drawCameraHud(p,dst,hudLines_,fpsVisible_?105:0);
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
    // In native WSI mode Qt does not own the pixels in this HWND. Camera HUD,
    // FPS and renderer-stamp changes must not schedule a Qt repaint that can
    // clear the just-presented Vulkan swapchain image to the widget background.
    void scheduleQtPaint(){if(!nativePresentation_)update();}
    QImage image_;
    QImage skyImage_;
    cse::weather::SkyCompositor skyComposer_;
    bool skyCompositing_=false,skyDirty_=true;
    bool paintPending_=false;
    bool nativePresentation_=false;
    std::function<void()> presented_;
    QString placeholder_;
    QStringList hudLines_;
    bool hudEnabled_=true;
    float scopeLevel_=0.f;
    bool fpsVisible_=false;
    QString fpsText_;
    QString displayedRenderer_;
    QString requestedRenderer_;
    void drawRendererStamp(QPainter& p,const QRect& bounds){
        if(bounds.width()<240||bounds.height()<80)return;
        QStringList lines;
        lines<<QStringLiteral("Affiché : %1").arg(displayedRenderer_.isEmpty()
            ?QStringLiteral("aucun rendu pour l’instant"):displayedRenderer_);
        if(!requestedRenderer_.isEmpty())
            lines<<QStringLiteral("Demandé : %1").arg(requestedRenderer_);
        p.save();
        QFont f=p.font();f.setFamily(QStringLiteral("Consolas"));f.setPointSize(9);f.setBold(true);p.setFont(f);
        const QFontMetrics fm(f);int longest=0;
        for(const auto& line:lines)longest=std::max(longest,fm.horizontalAdvance(line));
        const int pad=9,lh=fm.height()+3;
        const int panelWidth=std::min(bounds.width()-24,longest+pad*2);
        const int panelHeight=lh*lines.size()+pad*2;
        const QRect panel(bounds.left()+12,bounds.bottom()-panelHeight-12,panelWidth,panelHeight);
        p.setPen(Qt::NoPen);p.setBrush(QColor(9,15,22,220));p.drawRoundedRect(panel,6,6);
        for(int i=0;i<lines.size();++i){
            p.setPen(i==0?QColor(143,232,255):QColor(255,218,132));
            p.drawText(panel.left()+pad,panel.top()+pad+fm.ascent()+i*lh,
                       fm.elidedText(lines[i],Qt::ElideRight,panel.width()-pad*2));
        }
        p.restore();
    }
};
class V17Window final : public QMainWindow {
public:
    explicit V17Window(QString scenePath,int textureResolution=2048)
        :textureResolution_(textureResolution){
        setWindowTitle(QStringLiteral("CSE WARP Beta X — V17.%1 Exploration")
            .arg(CSE_WARP_BUILD_NUMBER));
        resize(1510,900);
        setMinimumSize(1100,670);
        // Every backend gets its own independent top-level HWND. Direct DX11
        // and Vulkan present into their own ImageView HWND; CPU/compute/QImage
        // modes paint into the matching window through the same view class.
        // None of the render targets is embedded in the controls window.
        for(int i=0;i<int(backendWindows_.size());++i){
            // No owner window: Windows must allow the controls and each
            // backend output to be raised independently.
            auto* view=new ImageView;
            view->setWindowFlag(Qt::Window,true);
            view->setAttribute(Qt::WA_NativeWindow);
            view->setFocusPolicy(Qt::StrongFocus);
            view->setMouseTracking(true);
            view->resize(1000,680);
            view->move(pos()+QPoint(36,70));
            view->setWindowTitle(QStringLiteral("CSE WARP — rendu dédié — backend %1")
                .arg(i+1));
            view->installEventFilter(this);
            view->onPresented([this]{recordImagePresented();});
            backendWindows_[size_t(i)]=view;
        }
        canvas_=backendWindows_[4];
        nativeRendererOverlay_=new QLabel(this,
            Qt::Tool|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus);
        nativeRendererOverlay_->setObjectName(QStringLiteral("v17NativeRendererStamp"));
        nativeRendererOverlay_->setGeometry(12,12,470,34);
        nativeRendererOverlay_->setWordWrap(false);
        nativeRendererOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
        nativeRendererOverlay_->setStyleSheet(QStringLiteral(
            "QLabel { color: #dff7ff; background: rgba(9,15,22,220); "
            "border-radius: 6px; padding: 7px; font: bold 9pt \"Consolas\"; }"));
        nativeRendererOverlay_->raise();
        auto* controlsPlaceholder=new QLabel(
            QStringLiteral("Chaque backend possède sa fenêtre de rendu dédiée. "
                           "Sélectionnez un moteur puis actualisez la scène."),this);
        controlsPlaceholder->setAlignment(Qt::AlignCenter);
        setCentralWidget(controlsPlaceholder);
        frameTimer_.setInterval(16);
        connect(&frameTimer_, &QTimer::timeout, this, [this]{ frameTick(); });
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
        const QStringList args=QCoreApplication::arguments();
        if(args.contains(QStringLiteral("--smoke-scene-switch")) ||
           args.contains(QStringLiteral("--smoke-repeat-raster")) ||
           args.contains(QStringLiteral("--smoke-walking-collisions")) ||
           args.contains(QStringLiteral("--smoke-scope-mouse")) ||
           args.contains(QStringLiteral("--smoke-immersive-mouse"))){
            const QSignalBlocker blocked(directDisplayBox_);
            directDisplayBox_->setChecked(false);
        }
        selectBackendOutputWindow();
        // A stationary camera must always display the native full-resolution
        // raster, regardless of the experimental launcher environment.
        peripheralExperimentEnabled_=
            qEnvironmentVariable("M3D_V17_PERIPHERAL_MULTIREZ")==QStringLiteral("1");
        setRasterPeripheralMultiresOverride(0);
        pacingClock_.start();
        skyMotionClock_.start();
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
        nativeHudLabel_=new QLabel(QStringLiteral("CAM —"),this);
        nativeHudLabel_->setMinimumWidth(215);
        nativeHudLabel_->hide(); // Camera details belong in the render-window HUD.
        nativeFpsLabel_=new QLabel(QStringLiteral("FPS —"),this);
        nativeFpsLabel_->hide(); // Avoid duplicating the render-window FPS badge.
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
            const bool nativePresentationFailed=!outcome.nativeFailure.isEmpty();
            if(nativePresentationFailed){
                // Retain failures even if the user switched backends while
                // the native WSI call was still in flight.
                const int failedBackend=outcome.backendIndex;
                if(failedBackend>=0&&failedBackend<int(nativeDirectFailed_.size())){
                    nativeDirectFailed_[size_t(failedBackend)]=true;
                    nativeDirectFailureReason_[size_t(failedBackend)]=outcome.nativeFailure;
                }
                const QString backend=failedBackend==5
                    ?QStringLiteral("Vulkan"):QStringLiteral("DX11");
                const QString message=QStringLiteral(
                    "%1 direct a échoué : %2\n%3")
                    .arg(backend,outcome.nativeFailure,outcome.nativeWindowDetails);
                qWarning().noquote()<<message;
                saveNativePresentationDiagnostic(backend,outcome.nativeFailure,
                                                  outcome.nativeWindowDetails);
            }
            if(outcome.backendIndex!=backendBox_->currentIndex()){
                // A queued frame belongs to its original backend HWND. Never
                // paint or report it as if it came from the newly selected one.
                if(pending_){pending_=false;requestRender();}
                return;
            }
            if(outcome.error.isEmpty()&&!outcome.nativeFallback&&
               outcome.directPresented!=nativeDirectSelected()){
                // Direct-display was toggled while this frame was in flight.
                // Keep the frame's original HWND mode until completion, then
                // immediately render again using the newly requested mode.
                pending_=false;
                requestRender();
                return;
            }
            if(nativePresentationFailed){
                // Keep the checked request, but stop retrying the unusable
                // swapchain every camera frame. Toggling off/on retries it.
                selectBackendOutputWindow();
            }
            if(outcome.error.isEmpty()&&(outcome.directPresented||!outcome.image.isNull())){
                lastPresentedRenderer_=outcome.renderer;
                if(!lastPresentedRenderer_.contains(QStringLiteral("(swapchain)"))&&
                   !lastPresentedRenderer_.contains(QStringLiteral("(QImage)")))
                    lastPresentedRenderer_+=outcome.directPresented
                        ?QStringLiteral(" — Direct GPU (swapchain)")
                        :QStringLiteral(" — affichage Qt (QImage)");
                backendLastRenderer_[size_t(outcome.backendIndex)]=lastPresentedRenderer_;
                backendLastRequest_[size_t(outcome.backendIndex)]=nativePresentationFailed
                    ?requestedRendererStamp():outcome.requestStamp;
                updateRendererStamp();
            }
            if(outcome.directPresented){
                // Vulkan submitted a swapchain image directly. No hidden
                // QImage and no per-frame GPU-to-CPU readback.
                lastRenderedPose_=outcome.cameraPose;
                hasRenderedPose_=true;
                lastCompletedRenderMs_=outcome.milliseconds;
                recordImagePresented(); // count submitted/presented frames
            }
            if(!outcome.image.isNull()){
                lastImage_=outcome.image;
                lastRenderedPose_=outcome.cameraPose;
                hasRenderedPose_=true;
                lastCompletedRenderMs_=outcome.milliseconds;
                canvas_->setImage(outcome.image);
                canvas_->enableSkyCompositing(outcome.skyCompositing);
            }
            if(outcome.error.isEmpty()&&
               (outcome.directPresented||!outcome.image.isNull())){
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
            const int completedBackend=outcome.backendIndex;
            const bool directFallbackActive=directDisplayBox_&&directDisplayBox_->isChecked()&&
                completedBackend>=0&&completedBackend<int(nativeDirectFailed_.size())&&
                nativeDirectFailed_[size_t(completedBackend)];
            if(nativePresentationFailed||directFallbackActive){
                const QString backend=completedBackend==5
                    ?QStringLiteral("Vulkan"):QStringLiteral("DX11");
                const QString detail=completedBackend>=0&&
                        completedBackend<int(nativeDirectFailureReason_.size())&&
                        !nativeDirectFailureReason_[size_t(completedBackend)].isEmpty()
                    ?nativeDirectFailureReason_[size_t(completedBackend)]
                    :QStringLiteral("cause native non disponible");
                const QString recovery=outcome.image.isNull()
                    ?QStringLiteral("QImage de secours également indisponible")
                    :QStringLiteral("QImage actif après échec direct");
                statusBar()->showMessage(QStringLiteral(
                    "%1 — %2 : %3. Diagnostic : %4. Décochez puis recochez pour retenter.")
                    .arg(backend,recovery,detail,nativeDiagnosticLogPath()),30000);
            }else statusBar()->showMessage(outcome.error.isEmpty()
                ?QStringLiteral("%1 — %2 ms")
                      .arg(outcome.renderer).arg(outcome.milliseconds,0,'f',1)
                :outcome.error);
            if(exploring_ && outcome.error.isEmpty() &&
               (outcome.directPresented||!outcome.image.isNull())){
                if(frameClock_.isValid()){
                    const qint64 frameMs=frameClock_.elapsed();
                    if(frameMs>0)renderTimeLabel_->setText(
                        QStringLiteral("%1 ms · %2 FPS")
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
    // UI contract smoke: each renderer owns a unique top-level HWND.
    void smokeNativeUi(){
        QTimer::singleShot(0,this,[this]{
#if defined(_WIN32)
            auto* quick=findChild<QComboBox*>(QStringLiteral("v17QuickRenderer"));
            QSet<quintptr> handles;
            bool allWindows=true;
            for(auto* view:backendWindows_){
                if(!view||!view->isWindow())allWindows=false;
                else handles.insert(quintptr(view->winId()));
            }
            const bool dx11Ready=backendBox_ && backendBox_->count()==6 &&
                backendWindows_[4] && backendWindows_[4]->isWindow() &&
                backendBox_->currentIndex()==4 && directDisplayBox_->isChecked() &&
                backendBox_->itemText(5).contains(QStringLiteral("Vulkan")) &&
                (backendBox_->model()->flags(backendBox_->model()->index(5,0)) &
                    Qt::ItemIsEnabled) &&
                quick && quick->count()==6 &&
                quick->itemText(5).contains(QStringLiteral("Vulkan")) &&
                canvas_==backendWindows_[4] && canvas_->testAttribute(Qt::WA_NativeWindow);
            backendBox_->setCurrentIndex(5);
            const bool vulkanVisiblePath=nativeDirectSelected() &&
                directDisplayBox_->isEnabled() &&
                canvas_==backendWindows_[5] && canvas_!=backendWindows_[4] &&
                backendWindows_[5]->isVisible() && !backendWindows_[4]->isVisible();
            backendBox_->setCurrentIndex(4);
            selectBackendOutputWindow();
            const bool ok=dx11Ready&&vulkanVisiblePath&&allWindows&&handles.size()==6;
            std::fprintf(stderr,"V17 BACKEND WINDOWS: %s, six unique top-level HWNDs\n",
                ok?"PASS":"FAIL");
            QCoreApplication::exit(ok?0:46);
#else
            QCoreApplication::exit(0);
#endif
        });
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
            const bool full=canvas_->isFullScreen();
            immersiveAction_->setChecked(false);
            QCoreApplication::processEvents();
            const bool back=!canvas_->isFullScreen()
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
        // Drain both producers before destroying backend output windows. Their
        // Win32 HWNDs must outlive DXGI swapchains and VkSurfaceKHR objects.
        loadWatcher_.waitForFinished();
        watcher_.waitForFinished();
#if defined(_WIN32)
        // Raster backends keep thread_local Vulkan/D3D11 contexts in the
        // dedicated one-worker pool. Destroy them on their owning thread
        // BEFORE their dedicated top-level HWNDs are destroyed.
        // This also covers closing directly after the first native frame.
        auto cleanup=QtConcurrent::run(&renderPool_,[]{
            releaseDirectX11NativeForCurrentThread();
            releaseVulkanDirectForCurrentThread();
            releaseVulkanPreviewForCurrentThread();
        });
        cleanup.waitForFinished();
#endif
        for(auto* view:backendWindows_){
            if(view){
                view->setNativePresentation(false);
                delete view;
            }
        }
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
    std::array<ImageView*,6> backendWindows_{};
    std::array<QString,6> backendLastRenderer_{};
    std::array<QString,6> backendLastRequest_{};
    std::array<bool,6> nativeDirectFailed_{};
    std::array<QString,6> nativeDirectFailureReason_{};
    int outputBackendIndex_=-1;
    int activeRenderBackend_=-1;
    bool activeRenderDirect_=false;
    QLabel* nativeHudLabel_=nullptr;
    QLabel* nativeFpsLabel_=nullptr;
    QLabel* nativeRendererOverlay_=nullptr;
    QString lastPresentedRenderer_;
    QCheckBox* directDisplayBox_=nullptr;
    bool nativeReadyRetryQueued_=false;
    int nativeReadyAttempts_=0;
    QDockWidget* sceneExplorerDock_=nullptr;
    QDockWidget* renderControlsDock_=nullptr;
    QDockWidget* anomalyReviewDock_=nullptr;
    QRect explorationSavedMainGeometry_;
    QRect explorationSavedRenderGeometry_;
    QSize explorationSavedMinimumSize_;
    bool explorationSavedSceneDockVisible_=false;
    bool explorationSavedControlsDockVisible_=false;
    bool explorationSavedReviewDockVisible_=false;
    bool explorationSavedMenuVisible_=true;
    bool explorationSavedMainMaximized_=false;
    bool explorationSavedMainFullScreen_=false;
    bool explorationSavedRenderMaximized_=false;
    bool explorationSavedRenderFullScreen_=false;
    bool explorationWindowLayoutActive_=false;
    QTimer frameTimer_;
    QTimer skyAnimationTimer_;
    QElapsedTimer skyMotionClock_;
    qint64 lastVulkanIdleSkyRequestMs_=-1000;
    QTimer dayNightTimer_;
    QElapsedTimer dayNightClock_;
    double dayNightOriginHour_=14.75;
    bool applyingDayNightTick_=false;
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
        if(nativeHudLabel_)nativeHudLabel_->setText(
            QStringLiteral("CAM X:%1 Y:%2 Z:%3")
                .arg(pose.eye.x,0,'f',1).arg(pose.eye.y,0,'f',1).arg(pose.eye.z,0,'f',1));
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
        if(!exploring_||mouseCaptured_||!canvas_||
           (!isActiveWindow()&&!canvas_->isActiveWindow()))return;
        mouseCaptured_=true;
        canvas_->setCursor(Qt::BlankCursor);
        focusRenderViewport();
        QCursor::setPos(canvas_->mapToGlobal(canvas_->rect().center()));
    }
    void releaseMouseLook(){
        if(!mouseCaptured_)return;
        mouseCaptured_=false;
        if(canvas_)canvas_->unsetCursor();
        for(auto* view:backendWindows_)if(view)view->unsetCursor();
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
        if(nativeFpsLabel_)nativeFpsLabel_->setText(QStringLiteral("FPS —"));
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
        if(!closingRequested_)restoreExplorationWindows();
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
        if(!supportsExplorationBackend(backendBox_->currentIndex()))
            backendBox_->setCurrentIndex(4);
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
        if(walkingModeBox_&&!walkin…20334 tokens truncated…rString());return;
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
        lastImage_=QImage{};
        hasRenderedPose_=false;
        for(auto* view:backendWindows_){
            if(!view)continue;
            // A native-present worker cannot be interrupted mid-Present. Keep
            // Qt from painting over its HWND until that frame leaves the GPU.
            const bool preserveInFlightPresent=watcher_.isRunning()&&
                activeRenderDirect_&&activeRenderBackend_>=0&&
                activeRenderBackend_<int(backendWindows_.size())&&
                view==backendWindows_[size_t(activeRenderBackend_)];
            view->setNativePresentation(preserveInFlightPresent);
            view->setImage(QImage{});
            view->setCameraHud({},false);
            view->enableSkyCompositing(false);
            view->setPlaceholder(QStringLiteral("CSE WARP V17\nChargement de la scène…"));
        }
        backendLastRenderer_.fill(QString{});
        backendLastRequest_.fill(QString{});
        hasRenderedPose_=false;
        freeCameraActive_=false;
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
        canvas_->enableSkyCompositing(false);
        renderTimeLabel_->setText(QStringLiteral("Dernier rendu : —"));
        materialLabel_->setText(QStringLiteral("Sélectionner un matériau dans le catalogue"));
        const QString sceneRevision=outcome.database.revision.trimmed();
        if(sceneRevisionLabel_)sceneRevisionLabel_->setText(sceneRevision.isEmpty()?QStringLiteral("Scène —"):QStringLiteral("Scène %1").arg(sceneRevision));
        const QString revisionSuffix=sceneRevision.isEmpty()?QStringLiteral("Scène sans révision"):QStringLiteral("Scène %1").arg(sceneRevision);
        setWindowTitle(QStringLiteral("CSE WARP Beta X V17.%1 — %2 — %3")
            .arg(CSE_WARP_BUILD_NUMBER).arg(runtime_->title,revisionSuffix));
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
            // Full camera names remain in the side selector and tooltip;
            // the toolbar uses compact numbers so DX11 / Vulkan stay visible.
            quickCameraActions_[i]->setText(QString::number(i+1));
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
        // Explicit benchmark launcher override only: ordinary scene launches
        // keep their existing DX11 Raster default and user controls.
        const bool vulkanProfile=isStressScene &&
            qEnvironmentVariable("CSE_WARP_VK_PROFILE_MODE")==QStringLiteral("1");
#if defined(_WIN32) && M3D_V17_VULKAN_ENABLED
        if(vulkanProfile){
            directDisplayBox_->setChecked(true);
            backendBox_->setCurrentIndex(5); // Vulkan Raster, native WSI
            dayNightCycleBox_->setChecked(true); // exercise solar/shadow updates
        }else
#endif
        if(isStressScene)backendBox_->setCurrentIndex(4);
        sceneLoading_=false;
        selectBackendOutputWindow();
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

    bool vulkanDirectAvailable() const{
#if defined(_WIN32) && M3D_V17_VULKAN_ENABLED
        return true;
#else
        return false;
#endif
    }
    bool nativeDirectSelected() const{
#ifdef _WIN32
        // Each native backend presents into its own backend-specific top-level
        // ImageView HWND; DXGI and Vulkan never share a target window.
        if(!backendBox_||!directDisplayBox_||!directDisplayBox_->isChecked())return false;
        const int index=backendBox_->currentIndex();
        return (index==4||(index==5&&vulkanDirectAvailable()))&&
               !nativeDirectFailed_[size_t(index)];
#else
        return false;
#endif
    }
    void focusRenderViewport(){
        if(canvas_){
            canvas_->setFocus(Qt::OtherFocusReason);
            canvas_->raise();
            canvas_->activateWindow();
        }
    }
    void restoreExplorationViewportFocus(){
        if(!exploring_)return;
        // Changing the backend or direct-display mode moves focus between
        // independent top-level views. Rebind mouse-look after Qt activates
        // the newly selected output window.
        releaseMouseLook();
        QTimer::singleShot(0,this,[this]{
            if(!exploring_||closingRequested_)return;
            focusRenderViewport();
            captureMouseLook();
        });
    }
    void arrangeExplorationWindows(){
        if(explorationWindowLayoutActive_||!canvas_)return;
        QScreen* screen=QGuiApplication::screenAt(frameGeometry().center());
        if(!screen)screen=QGuiApplication::primaryScreen();
        if(!screen)return;
        const QRect area=screen->availableGeometry();
        if(area.width()<900||area.height()<500)return;

        explorationSavedMainGeometry_=geometry();
        explorationSavedRenderGeometry_=canvas_->geometry();
        explorationSavedMinimumSize_=minimumSize();
        explorationSavedMainMaximized_=isMaximized();
        explorationSavedMainFullScreen_=isFullScreen();
        explorationSavedRenderMaximized_=canvas_->isMaximized();
        explorationSavedRenderFullScreen_=canvas_->isFullScreen();
        explorationSavedSceneDockVisible_=sceneExplorerDock_&&sceneExplorerDock_->isVisible();
        explorationSavedControlsDockVisible_=renderControlsDock_&&renderControlsDock_->isVisible();
        explorationSavedReviewDockVisible_=anomalyReviewDock_&&anomalyReviewDock_->isVisible();
        explorationSavedMenuVisible_=menuBar()&&menuBar()->isVisible();
        explorationWindowLayoutActive_=true;

        // Compact the controls into the right-hand band; hide secondary panels
        // and keep the renderer in the larger left window.
        if(sceneExplorerDock_)sceneExplorerDock_->hide();
        if(anomalyReviewDock_)anomalyReviewDock_->hide();
        if(renderControlsDock_){
            renderControlsDock_->show();
            renderControlsDock_->raise();
        }
        if(centralWidget())centralWidget()->hide();
        if(menuBar())menuBar()->hide();
        setMinimumSize(360,500);
        const int bandWidth=std::clamp(area.width()/4,380,480);
        const int renderWidth=area.width()-bandWidth;
        const QRect renderRect(area.left(),area.top(),renderWidth,area.height());
        const QRect bandRect(area.left()+renderWidth,area.top(),bandWidth,area.height());
        // setGeometry() does not leave Qt's maximized state. Normalize both
        // top-level windows first so the requested left/right layout takes effect.
        if(canvas_->isMinimized()||canvas_->isMaximized()||canvas_->isFullScreen())
            canvas_->showNormal();
        canvas_->setGeometry(renderRect);
        if(isMinimized()||isMaximized()||isFullScreen())showNormal();
        setGeometry(bandRect);
        show();
        raise();
        canvas_->show();
        focusRenderViewport();
        updateRendererStamp();
        QTimer::singleShot(0,this,[this]{
            if(exploring_&&!closingRequested_){
                focusRenderViewport();
                captureMouseLook();
            }
        });
    }
    void restoreExplorationWindows(){
        if(!explorationWindowLayoutActive_)return;
        explorationWindowLayoutActive_=false;
        setMinimumSize(explorationSavedMinimumSize_);
        if(centralWidget())centralWidget()->show();
        if(menuBar())menuBar()->setVisible(explorationSavedMenuVisible_);
        if(sceneExplorerDock_)sceneExplorerDock_->setVisible(explorationSavedSceneDockVisible_);
        if(renderControlsDock_)renderControlsDock_->setVisible(explorationSavedControlsDockVisible_);
        if(anomalyReviewDock_)anomalyReviewDock_->setVisible(explorationSavedReviewDockVisible_);
        if(!explorationSavedMainGeometry_.isEmpty())setGeometry(explorationSavedMainGeometry_);
        if(canvas_&&!explorationSavedRenderGeometry_.isEmpty())
            canvas_->setGeometry(explorationSavedRenderGeometry_);
        if(explorationSavedMainFullScreen_)showFullScreen();
        else if(explorationSavedMainMaximized_)showMaximized();
        if(canvas_){
            if(explorationSavedRenderFullScreen_)canvas_->showFullScreen();
            else if(explorationSavedRenderMaximized_)canvas_->showMaximized();
        }
        raise();
        activateWindow();
        updateRendererStamp();
    }
    QString describeNativeWindow(quintptr nativeHandle,QSize requested,qreal dpr) const{
#if defined(_WIN32)
        const HWND hwnd=reinterpret_cast<HWND>(nativeHandle);
        DWORD ownerProcess=0;
        const DWORD ownerThread=hwnd?GetWindowThreadProcessId(hwnd,&ownerProcess):0;
        RECT client{};
        const bool hasClient=hwnd&&GetClientRect(hwnd,&client);
        wchar_t className[128]{};
        const int classLength=hwnd?GetClassNameW(hwnd,className,128):0;
        const QString classText=classLength>0
            ?QString::fromWCharArray(className,classLength):QStringLiteral("<indisponible>");
        const auto hinstance=hwnd?GetWindowLongPtrW(hwnd,GWLP_HINSTANCE):0;
        const auto style=hwnd?GetWindowLongPtrW(hwnd,GWL_STYLE):0;
        const auto exStyle=hwnd?GetWindowLongPtrW(hwnd,GWL_EXSTYLE):0;
        return QStringLiteral(
            "HWND=0x%1 IsWindow=%2 ownerThread=%3 process=%4 HINSTANCE=0x%5 "
            "style=0x%6 exStyle=0x%7 class=%8 client=%9x%10 target=%11x%12 DPR=%13")
            .arg(QString::number(qulonglong(nativeHandle),16))
            .arg(hwnd&&IsWindow(hwnd)?1:0)
            .arg(ownerThread).arg(ownerProcess)
            .arg(QString::number(qulonglong(hinstance),16))
            .arg(QString::number(qulonglong(style),16))
            .arg(QString::number(qulonglong(exStyle),16))
            .arg(classText)
            .arg(hasClient?client.right-client.left:0)
            .arg(hasClient?client.bottom-client.top:0)
            .arg(requested.width()).arg(requested.height())
            .arg(dpr,0,'f',3);
#else
        Q_UNUSED(nativeHandle);
        return QStringLiteral("Win32 window diagnostics unavailable; target=%1x%2 DPR=%3")
            .arg(requested.width()).arg(requested.height()).arg(dpr,0,'f',3);
#endif
    }
    QString nativeDiagnosticLogPath() const{
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            +QStringLiteral("/CSE WARP/Diagnostics/V17.523-native-presentation.jsonl");
    }
    void saveNativePresentationDiagnostic(const QString& backend,
                                          const QString& reason,
                                          const QString& windowDetails){
        const QString path=nativeDiagnosticLogPath();
        const QString directory=QFileInfo(path).absolutePath();
        QDir().mkpath(directory);
        QFile file(path);
        if(file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){
            const QJsonObject record{
                {QStringLiteral("time"),QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
                {QStringLiteral("version"),QStringLiteral("17.523")},
                {QStringLiteral("backend"),backend},
                {QStringLiteral("reason"),reason},
                {QStringLiteral("window"),windowDetails}
            };
            file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
            file.write("\n");
        }
        const QString summary=reason.isEmpty()
            ?QStringLiteral("cause native non fournie par le backend")
            :reason;
        statusBar()->showMessage(QStringLiteral(
            "%1 direct indisponible ; QImage actif. %2 — diagnostic détaillé : %3. "
            "Décochez puis recochez l’option pour retenter.")
            .arg(backend,summary,path),30000);
    }
    QString requestedRendererStamp() const {
        if(!backendBox_)return QStringLiteral("Rendu en attente");
        const int index=backendBox_->currentIndex();
        QString name=backendBox_->currentText();
        if(index==4)name=QStringLiteral("V17 D3D11 Raster");
        else if(index==5)name=QStringLiteral("V17 Vulkan Raster");
        QSize requestedSize=qualityBox_?qualityBox_->currentData().toSize():QSize{};
        if(!requestedSize.isValid()||requestedSize.isEmpty()){
            requestedSize=canvas_?canvas_->size().boundedTo(QSize(1600,900)):QSize(1000,680);
            requestedSize.setWidth(std::max(64,requestedSize.width()));
            requestedSize.setHeight(std::max(64,requestedSize.height()));
        }
        const bool direct=(index==4||index==5)&&directDisplayBox_&&directDisplayBox_->isChecked();
        if(direct&&canvas_){
            const qreal dpr=canvas_->devicePixelRatioF();
            requestedSize=QSize(std::max(320,int(canvas_->width()*dpr)),
                                std::max(200,int(canvas_->height()*dpr)));
        }
        if(direct&&index>=0&&index<int(nativeDirectFailed_.size())&&
           nativeDirectFailed_[size_t(index)])
            name+=QStringLiteral(" — direct demandé, QImage actif après échec");
        else name+=direct?QStringLiteral(" — Direct GPU (swapchain)")
                         :QStringLiteral(" — affichage Qt (QImage)");
        name+=QStringLiteral(" — %1×%2 demandé").arg(requestedSize.width()).arg(requestedSize.height());
        return name;
    }
    void updateRendererStamp(){
        const QString requested=requestedRendererStamp();
        const int index=backendBox_?std::clamp(backendBox_->currentIndex(),0,
            int(backendWindows_.size())-1):0;
        const QString displayed=backendLastRenderer_[size_t(index)];
        const bool pending=requested!=backendLastRequest_[size_t(index)];
        if(canvas_)canvas_->setRendererStamp(displayed,pending?requested:QString{});
        if(nativeRendererOverlay_){
            const QString shown=displayed.isEmpty()
                ?QStringLiteral("Affiché : aucun rendu pour l’instant")
                :QStringLiteral("Affiché : %1").arg(displayed);
            const QString waiting=pending
                ?QStringLiteral("Demandé : %1").arg(requested):QString{};
            // Direct swapchain presentation bypasses ImageView::paintEvent,
            // including its camera HUD. Keep the live camera readout on the
            // native output window as well as in the controls status bar.
            const QString camera=cameraHudEnabled_&&nativeHudLabel_&&
                    !nativeHudLabel_->text().isEmpty()
                ?nativeHudLabel_->text():QString{};
            QStringList overlayLines;
            overlayLines<<shown;
            if(!waiting.isEmpty())overlayLines<<waiting;
            if(!camera.isEmpty())overlayLines<<camera;
            const QString overlay=overlayLines.join(QStringLiteral("   |   "));
            if(canvas_){
                nativeRendererOverlay_->move(canvas_->mapToGlobal(QPoint(12,12)));
                const int width=std::max(220,canvas_->width()-24);
                nativeRendererOverlay_->setFixedSize(width,34);
                const QFontMetrics metrics(nativeRendererOverlay_->font());
                nativeRendererOverlay_->setText(
                    metrics.elidedText(overlay,Qt::ElideRight,width-20));
            }
            nativeRendererOverlay_->setVisible(nativeDirectSelected()&&canvas_&&canvas_->isVisible());
            if(nativeDirectSelected()&&canvas_&&canvas_->isVisible())
                nativeRendererOverlay_->raise();
        }
    }
    void selectBackendOutputWindow(){
        if(!backendBox_)return;
        const int index=std::clamp(backendBox_->currentIndex(),0,
                                   int(backendWindows_.size())-1);
        const bool backendChanged=outputBackendIndex_!=index;
        const QRect explorationRenderGeometry=canvas_?canvas_->geometry():QRect{};
        outputBackendIndex_=index;
        canvas_=backendWindows_[size_t(index)];
        // Keep the native status overlay owned and stacked with the renderer
        // HWND, not with the controls window on the other side of the screen.
        if(nativeRendererOverlay_&&nativeRendererOverlay_->parentWidget()!=canvas_)
            nativeRendererOverlay_->setParent(canvas_,
                Qt::Tool|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus);
        canvas_->setWindowTitle(QStringLiteral("CSE WARP V17.%1 — %2 — fenêtre dédiée")
            .arg(CSE_WARP_BUILD_NUMBER).arg(backendBox_->itemText(index)));
        const bool direct=nativeDirectSelected();
        if(backendChanged||direct){
            // The global export/sky state belongs to the selected output.
            // Never carry another backend's QImage into this window.
            lastImage_=QImage{};
            hasRenderedPose_=false;
            canvas_->setImage(QImage{});
            canvas_->enableSkyCompositing(false);
            backendLastRenderer_[size_t(index)].clear();
            backendLastRequest_[size_t(index)].clear();
        }
        for(int i=0;i<int(backendWindows_.size());++i){
            auto* view=backendWindows_[size_t(i)];
            if(!view)continue;
            const bool preserveInFlightPresent=watcher_.isRunning()&&
                activeRenderDirect_&&i==activeRenderBackend_;
            view->setNativePresentation((i==index&&direct)||preserveInFlightPresent);
            if(i==index){
                if(isVisible()){
                    // Preserve the established left render pane when the
                    // user changes backend during an active exploration.
                    if(exploring_&&explorationWindowLayoutActive_&&
                       !explorationRenderGeometry.isEmpty())
                        view->setGeometry(explorationRenderGeometry);
                    if(view->isMinimized())view->showNormal();
                    else view->show();
                    view->raise();
                }
            }else view->hide();
        }
        updateRendererStamp();
        if(direct){
            statusBar()->showMessage(QStringLiteral(
                "Fenêtre backend dédiée — affichage GPU direct (swapchain), sans QImage ; "
                "décocher pour les captures PNG"),7000);
        }
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
                if(nativeFpsLabel_)nativeFpsLabel_->setText(
                    QStringLiteral("%1 FPS").arg(fpsFiltered_,0,'f',1));
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
            {"zPrepass",backendBox_->currentIndex()==5
                ?qEnvironmentVariable("M3D_V17_DISABLE_Z_PREPASS")!=QStringLiteral("1")
                :qEnvironmentVariable("M3D_V17_EXPERIMENT_Z_PREPASS")==QStringLiteral("1")},
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
        updateRendererStamp();
        if(sceneLoading_||!runtime_||runtime_->cameras.isEmpty())return;
        // Select the persistent top-level output HWND dedicated to this backend
        // before creating its DXGI/Vulkan surface.
        if(backendBox_){
            const int selected=std::clamp(backendBox_->currentIndex(),0,
                                          int(backendWindows_.size())-1);
            if(canvas_!=backendWindows_[size_t(selected)]||!canvas_->isVisible()||
               canvas_->nativePresentation()!=nativeDirectSelected())
                selectBackendOutputWindow();
        }
        if(nativeDirectSelected()){
            if(!canvas_->isVisible())
                selectBackendOutputWindow();
            const QSize target=canvas_->size();
            const bool ready=canvas_->isVisible() &&
                target.width()>=320 && target.height()>=200 &&
                canvas_->testAttribute(Qt::WA_NativeWindow);
            if(!ready){
                if(++nativeReadyAttempts_>40){
                    nativeReadyAttempts_=0;
                    // Keep the requested mode checked, latch this backend to
                    // QImage, and allow the user to retry by toggling it.
                    const int failedBackend=backendBox_->currentIndex();
                    if(failedBackend>=0&&failedBackend<int(nativeDirectFailed_.size())){
                        nativeDirectFailed_[size_t(failedBackend)]=true;
                        nativeDirectFailureReason_[size_t(failedBackend)]=
                            QStringLiteral("La fenêtre n’a pas fourni un HWND visible aux dimensions requises.");
                    }
                    const QString backend=failedBackend==5
                        ?QStringLiteral("Vulkan"):QStringLiteral("DX11");
                    const QString windowDetails=describeNativeWindow(
                        quintptr(canvas_->winId()),target,canvas_->devicePixelRatioF());
                    if(failedBackend>=0&&failedBackend<int(nativeDirectFailureReason_.size()))
                        saveNativePresentationDiagnostic(
                            backend,nativeDirectFailureReason_[size_t(failedBackend)],windowDetails);
                    selectBackendOutputWindow();
                    QTimer::singleShot(0,this,[this]{
                        if(!closingRequested_&&!sceneLoading_&&runtime_)requestRender();
                    });
                }else if(!nativeReadyRetryQueued_){
                    nativeReadyRetryQueued_=true;
                    QTimer::singleShot(50,this,[this]{
                        nativeReadyRetryQueued_=false;
                        if(!closingRequested_&&!sceneLoading_&&runtime_)
                            requestRender();
                    });
                }
                return;
            }
            nativeReadyAttempts_=0;
        }
        const auto serial=renderSerial_;
        if(watcher_.isRunning()){
            pending_=true; // Camera movement must not invalidate a finished image.
            ++pacingQueued_;
            return;
        }
        const uint64_t token=serial->fetch_add(1)+1;
        auto snapshot=runtime_;
        ImageView* const outputView=canvas_;
        const int requestedBackend=backendBox_->currentIndex();
        const bool directPresent=nativeDirectSelected();
        activeRenderBackend_=requestedBackend;
        activeRenderDirect_=directPresent;
        // V17.493 uses an independent 30 Hz Qt sky compositor. Vulkan WSI
        // bypasses that QWidget, so sample the SAME monotonic solar clock on
        // each native frame rather than the slower 1.5 s UI spinbox timer.
        // Never modify the approved solarMotion() or walking controller.
        const int backend=backendBox_->currentIndex();
        const float renderHour=(directPresent && dayNightCycleBox_ &&
                                dayNightCycleBox_->isChecked() && dayNightClock_.isValid())
            ?float(cse::weather::cycleHour(dayNightOriginHour_,
                              double(dayNightClock_.elapsed())/1000.0))
            :snapshot->daylightHour;
        // Keep lighting smooth, but rebuild the expensive 1024² CPU shadow map
        // only once per simulated hour during DirectX day/night. At 24h/6min,
        // this cuts rebuild stalls from every 3.75s to every 15s without lowering map quality.
        const bool stableDxDayNight=backend==4 && dayNightCycleBox_ && dayNightCycleBox_->isChecked();
        const float shadowStepHours=stableDxDayNight?1.0f:.25f;
        const float shadowHour=std::floor(renderHour/shadowStepHours)*shadowStepHours;
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
        int w=renderSize.width();
        int h=renderSize.height();
        // A swapchain always targets this backend's visible top-level HWND
        // extent; the offscreen quality resolution is irrelevant in this mode.
        if(directPresent && outputView){
            const qreal dpr=outputView->devicePixelRatioF();
            const QSize logicalSize=outputView->size();
            renderSize=QSize(std::max(320,int(logicalSize.width()*dpr)),
                             std::max(200,int(logicalSize.height()*dpr)));
            // The camera and renderer MUST use the native viewport dimensions.
            // Previously only renderSize changed: w/h still held the small Qt
            // canvas dimensions, stretching the Vulkan image on F11 fullscreen.
            w=renderSize.width();
            h=renderSize.height();
        }
        const quintptr nativeHandle=directPresent?quintptr(outputView->winId()):0;
        const qreal nativeDpr=directPresent&&outputView?outputView->devicePixelRatioF():1.0;
        const QString nativeWindowDetails=directPresent
            ?describeNativeWindow(nativeHandle,QSize(w,h),nativeDpr):QString{};
        const bool wantGpu=backend==1||backend==4||backend==5;
        const bool raster=backend==4||backend==5;
        const bool vulkan=backend==5;
        void* sharedVulkanInstance=nullptr;
        uintptr_t sharedVulkanSurface=0;
#if defined(_WIN32) && M3D_V17_VULKAN_ENABLED
        // Leave shared Vulkan handles null. VulkanRaster creates its own instance
        // and Win32 surface from nativeHandle, as in the passing hardware diagnostic.
#endif
        const bool globalIllumination=backend==2||backend==3;
        const bool edgeAA=edgeAa_->isChecked();
        const unsigned sampleCount=globalIllumination?
            unsigned(giSamples_->currentData().toInt()):
            (wantGpu?1u:unsigned(directSamples_->currentData().toInt()));
        const unsigned maxBounces=backend==2?2u:4u;
        const bool progressive=globalIllumination&&progressiveGi_->isChecked();
        const bool denoise=globalIllumination&&denoiseGi_->isChecked();
        const bool denoiseDirect=!globalIllumination&&!wantGpu&&denoiseDirect_->isChecked();
        const QString requestStamp=requestedRendererStamp();
        statusBar()->showMessage(QStringLiteral("Calcul de l'image…"));
        // Explain an abnormal delay rather than displaying "Calcul..." forever.
        // This watchdog does not claim to cancel a driver-blocked D3D call.
        if(raster){
            QTimer::singleShot(30000,this,[this,serial,token,vulkan]{
                if(serial->load()==token && watcher_.isRunning() && !closingRequested_)
                    statusBar()->showMessage(
                        QStringLiteral("Rendu %1 actif depuis plus de 30 secondes. GPU ou pilote possiblement saturé ; aucun basculement CPU.")
                        .arg(vulkan?QStringLiteral("Vulkan"):QStringLiteral("DirectX 11")));
            });
        }
        const auto shadowCache=cpuShadows_;
        watcher_.setFuture(QtConcurrent::run(&renderPool_,[this,serial,token,snapshot,preset,w,h,wantGpu,
            globalIllumination,raster,vulkan,directPresent,nativeHandle,sharedVulkanInstance,
            sharedVulkanSurface,edgeAA,sampleCount,maxBounces,progressive,denoise,outputView,requestedBackend,
            nativeWindowDetails,
            denoiseDirect,shadowCache,renderHour,shadowHour,requestStamp]{
            Result result;
            result.requestStamp=requestStamp;
            result.cameraPose=preset;
            result.generation=token;
            result.backendIndex=requestedBackend;
            result.nativeWindowDetails=nativeWindowDetails;
            result.skyCompositing=raster;
            result.renderThread=reinterpret_cast<quintptr>(QThread::currentThreadId());
#if defined(_WIN32)
            if(!result.nativeWindowDetails.isEmpty())
                result.nativeWindowDetails+=QStringLiteral(" renderThreadWin32=%1 renderThreadQt=0x%2")
                    .arg(GetCurrentThreadId())
                    .arg(QString::number(qulonglong(result.renderThread),16));
#endif
            updateLighting(renderHour);
            const auto camera=v16::makeCamera(preset,w,h);
            std::vector<uint32_t> pixels;
            Stats stats;
            double shadowBuildMs=0.0;
            bool ok=false;
            if(wantGpu){
                std::wstring reason;
                if(!raster){
                    releaseDirectX11NativeForCurrentThread();
                    releaseVulkanDirectForCurrentThread();
                    releaseVulkanPreviewForCurrentThread();
                }
                if(raster){
                    std::shared_ptr<const ShadowMap> shadow;
                    {
                        std::lock_guard<std::mutex> lock(shadowCache->mutex);
                        if(shadowCache->scene.lock()==snapshot &&
                           shadowCache->hour==shadowHour&&shadowCache->shadow)
                            shadow=shadowCache->shadow;
                        else {
                            Stats shadowStats;
                            const auto shadowStart=std::chrono::steady_clock::now();
                            shadow=std::make_shared<ShadowMap>(
                                buildShadow(snapshot->renderScene,shadowStats));
                            shadowBuildMs=std::chrono::duration<double,std::milli>(
                                std::chrono::steady_clock::now()-shadowStart).count();
                            // Optional diagnostic of the 1,024 x 1,024 CPU ray
                            // shadow pass. This cost occurs outside VulkanRaster
                            // and cannot appear in its GPU-only CSV timings.
                            const QByteArray shadowProfile=qgetenv("CSE_WARP_VK_SHADOW_PROFILE");
                            if(!shadowProfile.isEmpty()){
                                QFile log(QString::fromLocal8Bit(shadowProfile));
                                if(log.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){
                                    const QString line=QStringLiteral("%1,shadow_build_ms=%2,solar_hour=%3,shadow_bin=%4,backend=%5\n")
                                        .arg(QDateTime::currentMSecsSinceEpoch())
                                        .arg(shadowBuildMs,0,'f',3)
                                        .arg(renderHour,0,'f',5)
                                        .arg(shadowHour,0,'f',2)
                                        .arg(vulkan?QStringLiteral("Vulkan"):QStringLiteral("DX11"));
                                    log.write(line.toUtf8());
                                }
                            }
                            shadowCache->scene=snapshot;shadowCache->hour=shadowHour;
                            shadowCache->shadow=shadow;
                        }
                    }
                    if(vulkan){
                        releaseDirectX11NativeForCurrentThread(); // DXGI belongs to a different backend HWND.
                        // Keep one Vulkan device/resource set resident for the selected path.
                        if(directPresent)releaseVulkanPreviewForCurrentThread();
                        else releaseVulkanDirectForCurrentThread();
                        if(directPresent){
                            ok=renderVulkanDirectPresent(snapshot->renderScene,*shadow,camera,w,h,
                                reinterpret_cast<void*>(nativeHandle),stats,reason,
                                sharedVulkanInstance,sharedVulkanSurface);
                            if(!ok){
                                const std::wstring nativeReason=reason;
                                result.nativeFailure=QString::fromStdWString(nativeReason);
                                releaseVulkanDirectForCurrentThread();
                                reason.clear();
                                ok=renderVulkanPreview(snapshot->renderScene,*shadow,camera,w,h,
                                                       pixels,stats,reason);
                                result.nativeFallback=ok;
                                if(!ok){
                                    const std::wstring previewReason=reason;
                                    reason=L"Vulkan native presentation failed: "+nativeReason+
                                           L"; Vulkan image preview also failed: "+previewReason;
                                }
                            }
                        }else{
                            ok=renderVulkanPreview(snapshot->renderScene,*shadow,camera,w,h,
                                                   pixels,stats,reason);
                        }
                    }else{
                        // Vulkan WSI and DXGI must never own swapchains for the
                        // same backend HWND at the same time. Also release the
                        // offscreen renderer when leaving Vulkan.
                        releaseVulkanDirectForCurrentThread();
                        releaseVulkanPreviewForCurrentThread();
                        ok=directPresent?
                            renderDirectX11Native(snapshot->renderScene,*shadow,camera,w,h,
                                                   reinterpret_cast<void*>(nativeHandle),stats,reason):
                            renderDirectX11Preview(snapshot->renderScene,*shadow,camera,w,h,pixels,stats,reason);
                        if(!ok && directPresent){
                            // DXGI can be unavailable on remote desktop / certain drivers:
                            // recover the original full-quality CPU/QImage preview.
                            const std::wstring nativeReason=reason;
                            result.nativeFailure=QString::fromStdWString(nativeReason);
                            releaseDirectX11NativeForCurrentThread();
                            reason.clear();
                            ok=renderDirectX11Preview(snapshot->renderScene,*shadow,camera,w,h,
                                                      pixels,stats,reason);
                            result.nativeFallback=ok;
                            if(!ok){
                                const std::wstring previewReason=reason;
                                reason=L"DXGI native presentation failed: "+nativeReason+
                                       L"; DirectX 11 image preview also failed: "+previewReason;
                            }
                        }
                    }
                    result.directPresented=ok&&directPresent&&!result.nativeFallback;
                    if(ok){
                        result.renderer=vulkan
                            ?(result.directPresented
                                ?QStringLiteral("V17 Vulkan Raster — Direct GPU (swapchain)")
                                :QStringLiteral("V17 Vulkan Raster — affichage Qt (QImage)"))
                            :(result.directPresented
                                ?QStringLiteral("V17 D3D11 Raster — Direct GPU (swapchain)")
                                :QStringLiteral("V17 D3D11 Raster — affichage Qt (QImage)"));
                        if(!vulkan && rasterHybridLightingEnabled())
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
                        "Échec du rendu GPU %1 : %2. "
                        "Aucun basculement automatique vers le CPU.")
                        .arg(vulkan?QStringLiteral("Vulkan"):QStringLiteral("DirectX11"))
                        .arg(QString::fromStdWString(reason));
                    result.renderer=vulkan?QStringLiteral("V17 Vulkan — erreur")
                                            :QStringLiteral("V17 GPU D3D11 — erreur");
                    return result;
                }
            }
            if(!wantGpu){
                releaseDirectX11NativeForCurrentThread();
                releaseVulkanDirectForCurrentThread();
                releaseVulkanPreviewForCurrentThread();
            }
            if(!ok){
                releaseDirectX11NativeForCurrentThread();
                pixels.resize(size_t(w)*size_t(h));
                std::shared_ptr<const ShadowMap> shadow;
                bool shadowReused=false;
                {
                    std::lock_guard<std::mutex> lock(shadowCache->mutex);
                    if(shadowCache->scene.lock()==snapshot &&
                       shadowCache->hour==shadowHour && shadowCache->shadow){
                        shadow=shadowCache->shadow;
                        shadowReused=true;
                    }else{
                        Stats shadowStats;
                        shadow=std::make_shared<ShadowMap>(
                            buildShadow(snapshot->renderScene,shadowStats));
                        shadowCache->scene=snapshot;
                        shadowCache->hour=shadowHour;
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
                                [this,serial,token,outputView,count=completed,target=sampleCount,w,h,image=std::move(image)]() mutable {
                                    if(serial->load()!=token)return;
                                    if(outputView==canvas_)lastImage_=image;
                                    outputView->setImage(std::move(image));
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
            result.milliseconds=stats.ms+shadowBuildMs;
            result.renderer+=QStringLiteral(" — %1×%2").arg(w).arg(h);
            if(result.nativeFallback){
                result.renderer+=vulkan
                    ?QStringLiteral(" — secours image Vulkan via QImage (WSI indisponible)")
                    :QStringLiteral(" — secours QImage (DXGI indisponible)");
                if(!result.nativeFailure.isEmpty())
                    result.renderer+=QStringLiteral(" — échec présentation native : %1")
                        .arg(result.nativeFailure);
            }
            if(result.directPresented){
                result.renderer+=QStringLiteral(" — écran GPU direct (swapchain)");
                return result; // No 4-byte-per-pixel CPU/QImage allocation.
            }
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
    // Split only interactive raster output into a sky backdrop and an
    // alpha-preserving architecture foreground. Benchmarks stay unchanged.
    qputenv("M3D_V17_SPLIT_SKY","1");
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
    const bool nativeUiSmoke=argc>=2
        && QString::fromLocal8Bit(argv[1])==QStringLiteral("--smoke-native-ui");
    // Use the same Windows V17 brand icon for Qt titlebar/Alt-Tab/taskbar.
    // The ICO is also embedded in the EXE's native Win32 resources.
    const QString iconPath=QCoreApplication::applicationDirPath()
        +QStringLiteral("/cse_warp_v17.ico");
    if(QFileInfo::exists(iconPath))app.setWindowIcon(QIcon(iconPath));
    const QString path=nativeUiSmoke?QString():
        (switchSmoke||repeatSmoke||walkingSmoke||scopeSmoke||immersiveSmoke
        ?QString::fromLocal8Bit(argv[2])
        :(argc>1?QString::fromLocal8Bit(argv[1]):defaultPath()));
    // The smoke test uses reduced texture resolution to stay viable on
    // CI's software-only WARP device. Normal V17 keeps full 2048 PBR.
    V17Window window(path,(switchSmoke||repeatSmoke||walkingSmoke||scopeSmoke||immersiveSmoke)?256:2048);
    if(switchSmoke)window.smokeSceneSwitch(QString::fromLocal8Bit(argv[3]));
    if(repeatSmoke)window.smokeRepeatRaster();
    if(walkingSmoke)window.smokeWalkingCollisionPreference();
    if(scopeSmoke)window.smokeScopedAim();
    if(immersiveSmoke)window.smokeImmersiveMouse();
    if(nativeUiSmoke)window.smokeNativeUi();
    if(!app.windowIcon().isNull())window.setWindowIcon(app.windowIcon());
    window.show();
    return app.exec();
}
