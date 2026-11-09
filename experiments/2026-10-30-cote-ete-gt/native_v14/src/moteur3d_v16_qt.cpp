#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace m3d;
namespace {
struct Result {
    QImage image;
    QString renderer;
    QString error;
    double milliseconds=0;
    bool gpu=false;
};
class ImageView final : public QWidget {
public:
    void setImage(QImage im){image_=std::move(im);update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(),QColor(22,26,31));
        if(image_.isNull()) {
            p.setPen(QColor(180,191,204));
            p.drawText(rect(),Qt::AlignCenter,QStringLiteral("Pavillon 01 — charger une scène V16"));
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
};
class V16Window final : public QMainWindow {
public:
    explicit V16Window(QString scenePath){
        setWindowTitle(QStringLiteral("Moteur3D V16 — Pavillon 01"));
        resize(1510,900);
        setMinimumSize(1100,670);
        canvas_=new ImageView;
        setCentralWidget(canvas_);
        createUi();
        connect(&watcher_,&QFutureWatcher<Result>::finished,this,[this]{
            const auto outcome=watcher_.result();
            if(!outcome.image.isNull()){lastImage_=outcome.image;canvas_->setImage(outcome.image);}
            statusBar()->showMessage(outcome.error.isEmpty()
                ?QStringLiteral("%1 — %2 ms").arg(outcome.renderer).arg(outcome.milliseconds,0,'f',1)
                :outcome.error,15000);
            if(pending_){pending_=false;requestRender();}
        });
        if(!scenePath.isEmpty())openScene(scenePath);
    }
    ~V16Window() override {watcher_.waitForFinished();}
private:
    ImageView* canvas_=nullptr;
    QTreeWidget* sceneTree_=nullptr;
    QTreeWidget* resourceTree_=nullptr;
    QComboBox* cameraBox_=nullptr;
    QComboBox* backendBox_=nullptr;
    QComboBox* qualityBox_=nullptr;
    QSlider* azimuth_=nullptr;
    QDoubleSpinBox* roughness_=nullptr;
    QDoubleSpinBox* metallic_=nullptr;
    QLabel* materialLabel_=nullptr;
    QFutureWatcher<Result> watcher_;
    std::shared_ptr<v16::Runtime> runtime_;
    bool pending_=false, updatingControls_=false;
    int selectedMaterial_=-1;
    QString scenePath_;

    void createUi(){
        auto* file=menuBar()->addMenu(QStringLiteral("Fichier"));
        auto* open=file->addAction(QStringLiteral("Ouvrir Pavillon 01 (scene.json)…"));
        connect(open,&QAction::triggered,this,[this]{
            const QString chosen=QFileDialog::getOpenFileName(
                this,QStringLiteral("Scène V16"),QFileInfo(scenePath_).absolutePath(),
                QStringLiteral("Manifeste V16 (scene.json);;JSON (*.json)"));
            if(!chosen.isEmpty())openScene(chosen);
        });
        auto* save=file->addAction(QStringLiteral("Exporter l'image PNG…"));
        connect(save,&QAction::triggered,this,[this]{
            if(lastImage_.isNull()){QMessageBox::information(this,"PNG","Aucune image calculée.");return;}
            const QString path=QFileDialog::getSaveFileName(this,"Exporter PNG","pavillon01-v16.png","PNG (*.png)");
            if(!path.isEmpty()&&!lastImage_.save(path,"PNG"))
                QMessageBox::warning(this,"PNG","Échec de l'enregistrement.");
        });
        auto* quit=file->addAction(QStringLiteral("Quitter"));
        connect(quit,&QAction::triggered,this,&QWidget::close);

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
        backendBox_->addItems({QStringLiteral("CPU Ray Tracing PBR"),QStringLiteral("GPU D3D11 Compute — aperçu")});
#ifndef _WIN32
        backendBox_->setItemData(1,0,Qt::UserRole-1);
#endif
        qualityBox_=new QComboBox;
        qualityBox_->addItems({QStringLiteral("Aperçu 480 × 270"),QStringLiteral("Standard 800 × 450"),QStringLiteral("Haute qualité 1280 × 720")});
        qualityBox_->setCurrentIndex(1);
        azimuth_=new QSlider(Qt::Horizontal);
        azimuth_->setRange(-180,180);
        azimuth_->setValue(0);
        form->addRow(QStringLiteral("Caméra"),cameraBox_);
        form->addRow(QStringLiteral("Backend"),backendBox_);
        form->addRow(QStringLiteral("Résolution"),qualityBox_);
        form->addRow(QStringLiteral("Rotation caméra"),azimuth_);
        vertical->addLayout(form);
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
        connect(cameraBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){azimuth_->setValue(0);requestRender();});
        connect(backendBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){requestRender();});
        connect(qualityBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){requestRender();});
        connect(azimuth_,&QSlider::sliderReleased,this,[this]{requestRender();});
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
        if(watcher_.isRunning())watcher_.waitForFinished();
        auto& m=runtime_->renderScene.mat[size_t(selectedMaterial_)];
        m.roughnessScale=float(roughness_->value());
        m.metallic=float(metallic_->value());
        requestRender();
    }
    void openScene(const QString& path){
        if(watcher_.isRunning())watcher_.waitForFinished();
        auto loaded=v16::loadScene(path);
        if(!loaded){QMessageBox::critical(this,"Fichier scene.json invalide",loaded.error);return;}
        statusBar()->showMessage(QStringLiteral("Construction des triangles, BVH et mipmaps PBR…"));
        QApplication::processEvents();
        // 2048-pixel mip chains are built once and shared by every material reference.
        auto build=v16::buildRuntime(loaded.scene,2048);
        if(!build){QMessageBox::critical(this,"Scène V16",build.error);return;}
        auto replacement=std::make_shared<v16::Runtime>(std::move(build.runtime));
        runtime_=std::move(replacement);
        scenePath_=path;
        selectedMaterial_=-1;
        cameraBox_->blockSignals(true);
        cameraBox_->clear();
        for(const auto& c:runtime_->cameras)cameraBox_->addItem(c.name);
        cameraBox_->blockSignals(false);
        populateTrees(loaded.scene);
        statusBar()->showMessage(QStringLiteral("Pavillon 01 — %1 objets, %2 triangles, %3 matériaux, PBR %4 px")
            .arg(runtime_->objects.size())
            .arg(runtime_->renderScene.obj.size())
            .arg(runtime_->renderScene.mat.size())
            .arg(runtime_->textureResolution));
        requestRender();
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
            auto* leaf=new QTreeWidgetItem(catalog,{it.key(),QStringLiteral("PBR + mipmaps")});
            leaf->setData(0,Qt::UserRole,it.key());
        }
        sceneTree_->expandToDepth(2);
        resourceTree_->expandToDepth(0);
        catalog->setExpanded(true);
    }
    void requestRender(){
        if(!runtime_||runtime_->cameras.isEmpty())return;
        if(watcher_.isRunning()){pending_=true;return;}
        auto snapshot=runtime_;
        const int presetId=std::clamp(cameraBox_->currentIndex(),0,int(snapshot->cameras.size())-1);
        auto preset=snapshot->cameras[presetId];
        const auto dir=preset.eye-preset.target;
        const float angle=float(azimuth_->value())*PI/180;
        preset.eye={preset.target.x+dir.x*std::cos(angle)+dir.z*std::sin(angle),
                    preset.eye.y,
                    preset.target.z-dir.x*std::sin(angle)+dir.z*std::cos(angle)};
        const int w=qualityBox_->currentIndex()==0?480:qualityBox_->currentIndex()==1?800:1280;
        const int h=w*9/16;
        const bool wantGpu=backendBox_->currentIndex()==1;
        statusBar()->showMessage(QStringLiteral("Calcul de l'image…"));
        watcher_.setFuture(QtConcurrent::run([snapshot,preset,w,h,wantGpu]{
            Result result;
            updateLighting(15.f);
            const auto camera=v16::makeCamera(preset,w,h);
            std::vector<uint32_t> pixels;
            Stats stats;
            bool ok=false;
            if(wantGpu){
                std::wstring reason;
                ok=renderCompute11Preview(snapshot->renderScene,camera,w,h,pixels,stats,reason);
                if(ok)result.renderer=QStringLiteral("GPU D3D11 Compute (aperçu géométrique, PBR CPU)");
                else result.error=QStringLiteral("GPU indisponible (%1) — retour CPU. ").arg(QString::fromStdWString(reason));
            }
            if(!ok){
                Stats shadowStats;
                const auto shadow=buildShadow(snapshot->renderScene,shadowStats);
                std::atomic<uint64_t> generation{1};
                stats=m3d::render(snapshot->renderScene,shadow,camera,w,h,pixels,
                             std::max(1u,std::min(8u,std::thread::hardware_concurrency())),
                             false,generation,1,1,false,false,false,3);
                result.renderer=QStringLiteral("CPU Ray Tracing — PBR et mipmaps");
            }else result.gpu=true;
            result.milliseconds=stats.ms;
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
    const QString path=argc>1?QString::fromLocal8Bit(argv[1]):defaultPath();
    V16Window window(path);
    window.show();
    return app.exec();
}
