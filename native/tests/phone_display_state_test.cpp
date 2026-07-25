#include <cassert>
#include <cmath>

#include "Game.hpp"

namespace {

void step(Game& game, int ticks = 1, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < ticks; ++i) game.update(dt);
}

bool finite(float value) {
    return std::isfinite(value);
}

void expectFiniteAndBounded(const PhoneDisplayState& display) {
    assert(finite(display.brightness) && display.brightness >= 0.0f && display.brightness <= 1.0f);
    assert(finite(display.contentOpacity) && display.contentOpacity >= 0.0f && display.contentOpacity <= 1.0f);
    assert(finite(display.emissionStrength) && display.emissionStrength >= 0.0f && display.emissionStrength <= 2.4f);
    assert(finite(display.localLightIntensity) && display.localLightIntensity >= 0.0f && display.localLightIntensity <= 1.15f);
    assert(finite(display.localLightRadius) && display.localLightRadius >= 0.12f && display.localLightRadius <= 0.32f);
    assert(finite(display.blackLevel) && display.blackLevel >= 0.08f && display.blackLevel <= 1.0f);
    assert(finite(display.material.backgroundEmission));
    assert(finite(display.material.glassEmission));
    assert(finite(display.material.rimEmission));
    assert(display.material.backgroundEmission >= display.material.rimEmission);
    assert(display.material.rimEmission >= display.material.glassEmission);
    assert(finite(display.lighting.intensity));
    assert(finite(display.lighting.radius));
}

} // namespace

int main() {
    Game game;
    game.prepareStartScreen();
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::MainMenu);
    assert(game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);

    GameState& menu = const_cast<GameState&>(game.state());
    menu.localSettings.menuPage = LocalMenuPage::Controls;
    step(game);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Controls);
    assert(game.state().phoneDisplay.previousMode == PhoneDisplayMode::MainMenu);
    expectFiniteAndBounded(game.state().phoneDisplay);

    game.restart();
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Boot);
    expectFiniteAndBounded(game.state().phoneDisplay);
    step(game, 80);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Gameplay);
    assert(!game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);

    GameState& gameplay = const_cast<GameState&>(game.state());
    gameplay.hud.lowBattery = true;
    gameplay.player.battery = 8.0f;
    step(game, 6);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Warning);
    assert(game.state().phoneDisplay.lowBatteryPulse > 0.0f);
    expectFiniteAndBounded(game.state().phoneDisplay);

    game.setUiPaused(true);
    step(game);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Pause);
    assert(game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);

    GameState& death = const_cast<GameState&>(game.state());
    death.dead = true;
    death.started = false;
    death.uiPaused = false;
    step(game);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Death);
    assert(game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);

    return 0;
}
