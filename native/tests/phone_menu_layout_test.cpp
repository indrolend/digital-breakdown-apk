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
    assert(selectionElement(mainModel, 1).action == PhoneMenuAction::Online);
    assert(selectionElement(mainModel, 2).action == PhoneMenuAction::Settings);
    assert(selectionElement(mainModel, 3).action == PhoneMenuAction::Exit);
    PhoneMenuLayout main = makePhoneMenuLayout(state);
    assert(main.selectableCount == 4);
    expectRowsInsideSafe(main);

    state.started = true;
    state.uiPaused = true;
    state.multiplayer.enabled = false;
    state.upgradeMenu.active = false;
    state.localSettings.menuPage = LocalMenuPage::Main;
    PhoneMenuPageViewModel pauseModel = makePhoneMenuPageModel(state);
    assert(pauseModel.selectableCount == 5);
    assert(selectionElement(pauseModel, 0).action == PhoneMenuAction::Resume);
    assert(selectionElement(pauseModel, 4).action == PhoneMenuAction::ExitRun);
    PhoneMenuLayout pause = makePhoneMenuLayout(state);
    assert(pause.selectableCount == 5);
    expectRowsInsideSafe(pause);

    state.localSettings.menuPage = LocalMenuPage::Controls;
    state.localSettings.controlsPage = 0;
    PhoneMenuPageViewModel controlsOneModel = makePhoneMenuPageModel(state);
    assert(controlsOneModel.tablePage);
    assert(controlsOneModel.selectableCount == 8);
    assert(controlsOneModel.elements[0].kind == PhoneMenuRowKind::Section);
    assert(!controlsOneModel.elements[0].selectable);
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
    PhoneMenuPageViewModel controlsTwoModel = makePhoneMenuPageModel(state);
    assert(controlsTwoModel.tablePage);
    assert(controlsTwoModel.selectableCount == 9);
    assert(selectionElement(controlsTwoModel, 6).action == PhoneMenuAction::PreviousControls);
    assert(selectionElement(controlsTwoModel, 7).action == PhoneMenuAction::Defaults);
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
    PhoneMenuPageViewModel audioModel = makePhoneMenuPageModel(state);
    assert(audioModel.tablePage);
    assert(selectionElement(audioModel, 0).action == PhoneMenuAction::MusicVolume);
    assert(selectionElement(audioModel, 1).action == PhoneMenuAction::SfxVolume);
    PhoneMenuLayout audio = makePhoneMenuLayout(state);
    assert(audio.selectableCount == 5);
    expectRowsInsideSafe(audio);

    state.localSettings.menuPage = LocalMenuPage::Graphics;
    PhoneMenuPageViewModel graphicsModel = makePhoneMenuPageModel(state);
    assert(graphicsModel.tablePage);
    assert(selectionElement(graphicsModel, 0).action == PhoneMenuAction::GraphicsPreset);
    PhoneMenuLayout graphics = makePhoneMenuLayout(state);
    assert(graphics.selectableCount == 5);
    expectRowsInsideSafe(graphics);

    state.localSettings.menuPage = LocalMenuPage::JoinCode;
    PhoneMenuPageViewModel joinModel = makePhoneMenuPageModel(state);
    assert(joinModel.selectableCount == 0);
    assert(joinModel.joinCode);
    PhoneMenuLayout join = makePhoneMenuLayout(state);
    assert(join.selectableCount == 0);
    assert(join.joinCode);

    return 0;
}
