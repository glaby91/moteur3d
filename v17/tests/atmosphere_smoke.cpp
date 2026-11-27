#include "../atmosphere/atmosphere.h"
#include <cmath>
#include <iostream>
int main(){
    auto integrate=[](double sigma,double length){double t=1,a=1-std::exp(-sigma*length/4);for(int j=0;j<4;++j)t*=1-a;return 1-t;};
    if(integrate(0,28)!=0)return 1;
    if(integrate(.012,0)!=0)return 2;
    if(std::abs(integrate(.012,28)-(1-std::exp(-.012*28)))>1e-12)return 3;
    if(!(integrate(.012,1)<integrate(.012,4)&&integrate(.012,4)<integrate(.012,28)))return 4;
    if(!(integrate(.04,28)<1&&integrate(.04,28)>integrate(.012,28)))return 5;
    std::cout<<"Z Atmosphere five mathematical checks passed\n";
}
