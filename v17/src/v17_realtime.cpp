// CSE WARP Beta X — V17 realtime raster proof.
// Reuses the V16 scene database, triangulation, PBR textures and CPU renderer
// without replacing or forking the V16 engine. OpenGL 3.3 is widely supported
// on the target Intel HD Graphics 620. All GPU drawing remains on the GPU.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDir>
#include <QHash>
#include <QStatusBar>
#include <QVector4D>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMatrix4x4>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

using namespace m3d;
namespace {
struct Vertex {
    float x,y,z,nx,ny,nz,u,v,material;
};
static_assert(sizeof(Vertex)==9*sizeof(float));
struct TransparentFace {
    Vertex triangle[3];
    QVector3D center;
};
QVector3D qv(V a){return {a.x,a.y,a.z};}
QString shaderPath(const char* name){
    return QCoreApplication::applicationDirPath()+"/shaders/v17/"+QString::fromLatin1(name);
}
int channel(uint32_t color,int shift){return int((color>>shift)&255u);}
class RealtimeView final : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
public:
    explicit RealtimeView(std::shared_ptr<v16::Runtime> runtime,QWidget* parent=nullptr)
      :QOpenGLWidget(parent),runtime_(std::move(runtime)){
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setMinimumSize(440,275);
        selectCamera(0);
        setHour(runtime_->daylightHour);
        frameClock_.start();
        connect(&loop_,&QTimer::timeout,this,[this]{tick();});
        loop_.setTimerType(Qt::PreciseTimer);
        loop_.start(16);
    }
    ~RealtimeView() override {
        loop_.stop();
        if(context()&&isValid()){
            makeCurrent();
            target_.reset();
            if(shadowDepth_)glDeleteTextures(1,&shadowDepth_);
            if(shadowFbo_)glDeleteFramebuffers(1,&shadowFbo_);
            glDeleteTextures(4,textures_);
            if(opaqueVbo_)glDeleteBuffers(1,&opaqueVbo_);
            if(transparentVbo_)glDeleteBuffers(1,&transparentVbo_);
            if(opaqueVao_)glDeleteVertexArrays(1,&opaqueVao_);
            if(transparentVao_)glDeleteVertexArrays(1,&transparentVao_);
            if(timerQueries_[0])glDeleteQueries(2,timerQueries_);
            doneCurrent();
        }
    }
    void selectCamera(int index){
        if(index<0||index>=runtime_->cameras.size())return;
        const auto& p=runtime_->cameras[index];
        position_=qv(p.eye);
        QVector3D facing=(qv(p.target)-position_).normalized();
        yaw_=std::atan2(facing.z(),facing.x())*180.f/PI;
        pitch_=std::asin(std::clamp(facing.y(),-1.f,1.f))*180.f/PI;
        fov_=p.fov;
        update();
    }
    void setRenderSize(int width,int height){
        renderWidth_=std::max(320,width);
        renderHeight_=std::max(180,height);
        if(context()&&isValid()){
            makeCurrent();
            target_.reset();
            doneCurrent();
        }
        update();
    }
    void setHour(float hour){
        hour_=hour;
        const auto lighting=m3d::captureLighting(hour_);
        sun_=qv(lighting.sun).normalized();
        const V tint=lighting.sunTint;
        const float intensity=0.75f+1.3f*lighting.daylight;
        sunColor_={tint.x/255.f*intensity,tint.y/255.f*intensity,tint.z/255.f*intensity};
        const V sky=lighting.skyZenith*.45f+lighting.skyHorizon*.55f;
        skyColor_={sky.x/255.f*.68f,sky.y/255.f*.68f,sky.z/255.f*.68f};
        fill_={.19f,.165f,.14f};
        shadowDirty_=true;
        update();
    }
    bool saveCapture(const QString& path){
        if(!isValid()||!target_)return false;
        makeCurrent();
        QImage image=target_->toImage();
        doneCurrent();
        return !image.isNull()&&image.save(path,"PNG");
    }
    std::function<void(float,float)> onPerformance;
    std::function<void(const QString&)> onError;
protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        auto compile=[this](QOpenGLShaderProgram& program,
                            const char* vertex,const char* fragment)->bool{
            QFile vs(shaderPath(vertex)),fs(shaderPath(fragment));
            if(!vs.open(QIODevice::ReadOnly)||!fs.open(QIODevice::ReadOnly)){
                reportError(QStringLiteral("Shader V17 introuvable : %1 ou %2")
                            .arg(QString::fromLatin1(vertex),QString::fromLatin1(fragment)));
                return false;
            }
            const QByteArray vert=vs.readAll(),frag=fs.readAll();
            if(!program.addShaderFromSourceCode(QOpenGLShader::Vertex,vert)||
               !program.addShaderFromSourceCode(QOpenGLShader::Fragment,frag)||
               !program.link()){
                reportError(QStringLiteral("Erreur GLSL : %1").arg(program.log()));
                return false;
            }
            return true;
        };
        if(!compile(shader_,"pbr.vert","pbr.frag")||
           !compile(depthShader_,"shadow.vert","shadow.frag"))return;
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE); // V16 triangles may use both face windings
        glDisable(GL_FRAMEBUFFER_SRGB); // tonemapping+gamma happens in GLSL
        createBuffers();
        createTextures();
        createShadow();
        glGenQueries(2,timerQueries_);
        initialized_=true;
        statusFrames_=0;
        fpsClock_.start();
    }
    void resizeGL(int,int) override { update(); }
    void paintGL() override {
        if(!initialized_) {
            glClearColor(.11f,.13f,.16f,1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }
        if(!target_||target_->size()!=QSize(renderWidth_,renderHeight_)){
            QOpenGLFramebufferObjectFormat format;
            format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            format.setInternalTextureFormat(GL_RGBA8);
            format.setSamples(0); // no expensive MSAA for baseline Intel iGPU
            target_=std::make_unique<QOpenGLFramebufferObject>(
                renderWidth_,renderHeight_,format);
            if(!target_->isValid()){
                reportError("Impossible de créer la cible GPU OpenGL.");
                target_.reset();
                return;
            }
        }
        const int slot=querySlot_;
        if(queryPending_[slot]) {
            GLint available=GL_FALSE;
            glGetQueryObjectiv(timerQueries_[slot],GL_QUERY_RESULT_AVAILABLE,&available);
            if(available){
                GLuint64 nanos=0;
                glGetQueryObjectui64v(timerQueries_[slot],GL_QUERY_RESULT,&nanos);
                gpuMs_=float(double(nanos)/1000000.0);
                queryPending_[slot]=false;
            }
        }
        const bool measure=!queryPending_[slot];
        if(measure)glBeginQuery(GL_TIME_ELAPSED,timerQueries_[slot]);
        if(shadowDirty_)renderShadow();
        renderColor();
        if(measure){glEndQuery(GL_TIME_ELAPSED);queryPending_[slot]=true;}
        querySlot_=(slot+1)%2;
        // Qt's default framebuffer belongs to QOpenGLWidget, NOT to the OS
        // window. Blit internally rendered 1366x768/FHD texture into it.
        const int fw=std::max(1,int(width()*devicePixelRatioF()));
        const int fh=std::max(1,int(height()*devicePixelRatioF()));
        glBindFramebuffer(GL_READ_FRAMEBUFFER,target_->handle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,defaultFramebufferObject());
        glBlitFramebuffer(0,0,renderWidth_,renderHeight_,
                          0,0,fw,fh,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());
        ++statusFrames_;
        const qint64 elapsed=fpsClock_.elapsed();
        if(elapsed>=900){
            const float fps=float(statusFrames_)*1000.f/float(elapsed);
            if(onPerformance)onPerformance(fps,gpuMs_);
            fpsClock_.restart();
            statusFrames_=0;
        }
    }
    void keyPressEvent(QKeyEvent* event) override {
        if(!event->isAutoRepeat())pressed_[event->key()]=true;
        if(event->key()==Qt::Key_Escape) {rightMouse_=false;unsetCursor();}
        else if(event->key()>=Qt::Key_1&&event->key()<=Qt::Key_6)
            selectCamera(event->key()-Qt::Key_1);
        else if(event->key()==Qt::Key_F11) {
            if(window()->isFullScreen())window()->showNormal();
            else window()->showFullScreen();
        }
        event->accept();
    }
    void keyReleaseEvent(QKeyEvent* event) override {
        if(!event->isAutoRepeat())pressed_[event->key()]=false;
        event->accept();
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()==Qt::RightButton){
            rightMouse_=true;
            lastMouse_=event->position();
            setCursor(Qt::BlankCursor);
            setFocus();
        }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()==Qt::RightButton){rightMouse_=false;unsetCursor();}
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!rightMouse_)return;
        const QPointF delta=event->position()-lastMouse_;
        lastMouse_=event->position();
        yaw_+=float(delta.x())*.14f;
        pitch_=std::clamp(pitch_-float(delta.y())*.14f,-89.f,89.f);
    }
    void focusOutEvent(QFocusEvent* event) override {
        pressed_.clear();rightMouse_=false;unsetCursor();
        QOpenGLWidget::focusOutEvent(event);
    }
private:
    void reportError(const QString& error){
        initialized_=false;
        if(onError)onError(error);
    }
    QVector3D front() const {
        const float yaw=yaw_*PI/180.f,pitch=pitch_*PI/180.f;
        return QVector3D(std::cos(yaw)*std::cos(pitch),
                         std::sin(pitch),std::sin(yaw)*std::cos(pitch)).normalized();
    }
    void tick(){
        const float seconds=std::clamp(float(frameClock_.restart())*.001f,0.f,.05f);
        const QVector3D f=front(),r=QVector3D::crossProduct(f,{0,1,0}).normalized();
        QVector3D motion;
        // AZERTY/flèches are the primary controls; preserve optional WASD.
        if(pressed_[Qt::Key_Z]||pressed_[Qt::Key_W]||pressed_[Qt::Key_Up])motion+=f;
        if(pressed_[Qt::Key_S]||pressed_[Qt::Key_Down])motion-=f;
        if(pressed_[Qt::Key_D]||pressed_[Qt::Key_Right])motion+=r;
        if(pressed_[Qt::Key_Q]||pressed_[Qt::Key_A]||pressed_[Qt::Key_Left])motion-=r;
        if(pressed_[Qt::Key_E]||pressed_[Qt::Key_Space]||pressed_[Qt::Key_PageUp])motion+=QVector3D(0,1,0);
        if(pressed_[Qt::Key_Control]||pressed_[Qt::Key_PageDown])motion-=QVector3D(0,1,0);
        if(motion.lengthSquared()>.001f){
            const float speed=pressed_[Qt::Key_Shift]?14.f:4.5f;
            position_+=motion.normalized()*speed*seconds;
        }
        update(); // Continuous GPU render, independent of mouse and CPU snapshot
    }
    void createBuffers(){
        const Scene& scene=runtime_->renderScene;
        opaque_.reserve(scene.obj.size()*3);
        for(const Obj& object:scene.obj){
            if(object.kind!=Kind::Triangle)continue;
            const auto vertex=[&](V p,V n,V uv)->Vertex {
                return {p.x,p.y,p.z,n.x,n.y,n.z,uv.x,uv.y,float(object.mat)};
            };
            Vertex vertices[3]={
                vertex(object.a,object.n0,object.uv0),
                vertex(object.b,object.n1,object.uv1),
                vertex(object.c,object.n2,object.uv2)
            };
            const bool glass=object.mat<scene.mat.size()&&
                             scene.mat[object.mat].transmissionPercent>0;
            if(glass){
                TransparentFace face{};
                std::memcpy(face.triangle,vertices,sizeof(vertices));
                face.center=(qv(object.a)+qv(object.b)+qv(object.c))/3.f;
                glass_.push_back(face);
            }else for(const auto& v:vertices)opaque_.push_back(v);
        }
        auto allocate=[this](GLuint& vao,GLuint& buffer,
                             const std::vector<Vertex>& vertices,GLenum usage){
            glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);
            glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
            glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex)),
                         vertices.empty()?nullptr:vertices.data(),usage);
            constexpr GLsizei stride=sizeof(Vertex);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(Vertex,x)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(Vertex,nx)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(Vertex,u)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offsetof(Vertex,material)));
            glBindVertexArray(0);
        };
        allocate(opaqueVao_,opaqueVbo_,opaque_,GL_STATIC_DRAW);
        if(!glass_.empty()){
            const std::vector<Vertex> empty(glass_.size()*3);
            allocate(transparentVao_,transparentVbo_,empty,GL_DYNAMIC_DRAW);
        }
        // Bounds of actual V16 triangles; no handcrafted V17 scene geometry.
        QVector3D minimum(1.e9f,1.e9f,1.e9f),maximum(-1.e9f,-1.e9f,-1.e9f);
        for(const auto& o:scene.obj){
            minimum.setX(std::min(minimum.x(),o.mn.x));
            minimum.setY(std::min(minimum.y(),o.mn.y));
            minimum.setZ(std::min(minimum.z(),o.mn.z));
            maximum.setX(std::max(maximum.x(),o.mx.x));
            maximum.setY(std::max(maximum.y(),o.mx.y));
            maximum.setZ(std::max(maximum.z(),o.mx.z));
        }
        center_=(minimum+maximum)*.5f;
        sceneRadius_=std::max(12.f,(maximum-minimum).length()*.68f);
    }
    void createTextures(){
        const Scene& scene=runtime_->renderScene;
        const int layers=std::max(1,int(scene.mat.size()));
        const int side=std::clamp(runtime_->textureResolution,64,512);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glGenTextures(4,textures_);
        const std::array<GLenum,3> formats{GL_SRGB8_ALPHA8,GL_RGBA8,GL_R8};
        const std::array<GLenum,3> layouts{GL_RGBA,GL_RGBA,GL_RED};
        for(int kind=0;kind<3;++kind){
            glBindTexture(GL_TEXTURE_2D_ARRAY,textures_[kind]);
            glTexImage3D(GL_TEXTURE_2D_ARRAY,0,formats[kind],side,side,layers,
                         0,layouts[kind],GL_UNSIGNED_BYTE,nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);
            std::vector<uint8_t> pixels(size_t(side)*side*(kind==2?1:4));
            for(int layer=0;layer<layers;++layer){
                const Material* mat=layer<int(scene.mat.size())?&scene.mat[size_t(layer)]:nullptr;
                for(int y=0;y<side;++y)for(int x=0;x<side;++x){
                    uint32_t rgba=0x00b4b4b4u;
                    uint16_t packed=kind==1?uint16_t(0x841fu):uint16_t(0x7befu);
                    if(mat){
                        if(kind==0&&!mat->colorMip.empty()&&!mat->colorMip[0].p.empty()){
                            const auto& mip=mat->colorMip[0];
                            rgba=mip.p[size_t(y*int(mip.size)/side)*mip.size+
                                       size_t(x*int(mip.size)/side)];
                        }else if(kind==1&&!mat->normalMip.empty()&&!mat->normalMip[0].p.empty()){
                            const auto& mip=mat->normalMip[0];
                            packed=mip.p[size_t(y*int(mip.size)/side)*mip.size+
                                         size_t(x*int(mip.size)/side)];
                        }else if(kind==2&&!mat->roughnessMip.empty()&&!mat->roughnessMip[0].p.empty()){
                            const auto& mip=mat->roughnessMip[0];
                            packed=mip.p[size_t(y*int(mip.size)/side)*mip.size+
                                         size_t(x*int(mip.size)/side)];
                        }
                    }
                    const size_t at=size_t(y)*side+x;
                    if(kind==0){
                        pixels[at*4]=uint8_t(channel(rgba,16));
                        pixels[at*4+1]=uint8_t(channel(rgba,8));
                        pixels[at*4+2]=uint8_t(channel(rgba,0));
                        pixels[at*4+3]=255;
                    }else if(kind==1){
                        pixels[at*4]=uint8_t(((packed>>11)&31u)*255u/31u);
                        pixels[at*4+1]=uint8_t(((packed>>5)&63u)*255u/63u);
                        pixels[at*4+2]=uint8_t((packed&31u)*255u/31u);
                        pixels[at*4+3]=255;
                    }else pixels[at]=uint8_t(((packed>>11)&31u)*255u/31u);
                }
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,layer,side,side,1,
                                layouts[kind],GL_UNSIGNED_BYTE,pixels.data());
            }
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        }
        std::array<float,256*4> properties{};
        for(int i=0;i<256;++i) {
            properties[size_t(i)*4]=0.f;
            properties[size_t(i)*4+1]=1.f;
            properties[size_t(i)*4+2]=0.f;
            properties[size_t(i)*4+3]=1.5f;
        }
        for(size_t i=0;i<scene.mat.size()&&i<256;++i){
            const auto& m=scene.mat[i];
            properties[i*4]=m.metallic;
            properties[i*4+1]=m.roughnessScale;
            properties[i*4+2]=float(m.transmissionPercent)/100.f;
            properties[i*4+3]=m.refractiveIndex;
        }
        glBindTexture(GL_TEXTURE_1D,textures_[3]);
        glTexImage1D(GL_TEXTURE_1D,0,GL_RGBA32F,256,0,GL_RGBA,GL_FLOAT,properties.data());
        glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    void createShadow(){
        constexpr int size=1024;
        glGenFramebuffers(1,&shadowFbo_);
        glGenTextures(1,&shadowDepth_);
        glBindTexture(GL_TEXTURE_2D,shadowDepth_);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,size,size,
                     0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
        const GLfloat border[4]={1,1,1,1};
        glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
        glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D,shadowDepth_,0);
        glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            reportError("Framebuffer d'ombres OpenGL incomplet.");
        glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());
    }
    void renderShadow(){
        shadowMatrix_.setToIdentity();
        QMatrix4x4 projection,view;
        projection.ortho(-sceneRadius_,sceneRadius_,-sceneRadius_,sceneRadius_,
                         0.1f,sceneRadius_*5.f);
        QVector3D up(0,1,0);
        if(std::abs(QVector3D::dotProduct(sun_,up))>.96f)up={1,0,0};
        view.lookAt(center_+sun_*sceneRadius_*2.2f,center_,up);
        shadowMatrix_=projection*view;
        glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo_);
        glViewport(0,0,1024,1024);
        glClearDepth(1.0);glClear(GL_DEPTH_BUFFER_BIT);
        glDisable(GL_BLEND);
        depthShader_.bind();
        depthShader_.setUniformValue("uLightSpace",shadowMatrix_);
        glBindVertexArray(opaqueVao_);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(opaque_.size()));
        glBindVertexArray(0);
        depthShader_.release();
        shadowDirty_=false;
    }
    void renderColor(){
        target_->bind();
        glViewport(0,0,renderWidth_,renderHeight_);
        const QVector3D clear=skyColor_*.95f+QVector3D(.12f,.12f,.12f);
        glClearColor(clear.x(),clear.y(),clear.z(),1.f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        QMatrix4x4 view,projection;
        view.lookAt(position_,position_+front(),{0,1,0});
        projection.perspective(fov_,float(renderWidth_)/renderHeight_,.07f,1300.f);
        shader_.bind();
        shader_.setUniformValue("uView",view);
        shader_.setUniformValue("uProjection",projection);
        shader_.setUniformValue("uLightSpace",shadowMatrix_);
        shader_.setUniformValue("uCamera",position_);
        shader_.setUniformValue("uSun",sun_);
        shader_.setUniformValue("uSunColor",sunColor_);
        shader_.setUniformValue("uSkyColor",skyColor_);
        shader_.setUniformValue("uGroundFill",fill_);
        shader_.setUniformValue("uExposure",1.08f);
        shader_.setUniformValue("uTime",hour_);
        constexpr int maxLights=8;
        const auto& lights=runtime_->renderScene.lights;
        const int count=std::min(maxLights,int(lights.size()));
        shader_.setUniformValue("uLightCount",count);
        for(int i=0;i<count;++i){
            const auto& light=lights[size_t(i)];
            shader_.setUniformValue(("uLightPositionRadius["+QByteArray::number(i)+"]").constData(),
                QVector4D(light.p.x,light.p.y,light.p.z,light.radius));
            shader_.setUniformValue(("uLightColorIntensity["+QByteArray::number(i)+"]").constData(),
                QVector4D(light.color.x/255.f,light.color.y/255.f,light.color.z/255.f,light.intensity));
        }
        for(int i=0;i<3;++i){
            glActiveTexture(GL_TEXTURE0+i);
            glBindTexture(GL_TEXTURE_2D_ARRAY,textures_[i]);
        }
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_1D,textures_[3]);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D,shadowDepth_);
        shader_.setUniformValue("uAlbedo",0);
        shader_.setUniformValue("uNormalMap",1);
        shader_.setUniformValue("uRoughnessMap",2);
        shader_.setUniformValue("uMaterialProps",3);
        shader_.setUniformValue("uShadowMap",4);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glBindVertexArray(opaqueVao_);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(opaque_.size()));
        if(!glass_.empty()){
            // Transparent window triangles remain independent of opaque BVH.
            // Sorting is O(N log N) on a small glass-only list.
            std::vector<size_t> order(glass_.size());
            std::iota(order.begin(),order.end(),size_t(0));
            std::sort(order.begin(),order.end(),[this](size_t a,size_t b){
                return (glass_[a].center-position_).lengthSquared()>
                       (glass_[b].center-position_).lengthSquared();
            });
            std::vector<Vertex> sorted;
            sorted.reserve(glass_.size()*3);
            for(size_t index:order)
                for(const auto& vert:glass_[index].triangle)sorted.push_back(vert);
            glBindVertexArray(transparentVao_);
            glBindBuffer(GL_ARRAY_BUFFER,transparentVbo_);
            glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(sorted.size()*sizeof(Vertex)),
                         sorted.data(),GL_DYNAMIC_DRAW);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glDrawArrays(GL_TRIANGLES,0,GLsizei(sorted.size()));
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        glBindVertexArray(0);
        shader_.release();
        target_->release();
    }
    std::shared_ptr<v16::Runtime> runtime_;
    QOpenGLShaderProgram shader_,depthShader_;
    std::vector<Vertex> opaque_;
    std::vector<TransparentFace> glass_;
    GLuint opaqueVao_=0,opaqueVbo_=0,transparentVao_=0,transparentVbo_=0;
    GLuint textures_[4]={};
    GLuint shadowFbo_=0,shadowDepth_=0;
    GLuint timerQueries_[2]={};
    bool queryPending_[2]={false,false};
    int querySlot_=0;
    std::unique_ptr<QOpenGLFramebufferObject> target_;
    QMatrix4x4 shadowMatrix_;
    QVector3D center_,position_,sun_,sunColor_,skyColor_,fill_;
    float sceneRadius_=30.f,yaw_=-90.f,pitch_=0.f,fov_=60.f,hour_=14.75f;
    float gpuMs_=0.f;
    int renderWidth_=1366,renderHeight_=768,statusFrames_=0;
    bool initialized_=false,shadowDirty_=true,rightMouse_=false;
    QPointF lastMouse_;
    QHash<int,bool> pressed_;
    QTimer loop_;
    QElapsedTimer frameClock_,fpsClock_;
};

class RealtimeWindow final : public QMainWindow {
public:
    RealtimeWindow(std::shared_ptr<v16::Runtime> runtime,QString scenePath)
       :runtime_(std::move(runtime)),scenePath_(std::move(scenePath)){
        setWindowTitle(QStringLiteral("CSE WARP Beta X — V17 · Exploration OpenGL"));
        resize(1320,780);
        view_=new RealtimeView(runtime_);
        setCentralWidget(view_);
        createUi();
        view_->onPerformance=[this](float fps,float gpuMs){
            performance_->setText(QStringLiteral("Images : %1 FPS | GPU raster : %2 ms")
                             .arg(fps,0,'f',1).arg(gpuMs,0,'f',2));
        };
        view_->onError=[this](const QString& error){
            statusBar()->showMessage(error);
            performance_->setText(error);
        };
        view_->setFocus();
    }
    // Headless CI uses a real OpenGL context under Xvfb/Mesa, not fake
    // shader-only parsing, and saves the actual GPU framebuffer.
    void prepareGpuSmoke(){ view_->setRenderSize(640,360); }
    bool captureGpuSmoke(const QString& path){ return view_->saveCapture(path); }
private:
    void createUi(){
        QMenu* file=menuBar()->addMenu("Fichier");
        QAction* capture=file->addAction("Exporter l'image OpenGL (PNG)...");
        connect(capture,&QAction::triggered,this,[this]{
            const QString path=QFileDialog::getSaveFileName(
                this,"Capturer V17","v17-pavillon01.png","Images PNG (*.png)");
            if(!path.isEmpty()&&!view_->saveCapture(path))
                QMessageBox::warning(this,"Capture V17","Impossible d'exporter la capture.");
        });
        QAction* openCpu=file->addAction("Ouvrir le moteur CPU / HLSL V16");
        connect(openCpu,&QAction::triggered,this,[this]{
            const QString exe=QCoreApplication::applicationDirPath()+"/moteur3d_v16.exe";
            if(!QFileInfo::exists(exe) || !QProcess::startDetached(exe,{scenePath_}))
                QMessageBox::warning(this,"V16","Le moteur V16 est introuvable à côté de V17.");
        });
        file->addSeparator();
        QAction* quit=file->addAction("Quitter");
        connect(quit,&QAction::triggered,this,&QWidget::close);
        auto* dock=new QDockWidget("Exploration V17",this);
        auto* pane=new QWidget;
        auto* layout=new QVBoxLayout(pane);
        auto* form=new QFormLayout;
        auto* cameras=new QComboBox;
        for(const auto& c:viewRuntime()->cameras)cameras->addItem(c.name);
        connect(cameras,qOverload<int>(&QComboBox::currentIndexChanged),view_,&RealtimeView::selectCamera);
        form->addRow("Caméra",cameras);
        auto* resolution=new QComboBox;
        resolution->addItem("Dell natif 1366 × 768",QSize(1366,768));
        resolution->addItem("Full HD 1920 × 1080",QSize(1920,1080));
        resolution->addItem("QHD 2560 × 1440",QSize(2560,1440));
        connect(resolution,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,resolution]{
            const QSize s=resolution->currentData().toSize();
            view_->setRenderSize(s.width(),s.height());
        });
        form->addRow("Résolution GPU",resolution);
        auto* hour=new QDoubleSpinBox;
        hour->setRange(7,19);hour->setSingleStep(.25);hour->setDecimals(2);
        hour->setSuffix(" h");
        hour->setValue(viewRuntime()->daylightHour);
        connect(hour,qOverload<double>(&QDoubleSpinBox::valueChanged),view_,
                [this](double value){view_->setHour(float(value));});
        form->addRow("Heure solaire",hour);
        layout->addLayout(form);
        performance_=new QLabel("Calcul des FPS réels et du temps GPU...");
        performance_->setWordWrap(true);
        layout->addWidget(performance_);
        auto* instructions=new QLabel(
            "Fleches ou ZQSD : deplacement (WASD aussi)\n"
            "Souris + bouton droit : regarder\n"
            "E / Espace : monter, Ctrl : descendre\n"
            "Maj : accélérer\n"
            "Touches 1 à 6 : caméras\n"
            "F11 : plein écran, Échap : libérer souris\n\n"
            "OpenGL GPU temps réel : sans copie CPU par image.\n"
            "Le CPU GI et le backend Compute V16 restent disponibles.");
        instructions->setWordWrap(true);
        layout->addWidget(instructions);
        auto* button=new QPushButton("Ouvrir V16 — comparaison CPU/GPU");
        connect(button,&QPushButton::clicked,openCpu,&QAction::trigger);
        layout->addWidget(button);
        layout->addStretch(1);
        dock->setWidget(pane);
        addDockWidget(Qt::RightDockWidgetArea,dock);
        dock->setMinimumWidth(288);
        statusBar()->showMessage("V17 · rasterisation OpenGL 3.3, PBR, ombres GPU et navigation libre");
    }
    const v16::Runtime* viewRuntime()const{return runtime_.get();}
private:
    std::shared_ptr<v16::Runtime> runtime_;
    QString scenePath_;
    RealtimeView* view_=nullptr;
    QLabel* performance_=nullptr;
};
QString defaultScenePath(){
    const QString app=QCoreApplication::applicationDirPath();
    for(const QString& candidate : {
            app+"/pavillon01/scene.json",
            QDir::currentPath()+"/scenes/v16/pavillon01/scene.json"}){
        if(QFileInfo::exists(candidate))return candidate;
    }
    return app+"/pavillon01/scene.json";
}
std::shared_ptr<v16::Runtime> openScene(const QString& path,QString& error) {
    const auto loaded=v16::loadScene(path);
    if(!loaded){error=loaded.error;return {};}
    auto compiled=v16::buildRuntime(loaded.scene,512);
    if(!compiled){error=compiled.error;return {};}
    return std::make_shared<v16::Runtime>(std::move(compiled.runtime));
}
}

int main(int argc,char** argv){
    const bool selfTest=argc>=2&&QString::fromLocal8Bit(argv[1])=="--self-test";
    const bool gpuSmoke=argc>=2&&QString::fromLocal8Bit(argv[1])=="--smoke-gpu";
    if(selfTest){
        QCoreApplication app(argc,argv);
        const QString path=argc>=3?QString::fromLocal8Bit(argv[2]):defaultScenePath();
        QString error;
        const auto scene=openScene(path,error);
        if(!scene||scene->cameras.empty()||scene->renderScene.obj.empty()||
           scene->renderScene.mat.empty()){
            qCritical("V17 scene reuse test failed: %s",qPrintable(error));
            return 1;
        }
        qInfo("V17 scene reuse passed: %lld triangles, %lld materials, %lld cameras",
              static_cast<long long>(scene->renderScene.obj.size()),
              static_cast<long long>(scene->renderScene.mat.size()),
              static_cast<long long>(scene->cameras.size()));
        return 0;
    }
    QSurfaceFormat format;
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);
    const QString path=gpuSmoke?
        (argc>=4?QString::fromLocal8Bit(argv[3]):defaultScenePath()):
        (argc>=2?QString::fromLocal8Bit(argv[1]):defaultScenePath());
    QString error;
    const auto runtime=openScene(path,error);
    if(!runtime){
        QMessageBox::critical(nullptr,"CSE WARP V17","Impossible de charger la scène V16 : "+error);
        return 2;
    }
    RealtimeWindow window(runtime,path);
    if(gpuSmoke)window.prepareGpuSmoke();
    window.show();
    if(gpuSmoke){
        const QString png=argc>=3?QString::fromLocal8Bit(argv[2]):"v17-gpu-smoke.png";
        QTimer::singleShot(4500,&app,[&app,&window,png]{
            const bool captured=window.captureGpuSmoke(png);
            std::fprintf(stderr,"V17 Mesa OpenGL capture %s: %s\n",
                         captured?"OK":"FAILED",qPrintable(png));
            std::fflush(stderr);
            app.exit(captured?0:5);
        });
    }
    return app.exec();
}
