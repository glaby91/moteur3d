#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace m3d::static_geometry {

template<class Vertex>
struct PreparedMesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t sourceVertexCount=0;
    std::size_t sourceTriangleCount=0;
    std::size_t uniqueVertexCount=0;
    bool indexedPreparationEnabled=false;
    bool valid=false;
};

template<class Vertex>
std::uint64_t hashVertex(const Vertex& vertex) noexcept {
    static_assert(std::is_trivially_copyable<Vertex>::value,
                  "GPU vertex must be trivially copyable");
    const auto* bytes=reinterpret_cast<const unsigned char*>(&vertex);
    std::uint64_t hash=1469598103934665603ull;
    for(std::size_t i=0;i<sizeof(Vertex);++i) {
        hash^=bytes[i];
        hash*=1099511628211ull;
    }
    return hash;
}

template<class Vertex>
bool sameVertex(const Vertex& a,const Vertex& b) noexcept {
    static_assert(std::is_trivially_copyable<Vertex>::value,
                  "GPU vertex must be trivially copyable");
    return std::memcmp(&a,&b,sizeof(Vertex))==0;
}

template<class Vertex>
PreparedMesh<Vertex> prepareStaticMesh(const std::vector<Vertex>& source,
                                       bool enabled) {
    PreparedMesh<Vertex> result;
    result.sourceVertexCount=source.size();
    result.sourceTriangleCount=source.size()/3;
    result.indexedPreparationEnabled=enabled;
    if(source.size()%3!=0 ||
       source.size()>std::size_t(std::numeric_limits<std::uint32_t>::max()))
        return result;

    result.indices.reserve(source.size());
    if(!enabled) {
        result.vertices=source;
        for(std::size_t i=0;i<source.size();++i)
            result.indices.push_back(static_cast<std::uint32_t>(i));
        result.uniqueVertexCount=result.vertices.size();
        result.valid=true;
        return result;
    }

    result.vertices.reserve(source.size());
    std::unordered_map<std::uint64_t,std::vector<std::uint32_t>> buckets;
    buckets.reserve(source.size());
    for(const Vertex& vertex:source) {
        const std::uint64_t hash=hashVertex(vertex);
        auto& candidates=buckets[hash];
        std::uint32_t index=std::numeric_limits<std::uint32_t>::max();
        for(std::uint32_t candidate:candidates) {
            if(sameVertex(result.vertices[candidate],vertex)) {
                index=candidate;
                break;
            }
        }
        if(index==std::numeric_limits<std::uint32_t>::max()) {
            if(result.vertices.size()>=std::size_t(std::numeric_limits<std::uint32_t>::max()))
                return PreparedMesh<Vertex>{};
            index=static_cast<std::uint32_t>(result.vertices.size());
            result.vertices.push_back(vertex);
            candidates.push_back(index);
        }
        result.indices.push_back(index);
    }
    result.uniqueVertexCount=result.vertices.size();
    result.valid=true;
    return result;
}

} // namespace m3d::static_geometry
