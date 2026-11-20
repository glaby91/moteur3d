#include "m3d_lumatrace_bvh4.h"

#include <algorithm>
#include <array>
#include <limits>

namespace m3d {

bool buildLumaTraceBvh4(const Scene& scene,LumaTraceBvh4& out,
                        std::wstring& error) {
    out={};
    if(scene.root<0 || std::size_t(scene.root)>=scene.node.size()) {
        error=L"LumaTrace BVH4 : la scène V17 ne possède pas de BVH valide.";
        return false;
    }
    if(scene.order.empty()) {
        error=L"LumaTrace BVH4 : aucun primitive dans le BVH V17.";
        return false;
    }
    for(const Obj& object:scene.obj) {
        if(object.kind!=Kind::Triangle) {
            error=L"LumaTrace BVH4 : cette version de test exige une scène triangulée.";
            return false;
        }
    }
    for(std::uint32_t id:scene.order) {
        if(id>=scene.obj.size()) {
            error=L"LumaTrace BVH4 : l’ordre de primitives V17 est incohérent.";
            return false;
        }
    }

    std::vector<std::uint64_t> subtreePrimitiveCounts(scene.node.size(),0);
    std::vector<std::uint8_t> countState(scene.node.size(),0);
    const auto countSubtree=[&](auto&& self,std::uint32_t at)->std::uint64_t {
        if(at>=scene.node.size())return 0;
        if(countState[at]==2)return subtreePrimitiveCounts[at];
        if(countState[at]==1)return 0; // malformed cycle: rejected by emit below
        countState[at]=1;
        const Node& n=scene.node[at];
        std::uint64_t count=0;
        if(n.count>0)count=std::uint64_t(n.count);
        else if(n.left>=0&&n.right>=0)
            count=self(self,std::uint32_t(n.left))+self(self,std::uint32_t(n.right));
        countState[at]=2;
        return subtreePrimitiveCounts[at]=count;
    };
    countSubtree(countSubtree,std::uint32_t(scene.root));
    std::vector<std::uint8_t> emitState(scene.node.size(),0);

    // Convert recursively while expanding the most beneficial internal nodes.
    // This preserves V17's SAH partition and triangle ordering; it changes
    // only the upper-level fan-out consumed by the LumaTrace traversal shader.
    const auto emit=[&](auto&& self,std::uint32_t binaryIndex)->std::uint32_t {
        if(binaryIndex>=scene.node.size()||emitState[binaryIndex]!=0)
            return std::numeric_limits<std::uint32_t>::max();
        emitState[binaryIndex]=1;
        const std::uint32_t dst=std::uint32_t(out.nodes.size());
        out.nodes.emplace_back();
        const Node& source=scene.node[binaryIndex];
        LumaTraceBvh4Node node{};
        node.minX=source.mn.x;node.minY=source.mn.y;node.minZ=source.mn.z;
        node.maxX=source.mx.x;node.maxY=source.mx.y;node.maxZ=source.mx.z;
        if(source.count>0) {
            if(source.start<0 || std::size_t(source.start)+std::size_t(source.count)>scene.order.size())
                return std::numeric_limits<std::uint32_t>::max();
            node.firstPrimitive=std::uint32_t(source.start);
            node.primitiveCount=std::uint32_t(source.count);
            out.nodes[dst]=node;
            emitState[binaryIndex]=2;
            return dst;
        }
        if(source.left<0 || source.right<0 ||
           std::size_t(source.left)>=scene.node.size() ||
           std::size_t(source.right)>=scene.node.size())
            return std::numeric_limits<std::uint32_t>::max();

        std::array<std::uint32_t,4> children{};
        std::uint32_t childCount=2;
        children[0]=std::uint32_t(source.left);
        children[1]=std::uint32_t(source.right);
        while(childCount<4) {
            std::uint32_t selected=childCount;
            std::int64_t bestGain=std::numeric_limits<std::int64_t>::min();
            for(std::uint32_t i=0;i<childCount;++i) {
                const Node& candidate=scene.node[children[i]];
                if(candidate.count>0 || candidate.left<0 || candidate.right<0)continue;
                // Prefer expanding the child covering more primitives. This is
                // deterministic and avoids recomputing SAH data already baked
                // into V17's tree.
                const std::uint64_t weight=subtreePrimitiveCounts[children[i]];
                const std::int64_t gain=std::int64_t(std::min<std::uint64_t>(
                    weight,std::uint64_t(std::numeric_limits<std::int64_t>::max())));
                if(gain>bestGain){bestGain=gain;selected=i;}
            }
            if(selected==childCount)break;
            const Node& expand=scene.node[children[selected]];
            for(std::uint32_t j=childCount;j>selected+1;--j)children[j]=children[j-1];
            children[selected]=std::uint32_t(expand.left);
            children[selected+1]=std::uint32_t(expand.right);
            ++childCount;
        }
        std::array<std::uint32_t,4> emitted{};
        for(std::uint32_t i=0;i<childCount;++i) {
            emitted[i]=self(self,children[i]);
            if(emitted[i]==std::numeric_limits<std::uint32_t>::max())
                return emitted[i];
        }
        node.child0=emitted[0];node.child1=emitted[1];
        node.child2=emitted[2];node.child3=emitted[3];
        node.childCount=childCount;
        out.nodes[dst]=node;
        emitState[binaryIndex]=2;
        return dst;
    };

    out.nodes.reserve(scene.node.size());
    const std::uint32_t root=emit(emit,std::uint32_t(scene.root));
    if(root!=0 || out.nodes.empty() || out.nodes.size()>scene.node.size()) {
        out={};
        error=L"LumaTrace BVH4 : conversion de l’arbre V17 impossible.";
        return false;
    }
    out.primitiveCount=std::uint32_t(scene.order.size());
    error.clear();
    return true;
}

} // namespace m3d
