// Android-only validation frontend: real engine, not a synthetic renderer.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
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
    bool ready_=false,reported_=false,scheduled_=false,touchPending_=false,failed_=false;
    QPointF last_;
    void schedule(){
        if(!ready_||failed_||scheduled_||!isExposed())return;
        scheduled_=true;
        QTimer::singleShot(0,this,[this]{scheduled_=false;draw();});
    }
    void draw(){
        if(!isExposed()||!ready_||failed_||width()<1||height()<1)return;
        try{
            auto surface=QVulkanInstance::surfaceForWindow(this);
            require(surface!=VK_NULL_HANDLE,"Qt Android Vulkan surface unavailable");
            int w=int(width()*devicePixelRatio()),h=int(height()*devicePixelRatio());
            camera_.aspect=float(w)/float(h);
            Stats st{};std::wstring error;
            if(!renderVulkanDirectPresent(scene_,shadow_,camera_,w,h,this,st,error,
                    reinterpret_cast<void*>(vulkanInstance()->vkInstance()),reinterpret_cast<uintptr_t>(surface)))
                throw std::runtime_error(QString::fromStdWString(error).toStdString());
            require(st.gpu&&st.gpuDrawCalls>0,"native Vulkan frame not submitted");
            if(!reported_){reported_=true;qInfo()<<"CSE_ANDROID PASS WSI"<<w<<h<<"ms"<<st.ms;}
            if(touchPending_){touchPending_=false;qInfo()<<"CSE_ANDROID PASS TOUCH_CAMERA";}
        }catch(const std::exception& ex){
            failed_=true;releaseVulkanDirectForCurrentThread();qCritical()<<"CSE_ANDROID FAIL WSI"<<ex.what();
        }
    }
protected:
    void exposeEvent(QExposeEvent*) override{if(!isExposed())releaseVulkanDirectForCurrentThread();else schedule();}
    void resizeEvent(QResizeEvent*) override{schedule();}
    bool event(QEvent* e) override{
        if(e->type()==QEvent::PlatformSurface){
            auto* p=static_cast<QPlatformSurfaceEvent*>(e);
            if(p->surfaceEventType()==QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                releaseVulkanDirectForCurrentThread();
        }
        if(e->type()==QEvent::TouchBegin||e->type()==QEvent::TouchUpdate||e->type()==QEvent::TouchEnd){
            auto* t=static_cast<QTouchEvent*>(e);
            if(!t->points().isEmpty()){
                const auto pos=t->points().first().position();
                if(e->type()==QEvent::TouchUpdate&&ready_){
                    auto delta=pos-last_;
                    if(std::abs(delta.x())+std::abs(delta.y())>1){
                        camera_.f=norm(camera_.f+camera_.r*float(delta.x()*.002)+camera_.u*float(-delta.y()*.002));
                        camera_.r=norm(cross(camera_.f,V{0,1,0}));
                        camera_.u=norm(cross(camera_.r,camera_.f));
                        touchPending_=true;schedule();
                    }
                }
                last_=pos;
            }
            e->accept();return true;
        }
        return QWindow::event(e);
    }
public:
    PortWindow(Scene scene,ShadowMap shadow,Camera camera,QVulkanInstance* instance):
        scene_(std::move(scene)),shadow_(std::move(shadow)),camera_(camera){
        setSurfaceType(QSurface::VulkanSurface);setVulkanInstance(instance);
        setTitle(QStringLiteral("CSE WARP V17.031 Android Vulkan probe"));ready_=true;
    }
    ~PortWindow() override{releaseVulkanDirectForCurrentThread();}
};
#endif
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    QCoreApplication::setApplicationName(QStringLiteral("CSE WARP Android Probe"));
#ifdef CSE_ANDROID_EMULATOR_TEST
    // The x86_64 APK is explicitly an emulator-only diagnostic. ARM64 never opts in.
    qputenv("CSE_WARP_VK_ALLOW_SOFTWARE_TEST","1");
    qInfo()<<"CSE_ANDROID EMULATOR_ONLY x86_64";
#endif
    qputenv("CSE_WARP_VK_MSAA","1");
    try{
        auto root=extractAssets();
        triangle(QDir(root).filePath("triangle.png"));
        auto loaded=v16::loadScene(QDir(root).filePath("benchmark-civic/scene.json"));
        require(bool(loaded),"load exact scene V17.634");
        auto built=v16::buildRuntime(loaded.scene,1024);
        require(bool(built)&&!built.runtime.cameras.isEmpty(),"build scene runtime");
        updateLighting(14.f);
        Stats shadowStats{};auto shadow=buildShadow(built.runtime.renderScene,shadowStats);
        auto camera=v16::makeCamera(built.runtime.cameras.first(),640,360);
        imageTest(built.runtime.renderScene,shadow,camera,640,360,QDir(root).filePath("scene.png"),false);
        releaseVulkanPreviewForCurrentThread();
#if QT_CONFIG(vulkan)
        QVulkanInstance instance;instance.setApiVersion(QVersionNumber(1,3,204));
        require(instance.create(),"Qt Vulkan instance creation failed");
        PortWindow window(std::move(built.runtime.renderScene),std::move(shadow),camera,&instance);
        window.showFullScreen();
        QObject::connect(&app,&QGuiApplication::applicationStateChanged,&window,[](Qt::ApplicationState state){
            if(state!=Qt::ApplicationActive)releaseVulkanDirectForCurrentThread();
        });
        return app.exec();
#else
        throw std::runtime_error("Qt Android build lacks Vulkan surface support");
#endif
    }catch(const std::exception& e){
        releaseVulkanPreviewForCurrentThread();releaseVulkanDirectForCurrentThread();
        qCritical()<<"CSE_ANDROID FAIL"<<e.what();return 1;
    }
}
