#pragma once
// CSP001: portable, transactional BVH sidecar. Geometry remains authoritative.
#include "m3d_lumatrace_bvh4.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <utility>
namespace m3d::csp001 {
constexpr quint32 magic=0x43535031u, schema=1;
constexpr quint32 maxPrimitives=12000000u;
constexpr qint64 maxFileBytes=128ll*1024*1024; // larger scenes use legacy construction
inline QString path(const QString& root){return QDir(root).filePath("precomputed/csp001-bvh.bin");}
inline void configure(QDataStream& s){
    s.setVersion(QDataStream::Qt_6_0);s.setByteOrder(QDataStream::LittleEndian);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
}
inline bool validBox(V lo,V hi){
    return std::isfinite(lo.x)&&std::isfinite(lo.y)&&std::isfinite(lo.z)&&
        std::isfinite(hi.x)&&std::isfinite(hi.y)&&std::isfinite(hi.z)&&
        lo.x<=hi.x&&lo.y<=hi.y&&lo.z<=hi.z;
}
inline bool contains(V a,V b,V c,V d){
    return a.x<=c.x&&a.y<=c.y&&a.z<=c.z&&b.x>=d.x&&b.y>=d.y&&b.z>=d.z;
}
inline void putV(QDataStream& s,V v){s<<v.x<<v.y<<v.z;}
inline void getV(QDataStream& s,V& v){s>>v.x>>v.y>>v.z;}
// Use bounded chunks, not one heap allocation per triangle. No raw struct padding,
// pointers, host endian, texture content, or mutable lighting in the key.
inline QByteArray geometryKey(const Scene& scene){
    if(scene.obj.empty()||scene.obj.size()>maxPrimitives)return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray chunk;chunk.reserve(4096*60);
    QDataStream s(&chunk,QIODevice::WriteOnly);configure(s);
    s<<quint32(scene.obj.size());
    for(size_t i=0;i<scene.obj.size();++i){
        const auto& o=scene.obj[i];
        if(o.kind!=Kind::Triangle||!validBox(o.mn,o.mx))return {};
        s<<quint32(o.kind);putV(s,o.a);putV(s,o.b);putV(s,o.c);putV(s,o.mn);putV(s,o.mx);
        if((i+1)%4096==0){
            hash.addData(chunk);chunk.clear();s.device()->seek(0);
        }
    }
    if(s.status()!=QDataStream::Ok)return {};
    hash.addData(chunk);return hash.result();
}
struct TreeNode {
    V lo{},hi{};quint32 first=0,count=0,n=0;quint32 child[4]={};
};
inline TreeNode view(const Node& n){
    TreeNode t;t.lo=n.mn;t.hi=n.mx;
    if(n.count>0){t.first=quint32(n.start);t.count=quint32(n.count);}
    else{t.n=2;t.child[0]=quint32(n.left);t.child[1]=quint32(n.right);}
    return t;
}
inline TreeNode view(const LumaTraceBvh4Node& n){
    TreeNode t;t.lo={n.minX,n.minY,n.minZ};t.hi={n.maxX,n.maxY,n.maxZ};
    t.first=n.firstPrimitive;t.count=n.primitiveCount;t.n=n.childCount;
    t.child[0]=n.child0;t.child[1]=n.child1;t.child[2]=n.child2;t.child[3]=n.child3;return t;
}
inline bool validateOrder(const Scene& scene,const std::vector<uint32_t>& order){
    if(order.size()!=scene.obj.size())return false;
    std::vector<uint8_t> seen(order.size(),0);
    for(auto id:order){if(id>=seen.size()||seen[id])return false;seen[id]=1;}
    return true;
}
// Iterative traversal: validate bounds, coverage, child indices, cycles, depth.
// Reject invalid sidecar before modifying the scene; tree leaf spans must
// partition the complete primitive order, not merely have the correct count.
template<class N> inline bool validateTree(const Scene& scene,const std::vector<uint32_t>& order,
                                          const std::vector<N>& nodes,unsigned maxChildren){
    if(nodes.empty()||nodes.size()>2*order.size())return false;
    struct Pending{quint32 id,depth;};
    std::vector<Pending> work{{0,0}};
    std::vector<uint8_t> seen(nodes.size(),0),covered(order.size(),0);
    size_t visited=0,leaves=0;
    while(!work.empty()){
        const auto item=work.back();work.pop_back();
        if(item.id>=nodes.size()||seen[item.id]||item.depth>(maxChildren==4?20u:48u))return false;
        seen[item.id]=1;++visited;
        const auto t=view(nodes[item.id]);
        if(!validBox(t.lo,t.hi))return false;
        if(t.count){
            if(t.n||t.first>order.size()||t.count>order.size()-t.first||t.count>6)return false;
            for(quint32 k=0;k<t.count;++k){
                const auto slot=t.first+k;
                if(covered[slot]||order[slot]>=scene.obj.size())return false;
                covered[slot]=1;++leaves;
                const auto& o=scene.obj[order[slot]];
                if(!contains(t.lo,t.hi,o.mn,o.mx))return false;
            }
        }else{
            if(t.n<2||t.n>maxChildren)return false;
            for(quint32 k=0;k<t.n;++k){
                const auto child=t.child[k];
                if(child<=item.id||child>=nodes.size())return false;
                const auto c=view(nodes[child]);
                if(!validBox(c.lo,c.hi)||!contains(t.lo,t.hi,c.lo,c.hi))return false;
                work.push_back({child,item.depth+1});
            }
        }
    }
    return visited==nodes.size()&&leaves==order.size();
}
inline void writeNode(QDataStream& s,const Node& n){
    putV(s,n.mn);putV(s,n.mx);
    s<<qint32(n.left)<<qint32(n.right)<<qint32(n.start)<<qint32(n.count)<<quint8(n.simdTrianglePair);
}
inline bool readNode(QDataStream& s,Node& n){
    qint32 l,r,a,c;quint8 pair;
    getV(s,n.mn);getV(s,n.mx);s>>l>>r>>a>>c>>pair;
    if(s.status()!=QDataStream::Ok||pair!=0||a<0||c<0||(c>0&&(l!=-1||r!=-1)))return false;
    n.left=l;n.right=r;n.start=a;n.count=c;n.simdTrianglePair=false;return true;
}
inline void writeNode(QDataStream& s,const LumaTraceBvh4Node& n){
    s<<n.minX<<n.minY<<n.minZ<<quint32(n.child0)<<n.maxX<<n.maxY<<n.maxZ<<quint32(n.child1)
      <<quint32(n.child2)<<quint32(n.child3)<<quint32(n.firstPrimitive)<<quint32(n.primitiveCount)
      <<quint32(n.childCount)<<quint32(n.padding);
}
inline bool readNode(QDataStream& s,LumaTraceBvh4Node& n){
    s>>n.minX>>n.minY>>n.minZ>>n.child0>>n.maxX>>n.maxY>>n.maxZ>>n.child1
     >>n.child2>>n.child3>>n.firstPrimitive>>n.primitiveCount>>n.childCount>>n.padding;
    return s.status()==QDataStream::Ok&&n.padding==0;
}
inline bool save(const QString& file,const Scene& scene,const LumaTraceBvh4& bvh){
    auto key=geometryKey(scene);
    if(key.size()!=32||scene.root!=0||bvh.primitiveCount!=scene.order.size()||
       !validateOrder(scene,scene.order)||!validateTree(scene,scene.order,scene.node,2)||
       !validateTree(scene,scene.order,bvh.nodes,4))return false;
    const quint64 expected=52ull+4ull*scene.order.size()+41ull*scene.node.size()+56ull*bvh.nodes.size();
    if(expected>quint64(maxFileBytes-32))return false;
    QByteArray payload;payload.reserve(qsizetype(expected));QDataStream s(&payload,QIODevice::WriteOnly);configure(s);
    s<<magic<<schema<<quint32(scene.order.size())<<quint32(scene.node.size())<<quint32(bvh.nodes.size());
    if(s.writeRawData(key.constData(),32)!=32)return false;
    for(auto id:scene.order)s<<quint32(id);
    for(const auto& n:scene.node)writeNode(s,n);
    for(const auto& n:bvh.nodes)writeNode(s,n);
    if(s.status()!=QDataStream::Ok||payload.size()>maxFileBytes-32)return false;
    const auto digest=QCryptographicHash::hash(payload,QCryptographicHash::Sha256);
    QSaveFile out(file);
    return out.open(QIODevice::WriteOnly)&&out.write(payload)==payload.size()&&
        out.write(digest)==digest.size()&&out.commit();
}
inline bool load(const QString& file,Scene& scene,LumaTraceBvh4& bvh){
    QFile in(file);
    if(!in.open(QIODevice::ReadOnly)||in.size()<84||in.size()>maxFileBytes)return false;
    QByteArray payload=in.readAll();
    if(payload.size()!=in.size())return false;
    const auto digest=payload.right(32);payload.chop(32);
    if(QCryptographicHash::hash(payload,QCryptographicHash::Sha256)!=digest)return false;
    QDataStream s(payload);configure(s);
    quint32 tag=0,ver=0,p=0,n=0,m=0;s>>tag>>ver>>p>>n>>m;
    if(s.status()!=QDataStream::Ok)return false;
    if(tag!=magic||ver!=schema||p==0||p>maxPrimitives||p!=scene.obj.size()||
       n==0||m==0||n>2ull*p||m>n||
       quint64(payload.size())!=52ull+4ull*p+41ull*n+56ull*m)return false;
    QByteArray key(32,'\0');if(s.readRawData(key.data(),32)!=32||key!=geometryKey(scene))return false;
    std::vector<uint32_t> order(p);std::vector<Node> binary(n);LumaTraceBvh4 four;
    four.nodes.resize(m);four.primitiveCount=p;
    for(auto& id:order)s>>id;
    for(auto& v:binary)if(!readNode(s,v))return false;
    for(auto& v:four.nodes)if(!readNode(s,v))return false;
    if(s.status()!=QDataStream::Ok||!s.atEnd()||!validateOrder(scene,order)||
       !validateTree(scene,order,binary,2)||!validateTree(scene,order,four.nodes,4))return false;
    scene.order=std::move(order);scene.node=std::move(binary);scene.root=0;bvh=std::move(four);return true;
}
} // namespace m3d::csp001
