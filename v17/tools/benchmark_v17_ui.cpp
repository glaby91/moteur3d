#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QIcon>
#include <QMenuBar>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QStringList>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

static QString benchmarkBuildNumber(){
    QFile file(QCoreApplication::applicationDirPath()+QStringLiteral("/BUILD_NUMBER.txt"));
    if(file.open(QIODevice::ReadOnly|QIODevice::Text)){
        const auto build=QString::fromUtf8(file.readAll()).trimmed();
        if(!build.isEmpty())return build;
    }
    return QStringLiteral("0 (local)");
}
class BenchmarkWindow final : public QMainWindow {
public:
    explicit BenchmarkWindow(const QString& scenePath,QStringList rasterArgs={})
        :qualityArgs_(std::move(rasterArgs)){
        setWindowTitle(QStringLiteral("CSE WARP Beta X V17.%1 — Benchmark DirectX 11")
            .arg(CSE_WARP_BUILD_NUMBER));
        resize(820,590);
        auto* content=new QWidget(this);auto* layout=new QVBoxLayout(content);
        auto* intro=new QLabel(QStringLiteral(
            "Benchmark V17 : DirectX 11 Raster (temps rendu + lecture GPU). Export automatique "
            "CSV, JSON et PNG dans un sous-dossier horodaté. FPS estimés hors affichage Qt."));
        intro->setWordWrap(true);layout->addWidget(intro);
        if(!qualityArgs_.isEmpty()){
            auto* qualityInfo=new QLabel(QStringLiteral(
                "Configuration Raster héritée de V17 : %1").arg(qualityArgs_.join(' ')),this);
            qualityInfo->setWordWrap(true);
            layout->addWidget(qualityInfo);
        }
        auto* form=new QFormLayout;
        scene_=new QLineEdit(QFileInfo(scenePath).absoluteFilePath(),this);
        scene_->setReadOnly(true);form->addRow(QStringLiteral("Scène"),scene_);
        root_=new QLineEdit(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                            .absoluteFilePath(QStringLiteral("CSE-WARP-V17-Benchmarks")),this);
        auto* rootRow=new QWidget;auto* row=new QHBoxLayout(rootRow);row->setContentsMargins(0,0,0,0);
        auto* browse=new QPushButton(QStringLiteral("Parcourir..."));
        row->addWidget(root_);row->addWidget(browse);
        form->addRow(QStringLiteral("Dossier de sortie"),rootRow);
        layout->addLayout(form);
        quick_=new QCheckBox(QStringLiteral("Rapide : caméra 01, 1366 × 768, raster GPU (6 images mesurées)"));
        compare_=new QCheckBox(QStringLiteral("Comparer avec V16 : Compute HLSL et ray tracing CPU"));
        layout->addWidget(quick_);layout->addWidget(compare_);
        auto* gpuOptions=new QFormLayout;
        readback_=new QComboBox(this);
        readback_->addItem(QStringLiteral("1 buffer — lecture synchrone (référence)"),1);
        readback_->addItem(QStringLiteral("2 buffers — copie GPU en pipeline"),2);
        readback_->addItem(QStringLiteral("3 buffers — copie GPU en pipeline"),3);
        gpuOptions->addRow(QStringLiteral("Tampons GPU → CPU"),readback_);
        layout->addLayout(gpuOptions);
        compareReadback_=new QCheckBox(QStringLiteral(
            "Comparer automatiquement 1, 2 et 3 buffers (même scène, mêmes pixels)"),this);
        layout->addWidget(compareReadback_);
        connect(compareReadback_,&QCheckBox::toggled,readback_,[this](bool checked){
            readback_->setEnabled(!checked);
        });
        layout->addWidget(new QLabel(QStringLiteral(
            "Complet : deux caméras, 1366 × 768 et Full HD, 12 images par cas ; "
            "médiane, P95, P99 et image PNG. Les performances WARP ne sont pas celles de la HD 620.")));
        auto* controls=new QHBoxLayout;
        start_=new QPushButton(QStringLiteral("Lancer le benchmark V17.%1")
            .arg(CSE_WARP_BUILD_NUMBER));
        stop_=new QPushButton(QStringLiteral("Arrêter"));
        open_=new QPushButton(QStringLiteral("Ouvrir le dossier"));
        stop_->setEnabled(false);
        controls->addWidget(start_);controls->addWidget(stop_);controls->addWidget(open_);
        layout->addLayout(controls);
        log_=new QTextEdit;log_->setReadOnly(true);layout->addWidget(log_);
        status_=new QLabel(QStringLiteral("Prêt — le calcul est exécuté dans un processus séparé."));
        layout->addWidget(status_);
        setCentralWidget(content);
        auto* help=menuBar()->addMenu(QStringLiteral("Aide"));
        auto* about=help->addAction(QStringLiteral("À propos du Benchmark V17.%1…")
            .arg(CSE_WARP_BUILD_NUMBER));
        connect(about,&QAction::triggered,this,[this]{
            QString revision=QStringLiteral("développement");
            QFile file(QCoreApplication::applicationDirPath()
                +QStringLiteral("/REVISION.txt"));
            if(file.open(QIODevice::ReadOnly|QIODevice::Text))
                revision=QString::fromUtf8(file.readAll()).trimmed().left(12);
            QMessageBox::about(this,
                QStringLiteral("Benchmark CSE WARP V17.%1").arg(CSE_WARP_BUILD_NUMBER),
                QStringLiteral("<h3>CSE WARP V17.%1 — Benchmark DirectX 11</h3>"
                    "<p>Version 17.%1 — <b>Build GitHub : %1</b><br/>"
                    "Révision GitHub : %2</p>"
                    "<p>Mesure Raster D3D11 : premier rendu, P50/P95/P99, "
                    "FPS théoriques et triangles soumis au GPU.</p>"
                    "<p>Chronométrage GPU DirectX 11 par timestamp/disjoint, "
                    "attente CPU Map, conversion des pixels, "
                    "comparaison de lecture à 1, 2 et 3 buffers.</p>"
                    "<p>Résultats CSV/JSON/PNG, comparaison V16 optionnelle.</p>")
                    .arg(QString::number(CSE_WARP_BUILD_NUMBER),revision.toHtmlEscaped()));
        });
        connect(browse,&QPushButton::clicked,this,[this]{
            const QString selected=QFileDialog::getExistingDirectory(this,QStringLiteral("Dossier benchmarks"),root_->text());
            if(!selected.isEmpty())root_->setText(selected);
        });
        quick_->setChecked(true);
        // Comparison is optional even in quick mode: raster remains the main test.
        connect(start_,&QPushButton::clicked,this,[this]{runBenchmark();});
        connect(stop_,&QPushButton::clicked,this,[this]{
            if(process_.state()!=QProcess::NotRunning){
                log_->append(QStringLiteral("Demande d'arrêt… Les mesures déjà écrites restent dans le dossier."));
                process_.kill();
            }
        });
        connect(open_,&QPushButton::clicked,this,[this]{
            QDesktopServices::openUrl(QUrl::fromLocalFile(root_->text()));
        });
        connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{appendOutput(process_.readAllStandardOutput());});
        connect(&process_,&QProcess::readyReadStandardError,this,[this]{appendOutput(process_.readAllStandardError());});
        connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,
                [this](int code,QProcess::ExitStatus state){
            start_->setEnabled(true);stop_->setEnabled(false);
            status_->setText(state==QProcess::NormalExit&&code==0
                ?QStringLiteral("Terminé — CSV, JSON et images dans le dossier horodaté.")
                :QStringLiteral("Arrêt ou échec (code %1). Consulter le journal.").arg(code));
            appendOutput(process_.readAllStandardOutput());
            appendOutput(process_.readAllStandardError());
        });
        connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
            if(error==QProcess::FailedToStart){
                status_->setText(QStringLiteral("Impossible de démarrer le programme de benchmark."));
                start_->setEnabled(true);stop_->setEnabled(false);
            }
        });
    }
private:
    QLineEdit *scene_=nullptr,*root_=nullptr;
    QCheckBox *quick_=nullptr,*compare_=nullptr,*compareReadback_=nullptr;
    QComboBox *readback_=nullptr;
    QPushButton *start_=nullptr,*stop_=nullptr,*open_=nullptr;
    QTextEdit *log_=nullptr;QLabel *status_=nullptr;
    QProcess process_;
    QStringList qualityArgs_;
    void appendOutput(const QByteArray& data){
        if(data.isEmpty())return;
        log_->moveCursor(QTextCursor::End);
        log_->insertPlainText(QString::fromUtf8(data));
        log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
    }
    void runBenchmark(){
        if(process_.state()!=QProcess::NotRunning)return;
        if(!QFileInfo::exists(scene_->text())){
            QMessageBox::critical(this,QStringLiteral("Scène absente"),scene_->text());return;
        }
        if(!QDir().mkpath(root_->text())){
            QMessageBox::critical(this,QStringLiteral("Dossier inaccessible"),root_->text());return;
        }
        const QString exe=QCoreApplication::applicationDirPath()
                    +QStringLiteral("/moteur3d_v17_benchmark.exe");
        if(!QFileInfo::exists(exe)){
            QMessageBox::critical(this,QStringLiteral("Programme manquant"),exe);return;
        }
        QStringList args{scene_->text(),root_->text()};
        if(quick_->isChecked())args<<QStringLiteral("--quick");
        if(compare_->isChecked())args<<QStringLiteral("--compare-v16");
        args.append(qualityArgs_);
        if(compareReadback_->isChecked())args<<QStringLiteral("--compare-readback");
        else args<<QStringLiteral("--readback-depth")
                 <<QString::number(readback_->currentData().toInt());
        log_->clear();
        status_->setText(QStringLiteral("Benchmark en cours — export automatique actif…"));
        start_->setEnabled(false);stop_->setEnabled(true);
        process_.setWorkingDirectory(QCoreApplication::applicationDirPath());
        process_.start(exe,args);
    }
};
int main(int argc,char** argv){
    QApplication app(argc,argv);
    QCoreApplication::setApplicationName(QStringLiteral("CSE WARP V17.%1 Benchmark")
        .arg(CSE_WARP_BUILD_NUMBER));
    QCoreApplication::setApplicationVersion(QStringLiteral("17.%1")
        .arg(CSE_WARP_BUILD_NUMBER));
    const QString icon=QCoreApplication::applicationDirPath()
        +QStringLiteral("/cse_warp_v17_bench.ico");
    if(QFileInfo::exists(icon))app.setWindowIcon(QIcon(icon));
    QString scene;
    if(argc>1)scene=QString::fromLocal8Bit(argv[1]);
    else scene=QCoreApplication::applicationDirPath()+QStringLiteral("/pavillon01/scene.json");
    BenchmarkWindow window(scene,argc>1?app.arguments().mid(2):QStringList{});
    window.show();
    return app.exec();
}
