#include <cassert>
#include <cmath>

#include "Game.hpp"
#include "GameplayPhoneModel.hpp"
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
    const Vec3 controlsAccent=phoneDisplayModeAccent(PhoneDisplayMode::Controls);
    const Vec3 audioAccent=phoneDisplayModeAccent(PhoneDisplayMode::Audio);
    const Vec3 graphicsAccent=phoneDisplayModeAccent(PhoneDisplayMode::Graphics);
    assert(controlsAccent.y>controlsAccent.x&&controlsAccent.z>controlsAccent.x);
    assert(audioAccent.x>audioAccent.y&&audioAccent.z>audioAccent.y);
    assert(graphicsAccent.x>graphicsAccent.z&&graphicsAccent.y>graphicsAccent.z);
    PhoneDisplayState transitioning{};
    transitioning.previousMode=PhoneDisplayMode::Controls;
    transitioning.mode=PhoneDisplayMode::Audio;
    transitioning.transitionProgress=0.0f;
    assert(length(phoneDisplayResolvedAccent(transitioning)-controlsAccent)<0.001f);
    transitioning.transitionProgress=1.0f;
    assert(length(phoneDisplayResolvedAccent(transitioning)-audioAccent)<0.001f);
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
    assert(game.state().attractMode);
    assert(game.state().cinematic.attractExitActive);
    for (int tick = 0; tick < 24 && game.state().attractMode; ++tick) step(game);
    assert(!game.state().attractMode);
    assert(!game.state().started);
    assert(game.state().cinematic.menuEnterActive);
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::MainMenu);

    game.prepareStartScreen();
    assert(game.state().phoneDisplay.mode == PhoneDisplayMode::MainMenu);
    assert(game.state().phoneDisplay.interactive);
    expectFiniteAndBounded(game.state().phoneDisplay);

    GameState& menu = const_cast<GameState&>(game.state());
    PhoneDisplayMenuLayout mainLayout = makePhoneDisplayMenuLayout(menu);
    expectLayoutInside(mainLayout);
    assert(mainLayout.title.empty());
    assert(mainLayout.navigationHint.empty());
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
    menu.hud.menuSelection = 9;
    controls = makePhoneDisplayMenuLayout(menu);
    assert(controls.navigationHint.find("ADJUST") != std::string::npos);
    assert(phoneMenuEmphasis(PhoneMenuAction::Solo) == PhoneMenuEmphasis::Primary);
    assert(phoneMenuEmphasis(PhoneMenuAction::ExitRun) == PhoneMenuEmphasis::Destructive);
    menu.localSettings.controllerLookSensitivity = 1.125f;
    assert(std::abs(phoneMenuVisualAmount(PhoneMenuAction::AdjustController,menu.localSettings)-0.5f)<0.001f);
    menu.localSettings.shadows = false;
    assert(phoneMenuVisualAmount(PhoneMenuAction::ToggleShadows,menu.localSettings)==0.0f);
    menu.localSettings.shadows = true;
    assert(phoneMenuVisualAmount(PhoneMenuAction::ToggleShadows,menu.localSettings)==1.0f);
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
    gameplay.player.battery = 67.0f;
    gameplay.player.souls = 4;
    gameplay.requiredSouls = 7;
    gameplay.depositedSouls = 3;
    gameplay.roomIndex = 6;
    gameplay.progression.permanent.tokens = 11;
    gameplay.energy.supplementalActive = true;
    gameplay.energy.supplementalValue = 20.0f;
    gameplay.energy.supplementalMax = 80.0f;
    gameplay.energy.flowerStacks = 2;
    const GameplayPhoneModel instrument = makeGameplayPhoneModel(gameplay);
    assert(instrument.batteryPercent == 67);
    assert(!instrument.lowBattery);
    assert(instrument.storedSouls == 4 && instrument.soulCapacity == PHONE_CAPACITY);
    assert(instrument.filledGoals == 3 && instrument.requiredGoals == 7);
    assert(instrument.roomIndex == 6 && instrument.tokens == 11);
    assert(instrument.supplementalActive && std::abs(instrument.supplementalFill - 0.25f) < 0.001f);
    assert(instrument.flowerStacks == 2);
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
