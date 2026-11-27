#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_zmaterial_catalog.h"
#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_zmaterial.h"
#include <iostream>
#include <cmath>
using namespace m3d::zmaterial;
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    auto check=[](bool ok,const char* name){if(!ok)std::cerr<<name<<" failed\n";return ok;};
    bool ok=true;QString error;
    QVector<QJsonObject> materials,instances;
    for(int i=0;i<8192;i++){QJsonObject m;m.insert("id",QString::number(i));materials.append(m);}
    for(int i=0;i<10000;i++){QJsonObject o;o.insert("material",QStringLiteral("42"));instances.append(o);}
    auto active=materials;
    ok &= check(selectActive(active,instances,error)&&active.size()==1&&active[0].value("id").toString()=="42","select 1/8192 repeated");
    instances.clear();
    for(int i=0;i<257;i++){QJsonObject o;o.insert("material",QString::number(i));instances.append(o);}
    active=materials;
    ok &= check(!selectActive(active,instances,error)&&active.size()==8192,"reject 257 transactionally");
    instances.removeLast();active=materials;
    ok &= check(selectActive(active,instances,error)&&active.size()==256,"select 256");
    QJsonObject unknown;unknown.insert("material","unknown");instances.append(unknown);active=materials;
    ok &= check(!selectActive(active,instances,error),"unknown material");
    Catalog catalog;QJsonObject m;m.insert("libraryMaterial","inox_01_01");m.insert("roughness",.32);
    ok &= check(catalog.resolve(m,error)&&m.value("metallic").toDouble()==1&&m.value("roughness").toDouble()==.32&&m.value("baseColor").toArray().size()==3,"catalogue override");
    QJsonObject water;water.insert("libraryMaterial","eau_08_16");
    ok &= check(catalog.resolve(water,error)&&water.value("surfaceModel").toString()=="water","water preset");
    QJsonObject invalid;invalid.insert("libraryMaterial","inox_09_01");
    ok &= check(!catalog.resolve(invalid,error),"invalid variant");
    auto p=prepare(1,.25f,true,false,false,true);
    ok &= check(p.metal==1&&p.diffuse==0&&p.roughScale==.25f,"metal prep");
    p=prepare(1,.25f,false,false,false,true);ok &= check(p.metal==0&&p.diffuse==1,"optical bypass");
    p=prepare(1,.25f,true,false,false,false);ok &= check(p.metal==0&&p.diffuse==1,"disable A/B");
    return ok?0:1;
}
