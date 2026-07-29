#pragma once

#include <algorithm>

namespace dbmenu {

inline int moveUpgradeGridSelection(int current, int horizontal, int vertical) {
    current = std::max(0, std::min(5, current));
    int row = current / 3;
    int column = current % 3;
    if (horizontal != 0) column = (column + (horizontal > 0 ? 1 : 2)) % 3;
    if (vertical != 0) row = 1 - row;
    return row * 3 + column;
}

} // namespace dbmenu
