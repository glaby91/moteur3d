#include "m3d_engine.h"

namespace m3d {
static bool readBytes(std::ifstream& f,void* p,size_t n){return bool(f.read((char*)p,(std::streamsize)n));}
static bool readU8(std::ifstream& f,uint8_t& v){return readBytes(f,&v,1);}
static bool readU16(std::ifstream& f,uint16_t& v){uint8_t b[2];if(!readBytes(f,b,2))return false;v=uint16_t(b[0]|(b[1]<<8));return true;}
static bool readU32(std::ifstream& f,uint32_t& v){uint8_t b[4];if(!readBytes(f,b,4))return false;v=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true;}
static bool readI32(std::ifstream& f,int32_t& v){uint32_t q;if(!readU32(f,q))return false;v=(int32_t)q;return true;}
static bool readF(std::ifstream& f,float& v){uint32_t q;if(!readU32(f,q))return false;std::memcpy(&v,&q,4);return std::isfinite(v);}
static bool readV(std::ifstream& f,V& v){return readF(f,v.x)&&readF(f,v.y)&&readF(f,v.z);}
static bool validBounds(V a,V b){return a.x<=b.x&&a.y<=b.y&&a.z<=b.z;}
static bool fail(std::wstring& e,const wchar_t* m){e=m;return false;}
bool loadScene(const std::filesystem::path& path,Scene& s,std::wstring& error){
 error.clear();
 try{
  std::ifstream f(path,std::ios::binary);
  if(!f)return fail(error,L"Fichier moteur3d_v1314_scene.bin introuvable à côté du programme.");
  char magic[8]{};uint32_t version=0,no=0,nn=0,nord=0,nm=0,nt=0,texSize=0;int32_t root=-1;
  if(!readBytes(f,magic,8)||std::memcmp(magic,"M3D1314",7)||magic[7]!='\0'||!readU32(f,version)||!readU32(f,no)||!readU32(f,nn)||!readI32(f,root)||!readU32(f,nord)||!readU32(f,nm)||!readU32(f,nt)||!readU32(f,texSize))
   return fail(error,L"En-tête de scène absent ou tronqué.");
  if((version<3||version>5)||!no||no>4200000||!nn||nn>4200000||nord!=no||!nm||nm>256||nt!=nm||texSize<512||texSize>2048||(texSize&(texSize-1))!=0||root<0||(uint32_t)root>=nn)
   return fail(error,L"En-tête de scène V13.14 invalide (version, quantités ou racine BVH).");
  constexpr uint64_t maxSceneBytes=2048ull*1024ull*1024ull;
  uint64_t estimatedBytes=uint64_t(no)*sizeof(Obj)+uint64_t(nn)*sizeof(Node)+uint64_t(nord)*sizeof(uint32_t)+uint64_t(nm)*sizeof(Material);
  if(estimatedBytes>maxSceneBytes)return fail(error,L"La géométrie de la scène dépasse la limite de mémoire de sécurité (2 Gio).");
  auto reserveTexture=[&](uint32_t size,uint32_t bytesPerTexel=2){const uint64_t bytes=uint64_t(size)*size*bytesPerTexel;if(bytes>maxSceneBytes-estimatedBytes)return false;estimatedBytes+=bytes;return true;};
  Scene tmp;tmp.root=root;tmp.obj.resize(no);tmp.node.resize(nn);tmp.order.resize(nord);tmp.mat.resize(nm);
  for(auto& m:tmp.mat){char name[24]{};if(!readBytes(f,name,sizeof(name)))return fail(error,L"Table de matériaux tronquée.");size_t n=0;while(n<sizeof(name)&&name[n])++n;m.name.assign(name,n);uint16_t iorMilli=0;if(!readU8(f,m.reflectivityPercent)||!readU8(f,m.transmissionPercent)||!readU16(f,iorMilli)||m.reflectivityPercent>100||m.transmissionPercent>100||(iorMilli&& (iorMilli<1000||iorMilli>3000)))return fail(error,L"Paramètres optiques du matériau invalides ou tronqués.");if(iorMilli)m.refractiveIndex=iorMilli*.001f;}
  for(auto& o:tmp.obj){
   uint8_t kind=0,mi=0;uint16_t reserved=0;
   if(!readU8(f,kind)||!readU8(f,mi)||!readU16(f,reserved)||kind>5||mi>=nm||!readV(f,o.mn)||!readV(f,o.mx)||!validBounds(o.mn,o.mx))return fail(error,L"En-tête d’objet invalide ou géométrie tronquée.");
   o.kind=(Kind)kind;o.mat=mi;
   if(kind==0){if(!readV(f,o.a)||!readV(f,o.b)||!readV(f,o.c)||!readV(f,o.n0)||!readV(f,o.n1)||!readV(f,o.n2)||!readF(f,o.uv0.x)||!readF(f,o.uv0.y)||!readF(f,o.uv1.x)||!readF(f,o.uv1.y)||!readF(f,o.uv2.x)||!readF(f,o.uv2.y))return fail(error,L"Triangle incomplet ou non fini.");o.center=o.b-o.a;o.core=o.c-o.a;o.uv0.z=dot(o.center,o.center);o.uv1.z=dot(o.center,o.core);o.uv2.z=dot(o.core,o.core);const float denom=o.uv0.z*o.uv2.z-o.uv1.z*o.uv1.z;o.len2=std::abs(denom)>1e-20f?1.f/denom:0.f;}
   else if(kind==2){if(!readF(f,o.r)||!readV(f,o.center)||!readV(f,o.core)||o.r<0||o.core.x<0||o.core.y<0||o.core.z<0)return fail(error,L"Boîte arrondie invalide ou tronquée.");}
   else if(kind==3){if(!readV(f,o.a)||!readV(f,o.b)||!readF(f,o.r)||!readF(f,o.len2)||o.r<0||o.len2<=0)return fail(error,L"Capsule invalide ou tronquée.");}
   else if(kind==4){if(!readV(f,o.a)||!readV(f,o.b)||!readF(f,o.r)||!readF(f,o.r1)||!readF(f,o.k)||!readF(f,o.len)||!readF(f,o.n0.x)||!readF(f,o.n0.y)||!readF(f,o.n0.z)||o.r<0||o.r1<0||o.len<=0)return fail(error,L"Cône invalide ou tronqué.");}
   else if(kind==5){if(!readV(f,o.a)||!readF(f,o.b.x)||!readF(f,o.b.y)||!readF(f,o.b.z)||o.b.x<=0||o.b.y<=0||o.b.z<=0)return fail(error,L"Ellipsoïde invalide ou tronqué.");}
  }
  for(auto& n:tmp.node)if(!readV(f,n.mn)||!readV(f,n.mx)||!validBounds(n.mn,n.mx)||!readI32(f,n.left)||!readI32(f,n.right)||!readI32(f,n.start)||!readI32(f,n.count)||n.count<0||n.start<0)return fail(error,L"Nœud BVH invalide ou tronqué.");
  for(auto& i:tmp.order)if(!readU32(f,i)||i>=no)return fail(error,L"Ordre BVH tronqué ou indice hors limites.");
  std::vector<uint8_t> seenObjects(no,0),seenNodes(nn,0),seenOrder(nord,0);std::vector<int32_t> stack{root};
  for(uint32_t i:tmp.order)if(seenObjects[i]++)return fail(error,L"Un objet apparaît plusieurs fois dans l’ordre BVH.");
  while(!stack.empty()){
   int32_t idx=stack.back();stack.pop_back();if(idx<0||(uint32_t)idx>=nn||seenNodes[idx]++)return fail(error,L"Le graphe BVH contient un lien invalide, un cycle ou un nœud partagé.");
   const Node& n=tmp.node[idx];
   if(n.count>0){if(n.left!=-1||n.right!=-1||(uint64_t)n.start+(uint64_t)n.count>nord)return fail(error,L"Plage de feuille BVH invalide.");for(int32_t k=0;k<n.count;k++)if(seenOrder[n.start+k]++)return fail(error,L"Deux feuilles BVH se chevauchent.");}
   else{if(n.left<0||n.right<0||(uint32_t)n.left>=nn||(uint32_t)n.right>=nn||n.left==n.right)return fail(error,L"Liens de nœud BVH invalides.");stack.push_back(n.right);stack.push_back(n.left);}
  }
  if(std::find(seenNodes.begin(),seenNodes.end(),0)!=seenNodes.end()||std::find(seenObjects.begin(),seenObjects.end(),0)!=seenObjects.end()||std::find(seenOrder.begin(),seenOrder.end(),0)!=seenOrder.end())return fail(error,L"Le BVH ne couvre pas tous les nœuds et objets.");
  auto encloses=[](V outerMin,V outerMax,V innerMin,V innerMax){constexpr float e=.002f;return outerMin.x<=innerMin.x+e&&outerMin.y<=innerMin.y+e&&outerMin.z<=innerMin.z+e&&outerMax.x+e>=innerMax.x&&outerMax.y+e>=innerMax.y&&outerMax.z+e>=innerMax.z;};
  for(const Node& n:tmp.node){
   if(n.count>0){for(int32_t k=0;k<n.count;k++){const Obj& o=tmp.obj[tmp.order[n.start+k]];if(!encloses(n.mn,n.mx,o.mn,o.mx))return fail(error,L"Une feuille BVH n’inclut pas les bornes d’un objet.");}}
   else if(!encloses(n.mn,n.mx,tmp.node[n.left].mn,tmp.node[n.left].mx)||!encloses(n.mn,n.mx,tmp.node[n.right].mn,tmp.node[n.right].mx))return fail(error,L"Un nœud BVH n’inclut pas les bornes de ses enfants.");
  }
  for(auto& n:tmp.node)if(!n.count){
   const Node& l=tmp.node[n.left];const Node& r=tmp.node[n.right];
   if(l.count<=0||r.count<=0||l.count+r.count!=8)continue;
   bool triangles=true;
   for(int32_t k=0;k<l.count&&triangles;k++)triangles=tmp.obj[tmp.order[l.start+k]].kind==Kind::Triangle;
   for(int32_t k=0;k<r.count&&triangles;k++)triangles=tmp.obj[tmp.order[r.start+k]].kind==Kind::Triangle;
   n.simdTrianglePair=triangles;
  }
  for(auto& m:tmp.mat){uint32_t levels=0;if(!readU32(f,levels)||levels<7||levels>12)return fail(error,L"Pyramide de texture invalide.");uint32_t expected=0;
   if(version>=5){m.colorMip.resize(levels);for(size_t i=0;i<m.colorMip.size();i++){auto& l=m.colorMip[i];uint32_t size=0;if(!readU32(f,size)||(i==0&&(size<512||size>texSize||(size&(size-1))!=0))||(i>0&&size!=expected)||!reserveTexture(size,4))return fail(error,L"Taille de niveau mip RGB888 invalide ou limite mémoire de texture dépassée.");l.size=size;l.p.resize(size_t(size)*size);for(auto& v:l.p)if(!readU32(f,v))return fail(error,L"Texture RGB888 tronquée.");expected=size>>1;}}
   else{m.mip.resize(levels);for(size_t i=0;i<m.mip.size();i++){auto& l=m.mip[i];uint32_t size=0;if(!readU32(f,size)||(i==0&&(size<512||size>texSize||(size&(size-1))!=0))||(i>0&&size!=expected)||!reserveTexture(size))return fail(error,L"Taille de niveau mip invalide ou limite mémoire de texture dépassée.");l.size=size;l.p.resize(size_t(size)*size);for(auto& v:l.p)if(!readU16(f,v))return fail(error,L"Texture RGB565 tronquée.");expected=size>>1;}}
   const uint32_t finalSize=version>=5?m.colorMip.back().size:m.mip.back().size;
   if(finalSize!=8&&finalSize!=1)return fail(error,L"La pyramide de texture doit se terminer à 8 px ou 1 px.");
  }
  auto readMap=[&](std::vector<Level>& chain){uint32_t levels=0;if(!readU32(f,levels)||levels>12)return false;if(levels==0)return true;if(levels<7)return false;chain.resize(levels);uint32_t expected=0;for(size_t i=0;i<chain.size();i++){auto& l=chain[i];uint32_t size=0;if(!readU32(f,size)||(i==0&&(size<512||size>texSize||(size&(size-1))!=0))||(i>0&&size!=expected)||!reserveTexture(size))return false;l.size=size;l.p.resize(size_t(size)*size);for(auto& v:l.p)if(!readU16(f,v))return false;expected=size>>1;}return chain.back().size==8||chain.back().size==1;};
  for(auto& m:tmp.mat)if(!readMap(m.normalMip)||!readMap(m.roughnessMip))return fail(error,L"Cartes PBR absentes, tronquées ou de taille invalide.");
  if(version>=4){uint32_t lightCount=0;if(!readU32(f,lightCount)||lightCount>256)return fail(error,L"Table de lumières invalide ou tronquée.");tmp.lights.resize(lightCount);for(auto& l:tmp.lights){if(!readV(f,l.p)||!readF(f,l.radius)||!readF(f,l.intensity)||!readV(f,l.color)||l.radius<=0||l.radius>10000||l.intensity<0||l.intensity>10000||l.color.x<0||l.color.y<0||l.color.z<0||l.color.x>255||l.color.y>255||l.color.z>255)return fail(error,L"Paramètre d’une lumière de scène invalide ou tronqué.");}}
  if(f.peek()!=std::char_traits<char>::eof())return fail(error,L"Le fichier contient des données supplémentaires après la scène.");
  tmp.underwaterEnvironment=std::any_of(tmp.mat.begin(),tmp.mat.end(),[](const Material& m){return m.name=="raptureGlass";});
  s=std::move(tmp);return true;
 }catch(const std::bad_alloc&){return fail(error,L"Mémoire insuffisante pour charger la scène.");}
 catch(const std::exception&){return fail(error,L"Erreur inattendue pendant le chargement de la scène.");}
}
}
