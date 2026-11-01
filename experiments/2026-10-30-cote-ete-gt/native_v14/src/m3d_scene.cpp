#include "m3d_engine.h"

namespace m3d {
static bool readBytes(std::ifstream& f,void* p,size_t n){return bool(f.read((char*)p,(std::streamsize)n));}
static bool readU8(std::ifstream& f,uint8_t& v){return readBytes(f,&v,1);}
static bool readU16(std::ifstream& f,uint16_t& v){uint8_t b[2];if(!readBytes(f,b,2))return false;v=uint16_t(b[0]|(b[1]<<8));return true;}
static bool readU32(std::ifstream& f,uint32_t& v){uint8_t b[4];if(!readBytes(f,b,4))return false;v=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true;}
static bool readI32(std::ifstream& f,int32_t& v){uint32_t q;if(!readU32(f,q))return false;v=(int32_t)q;return true;}
static bool readF(std::ifstream& f,float& v){uint32_t q;if(!readU32(f,q))return false;std::memcpy(&v,&q,4);return std::isfinite(v);}
static uint8_t defaultReflectivityPercent(const std::string& name){if(name=="sea")return 51;if(name=="glass"||name=="carGlass")return 42;if(name=="metal")return 35;if(name=="wheelMetal")return 55;if(name=="carSilver")return 95;if(name=="classicBody"||name=="carBlue"||name=="carGreen"||name.rfind("sports",0)==0)return 29;return 0;}
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
  if((version!=1&&version!=2)||!no||no>4200000||!nn||nn>4200000||nord!=no||!nm||nm>256||nt!=nm||texSize!=512||root<0||(uint32_t)root>=nn)
   return fail(error,L"En-tête de scène V13.14 invalide (version, quantités ou racine BVH).");
  Scene tmp;tmp.root=root;tmp.obj.resize(no);tmp.node.resize(nn);tmp.order.resize(nord);tmp.mat.resize(nm);
  for(auto& m:tmp.mat){char name[24]{};if(!readBytes(f,name,sizeof(name)))return fail(error,L"Table de matériaux tronquée.");size_t n=0;while(n<sizeof(name)&&name[n])++n;m.name.assign(name,n);if(version==2){uint8_t reserved8=0;uint16_t reserved16=0;if(!readU8(f,m.reflectivityPercent)||!readU8(f,reserved8)||!readU16(f,reserved16)||m.reflectivityPercent>100)return fail(error,L"Pourcentage de réflexion matériau invalide ou tronqué.");}else m.reflectivityPercent=defaultReflectivityPercent(m.name);}
  for(auto& o:tmp.obj){
   uint8_t kind=0,mi=0;uint16_t reserved=0;
   if(!readU8(f,kind)||!readU8(f,mi)||!readU16(f,reserved)||kind>5||mi>=nm||!readV(f,o.mn)||!readV(f,o.mx)||!validBounds(o.mn,o.mx))return fail(error,L"En-tête d’objet invalide ou géométrie tronquée.");
   o.kind=(Kind)kind;o.mat=mi;
   if(kind==0){if(!readV(f,o.a)||!readV(f,o.b)||!readV(f,o.c)||!readV(f,o.n0)||!readV(f,o.n1)||!readV(f,o.n2)||!readF(f,o.uv0.x)||!readF(f,o.uv0.y)||!readF(f,o.uv1.x)||!readF(f,o.uv1.y)||!readF(f,o.uv2.x)||!readF(f,o.uv2.y))return fail(error,L"Triangle incomplet ou non fini.");o.uv0.z=o.uv1.z=o.uv2.z=0;}
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
  for(auto& m:tmp.mat){uint32_t levels=0;if(!readU32(f,levels)||levels!=7)return fail(error,L"Pyramide de texture invalide (7 niveaux attendus).");m.mip.resize(levels);uint32_t expected=512;
   for(auto& l:m.mip){uint32_t size=0;if(!readU32(f,size)||size!=expected)return fail(error,L"Taille de niveau mip invalide.");l.size=size;l.p.resize(size_t(size)*size);for(auto& v:l.p)if(!readU16(f,v))return fail(error,L"Texture RGB565 tronquée.");expected>>=1;}
  }
  if(f.peek()!=std::char_traits<char>::eof())return fail(error,L"Le fichier contient des données supplémentaires après la scène.");
  s=std::move(tmp);return true;
 }catch(const std::bad_alloc&){return fail(error,L"Mémoire insuffisante pour charger la scène.");}
 catch(const std::exception&){return fail(error,L"Erreur inattendue pendant le chargement de la scène.");}
}
}
