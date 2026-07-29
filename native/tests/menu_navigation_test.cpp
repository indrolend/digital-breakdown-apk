#include <cassert>

#include "MenuNavigation.hpp"

int main() {
    using dbmenu::moveUpgradeGridSelection;
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
