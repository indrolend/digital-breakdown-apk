#include <cassert>
#include <cmath>

#include "Game.hpp"
#include "PhoneDisplayLayout.hpp"

namespace {

void expectRectInside(const PhoneDisplayRect& outer, const PhoneDisplayRect& inner) {
    constexpr float epsilon = 0.001f;
    assert(inner.x + epsilon >= outer.x);
    assert(inner.y + epsilon >= outer.y);
    assert(inner.x + inner.w <= outer.x + outer.w + epsilon);
    assert(inner.y + inner.h <= outer.y + outer.h + epsilon);
}

void expectVisibleRowsInsideSafe(const PhoneDisplayMenuLayout& layout) {
    assert(layout.logicalW == PhoneDisplayState::LogicalWidth);
    assert(layout.logicalH == PhoneDisplayState::LogicalHeight);
    assert(layout.safe.x > 0.0f);
    assert(layout.safe.y > 0.0f);
    assert(layout.safe.x + layout.safe.w < static_cast<float>(layout.logicalW));
    assert(layout.safe.y + layout.safe.h < static_cast<float>(layout.logicalH));
    for (int i = 0; i < layout.rowCount; ++i) {
        const PhoneDisplayMenuRow& row = layout.rows[i];
        assert(std::isfinite(row.baselineY));
        if (row.kind == PhoneMenuRowKind::Section) {
            assert(!row.selectable);
            assert(row.selectableIndex < 0);
            assert(row.hit.w == 0.0f && row.hit.h == 0.0f);
            continue;
        }
        assert(row.selectable);
        assert(row.selectableIndex >= 0);
        if (row.visible) expectRectInside(layout.safe, row.hit);
        else assert(row.hit.w == 0.0f && row.hit.h == 0.0f);
    }
}

const PhoneDisplayMenuRow& selectionRow(const PhoneDisplayMenuLayout& layout, int selection) {
    const PhoneDisplayMenuRow* row = phoneDisplayRowForSelection(layout, selection);
    assert(row);
    assert(row->visible);
    const float cx = row->hit.x + row->hit.w * 0.5f;
    const float cy = row->hit.y + row->hit.h * 0.5f;
    assert(phoneDisplayItemAt(layout, cx, cy) == selection);
    return *row;
}

const PhoneMenuElement& selectionElement(const PhoneMenuPageViewModel& page, int selection) {
    const PhoneMenuElement* element = phoneMenuElementForSelection(page, selection);
    assert(element);
    return *element;
}

} // namespace

int main() {
    GameState state;
    state.phoneVisual.screenScale = {1.0f, 1.0f, 1.0f};

    state.started = false;
    state.dead = false;
    state.localSettings.menuPage = LocalMenuPage::Main;
    PhoneMenuPageViewModel mainModel = makePhoneMenuPageModel(state);
    assert(mainModel.selectableCount == 4);
    assert(selectionElement(mainModel, 0).action == PhoneMenuAction::Solo);
    assert(selectionElement(mainModel, 3).action == PhoneMenuAction::Exit);
    PhoneDisplayMenuLayout main = makePhoneDisplayMenuLayout(state);
    assert(main.selectableCount == 4);
    expectVisibleRowsInsideSafe(main);
    assert(selectionRow(main, 0).action == PhoneMenuAction::Solo);

    state.started = true;
    state.uiPaused = true;
    state.multiplayer.enabled = false;
    state.upgradeMenu.active = false;
    state.localSettings.menuPage = LocalMenuPage::Main;
    PhoneMenuPageViewModel pauseModel = makePhoneMenuPageModel(state);
    assert(pauseModel.selectableCount == 5);
    assert(selectionElement(pauseModel, 0).action == PhoneMenuAction::Resume);
    assert(selectionElement(pauseModel, 4).action == PhoneMenuAction::ExitRun);
    PhoneDisplayMenuLayout pause = makePhoneDisplayMenuLayout(state);
    assert(pause.selectableCount == 5);
    expectVisibleRowsInsideSafe(pause);

    state.localSettings.menuPage = LocalMenuPage::Controls;
    state.localSettings.menuScroll = 0.0f;
    PhoneMenuPageViewModel controlsModel = makePhoneMenuPageModel(state);
    assert(controlsModel.tablePage);
    assert(controlsModel.selectableCount == 16);
    assert(controlsModel.elementCount == 19);
    assert(controlsModel.elements[0].kind == PhoneMenuRowKind::Section);
    assert(!controlsModel.elements[0].selectable);
    assert(selectionElement(controlsModel, 0).bindingAction == 0);
    assert(selectionElement(controlsModel, 5).bindingAction == 5);
    assert(selectionElement(controlsModel, 6).bindingAction == 6);
    assert(selectionElement(controlsModel, 9).bindingAction == 9);
    assert(selectionElement(controlsModel, 10).action == PhoneMenuAction::AdjustMouse);
    assert(selectionElement(controlsModel, 11).action == PhoneMenuAction::AdjustController);
    assert(selectionElement(controlsModel, 12).action == PhoneMenuAction::AdjustTriggers);
    assert(selectionElement(controlsModel, 12).value == "Balanced");
    assert(selectionElement(controlsModel, 13).action == PhoneMenuAction::AdjustVibration);
    assert(selectionElement(controlsModel, 13).value == "Subtle");
    assert(selectionElement(controlsModel, 14).action == PhoneMenuAction::Defaults);
    assert(selectionElement(controlsModel, 15).action == PhoneMenuAction::Back);

    PhoneDisplayMenuLayout controlsTop = makePhoneDisplayMenuLayout(state);
    assert(controlsTop.title == "Controls");
    assert(controlsTop.selectableCount == 16);
    assert(controlsTop.rowCount == 19);
    assert(controlsTop.maxScroll > 0.0f);
    assert(!phoneDisplayHasMoreAbove(controlsTop));
    assert(phoneDisplayHasMoreBelow(controlsTop));
    assert(phoneDisplayScrollThumbFraction(controlsTop) < 1.0f);
    assert(phoneDisplayScrollProgress(controlsTop) == 0.0f);
    assert(controlsTop.rows[0].kind == PhoneMenuRowKind::Section);
    assert(controlsTop.rows[7].kind == PhoneMenuRowKind::Section);
    assert(controlsTop.rows[12].kind == PhoneMenuRowKind::Section);
    assert(selectionRow(controlsTop, 0).action == PhoneMenuAction::Rebind);
    expectVisibleRowsInsideSafe(controlsTop);

    state.hud.menuSelection = 15;
    state.localSettings.menuScroll = phoneDisplayScrollForSelection(controlsTop, 15);
    PhoneDisplayMenuLayout controlsBottom = makePhoneDisplayMenuLayout(state);
    assert(controlsBottom.scrollOffset > 0.0f);
    assert(phoneDisplayHasMoreAbove(controlsBottom));
    assert(!phoneDisplayHasMoreBelow(controlsBottom));
    assert(phoneDisplayScrollProgress(controlsBottom) > 0.99f);
    assert(selectionRow(controlsBottom, 15).action == PhoneMenuAction::Back);
    expectVisibleRowsInsideSafe(controlsBottom);

    state.localSettings.menuPage = LocalMenuPage::Audio;
    state.localSettings.menuScroll = 0.0f;
    PhoneMenuPageViewModel audioModel = makePhoneMenuPageModel(state);
    assert(audioModel.tablePage);
    assert(selectionElement(audioModel, 0).action == PhoneMenuAction::MusicVolume);
    assert(selectionElement(audioModel, 2).horizontal == PhoneMenuHorizontal::Toggle);
    PhoneDisplayMenuLayout audio = makePhoneDisplayMenuLayout(state);
    assert(audio.selectableCount == 5);
    assert(audio.maxScroll == 0.0f);
    assert(!phoneDisplayHasMoreAbove(audio));
    assert(!phoneDisplayHasMoreBelow(audio));
    assert(phoneDisplayScrollThumbFraction(audio) == 1.0f);
    expectVisibleRowsInsideSafe(audio);

    state.localSettings.menuPage = LocalMenuPage::Graphics;
    PhoneMenuPageViewModel graphicsModel = makePhoneMenuPageModel(state);
    assert(graphicsModel.tablePage);
    assert(selectionElement(graphicsModel, 0).action == PhoneMenuAction::GraphicsPreset);
    assert(selectionElement(graphicsModel, 1).horizontal == PhoneMenuHorizontal::Toggle);
    applyPhoneGraphicsPreset(state.localSettings, 0);
    assert(state.localSettings.graphicsPreset == 0 && !state.localSettings.shadows && !state.localSettings.portalWindow && !state.localSettings.particles);
    applyPhoneGraphicsPreset(state.localSettings, 1);
    assert(state.localSettings.graphicsPreset == 1 && !state.localSettings.shadows && state.localSettings.portalWindow && state.localSettings.particles);
    applyPhoneGraphicsPreset(state.localSettings, 2);
    assert(state.localSettings.graphicsPreset == 2 && state.localSettings.shadows && state.localSettings.portalWindow && state.localSettings.particles);
    PhoneDisplayMenuLayout graphics = makePhoneDisplayMenuLayout(state);
    assert(graphics.selectableCount == 5);
    expectVisibleRowsInsideSafe(graphics);

    state.localSettings.menuPage = LocalMenuPage::JoinCode;
    PhoneMenuPageViewModel joinModel = makePhoneMenuPageModel(state);
    assert(joinModel.selectableCount == 0);
    assert(joinModel.joinCode);
    PhoneDisplayMenuLayout join = makePhoneDisplayMenuLayout(state);
    assert(join.selectableCount == 0);
    assert(join.joinCode);

    state.dead = true;
    state.started = false;
    state.localSettings.menuPage = LocalMenuPage::Main;
    PhoneMenuPageViewModel deathModel = makePhoneMenuPageModel(state);
    assert(deathModel.selectableCount == 0);
    PhoneDisplayMenuLayout death = makePhoneDisplayMenuLayout(state);
    assert(death.selectableCount == 0);
    assert(death.rowCount == 0);

    return 0;
}
