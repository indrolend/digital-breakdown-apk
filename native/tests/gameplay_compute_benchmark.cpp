#include "Game.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {
constexpr std::uint32_t kSeed = 0xB34DBEEFu;
constexpr int kWarmupFrames = 3600;
constexpr int kMeasuredFrames = 36000;
constexpr float kDt = 1.0f / 60.0f;

struct Random {
    std::uint32_t value = kSeed;
    std::uint32_t next() {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        return value;
    }
    float signedUnit() { return static_cast<float>(next() & 0xffffu) / 32767.5f - 1.0f; }
    bool oneIn(std::uint32_t n) { return next() % n == 0; }
};

std::uint64_t mix(std::uint64_t hash, std::uint64_t value) {
    return (hash ^ value) * 1099511628211ull;
}

std::uint64_t quantized(float value) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(value * 1000.0f)));
}

std::uint64_t stateHash(const GameState& state) {
    std::uint64_t hash = 1469598103934665603ull;
    hash = mix(hash, static_cast<std::uint64_t>(state.frame));
    hash = mix(hash, static_cast<std::uint64_t>(state.roomIndex));
    hash = mix(hash, quantized(state.player.pos.x));
    hash = mix(hash, quantized(state.player.pos.y));
    hash = mix(hash, quantized(state.player.pos.z));
    hash = mix(hash, quantized(state.player.battery));
    hash = mix(hash, static_cast<std::uint64_t>(state.player.souls));
    for (const TargetState& target : state.targets) {
        hash = mix(hash, quantized(target.pos.x));
        hash = mix(hash, quantized(target.pos.z));
        hash = mix(hash, quantized(target.health));
        hash = mix(hash, quantized(target.ingestProgress));
    }
    return hash;
}

void drive(Game& game, Random& random, int frame, float& moveX, float& moveZ) {
    if (game.state().upgradeMenu.active) {
        game.chooseTemporaryUpgrade(static_cast<int>(random.next() % 3u));
    }
    if (game.state().dead) {
        game.restart();
    }
    if ((frame % 75) == 0) {
        moveX = random.signedUnit();
        moveZ = random.signedUnit();
    }
    game.setTouchControls(
        moveX, moveZ, random.signedUnit() * 2.4f, random.signedUnit() * 1.5f,
        (random.next() % 100u) < 24u, (random.next() % 100u) < 38u,
        random.oneIn(70), random.oneIn(42), random.oneIn(95), random.oneIn(1200));
    game.update(kDt);
}

std::uint64_t run(bool measured, double& elapsedMs, int& maxRoom, int& maxSouls) {
    Game game;
    game.reset();
    Random random;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    const int frames = measured ? kMeasuredFrames : kWarmupFrames;
    const auto begin = std::chrono::steady_clock::now();
    for (int frame = 0; frame < frames; ++frame) {
        drive(game, random, frame, moveX, moveZ);
        maxRoom = std::max(maxRoom, game.state().roomIndex);
        maxSouls = std::max(maxSouls, game.state().player.souls);
    }
    const auto end = std::chrono::steady_clock::now();
    elapsedMs = std::chrono::duration<double, std::milli>(end - begin).count();
    return stateHash(game.state());
}
}  // namespace

int main() {
    double warmupMs = 0.0;
    int warmupRoom = 0;
    int warmupSouls = 0;
    run(false, warmupMs, warmupRoom, warmupSouls);

    double elapsedMs = 0.0;
    int maxRoom = 0;
    int maxSouls = 0;
    const std::uint64_t firstHash = run(true, elapsedMs, maxRoom, maxSouls);

    double verifyMs = 0.0;
    int verifyRoom = 0;
    int verifySouls = 0;
    const std::uint64_t secondHash = run(true, verifyMs, verifyRoom, verifySouls);
    if (firstHash != secondHash || maxRoom != verifyRoom || maxSouls != verifySouls) {
        std::fprintf(stderr, "GAMEPLAY_COMPUTE_FAIL deterministic=0 hash_a=%llu hash_b=%llu\n",
                     static_cast<unsigned long long>(firstHash),
                     static_cast<unsigned long long>(secondHash));
        return 1;
    }

    const double seconds = elapsedMs / 1000.0;
    const double ticksPerSecond = static_cast<double>(kMeasuredFrames) / seconds;
    const double realtimeFactor = ticksPerSecond / 60.0;
    const double microsecondsPerTick = elapsedMs * 1000.0 / static_cast<double>(kMeasuredFrames);
    std::printf(
        "GAMEPLAY_COMPUTE_OK seed=%u frames=%d simulated_seconds=%.1f elapsed_ms=%.3f "
        "ticks_per_second=%.1f realtime_factor=%.2f us_per_tick=%.3f max_room=%d max_souls=%d "
        "state_hash=%llu deterministic=1\n",
        kSeed, kMeasuredFrames, kMeasuredFrames * kDt, elapsedMs, ticksPerSecond,
        realtimeFactor, microsecondsPerTick, maxRoom, maxSouls,
        static_cast<unsigned long long>(firstHash));
    return 0;
}
