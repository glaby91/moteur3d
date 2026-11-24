#include <QApplication>
#include <QByteArray>
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
#include <QStringList>
#include <QTextCursor>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

class BenchmarkWindow final : public QMainWindow {
public:
    explicit BenchmarkWindow(const QString& scenePath) {
        setWindowTitle(QStringLiteral("CSE WARP Beta X — Benchmarks Windows"));
        resize(820, 590);

        buildInterface(scenePath);
        connectActions();
    }

private:
    QLineEdit* scene_ = nullptr;
    QLineEdit* root_ = nullptr;
    QCheckBox* quick_ = nullptr;
    QCheckBox* cpuOnly_ = nullptr;
    QPushButton* browse_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* stop_ = nullptr;
    QPushButton* open_ = nullptr;
    QTextEdit* log_ = nullptr;
    QLabel* status_ = nullptr;
    QProcess process_;

    void buildInterface(const QString& scenePath) {
        auto* content = new QWidget(this);
        auto* layout = new QVBoxLayout(content);

        auto* intro = new QLabel(QStringLiteral(
            "Benchmarks autonomes du moteur. Toutes les données sont automatiquement écrites "
            "en CSV, JSON et PNG dans un nouveau sous-dossier horodaté."));
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto* form = new QFormLayout;
        scene_ = new QLineEdit(QFileInfo(scenePath).absoluteFilePath(), this);
        scene_->setReadOnly(true);
        form->addRow(QStringLiteral("Scène"), scene_);

        const QString defaultOutputDirectory =
            QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                .absoluteFilePath(QStringLiteral("Moteur3D-V16-Benchmarks"));
        root_ = new QLineEdit(defaultOutputDirectory, this);

        auto* rootRow = new QWidget;
        auto* row = new QHBoxLayout(rootRow);
        row->setContentsMargins(0, 0, 0, 0);

        browse_ = new QPushButton(QStringLiteral("Parcourir..."));
        row->addWidget(root_);
        row->addWidget(browse_);
        form->addRow(QStringLiteral("Dossier de sortie"), rootRow);
        layout->addLayout(form);

        quick_ = new QCheckBox(
            QStringLiteral("Test rapide (1 caméra, Full HD 1920 × 1080, CPU uniquement)"));
        cpuOnly_ = new QCheckBox(
            QStringLiteral("CPU uniquement — ne pas tester l'aperçu GPU Compute"));
        layout->addWidget(quick_);
        layout->addWidget(cpuOnly_);

        layout->addWidget(new QLabel(QStringLiteral(
            "Complet : 6 caméras, Full HD / 1440p / 4K, 1/2/4/8/16 threads, "
            "test CPU 4 spp et essai GPU par caméra (couleurs sans PBR). "
            "GPU sans accélération : test ignoré, jamais simulé.")));

        auto* controls = new QHBoxLayout;
        start_ = new QPushButton(QStringLiteral("Lancer le benchmark complet"));
        stop_ = new QPushButton(QStringLiteral("Arrêter"));
        open_ = new QPushButton(QStringLiteral("Ouvrir le dossier"));
        stop_->setEnabled(false);
        controls->addWidget(start_);
        controls->addWidget(stop_);
        controls->addWidget(open_);
        layout->addLayout(controls);

        log_ = new QTextEdit;
        log_->setReadOnly(true);
        layout->addWidget(log_);

        status_ = new QLabel(
            QStringLiteral("Prêt — le calcul est exécuté dans un processus séparé."));
        layout->addWidget(status_);

        setCentralWidget(content);
    }

    void connectActions() {
        connect(browse_, &QPushButton::clicked, this, &BenchmarkWindow::browseForOutput);
        connect(quick_, &QCheckBox::toggled, this, &BenchmarkWindow::updateQuickMode);
        connect(start_, &QPushButton::clicked, this, &BenchmarkWindow::runBenchmark);
        connect(stop_, &QPushButton::clicked, this, &BenchmarkWindow::stopBenchmark);
        connect(open_, &QPushButton::clicked, this, &BenchmarkWindow::openOutputDirectory);

        connect(&process_, &QProcess::readyReadStandardOutput,
                this, &BenchmarkWindow::appendStandardOutput);
        connect(&process_, &QProcess::readyReadStandardError,
                this, &BenchmarkWindow::appendStandardError);
        connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, &BenchmarkWindow::handleBenchmarkFinished);
        connect(&process_, &QProcess::errorOccurred,
                this, &BenchmarkWindow::handleProcessError);
    }

    void browseForOutput() {
        const QString selectedDirectory = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Dossier benchmarks"), root_->text());
        if (!selectedDirectory.isEmpty()) {
            root_->setText(selectedDirectory);
        }
    }

    void updateQuickMode(bool enabled) {
        if (enabled) {
            cpuOnly_->setChecked(true);
        }
        cpuOnly_->setEnabled(!enabled);
    }

    void stopBenchmark() {
        if (process_.state() == QProcess::NotRunning) {
            return;
        }

        log_->append(QStringLiteral(
            "Demande d'arrêt… Les mesures déjà écrites restent dans le dossier."));
        process_.kill();
    }

    void openOutputDirectory() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(root_->text()));
    }

    void appendStandardOutput() {
        appendOutput(process_.readAllStandardOutput());
    }

    void appendStandardError() {
        appendOutput(process_.readAllStandardError());
    }

    void handleBenchmarkFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        start_->setEnabled(true);
        stop_->setEnabled(false);

        const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
        status_->setText(succeeded
            ? QStringLiteral("Terminé — CSV, JSON et images dans le dossier horodaté.")
            : QStringLiteral("Arrêt ou échec (code %1). Consulter le journal.")
                  .arg(exitCode));

        appendStandardOutput();
        appendStandardError();
    }

    void handleProcessError(QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }

        status_->setText(QStringLiteral(
            "Impossible de démarrer le programme de benchmark."));
        start_->setEnabled(true);
        stop_->setEnabled(false);
    }

    void appendOutput(const QByteArray& data) {
        if (data.isEmpty()) {
            return;
        }

        log_->moveCursor(QTextCursor::End);
        log_->insertPlainText(QString::fromUtf8(data));

        QScrollBar* scrollBar = log_->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }

    void runBenchmark() {
        if (process_.state() != QProcess::NotRunning) {
            return;
        }

        const QString scenePath = scene_->text();
        if (!QFileInfo::exists(scenePath)) {
            QMessageBox::critical(this, QStringLiteral("Scène absente"), scenePath);
            return;
        }

        const QString outputDirectory = root_->text();
        if (!QDir().mkpath(outputDirectory)) {
            QMessageBox::critical(
                this, QStringLiteral("Dossier inaccessible"), outputDirectory);
            return;
        }

        const QString executablePath = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("moteur3d_v16_benchmark.exe"));
        if (!QFileInfo::exists(executablePath)) {
            QMessageBox::critical(
                this, QStringLiteral("Programme manquant"), executablePath);
            return;
        }

        QStringList arguments{scenePath, outputDirectory};
        if (quick_->isChecked()) {
            arguments << QStringLiteral("--quick");
        } else if (cpuOnly_->isChecked()) {
            arguments << QStringLiteral("--cpu-only");
        }

        log_->clear();
        status_->setText(QStringLiteral(
            "Benchmark en cours — export automatique actif…"));
        start_->setEnabled(false);
        stop_->setEnabled(true);

        process_.setWorkingDirectory(QCoreApplication::applicationDirPath());
        process_.start(executablePath, arguments);
    }
};

int main(int argc, char** argv) {
    QApplication app(argc,argv);
    QString scenePath;
    if (argc > 1) {
        scenePath = QString::fromLocal8Bit(argv[1]);
    } else {
        scenePath = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("pavillon01/scene.json"));
    }

    BenchmarkWindow window(scenePath);
    window.show();
    return app.exec();
}
