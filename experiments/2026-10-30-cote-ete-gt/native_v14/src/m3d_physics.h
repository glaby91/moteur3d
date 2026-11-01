#pragma once
#include <cstdint>

// Renderer-independent physics contract. The PhysX adapter can be compiled in
// later without making the software renderer depend on the PhysX SDK.
namespace m3d {
namespace physics {

enum class BackendKind : uint8_t { Disabled=0, PhysX=1 };

struct Vec3 { float x=0.f,y=0.f,z=0.f; };
struct Quaternion { float x=0.f,y=0.f,z=0.f,w=1.f; };
struct Transform { Vec3 position; Quaternion rotation; };
struct BodyHandle { uint32_t value=UINT32_MAX; explicit operator bool()const{return value!=UINT32_MAX;} };

struct BodyDesc {
 Transform pose{};
 Vec3 linearVelocity{};
 float massKg=0.f;
 bool kinematic=false;
 bool collisionEnabled=true;
};

class Backend {
public:
 virtual ~Backend()=default;
 virtual BackendKind kind()const noexcept=0;
 virtual bool initialize()=0;
 virtual void shutdown()noexcept=0;
 virtual BodyHandle createBody(const BodyDesc& desc)=0;
 virtual bool removeBody(BodyHandle body)=0;
 virtual void step(float deltaSeconds)=0;
 virtual bool getTransform(BodyHandle body,Transform& out)const=0;
 virtual bool setTransform(BodyHandle body,const Transform& pose)=0;
};

// False until a build explicitly supplies the NVIDIA PhysX SDK adapter.
constexpr bool physXAvailable()noexcept{return false;}

} // namespace physics
} // namespace m3d
