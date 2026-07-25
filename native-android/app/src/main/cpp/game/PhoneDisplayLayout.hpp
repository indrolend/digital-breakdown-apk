#pragma once

#include "PhoneDisplay.hpp"
#include "PhoneMenuModel.hpp"

#include <algorithm>
#include <array>
#include <string>

struct PhoneDisplayRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct PhoneDisplayMenuRow {
    PhoneMenuRowKind kind = PhoneMenuRowKind::Item;
    PhoneMenuAction action = PhoneMenuAction::None;
    int selectableIndex = -1;
    int bindingAction = -1;
    std::string label;
    std::string value;
    PhoneDisplayRect visual;
    PhoneDisplayRect hit;
    float labelX = 0.0f;
    float valueRightX = 0.0f;
    float baselineY = 0.0f;
    float fontPx = 34.0f;
    bool selectable = false;
};

struct PhoneDisplayMenuLayout {
    static constexpr int MaxRows = 16;
    int logicalW = PhoneDisplayState::LogicalWidth;
    int logicalH = PhoneDisplayState::LogicalHeight;
    PhoneDisplayRect safe;
    PhoneDisplayRect header;
    PhoneDisplayRect content;
    PhoneDisplayRect footer;
    std::string title;
    float titleCenterY = 0.0f;
    float titlePx = 52.0f;
    std::array<PhoneDisplayMenuRow, MaxRows> rows{};
    int rowCount = 0;
    int selectableCount = 0;
    bool paletteTitle = false;
    bool joinCode = false;
    bool tablePage = false;
};

inline void addPhoneDisplayRow(PhoneDisplayMenuLayout& layout, PhoneDisplayMenuRow row) {
    if (layout.rowCount >= PhoneDisplayMenuLayout::MaxRows) return;
    if (row.selectable) row.selectableIndex = layout.selectableCount++;
    layout.rows[layout.rowCount++] = row;
}

inline PhoneDisplayMenuLayout makePhoneDisplayMenuLayout(const GameState& state) {
    const PhoneMenuPageViewModel page = makePhoneMenuPageModel(state);
    PhoneDisplayMenuLayout layout;
    layout.title = page.title;
    layout.paletteTitle = page.paletteTitle;
    layout.joinCode = page.joinCode;
    layout.tablePage = page.tablePage;

    const float w = static_cast<float>(layout.logicalW);
    const float h = static_cast<float>(layout.logicalH);
    const float marginX = w * 0.12f;
    const float marginTop = h * 0.10f;
    const float marginBottom = h * 0.10f;
    layout.safe = {marginX, marginTop, w - marginX * 2.0f, h - marginTop - marginBottom};
    const float headerH = h * 0.16f;
    const float footerH = h * 0.12f;
    layout.header = {layout.safe.x, layout.safe.y, layout.safe.w, headerH};
    layout.footer = {layout.safe.x, layout.safe.y + layout.safe.h - footerH, layout.safe.w, footerH};
    layout.content = {layout.safe.x, layout.safe.y + headerH, layout.safe.w, layout.safe.h - headerH - footerH};
    layout.titleCenterY = layout.header.y + layout.header.h * 0.50f;
    layout.titlePx = page.paletteTitle ? 62.0f : 48.0f;

    for (int i = 0; i < page.elementCount; ++i) {
        const PhoneMenuElement& element = page.elements[i];
        PhoneDisplayMenuRow row;
        row.kind = element.kind;
        row.action = element.action;
        row.bindingAction = element.bindingAction;
        row.label = element.label;
        row.value = element.value;
        row.selectable = element.selectable;
        row.fontPx = element.kind == PhoneMenuRowKind::Section ? 25.0f : (page.tablePage ? 34.0f : 42.0f);
        addPhoneDisplayRow(layout, row);
    }

    if (layout.rowCount <= 0) return layout;
    const float rowH = page.tablePage ? 58.0f : 72.0f;
    const float preferredStep = page.tablePage ? 62.0f : 86.0f;
    const float usableStep = layout.rowCount > 1
        ? std::min(preferredStep, (layout.content.h - rowH) / static_cast<float>(layout.rowCount - 1))
        : preferredStep;
    const float usedH = usableStep * static_cast<float>(layout.rowCount - 1) + rowH;
    const float firstCenterY = layout.content.y + (layout.content.h - usedH) * 0.5f + rowH * 0.5f;
    for (int i = 0; i < layout.rowCount; ++i) {
        PhoneDisplayMenuRow& row = layout.rows[i];
        const float cy = firstCenterY + static_cast<float>(i) * usableStep;
        row.visual = {layout.safe.x, cy - rowH * 0.5f, layout.safe.w, rowH};
        row.hit = row.selectable ? row.visual : PhoneDisplayRect{};
        row.baselineY = cy + row.fontPx * 0.34f;
        row.labelX = page.tablePage ? layout.safe.x + layout.safe.w * 0.06f : layout.safe.x + layout.safe.w * 0.38f;
        row.valueRightX = layout.safe.x + layout.safe.w * 0.94f;
        if (row.kind == PhoneMenuRowKind::Section) {
            row.labelX = layout.safe.x + layout.safe.w * 0.06f;
            row.hit = {};
        }
        if (state.dead && row.action == PhoneMenuAction::Restart) {
            row.labelX = layout.logicalW * 0.5f;
            row.visual = {layout.safe.x, layout.logicalH * 0.5f - rowH * 0.5f, layout.safe.w, rowH};
            row.hit = row.visual;
            row.baselineY = layout.logicalH * 0.5f + row.fontPx * 0.34f;
        }
    }
    return layout;
}

inline const PhoneDisplayMenuRow* phoneDisplayRowForSelection(const PhoneDisplayMenuLayout& layout, int selection) {
    for (int i = 0; i < layout.rowCount; ++i) {
        if (layout.rows[i].selectable && layout.rows[i].selectableIndex == selection) return &layout.rows[i];
    }
    return nullptr;
}

inline int phoneDisplayItemAt(const PhoneDisplayMenuLayout& layout, float x, float y) {
    for (int i = 0; i < layout.rowCount; ++i) {
        const PhoneDisplayMenuRow& row = layout.rows[i];
        if (!row.selectable) continue;
        if (x >= row.hit.x && x <= row.hit.x + row.hit.w && y >= row.hit.y && y <= row.hit.y + row.hit.h) {
            return row.selectableIndex;
        }
    }
    return -1;
}
