#pragma once

#include "Game.hpp"
#include "ModelData.hpp"
#include "HumanModelData.hpp"
#include "DeveloperCodec.hpp"

#include <filesystem>
#include <vector>
#include "../native-android/app/src/main/cpp/game/TvGifWall.hpp"

class DesktopRenderer {
public:
    static void drawBox(const Vec3& position, const Vec3& scale, float pitch, float yaw, float roll, float r, float g, float b, float a = 1.0f);
    static void drawBox(const Vec3& position, const Vec3& scale, const Quat& orientation, float r, float g, float b);
    void setAssetRoot(const std::filesystem::path& root);
    void resize(int width, int height);
    void setHudVisible(bool visible);
    void draw(const GameState& state, const DeveloperCodecState* codec=nullptr) const;

private:
    TvGifWall tvGifWall_;
    mutable int width_ = 1280;
    mutable int height_ = 720;
    unsigned int phoneModelList_ = 0;
    unsigned int phoneShadowList_ = 0;
    HumanModelData humanModel_;
    mutable std::vector<float> humanVertices_;
    mutable unsigned int datamoshTexture_ = 0;
    mutable unsigned int tvScreenTexture_ = 0;
    mutable unsigned int phoneDisplayTexture_ = 0;
    mutable unsigned int fieldGrassTexture_ = 0;
    mutable std::vector<unsigned char> phoneDisplayPixels_;
    mutable unsigned long long phoneDisplayCacheKey_ = 0;
    mutable bool phoneDisplayCacheValid_ = false;
    mutable bool phoneDisplayTextureAllocated_ = false;
    mutable bool datamoshFrameReady_ = false;
    bool hudVisible_ = true;

    void drawRoomTile(const GameState& state, int tileIndex) const;
    void drawFieldGrass(int tileIndex) const;
    static void applyCamera(const GameState& state, float aspect);
    static void drawStaticModel(unsigned int list, const Vec3& position, const Vec3& scale, const Quat& orientation);
    void drawHumanModel(const TargetState& target, float time, bool shadow = false) const;
    static void drawSoulFlesh(const TargetState& target,const Vec3& center);
    void drawSecretTvScreen(const GameState& state, float phoneProximity) const;
    void drawPhoneDisplayTexture(const GameState& state) const;
    void drawHud(const GameState& state) const;
    void drawDeveloperCodec(const DeveloperCodecState& codec) const;
    void drawDoorDataMosh(const GameState& state) const;
};
