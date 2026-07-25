#include <cassert>
#include <cmath>

#include "Game.hpp"
#include "PhoneMenuLayout.hpp"

namespace {

void expectRowsInsideSafe(const PhoneMenuLayout& layout) {
    const float safeLeft = layout.safe.x - layout.safe.w * 0.5f;
    const float safeRight = layout.safe.x + layout.safe.w * 0.5f;
    const float safeBottom = layout.safe.y - layout.safe.h * 0.5f;
    const float safeTop = layout.safe.y + layout.safe.h * 0.5f;
    for (int i = 0; i < layout.rowCount; ++i) {
        const PhoneMenuRow& row = layout.rows[i];
        const float left = row.visual.x - row.visual.w * 0.5f;
        const float right = row.visual.x + row.visual.w * 0.5f;
        const float bottom = row.visual.y - row.visual.h * 0.5f;
        const float top = row.visual.y + row.visual.h * 0.5f;
        assert(left >= safeLeft - 0.00001f);
        assert(right <= safeRight + 0.00001f);
        assert(bottom >= safeBottom - 0.00001f);
        assert(top <= safeTop + 0.00001f);
        if (row.kind == PhoneMenuRowKind::Section) {
            assert(!row.selectable);
            assert(row.selectableIndex < 0);
        }
    }
}

const PhoneMenuRow& selectionRow(const PhoneMenuLayout& layout, int selection) {
    const PhoneMenuRow* row = phoneMenuRowForSelection(layout, selection);
    assert(row);
    return *row;
}

} // namespace

int main() {
    GameState state;
    state.phoneVisual.screenScale = {1.0f, 1.0f, 1.0f};

    state.started = false;
    state.dead = false;
    state.localSettings.menuPage = LocalMenuPage::Main;
    PhoneMenuLayout main = makePhoneMenuLayout(state);
    assert(main.selectableCount == 4);
    expectRowsInsideSafe(main);

    state.started = true;
    state.uiPaused = true;
    state.multiplayer.enabled = false;
    state.upgradeMenu.active = false;
    state.localSettings.menuPage = LocalMenuPage::Main;
    PhoneMenuLayout pause = makePhoneMenuLayout(state);
    assert(pause.selectableCount == 5);
    expectRowsInsideSafe(pause);

    state.localSettings.menuPage = LocalMenuPage::Controls;
    state.localSettings.controlsPage = 0;
    PhoneMenuLayout controlsOne = makePhoneMenuLayout(state);
    assert(controlsOne.title == "Controls 1/2");
    assert(controlsOne.selectableCount == 8);
    assert(selectionRow(controlsOne, 0).action == PhoneMenuAction::Rebind);
    assert(selectionRow(controlsOne, 0).bindingAction == 0);
    assert(selectionRow(controlsOne, 5).bindingAction == 5);
    assert(selectionRow(controlsOne, 6).action == PhoneMenuAction::NextControls);
    assert(selectionRow(controlsOne, 7).action == PhoneMenuAction::Back);
    expectRowsInsideSafe(controlsOne);

    state.localSettings.controlsPage = 1;
    PhoneMenuLayout controlsTwo = makePhoneMenuLayout(state);
    assert(controlsTwo.title == "Controls 2/2");
    assert(controlsTwo.selectableCount == 9);
    assert(selectionRow(controlsTwo, 0).bindingAction == 6);
    assert(selectionRow(controlsTwo, 3).bindingAction == 9);
    assert(selectionRow(controlsTwo, 4).action == PhoneMenuAction::AdjustMouse);
    assert(selectionRow(controlsTwo, 5).action == PhoneMenuAction::AdjustController);
    assert(selectionRow(controlsTwo, 6).action == PhoneMenuAction::PreviousControls);
    assert(selectionRow(controlsTwo, 7).action == PhoneMenuAction::Defaults);
    assert(selectionRow(controlsTwo, 8).action == PhoneMenuAction::Back);
    expectRowsInsideSafe(controlsTwo);

    state.localSettings.menuPage = LocalMenuPage::Audio;
    PhoneMenuLayout audio = makePhoneMenuLayout(state);
    assert(audio.selectableCount == 5);
    expectRowsInsideSafe(audio);

    state.localSettings.menuPage = LocalMenuPage::Graphics;
    PhoneMenuLayout graphics = makePhoneMenuLayout(state);
    assert(graphics.selectableCount == 5);
    expectRowsInsideSafe(graphics);

    state.localSettings.menuPage = LocalMenuPage::JoinCode;
    PhoneMenuLayout join = makePhoneMenuLayout(state);
    assert(join.selectableCount == 0);
    assert(join.joinCode);

    return 0;
}
