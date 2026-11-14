#include "../weather/astronomy.h"
#include <cassert>
#include <cmath>
int main(){
 using namespace cse::weather;
 // Winter and summer noon in Ukraine must have different solar elevations.
 const auto summer=calculate(1782129600,50.45,30.52);
 const auto winter=calculate(1797940800,50.45,30.52);
 assert(std::isfinite(summer.sun.altitudeDeg));
 assert(std::isfinite(summer.moon.altitudeDeg));
 assert(summer.moonIllumination>=0&&summer.moonIllumination<=1);
 assert(summer.siderealDeg>=0&&summer.siderealDeg<360);
 assert(summer.sun.altitudeDeg>winter.sun.altitudeDeg);
 const auto later=calculate(1782129600+3600,50.45,30.52);
 assert(std::abs(later.siderealDeg-summer.siderealDeg)>10);
 return 0;
}
