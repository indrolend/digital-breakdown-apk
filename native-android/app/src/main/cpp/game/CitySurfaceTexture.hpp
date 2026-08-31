#pragma once
#include <array>

namespace city_surface_texture {
constexpr int Size=32;
inline std::array<unsigned char,Size*Size*3> pixels(){
    std::array<unsigned char,Size*Size*3> out{};
    for(int y=0;y<Size;++y)for(int x=0;x<Size;++x){
        unsigned int h=static_cast<unsigned int>(x*2246822519u+y*3266489917u+0x85ebca6bu);h=(h^(h>>15))*2246822519u;h^=h>>13;
        const int aggregate=static_cast<int>(h&15u)-7;
        const bool seam=(x==0||y==0),repair=((x+2*y+7)%29)==0;
        const int base=seam?53:(repair?68:62+aggregate/2),i=(y*Size+x)*3;
        out[i]=static_cast<unsigned char>(base);out[i+1]=static_cast<unsigned char>(base+3);out[i+2]=static_cast<unsigned char>(base+5);
    }
    return out;
}
}
