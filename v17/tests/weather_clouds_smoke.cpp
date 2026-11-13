#include "../weather/clouds.h"
#include <cassert>
#include <cmath>
int main(){
 cse::weather::CloudState s;
 const auto a=cse::weather::cirrusAt(s,0,45);
 const auto b=cse::weather::cirrusAt(s,3600,45);
 const auto n=cse::weather::cirrusAt(s,0,-30);
 assert(a.coverage>=0.05f&&a.coverage<=0.10f);
 assert(b.uOffset!=a.uOffset&&b.vOffset!=a.vOffset);
 assert(a.daylight>n.daylight);
 assert(std::isfinite(b.uOffset)&&std::isfinite(b.vOffset));
 return 0;
}
