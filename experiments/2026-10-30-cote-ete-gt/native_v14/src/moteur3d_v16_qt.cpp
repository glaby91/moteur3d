#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
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
#include <QDateTime>
#include <QStandardPaths>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
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
#include <memory>
#include <mutex>
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
        setWindowTitle(QStringLiteral("CSE WARP Beta X — Scène V16"));
        resize(1510,900);
        setMinimumSize(1100,670);
        canvas_=new ImageView;
        setCentralWidget(canvas_);
        createUi();
        // A permanent status-bar widget outlives temporary render/progress
        // messages. QStatusBar::showMessage(..., 15000) used to erase the
        // completed render time after 15 seconds.
        renderTimeLabel_=new QLabel(QStringLiteral("Dernier rendu : —"),this);
        renderTimeLabel_->setObjectName(QStringLiteral("v16LastRenderTime"));
        statusBar()->addPermanentWidget(renderTimeLabel_);
        connect(&watcher_,&QFutureWatcher<Result>::finished,this,[this]{
            const auto outcome=watcher_.result();
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
            if(pending_){pending_=false;requestRender();}
        });
        if(!scenePath.isEmpty())openScene(scenePath);
    }
    ~V16Window() override {renderSerial_->fetch_add(1);watcher_.waitForFinished();}
private:
    ImageView* canvas_=nullptr;
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
    bool pending_=false, updatingControls_=false;
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
        auto* benchmarkAction=toolsMenu->addAction(QStringLiteral("Benchmark complet…"));
        connect(benchmarkAction,&QAction::triggered,this,[this]{
            if(scenePath_.isEmpty()){
                QMessageBox::information(this,QStringLiteral("Benchmark"),QStringLiteral("Charger Pavillon 01 avant de lancer le benchmark."));
                return;
            }
            const QString path=QCoreApplication::applicationDirPath()
                +QStringLiteral("/moteur3d_v16_benchui.exe");
            if(!QFileInfo::exists(path)){
                QMessageBox::warning(this,QStringLiteral("Benchmark indisponible"),
                                     QStringLiteral("Programme de benchmark introuvable : %1").arg(path));
                return;
            }
            if(!QProcess::startDetached(path,{scenePath_},QCoreApplication::applicationDirPath()))
                QMessageBox::critical(this,QStringLiteral("Benchmark"),QStringLiteral("Impossible d'ouvrir la fenêtre de benchmark."));
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
            QStringLiteral("CPU Éclairage global — 4 rebonds (qualité réglable)")
        });
#ifndef _WIN32
        backendBox_->setItemData(1,0,Qt::UserRole-1);
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
        connect(backendBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){
            const bool gi=index>=2;
            directSamples_->setEnabled(index==0);
            denoiseDirect_->setEnabled(index==0);
            giSamples_->setEnabled(gi);
            progressiveGi_->setEnabled(gi);
            denoiseGi_->setEnabled(gi);
            requestRender();
        });
        connect(qualityBox_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){requestRender();});
        connect(directSamples_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){if(backendBox_->currentIndex()==0)requestRender();});
        connect(giSamples_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){if(backendBox_->currentIndex()>=2)requestRender();});
        connect(progressiveGi_,&QCheckBox::toggled,this,[this](bool){if(backendBox_->currentIndex()>=2)requestRender();});
        connect(denoiseGi_,&QCheckBox::toggled,this,[this](bool){if(backendBox_->currentIndex()>=2)requestRender();});
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
        renderSerial_->fetch_add(1);
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
        setWindowTitle(QStringLiteral("CSE WARP Beta X — %1").arg(runtime_->title));
        sunHour_->blockSignals(true);
        sunHour_->setValue(double(runtime_->daylightHour));
        sunHour_->blockSignals(false);
        scenePath_=path;
        materialsPath_.clear();
        for(const auto& sector:loaded.scene.sectors){
            const auto entries=sector.resources.value(QStringLiteral("materials"));
            if(!entries.isEmpty()){
                materialsPath_=QDir(QFileInfo(path).absolutePath()).absoluteFilePath(entries.front().path);
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
        populateTrees(loaded.scene);
        statusBar()->showMessage(QStringLiteral("%1 — %2 objets, %3 triangles, %4 matériaux, PBR %5 px")
            .arg(runtime_->title)
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
        if(!runtime_||runtime_->cameras.isEmpty())return;
        const auto serial=renderSerial_;
        const uint64_t token=serial->fetch_add(1)+1;
        if(watcher_.isRunning()){pending_=true;return;}
        auto snapshot=runtime_;
        const int presetId=std::clamp(cameraBox_->currentIndex(),0,int(snapshot->cameras.size())-1);
        auto preset=snapshot->cameras[presetId];
        const auto dir=preset.eye-preset.target;
        const float angle=float(azimuth_->value())*PI/180;
        preset.eye={preset.target.x+dir.x*std::cos(angle)+dir.z*std::sin(angle),
                    preset.eye.y,
                    preset.target.z-dir.x*std::sin(angle)+dir.z*std::cos(angle)};
        const QSize renderSize=qualityBox_->currentData().toSize();
        const int w=renderSize.width();
        const int h=renderSize.height();
        const int backend=backendBox_->currentIndex();
        const bool wantGpu=backend==1;
        const bool globalIllumination=backend>=2;
        const bool edgeAA=edgeAa_->isChecked();
        const unsigned sampleCount=globalIllumination?
            unsigned(giSamples_->currentData().toInt()):
            (wantGpu?1u:unsigned(directSamples_->currentData().toInt()));
        const unsigned maxBounces=backend==2?2u:4u;
        const bool progressive=globalIllumination&&progressiveGi_->isChecked();
        const bool denoise=globalIllumination&&denoiseGi_->isChecked();
        const bool denoiseDirect=!globalIllumination&&!wantGpu&&denoiseDirect_->isChecked();
        statusBar()->showMessage(QStringLiteral("Calcul de l'image…"));
        const auto shadowCache=cpuShadows_;
        watcher_.setFuture(QtConcurrent::run([this,serial,token,snapshot,preset,w,h,wantGpu,
            globalIllumination,edgeAA,sampleCount,maxBounces,progressive,denoise,denoiseDirect,shadowCache]{
            Result result;
            updateLighting(snapshot->daylightHour);
            const auto camera=v16::makeCamera(preset,w,h);
            std::vector<uint32_t> pixels;
            Stats stats;
            bool ok=false;
            if(wantGpu){
                std::wstring reason;
                ok=renderCompute11Preview(snapshot->renderScene,camera,w,h,pixels,stats,reason);
                if(ok)result.renderer=QStringLiteral("GPU D3D11 Compute — PBR simplifié, ombres et verre");
                else result.error=QStringLiteral("GPU indisponible (%1) — retour CPU. ").arg(QString::fromStdWString(reason));
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
                std::atomic<uint64_t> generation{1};
                stats=m3d::render(snapshot->renderScene,*shadow,camera,w,h,pixels,
                                  workers,false,generation,1,sampleCount,false,false,false,3);
                if(denoiseDirect) pixels=denoiseBilateral(pixels,w,h,workers);
                result.renderer=shadowReused
                    ?QStringLiteral("CPU Ray Tracing — %1 spp, ombres en cache").arg(sampleCount)
                    :QStringLiteral("CPU Ray Tracing — %1 spp, ombres calculées").arg(sampleCount);
            }else result.gpu=true;
            result.milliseconds=stats.ms;
            result.renderer+=QStringLiteral(" — %1×%2").arg(w).arg(h);
            if(edgeAA && pixels.size()==size_t(w)*size_t(h))
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
    const QString path=argc>1?QString::fromLocal8Bit(argv[1]):defaultPath();
    V16Window window(path);
    window.show();
    return app.exec();
}
