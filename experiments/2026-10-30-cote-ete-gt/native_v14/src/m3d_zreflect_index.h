#pragma once
// CPU-side immutable broadphase for logical optical camera descriptors.
// No Qt, no GPU resource, no frame-time scan of the full descriptor array.
#include "m3d_zreflect_visibility.h"
#include <algorithm>
#include <cstdint>
#include <vector>
namespace m3d::zreflect {
struct IndexedPlane { uint32_t id=0; Bounds bounds; };
struct QueryStats { size_t nodeVisits=0, planeTests=0, candidates=0; };
class SpatialIndex {
    struct Node { Bounds bounds; uint32_t begin=0,end=0,left=0,right=0; bool leaf=true; };
    std::vector<IndexedPlane> entries_;
    std::vector<uint32_t> uncertain_;
    std::vector<Node> nodes_;
    static double axis(V p,int a) { return a==0?p.x:a==1?p.y:p.z; }
    static double center(const Bounds& b,int a) { return (axis(b.lo,a)+axis(b.hi,a))*.5; }
    static bool usable(const Bounds& b) {
        return b.valid&&!b.uncertain&&std::isfinite(b.lo.x)&&std::isfinite(b.lo.y)&&std::isfinite(b.lo.z)
            &&std::isfinite(b.hi.x)&&std::isfinite(b.hi.y)&&std::isfinite(b.hi.z)
            &&b.lo.x<=b.hi.x&&b.lo.y<=b.hi.y&&b.lo.z<=b.hi.z;
    }
    uint32_t buildNode(uint32_t begin,uint32_t end) {
        Node n; n.begin=begin;n.end=end;
        Bounds centers;
        for(uint32_t i=begin;i<end;++i){
            const auto& b=entries_[i].bounds;n.bounds.add(b.lo);n.bounds.add(b.hi);
            centers.add({float(center(b,0)),float(center(b,1)),float(center(b,2))});
        }
        const auto id=uint32_t(nodes_.size());nodes_.push_back(n);
        if(end-begin<=4)return id;
        const double x=axis(centers.hi,0)-axis(centers.lo,0),y=axis(centers.hi,1)-axis(centers.lo,1),z=axis(centers.hi,2)-axis(centers.lo,2);
        const int a=x>=y&&x>=z?0:y>=z?1:2;
        const uint32_t mid=begin+(end-begin)/2;
        std::nth_element(entries_.begin()+begin,entries_.begin()+mid,entries_.begin()+end,[a](const IndexedPlane& l,const IndexedPlane& r){
            const double lc=center(l.bounds,a),rc=center(r.bounds,a);return lc<rc||(lc==rc&&l.id<r.id);
        });
        const auto left=buildNode(begin,mid),right=buildNode(mid,end);
        nodes_[id].leaf=false;nodes_[id].left=left;nodes_[id].right=right;return id;
    }
    void visit(uint32_t id,const Camera& c,std::vector<uint32_t>& out,QueryStats& stats) const {
        const Node& n=nodes_[id];++stats.nodeVisits;
        if(!potentiallyVisible(n.bounds,c))return;
        if(!n.leaf){visit(n.left,c,out,stats);visit(n.right,c,out,stats);return;}
        for(uint32_t i=n.begin;i<n.end;++i){++stats.planeTests;
            if(potentiallyVisible(entries_[i].bounds,c))out.push_back(entries_[i].id);
        }
    }
public:
    void build(const std::vector<IndexedPlane>& planes) {
        entries_.clear();uncertain_.clear();nodes_.clear();entries_.reserve(planes.size());
        for(const auto& p:planes)if(usable(p.bounds))entries_.push_back(p);else uncertain_.push_back(p.id);
        if(!entries_.empty())buildNode(0,uint32_t(entries_.size()));
    }
    QueryStats query(const Camera& c,std::vector<uint32_t>& out) const {
        QueryStats stats;out.clear();
        // Unknown bounds must never disappear. No cap and no per-ID allocation.
        out.insert(out.end(),uncertain_.begin(),uncertain_.end());stats.planeTests=uncertain_.size();
        if(!nodes_.empty())visit(0,c,out,stats);
        std::sort(out.begin(),out.end());stats.candidates=out.size();return stats;
    }
    bool snapshot(std::vector<OpticalIndexNode>& nodes,std::vector<uint32_t>& order)const{
        nodes.clear();order.clear();if(!uncertain_.empty())return false;
        for(const auto& e:entries_)order.push_back(e.id);
        for(const auto& n:nodes_)nodes.push_back({n.bounds.lo,n.bounds.hi,n.begin,n.end,n.left,n.right,n.leaf});
        return true;
    }
    bool restore(const std::vector<IndexedPlane>& planes,const std::vector<OpticalIndexNode>& nodes,
                 const std::vector<uint32_t>& order){
        if(planes.empty()){if(!nodes.empty()||!order.empty())return false;build({});return true;}
        if(order.size()!=planes.size()||nodes.empty()||nodes.size()>2*planes.size())return false;
        std::vector<IndexedPlane> entries;std::vector<Node> restored;
        std::vector<uint8_t> seen(planes.size(),0),visited(nodes.size(),0),covered(order.size(),0);
        for(auto id:order){if(id>=planes.size()||seen[id]||planes[id].id!=id||!usable(planes[id].bounds))return false;
            seen[id]=1;entries.push_back(planes[id]);}
        for(const auto& n:nodes){
            Bounds b;b.add(n.lo);b.add(n.hi);
            if(!usable(b)||n.lo.x>n.hi.x||n.lo.y>n.hi.y||n.lo.z>n.hi.z||
               n.begin>=n.end||n.end>order.size())return false;
            restored.push_back({b,n.begin,n.end,n.left,n.right,n.leaf});
        }
        auto contains=[](const Bounds& a,const Bounds& b){
            return a.lo.x<=b.lo.x&&a.lo.y<=b.lo.y&&a.lo.z<=b.lo.z&&
                   a.hi.x>=b.hi.x&&a.hi.y>=b.hi.y&&a.hi.z>=b.hi.z;
        };
        if(restored[0].begin!=0||restored[0].end!=order.size())return false;
        std::vector<std::pair<uint32_t,uint32_t>> stack{{0,0}};size_t count=0,total=0;
        while(!stack.empty()){
            const auto item=stack.back();stack.pop_back();const auto id=item.first;
            if(id>=restored.size()||visited[id]||item.second>48)return false;
            visited[id]=1;++count;const auto& n=restored[id];
            if(n.leaf){
                if(n.end-n.begin>4)return false;
                for(uint32_t k=n.begin;k<n.end;++k){if(covered[k]||!contains(n.bounds,entries[k].bounds))return false;covered[k]=1;++total;}
            }else{
                if(n.left<=id||n.right<=id||n.left>=restored.size()||n.right>=restored.size()||n.left==n.right)return false;
                const auto& l=restored[n.left];const auto& r=restored[n.right];
                if(l.begin!=n.begin||l.end!=r.begin||r.end!=n.end||!contains(n.bounds,l.bounds)||!contains(n.bounds,r.bounds))return false;
                stack.push_back({n.left,item.second+1});stack.push_back({n.right,item.second+1});
            }
        }
        if(count!=restored.size()||total!=order.size())return false;
        entries_=std::move(entries);nodes_=std::move(restored);uncertain_.clear();return true;
    }
    size_t nodeCount()const{return nodes_.size();}
};
}
