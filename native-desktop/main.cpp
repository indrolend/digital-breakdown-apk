#include "DesktopRenderer.hpp"
#include "DesktopAudio.hpp"
#include "DesktopMultiplayer.hpp"
#include "Game.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#undef near
#undef far
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>

namespace {
constexpr int KEY_W_ANDROID = 51;
constexpr int KEY_A_ANDROID = 29;
constexpr int KEY_S_ANDROID = 47;
constexpr int KEY_D_ANDROID = 32;
constexpr int KEY_Q_ANDROID = 45;
constexpr int KEY_C_ANDROID = 31;
constexpr int KEY_F_ANDROID = 34;
constexpr int KEY_SHIFT_LEFT_ANDROID = 59;
constexpr int KEY_SHIFT_RIGHT_ANDROID = 60;
constexpr int KEY_SPACE_ANDROID = 62;

struct HostState {
    Game game;
    DesktopRenderer renderer;
    DesktopAudio audio;
    DesktopMultiplayer multiplayer;
    std::string multiplayerService="https://digital-breakdown-multiplayer.indrolend.workers.dev";
    std::string joinCode;
    bool enteringJoinCode=false;
    double lookX = 0.0;
    double lookY = 0.0;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool haveMouse = false;
    bool mouseCaptured = true;
    bool focused = true;
};

int androidKeyForGlfw(int key) {
    switch (key) {
        case GLFW_KEY_W: return KEY_W_ANDROID;
        case GLFW_KEY_A: return KEY_A_ANDROID;
        case GLFW_KEY_S: return KEY_S_ANDROID;
        case GLFW_KEY_D: return KEY_D_ANDROID;
        case GLFW_KEY_Q: return KEY_Q_ANDROID;
        case GLFW_KEY_C: return KEY_C_ANDROID;
        case GLFW_KEY_F: return KEY_F_ANDROID;
        case GLFW_KEY_LEFT_SHIFT: return KEY_SHIFT_LEFT_ANDROID;
        case GLFW_KEY_RIGHT_SHIFT: return KEY_SHIFT_RIGHT_ANDROID;
        case GLFW_KEY_SPACE: return KEY_SPACE_ANDROID;
        default: return -1;
    }
}

HostState* stateFor(GLFWwindow* window) {
    return static_cast<HostState*>(glfwGetWindowUserPointer(window));
}

void setMouseCaptured(GLFWwindow* window, HostState& host, bool captured) {
    host.mouseCaptured = captured;
    host.haveMouse = false;
    host.lookX = 0.0;
    host.lookY = 0.0;
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    host.game.setUiPaused(!captured);
    if (captured) {
        glfwGetCursorPos(window, &host.lastMouseX, &host.lastMouseY);
        host.haveMouse = true;
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    HostState* host = stateFor(window);
    if (!host) return;

    if(action==GLFW_PRESS&&!host->game.state().started&&host->enteringJoinCode){
        if(key==GLFW_KEY_ESCAPE){host->enteringJoinCode=false;host->joinCode.clear();return;}
        if(key==GLFW_KEY_BACKSPACE){if(!host->joinCode.empty())host->joinCode.pop_back();return;}
        if(key==GLFW_KEY_ENTER&&host->joinCode.size()==6){host->multiplayer.join(host->multiplayerService,host->joinCode);host->enteringJoinCode=false;return;}
        if(host->joinCode.size()<6&&((key>=GLFW_KEY_A&&key<=GLFW_KEY_Z)||(key>=GLFW_KEY_2&&key<=GLFW_KEY_9))){host->joinCode.push_back(static_cast<char>(key));host->game.setNetworkRoom(host->joinCode.c_str(),"ENTER ROOM CODE",false);return;}
        return;
    }
    if(action==GLFW_PRESS&&!host->game.state().started&&key==GLFW_KEY_H){host->multiplayer.host(host->multiplayerService);return;}
    if(action==GLFW_PRESS&&!host->game.state().started&&key==GLFW_KEY_J){host->enteringJoinCode=true;host->joinCode.clear();host->game.setNetworkRoom("","ENTER ROOM CODE",false);return;}
    if (action == GLFW_PRESS && !host->game.state().started && host->multiplayer.role()==DesktopMultiplayer::Role::Offline &&
        (key == GLFW_KEY_ENTER || key == GLFW_KEY_R || key == GLFW_KEY_SPACE)) {
        host->game.restart();
        setMouseCaptured(window, *host, true);
        return;
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (host->mouseCaptured) {
            setMouseCaptured(window, *host, false);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        setMouseCaptured(window, *host, !host->mouseCaptured);
        return;
    }

    const int androidKey = androidKeyForGlfw(key);
    if (androidKey >= 0) {
        host->game.setKey(androidKey, action != GLFW_RELEASE);
    }
}

void cursorCallback(GLFWwindow* window, double x, double y) {
    HostState* host = stateFor(window);
    if (!host || !host->mouseCaptured) return;

    if (!host->haveMouse) {
        host->lastMouseX = x;
        host->lastMouseY = y;
        host->haveMouse = true;
        return;
    }

    host->lookX += x - host->lastMouseX;
    host->lookY += y - host->lastMouseY;
    host->lastMouseX = x;
    host->lastMouseY = y;
}

void framebufferCallback(GLFWwindow* window, int width, int height) {
    HostState* host = stateFor(window);
    if (host) host->renderer.resize(width, height);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    HostState* host = stateFor(window);
    if (!host || action != GLFW_PRESS) return;
    if(button==GLFW_MOUSE_BUTTON_RIGHT && !host->game.state().dead) {
        host->game.setTouchControls(0,0,0,0,false,false,false,true,false,false);
        return;
    }
    if(button!=GLFW_MOUSE_BUTTON_LEFT) return;
    if (!host->game.state().started) {
        if(host->multiplayer.role()!=DesktopMultiplayer::Role::Offline)return;
        host->game.restart();
        setMouseCaptured(window, *host, true);
        return;
    }
    if (!host->mouseCaptured) {
        setMouseCaptured(window, *host, true);
    }
}

void windowFocusCallback(GLFWwindow* window, int focused) {
    HostState* host = stateFor(window);
    if (!host) return;
    host->focused = focused == GLFW_TRUE;
    host->game.clearInputState();
    if (!host->focused) {
        setMouseCaptured(window, *host, false);
    } else {
        host->lookX = 0.0;
        host->lookY = 0.0;
        host->haveMouse = false;
    }
}

void errorCallback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description ? description : "unknown");
}

bool hasArg(int argc, char** argv, const char* expected) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], expected) == 0) return true;
    }
    return false;
}

bool near(float a, float b, float eps = 0.025f) {
    return std::abs(a - b) <= eps;
}

int runParityProximityTest() {
    Game game;
    game.reset();
    GameState& s = const_cast<GameState&>(game.state());
    bool ok = true;

    const float playerRadius = 0.34f;
    const float cameraRadius = 0.42f;
    const float minWallX = -30.0f * 0.5f + 1.1f;
    const float maxWallX =  30.0f * 0.5f - 1.1f;

    s.player.pos = {minWallX - 0.25f, 0.08f, 0.0f};
    s.player.vel = {-4.0f, 0.0f, 0.0f};
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.x, minWallX);
    std::printf("PARITY left_wall_stop x=%.3f expected=%.3f\n", s.player.pos.x, minWallX);

    s.player.pos = {maxWallX + 0.25f, 0.08f, 0.0f};
    s.player.vel = {4.0f, 0.0f, 0.0f};
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.x, maxWallX);
    std::printf("PARITY right_wall_stop x=%.3f expected=%.3f\n", s.player.pos.x, maxWallX);

    const RoomCollider& c = s.roomColliders[0];
    s.player.grounded = true;
    s.player.pos = {c.minX - playerRadius + 0.10f, 0.08f, (c.minZ + c.maxZ) * 0.5f};
    s.player.vel = {2.0f, 0.0f, 0.0f};
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.x, c.minX - playerRadius);
    std::printf("PARITY obstacle_face_stop x=%.3f expected=%.3f radius=%.2f\n", s.player.pos.x, c.minX - playerRadius, playerRadius);

    s.player.pos = {c.minX - playerRadius + 0.10f, 0.08f, c.minZ - playerRadius + 0.12f};
    s.player.vel = {2.0f, 0.0f, 2.0f};
    game.update(1.0f / 60.0f);
    const bool cornerResolved = s.player.pos.x <= c.minX - playerRadius + 0.025f || s.player.pos.z <= c.minZ - playerRadius + 0.025f;
    ok &= cornerResolved;
    std::printf("PARITY obstacle_corner x=%.3f z=%.3f limits=(%.3f,%.3f)\n", s.player.pos.x, s.player.pos.z, c.minX - playerRadius, c.minZ - playerRadius);

    s.player.pos = {(c.minX + c.maxX) * 0.5f, c.topY + 0.08f, (c.minZ + c.maxZ) * 0.5f};
    s.player.vel = {0.0f, 0.0f, 0.0f};
    s.player.grounded = true;
    game.update(1.0f / 60.0f);
    ok &= near(s.player.pos.y, c.topY + 0.08f);
    std::printf("PARITY support_on_obstacle y=%.3f expected=%.3f support_radius=%.2f\n", s.player.pos.y, c.topY + 0.08f, playerRadius);

    s.player.pos = {0.0f, 0.08f, 0.0f};
    s.camera.firstPerson = false;
    s.camera.yaw = 3.14159265f * 0.5f;
    s.camera.pitch = 0.0f;
    game.update(1.0f / 60.0f);
    ok &= s.camera.pos.x >= minWallX - 3.0f && s.camera.pos.x <= 30.0f * 0.5f - cameraRadius + 0.001f;
    std::printf("PARITY camera_open pos=(%.3f,%.3f,%.3f) radius=%.2f\n", s.camera.pos.x, s.camera.pos.y, s.camera.pos.z, cameraRadius);

    s.player.pos = {maxWallX - 0.05f, 0.08f, 0.0f};
    s.camera.yaw = 3.14159265f * 0.5f;
    game.update(1.0f / 60.0f);
    ok &= s.camera.pos.x <= 30.0f * 0.5f - cameraRadius + 0.001f;
    std::printf("PARITY camera_wall_compress x=%.3f max=%.3f\n", s.camera.pos.x, 30.0f * 0.5f - cameraRadius);

    std::printf("PARITY_PROXIMITY_%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}

int runSmokeTest() {
    Game game;
    game.reset();

    for (int i = 0; i < 8; ++i) {
        game.update(1.0f / 60.0f);
    }

    const GameState& state = game.state();
    if (state.frame != 8 || state.roomIndex < 1 || !state.player.alive) {
        std::fprintf(stderr, "SMOKE_TEST_FAILED frame=%d room=%d alive=%d\n",
                     state.frame,
                     state.roomIndex,
                     state.player.alive ? 1 : 0);
        return 1;
    }

    std::printf(
        "SMOKE_TEST_OK frame=%d room=%d battery=%.2f targets=%d\n",
        state.frame,
        state.roomIndex,
        state.player.battery,
        TARGET_COUNT
    );
    return 0;
}
const char* argValue(int argc,char** argv,const char* expected){for(int i=1;i+1<argc;++i)if(std::strcmp(argv[i],expected)==0)return argv[i+1];return nullptr;}
bool captureFramebuffer(const std::filesystem::path& path,int width,int height){std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3u);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<width<<" "<<height<<"\n255\n";for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3u),static_cast<std::streamsize>(width*3));return static_cast<bool>(out);}

int runModelTest(const std::filesystem::path& root) {
    HumanModelData human;StaticModelData phone,flower;
    if(!human.load((root/"models"/"human.dbhuman").string())||!phone.load((root/"models"/"phone.dbmesh").string())||!flower.load((root/"models"/"flower.dbmesh").string())){std::fprintf(stderr,"MODEL_TEST_FAILED load\n");return 1;}
    std::vector<float> idle,walk,attack;human.skin(0.0f,0.0f,0,idle);human.skin(0.25f,0.0f,0,walk);human.skin(0.25f,0.24f,1,attack);
    auto finite=[](const std::vector<float>& v){return !v.empty()&&std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x)&&std::abs(x)<100.0f;});};
    auto differs=[](const std::vector<float>& a,const std::vector<float>& b){if(a.size()!=b.size())return true;for(std::size_t i=0;i<a.size();++i)if(std::abs(a[i]-b[i])>0.00001f)return true;return false;};
    float minY=100,maxY=-100;for(std::size_t i=1;i<idle.size();i+=3){minY=std::min(minY,idle[i]);maxY=std::max(maxY,idle[i]);}
    const bool ok=human.vertices.size()==4164&&human.bones.size()==33&&human.frameCount==60&&phone.vertices.size()/3==253740&&phone.batches.size()==30&&flower.vertices.size()/3==11628&&finite(idle)&&finite(walk)&&finite(attack)&&differs(idle,walk)&&differs(walk,attack)&&minY>-0.03f&&maxY>1.0f&&maxY<1.25f;
    std::printf("MODEL_TEST_%s human=%zu bones=%zu frames=%u phone=%zu/%zu flower=%zu y=[%.4f,%.4f] walk=%d attack=%d\n",ok?"OK":"FAILED",human.vertices.size(),human.bones.size(),human.frameCount,phone.vertices.size()/3,phone.batches.size(),flower.vertices.size()/3,minY,maxY,differs(idle,walk)?1:0,differs(walk,attack)?1:0);return ok?0:1;
}
}

int main(int argc, char** argv) {
    const bool captureHuman=argValue(argc,argv,"--capture-human-frame")!=nullptr;
    const bool captureSoul=argValue(argc,argv,"--capture-soul-frame")!=nullptr;
    const bool captureStart=argValue(argc,argv,"--capture-start-frame")!=nullptr;
    const bool capturePaused=argValue(argc,argv,"--capture-paused-frame")!=nullptr;
    const bool captureMosh=argValue(argc,argv,"--capture-mosh-frame")!=nullptr;
    const bool capturePhone=argValue(argc,argv,"--capture-phone-frame")!=nullptr;
    const char* capturePath=captureHuman?argValue(argc,argv,"--capture-human-frame"):(captureSoul?argValue(argc,argv,"--capture-soul-frame"):(captureStart?argValue(argc,argv,"--capture-start-frame"):(capturePaused?argValue(argc,argv,"--capture-paused-frame"):(captureMosh?argValue(argc,argv,"--capture-mosh-frame"):(capturePhone?argValue(argc,argv,"--capture-phone-frame"):argValue(argc,argv,"--capture-frame"))))));
    if (hasArg(argc, argv, "--smoke-test")) {
        return runSmokeTest();
    }
    if (hasArg(argc, argv, "--parity-proximity-test")) {
        return runParityProximityTest();
    }
    if (hasArg(argc, argv, "--model-test")) {
        return runModelTest(std::filesystem::absolute(argv[0]).parent_path());
    }

    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Digital Breakdown: GLFW initialization failed.\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    // Browser reference creates WebGL with antialias:true. Four samples are a
    // modest desktop cost and remove the most visible geometry/crosshair jaggies.
    glfwWindowHint(GLFW_SAMPLES, 4);
    if(capturePath)glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "Digital Breakdown - Native Desktop",
        nullptr,
        nullptr
    );
    if (!window) {
        glfwTerminate();
        std::fprintf(stderr, "Digital Breakdown: window creation failed.\n");
        return 1;
    }

    HostState host;
    if(const char* service=std::getenv("DIGITAL_BREAKDOWN_MULTIPLAYER_URL"))host.multiplayerService=service;
    host.game.reset();
    if(!capturePath||captureStart)host.game.prepareStartScreen();
    if(captureHuman){GameState& fixture=const_cast<GameState&>(host.game.state());for(auto& target:fixture.targets)target.alive=false;auto& target=fixture.targets[0];target.alive=true;target.slurpable=false;target.pos={0,0.08f,fixture.player.pos.z-4.0f};target.walkTarget=target.pos;target.visualYaw=0;target.scale=1;target.visibility=1;target.attackCooldown=999;fixture.camera.yaw=0;fixture.camera.pitch=0;}
    if(captureSoul){GameState& fixture=const_cast<GameState&>(host.game.state());for(int i=1;i<TARGET_COUNT;++i)fixture.targets[i].alive=false;auto& target=fixture.targets[0];target.alive=true;target.slurpable=true;target.soulMorph=1;target.soulCubeAmount=1;target.pos=fixture.player.pos+Vec3{0,0.5f,-1.5f};target.walkTarget=target.pos;target.health=1;target.armor=0;target.soulState=SoulState::Free;fixture.camera.yaw=0;fixture.camera.pitch=0;}
    if(capturePaused)host.game.setUiPaused(true);
    if(capturePhone){GameState& fixture=const_cast<GameState&>(host.game.state());for(auto& target:fixture.targets)target.alive=false;}
    host.audio.setAssetRoot(std::filesystem::absolute(argv[0]).parent_path()/"audio");

    glfwSetWindowUserPointer(window, &host);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetWindowFocusCallback(window, windowFocusCallback);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);

    glfwMakeContextCurrent(window);
    host.renderer.setAssetRoot(std::filesystem::absolute(argv[0]).parent_path()/"models");
    glfwSwapInterval(1);
    setMouseCaptured(window, host, host.game.state().started);
    if(capturePaused)host.game.setUiPaused(true);

    int framebufferWidth = 1;
    int framebufferHeight = 1;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    host.renderer.resize(framebufferWidth, framebufferHeight);
    if(const char* service=argValue(argc,argv,"--service-url"))host.multiplayerService=service;
    if(hasArg(argc,argv,"--host-room"))host.multiplayer.host(host.multiplayerService);
    if(const char* room=argValue(argc,argv,"--join-room"))host.multiplayer.join(host.multiplayerService,room);

    std::printf("Digital Breakdown native desktop host running.\n");
    std::printf("WASD move | Shift sprint | Space jump | Mouse look | Left mouse vacuum | F melee | Q shoot | C camera | Tab release mouse | Esc quit\n");

    auto previous = std::chrono::steady_clock::now();
    int captureFrames=0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        const bool vacuumHeld = captureSoul || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool sprintHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        host.game.setTouchControls(
            0.0f,
            0.0f,
            static_cast<float>(host.lookX),
            static_cast<float>(host.lookY),
            vacuumHeld,
            sprintHeld,
            false,
            false,
            false,
            false
        );
        host.lookX = 0.0;
        host.lookY = 0.0;

        host.multiplayer.update(host.game);
        if (host.game.state().started && host.game.state().multiplayer.connected && !host.mouseCaptured) {
            setMouseCaptured(window, host, true);
        }

        if(captureMosh&&captureFrames==10){GameState& fixture=const_cast<GameState&>(host.game.state());fixture.doorTransition.active=true;fixture.doorTransition.progress=1.0f;fixture.doorTransition.distanceTravelled=0;fixture.doorTransition.lastPlayerPos=fixture.player.pos;}
        host.game.update(capturePath?1.0f/60.0f:std::min(dt, 0.033f));
        if(capturePhone){GameState& fixture=const_cast<GameState&>(host.game.state());fixture.camera.pos=fixture.phoneTransform.position+Vec3{0,0.035f,0.38f};fixture.camera.lookTarget=fixture.phoneTransform.position;fixture.camera.forward=normalized(fixture.camera.lookTarget-fixture.camera.pos);}
        host.audio.update(host.game.state());
        host.renderer.draw(host.game.state());
        if(capturePath&&++captureFrames>=30){const bool captured=captureFramebuffer(capturePath,framebufferWidth,framebufferHeight);std::printf("CAPTURE_FRAME_%s %s\n",captured?"OK":"FAILED",capturePath);glfwSetWindowShouldClose(window,GLFW_TRUE);}
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    host.audio.stopAll();
    host.multiplayer.disconnect();
    glfwTerminate();
    return 0;
}
