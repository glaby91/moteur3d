#include "m3d_engine.h"
namespace m3d {
// Draft 12beta4: CPU tile raster visibility for triangles, BVH fallback for
// mixed primitives. Keeps the existing material, lighting and shadow shaders.
Stats renderHybrid(const Scene& s,const ShadowMap& sm,const Camera& cam,int w,int h,
                   std::vector<uint32_t>& pixels,unsigned threads,bool fog,
                   const std::atomic<uint64_t>& generation,uint64_t token){
 auto start=std::chrono::steady_clock::now();
 pixels.assign(size_t(w)*h,0xff000000u);
 struct Vertex {float x,y,z;};
 struct Tri {Vertex v[3];uint32_t id;float area;int x0,y0,x1,y1;};
 std::vector<Tri> triangles;
 bool mixed=false;
 for(uint32_t id=0;id<s.obj.size();++id){
  const Obj& o=s.obj[id];
  if(o.kind!=Kind::Triangle || (o.mat<s.mat.size() && s.mat[o.mat].transmissionPercent)){mixed=true;continue;}
  V pts[3]={o.a,o.b,o.c};Tri t{};t.id=id;bool nearClip=false;
  for(int i=0;i<3;++i){
   V q=pts[i]-cam.eye;float z=dot(q,cam.f);
   if(z<=0.05f){nearClip=true;break;}
   t.v[i]={w*(.5f+.5f*dot(q,cam.r)/(z*cam.tanHalf)),
           h*(.5f-.5f*dot(q,cam.u)*cam.aspect/(z*cam.tanHalf)),z};
  }
  if(nearClip){mixed=true;continue;}
  auto edge=[](const Vertex&a,const Vertex&b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
  t.area=edge(t.v[0],t.v[1],t.v[2].x,t.v[2].y);
  if(std::abs(t.area)<1e-7f)continue;
  t.x0=std::max(0,int(std::floor(std::min({t.v[0].x,t.v[1].x,t.v[2].x}))));
  t.y0=std::max(0,int(std::floor(std::min({t.v[0].y,t.v[1].y,t.v[2].y}))));
  t.x1=std::min(w,int(std::ceil(std::max({t.v[0].x,t.v[1].x,t.v[2].x}))));
  t.y1=std::min(h,int(std::ceil(std::max({t.v[0].y,t.v[1].y,t.v[2].y}))));
  if(t.x0<t.x1&&t.y0<t.y1)triangles.push_back(t);
 }
 const int tileSize=16,nx=(w+tileSize-1)/tileSize,ny=(h+tileSize-1)/tileSize;
 std::vector<std::vector<uint32_t>> bins(size_t(nx)*ny);
 for(uint32_t i=0;i<triangles.size();++i){const Tri&t=triangles[i];
  for(int ty=t.y0/tileSize;ty<=(t.y1-1)/tileSize;++ty)
   for(int tx=t.x0/tileSize;tx<=(t.x1-1)/tileSize;++tx)bins[size_t(ty)*nx+tx].push_back(i);
 }
 std::atomic<int> next{0};std::vector<Stats> perThread(std::max(1u,threads));
 auto worker=[&](unsigned workerId){
  Stats&st=perThread[workerId];std::vector<float> zbuf(256);std::vector<int> ids(256);
  auto edge=[](const Vertex&a,const Vertex&b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
  while(generation.load()==token){
   int tile=next.fetch_add(1);if(tile>=nx*ny)break;
   int bx=(tile%nx)*tileSize,by=(tile/nx)*tileSize;
   std::fill(zbuf.begin(),zbuf.end(),INF);std::fill(ids.begin(),ids.end(),-1);
   for(uint32_t idx:bins[tile]){const Tri&t=triangles[idx];
    int minx=std::max(bx,t.x0),maxx=std::min(bx+tileSize,t.x1);
    int miny=std::max(by,t.y0),maxy=std::min(by+tileSize,t.y1);
    for(int y=miny;y<maxy;++y)for(int x=minx;x<maxx;++x){
     float px=x+.5f,py=y+.5f;
     float a=edge(t.v[1],t.v[2],px,py)/t.area;
     float b=edge(t.v[2],t.v[0],px,py)/t.area;
     float c=1.f-a-b;
     if(a<0||b<0||c<0)continue;
     float invz=a/t.v[0].z+b/t.v[1].z+c/t.v[2].z;
     if(invz<=0)continue;
     float z=1.f/invz;int at=(y-by)*tileSize+x-bx;
     if(z<zbuf[at]){zbuf[at]=z;ids[at]=int(t.id);}
    }
   }
   for(int y=by;y<std::min(by+tileSize,h);++y)for(int x=bx;x<std::min(bx+tileSize,w);++x){
    if(generation.load()!=token)return;
    int at=(y-by)*tileSize+x-bx;
    float rx=(2.f*(x+.5f)/w-1.f)*cam.tanHalf;
    float ry=(1.f-2.f*(y+.5f)/h)*cam.tanHalf/cam.aspect;
    Ray ray{cam.eye,norm(cam.f+cam.r*rx+cam.u*ry)};
    Hit hit;
    if(mixed||ids[at]<0){closest(s,ray,hit,&st,true); }
    else {
     float t=zbuf[at]/std::max(1e-8f,dot(ray.d,cam.f));
     hit.t=t;hit.p=ray.o+ray.d*t;hit.id=ids[at];hit.mat=s.obj[hit.id].mat;
     const Obj&o=s.obj[hit.id];V e1=o.b-o.a,e2=o.c-o.a,q=hit.p-o.a;
     float d00=dot(e1,e1),d01=dot(e1,e2),d11=dot(e2,e2),d20=dot(q,e1),d21=dot(q,e2);
     float den=d00*d11-d01*d01;
     float v=std::abs(den)>1e-18f?(d11*d20-d01*d21)/den:0;
     float u=std::abs(den)>1e-18f?(d00*d21-d01*d20)/den:0;
     hit.n=norm(o.n0*(1-v-u)+o.n1*v+o.n2*u);
    }
    // Ray-based secondary effects stay in the original shader.
    V color=shade(s,sm,cam,ray,hit,st,fog);
    auto byte=[](float v)->uint32_t{return uint32_t(clampf(v,0.f,255.f));};
    pixels[size_t(y)*w+x]=0xff000000u|(byte(color.x)<<16)|(byte(color.y)<<8)|byte(color.z);
    ++st.rays;if(hit.id>=0)++st.hits;else if(hit.id==-1)++st.sky;
   }
  }
 };
 threads=std::max(1u,std::min(threads,unsigned(nx*ny)));
 perThread.resize(threads);std::vector<std::thread> pool;
 for(unsigned i=0;i<threads;++i)pool.emplace_back(worker,i);
 for(auto& t:pool)t.join();
 Stats out;out.threads=threads;
 for(const Stats&st:perThread){out.rays+=st.rays;out.tests+=st.tests;out.aabb+=st.aabb;out.nodes+=st.nodes;out.hits+=st.hits;out.sky+=st.sky;out.texels+=st.texels;out.depthReads+=st.depthReads;out.shadowRays+=st.shadowRays;}
 out.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 return out;
}
}
