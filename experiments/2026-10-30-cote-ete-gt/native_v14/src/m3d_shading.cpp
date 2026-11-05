#include "m3d_engine.h"

namespace m3d {
ShadowMap buildShadow(const Scene&s,Stats&st){ShadowMap m;m.right=norm(cross({0,1,0},sun));m.up=cross(m.right,sun);m.d.resize(m.size*m.size);for(int y=0;y<m.size;y++){float vy=((y+.5f)/m.size-.5f)*2*m.span;for(int x=0;x<m.size;x++){float vx=((x+.5f)/m.size-.5f)*2*m.span;V o=m.base+sun*m.dist+m.right*vx+m.up*vy;Ray r{o,sun*-1};Hit h;closest(s,r,h,&st,false,-1,true);m.d[y*m.size+x]=h.t;}}m.min.assign(m.d.size(),INF);m.max.assign(m.d.size(),-INF);for(int y=0;y<m.size;y++)for(int x=0;x<m.size;x++){float lo=INF,hi=-INF;for(int j=-2;j<=2;j++)for(int i=-2;i<=2;i++){float v=m.d[std::clamp(y+j,0,m.size-1)*m.size+std::clamp(x+i,0,m.size-1)];lo=std::min(lo,v);hi=std::max(hi,v);}m.min[y*m.size+x]=lo;m.max[y*m.size+x]=hi;}return m;}
static float visibility(const ShadowMap&m,V p,Stats*st){V q=p-m.base;float fac=m.size/(2*m.span),center=m.size*.5f-.5f;int x=(int)std::lround(dot(q,m.right)*fac+center),y=(int)std::lround(dot(q,m.up)*fac+center);if(x<2||y<2||x>=m.size-2||y>=m.size-2)return 1;int ix=y*m.size+x;float td=m.dist-dot(q,sun),bias=.095f;if(st)st->depthReads++;if(td<=m.min[ix]+bias)return 1;if(st)st->depthReads++;if(td>m.max[ix]+bias)return .28f;float v=0;for(int j=-2;j<=2;j++)for(int i=-2;i<=2;i++){if(st)st->depthReads++;v+=td<=m.d[(y+j)*m.size+x+i]+bias?1.f:0.f;}return .16f+.84f*v/25;}
static V uvAt(const Scene&s,const Obj&o,V p,V n){if(o.kind==Kind::Triangle){const V e1=o.len2>0?o.center:o.b-o.a,e2=o.len2>0?o.core:o.c-o.a,q=p-o.a;const float d00=o.len2>0?o.uv0.z:dot(e1,e1),d01=o.len2>0?o.uv1.z:dot(e1,e2),d11=o.len2>0?o.uv2.z:dot(e2,e2),d20=dot(q,e1),d21=dot(q,e2),den=d00*d11-d01*d01,inv=o.len2>0?o.len2:1.f/den,b1=(d11*d20-d01*d21)*inv,b2=(d00*d21-d01*d20)*inv,b0=1-b1-b2;return{o.uv0.x*b0+o.uv1.x*b1+o.uv2.x*b2,o.uv0.y*b0+o.uv1.y*b1+o.uv2.y*b2,.8f};}if(o.kind==Kind::Ellipse){return{std::atan2((p.z-o.a.z)/o.b.z,(p.x-o.a.x)/o.b.x)/(2*PI)*2.5f,(.5f-std::asin(clampf((p.y-o.a.y)/o.b.y,-1,1))/PI)*2.5f,.9f};}if(o.kind==Kind::Capsule){V ba=o.b-o.a,q=p-o.a;float h=clampf(dot(q,ba)/o.len2,0,1);V radial=q-ba*h;return{std::atan2(radial.z,radial.x)/(2*PI),(h*std::sqrt(o.len2)+std::asin(clampf(radial.y/o.r,-1,1))*o.r)*.55f,.72f};}if(o.kind==Kind::Cone){V q=p-o.a;return{std::atan2(q.z,q.x)/(2*PI)*3,dot(q,o.n0)*.84f,.8f};}const std::string&mat=s.mat[o.mat].name;float scale=mat=="seatSteel"?.95f:(mat=="seatFabricClean"||mat=="seatTrim"||mat=="seatVinylBrown"||mat=="seatSupportPaint")?.85f:mat=="seatWood"?.65f:mat=="mural"?.075f:mat=="concrete"?.34f:(mat=="stone"||mat=="brick")?.28f:(mat=="glass"||mat=="raptureGlass"||mat=="raptureStone")?.16f:.65f,ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);if(ay>=ax&&ay>=az)return{p.x*scale,p.z*scale,scale};if(ax>=az)return{p.z*scale,p.y*scale,scale};return{p.x*scale,p.y*scale,scale};}
static V sampleMipChain(const std::vector<Level>& mip,float u,float v,float footprint,Stats&st){
 if(mip.empty())return{128,128,128};
 const float lod=clampf(std::log2(std::max(1.f,footprint)),0.f,(float)mip.size()-1);
 const size_t lo=(size_t)std::floor(lod),hi=std::min(lo+1,mip.size()-1);const float blend=lod-(float)lo;
 auto sampleLevel=[&](size_t level){
  const Level& l=mip[level];const int n=(int)l.size,mask=n-1;const float fxp=u*n-.5f,fyp=v*n-.5f;
  const int ix=(int)std::floor(fxp),iy=(int)std::floor(fyp);const float ax=fxp-ix,ay=fyp-iy;
  const int x0=ix&mask,y0=iy&mask,x1=(x0+1)&mask,y1=(y0+1)&mask;
  const uint16_t a=l.p[y0*n+x0],b=l.p[y0*n+x1],c=l.p[y1*n+x0],d=l.p[y1*n+x1];
  const float w0=(1-ax)*(1-ay),w1=ax*(1-ay),w2=(1-ax)*ay,w3=ax*ay;st.texels+=4;
  if(st.avx2){st.simdTextureSamples++;return sampleRgb565Avx2(a,b,c,d,w0,w1,w2,w3);}
  const float r=((a>>11)*w0+(b>>11)*w1+(c>>11)*w2+(d>>11)*w3)*255/31;
  const float g=(((a>>5&63)*w0+(b>>5&63)*w1+(c>>5&63)*w2+(d>>5&63)*w3)*255/63);
  const float bl=((a&31)*w0+(b&31)*w1+(c&31)*w2+(d&31)*w3)*255/31;
  return V{std::floor(r+.5f),std::floor(g+.5f),std::floor(bl+.5f)};
 };
 const V a=sampleLevel(lo);if(lo==hi)return a;const V b=sampleLevel(hi);return a*(1-blend)+b*blend;
}
static V sampleColorMipChain(const std::vector<ColorLevel>& mip,float u,float v,float footprint,Stats&st){
 if(mip.empty())return{128,128,128};
 const float lod=clampf(std::log2(std::max(1.f,footprint)),0.f,(float)mip.size()-1);
 const size_t lo=(size_t)std::floor(lod),hi=std::min(lo+1,mip.size()-1);const float blend=lod-(float)lo;
 auto sampleLevel=[&](size_t level){
  const ColorLevel& l=mip[level];const int n=(int)l.size,mask=n-1;const float fxp=u*n-.5f,fyp=v*n-.5f;
  const int ix=(int)std::floor(fxp),iy=(int)std::floor(fyp);const float ax=fxp-ix,ay=fyp-iy;
  const int x0=ix&mask,y0=iy&mask,x1=(x0+1)&mask,y1=(y0+1)&mask;
  const uint32_t q[4]={l.p[size_t(y0)*n+x0],l.p[size_t(y0)*n+x1],l.p[size_t(y1)*n+x0],l.p[size_t(y1)*n+x1]};
  const float w[4]={(1-ax)*(1-ay),ax*(1-ay),(1-ax)*ay,ax*ay};st.texels+=4;
  float r=0,g=0,b=0;for(int i=0;i<4;i++){r+=((q[i]>>16)&255)*w[i];g+=((q[i]>>8)&255)*w[i];b+=(q[i]&255)*w[i];}
  return V{std::floor(r+.5f),std::floor(g+.5f),std::floor(b+.5f)};
 };
 const V a=sampleLevel(lo);if(lo==hi)return a;const V b=sampleLevel(hi);return a*(1-blend)+b*blend;
}
static uint32_t albedoSize(const Material& m){return !m.colorMip.empty()?m.colorMip.front().size:(!m.mip.empty()?m.mip.front().size:512);}
static V texSample(const Material&m,float u,float v,float footprint,Stats&st){return !m.colorMip.empty()?sampleColorMipChain(m.colorMip,u,v,footprint,st):sampleMipChain(m.mip,u,v,footprint,st);}
static V sky(V d,bool fog,bool deepOcean=false){
 if(deepOcean){
  const float k=clampf(d.y*.5f+.5f,0,1);
  const float horizon=1.f-clampf(std::abs(d.y)*2.f,0,1);
  V c=V{4,15,27}*(1-k)+V{10,37,51}*k+V{10,28,36}*horizon;
  if(fog)c=c*.86f+V{13,52,62}*.14f;
  return c;
 }
 float k=clampf(d.y*.90f+.25f,0,1);V c=skyHorizon*(1-k)+skyZenith*k;float nearDist=std::max(0.f,dot(d,sun)),g=(std::pow(nearDist,92)*12+std::pow(nearDist,28)*2.2f)*daylight;c=c+V{g*sunTint.x/255.f,g*.82f*sunTint.y/255.f,g*.62f*sunTint.z/255.f};if(daylight>.05f&&nearDist>.99975f)c=sunTint;else if(daylight>.05f&&nearDist>.9985f)c=c+V{sunTint.x*.141f,sunTint.y*.123f,sunTint.z*.093f};if(fog&&d.y>-.08f){float w=std::sin(d.x*11.7f+d.z*4.2f)*std::sin(d.z*8.4f-d.y*15.1f)+.54f*std::sin(d.x*23.2f+d.y*10.4f)*std::cos(d.z*13.8f),cloud=clampf((w-.45f)*2.4f,0,1)*clampf((d.y+.16f)*3,0,1)*daylight;c=c*(1-cloud)+V{174,182,186}*cloud;}if(daylight<.18f){V moon=norm({-sun.x,.46f,-sun.z});float md=std::max(0.f,dot(d,moon)),a=std::pow(md,18)*.10f*(1-daylight),disc=clampf((md-.9994f)*1200.f,0,1)*(1-daylight);c=c+V{246,214,178}*(a+disc*.72f);}return c;
}
// Procedural bump mapping: perturb the shading normal without changing geometry.
static V bumpNormal(const std::string&mat,V p,V n,float pixelFootprint){
 float amp=0,f1=0,f2=0;
 if(mat=="asphalt"){amp=.0018f;f1=72.f;f2=125.f;}
 else if(mat=="sand"){amp=.0035f;f1=28.f;f2=48.f;}
 else if(mat=="concrete"){amp=.0009f;f1=68.f;f2=104.f;}
 else if(mat=="stone"){amp=.0060f;f1=32.f;f2=54.f;}
 else if(mat=="wood"){amp=.0007f;f1=90.f;f2=140.f;}
 else if(mat=="brick"){amp=.0012f;f1=58.f;f2=96.f;}
 else if(mat=="sea"){amp=.00055f;f1=160.f;f2=230.f;}
 else if(mat=="orange"){amp=.00115f;f1=54.f;f2=103.f;}
 else return n;
 const V d1=norm({.76f,.45f,.47f}),d2=norm({-.22f,.92f,.32f});
 const float a=dot(p,d1)*f1,b=dot(p,d2)*f2;
 V grad=d1*(amp*f1*.65f*std::cos(a))+d2*(amp*f2*.35f*std::cos(b));
 grad=grad-n*dot(grad,n);
 const float fade=clampf(1.f-pixelFootprint*std::max(f1,f2)/PI,0.f,1.f);
 return norm(n-grad*fade);
}
static V mappedNormal(const Material&m,const Obj&o,V n,float u,float v,float footprint,Stats&st){
 V t,b;
 if(o.kind==Kind::Triangle){
  V e1=o.len2>0?o.center:o.b-o.a,e2=o.len2>0?o.core:o.c-o.a;float du1=o.uv1.x-o.uv0.x,dv1=o.uv1.y-o.uv0.y,du2=o.uv2.x-o.uv0.x,dv2=o.uv2.y-o.uv0.y,det=du1*dv2-du2*dv1;
  if(std::abs(det)>1e-10f){t=(e1*dv2-e2*dv1)/det;V rawB=(e2*du1-e1*du2)/det;t=norm(t-n*dot(t,n));b=norm(cross(n,t))*(dot(cross(n,t),rawB)<0?-1.f:1.f);}
 }
 if(length(t)<1e-6f){V axis=std::abs(n.y)<.92f?V{0,1,0}:V{1,0,0};t=norm(cross(axis,n));b=norm(cross(n,t));}
 V tex=sampleMipChain(m.normalMip,u,v,footprint,st),q=norm({tex.x/127.5f-1.f,tex.y/127.5f-1.f,tex.z/127.5f-1.f});
 return norm(t*q.x+b*q.y+n*q.z);
}
V shade(const Scene&s,const ShadowMap&sm,const Camera&cam,const Ray&r,const Hit&h,Stats&st,bool fog,bool reflections){if(h.id==-1)return sky(r.d,fog,s.underwaterEnvironment);V p=h.p,n=h.n;int mi=h.id==-2?groundMaterial(s,p):h.mat;const std::string&mat=s.mat[mi].name;const bool carPaint=mat.rfind("sports",0)==0||mat=="carSilver"||mat=="carBlue"||mat=="carGreen"||mat=="classicBody";const bool raptureSurface=mat=="raptureStone"||mat=="raptureBronze"||mat=="raptureGlass";V uv;float footprint=1,worldFootprint=h.t*cam.pixelAngle;const Obj*o=h.id>=0?&s.obj[h.id]:nullptr;if(h.id==-2){uv={p.x*.34f,p.z*.34f,.34f};}else{uv=uvAt(s,*o,p,n);float slope=std::max(.16f,std::abs(dot(n,r.d)));worldFootprint=h.t*cam.pixelAngle/slope;footprint=float(albedoSize(s.mat[mi]))*worldFootprint*uv.z;}const Material&material=s.mat[mi];if(o&&!material.normalMip.empty())n=mappedNormal(material,*o,n,uv.x,uv.y,footprint,st);else n=bumpNormal(mat,p,n,worldFootprint);V base=texSample(material,uv.x,uv.y,footprint,st);float roughness=material.roughnessMip.empty()?.72f:clampf(sampleMipChain(material.roughnessMip,uv.x,uv.y,footprint,st).x/255.f,.08f,.98f);float ndotl=std::max(0.f,dot(n,sun)),vis=daylight>.001f?visibility(sm,p+n*.025f,&st):1.f,direct=ndotl*vis*daylight,diffuseLevel=mat=="foam"?.60f+.30f*daylight+.24f*direct:mat=="orange"?.34f+.43f*daylight+.26f*direct:carPaint?.32f+.42f*daylight+.32f*direct:raptureSurface?.42f+.28f*daylight+.50f*direct:.30f+.44f*daylight+.56f*direct;V c=base*diffuseLevel;float glow=mat=="led"?1.35f*lampFactor:mat=="taillight"?1.05f*lampFactor:mat=="neon"?1.1f:mat=="lamp"?.45f+1.05f*lampFactor:mat=="windowlit"?.88f:0;if(glow)c=c+base*glow;if(mat=="lamp")c=c+V{255,166,91}*(.75f+1.9f*lampFactor);if(mat=="led")c=c+V{255,217,165}*(.48f+.85f*lampFactor);if(mat=="taillight")c=c+V{255,28,12}*(1.25f*lampFactor);if(daylight<1.f)c=c+V{28.f,13.f,4.f}*(1-daylight);float spec=mat=="sea"?.46f:mat=="puddle"?.64f:mat=="glass"?.27f:(mat=="metal"||mat=="seatSteel")?.24f:mat=="carSilver"?.94f:carPaint?.68f:mat=="wheelMetal"?.34f:mat=="carGlass"?.52f:mat=="taxi"?.2f:mat=="mutant"?.23f:mat=="scar"?.28f:mat=="wound"?.19f:mat=="asphalt"?.16f:mat=="orange"?.045f+.16f*(1.f-roughness):(material.reflectivityPercent>0?.20f*material.reflectivityPercent/100.f:0.f);if(spec){V refl=n*(2*ndotl)-sun;float power=mat=="sea"?25:mat=="puddle"?18:mat=="asphalt"?13:mat=="mutant"?29:mat=="scar"?35:mat=="wound"?20:mat=="carSilver"?52:carPaint?30:mat=="wheelMetal"?42:mat=="carGlass"?38:mat=="orange"?42:34;if(!material.roughnessMip.empty())power=clampf(2.f/(roughness*roughness)-2.f,8.f,80.f);float shine=std::pow(std::max(0.f,-dot(r.d,refl)),power)*spec*vis*daylight;float moonSheen=mat=="sea"?std::pow(std::max(0.f,-dot(r.d,refl)),17)*.42f*(1-daylight):0;c=c+V{178,202,214}*shine+V{148,169,196}*moonSheen;}
 const float sceneLightScale=.42f+.58f*(1.f-daylight);
 if(st.avx2){st.simdLightBatches+=pointLights.size()/8+s.lights.size()/8;c=c+accumulatePointLightsAvx2(pointLights.data(),pointLights.size(),p,n,1.f);c=c+accumulatePointLightsAvx2(s.lights.data(),s.lights.size(),p,n,sceneLightScale);}
 auto applyPointLight=[&](const Light&l,float scale){float wet=mat=="puddle"?.48f:mat=="asphalt"?.25f:mat=="raptureStone"?.17f:mat=="raptureBronze"?.24f:carPaint?.74f:mat=="taxi"?.10f:mat=="concrete"?.12f:mat=="wood"?.18f:0;if(st.avx2&&!wet)return;V q=l.p-p;float d2=dot(q,q),r2=l.radius*l.radius;if(d2>=r2||d2<1e-5f)return;float d=std::sqrt(d2);V ld=q/d;float edge=1-d/l.radius,energy=edge*edge*l.intensity*scale,lam=std::max(0.f,dot(n,ld));if(!st.avx2)c=c+V{l.color.x,l.color.y,l.color.z}*(energy*lam*.27f);if(wet){V half=norm(ld-r.d);float sp=std::pow(std::max(0.f,dot(n,half)),mat=="asphalt"?18:carPaint?14:38)*energy*wet;c=c+V{l.color.x,l.color.y,l.color.z}*sp;}};
 for(const auto&l:pointLights)applyPointLight(l,1.f);for(const auto&l:s.lights)applyPointLight(l,sceneLightScale);
 if(reflections&&s.mat[mi].reflectivityPercent){V rd=norm(r.d-n*(2.f*dot(r.d,n)));float grazing=1.f-clampf(-dot(r.d,n),0.f,1.f);float profile=mat=="sea"?(.27f+.24f*grazing)/.51f:mat=="carSilver"?(.70f+.25f*std::pow(grazing,3))/.95f:(mat=="carGlass"||mat=="glass")?(.14f+.28f*grazing)/.42f:(.07f+.22f*std::pow(grazing,5))/.29f;float amount=(s.mat[mi].reflectivityPercent*.01f)*profile;V reflected{0,0,0};int taps=mat=="sea"?2:1;for(int k=0;k<taps;k++){V dir=rd;if(k){V axis=std::abs(rd.y)>.92f?V{1,0,0}:V{0,1,0},tangent=norm(cross(rd,axis)),bitangent=norm(cross(rd,tangent));dir=norm(rd+tangent*.045f+bitangent*.03f);}Ray rr{p+n*.035f,dir};Hit rh;closest(s,rr,rh,&st);st.rays++;if(rh.id==-1)st.sky++;else st.hits++;reflected=reflected+shade(s,sm,cam,rr,rh,st,fog,false);}reflected=reflected*(1.f/taps);if(mat=="carSilver")reflected={reflected.x*.84f,reflected.y*.84f,reflected.z*.84f};c=c*(1.f-amount)+reflected*amount;} if(fog){float f=1-std::exp(-h.t*.038f);c=c*(1-f)+skyHorizon*f;}return{clampf(c.x,0,255),clampf(c.y,0,255),clampf(c.z,0,255)};}

V traceColor(const Scene&s,const ShadowMap&sm,const Camera&cam,const Ray&input,Stats&st,bool fog){
 Ray ray=input;V result{0,0,0};float remaining=1.f;int ignore=-1;
 for(int layer=0;layer<8;layer++){
  Hit hit;closest(s,ray,hit,&st,true,ignore);st.rays++;
  if(hit.id==-1)st.sky++;else st.hits++;
  const uint8_t transmission=hit.id>=0?s.mat[hit.mat].transmissionPercent:0;
  if(!transmission){result=result+shade(s,sm,cam,ray,hit,st,fog,true)*remaining;return result;}

  const Material& material=s.mat[hit.mat];
  V normal=dot(ray.d,hit.n)<0?hit.n:hit.n*-1.f;
  const float cosine=clampf(-dot(ray.d,normal),0.f,1.f);
  const float ior=std::max(1.0001f,material.refractiveIndex);
  const float f0=(ior-1.f)/(ior+1.f),f02=f0*f0;
  const float fresnel=f02+(1.f-f02)*std::pow(1.f-cosine,5.f);
  const bool entering=dot(ray.d,hit.n)<0.f;
  const float eta=entering?1.f/ior:ior;
  const float sin2=std::max(0.f,1.f-cosine*cosine),k=1.f-eta*eta*sin2;
  const bool totalInternalReflection=k<=0.f;
  const float reflectedWeight=totalInternalReflection?1.f:fresnel;
  const float pass=totalInternalReflection?0.f:(transmission*.01f)*(1.f-fresnel);

  V surface=shade(s,sm,cam,ray,hit,st,fog,false);
  result=result+surface*(remaining*std::max(0.f,1.f-pass-reflectedWeight));
  if(reflectedWeight>0.f){
   Ray reflected{hit.p+normal*.035f,norm(ray.d-normal*(2.f*dot(ray.d,normal)))};
   Hit rh;closest(s,reflected,rh,&st,true,hit.id);st.rays++;
   if(rh.id==-1)st.sky++;else st.hits++;
   result=result+shade(s,sm,cam,reflected,rh,st,fog,false)*(remaining*reflectedWeight);
  }
  remaining*=pass;
  if(remaining<.008f)return result;
  V direction=ray.d;
  if(!totalInternalReflection)direction=norm(ray.d*eta+normal*(eta*cosine-std::sqrt(k)));
  ignore=hit.id;ray={hit.p+direction*.002f,direction};
 }
 Hit tail;closest(s,ray,tail,&st,true,ignore);st.rays++;if(tail.id==-1)st.sky++;else st.hits++;
 return result+shade(s,sm,cam,ray,tail,st,fog,true)*remaining;
}
static float srgbToLinear(float x){x=clampf(x,0.f,1.f);return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
static V rgbBytesToLinear(V c){return{srgbToLinear(c.x/255.f),srgbToLinear(c.y/255.f),srgbToLinear(c.z/255.f)};}
static V mul(V a,V b){return{a.x*b.x,a.y*b.y,a.z*b.z};}
static V oneMinus(V a){return{1.f-a.x,1.f-a.y,1.f-a.z};}
static float maxComponent(V c){return std::max(c.x,std::max(c.y,c.z));}
static float ggxDistribution(float nDotH,float roughness){
 const float a=std::max(.045f,roughness);const float a2=a*a*a*a;
 const float d=nDotH*nDotH*(a2-1.f)+1.f;return a2/(PI*d*d+1e-7f);
}
static float smithG1(float nDotV,float roughness){
 const float a=std::max(.045f,roughness),k=(a+1.f)*(a+1.f)*.125f;
 return nDotV/(nDotV*(1.f-k)+k+1e-7f);
}
static V fresnelSchlick(V f0,float vDotH){
 const float f=std::pow(1.f-clampf(vDotH,0.f,1.f),5.f);
 return f0+oneMinus(f0)*f;
}
static V offlinePbrShade(const Scene&s,const ShadowMap&sm,const Camera&cam,const Ray&r,const Hit&h,Stats&st,bool fog,float* roughOut=nullptr,float* metallicOut=nullptr,V* baseOut=nullptr,V* normalOut=nullptr){
 if(h.id==-1){const V env=sky(r.d,fog,s.underwaterEnvironment);return rgbBytesToLinear(env);}
 const int mi=h.id==-2?groundMaterial(s,h.p):h.mat;
 const Material& material=s.mat[mi];const std::string& name=material.name;
 const Obj* o=h.id>=0?&s.obj[h.id]:nullptr;V n=dot(h.n,r.d)>0.f?h.n*-1.f:h.n,uv;float footprint=1.f,worldFootprint=h.t*cam.pixelAngle;
 if(h.id==-2)uv={h.p.x*.34f,h.p.z*.34f,.34f};
 else{uv=uvAt(s,*o,h.p,n);const float slope=std::max(.16f,std::abs(dot(n,r.d)));worldFootprint=h.t*cam.pixelAngle/slope;footprint=float(albedoSize(material))*worldFootprint*uv.z;}
 if(o&&!material.normalMip.empty())n=mappedNormal(material,*o,n,uv.x,uv.y,footprint,st);else n=bumpNormal(name,h.p,n,worldFootprint);
 const V base=rgbBytesToLinear(texSample(material,uv.x,uv.y,footprint,st));
 const float roughness=material.roughnessMip.empty()?.72f:clampf(sampleMipChain(material.roughnessMip,uv.x,uv.y,footprint,st).x/255.f,.08f,.98f);
 // V4 defines reflectivity, transmission and IOR, but no metallic channel.
 // Preserve reflectivity as dielectric F0; never reinterpret it as metallicity.
 const float f0Level=material.reflectivityPercent?clampf(float(material.reflectivityPercent)*.01f,.02f,.95f):.04f;
 const float metallic=0.f;
 if(roughOut)*roughOut=roughness;
 if(metallicOut)*metallicOut=metallic;
 if(baseOut)*baseOut=base;
 if(normalOut)*normalOut=n;
 const V view=norm(r.d*-1.f);const float nv=std::max(1e-4f,dot(n,view));
 const V f0{f0Level,f0Level,f0Level};
 auto evaluate=[&](V l,V radiance,float visibilityFactor){const float nl=std::max(0.f,dot(n,l));if(nl<=0.f||visibilityFactor<=0.f)return V{0,0,0};const V half=norm(view+l);const float nh=std::max(0.f,dot(n,half)),vh=std::max(0.f,dot(view,half));const V F=fresnelSchlick(f0,vh);const float D=ggxDistribution(nh,roughness),G=smithG1(nv,roughness)*smithG1(nl,roughness);const V spec=F*(D*G/(4.f*nv*nl+1e-6f));const V kd=oneMinus(F);const V diff=mul(kd,base)*(1.f/PI);return mul(diff+spec,radiance)*(nl*visibilityFactor);};
 V result{0,0,0};
 const float sunVis=daylight>.001f?visibility(sm,h.p+n*.025f,&st):1.f;
 const V sunRadiance=rgbBytesToLinear(sunTint)*(2.8f*daylight);
 result=result+evaluate(norm(sun),sunRadiance,sunVis);
 const float sceneLightScale=.42f+.58f*(1.f-daylight);
 auto addPoint=[&](const Light& light,float scale){V delta=light.p-h.p;const float d2=dot(delta,delta),radius2=light.radius*light.radius;if(d2<1e-5f||d2>=radius2)return;const float d=std::sqrt(d2),edge=1.f-d/light.radius;const V radiance=rgbBytesToLinear(light.color)*(light.intensity*scale*edge*edge*.45f);result=result+evaluate(delta/d,radiance,1.f);};
 for(const Light& l:pointLights)addPoint(l,1.f);
 for(const Light& l:s.lights)addPoint(l,sceneLightScale);
 const V skyAverage=rgbBytesToLinear(skyHorizon*.5f+skyZenith*.5f);
 result=result+mul(base,skyAverage)*.16f;
 V emission{0,0,0};
 if(name=="lamp")emission=rgbBytesToLinear({255,166,91})*(.7f+2.f*lampFactor);
 else if(name=="led")emission=rgbBytesToLinear({255,217,165})*(.6f+lampFactor);
 else if(name=="taillight")emission=rgbBytesToLinear({255,28,12})*(1.2f*lampFactor);
 else if(name=="neon")emission=base*1.2f;
 else if(name=="windowlit")emission=base*.8f;
 result=result+emission;
 return result;
}
V traceOfflineColor(const Scene&s,const ShadowMap&sm,const Camera&cam,const Ray&input,Stats&st,bool fog,uint64_t seed,unsigned maxBounces){
 auto random01=[&](){seed+=0x9e3779b97f4a7c15ull;uint64_t z=seed;z=(z^(z>>30))*0xbf58476d1ce4e5b9ull;z=(z^(z>>27))*0x94d049bb133111ebull;z^=z>>31;return float((z>>40)&0xffffffu)/16777216.f;};
 maxBounces=std::clamp(maxBounces,1u,8u);
 V radiance{0,0,0},throughput{1,1,1};Ray ray=input;int ignore=-1;
 for(unsigned bounce=0;bounce<=maxBounces;bounce++){
  Hit hit;closest(s,ray,hit,&st,true,ignore);st.rays++;if(hit.id==-1)st.sky++;else st.hits++;
  if(hit.id>=0&&s.mat[hit.mat].transmissionPercent>0){const V transmitted=rgbBytesToLinear(traceColor(s,sm,cam,ray,st,fog));radiance=radiance+mul(throughput,transmitted);break;}
  float roughness=.72f,metallic=0.f;V base{1,1,1},n{0,1,0};
  const V direct=offlinePbrShade(s,sm,cam,ray,hit,st,fog,&roughness,&metallic,&base,&n);
  radiance=radiance+mul(throughput,direct);
  if(hit.id<0||bounce==maxBounces)break;
  const Material& bounceMaterial=hit.id>=0?s.mat[hit.mat]:s.mat[groundMaterial(s,hit.p)];
  const float f0Level=bounceMaterial.reflectivityPercent?clampf(float(bounceMaterial.reflectivityPercent)*.01f,.02f,.95f):.04f;
  const float specProbability=clampf(f0Level,.05f,.95f);
  V direction;
  if(random01()<specProbability){
   const V perfect=norm(ray.d-n*(2.f*dot(ray.d,n))),axis=std::abs(perfect.y)<.9f?V{0,1,0}:V{1,0,0};
   const V tangent=norm(cross(axis,perfect)),bitangent=norm(cross(perfect,tangent));const float u=random01(),v=random01(),rad=std::sqrt(u),phi=2.f*PI*v;
   const V jitter=tangent*(rad*std::cos(phi))+bitangent*(rad*std::sin(phi))+perfect*std::sqrt(std::max(0.f,1.f-u));
   direction=norm(perfect*(1.f-roughness*roughness)+jitter*(roughness*roughness));
   if(dot(direction,n)<=.001f)direction=perfect;
   const V f0{f0Level,f0Level,f0Level};
   throughput=mul(throughput,f0)/specProbability;
  }else{
   const V axis=std::abs(n.y)<.9f?V{0,1,0}:V{1,0,0},tangent=norm(cross(axis,n)),bitangent=norm(cross(n,tangent));
   const float u=random01(),v=random01(),rad=std::sqrt(u),phi=2.f*PI*v;
   direction=norm(tangent*(rad*std::cos(phi))+bitangent*(rad*std::sin(phi))+n*std::sqrt(std::max(0.f,1.f-u)));
   throughput=mul(throughput,base)*(1.f/(1.f-specProbability));
  }
  if(bounce>=2){const float survive=clampf(maxComponent(throughput),.12f,.95f);if(random01()>survive)break;throughput=throughput/survive;}
  ignore=hit.id;ray={hit.p+n*.025f,direction};
 }
 return radiance;
}
}
