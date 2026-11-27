#pragma once
// Offline R001 eligibility policy. Author decisions precede AUTO heuristics.
#include <cmath>
namespace m3d::zreflect {
struct Policy {
    double maxNominalRoughness=.15, minArea=1., minProjected=.02;
    double maxSlab=.15, maxThicknessRatio=.15, minParallelAreaRatio=.90;
    unsigned futureCameraLimit=3; // reserved, NEVER used by this diagnostic
};
enum class Permission { Unspecified, Disabled, Enabled, Auto, Invalid };
inline bool opticalEligible(bool water,bool mirror,double transmission,double metallic,
                            double roughness,Permission permission,const Policy& p={}){
    if(permission==Permission::Disabled||permission==Permission::Invalid)return false;
    if(permission==Permission::Enabled)return true;
    return std::isfinite(roughness)&&roughness>=0&&roughness<=p.maxNominalRoughness&&
        (water||mirror||transmission>0||metallic>=.80);
}
inline bool slabEligible(double thickness,double lateral,double parallel,double total,const Policy& p={}){
    return std::isfinite(thickness)&&std::isfinite(lateral)&&std::isfinite(parallel)&&std::isfinite(total)&&
        thickness>=0&&lateral>0&&total>0&&thickness<=p.maxSlab&&
        thickness<=p.maxThicknessRatio*lateral&&parallel>=p.minParallelAreaRatio*total;
}
}
