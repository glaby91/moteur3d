#pragma once

#include <QCloseEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <utility>

namespace cse::diagnostics {

struct DashboardData {
    QString engineVersion;
    QString engineRevision;
    QString sceneName;
    QString sceneRevision;
    QString backend;
    QString presentation;
    QString diagnosticStatus;
    QString diagnosticDetail;
    QString cameraPosition;
    int renderWidth=0;
    int renderHeight=0;
    int sourceObjects=0;
    int triangles=0;
    int materials=0;
    int imageTextures=0;
    int lights=0;
    int systemMemoryPercent=-1;
    int textureResolution=0;
    double fps=0.0;
    double lastRenderMs=0.0;
    double solarHour=0.0;
    double daylight=0.0;
    double lampFactor=0.0;
    bool sceneLoading=false;
    bool exploring=false;
};

class Dashboard final : public QWidget {
public:
    explicit Dashboard(QWidget* parent=nullptr)
        :QWidget(parent,Qt::Window|Qt::FramelessWindowHint){
        setWindowTitle(QStringLiteral("CSE WARP — Diagnostic moteur"));
        setFocusPolicy(Qt::StrongFocus);
        setStyleSheet(QStringLiteral(
            "QWidget { background:#101817; color:#d9e5df; font-family:'Segoe UI'; }"
            "QLabel#eyebrow { color:#9ab4a6; font-size:10pt; letter-spacing:2px; }"
            "QLabel#title { color:#e8f2ec; font-size:24pt; font-weight:700; }"
            "QLabel#subtitle { color:#9eafa7; font-size:10pt; }"
            "QLabel#section { color:#9dd4bf; font-size:11pt; font-weight:700; }"
            "QLabel#metric { color:#f1f5ee; font-family:'Consolas'; font-size:21pt; font-weight:700; }"
            "QLabel#smallMetric { color:#d9e5df; font-family:'Consolas'; font-size:13pt; }"
            "QLabel#muted { color:#9eafa7; font-size:9pt; }"
            "QFrame#card { background:#1a2522; border:1px solid #3b554a; border-radius:10px; }"
            "QTabWidget::pane { border:1px solid #31463d; border-radius:8px; top:-1px; }"
            "QTabBar::tab { background:#1b2723; color:#afc0b7; padding:12px 24px;"
            " margin-right:5px; border:1px solid #31463d; border-bottom:none; }"
            "QTabBar::tab:selected { background:#283a32; color:#dff6e7; }"
            "QPushButton { background:#29483b; color:#e6f4eb; border:1px solid #638b73;"
            " border-radius:7px; padding:10px 18px; font-weight:700; }"
            "QPushButton:hover { background:#365d49; }"));
        auto* root=new QVBoxLayout(this);
        root->setContentsMargins(34,26,34,22);
        root->setSpacing(16);

        auto* heading=new QGridLayout;
        auto* eyebrow=new QLabel(QStringLiteral("CSE WARP  /  TELEMETRIE EN DIRECT"));
        eyebrow->setObjectName(QStringLiteral("eyebrow"));
        auto* title=new QLabel(QStringLiteral("Tableau de bord moteur"));
        title->setObjectName(QStringLiteral("title"));
        auto* subtitle=new QLabel(QStringLiteral("Diagnostic de la scène en cours · actualisé en temps réel"));
        subtitle->setObjectName(QStringLiteral("subtitle"));
        heading->addWidget(eyebrow,0,0);
        heading->addWidget(title,1,0);
        heading->addWidget(subtitle,2,0);
        auto* returnButton=new QPushButton(QStringLiteral("RETOUR À LA SCÈNE  ·  TAB"));
        heading->addWidget(returnButton,0,1,3,1,Qt::AlignRight|Qt::AlignVCenter);
        connect(returnButton,&QPushButton::clicked,this,[this]{requestReturn();});
        root->addLayout(heading);

        auto* identity=new QGridLayout;
        identity->setSpacing(12);
        engineValue_=addCard(identity,0,QStringLiteral("MOTEUR"));
        sceneValue_=addCard(identity,1,QStringLiteral("SCÈNE"));
        backendValue_=addCard(identity,2,QStringLiteral("RENDU"));
        root->addLayout(identity);

        pages_=new QTabWidget;
        pages_->addTab(makeOverviewPage(),QStringLiteral("APERÇU"));
        pages_->addTab(makeScenePage(),QStringLiteral("SCÈNE"));
        pages_->addTab(makeLightingPage(),QStringLiteral("LUMIÈRES"));
        pages_->addTab(makeResourcesPage(),QStringLiteral("RESSOURCES"));
        pages_->addTab(makeDiagnosticsPage(),QStringLiteral("DIAGNOSTIC"));
        root->addWidget(pages_,1);

        auto* footer=new QLabel(QStringLiteral(
            "TAB : retour à la scène    ·    ÉCHAP : arrêter le mode caméra    ·    Les valeurs indisponibles sont signalées, jamais estimées. En affichage direct, le temps peut mesurer la soumission CPU, pas l’exécution GPU."));
        footer->setObjectName(QStringLiteral("muted"));
        footer->setWordWrap(true);
        root->addWidget(footer);
        const auto children=findChildren<QWidget*>();
        for(auto* child:children)child->installEventFilter(this);
    }

    void setReturnHandler(std::function<void()> handler){returnHandler_=std::move(handler);}
    void setExitHandler(std::function<void()> handler){exitHandler_=std::move(handler);}

    void setData(const DashboardData& d){
        engineValue_->setText(QStringLiteral("%1\nRévision %2")
            .arg(d.engineVersion,d.engineRevision.isEmpty()?QStringLiteral("n/d"):d.engineRevision));
        sceneValue_->setText(QStringLiteral("%1\nRévision %2")
            .arg(d.sceneName.isEmpty()?QStringLiteral("Aucune scène"):d.sceneName,
                 d.sceneRevision.isEmpty()?QStringLiteral("non renseignée"):d.sceneRevision));
        backendValue_->setText(QStringLiteral("%1\n%2")
            .arg(d.backend.isEmpty()?QStringLiteral("Backend n/d"):d.backend,d.presentation));

        fpsValue_->setText(d.fps>0.0?QStringLiteral("%1 FPS").arg(d.fps,0,'f',1)
                                     :QStringLiteral("— FPS"));
        renderValue_->setText(d.lastRenderMs>0.0
            ?QStringLiteral("%1 ms").arg(d.lastRenderMs,0,'f',1):QStringLiteral("— ms"));
        resolutionValue_->setText(d.renderWidth>0&&d.renderHeight>0
            ?QStringLiteral("%1 × %2").arg(d.renderWidth).arg(d.renderHeight)
            :QStringLiteral("n/d"));
        cameraValue_->setText(d.cameraPosition.isEmpty()?QStringLiteral("n/d"):d.cameraPosition);

        objectValue_->setText(QString::number(d.sourceObjects));
        triangleValue_->setText(QString::number(d.triangles));
        materialValue_->setText(QString::number(d.materials));
        textureValue_->setText(QString::number(d.imageTextures));
        lightValue_->setText(QString::number(d.lights));

        pointLightValue_->setText(QStringLiteral("%1 lumières ponctuelles de scène").arg(d.lights));
        solarValue_->setText(QStringLiteral("%1 h").arg(d.solarHour,0,'f',2));
        daylightValue_->setText(QStringLiteral("%1").arg(d.daylight,0,'f',2));
        lampValue_->setText(QStringLiteral("%1").arg(d.lampFactor,0,'f',2));

        ramValue_->setText(d.systemMemoryPercent>=0
            ?QStringLiteral("%1 %").arg(d.systemMemoryPercent):QStringLiteral("n/d"));
        textureResolutionValue_->setText(d.textureResolution>0
            ?QStringLiteral("%1 px max.").arg(d.textureResolution):QStringLiteral("n/d"));
        processMemoryValue_->setText(QStringLiteral("Non instrumentée"));
        vramValue_->setText(QStringLiteral("Non publiée par le backend"));

        statusValue_->setText(d.sceneLoading?QStringLiteral("Chargement de scène en cours")
            :(d.exploring?QStringLiteral("Exploration active"):QStringLiteral("Moteur prêt")));
        backendStatusValue_->setText(d.diagnosticStatus.isEmpty()
            ?QStringLiteral("Aucun échec de présentation signalé")
            :d.diagnosticStatus);
        detailValue_->setText(d.diagnosticDetail.isEmpty()
            ?QStringLiteral("Aucun détail supplémentaire.")
            :d.diagnosticDetail);
        revisionValue_->setText(QStringLiteral("Moteur %1 · révision %2\nScène %3 · révision %4")
            .arg(d.engineVersion,d.engineRevision.isEmpty()?QStringLiteral("n/d"):d.engineRevision,
                 d.sceneName.isEmpty()?QStringLiteral("non chargée"):d.sceneName,
                 d.sceneRevision.isEmpty()?QStringLiteral("non renseignée"):d.sceneRevision));
        presentationValue_->setText(d.presentation);
    }

protected:
    bool eventFilter(QObject* watched,QEvent* event) override{
        if(event->type()==QEvent::KeyPress){
            auto* key=static_cast<QKeyEvent*>(event);
            if(key->key()==Qt::Key_Tab){
                requestReturn();key->accept();return true;
            }
            if(key->key()==Qt::Key_Escape){
                requestExit();key->accept();return true;
            }
        }
        return QWidget::eventFilter(watched,event);
    }
    void keyPressEvent(QKeyEvent* event) override{
        if(event->key()==Qt::Key_Tab){
            requestReturn();event->accept();return;
        }
        if(event->key()==Qt::Key_Escape){
            requestExit();event->accept();return;
        }
        QWidget::keyPressEvent(event);
    }
    void closeEvent(QCloseEvent* event) override{
        event->ignore();
        requestReturn();
    }

private:
    QLabel* engineValue_=nullptr;
    QLabel* sceneValue_=nullptr;
    QLabel* backendValue_=nullptr;
    QLabel* fpsValue_=nullptr;
    QLabel* renderValue_=nullptr;
    QLabel* resolutionValue_=nullptr;
    QLabel* cameraValue_=nullptr;
    QLabel* objectValue_=nullptr;
    QLabel* triangleValue_=nullptr;
    QLabel* materialValue_=nullptr;
    QLabel* textureValue_=nullptr;
    QLabel* lightValue_=nullptr;
    QLabel* pointLightValue_=nullptr;
    QLabel* solarValue_=nullptr;
    QLabel* daylightValue_=nullptr;
    QLabel* lampValue_=nullptr;
    QLabel* ramValue_=nullptr;
    QLabel* textureResolutionValue_=nullptr;
    QLabel* processMemoryValue_=nullptr;
    QLabel* vramValue_=nullptr;
    QLabel* statusValue_=nullptr;
    QLabel* backendStatusValue_=nullptr;
    QLabel* detailValue_=nullptr;
    QLabel* revisionValue_=nullptr;
    QLabel* presentationValue_=nullptr;
    QTabWidget* pages_=nullptr;
    std::function<void()> returnHandler_;
    std::function<void()> exitHandler_;

    void requestReturn(){if(returnHandler_)returnHandler_();}
    void requestExit(){if(exitHandler_)exitHandler_();}

    QLabel* addCard(QGridLayout* grid,int column,const QString& caption){
        auto* card=new QFrame;
        card->setObjectName(QStringLiteral("card"));
        auto* layout=new QVBoxLayout(card);
        layout->setContentsMargins(16,12,16,12);
        auto* label=new QLabel(caption);
        label->setObjectName(QStringLiteral("section"));
        auto* value=new QLabel(QStringLiteral("—"));
        value->setObjectName(QStringLiteral("smallMetric"));
        value->setWordWrap(true);
        layout->addWidget(label);
        layout->addWidget(value,1);
        grid->addWidget(card,0,column);
        return value;
    }

    QWidget* makeOverviewPage(){
        auto* page=new QWidget;
        auto* grid=new QGridLayout(page);
        grid->setContentsMargins(12,16,12,12);grid->setSpacing(12);
        fpsValue_=addMetric(grid,0,0,QStringLiteral("CADENCE"));
        renderValue_=addMetric(grid,1,0,QStringLiteral("DERNIER TEMPS MOTEUR / SOUMISSION"));
        resolutionValue_=addMetric(grid,0,1,QStringLiteral("VIEWPORT ACTIF"));
        cameraValue_=addMetric(grid,1,1,QStringLiteral("CAMÉRA · X / Y / Z"));
        grid->setRowStretch(2,1);
        return page;
    }
    QWidget* makeScenePage(){
        auto* page=new QWidget;
        auto* grid=new QGridLayout(page);
        grid->setContentsMargins(12,16,12,12);grid->setSpacing(12);
        objectValue_=addMetric(grid,0,0,QStringLiteral("OBJETS SOURCE"));
        triangleValue_=addMetric(grid,1,0,QStringLiteral("TRIANGLES DE LA SCÈNE"));
        materialValue_=addMetric(grid,0,1,QStringLiteral("MATÉRIAUX"));
        textureValue_=addMetric(grid,1,1,QStringLiteral("MATÉRIAUX AVEC ALBEDO IMAGE"));
        lightValue_=addMetric(grid,0,2,QStringLiteral("LUMIÈRES PONCTUELLES"));
        grid->setRowStretch(3,1);
        return page;
    }
    QWidget* makeLightingPage(){
        auto* page=new QWidget;
        auto* grid=new QGridLayout(page);
        grid->setContentsMargins(12,16,12,12);grid->setSpacing(12);
        pointLightValue_=addMetric(grid,0,0,QStringLiteral("LUMIÈRES DE SCÈNE"));
        solarValue_=addMetric(grid,1,0,QStringLiteral("HEURE SOLAIRE"));
        daylightValue_=addMetric(grid,0,1,QStringLiteral("FACTEUR JOUR"));
        lampValue_=addMetric(grid,1,1,QStringLiteral("FACTEUR LAMPES"));
        grid->setRowStretch(2,1);
        return page;
    }
    QWidget* makeResourcesPage(){
        auto* page=new QWidget;
        auto* grid=new QGridLayout(page);
        grid->setContentsMargins(12,16,12,12);grid->setSpacing(12);
        ramValue_=addMetric(grid,0,0,QStringLiteral("RAM SYSTÈME UTILISÉE"));
        textureResolutionValue_=addMetric(grid,1,0,QStringLiteral("RÉSOLUTION TEXTURE MAX."));
        processMemoryValue_=addMetric(grid,0,1,QStringLiteral("MÉMOIRE DU PROCESSUS"));
        vramValue_=addMetric(grid,1,1,QStringLiteral("MÉMOIRE GPU"));
        grid->setRowStretch(2,1);
        return page;
    }
    QWidget* makeDiagnosticsPage(){
        auto* page=new QWidget;
        auto* grid=new QGridLayout(page);
        grid->setContentsMargins(12,16,12,12);grid->setSpacing(12);
        statusValue_=addMetric(grid,0,0,QStringLiteral("ÉTAT DU MOTEUR"));
        backendStatusValue_=addMetric(grid,1,0,QStringLiteral("PRÉSENTATION GPU"));
        presentationValue_=addMetric(grid,0,1,QStringLiteral("CHEMIN D’AFFICHAGE"));
        revisionValue_=addMetric(grid,1,1,QStringLiteral("IDENTITÉ DES RÉVISIONS"));
        detailValue_=addMetric(grid,0,2,QStringLiteral("DÉTAIL"));
        grid->setRowStretch(3,1);
        return page;
    }
    static QLabel* addMetric(QGridLayout* grid,int column,int row,const QString& caption){
        auto* card=new QFrame;
        card->setObjectName(QStringLiteral("card"));
        auto* layout=new QVBoxLayout(card);
        layout->setContentsMargins(18,15,18,15);
        auto* heading=new QLabel(caption);
        heading->setObjectName(QStringLiteral("section"));
        auto* value=new QLabel(QStringLiteral("—"));
        value->setObjectName(QStringLiteral("metric"));
        value->setWordWrap(true);
        layout->addWidget(heading);
        layout->addWidget(value,1);
        grid->addWidget(card,row,column);
        return value;
    }
};


class WristPanel final : public QWidget {
public:
    explicit WristPanel(QWidget* parent=nullptr)
        :QWidget(parent,Qt::Tool|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus){
        setWindowTitle(QStringLiteral("CSE WARP — Pip-Boy"));
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(370,190);
        setStyleSheet(QStringLiteral(
            "QWidget { background:transparent; color:#e4f0e2; font-family:'Segoe UI'; }"
            "QFrame#panel { background:#111b18; border:2px solid #71966c; border-radius:14px; }"
            "QLabel#brand { color:#a8d58f; font: bold 10pt 'Consolas'; letter-spacing:1px; }"
            "QLabel#name { color:#eff7e9; font: bold 14pt 'Segoe UI'; }"
            "QLabel#value { color:#d7edc6; font: bold 11pt 'Consolas'; }"
            "QLabel#muted { color:#9ab09b; font: 8pt 'Segoe UI'; }"));
        auto* outer=new QVBoxLayout(this);
        outer->setContentsMargins(0,0,0,0);
        auto* panel=new QFrame(this);
        panel->setObjectName(QStringLiteral("panel"));
        auto* layout=new QVBoxLayout(panel);
        layout->setContentsMargins(15,11,15,10);
        layout->setSpacing(6);
        auto* heading=new QHBoxLayout;
        auto* brand=new QLabel(QStringLiteral("CSE WARP  /  WRIST LINK"));
        brand->setObjectName(QStringLiteral("brand"));
        engine_=new QLabel(QStringLiteral("V17 —"));
        engine_->setObjectName(QStringLiteral("muted"));
        heading->addWidget(brand,1);
        heading->addWidget(engine_,0,Qt::AlignRight);
        layout->addLayout(heading);
        scene_=new QLabel(QStringLiteral("Aucune scène chargée"));
        scene_->setObjectName(QStringLiteral("name"));
        scene_->setWordWrap(true);
        layout->addWidget(scene_);
        auto* metrics=new QGridLayout;
        metrics->setHorizontalSpacing(16);
        metrics->setVerticalSpacing(3);
        fps_=addValue(metrics,0,0,QStringLiteral("FPS"));
        triangles_=addValue(metrics,1,0,QStringLiteral("TRIANGLES"));
        lights_=addValue(metrics,0,1,QStringLiteral("LUMIÈRES"));
        memory_=addValue(metrics,1,1,QStringLiteral("RAM SYSTÈME"));
        layout->addLayout(metrics);
        auto* footer=new QLabel(QStringLiteral("Appui bref : afficher/masquer  ·  Maintenir TAB : ouvrir"));
        footer->setObjectName(QStringLiteral("muted"));
        layout->addWidget(footer);
        outer->addWidget(panel);
    }

    void setData(const DashboardData& d){
        engine_->setText(QStringLiteral("%1 · %2")
            .arg(d.engineVersion,d.engineRevision.isEmpty()?QStringLiteral("n/d"):d.engineRevision));
        scene_->setText(QStringLiteral("%1  ·  scène %2")
            .arg(d.sceneName.isEmpty()?QStringLiteral("CSE WARP"):d.sceneName,
                 d.sceneRevision.isEmpty()?QStringLiteral("n/d"):d.sceneRevision));
        fps_->setText(d.fps>0.0?QStringLiteral("%1").arg(d.fps,0,'f',1):QStringLiteral("—"));
        triangles_->setText(QString::number(d.triangles));
        lights_->setText(QString::number(d.lights));
        memory_->setText(d.systemMemoryPercent>=0
            ?QStringLiteral("%1 %").arg(d.systemMemoryPercent):QStringLiteral("n/d"));
    }

private:
    QLabel* engine_=nullptr;
    QLabel* scene_=nullptr;
    QLabel* fps_=nullptr;
    QLabel* triangles_=nullptr;
    QLabel* lights_=nullptr;
    QLabel* memory_=nullptr;

    static QLabel* addValue(QGridLayout* grid,int column,int row,const QString& caption){
        auto* cell=new QWidget;
        auto* layout=new QHBoxLayout(cell);
        layout->setContentsMargins(0,0,0,0);
        layout->setSpacing(6);
        auto* label=new QLabel(caption);
        label->setObjectName(QStringLiteral("muted"));
        auto* value=new QLabel(QStringLiteral("—"));
        value->setObjectName(QStringLiteral("value"));
        layout->addWidget(label);
        layout->addWidget(value,1,Qt::AlignRight);
        grid->addWidget(cell,row,column);
        return value;
    }
};

} // namespace cse::diagnostics

