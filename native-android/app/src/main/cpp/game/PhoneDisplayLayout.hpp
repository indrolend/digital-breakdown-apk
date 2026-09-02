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
    PhoneMenuHorizontal horizontal = PhoneMenuHorizontal::None;
    std::string label;
    std::string value;
    PhoneDisplayRect visual;
    PhoneDisplayRect contentVisual;
    PhoneDisplayRect hit;
    float labelX = 0.0f;
    float valueRightX = 0.0f;
    float baselineY = 0.0f;
    float fontPx = 34.0f;
    bool selectable = false;
    bool visible = true;
    bool fixedFooter = false;
    bool peek = false;
};

struct PhoneDisplayMenuLayout {
    static constexpr int MaxRows = 24;
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
    float scrollOffset = 0.0f;
    float maxScroll = 0.0f;
    float contentHeight = 0.0f;
    float rowHeight = 0.0f;
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
    const float marginTop = h * 0.12f;
    const float marginBottom = h * 0.16f;
    layout.safe = {marginX, marginTop, w - marginX * 2.0f, h - marginTop - marginBottom};
    const float headerH = h * 0.16f;
    const float footerH = h * 0.12f;
    layout.header = {layout.safe.x, layout.safe.y, layout.safe.w, headerH};
    layout.footer = {layout.safe.x, layout.safe.y + layout.safe.h - footerH, layout.safe.w, footerH};
    layout.content = {layout.safe.x, layout.safe.y + headerH, layout.safe.w, layout.safe.h - headerH - footerH};
    layout.titleCenterY = layout.header.y + layout.header.h * 0.50f;
    layout.titlePx = page.paletteTitle ? 74.0f : 56.0f;
    layout.scrollOffset = std::max(0.0f, state.localSettings.menuScroll);

    for (int i = 0; i < page.elementCount; ++i) {
        const PhoneMenuElement& element = page.elements[i];
        PhoneDisplayMenuRow row;
        row.kind = element.kind;
        row.action = element.action;
        row.bindingAction = element.bindingAction;
        row.horizontal = element.horizontal;
        row.label = element.label;
        row.value = element.value;
        row.selectable = element.selectable;
        row.fontPx = element.kind == PhoneMenuRowKind::Section ? 32.0f : (page.tablePage ? 43.0f : 52.0f);
        addPhoneDisplayRow(layout, row);
    }

    if (layout.rowCount <= 0) return layout;
    const float rowH = page.tablePage ? 72.0f : 86.0f;
    const float preferredStep = page.tablePage ? 78.0f : 96.0f;
    layout.rowHeight = rowH;
    int scrollingRowCount = 0;
    for (int i = 0; i < layout.rowCount; ++i) {
        PhoneDisplayMenuRow& row = layout.rows[i];
        row.fixedFooter = row.action == PhoneMenuAction::Back;
        if (!row.fixedFooter) ++scrollingRowCount;
    }
    const float usedH = scrollingRowCount > 0
        ? preferredStep * static_cast<float>(scrollingRowCount - 1) + rowH
        : 0.0f;
    layout.contentHeight = usedH;
    layout.maxScroll = std::max(0.0f, usedH - layout.content.h);
    layout.scrollOffset = std::min(layout.scrollOffset, layout.maxScroll);
    const float firstContentCenterY = rowH * 0.5f;
    const float shortPageOffset = layout.maxScroll <= 0.0f ? (layout.content.h - usedH) * 0.5f : 0.0f;
    int scrollingRowIndex = 0;
    for (int i = 0; i < layout.rowCount; ++i) {
        PhoneDisplayMenuRow& row = layout.rows[i];
        if (row.fixedFooter) {
            row.visual = {layout.safe.x, layout.footer.y + (layout.footer.h - rowH) * 0.5f,
                          layout.safe.w, rowH};
            row.contentVisual = row.visual;
            row.hit = row.visual;
            row.visible = true;
            row.baselineY = row.visual.y + row.visual.h * 0.5f + row.fontPx * 0.34f;
            row.labelX = page.tablePage ? layout.safe.x + layout.safe.w * 0.08f : layout.safe.x + layout.safe.w * 0.38f;
            row.valueRightX = row.visual.x + row.visual.w * 0.92f;
            continue;
        }
        const float contentCy = firstContentCenterY + static_cast<float>(scrollingRowIndex++) * preferredStep;
        const float cy = layout.content.y + shortPageOffset + contentCy - layout.scrollOffset;
        row.contentVisual = {layout.safe.x, contentCy - rowH * 0.5f, layout.safe.w, rowH};
        row.visual = {layout.safe.x, cy - rowH * 0.5f, layout.safe.w, rowH};
        const float clippedTop = std::max(row.visual.y, layout.content.y);
        const float clippedBottom = std::min(row.visual.y + row.visual.h, layout.content.y + layout.content.h);
        const float visibleHeight = std::max(0.0f, clippedBottom - clippedTop);
        row.visible = visibleHeight > 0.0f;
        row.peek = row.visible && visibleHeight + 0.5f < row.visual.h;
        row.hit = row.selectable && row.visible && !row.peek
            ? row.visual
            : PhoneDisplayRect{};
        row.baselineY = cy + row.fontPx * 0.34f;
        row.labelX = page.tablePage ? layout.safe.x + layout.safe.w * 0.08f : layout.safe.x + layout.safe.w * 0.38f;
        row.valueRightX = layout.safe.x + layout.safe.w * 0.92f;
        if (row.kind == PhoneMenuRowKind::Section) {
            row.labelX = layout.safe.x + layout.safe.w * 0.08f;
            row.hit = {};
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

inline float phoneDisplayScrollForSelection(const PhoneDisplayMenuLayout& layout, int selection) {
    const PhoneDisplayMenuRow* row = phoneDisplayRowForSelection(layout, selection);
    if (!row) return layout.scrollOffset;
    if (row->fixedFooter) return layout.scrollOffset;
    float next = layout.scrollOffset;
    const float rowTop = row->contentVisual.y;
    const float rowBottom = row->contentVisual.y + row->contentVisual.h;
    const float margin = layout.rowHeight * 0.30f;
    if (rowTop < next + margin) next = rowTop - margin;
    if (rowBottom > next + layout.content.h - margin) next = rowBottom - layout.content.h + margin;
    return std::max(0.0f, std::min(layout.maxScroll, next));
}

inline bool phoneDisplayHasMoreAbove(const PhoneDisplayMenuLayout& layout) {
    return layout.maxScroll > 0.0f && layout.scrollOffset > 0.5f;
}

inline bool phoneDisplayHasMoreBelow(const PhoneDisplayMenuLayout& layout) {
    return layout.maxScroll > 0.0f && layout.scrollOffset < layout.maxScroll - 0.5f;
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
