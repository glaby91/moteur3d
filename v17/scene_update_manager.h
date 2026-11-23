#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QUuid>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUrl>
#include <QWidget>
#include <functional>

class V17SceneUpdateChecker final : public QObject {
public:
    explicit V17SceneUpdateChecker(QObject* parent=nullptr)
        : QObject(parent), network_(new QNetworkAccessManager(this)) {}

    // Compare the loaded civic scene with the manifest attached to the latest
    // moteur3d-scene release. The package remains untouched; accepted updates
    // are installed in the per-user data directory.
    void checkAtStartup(const QString& currentScene,const QString& currentRevision,
                        QWidget* window,std::function<void(const QString&)> integrate) {
        if(!currentScene.endsWith(QStringLiteral("/benchmark-civic/scene.json"),
                                  Qt::CaseInsensitive) &&
           !currentScene.endsWith(QStringLiteral("\\benchmark-civic\\scene.json"),
                                  Qt::CaseInsensitive)) return;
        window_=window;
        integrate_=std::move(integrate);
        currentRevision_=currentRevision.trimmed();
        setStatus(QStringLiteral("Vérification de la scène de référence…"));
        QNetworkRequest request(QUrl(QStringLiteral(
            "https://github.com/glaby91/moteur3d-scene/releases/latest/download/SCENE_VERSION.json")));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        auto* reply=network_->get(request);
        connect(reply,&QNetworkReply::finished,this,[this,reply]{
            const QByteArray bytes=reply->readAll();
            const auto networkError=reply->error();
            const QString error=reply->errorString();
            reply->deleteLater();
            if(networkError!=QNetworkReply::NoError){
                setStatus(QStringLiteral("Vérification de scène indisponible (scène locale conservée)."));
                qInfo().noquote()<<"Scene update check unavailable:"<<error;
                return;
            }
            QJsonParseError parsing;
            const auto document=QJsonDocument::fromJson(bytes,&parsing);
            const auto manifest=document.object();
            const QString latest=manifest.value(QStringLiteral("revision")).toString().trimmed();
            if(parsing.error!=QJsonParseError::NoError ||
               manifest.value(QStringLiteral("repository")).toString()!=
                   QStringLiteral("glaby91/moteur3d-scene") ||
               manifest.value(QStringLiteral("scene")).toString()!=
                   QStringLiteral("benchmark-civic/scene.json") ||
               revisionNumber(latest)<0 || revisionNumber(currentRevision_)<0){
                setStatus(QStringLiteral("Manifeste de scène invalide ; scène locale conservée."));
                qWarning()<<"Invalid latest scene manifest";
                return;
            }
            const int latestNumber=revisionNumber(latest);
            const int currentNumber=revisionNumber(currentRevision_);
            if(latestNumber<=currentNumber){
                setStatus(QStringLiteral("Scène à jour (%1).").arg(currentRevision_),8000);
                return;
            }
            QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
            if(settings.value(QStringLiteral("Scene/DeferredRevision")).toString()==latest){
                setStatus(QStringLiteral("Une mise à jour de scène (%1) est disponible.").arg(latest),10000);
                return;
            }
            QMessageBox prompt(QMessageBox::Information,
                QStringLiteral("Mise à jour de la scène CSE WARP"),
                QStringLiteral("La scène chargée est %1. La dernière scène publiée est %2.\n\n"
                               "Voulez-vous la télécharger et l’intégrer maintenant ?")
                    .arg(currentRevision_,latest),
                QMessageBox::NoButton,window_);
            auto* accept=prompt.addButton(QStringLiteral("Télécharger et intégrer"),
                                          QMessageBox::AcceptRole);
            auto* later=prompt.addButton(QStringLiteral("Plus tard"),QMessageBox::RejectRole);
            prompt.setDefaultButton(qobject_cast<QPushButton*>(accept));
            prompt.exec();
            if(prompt.clickedButton()!=accept){
                if(prompt.clickedButton()==later)
                    settings.setValue(QStringLiteral("Scene/DeferredRevision"),latest);
                setStatus(QStringLiteral("Scène %1 conservée.").arg(currentRevision_),8000);
                return;
            }
            settings.remove(QStringLiteral("Scene/DeferredRevision"));
            beginDownload(latest,window_);
        });
    }

private:
    static int revisionNumber(const QString& revision) {
        const auto match=QRegularExpression(QStringLiteral("^V17\\.(\\d+)$"))
            .match(revision);
        return match.hasMatch()?match.captured(1).toInt():-1;
    }

    void setStatus(const QString& text,int timeout=0) {
        if(auto* status=window_?window_->findChild<QStatusBar*>():nullptr)
            status->showMessage(text,timeout);
    }

    QString dataRoot() const {
        QString root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if(root.isEmpty())root=QDir::home().filePath(QStringLiteral(".cse-warp"));
        return root;
    }

    void beginDownload(const QString& revision,QWidget* window) {
        window_=window;
        expectedRevision_=revision;
        const QString folder=QDir(dataRoot()).filePath(QStringLiteral("scene-updates"));
        if(!QDir().mkpath(folder)){
            fail(QStringLiteral("Impossible de créer le dossier utilisateur pour la scène."));
            return;
        }
        archivePath_=QDir(folder).filePath(revision+QStringLiteral(".zip"));
        archiveFile_=new QSaveFile(archivePath_,this);
        if(!archiveFile_->open(QIODevice::WriteOnly)){
            fail(QStringLiteral("Impossible d’écrire l’archive de scène téléchargée."));
            archiveFile_->deleteLater();archiveFile_=nullptr;
            return;
        }
        setStatus(QStringLiteral("Téléchargement de la scène %1…").arg(revision));
        QNetworkRequest request(QUrl(QStringLiteral(
            "https://github.com/glaby91/moteur3d-scene/releases/latest/download/V17-scene.zip")));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        auto* reply=network_->get(request);
        connect(reply,&QNetworkReply::readyRead,this,[this,reply]{
            if(archiveFile_ && archiveFile_->isOpen()){
                const QByteArray chunk=reply->readAll();
                if(archiveFile_->write(chunk)!=chunk.size()){
                    reply->abort();
                    fail(QStringLiteral("Échec d’écriture de l’archive de scène."));
                }
            }
        });
        connect(reply,&QNetworkReply::finished,this,[this,reply]{
            if(archiveFile_ && archiveFile_->isOpen()){
                const QByteArray tail=reply->readAll();
                if(!tail.isEmpty() && archiveFile_->write(tail)!=tail.size()){
                    archiveFile_->cancelWriting();
                }
            }
            const bool networkOk=reply->error()==QNetworkReply::NoError;
            const QString networkMessage=reply->errorString();
            reply->deleteLater();
            const bool fileOk=archiveFile_ && archiveFile_->commit();
            if(archiveFile_){archiveFile_->deleteLater();archiveFile_=nullptr;}
            if(!networkOk||!fileOk){
                QFile::remove(archivePath_);
                fail(QStringLiteral("Téléchargement de la scène impossible : %1")
                    .arg(networkOk?QStringLiteral("écriture interrompue"):networkMessage));
                return;
            }
            extractAndValidate();
        });
    }

    void extractAndValidate() {
        stagingPath_=QDir(dataRoot()).filePath(
            QStringLiteral("scene-updates/extract-")+QUuid::createUuid().toString(QUuid::WithoutBraces));
        if(!QDir().mkpath(stagingPath_)){
            fail(QStringLiteral("Impossible de préparer l’extraction de la scène."));
            return;
        }
        auto* process=new QProcess(this);
#if defined(_WIN32)
        process->setProgram(QStringLiteral("powershell.exe"));
        process->setArguments({QStringLiteral("-NoProfile"),QStringLiteral("-NonInteractive"),
             QStringLiteral("-ExecutionPolicy"),QStringLiteral("Bypass"),
             QStringLiteral("-Command"),
             QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                .arg(archivePath_).arg(stagingPath_)});
#else
        process->setProgram(QStringLiteral("unzip"));
        process->setArguments({QStringLiteral("-q"),QStringLiteral("-o"),archivePath_,
             QStringLiteral("-d"),stagingPath_});
#endif
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,
            [this,process](int code,QProcess::ExitStatus status){
                const QString details=QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if(code!=0||status!=QProcess::NormalExit){
                    fail(QStringLiteral("Extraction de scène échouée : %1").arg(details));
                    return;
                }
                validateAndInstall();
            });
        connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError){
            if(process->state()==QProcess::NotRunning){
                const QString details=process->errorString();
                process->deleteLater();
                fail(QStringLiteral("Outil d’extraction indisponible : %1").arg(details));
            }
        });
        process->start();
        setStatus(QStringLiteral("Extraction et validation de la scène %1…").arg(expectedRevision_));
    }

    static bool copyDirectory(const QString& source,const QString& destination) {
        QDir src(source);
        if(!src.exists()||!QDir().mkpath(destination))return false;
        for(const auto& name:src.entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
            if(!copyDirectory(src.filePath(name),QDir(destination).filePath(name)))return false;
        }
        for(const auto& name:src.entryList(QDir::Files)){
            const QString target=QDir(destination).filePath(name);
            QFile::remove(target);
            if(!QFile::copy(src.filePath(name),target))return false;
        }
        return true;
    }

    void validateAndInstall() {
        const QString extractedManifest=QDir(stagingPath_).filePath(QStringLiteral("SCENE_VERSION.json"));
        QFile manifestFile(extractedManifest);
        if(!manifestFile.open(QIODevice::ReadOnly)){
            fail(QStringLiteral("L’archive ne contient pas SCENE_VERSION.json."));
            return;
        }
        QJsonParseError parsing;
        const auto manifest=QJsonDocument::fromJson(manifestFile.readAll(),&parsing).object();
        const QString sceneRelative=manifest.value(QStringLiteral("scene")).toString();
        if(parsing.error!=QJsonParseError::NoError ||
           manifest.value(QStringLiteral("repository")).toString()!=QStringLiteral("glaby91/moteur3d-scene") ||
           manifest.value(QStringLiteral("revision")).toString()!=expectedRevision_ ||
           sceneRelative!=QStringLiteral("benchmark-civic/scene.json")){
            fail(QStringLiteral("Le manifeste de l’archive ne correspond pas à la révision annoncée."));
            return;
        }
        const QString extractedScene=QDir(stagingPath_).filePath(sceneRelative);
        QFile sceneFile(extractedScene);
        if(!sceneFile.open(QIODevice::ReadOnly)){
            fail(QStringLiteral("L’archive ne contient pas benchmark-civic/scene.json."));
            return;
        }
        const auto scene=QJsonDocument::fromJson(sceneFile.readAll(),&parsing).object();
        const QString sceneRevision=scene.value(QStringLiteral("revision")).toString().trimmed();
        if(parsing.error!=QJsonParseError::NoError || sceneRevision!=expectedRevision_){
            fail(QStringLiteral("La révision de scene.json ne correspond pas au manifeste téléchargé."));
            return;
        }
        const QString sourceDirectory=QFileInfo(extractedScene).absolutePath();
        const QString destination=QDir(dataRoot()).filePath(
            QStringLiteral("scenes/%1/benchmark-civic").arg(expectedRevision_));
        QDir(destination).removeRecursively();
        if(!copyDirectory(sourceDirectory,destination)){
            fail(QStringLiteral("Impossible d’installer la scène %1 dans le dossier utilisateur.")
                 .arg(expectedRevision_));
            return;
        }
        const QString installedScene=QDir(destination).filePath(QStringLiteral("scene.json"));
        if(!QFileInfo::exists(installedScene)){
            fail(QStringLiteral("La scène installée est incomplète."));
            return;
        }
        QFile::remove(archivePath_);
        QDir(stagingPath_).removeRecursively();
        setStatus(QStringLiteral("Scène %1 téléchargée et validée ; intégration en cours…")
                  .arg(expectedRevision_),15000);
        if(integrate_)integrate_(installedScene);
    }

    void fail(const QString& message) {
        setStatus(message,20000);
        qWarning().noquote()<<message;
        if(window_)QMessageBox::warning(window_,QStringLiteral("Mise à jour de la scène"),message);
    }

    QNetworkAccessManager* network_=nullptr;
    QSaveFile* archiveFile_=nullptr;
    QWidget* window_=nullptr;
    std::function<void(const QString&)> integrate_;
    QString currentRevision_;
    QString expectedRevision_;
    QString archivePath_;
    QString stagingPath_;
};
