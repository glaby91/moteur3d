#include "m3d_engine.h"

namespace m3d {
static bool readBytes(std::ifstream&f,void*p,size_t n){return bool(f.read((char*)p,(std::streamsize)n));}
static bool readU8(std::ifstream&f,uint8_t&v){return readBytes(f,&v,1);} static bool readU16(std::ifstream&f,uint16_t&v){uint8_t b[2];if(!readBytes(f,b,2))return false;v=uint16_t(b[0]|(b[1]<<8));return true;} static bool readU32(std::ifstream&f,uint32_t&v){uint8_t b[4];if(!readBytes(f,b,4))return false;v=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true;} static bool readI32(std::ifstream&f,int32_t&v){uint32_t q;if(!readU32(f,q))return false;v=(int32_t)q;return true;} static bool readF(std::ifstream&f,float&v){uint32_t q;if(!readU32(f,q))return false;std::memcpy(&v,&q,4);return true;} static bool readV(std::ifstream&f,V&v){return readF(f,v.x)&&readF(f,v.y)&&readF(f,v.z);}
bool loadScene(const wchar_t*path,Scene&s,std::wstring&error){
 std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f){error=L"Fichier moteur3d_v1314_scene.bin introuvable à côté du programme.";return false;}
 char magic[8]{};uint32_t version=0,no=0,nn=0,nord=0,nm=0,nt=0,texSize=0;int32_t root=-1;
 if(!readBytes(f,magic,8)||std::memcmp(magic,"M3D1314",7)||!readU32(f,version)||!readU32(f,no)||!readU32(f,nn)||!readI32(f,root)||!readU32(f,nord)||!readU32(f,nm)||!readU32(f,nt)||!readU32(f,texSize)||version!=1||no>4200000||nn>4200000||nord!=no){error=L"Le fichier de scène n'est pas un export V13.14 valide.";return false;}
 s.root=root;s.obj.resize(no);s.node.resize(nn);s.order.resize(nord);s.mat.resize(nm);
 for(auto&m:s.mat){char name[24]{};if(!readBytes(f,name,24)){error=L"Table de matériaux tronquée.";return false;}m.name=name;}
 for(auto&o:s.obj){uint8_t kind,mi;uint16_t reserved;if(!readU8(f,kind)||!readU8(f,mi)||!readU16(f,reserved)||kind>5||mi>=nm||!readV(f,o.mn)||!readV(f,o.mx)){error=L"Géométrie tronquée.";return false;}o.kind=(Kind)kind;o.mat=mi;
  if(kind==0){if(!readV(f,o.a)||!readV(f,o.b)||!readV(f,o.c)||!readV(f,o.n0)||!readV(f,o.n1)||!readV(f,o.n2)||!readF(f,o.uv0.x)||!readF(f,o.uv0.y)||!readF(f,o.uv1.x)||!readF(f,o.uv1.y)||!readF(f,o.uv2.x)||!readF(f,o.uv2.y)){error=L"Triangle incomplet.";return false;}o.uv0.z=o.uv1.z=o.uv2.z=0;}
  else if(kind==2){if(!readF(f,o.r)||!readV(f,o.center)||!readV(f,o.core))return false;}
  else if(kind==3){if(!readV(f,o.a)||!readV(f,o.b)||!readF(f,o.r)||!readF(f,o.len2))return false;}
  else if(kind==4){if(!readV(f,o.a)||!readV(f,o.b)||!readF(f,o.r)||!readF(f,o.r1)||!readF(f,o.k)||!readF(f,o.len)||!readF(f,o.n0.x)||!readF(f,o.n0.y)||!readF(f,o.n0.z))return false;}
  else if(kind==5){if(!readV(f,o.a)||!readF(f,o.b.x)||!readF(f,o.b.y)||!readF(f,o.b.z))return false;}
 }
 for(auto&n:s.node){if(!readV(f,n.mn)||!readV(f,n.mx)||!readI32(f,n.left)||!readI32(f,n.right)||!readI32(f,n.start)||!readI32(f,n.count)){error=L"BVH incomplet.";return false;}}
 for(auto&i:s.order)if(!readU32(f,i)||i>=no){error=L"Ordre BVH incorrect.";return false;}
 for(auto&m:s.mat){uint32_t levels;if(!readU32(f,levels)||levels>10){error=L"Pyramide de texture incorrecte.";return false;}m.mip.resize(levels);for(auto&l:m.mip){uint32_t size;if(!readU32(f,size)||!size||size>4096){error=L"Niveau mip incorrect.";return false;}l.size=size;l.p.resize(size_t(size)*size);for(auto&v:l.p)if(!readU16(f,v)){error=L"Texture RGB565 tronquée.";return false;}}}
 return true;
}
}
