#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

struct StaticModelBatch {
    std::uint32_t start=0,count=0;
    float color[4]{1,1,1,1};
};

struct StaticModelData {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<StaticModelBatch> batches;
    bool load(const std::string& path) {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input) return false;
        const auto size=input.tellg(); if(size<12) return false;
        input.seekg(0); std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        if(!input.read(reinterpret_cast<char*>(bytes.data()),size)) return false;
        if(std::memcmp(bytes.data(),"DBM1",4)!=0) return false;
        std::uint32_t vertexCount=0,batchCount=0; std::memcpy(&vertexCount,bytes.data()+4,4);std::memcpy(&batchCount,bytes.data()+8,4);
        const std::size_t expected=12u+static_cast<std::size_t>(vertexCount)*12u+static_cast<std::size_t>(batchCount)*24u;
        if(bytes.size()!=expected || vertexCount>1000000u || batchCount>1024u) return false;
        vertices.resize(static_cast<std::size_t>(vertexCount)*3u); std::memcpy(vertices.data(),bytes.data()+12,vertices.size()*sizeof(float));
        normals.assign(vertices.size(),0.0f);for(std::size_t i=0;i+8<vertices.size();i+=9){const float ax=vertices[i],ay=vertices[i+1],az=vertices[i+2],bx=vertices[i+3],by=vertices[i+4],bz=vertices[i+5],cx=vertices[i+6],cy=vertices[i+7],cz=vertices[i+8],ux=bx-ax,uy=by-ay,uz=bz-az,vx=cx-ax,vy=cy-ay,vz=cz-az;float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;const float len=std::sqrt(nx*nx+ny*ny+nz*nz);if(len>0.000001f){nx/=len;ny/=len;nz/=len;}for(int v=0;v<3;++v){normals[i+v*3]=nx;normals[i+v*3+1]=ny;normals[i+v*3+2]=nz;}}
        batches.resize(batchCount); std::size_t at=12+vertices.size()*sizeof(float);
        for(auto& batch:batches){std::memcpy(&batch.start,bytes.data()+at,4);std::memcpy(&batch.count,bytes.data()+at+4,4);std::memcpy(batch.color,bytes.data()+at+8,16);at+=24;if(batch.start+batch.count>vertexCount)return false;}
        return true;
    }
    bool valid() const { return !vertices.empty() && !batches.empty(); }
};
