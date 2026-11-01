#include "m3d_engine.h"

namespace m3d {
V sun=norm({.70f,.78f,-.714f}),skyZenith={126,170,209},skyHorizon={244,185,134},sunTint={255,246,223};
float daylight=1.f,lampFactor=0.f;
std::vector<Light> pointLights;
void updateLighting(float hour){
 float t=clampf((hour-6.f)/13.f,0.f,1.f);
 daylight=std::sin(PI*t);
 float dusk=clampf(1.f-std::abs(hour-18.3f)/2.3f,0.f,1.f),dawn=clampf(1.f-std::abs(hour-7.1f)/1.8f,0.f,1.f),warm=std::max(dusk,dawn*.72f);
 lampFactor=clampf((hour-17.f)/3.f,0.f,1.f);
 sun=norm({.70f,-.16f+.92f*daylight,-.714f});
 V dayZenith={126,170,209},nightZenith={28,23,39},dayHorizon={244,185,134},nightHorizon={77,45,38};
 skyZenith=dayZenith*daylight+nightZenith*(1-daylight);
 skyHorizon=dayHorizon*daylight+nightHorizon*(1-daylight);
 skyHorizon=skyHorizon*(1-warm)+V{255,139,92}*warm;
 sunTint=V{255,246,223}*(1-warm)+V{255,128,70}*warm;
 pointLights.clear();
 if(lampFactor>.005f){
  pointLights.push_back({{6.35f,.72f,-4.2f},11.5f,3.2f*lampFactor,{255,166,92}});
  pointLights.push_back({{8.05f,.72f,-11.6f},11.5f,2.5f*lampFactor,{255,194,126}});
  for(int i=0;i<14;i++)pointLights.push_back({{4.87f,4.58f,-12.f-18.f*i},14.f,5.8f*lampFactor,{255,151,72}});
  // Phares chauds des trois voitures, assez bas pour éclairer la chaussée.
  pointLights.push_back({{-2.05f,.53f,-9.95f},8.f,1.8f*lampFactor,{255,218,172}});
  pointLights.push_back({{2.12f,.44f,-10.65f},7.f,1.35f*lampFactor,{255,218,172}});
  pointLights.push_back({{-2.10f,.31f,-18.05f},6.f,1.1f*lampFactor,{255,218,172}});
  pointLights.push_back({{2.15f,.66f,-32.75f},7.5f,1.1f*lampFactor,{255,218,172}});
  pointLights.push_back({{-2.0f,.65f,-46.1f},7.f,.95f*lampFactor,{255,218,172}});
  pointLights.push_back({{2.1f,.68f,-58.6f},8.f,1.05f*lampFactor,{255,218,172}});
 }
}
}
