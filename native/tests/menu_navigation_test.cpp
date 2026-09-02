#include <cassert>

#include "MenuNavigation.hpp"

int main() {
    using dbmenu::moveUpgradeGridSelection;
    dbmenu::MenuRepeatState repeat;
    assert(dbmenu::menuRepeatMove(repeat, 1, 10.0));
    assert(!dbmenu::menuRepeatMove(repeat, 1, 10.34));
    assert(dbmenu::menuRepeatMove(repeat, 1, 10.35));
    assert(!dbmenu::menuRepeatMove(repeat, 1, 10.43));
    assert(dbmenu::menuRepeatMove(repeat, 1, 10.44));
    assert(dbmenu::menuRepeatMove(repeat, -1, 10.45));
    assert(!dbmenu::menuRepeatMove(repeat, -1, 10.79));
    assert(dbmenu::menuRepeatMove(repeat, -1, 10.80));
    assert(!dbmenu::menuRepeatMove(repeat, 0, 10.81));
    assert(repeat.direction == 0);
    assert(dbmenu::menuRepeatMove(repeat, 1, 10.82));
    assert(dbmenu::pointerAction(0, 0, 1) == dbmenu::PointerAction::Activate);
    assert(dbmenu::pointerAction(1, 0, 1) == dbmenu::PointerAction::Back);
    assert(dbmenu::pointerAction(2, 0, 1) == dbmenu::PointerAction::None);
    assert(dbmenu::wheelSelection(0, 16, 1) == 1);
    assert(dbmenu::wheelSelection(1, 16, -1) == 0);
    assert(dbmenu::wheelSelection(0, 16, -1) == 0);
    assert(dbmenu::wheelSelection(15, 16, 1) == 15);
    assert(dbmenu::wheelSelection(3, 0, 1) == 3);
    const auto deep = dbmenu::controllerTriggerThresholds(0);
    const auto balanced = dbmenu::controllerTriggerThresholds(1);
    const auto hair = dbmenu::controllerTriggerThresholds(2);
    assert(deep.left == 0.55f && deep.right == 0.40f);
    assert(balanced.left == 0.35f && balanced.right == 0.20f);
    assert(hair.left == 0.16f && hair.right == 0.08f);
    assert(dbmenu::controllerTriggerThresholds(-1).left == deep.left);
    assert(dbmenu::controllerTriggerThresholds(99).right == hair.right);
    assert(moveUpgradeGridSelection(0, 1, 0) == 1);
    assert(moveUpgradeGridSelection(1, 1, 0) == 2);
    assert(moveUpgradeGridSelection(2, 1, 0) == 0);
    assert(moveUpgradeGridSelection(0, -1, 0) == 2);
    assert(moveUpgradeGridSelection(2, -1, 0) == 1);
    assert(moveUpgradeGridSelection(0, 0, 1) == 3);
    assert(moveUpgradeGridSelection(1, 0, 1) == 4);
    assert(moveUpgradeGridSelection(5, 0, -1) == 2);
    assert(moveUpgradeGridSelection(3, 0, -1) == 0);
    assert(moveUpgradeGridSelection(-4, 1, 0) == 1);
    assert(moveUpgradeGridSelection(99, 0, -1) == 2);
    return 0;
}
