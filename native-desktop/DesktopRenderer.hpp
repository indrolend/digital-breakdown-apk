#pragma once

#include "Game.hpp"
#include "ModelData.hpp"
#include "HumanModelData.hpp"

#include <filesystem>
#include <string>
#include "../native-android/app/src/main/cpp/game/TvGifWall.hpp"

class DesktopRenderer {
public:
    static void drawBox(const Vec3& position, const Vec3& scale, float pitch, float yaw, float roll, float r, float g, float b, float a = 1.0f);
    static void drawBox(const Vec3& position, const Vec3& scale, const Quat& orientation, float r, float g, float b);
    void setAssetRoot(const std::filesystem::path& root);
    void setReleaseStatus(const std::string& version, bool updateAvailable, const std::string& updateVersion);
    void resize(int width, int height);
    void draw(const GameState& state) const;

private:
    TvGifWall tvGifWall_;
    mutable int width_ = 1280;
    mutable int height_ = 720;
    unsigned int phoneModelList_ = 0;
    unsigned int flowerModelList_ = 0;
    unsigned int phoneShadowList_ = 0;
    unsigned int flowerShadowList_ = 0;
    HumanModelData humanModel_;
    mutable std::vector<float> humanVertices_;
    mutable unsigned int datamoshTexture_ = 0;
    mutable bool datamoshFrameReady_ = false;
    std::string appVersion_ = "0.0.0";
    std::string updateVersion_;
    bool updateAvailable_ = false;

    static void drawRoomTile(const GameState& state, int tileIndex);
    static void applyCamera(const GameState& state, float aspect);
    static void drawStaticModel(unsigned int list, const Vec3& position, const Vec3& scale, const Quat& orientation);
    void drawHumanModel(const TargetState& target, float time, bool shadow = false) const;
    static void drawSoulFlesh(const TargetState& target,const Vec3& center);
    void drawHud(const GameState& state) const;
    void drawDoorDataMosh(const GameState& state) const;
};
