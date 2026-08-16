#pragma once

#include <algorithm>

namespace dbmenu {

enum class PointerAction {
    None,
    Activate,
    Back,
};

inline PointerAction pointerAction(int button, int primaryButton, int secondaryButton) {
    if (button == primaryButton) return PointerAction::Activate;
    if (button == secondaryButton) return PointerAction::Back;
    return PointerAction::None;
}

inline int wheelSelection(int current, int count, int direction) {
    if (count <= 0 || direction == 0) return current;
    return std::max(0, std::min(count - 1, current + (direction > 0 ? 1 : -1)));
}

struct TriggerThresholds {
    float left;
    float right;
};

inline TriggerThresholds controllerTriggerThresholds(int sensitivity) {
    static constexpr TriggerThresholds Thresholds[] = {
        {0.55f, 0.40f},
        {0.35f, 0.20f},
        {0.16f, 0.08f},
    };
    return Thresholds[std::max(0, std::min(2, sensitivity))];
}

inline int moveUpgradeGridSelection(int current, int horizontal, int vertical) {
    current = std::max(0, std::min(5, current));
    int row = current / 3;
    int column = current % 3;
    if (horizontal != 0) column = (column + (horizontal > 0 ? 1 : 2)) % 3;
    if (vertical != 0) row = 1 - row;
    return row * 3 + column;
}

} // namespace dbmenu
