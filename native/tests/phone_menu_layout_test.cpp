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

void expectRowsInsideSafe(const PhoneDisplayMenuLayout& layout) {
    assert(layout.logicalW == PhoneDisplayState::LogicalWidth);
    assert(layout.logicalH == PhoneDisplayState::LogicalHeight);
    assert(layout.safe.x > 0.0f);
    assert(layout.safe.y > 0.0f);
    assert(layout.safe.x + layout.safe.w < static_cast<float>(layout.logicalW));
    assert(layout.safe.y + layout.safe.h < static_cast<float>(layout.logicalH));
    for (int i = 0; i < layout.rowCount; ++i) {
        const PhoneDisplayMenuRow& row = layout.rows[i];
        expectRectInside(layout.safe, row.visual);
        assert(std::isfinite(row.baselineY));
        assert(row.baselineY >= layout.safe.y);
        assert(row.baselineY <= layout.safe.y + layout.safe.h);
        if (row.kind == PhoneMenuRowKind::Section) {
            assert(!row.selectable);
            assert(row.selectableIndex < 0);
            assert(row.hit.w == 0.0f && row.hit.h == 0.0f);
        } else {
            assert(row.selectable);
            assert(row.selectableIndex >= 0);
            expectRectInside(layout.safe, row.hit);
        }
    }
}

const PhoneDisplayMenuRow& selectionRow(const PhoneDisplayMenuLayout& layout, int selection) {
    const PhoneDisplayMenuRow* row = phoneDisplayRowForSelection(layout, selection);
    assert(row);
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
    assert(selectionElement(mainModel, 1).action == PhoneMenuAction::Online);
    assert(selectionElement(mainModel, 2).action == PhoneMenuAction::Settings);
    assert(selectionElement(mainModel, 3).action == PhoneMenuAction::Exit);
    PhoneDisplayMenuLayout main = makePhoneDisplayMenuLayout(state);
    assert(main.selectableCount == 4);
    expectRowsInsideSafe(main);
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
    expectRowsInsideSafe(pause);

    state.localSettings.menuPage = LocalMenuPage::Controls;
    state.localSettings.controlsPage = 0;
    PhoneMenuPageViewModel controlsOneModel = makePhoneMenuPageModel(state);
    assert(controlsOneModel.tablePage);
    assert(controlsOneModel.selectableCount == 8);
    assert(controlsOneModel.elements[0].kind == PhoneMenuRowKind::Section);
    assert(!controlsOneModel.elements[0].selectable);
    PhoneDisplayMenuLayout controlsOne = makePhoneDisplayMenuLayout(state);
    assert(controlsOne.title == "Controls 1/2");
    assert(controlsOne.selectableCount == 8);
    assert(controlsOne.rowCount == 9);
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
    assert(selectionElement(controlsTwoModel, 5).action == PhoneMenuAction::AdjustController);
    assert(selectionElement(controlsTwoModel, 6).action == PhoneMenuAction::PreviousControls);
    assert(selectionElement(controlsTwoModel, 7).action == PhoneMenuAction::Defaults);
    PhoneDisplayMenuLayout controlsTwo = makePhoneDisplayMenuLayout(state);
    assert(controlsTwo.title == "Controls 2/2");
    assert(controlsTwo.selectableCount == 9);
    assert(controlsTwo.rowCount == 11);
    assert(controlsTwo.rows[0].kind == PhoneMenuRowKind::Section);
    assert(controlsTwo.rows[5].kind == PhoneMenuRowKind::Section);
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
    PhoneDisplayMenuLayout audio = makePhoneDisplayMenuLayout(state);
    assert(audio.selectableCount == 5);
    expectRowsInsideSafe(audio);

    state.localSettings.menuPage = LocalMenuPage::Graphics;
    PhoneMenuPageViewModel graphicsModel = makePhoneMenuPageModel(state);
    assert(graphicsModel.tablePage);
    assert(selectionElement(graphicsModel, 0).action == PhoneMenuAction::GraphicsPreset);
    PhoneDisplayMenuLayout graphics = makePhoneDisplayMenuLayout(state);
    assert(graphics.selectableCount == 5);
    expectRowsInsideSafe(graphics);

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
    assert(deathModel.selectableCount == 1);
    assert(selectionElement(deathModel, 0).label == "Again?");
    assert(selectionElement(deathModel, 0).action == PhoneMenuAction::Restart);
    PhoneDisplayMenuLayout death = makePhoneDisplayMenuLayout(state);
    assert(death.selectableCount == 1);
    assert(selectionRow(death, 0).action == PhoneMenuAction::Restart);
    expectRowsInsideSafe(death);

    return 0;
}
