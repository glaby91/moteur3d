#pragma once
// CSE WARP Weather - UTC astronomical clock, no external dependencies.
// Approximate solar and lunar ephemerides for visual simulation.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace cse::weather {
constexpr double pi=3.14159265358979323846;
constexpr double rad=pi/180.0;
inline double wrap(double v,double p){v=std::fmod(v,p);return v<0?v+p:v;}
inline double clamp(double v,double lo,double hi){return std::max(lo,std::min(v,hi));}
struct Direction {double east=0,up=0,north=0,azimuthDeg=0,altitudeDeg=0;};
struct SkyState {Direction sun,moon;double moonIllumination=0,daylight=0,starVisibility=0,siderealDeg=0;};
inline Direction horizontal(double ra,double dec,double jd,double lat,double lon) {
 const double T=(jd-2451545.0)/36525.;
 const double sid=wrap(280.46061837+360.98564736629*(jd-2451545.0)+0.000387933*T*T-T*T*T/38710000.+lon,360)*rad;
 const double H=std::remainder(sid-ra,2*pi),phi=lat*rad;
 Direction a;
 a.east=-std::cos(dec)*std::sin(H);
 a.north=std::sin(dec)*std::cos(phi)-std::cos(dec)*std::cos(H)*std::sin(phi);
 a.up=std::sin(dec)*std::sin(phi)+std::cos(dec)*std::cos(H)*std::cos(phi);
 a.altitudeDeg=std::asin(clamp(a.up,-1,1))/rad;
 a.azimuthDeg=wrap(std::atan2(a.east,a.north)/rad,360);
 return a;
}
inline double smooth(double a,double b,double v){double t=clamp((v-a)/(b-a),0,1);return t*t*(3-2*t);}
inline SkyState calculate(std::int64_t utcSeconds,double latDeg,double lonDeg,double lightPollution=.65) {
 const double jd=2440587.5+double(utcSeconds)/86400.,d=jd-2451545.;
 const double eps=(23.4393-.00000036*d)*rad;
 const double L=wrap(280.460+.9856474*d,360)*rad,g=wrap(357.528+.9856003*d,360)*rad;
 const double sl=L+(1.915*std::sin(g)+.020*std::sin(2*g))*rad;
 const double sra=std::atan2(std::cos(eps)*std::sin(sl),std::cos(sl));
 const double sdec=std::asin(std::sin(eps)*std::sin(sl));
 const double N=wrap(125.1228-.0529538083*d,360)*rad;
 const double inc=5.1454*rad,w=wrap(318.0634+.1643573223*d,360)*rad;
 const double e=.054900,M=wrap(115.3654+13.0649929509*d,360)*rad;
 const double E=M+e*std::sin(M)*(1+e*std::cos(M));
 const double v=std::atan2(std::sqrt(1-e*e)*std::sin(E),std::cos(E)-e),u=v+w;
 const double mx=std::cos(N)*std::cos(u)-std::sin(N)*std::sin(u)*std::cos(inc);
 const double my=std::sin(N)*std::cos(u)+std::cos(N)*std::sin(u)*std::cos(inc);
 const double mz=std::sin(u)*std::sin(inc);
 const double mra=std::atan2(my*std::cos(eps)-mz*std::sin(eps),mx);
 const double mdec=std::atan2(my*std::sin(eps)+mz*std::cos(eps),std::hypot(mx,my));
 SkyState state;
 state.sun=horizontal(sra,sdec,jd,latDeg,lonDeg);
 state.moon=horizontal(mra,mdec,jd,latDeg,lonDeg);
 const double cosElong=clamp(std::sin(sdec)*std::sin(mdec)+std::cos(sdec)*std::cos(mdec)*std::cos(sra-mra),-1,1);
 state.moonIllumination=(1-cosElong)*.5;
 state.daylight=smooth(-12,0,state.sun.altitudeDeg);
 state.starVisibility=(1-smooth(-16,-5,state.sun.altitudeDeg))*(1-clamp(lightPollution,0,1)*.88);
 const double T=(jd-2451545.0)/36525.;
 state.siderealDeg=wrap(280.46061837+360.98564736629*(jd-2451545.0)+.000387933*T*T-T*T*T/38710000.+lonDeg,360);
 return state;
}
}
