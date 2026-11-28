#include "../escargot/object_visibility.h"
#include <iostream>
#include <cassert>
using namespace m3d::escargot::object_visibility;
int main(){
    const int w=320,h=180;
    std::vector<int> domain;for(int y=145;y<175;++y)for(int x=43;x<104;++x)
        if(x>=43+(y-145)/3&&x<104-(y-145)/4)domain.push_back(y*w+x);
    for(int pose=0;pose<3;++pose)for(int thin:{0,1})for(int budget:{256,1024,4096}){
        auto truth=[&](int i){int x=i%w,y=i/w;return int(!((x>62+pose&&x<76+pose&&y<166)||(thin&&y==160)));};
        std::vector<float> mask(w*h,-1);int queries=0;
        auto s=reconstruct(w,domain,budget/2,[&](int i){++queries;return truth(i);},mask);
        int fp=0,fn=0,unknown=0;for(int i:domain){if(mask[i]<0)++unknown;else{fp+=mask[i]>=.5&&!truth(i);fn+=mask[i]<.5&&truth(i);}}
        if(s.seedSamples<std::min({32,int(domain.size()),budget/2})||queries>budget/2||unknown||
           (s.exhaustive&&(fp||fn))||(!s.exhaustive&&!s.zeroSplits)){std::cerr<<"FAIL\n";return 1;}
        std::cout<<"pose="<<pose<<" thin="<<thin<<" budget="<<budget<<" projected="<<domain.size()<<" samples="<<queries
                 <<" seed="<<s.seedSamples<<" zeroProbes="<<s.zeroProbes<<" falseVisible="<<fp<<" falseHidden="<<fn<<" exhaustive="<<s.exhaustive<<"\n";
    }
    // Tiny all-zero footprint and a budget too low to see everything:
    // zero remains zero; exploratory refinement must still occur.
    std::vector<int> d;for(int y=10;y<20;y++)for(int x=10;x<20;x++)d.push_back(y*w+x);
    std::vector<float> mask(w*h,-1);auto z=reconstruct(w,d,64,[](int){return 0;},mask);
    if(!z.zeroSplits||!z.zeroProbes||z.measured!=64)return 2;
    for(int i:d)if(mask[i]!=0)return 3;
    return 0;
}
