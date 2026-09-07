#include "Game.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr float kDt = 1.0f / 60.0f;

bool tickerIs(const GameState& state, const char* expected) {
    return std::strcmp(state.hud.energyTicker.data(), expected) == 0;
}

// Places the local player inside the secret room in a cleared room so
// updateSecretTv() runs the donation branch on the next update.
void prepareSecretRoom(Game& game, int roomIndex, int souls, int signal) {
    GameState& state = game.networkMutableState();
    state.roomIndex = roomIndex;
    state.roomClear = true;
    state.secretTv.signal = signal;
    state.secretTv.available = true;
    state.secretTv.donationCooldown = 0.0f;
    state.secretTv.knockCueTimer = 0.0f;
    state.player.souls = souls;
    state.player.alive = true;
    state.player.downed = false;
    state.player.inSecretRoom = true;
    state.player.secretVisitRoom = roomIndex;
    state.player.secretVisitTimer = 120.0f;
    state.player.pos = {38.95f, 0.6f, 0.0f};
    state.player.vel = {};
    for (auto& target : state.targets) target.alive = false;
    for (auto& request : state.respawnQueue) request = HumanRespawnRequest{};
}

void donate(Game& game) {
    game.setTouchControls(0.0f, 0.0f, 0.0f, 0.0f,
                          false, false, false, false, true, false);
    game.update(kDt);
}

// Mirrors the seeded damage roll in updateSecretTv(): signal and damage are
// the values the branch sees when the roll is evaluated.
bool donationRollDamages(int roomSeed, int signal, int damage) {
    const int denominator = signal < 6 ? 12 : signal < 12 ? 9 : signal < 18 ? 7 : signal < 24 ? 5 : 4;
    const float x = std::sin((static_cast<float>(roomSeed) + static_cast<float>(roomSeed) * 0.17f +
                              static_cast<float>(signal) * 13.71f + static_cast<float>(damage) * 31.3f) * 12.9898f) * 43758.5453f;
    const float roll = x - std::floor(x);
    return roll < 1.0f / static_cast<float>(denominator);
}

}  // namespace

int main() {
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) {
            std::fprintf(stderr, "SECRET_TV_POLICY_FAIL %s\n", name);
            ++failures;
        }
    };

    // Availability cadence: first eligible room is 3, then every 2 rooms.
    for (int room = 0; room <= 15; ++room) {
        Game game;
        game.reset();
        GameState& state = game.networkMutableState();
        state.roomIndex = room;
        state.roomClear = false;
        game.update(kDt);
        if (state.secretTv.available) {
            check(false, "available-before-clear");
            break;
        }
        state.roomClear = true;
        game.update(kDt);
        const bool expected = room >= 3 && ((room - 3) % 2) == 0;
        if (state.secretTv.available != expected) {
            std::fprintf(stderr, "SECRET_TV_POLICY_FAIL cadence room=%d expected=%d\n",
                         room, expected ? 1 : 0);
            ++failures;
        }
    }

    // Signal and damage survive room transitions (recurrence relies on this).
    {
        Game game;
        game.reset();
        GameState& state = game.networkMutableState();
        state.secretTv.signal = 7;
        state.secretTv.damage = 1;
        state.roomIndex = 5;
        game.debugNextRoom();
        check(game.state().secretTv.signal == 7, "signal-persists-across-rooms");
        check(game.state().secretTv.damage == 1, "damage-persists-across-rooms");
    }

    // Donation consumes exactly one soul; gain scales with the hoard size
    // measured before that soul is consumed.
    struct Boundary { int souls; int gain; };
    const Boundary boundaries[] = {
        {1, 1}, {5, 1}, {6, 2}, {11, 2}, {12, 3}, {17, 3},
        {18, 4}, {23, 4}, {24, 5}, {29, 5}, {30, 6}, {35, 6},
    };
    for (const Boundary& boundary : boundaries) {
        Game game;
        game.reset();
        const int signal = 0;
        const int seed = game.state().roomSeed;
        prepareSecretRoom(game, 3, boundary.souls, signal);
        const bool damages = donationRollDamages(seed, signal + boundary.gain, 0);
        donate(game);
        const GameState& state = game.state();
        if (state.player.souls != boundary.souls - 1) {
            std::fprintf(stderr, "SECRET_TV_POLICY_FAIL one-soul souls=%d after=%d\n",
                         boundary.souls, state.player.souls);
            ++failures;
            continue;
        }
        if (state.secretTv.signal != signal + boundary.gain) {
            std::fprintf(stderr, "SECRET_TV_POLICY_FAIL gain souls=%d signal=%d expected=%d\n",
                         boundary.souls, state.secretTv.signal, signal + boundary.gain);
            ++failures;
            continue;
        }
        if (state.secretTv.damage != (damages ? 1 : 0)) {
            std::fprintf(stderr, "SECRET_TV_POLICY_FAIL damage-roll souls=%d damage=%d expected=%d\n",
                         boundary.souls, state.secretTv.damage, damages ? 1 : 0);
            ++failures;
            continue;
        }
        check(!state.secretTv.broken, "unexpected-break-at-tolerance-2");
        char expected[48]{};
        std::snprintf(expected, sizeof(expected), "SIGNAL +%d", boundary.gain);
        if (!tickerIs(state, expected)) {
            std::fprintf(stderr, "SECRET_TV_POLICY_FAIL ticker souls=%d text=%s expected=%s\n",
                         boundary.souls, state.hud.energyTicker.data(), expected);
            ++failures;
        }
    }

    // Donation is blocked while the TV is broken.
    {
        Game game;
        game.reset();
        prepareSecretRoom(game, 3, 12, 4);
        GameState& state = game.networkMutableState();
        state.secretTv.broken = true;
        state.secretTv.damage = state.secretTv.tolerance;
        donate(game);
        check(game.state().player.souls == 12, "broken-keeps-souls");
        check(game.state().secretTv.signal == 4, "broken-keeps-signal");
    }

    // The seeded damage roll still runs and breaks the TV at tolerance.
    {
        Game game;
        game.reset();
        const int seed = game.state().roomSeed;
        bool sawDamage = false;
        bool sawSafe = false;
        for (int signal = 1; signal < 64 && !(sawDamage && sawSafe); ++signal) {
            if (donationRollDamages(seed, signal, 0)) sawDamage = true; else sawSafe = true;
        }
        check(sawDamage, "break-roll-never-damages");
        check(sawSafe, "break-roll-always-damages");

        int breakSignal = -1;
        for (int signal = 1; signal < 256; ++signal) {
            if (donationRollDamages(seed, signal, game.state().secretTv.tolerance - 1)) { breakSignal = signal; break; }
        }
        check(breakSignal >= 0, "no-damaging-signal-found");
        if (breakSignal >= 0) {
            prepareSecretRoom(game, 3, 1, breakSignal - 1);
            GameState& state = game.networkMutableState();
            state.secretTv.damage = state.secretTv.tolerance - 1;
            donate(game);
            check(game.state().secretTv.broken, "tv-breaks-at-tolerance");
            check(game.state().secretTv.damage == game.state().secretTv.tolerance,
                  "damage-reaches-tolerance");
            check(tickerIs(game.state(), "NO SIGNAL"), "broken-ticker");
            check(game.state().player.souls == 0, "breaking-donation-still-consumes-soul");
        }
    }

    if (failures == 0) std::printf("SECRET_TV_POLICY_OK\n");
    return failures == 0 ? 0 : 1;
}
