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
        if (row.visible && !row.peek) expectRectInside(layout.safe, row.hit);
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
    static_assert(!PhoneMenuMultiplayerAvailable);
    assert(mainModel.selectableCount == 3);
    assert(selectionElement(mainModel, 0).action == PhoneMenuAction::Solo);
    assert(selectionElement(mainModel, 0).label == "Play");
    assert(selectionElement(mainModel, 2).action == PhoneMenuAction::Exit);
    for (int i = 0; i < mainModel.selectableCount; ++i) assert(selectionElement(mainModel, i).action != PhoneMenuAction::Online);
    PhoneDisplayMenuLayout main = makePhoneDisplayMenuLayout(state);
    assert(main.selectableCount == 3);
    expectVisibleRowsInsideSafe(main);
    assert(selectionRow(main, 0).action == PhoneMenuAction::Solo);
    assert(selectionRow(main, 0).label == "Play");

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
    assert(controlsModel.selectableCount == 15);
    assert(controlsModel.elementCount == 18);
    assert(controlsModel.elements[0].kind == PhoneMenuRowKind::Section);
    assert(!controlsModel.elements[0].selectable);
    assert(selectionElement(controlsModel, 0).bindingAction == 0);
    assert(selectionElement(controlsModel, 0).horizontal == PhoneMenuHorizontal::None);
    assert(selectionElement(controlsModel, 5).bindingAction == 5);
    assert(selectionElement(controlsModel, 6).bindingAction == 6);
    assert(selectionElement(controlsModel, 8).bindingAction == 8);
    assert(selectionElement(controlsModel, 9).action == PhoneMenuAction::AdjustMouse);
    assert(selectionElement(controlsModel, 10).action == PhoneMenuAction::AdjustController);
    assert(selectionElement(controlsModel, 11).action == PhoneMenuAction::AdjustTriggers);
    assert(selectionElement(controlsModel, 11).value == "Balanced");
    assert(selectionElement(controlsModel, 12).action == PhoneMenuAction::AdjustVibration);
    assert(selectionElement(controlsModel, 12).value == "Standard");
    assert(selectionElement(controlsModel, 12).horizontal == PhoneMenuHorizontal::Adjust);
    assert(selectionElement(controlsModel, 13).action == PhoneMenuAction::Defaults);
    assert(selectionElement(controlsModel, 14).action == PhoneMenuAction::Back);

    PhoneDisplayMenuLayout controlsTop = makePhoneDisplayMenuLayout(state);
    assert(controlsTop.title == "Controls");
    assert(controlsTop.selectableCount == 15);
    assert(controlsTop.rowCount == 18);
    assert(controlsTop.maxScroll > 0.0f);
    assert(!phoneDisplayHasMoreAbove(controlsTop));
    assert(phoneDisplayHasMoreBelow(controlsTop));
    assert(controlsTop.rows[0].kind == PhoneMenuRowKind::Section);
    assert(controlsTop.rows[7].kind == PhoneMenuRowKind::Section);
    assert(controlsTop.rows[11].kind == PhoneMenuRowKind::Section);
    assert(selectionRow(controlsTop, 0).action == PhoneMenuAction::Rebind);
    assert(selectionRow(controlsTop, 0).horizontal == PhoneMenuHorizontal::None);
    assert(phoneDisplayRowForSelection(controlsTop, 14)->fixedFooter);
    bool hasBottomPeek = false;
    for (int i = 0; i < controlsTop.rowCount; ++i) {
        const PhoneDisplayMenuRow& row = controlsTop.rows[i];
        if (row.peek && row.visual.y + row.visual.h > controlsTop.content.y + controlsTop.content.h) hasBottomPeek = true;
    }
    assert(hasBottomPeek);
    expectVisibleRowsInsideSafe(controlsTop);

    state.hud.menuSelection = 13;
    state.localSettings.menuScroll = phoneDisplayScrollForSelection(controlsTop, 13);
    PhoneDisplayMenuLayout controlsBottom = makePhoneDisplayMenuLayout(state);
    assert(controlsBottom.scrollOffset > 0.0f);
    assert(phoneDisplayHasMoreAbove(controlsBottom));
    assert(!phoneDisplayHasMoreBelow(controlsBottom));
    assert(selectionRow(controlsBottom, 13).action == PhoneMenuAction::Defaults);
    assert(selectionRow(controlsBottom, 14).action == PhoneMenuAction::Back);
    assert(phoneDisplayScrollForSelection(controlsBottom, 14) == controlsBottom.scrollOffset);
    expectVisibleRowsInsideSafe(controlsBottom);

    state.localSettings.menuPage = LocalMenuPage::Audio;
    state.localSettings.menuScroll = 0.0f;
    PhoneMenuPageViewModel audioModel = makePhoneMenuPageModel(state);
    assert(audioModel.tablePage);
    assert(selectionElement(audioModel, 0).action == PhoneMenuAction::MusicVolume);
    assert(selectionElement(audioModel, 0).horizontal == PhoneMenuHorizontal::Adjust);
    assert(selectionElement(audioModel, 2).horizontal == PhoneMenuHorizontal::Toggle);
    PhoneDisplayMenuLayout audio = makePhoneDisplayMenuLayout(state);
    assert(audio.selectableCount == 5);
    assert(selectionRow(audio, 2).label == "Music");
    assert(selectionRow(audio, 2).value == "On");
    assert(selectionRow(audio, 3).label == "Sound Effects");
    assert(audio.maxScroll == 0.0f);
    assert(!phoneDisplayHasMoreAbove(audio));
    assert(!phoneDisplayHasMoreBelow(audio));
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
    assert(selectionRow(graphics, 1).label == "Shadows");
    assert(selectionRow(graphics, 1).value == "On");
    assert(selectionRow(graphics, 3).label == "Frame Rate");
    expectVisibleRowsInsideSafe(graphics);

    assert(phoneMenuCycleIndex(0, 1, 3) == 1);
    assert(phoneMenuCycleIndex(2, 1, 3) == 0);
    assert(phoneMenuCycleIndex(0, -1, 3) == 2);
    assert(phoneMenuCycleFloat(1.0f, 1, 0.0f, 1.0f, 0.1f) == 0.0f);
    assert(phoneMenuCycleFloat(0.0f, -1, 0.0f, 1.0f, 0.1f) == 1.0f);

    state.localSettings.menuPage = LocalMenuPage::JoinCode;
    PhoneMenuPageViewModel joinModel = makePhoneMenuPageModel(state);
    assert(joinModel.selectableCount == 1);
    assert(joinModel.joinCode);
    PhoneDisplayMenuLayout join = makePhoneDisplayMenuLayout(state);
    assert(join.selectableCount == 1);
    assert(join.joinCode);
    assert(selectionRow(join, 0).fixedFooter);

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
