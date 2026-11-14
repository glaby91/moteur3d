#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

class BenchmarkWindow final : public QMainWindow {
public:
    explicit BenchmarkWindow(const QString& scenePath){
        setWindowTitle(QStringLiteral("CSE WARP Beta X — Benchmarks Windows"));
        resize(820,590);
        auto* content=new QWidget(this);auto* layout=new QVBoxLayout(content);
        auto* intro=new QLabel(QStringLiteral(
            "Benchmarks autonomes du moteur. Toutes les données sont automatiquement écrites "
            "en CSV, JSON et PNG dans un nouveau sous-dossier horodaté."));
        intro->setWordWrap(true);layout->addWidget(intro);
        auto* form=new QFormLayout;
        scene_=new QLineEdit(QFileInfo(scenePath).absoluteFilePath(),this);
        scene_->setReadOnly(true);form->addRow(QStringLiteral("Scène"),scene_);
        root_=new QLineEdit(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                            .absoluteFilePath(QStringLiteral("Moteur3D-V16-Benchmarks")),this);
        auto* rootRow=new QWidget;auto* row=new QHBoxLayout(rootRow);row->setContentsMargins(0,0,0,0);
        auto* browse=new QPushButton(QStringLiteral("Parcourir..."));
        row->addWidget(root_);row->addWidget(browse);
        form->addRow(QStringLiteral("Dossier de sortie"),rootRow);
        layout->addLayout(form);
        quick_=new QCheckBox(QStringLiteral("Test rapide (1 caméra, Full HD 1920 × 1080, CPU uniquement)"));
        cpuOnly_=new QCheckBox(QStringLiteral("CPU uniquement — ne pas tester l'aperçu GPU Compute"));
        layout->addWidget(quick_);layout->addWidget(cpuOnly_);
        layout->addWidget(new QLabel(QStringLiteral(
            "Complet : 6 caméras, Full HD / 1440p / 4K, 1/2/4/8/16 threads, "
            "test CPU 4 spp et essai GPU par caméra (couleurs sans PBR). GPU sans accélération : test ignoré, jamais simulé.")));
        auto* controls=new QHBoxLayout;
        start_=new QPushButton(QStringLiteral("Lancer le benchmark complet"));
        stop_=new QPushButton(QStringLiteral("Arrêter"));
        open_=new QPushButton(QStringLiteral("Ouvrir le dossier"));
        stop_->setEnabled(false);
        controls->addWidget(start_);controls->addWidget(stop_);controls->addWidget(open_);
        layout->addLayout(controls);
        log_=new QTextEdit;log_->setReadOnly(true);layout->addWidget(log_);
        status_=new QLabel(QStringLiteral("Prêt — le calcul est exécuté dans un processus séparé."));
        layout->addWidget(status_);
        setCentralWidget(content);
        connect(browse,&QPushButton::clicked,this,[this]{
            const QString selected=QFileDialog::getExistingDirectory(this,QStringLiteral("Dossier benchmarks"),root_->text());
            if(!selected.isEmpty())root_->setText(selected);
        });
        connect(quick_,&QCheckBox::toggled,this,[this](bool selected){
            if(selected)cpuOnly_->setChecked(true);
            cpuOnly_->setEnabled(!selected);
        });
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
    QCheckBox *quick_=nullptr,*cpuOnly_=nullptr;
    QPushButton *start_=nullptr,*stop_=nullptr,*open_=nullptr;
    QTextEdit *log_=nullptr;QLabel *status_=nullptr;
    QProcess process_;
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
                    +QStringLiteral("/moteur3d_v16_benchmark.exe");
        if(!QFileInfo::exists(exe)){
            QMessageBox::critical(this,QStringLiteral("Programme manquant"),exe);return;
        }
        QStringList args{scene_->text(),root_->text()};
        if(quick_->isChecked())args<<QStringLiteral("--quick");
        else if(cpuOnly_->isChecked())args<<QStringLiteral("--cpu-only");
        log_->clear();
        status_->setText(QStringLiteral("Benchmark en cours — export automatique actif…"));
        start_->setEnabled(false);stop_->setEnabled(true);
        process_.setWorkingDirectory(QCoreApplication::applicationDirPath());
        process_.start(exe,args);
    }
};
int main(int argc,char** argv){
    QApplication app(argc,argv);
    QString scene;
    if(argc>1)scene=QString::fromLocal8Bit(argv[1]);
    else scene=QCoreApplication::applicationDirPath()+QStringLiteral("/pavillon01/scene.json");
    BenchmarkWindow window(scene);
    window.show();
    return app.exec();
}
