#pragma once
#include <array>

namespace field_grass_texture {
constexpr int Size=32;
inline std::array<unsigned char,Size*Size*3> pixels(){
    std::array<unsigned char,Size*Size*3> out{};
    for(int y=0;y<Size;++y)for(int x=0;x<Size;++x){
        unsigned int h=static_cast<unsigned int>(x*374761393u+y*668265263u+0x9e3779b9u);h=(h^(h>>13))*1274126177u;h^=h>>16;
        const int blade=((x*5+y*3)%17)<3?12:0,noise=static_cast<int>(h&15u)-7,i=(y*Size+x)*3;
        out[i]=static_cast<unsigned char>(52+noise/2);out[i+1]=static_cast<unsigned char>(126+noise+blade);out[i+2]=static_cast<unsigned char>(62+noise/2+blade/3);
    }
    return out;
}
}
