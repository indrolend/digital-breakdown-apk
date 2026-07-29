#include <cassert>

#include "MenuNavigation.hpp"

int main() {
    using dbmenu::moveUpgradeGridSelection;
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
