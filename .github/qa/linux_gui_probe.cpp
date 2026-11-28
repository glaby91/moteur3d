#include <QtWidgets>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <dlfcn.h>
#include <functional>
#include <iostream>
namespace {
QString out; QMainWindow* win=nullptr; QJsonArray cases; QStringList dialogs;
void pause(int ms){QEventLoop l;QTimer::singleShot(ms,&l,&QEventLoop::quit);l.exec();}
bool wait(const std::function<bool()>& f,int limit=90000){QElapsedTimer t;t.start();while(t.elapsed()<limit){if(f())return true;pause(100);}return f();}
void record(QString n,QString s,QString d={}){cases.append(QJsonObject{{"test",n},{"status",s},{"detail",d}});QFile f(out+"/gui-results.json");if(f.open(QIODevice::WriteOnly|QIODevice::Truncate))f.write(QJsonDocument(cases).toJson());std::cout<<n.toStdString()<<" "<<s.toStdString()<<" "<<d.toStdString()<<std::endl;}
void check(QString n,bool ok,QString d={}){record(n,ok?"PASS":"FAIL",d);}
void shot(QString n){if(auto* s=QGuiApplication::primaryScreen())s->grabWindow(0).save(out+"/"+n+".png");}
QAction* action(QString s){for(auto* x:win->findChildren<QAction*>())if(x->text().contains(s,Qt::CaseInsensitive))return x;return nullptr;}
QPushButton* button(QString s){for(auto* x:win->findChildren<QPushButton*>())if(x->text().contains(s,Qt::CaseInsensitive))return x;return nullptr;}
QCheckBox* box(QString s){for(auto* x:win->findChildren<QCheckBox*>())if(x->text().contains(s,Qt::CaseInsensitive))return x;return nullptr;}
template<class T>T* field(QString label){for(auto* form:win->findChildren<QFormLayout*>())for(int i=0;i<form->rowCount();++i){auto* item=form->itemAt(i,QFormLayout::LabelRole);auto* l=item?qobject_cast<QLabel*>(item->widget()):nullptr;if(l&&l->text().contains(label,Qt::CaseInsensitive)){auto* f=form->itemAt(i,QFormLayout::FieldRole);if(f)return qobject_cast<T*>(f->widget());}}return nullptr;}
QString status(){return win->statusBar()->currentMessage();}
bool rendered(QString expected,int timeout=90000){return wait([&]{auto* l=win->findChild<QLabel*>("v16LastRenderTime");return l&&l->toolTip().contains(expected,Qt::CaseInsensitive)&&status().contains(" ms")&&!status().contains("Calcul");},timeout);}
void handle(const std::function<void()>& fn,QString path={}){
 QElapsedTimer deadline;deadline.start(); bool handling=false;
 dialogs.clear();QTimer t;t.setInterval(100);
 QObject::connect(&t,&QTimer::timeout,[&]{if(handling)return;QScopedValueRollback<bool> guard(handling,true);for(auto* w:QApplication::topLevelWidgets()){if(!w->isVisible())continue;
 if(auto* d=qobject_cast<QFileDialog*>(w)){if(path.isEmpty()||deadline.elapsed()>10000)d->reject();else{d->selectFile(path);QMetaObject::invokeMethod(d,"accept",Qt::DirectConnection);}}
 else if(auto* m=qobject_cast<QMessageBox*>(w)){dialogs<<m->windowTitle()+": "+m->text();m->done(QMessageBox::Ok);}}});
 t.start();fn();t.stop();pause(150);
}
bool invoke(QString s,QString file={}){auto* a=action(s);if(!a)return false;handle([&]{a->trigger();},file);return true;}
bool png(QString n){QString f=out+"/"+n+".png";QFile::remove(f);return invoke("Exporter l'image PNG",f)&&!QImage(f).isNull();}
QByteArray hash(QString n){QFile f(out+"/"+n+".png");if(!f.open(QIODevice::ReadOnly))return {};return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256);}
void toggle(QString s){auto* b=box(s);if(!b||!b->isEnabled()){record(s,"SKIP","missing or disabled");return;}bool old=b->isChecked();b->click();pause(150);check(s,b->isChecked()!=old,"UI state only");}

int completions=0;
QString lastStatus;
bool fresh(const std::function<void()>& op,QString expected,int limit=90000){
 int before=completions;op();
 return wait([&]{auto* l=win->findChild<QLabel*>("v16LastRenderTime");return completions>before&&l&&l->toolTip().contains(expected,Qt::CaseInsensitive);},limit);
}
void run(){
 out=qEnvironmentVariable("CSE_GUI_QA_OUT");QDir().mkpath(out);QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
 const QString task=qEnvironmentVariable("CSE_GUI_QA_CASE");
 record("Case started","INFO",task);
 for(auto* w:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMainWindow*>(w)){win=m;break;}
 check("Qt main window",win!=nullptr);if(!win){QCoreApplication::exit(2);return;}
 QObject::connect(win->statusBar(),&QStatusBar::messageChanged,win,[](const QString& s){
   lastStatus=s;if(s.contains(" ms")&&(s.contains("Vulkan",Qt::CaseInsensitive)||s.contains("CPU",Qt::CaseInsensitive)))++completions;
 });
 auto* cam=field<QComboBox>("Caméra");auto* backend=field<QComboBox>("Méthode de rendu");auto* res=field<QComboBox>("Résolution");
 const bool loaded=cam&&backend&&res&&wait([&]{return cam->count()>1;},120000);
 check("Scene loaded",loaded,loaded?QString::number(cam->count())+" cameras":status());
 if(!loaded){shot("load-fail");QCoreApplication::exit(2);return;}
 bool vulkan=rendered("Vulkan",90000);
 check("Initial Vulkan render",vulkan,status());shot("initial");
 if(task.startsWith("camera-")){
   int i=task.mid(7).toInt()-1;
   if(i<0||i>=cam->count())record("Camera","FAIL","index unavailable");
   else {
     const QString name=cam->itemText(i);
     bool ok=vulkan;
     if(cam->currentIndex()!=i)ok=fresh([&]{cam->setCurrentIndex(i);},"Vulkan",60000);
     check("Camera selection",cam->currentIndex()==i,name);
     check("New Vulkan frame",ok,status());shot("camera");
     if(ok){auto* a=action("Copier la position caméra");if(a){a->trigger();QFile f(out+"/pose.json");if(f.open(QIODevice::WriteOnly))f.write(QGuiApplication::clipboard()->text().toUtf8());}}
   }
 }else if(task=="backends"){
   auto* quick=win->findChild<QComboBox*>("v17QuickRenderer");
   check("Linux renderer menu",backend->count()==7&&quick&&quick->currentIndex()==backend->currentIndex()&&!(backend->model()->flags(backend->model()->index(4,0))&Qt::ItemIsEnabled)&&!(backend->model()->flags(backend->model()->index(6,0))&Qt::ItemIsEnabled));
   check("Full HD",fresh([&]{res->setCurrentIndex(2);},"Vulkan",90000),status());shot("fhd");
 }else if(task.startsWith("cpu-")){
   int i=task=="cpu-direct"?0:task=="cpu-gi2"?2:3;
   if(i==0){if(auto* s=field<QComboBox>("Échantillons CPU direct"))s->setCurrentIndex(0);}
   else{if(auto* s=field<QComboBox>("Échantillons GI"))s->setCurrentIndex(0);}
   bool ok=fresh([&]{backend->setCurrentIndex(i);},i==0?"CPU":"GI",180000);
   check("CPU backend render",ok,status());shot(task);
 }else if(task=="lighting"){
   auto* hour=field<QDoubleSpinBox>("Heure solaire");
   if(hour){
     bool day=fresh([&]{hour->setValue(13);},"Vulkan");shot("day");
     bool night=fresh([&]{hour->setValue(0);},"Vulkan");shot("night");
     check("Manual day/night",day&&night,status());
     if(auto* c=box("Simulation jour/nuit")){double before=hour->value();c->setChecked(true);pause(3000);check("Automatic solar timer",hour->value()!=before);c->setChecked(false);}
   }
   for(const QString& s:QStringList{"Nuages animés","Éclairage extérieur automatique","Rebonds indirects 3D","Couleurs fidèles","Compression tonale","Antialiasing des contours"})toggle(s);
   if(auto* e=box("Couleurs fidèles"))e->setChecked(true);
   if(auto* e=field<QDoubleSpinBox>("Exposition linéaire")){e->setValue(.85);check("Exposure UI",qAbs(e->value()-.85)<.001);}
   if(auto* a=field<QSlider>("Rotation caméra")){a->setValue(25);check("Azimuth UI",a->value()==25);}
   shot("lighting-controls");
 }else if(task=="raster"){
   for(const QString& s:QStringList{"Préréglage","Anti-aliasing GPU","Textures PBR","Distance détails"}){
     auto* c=field<QComboBox>(s);
     if(c&&c->isEnabled()&&c->count()>1){c->setCurrentIndex((c->currentIndex()+1)%c->count());check("Raster "+s,!status().contains("DirectX 11 indisponible"),status());}
     else record("Raster "+s,"SKIP","disabled/missing");
   }
   for(const QString& s:QStringList{"Frustum","LOD adaptatif"}){
     auto* c=box(s);if(c&&c->isEnabled()){c->click();check("Raster "+s,!status().contains("DirectX 11 indisponible"),status());}
     else record("Raster "+s,"SKIP","disabled/missing");
   }
   shot("raster");
 }else if(task=="menus"){
   for(const QString& s:QStringList{"Afficher le HUD de la fenêtre","Afficher les coordonnées caméra"}){auto* a=action(s);if(a){bool old=a->isChecked();a->trigger();check(s,a->isChecked()!=old);a->trigger();}}
   if(auto* a=action("Plein écran 3D")){a->setChecked(true);pause(500);bool full=false;for(auto* w:QApplication::topLevelWidgets())if(w->isVisible()&&w->isFullScreen())full=true;shot("fullscreen");a->setChecked(false);pause(500);check("F11 and return",full&&win->menuBar()->isVisible());}
   check("About dialog",invoke("À propos")&&dialogs.join(" ").contains("V17"),dialogs.join(" ").left(400));
   if(auto* a=action("Copier la position caméra")){a->trigger();QJsonParseError e;auto d=QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8(),&e);check("Camera JSON clipboard",e.error==QJsonParseError::NoError&&d.isObject());}
   for(auto* m:win->findChildren<QMenu*>())if(m->title()=="Outils")record("Benchmark menu",m->menuAction()->isVisible()?"PASS":"SKIP","hidden if unavailable");
 }else if(task=="export"){
   record("PNG export","INFO","opening file dialog");
   check("PNG export",vulkan&&png("exported"),dialogs.join(" "));shot("export");
 }else if(task=="anomalies"){
   if(vulkan&&invoke("Capturer une anomalie")){
     QDir dir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/CSE WARP/Anomalies");auto js=dir.entryList({"*.json"},QDir::Files,QDir::Time);
     bool pair=!js.isEmpty()&&QFileInfo::exists(dir.filePath(js.first().chopped(5)+".png"));check("Anomaly PNG + JSON",pair);
     if(pair){QFile::copy(dir.filePath(js.first()),out+"/anomaly.json");QFile::copy(dir.filePath(js.first().chopped(5)+".png"),out+"/anomaly.png");bool ok=fresh([&]{invoke("Restaurer une caméra",dir.filePath(js.first()));},"Vulkan");check("Restore anomaly",ok&&dialogs.isEmpty(),dialogs.join(" "));}
   }
 }else if(task=="materials"){
   QTreeWidget* tree=nullptr;for(auto* t:win->findChildren<QTreeWidget*>())if(t->headerItem()->text(0).contains("Ressource"))tree=t;
   check("Resource tree populated",tree&&tree->topLevelItemCount()>0);
   if(tree){for(int i=0;i<tree->topLevelItemCount();++i){auto* cat=tree->topLevelItem(i);if(cat->text(0).contains("Catalogue")&&cat->childCount()){
     auto* leaf=cat->child(0);tree->setCurrentItem(leaf);auto* r=field<QDoubleSpinBox>("Rugosité");auto* m=field<QDoubleSpinBox>("Métallicité");
     if(r&&m){double old=r->value(),metal=m->value();r->setValue(old>.5?.3:.7);m->setValue(metal>.5?.2:.8);check("Material editor UI",r->value()!=old&&m->value()!=metal,leaf->text(0));r->setValue(old);m->setValue(metal);invoke("Enregistrer les matériaux");check("Save material catalogue",status().contains("Matériaux enregistrés"),status());}break;}}}
   shot("materials");
 }else if(task=="gibake"){
   if(auto* b=button("Précalculer la table GI")){handle([&]{b->click();});check("GI bake launcher",dialogs.isEmpty()&&!b->isEnabled(),dialogs.join(" "));}else record("GI bake","FAIL","button missing");
 }else if(task=="scene"){
   QString other=qEnvironmentVariable("CSE_GUI_QA_SECOND_SCENE");
   if(QFileInfo::exists(other)){invoke("Ouvrir une scène",other);check("Open secondary scene",wait([&]{return win->windowTitle().contains("pavillon",Qt::CaseInsensitive);},120000)&&rendered("Vulkan"),win->windowTitle());}
   else record("Open scene","SKIP","secondary manifest missing");
 }else if(task=="audio"){
   if(auto* b=button("Activer la radio")){
     b->click();bool local=wait([&]{for(auto* l:win->findChildren<QLabel*>())if(l->text().contains("lecture du WAV local"))return true;return false;},20000);
     check("Radio offline fallback",local);shot("radio");
     pause(3000);
     if(auto* stop=button("Arrêter la radio")){stop->click();check("Radio stop",button("Activer la radio")!=nullptr);}
     else record("Radio stop","FAIL","stop unavailable");
   }else record("Radio toggle","FAIL","missing");
 }
 shot("final");record("Case finished","INFO",task);QCoreApplication::exit(0);
}
}
int QApplication::exec(){using Fn=int(*)();auto original=reinterpret_cast<Fn>(dlsym(RTLD_NEXT,"_ZN12QApplication4execEv"));if(!original)return 99;QTimer::singleShot(0,[]{run();});return original();}
