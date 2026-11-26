#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
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
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <functional>
#include <utility>

class V17SceneUpdateChecker final : public QObject {
public:
    explicit V17SceneUpdateChecker(QObject* parent=nullptr)
        : QObject(parent), network_(new QNetworkAccessManager(this)) {}

    // Compare the loaded civic scene with the manifest attached to the latest
    // moteur3d-scene release. The package remains untouched; accepted updates
    // are installed in the per-user data directory.
    void checkAtStartup(const QString& currentScene,const QString& currentRevision,
                        QWidget* window,std::function<void(const QString&)> integrate,
                        bool allowMissingScene=false) {
        if(checking_||downloading_)return;
        const bool isCivicScene=currentScene.endsWith(QStringLiteral("/benchmark-civic/scene.json"),
                                  Qt::CaseInsensitive) ||
           currentScene.endsWith(QStringLiteral("\\benchmark-civic\\scene.json"),
                                  Qt::CaseInsensitive);
        if(!isCivicScene&&!allowMissingScene)return;
        window_=window;
        integrate_=std::move(integrate);
        currentRevision_=isCivicScene?currentRevision.trimmed():QString{};
        checking_=true;
        setStatus(QStringLiteral("Vérification de la scène de référence…"));
        QNetworkRequest request(QUrl(QStringLiteral(
            "https://api.github.com/repos/glaby91/moteur3d-scene/releases/latest")));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setRawHeader("Accept","application/vnd.github+json");
        auto* releaseReply=network_->get(request);
        connect(releaseReply,&QNetworkReply::finished,this,[this,releaseReply]{
            const QByteArray releaseBytes=releaseReply->readAll();
            const auto networkError=releaseReply->error();
            const QString error=releaseReply->errorString();
            releaseReply->deleteLater();
            if(networkError!=QNetworkReply::NoError){
                checking_=false;
                setStatus(currentRevision_.isEmpty()
                    ? QStringLiteral("Vérification GitHub indisponible ; aucune scène civique locale.")
                    : QStringLiteral("Vérification de scène indisponible ; la scène locale est conservée."));
                qInfo().noquote()<<"Scene update check unavailable:"<<error;
                return;
            }
            QJsonParseError parsing;
            const auto releaseDocument=QJsonDocument::fromJson(releaseBytes,&parsing);
            const auto release=releaseDocument.object();
            const QString tag=release.value(QStringLiteral("tag_name")).toString().trimmed();
            QString manifestUrl,archiveUrl;
            const auto assets=release.value(QStringLiteral("assets")).toArray();
            for(const auto& value:assets){
                const auto asset=value.toObject();
                const QString name=asset.value(QStringLiteral("name")).toString();
                const QString url=asset.value(QStringLiteral("browser_download_url")).toString();
                if(name==QStringLiteral("SCENE_VERSION.json"))manifestUrl=url;
                else if(name==QStringLiteral("V17-scene.zip"))archiveUrl=url;
            }
            const QString releasePrefix=QStringLiteral(
                "https://github.com/glaby91/moteur3d-scene/releases/download/");
            if(parsing.error!=QJsonParseError::NoError ||
               !QRegularExpression(QStringLiteral("^scene-V17\\.\\d+-\\d+$")).match(tag).hasMatch() ||
               !manifestUrl.startsWith(releasePrefix) ||
               !manifestUrl.endsWith(QStringLiteral("/SCENE_VERSION.json")) ||
               !archiveUrl.startsWith(releasePrefix) ||
               !archiveUrl.endsWith(QStringLiteral("/V17-scene.zip"))){
                checking_=false;
                setStatus(currentRevision_.isEmpty()
                    ? QStringLiteral("Publication de scène invalide ; aucune scène civique locale.")
                    : QStringLiteral("Publication de scène invalide ; la scène locale est conservée."));
                qWarning()<<"Invalid latest scene release metadata";
                return;
            }
            QNetworkRequest manifestRequest{QUrl(manifestUrl)};
            manifestRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                         QNetworkRequest::NoLessSafeRedirectPolicy);
            auto* manifestReply=network_->get(manifestRequest);
            connect(manifestReply,&QNetworkReply::finished,this,
                    [this,manifestReply,archiveUrl,tag]{
                const QByteArray bytes=manifestReply->readAll();
                const auto manifestError=manifestReply->error();
                const QString manifestErrorText=manifestReply->errorString();
                manifestReply->deleteLater();
                checking_=false;
                if(manifestError!=QNetworkReply::NoError){
                    setStatus(currentRevision_.isEmpty()
                        ? QStringLiteral("Manifeste GitHub indisponible ; aucune scène civique locale.")
                        : QStringLiteral("Manifeste de scène indisponible ; la scène locale est conservée."));
                    qInfo().noquote()<<"Scene manifest fetch failed:"<<manifestErrorText;
                    return;
                }
                QJsonParseError manifestParsing;
                const auto document=QJsonDocument::fromJson(bytes,&manifestParsing);
                const auto manifest=document.object();
                const QString latest=manifest.value(QStringLiteral("revision")).toString().trimmed();
                if(manifestParsing.error!=QJsonParseError::NoError ||
                   manifest.value(QStringLiteral("repository")).toString()!=
                       QStringLiteral("glaby91/moteur3d-scene") ||
                   manifest.value(QStringLiteral("scene")).toString()!=
                       QStringLiteral("benchmark-civic/scene.json") ||
                   !tag.startsWith(QStringLiteral("scene-%1-").arg(latest)) ||
                   revisionNumber(latest)<0 ||
                   (!currentRevision_.isEmpty() && revisionNumber(currentRevision_)<0)){
                    setStatus(currentRevision_.isEmpty()
                        ? QStringLiteral("Manifeste GitHub invalide ; aucune scène civique locale.")
                        : QStringLiteral("Manifeste GitHub invalide ; la scène locale est conservée."));
                    qWarning()<<"Invalid latest scene manifest";
                    return;
                }
                const int latestNumber=revisionNumber(latest);
                const int currentNumber=revisionNumber(currentRevision_);
                if(!currentRevision_.isEmpty() && latestNumber<=currentNumber){
                    setStatus(QStringLiteral("Scène à jour (%1).").arg(currentRevision_),8000);
                    return;
                }
                QSettings settings(QStringLiteral("CSE WARP"),QStringLiteral("CSE WARP V17"));
                if(settings.value(QStringLiteral("Scene/DeferredRevision")).toString()==latest){
                    setStatus(QStringLiteral("Une mise à jour de scène (%1) est disponible.").arg(latest),10000);
                    return;
                }
                const QString promptText=currentRevision_.isEmpty()
                    ? QStringLiteral("Aucune scène civique n’est installée. La dernière scène publiée est %1.\n\n"
                                     "Voulez-vous la télécharger et la charger maintenant ?").arg(latest)
                    : QStringLiteral("La scène locale est %1 et la dernière publiée est %2.\n\n"
                                     "Voulez-vous télécharger la mise à jour et redémarrer le moteur ?")
                        .arg(currentRevision_,latest);
                QMessageBox prompt(QMessageBox::Information,
                    QStringLiteral("Mise à jour de la scène CSE WARP"),
                    promptText,
                    QMessageBox::NoButton,window_);
                auto* accept=prompt.addButton(currentRevision_.isEmpty()
                                              ?QStringLiteral("Télécharger et charger")
                                              :QStringLiteral("Télécharger et redémarrer"),
                                              QMessageBox::AcceptRole);
                prompt.addButton(QStringLiteral("Plus tard"),QMessageBox::RejectRole);
                prompt.setDefaultButton(qobject_cast<QPushButton*>(accept));
                prompt.exec();
                if(prompt.clickedButton()!=accept){
                    settings.setValue(QStringLiteral("Scene/DeferredRevision"),latest);
                    setStatus(currentRevision_.isEmpty()
                        ? QStringLiteral("Aucune scène civique de référence installée.")
                        : QStringLiteral("Scène %1 conservée.").arg(currentRevision_),8000);
                    return;
                }
                settings.remove(QStringLiteral("Scene/DeferredRevision"));
                latestArchiveUrl_=archiveUrl;
                downloading_=true;
                beginDownload(latest,window_);
            });
        });
    }

    static int revisionNumber(const QString& revision) {
        static const QRegularExpression format(QStringLiteral("^V17\\.(\\d+)$"));
        const auto match=format.match(revision);
        bool ok=false;
        const int number=match.captured(1).toInt(&ok);
        return ok?number:-1;
    }

private:
    void setStatus(const QString& text,int timeout=0) {
        if(auto* status=window_?window_->findChild<QStatusBar*>():nullptr)
            status->showMessage(text,timeout);
    }

    QString dataRoot() const {
        QString root=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if(root.isEmpty())root=QDir::home().filePath(QStringLiteral(".cse-warp"));
        return QDir(root).filePath(QStringLiteral("CSE WARP/V17"));
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
        if(latestArchiveUrl_.isEmpty()){
            fail(QStringLiteral("L’URL de l’archive de scène n’est pas définie."));
            return;
        }
        QNetworkRequest request{QUrl(latestArchiveUrl_)};
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
        QString safeArchive=archivePath_;
        QString safeStaging=stagingPath_;
        safeArchive.replace(QStringLiteral("'"),QStringLiteral("''"));
        safeStaging.replace(QStringLiteral("'"),QStringLiteral("''"));
        process->setProgram(QStringLiteral("powershell.exe"));
        process->setArguments({QStringLiteral("-NoProfile"),QStringLiteral("-NonInteractive"),
             QStringLiteral("-ExecutionPolicy"),QStringLiteral("Bypass"),
             QStringLiteral("-Command"),
             QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                .arg(safeArchive).arg(safeStaging)});
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
        downloading_=false;
        setStatus(QStringLiteral("Scène %1 téléchargée et validée.")
                  .arg(expectedRevision_),15000);
        if(integrate_)integrate_(installedScene);
    }

    void fail(const QString& message) {
        checking_=false;
        downloading_=false;
        setStatus(message,20000);
        qWarning().noquote()<<message;
        if(window_)QMessageBox::warning(window_,QStringLiteral("Mise à jour de la scène"),message);
    }

    QNetworkAccessManager* network_=nullptr;
    QSaveFile* archiveFile_=nullptr;
    QWidget* window_=nullptr;
    std::function<void(const QString&)> integrate_;
    QString currentRevision_;
    QString latestArchiveUrl_;
    QString expectedRevision_;
    QString archivePath_;
    QString stagingPath_;
    bool checking_=false;
    bool downloading_=false;
};
