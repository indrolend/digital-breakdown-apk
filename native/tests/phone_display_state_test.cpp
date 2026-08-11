#include <cassert>
#include <cmath>

#include "Game.hpp"
#include "PhoneDisplayLayout.hpp"

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

void expectRectInside(const PhoneDisplayRect& outer, const PhoneDisplayRect& inner) {
    constexpr float epsilon = 0.001f;
    assert(inner.x + epsilon >= outer.x);
    assert(inner.y + epsilon >= outer.y);
    assert(inner.x + inner.w <= outer.x + outer.w + epsilon);
    assert(inner.y + inner.h <= outer.y + outer.h + epsilon);
}

void expectLayoutInside(const PhoneDisplayMenuLayout& layout) {
    assert(layout.logicalW == PhoneDisplayState::LogicalWidth);
    assert(layout.logicalH == PhoneDisplayState::LogicalHeight);
    assert(layout.safe.x > 0.0f && layout.safe.y > 0.0f);
    assert(layout.safe.x + layout.safe.w < static_cast<float>(layout.logicalW));
    assert(layout.safe.y + layout.safe.h < static_cast<float>(layout.logicalH));
    for (int i = 0; i < layout.rowCount; ++i) {
        const PhoneDisplayMenuRow& row = layout.rows[i];
        if (row.selectable) {
            assert(row.selectableIndex >= 0);
            if (row.visible) expectRectInside(layout.safe, row.hit);
            else assert(row.hit.w == 0.0f && row.hit.h == 0.0f);
        } else {
            assert(row.selectableIndex < 0);
            assert(row.hit.w == 0.0f && row.hit.h == 0.0f);
        }
    }
}

void expectSelectableHit(const PhoneDisplayMenuLayout& layout, int selection) {
    const PhoneDisplayMenuRow* row = phoneDisplayRowForSelection(layout, selection);
    assert(row != nullptr);
    const float cx = row->hit.x + row->hit.w * 0.5f;
    const float cy = row->hit.y + row->hit.h * 0.5f;
    assert(phoneDisplayItemAt(layout, cx, cy) == selection);
}

} // namespace

int main() {
    Game game;
    game.prepareAttractScreen();
    assert(game.state().attractMode);
    assert(game.state().started);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Gameplay);
    const unsigned int attractAudioSerial = game.state().audio.nextSerial;
    float previousAttractYaw = game.state().camera.yaw;
    for (int tick = 0; tick < 180; ++tick) {
        step(game);
        const float yawStep = std::atan2(
            std::sin(game.state().camera.yaw - previousAttractYaw),
            std::cos(game.state().camera.yaw - previousAttractYaw));
        assert(std::abs(yawStep) < 0.30f);
        previousAttractYaw = game.state().camera.yaw;
    }
    assert(game.state().attractMode);
    assert(game.state().frame > 0);
    assert(game.state().audio.nextSerial == attractAudioSerial);
    GameState& exhaustedAttract = const_cast<GameState&>(game.state());
    exhaustedAttract.dead = true;
    step(game);
    assert(game.state().attractMode);
    assert(game.state().started);
    assert(!game.state().dead);
    game.dismissAttractMode();
    assert(!game.state().attractMode);
    assert(!game.state().started);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::MainMenu);

    game.prepareStartScreen();
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::MainMenu);
    assert(game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);

    GameState& menu = const_cast<GameState&>(game.state());
    PhoneDisplayMenuLayout mainLayout = makePhoneDisplayMenuLayout(menu);
    expectLayoutInside(mainLayout);
    assert(mainLayout.title.empty());
    assert(mainLayout.selectableCount == 4);
    expectSelectableHit(mainLayout, 0);

    menu.localSettings.menuPage = LocalMenuPage::Controls;
    menu.localSettings.menuScroll = 0.0f;
    PhoneDisplayMenuLayout controls = makePhoneDisplayMenuLayout(menu);
    expectLayoutInside(controls);
    assert(controls.title == "Controls");
    assert(controls.selectableCount == 14);
    assert(controls.rowCount == 17);
    assert(controls.rows[0].kind == PhoneMenuRowKind::Section);
    assert(!controls.rows[0].selectable);
    expectSelectableHit(controls, 0);
    menu.localSettings.menuScroll = phoneDisplayScrollForSelection(controls, controls.selectableCount - 1);
    PhoneDisplayMenuLayout controlsScrolled = makePhoneDisplayMenuLayout(menu);
    expectLayoutInside(controlsScrolled);
    expectSelectableHit(controlsScrolled, controlsScrolled.selectableCount - 1);

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
    death.localSettings.menuPage = LocalMenuPage::Main;
    step(game);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::Off);
    assert(!game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);
    PhoneDisplayMenuLayout deathLayout = makePhoneDisplayMenuLayout(death);
    expectLayoutInside(deathLayout);
    assert(deathLayout.selectableCount == 0);
    assert(deathLayout.rowCount == 0);

    return 0;
}
