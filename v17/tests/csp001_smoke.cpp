#include "src/m3d_csp001_bvh.h"
#include "src/m3d_zreflect_visibility.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
using namespace m3d;
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    QTemporaryDir temp;if(!temp.isValid())return 1;
    Scene src;Obj o;o.kind=Kind::Triangle;o.a={0,0,1};o.b={1,0,1};o.c={0,1,1};
    o.mn={0,0,1};o.mx={1,1,1};src.obj.push_back(o);
    Node n;n.mn=o.mn;n.mx=o.mx;n.start=0;n.count=1;
    src.node.push_back(n);src.order={0};src.root=0;
    LumaTraceBvh4 bvh;std::wstring error;if(!buildLumaTraceBvh4(src,bvh,error))return 2;
    const QString path=temp.filePath("cache.bin");
    if(!csp001::save(path,src,bvh))return 3;
    Scene dst;dst.obj=src.obj;LumaTraceBvh4 b;
    if(!csp001::load(path,dst,b)||dst.root!=0||dst.order!=src.order||b.nodes.size()!=1)return 4;
    dst.obj[0].a.x=.125f; // key must change even if AABB unchanged
    if(csp001::load(path,dst,b)||dst.root!=0||b.nodes.size()!=1)return 5;
    dst.obj=src.obj;QFile f(path);if(!f.open(QIODevice::ReadWrite)||!f.seek(52)||f.write("X",1)!=1)return 6;
    f.close();if(csp001::load(path,dst,b))return 7;
    // Real raster projection uses horizontal FOV. Portrait is regression case.
    Camera c;c.eye={0,0,0};c.f={0,0,1};c.r={1,0,0};c.u={0,1,0};c.tanHalf=1;c.aspect=.5f;
    zreflect::Bounds box;box.add({.75f,1.5f,1});box.add({.8f,1.6f,1.1f});
    if(!zreflect::potentiallyVisible(box,c))return 8;
    zreflect::Bounds hidden;hidden.add({3,0,1});hidden.add({4,1,1.1f});
    if(zreflect::potentiallyVisible(hidden,c))return 9;
    if(!zreflect::potentiallyVisible(zreflect::Bounds{},c))return 10;
    if(std::abs(zreflect::dielectricF0(1.33f)-.02006f)>.0001f)return 11;
    std::cout<<"CSP001 roundtrip, mismatch, corruption, portrait visibility OK\n";return 0;
}
