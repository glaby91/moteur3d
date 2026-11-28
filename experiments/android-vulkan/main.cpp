// Android-only validation frontend: real engine, not a synthetic renderer.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include "src/m3d_walk_controller.h"
#include "reference_views.h"
#include "../../v17/exploration_motion.h"
#include <QElapsedTimer>
#include <QJniObject>
#include <QCoreApplication>
#include <QSettings>
#include <QKeyEvent>
#include <QEventPoint>
#include <QGuiApplication>
#include <QWindow>
#include <QTimer>
#include <QTouchEvent>
#include <QPlatformSurfaceEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QImage>
#include <QDebug>
#include <QVersionNumber>
#include <QByteArray>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif

using namespace m3d;
#ifdef CSE_ANDROID_EMULATOR_TEST
int cseAndroidWalkRegression();
#endif
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static QString extractAssets(){
    const QString root=QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("cse-port-")+QString::fromLatin1(CSE_ANDROID_BUILD_ID));
    require(QDir().mkpath(root),"create private data root");
    QFile manifest(QStringLiteral("assets:/cse/index.txt"));
    require(manifest.open(QIODevice::ReadOnly),"open packaged asset index");
    const auto entries=manifest.readAll().split('\n');
    for(const QByteArray& entry:entries){
        if(entry.isEmpty())continue;
        const QString rel=QString::fromUtf8(entry);
        require(!QDir::isAbsolutePath(rel)&&!rel.split('/').contains(QStringLiteral("..")),"invalid asset path");
        QFile src(QStringLiteral("assets:/cse/")+rel);
        require(src.open(QIODevice::ReadOnly),"open packaged asset");
        const QString dest=QDir(root).filePath(rel);
        if(QFileInfo(dest).isFile()&&QFileInfo(dest).size()==src.size())continue;
        require(QDir().mkpath(QFileInfo(dest).absolutePath()),"create asset directory");
        QSaveFile dst(dest);
        require(dst.open(QIODevice::WriteOnly),"open private asset");
        while(!src.atEnd()){
            const auto bytes=src.read(1024*1024);
            require(!bytes.isEmpty()&&dst.write(bytes)==bytes.size(),"copy asset bytes");
        }
        require(dst.commit(),"commit private asset");
    }
    require(QDir::setCurrent(root),"set shader/scene search root");
    qInfo().noquote()<<"CSE_ANDROID ASSETS"<<root;
    return root;
}
static void imageTest(const Scene& scene,const ShadowMap& sm,const Camera& cam,int w,int h,const QString& file,bool triangle){
    Stats st{};std::wstring reason;std::vector<uint32_t> pixels;
    if(!renderVulkanPreview(scene,sm,cam,w,h,pixels,st,reason))
        throw std::runtime_error(QString::fromStdWString(reason).toStdString());
    require(st.gpu&&pixels.size()==size_t(w*h)&&st.gpuDrawCalls>0,"invalid Vulkan frame");
    if(triangle)require(st.gpuDrawCalls==3,"unexpected triangle draw count");
    require(std::any_of(pixels.begin(),pixels.end(),[&](uint32_t p){return p!=pixels.front();}),"flat Vulkan frame");
    QImage image(w,h,QImage::Format_ARGB32);
    std::memcpy(image.bits(),pixels.data(),pixels.size()*sizeof(uint32_t));
    require(image.save(file),"PNG export");
    qInfo().noquote()<<"CSE_ANDROID PASS"<<(triangle?"TRIANGLE":"SCENE")<<w<<h<<"ms"<<st.ms<<"draws"<<st.gpuDrawCalls;
}
static void triangle(const QString& path){
    Scene s;s.mat.resize(1);s.mat[0].name="test_surface";
    ColorLevel img;img.size=4;img.p.resize(16,0x00e2b07cu);s.mat[0].colorMip.push_back(std::move(img));
    Obj o{};o.kind=Kind::Triangle;o.mat=0;o.a={-1.f,0.f,-2.f};o.b={1.f,0.f,-2.f};o.c={0.f,2.f,-2.f};
    o.n0=o.n1=o.n2={0.f,0.f,1.f};o.uv0={0.f,0.f,0.f};o.uv1={1.f,0.f,0.f};o.uv2={.5f,1.f,0.f};
    o.mn={-1.f,0.f,-2.f};o.mx={1.f,2.f,-2.f};s.obj.push_back(o);s.gpuCacheIdentity=0x13579u;
    Camera c{};c.eye={0.f,1.f,2.f};c.f={0.f,0.f,-1.f};c.r={1.f,0.f,0.f};c.u={0.f,1.f,0.f};c.aspect=320.f/180.f;c.tanHalf=.7f;
    ShadowMap sm{};sm.size=1;sm.d={1e9f};
    imageTest(s,sm,c,320,180,path,true);
    releaseVulkanPreviewForCurrentThread();
}
#if QT_CONFIG(vulkan)
class PortWindow final:public QWindow{
    Scene scene_;
    ShadowMap shadow_;
    Camera camera_;
    QVector<v16::CameraPreset> presets_;
    int preset_=0,scale_=100,msaa_=1,sensitivity_=1;
    bool hud_=true,menu_=false,active_=true,resumePending_=false,scalePending_=false,msaaPending_=false;
    QElapsedTimer hudClock_,frameClock_;
    int frameCount_=0;
    double fps_=0.;
    Stats lastStats_{};
    bool ready_=false,reported_=false,scheduled_=false,touchPending_=false,failed_=false;
    walk::State walker_{};
    QElapsedTimer tick_;
    QTimer motionTimer_;
    QPointF moveAnchor_,moveDelta_;
    int moveId_=-1,lookId_=-1;
    bool walking_=false,movePending_=false,jumpPending_=false,crouchPending_=false;
    QPointF last_;
    float jumpStartY_=0.f;
    bool gestureActive_=false,gestureFramePending_=false;
    V gestureStartFeet_{};
    void publishHud(bool force=false){
        if(!force&&hudClock_.isValid()&&hudClock_.elapsed()<200)return;
        hudClock_.start();
        if(!QNativeInterface::QAndroidApplication::isActivityContext())return;
        QJniObject activity=QNativeInterface::QAndroidApplication::context();
        if(!activity.isValid())return;
        QString top=QStringLiteral("CSE WARP  |  Vulkan  |  %1").arg(presets_.isEmpty()?QStringLiteral("Scene"):presets_[preset_].name);
        QString metrics=QStringLiteral("%1 x %2  |  %3 ms  |  %4 FPS  |  MSAA x%5")
            .arg(lastStats_.androidRenderWidth).arg(lastStats_.androidRenderHeight)
            .arg(lastStats_.ms,0,'f',1).arg(fps_,0,'f',1).arg(lastStats_.androidMsaa);
#ifdef CSE_ANDROID_EMULATOR_TEST
        metrics=QStringLiteral("QA SwiftShader  |  %1 x %2  |  %3 ms/image  |  MSAA x%4")
            .arg(lastStats_.androidRenderWidth).arg(lastStats_.androidRenderHeight)
            .arg(lastStats_.ms,0,'f',1).arg(lastStats_.androidMsaa);
#endif
        auto a=QJniObject::fromString(top),b=QJniObject::fromString(metrics);
        const float range=std::max(30.f,float(height())*.14f);
        float dx=clampf(float(moveDelta_.x())/range,-1.f,1.f),dy=clampf(float(moveDelta_.y())/range,-1.f,1.f);
        const float mag=std::sqrt(dx*dx+dy*dy);if(mag>1.f){dx/=mag;dy/=mag;}
        activity.callMethod<void>("updateWarpHud","(Ljava/lang/String;Ljava/lang/String;ZZZIIIFF)V",
            a.object<jstring>(),b.object<jstring>(),jboolean(hud_),jboolean(menu_),jboolean(walker_.crouched),
            jint(scale_),jint(msaa_),jint(sensitivity_),jfloat(dx),jfloat(dy));
    }
    void saveSettings(){
#ifndef CSE_ANDROID_EMULATOR_TEST
        QSettings settings;settings.setValue("scale",scale_);settings.setValue("msaa",msaa_);settings.setValue("sensitivity",sensitivity_);settings.setValue("hud",hud_);
#endif
    }
    void setScale(int value){
        scale_=value;qputenv("CSE_WARP_ANDROID_RENDER_SCALE",QByteArray::number(scale_));
        scalePending_=true;saveSettings();schedule();publishHud(true);
    }
    void setMsaa(int value){
        msaa_=value;qputenv("CSE_WARP_VK_MSAA",QByteArray::number(msaa_));
        releaseVulkanDirectForCurrentThread();msaaPending_=true;saveSettings();schedule();publishHud(true);
    }
    void nextCamera(){
        if(presets_.isEmpty())return;
        stopInput();walking_=false;walker_={};
        preset_=(preset_+1)%presets_.size();
        camera_=v16::makeCamera(presets_[preset_],std::max(1,width()),std::max(1,height()));
        schedule();publishHud(true);
        qInfo().noquote()<<"CSE_ANDROID PASS CAMERA_MENU"<<preset_;
    }
    bool uiPress(QPointF pos){
        const double x=pos.x()/std::max(1,width()),y=pos.y()/std::max(1,height());
        if(y<.12&&x>.86){
            moveId_=lookId_=-1;moveDelta_=QPointF{};menu_=!menu_;publishHud(true);
            return true;
        }
        if(y<.12&&x>.73){
            hud_=!hud_;saveSettings();publishHud(true);return true;
        }
        if(!menu_)return false;
        if(x>.30&&x<.70){
            if(y>.23&&y<.37)setScale(scale_==100?75:scale_==75?50:100);
            else if(y>.39&&y<.53)setMsaa(msaa_==1?2:msaa_==2?4:1);
            else if(y>.55&&y<.68)nextCamera();
            else if(y>.70&&y<.82){sensitivity_=sensitivity_==3?1:sensitivity_+1;saveSettings();publishHud(true);qInfo().noquote()<<"CSE_ANDROID PASS SENSITIVITY"<<sensitivity_;}
            else if(y>.84&&y<.96){menu_=false;publishHud(true);schedule();}
        }
        return true;
    }
    void beginWalking(){
        if(walking_)return;
        walk::spawn(scene_,camera_.eye,walker_);
        camera_.eye=walk::eye(walker_);walking_=true;
    }
    void stopInput(){moveId_=lookId_=-1;moveDelta_=QPointF{};gestureActive_=false;gestureFramePending_=false;}
    void tickMotion(){
        const float dt=std::min(.1f,float(tick_.restart())*.001f);
        if(!active_||!isExposed()||failed_||menu_||dt<=0.f)return;
#ifndef CSE_ANDROID_EMULATOR_TEST
        schedule(); // continuous hardware frames for a real measured cadence
#endif
        if(!walking_)return;
        if(moveId_<0&&walker_.grounded)return;
        V motion{};
        if(moveId_>=0){
            const float range=std::max(30.f,float(height())*.14f);
            float x=clampf(float(moveDelta_.x())/range,-1.f,1.f);
            float y=clampf(float(-moveDelta_.y())/range,-1.f,1.f);
            const float mag=std::sqrt(x*x+y*y);if(mag>1.f){x/=mag;y/=mag;}
            V forward=norm(V{camera_.f.x,0,camera_.f.z});
            V right=norm(cross(forward,V{0,1,0}));
            motion=(forward*y+right*x)*(cse::exploration::speedMetersPerSecond(false)*dt);
        }
        V old=walker_.feet;
        walk::advance(scene_,walker_,motion,dt);
        camera_.eye=walk::eye(walker_);
        if(length(walker_.feet-old)>.0001f){
            if(moveId_>=0&&length(V{walker_.feet.x-old.x,0.f,walker_.feet.z-old.z})>.0001f)movePending_=true;
            schedule();
        }
    }
    void schedule(){
        if(!active_||!ready_||failed_||scheduled_||!isExposed())return;
#ifdef CSE_ANDROID_EMULATOR_TEST
        // SwiftShader Full HD takes many seconds per frame. Keep the Qt input
        // loop responsive throughout a gesture, then present its final pose.
        if(gestureActive_){gestureFramePending_=true;return;}
#endif
        scheduled_=true;
        QTimer::singleShot(0,this,[this]{
            scheduled_=false;
#ifdef CSE_ANDROID_EMULATOR_TEST
            if(gestureActive_){gestureFramePending_=true;return;}
#endif
            draw();
        });
    }
    void draw(){
        if(!active_||!isExposed()||!ready_||failed_||width()<1||height()<1)return;
        try{
            auto surface=QVulkanInstance::surfaceForWindow(this);
            require(surface!=VK_NULL_HANDLE,"Qt Android Vulkan surface unavailable");
            int w=int(width()*devicePixelRatio()),h=int(height()*devicePixelRatio());
            camera_.aspect=float(w)/float(h);
            Stats st{};std::wstring error;
            if(!renderVulkanDirectPresent(scene_,shadow_,camera_,w,h,this,st,error,
                    reinterpret_cast<void*>(vulkanInstance()->vkInstance()),reinterpret_cast<uintptr_t>(surface)))
                throw std::runtime_error(QString::fromStdWString(error).toStdString());
            require(st.gpu&&st.gpuDrawCalls>0&&st.readbackBufferCount==0,"native Vulkan frame missing or CPU readback");
            lastStats_=st;
            if(!frameClock_.isValid())frameClock_.start();
            ++frameCount_;
            if(frameClock_.elapsed()>=1000){fps_=1000.*frameCount_/frameClock_.elapsed();frameCount_=0;frameClock_.restart();}
            publishHud(!reported_||resumePending_||scalePending_||msaaPending_);
            if(resumePending_){resumePending_=false;qInfo().noquote()<<"CSE_ANDROID PASS RESUME";}
            if(scalePending_){
                scalePending_=false;
                require(st.androidRenderWidth==st.androidSurfaceWidth*unsigned(scale_)/100 &&
                        st.androidRenderHeight==st.androidSurfaceHeight*unsigned(scale_)/100,"scale did not change actual GPU targets");
                qInfo().noquote()<<"CSE_ANDROID PASS SCALE"<<scale_<<st.androidRenderWidth<<st.androidRenderHeight<<"surface"<<st.androidSurfaceWidth<<st.androidSurfaceHeight<<"readback"<<st.readbackBufferCount;
            }
            if(msaaPending_){
                msaaPending_=false;qInfo().noquote()<<"CSE_ANDROID PASS MSAA requested"<<msaa_<<"actual"<<st.androidMsaa;
            }
            if(!reported_){reported_=true;qInfo().noquote()<<"CSE_ANDROID PASS WSI"<<w<<h<<"ms"<<st.ms<<"readback"<<st.readbackBufferCount;}
            if(touchPending_){touchPending_=false;qInfo().noquote()<<"CSE_ANDROID PASS TOUCH_CAMERA";}
            if(movePending_){movePending_=false;qInfo().noquote()<<"CSE_ANDROID PASS WALK_MOVE";}
            if(jumpPending_&&walker_.feet.y>jumpStartY_+.01f){jumpPending_=false;qInfo().noquote()<<"CSE_ANDROID PASS JUMP_INPUT";}
            if(crouchPending_){crouchPending_=false;qInfo().noquote()<<"CSE_ANDROID PASS CROUCH_INPUT";}
        }catch(const std::exception& ex){
            failed_=true;releaseVulkanDirectForCurrentThread();qCritical().noquote()<<"CSE_ANDROID FAIL WSI"<<ex.what();
        }
    }
protected:
    void exposeEvent(QExposeEvent*) override{if(!isExposed()){stopInput();releaseVulkanDirectForCurrentThread();}else schedule();}
    void resizeEvent(QResizeEvent*) override{schedule();}
    bool event(QEvent* e) override{
        if(e->type()==QEvent::PlatformSurface){
            auto* p=static_cast<QPlatformSurfaceEvent*>(e);
            if(p->surfaceEventType()==QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                releaseVulkanDirectForCurrentThread();
        }
        if(e->type()==QEvent::TouchCancel){stopInput();publishHud(true);schedule();e->accept();return true;}
        if(e->type()==QEvent::TouchBegin||e->type()==QEvent::TouchUpdate||e->type()==QEvent::TouchEnd){
            auto* t=static_cast<QTouchEvent*>(e);
            if(e->type()==QEvent::TouchBegin){
                // A new sequence must not inherit a stale point ID.
                stopInput();gestureActive_=true;tick_.restart();
            }
            for(const auto& point:t->points()){
                const auto pos=point.position();const int id=point.id();
                if(point.state()==QEventPoint::State::Pressed){
                    if(uiPress(pos))continue;
                    if(hud_&&pos.x()>width()*.80&&pos.y()>height()*.72){
                        beginWalking();
                        if(pos.x()>width()*.90){
                            if(walk::jump(walker_)){jumpStartY_=walker_.feet.y;jumpPending_=true;}
                        }else if(walk::setCrouched(scene_,walker_,!walker_.crouched)){
                            camera_.eye=walk::eye(walker_);crouchPending_=true;schedule();
                        }
                    }else if(pos.x()<width()*.45&&moveId_<0){
                        beginWalking();moveId_=id;moveAnchor_=pos;moveDelta_=QPointF{};gestureStartFeet_=walker_.feet;
                        qInfo().noquote()<<"CSE_ANDROID INPUT MOVE_BEGIN"<<id<<pos.x()<<pos.y()<<"grounded"<<walker_.grounded;
                    }else if(lookId_<0){lookId_=id;last_=pos;qInfo().noquote()<<"CSE_ANDROID INPUT LOOK_BEGIN"<<id;}
                }else if(point.state()==QEventPoint::State::Updated){
                    if(id==moveId_){
                        moveDelta_=pos-moveAnchor_;
                        // Apply a bounded input step even under a slow software renderer.
                        tickMotion();
                    }
                    if(id==lookId_){
                        const auto delta=pos-last_;last_=pos;
                        if(std::abs(delta.x())+std::abs(delta.y())>1){
                            V next=norm(camera_.f+camera_.r*float(delta.x()*.0015*sensitivity_)+camera_.u*float(-delta.y()*.0015*sensitivity_));
                            if(std::abs(next.y)<.98f){
                                camera_.f=next;camera_.r=norm(cross(next,V{0,1,0}));
                                camera_.u=norm(cross(camera_.r,next));touchPending_=true;schedule();
                            }
                        }
                    }
                }else if(point.state()==QEventPoint::State::Released){
                    if(id==moveId_){
                        const float travelled=length(V{walker_.feet.x-gestureStartFeet_.x,0.f,walker_.feet.z-gestureStartFeet_.z});
                        qInfo().noquote()<<"CSE_ANDROID INPUT MOVE_END metres"<<travelled;
                        moveId_=-1;moveDelta_=QPointF{};publishHud(true);
                    }
                    if(id==lookId_)lookId_=-1;
                }
            }
            if(e->type()==QEvent::TouchEnd){
                const bool frame=gestureFramePending_||touchPending_||movePending_||crouchPending_||scalePending_||msaaPending_;
                stopInput();
                if(frame)schedule();
                publishHud(true);
                qInfo().noquote()<<"CSE_ANDROID INPUT GESTURE_END";
            }
            e->accept();return true;
        }
        return QWindow::event(e);
    }
public:
    PortWindow(Scene scene,ShadowMap shadow,Camera camera,QVector<v16::CameraPreset> presets,QVulkanInstance* instance):
        scene_(std::move(scene)),shadow_(std::move(shadow)),camera_(camera),presets_(std::move(presets)){
#ifndef CSE_ANDROID_EMULATOR_TEST
        QSettings settings;
        const int a=settings.value("scale",100).toInt(),b=settings.value("msaa",1).toInt();
        scale_=(a==50||a==75)?a:100;msaa_=(b==2||b==4)?b:1;
        sensitivity_=std::clamp(settings.value("sensitivity",1).toInt(),1,3);hud_=settings.value("hud",true).toBool();
#endif
        qputenv("CSE_WARP_ANDROID_RENDER_SCALE",QByteArray::number(scale_));
        qputenv("CSE_WARP_VK_MSAA",QByteArray::number(msaa_));
        setSurfaceType(QSurface::VulkanSurface);setVulkanInstance(instance);
        setTitle(QStringLiteral("CSE WARP V17.031 Android Vulkan exploration"));ready_=true;
        tick_.start();motionTimer_.setInterval(16);
        QObject::connect(&motionTimer_,&QTimer::timeout,this,[this]{tickMotion();});motionTimer_.start();
    }
    void applicationState(Qt::ApplicationState state){
        const bool next=state==Qt::ApplicationActive;
        if(!next){active_=false;stopInput();releaseVulkanDirectForCurrentThread();qInfo().noquote()<<"CSE_ANDROID PAUSED";}
        else if(!active_){active_=true;resumePending_=true;tick_.restart();frameClock_.invalidate();frameCount_=0;fps_=0;schedule();}
    }
    ~PortWindow() override{motionTimer_.stop();releaseVulkanDirectForCurrentThread();}
};
#endif
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    QCoreApplication::setOrganizationName(QStringLiteral("CSE WARP"));
    QCoreApplication::setApplicationName(QStringLiteral("CSE WARP Android"));
#ifdef CSE_ANDROID_EMULATOR_TEST
    // The x86_64 APK is explicitly an emulator-only diagnostic. ARM64 never opts in.
    qputenv("CSE_WARP_VK_ALLOW_SOFTWARE_TEST","1");
    qInfo().noquote()<<"CSE_ANDROID EMULATOR_ONLY x86_64; strict tablet policy unchanged";
#endif
    qputenv("CSE_WARP_VK_MSAA","1");
    qputenv("CSE_WARP_VK_FRAMES_IN_FLIGHT","1");
    try{
        auto root=extractAssets();
#ifdef CSE_ANDROID_EMULATOR_TEST
        require(cseAndroidWalkRegression()==0,"shared walk regression");
        qInfo().noquote()<<"CSE_ANDROID PASS WALK_REGRESSION";
        triangle(QDir(root).filePath("triangle.png"));
#endif
        auto loaded=v16::loadScene(QDir(root).filePath("benchmark-civic/scene.json"));
        require(bool(loaded),"load exact scene V17.634");
        auto built=v16::buildRuntime(loaded.scene,1024);
        require(bool(built)&&!built.runtime.cameras.isEmpty(),"build scene runtime");
        updateLighting(14.f);
        Stats shadowStats{};auto shadow=buildShadow(built.runtime.renderScene,shadowStats);
        auto camera=v16::makeCamera(built.runtime.cameras.first(),640,360);
#ifdef CSE_ANDROID_EMULATOR_TEST
        imageTest(built.runtime.renderScene,shadow,camera,640,360,QDir(root).filePath("scene.png"),false);
        cse::androidqa::renderViews(built.runtime,shadow,root);
        qInfo().noquote()<<"CSE_ANDROID PASS FIVE_VIEWS";
#endif
        releaseVulkanPreviewForCurrentThread();
#if QT_CONFIG(vulkan)
        QVulkanInstance instance;instance.setApiVersion(QVersionNumber(1,3,204));
        require(instance.create(),"Qt Vulkan instance creation failed");
        PortWindow window(std::move(built.runtime.renderScene),std::move(shadow),camera,built.runtime.cameras,&instance);
        window.showFullScreen();
        QObject::connect(&app,&QGuiApplication::applicationStateChanged,&window,[&window](Qt::ApplicationState state){window.applicationState(state);});
        return app.exec();
#else
        throw std::runtime_error("Qt Android build lacks Vulkan surface support");
#endif
    }catch(const std::exception& e){
        releaseVulkanPreviewForCurrentThread();releaseVulkanDirectForCurrentThread();
        qCritical().noquote()<<"CSE_ANDROID FAIL"<<e.what();return 1;
    }
}
