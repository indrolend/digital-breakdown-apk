#pragma once

#include "../game/Game.hpp"
#include "../game/ModelData.hpp"
#include "../game/HumanModelData.hpp"
#include "../game/TvGifWall.hpp"
#include <string>

class Renderer {
public:
    void surfaceCreated();
    void setAssetRoot(const std::string& root) { assetRoot_=root; tvGifWall_.load(std::filesystem::path(root)/"tv-gifs"); }
    void surfaceChanged(int width, int height);
    void draw(const GameState& state);

private:
    int width_ = 1;
    int height_ = 1;
    unsigned int program_ = 0;
    unsigned int vbo_ = 0;
    unsigned int roundedVbo_ = 0;
    unsigned int phoneVbo_ = 0;
    unsigned int phoneNormalVbo_ = 0;
    unsigned int humanVbo_ = 0;
    unsigned int humanNormalVbo_ = 0;
    unsigned int soulSurfaceVbo_ = 0;
    unsigned int datamoshProgram_ = 0;
    unsigned int datamoshTexture_ = 0;
    unsigned int datamoshVbo_ = 0;
    int datamoshPos_ = -1;
    int datamoshUv_ = -1;
    int datamoshSampler_ = -1;
    int datamoshAlpha_ = -1;
    bool datamoshFrameReady_ = false;
    int roundedVertexCount_ = 0;
    int uMvp_ = -1;
    int uColor_ = -1;
    int aPos_ = -1;
    int aNormal_ = -1;
    int uModel_ = -1;
    int uUseNormal_ = -1;
    std::string assetRoot_;
    StaticModelData phoneModel_;
    HumanModelData humanModel_;
    TvGifWall tvGifWall_;
    std::vector<float> humanVertices_;
    std::vector<float> humanNormals_;

    bool initProgram();
    bool initDatamoshProgram();
    void drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]);
    void drawBox(const float* viewProj, const Vec3& pos, const Vec3& scale, const Quat& orientation, const float color[4]);
    void drawFxStrip(const float* viewProj, const Vec3& pos, const Vec3& scale, const Quat& orientation, const float color[4], const float* vertices, int vertexCount);
    void drawGrassBatch(const float* viewProj,const GameState& state,int tileIndex);
    void drawRoundedEllipsoid(const float* viewProj, const Vec3& pos, const Vec3& scale, float yaw, const float color[4]);
    void drawRoomTile(const float* viewProj, const GameState& state, int tileIndex);
    void drawHud(const GameState& state);
    void drawStaticModel(const float* viewProj,const StaticModelData& model,unsigned int vbo,unsigned int normalVbo,const Vec3& pos,const Vec3& scale,const Quat& orientation,bool shadow=false);
    void drawCheapHuman(const float* viewProj, const TargetState& target, const float color[4]);
    void drawProceduralHuman(const float* viewProj, const TargetState& target, float time, const float color[4]);
    void drawHumanModel(const float* viewProj,const TargetState& target,float time,bool shadow=false);
    void drawSoulFlesh(const float* viewProj,const TargetState& target,const Vec3& center);
    void drawDoorDataMosh(const GameState& state);
};
