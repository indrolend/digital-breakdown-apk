#pragma once

#include "PhoneMenuModel.hpp"

#include <algorithm>
#include <array>
#include <string>

struct PhoneMenuTokens {
    float horizontalSafeMargin = 0.11f;
    float topSafeMargin = 0.095f;
    float bottomSafeMargin = 0.095f;
    float titleZone = 0.135f;
    float footerZone = 0.12f;
    float panelInset = 0.04f;
    float titleScale = 0.0240f;
    float itemScale = 0.0158f;
    float metadataScale = 0.0104f;
    float sectionScale = 0.0087f;
    float majorSpacing = 0.030f;
    float rowSpacing = 0.086f;
    float denseRowSpacing = 0.063f;
    float rowHeight = 0.062f;
    float denseRowHeight = 0.044f;
    float dividerThickness = 0.004f;
    float selectedFillOpacity = 0.0f;
    float inactiveFillOpacity = 0.0f;
    float inactiveTextIntensity = 0.72f;
    float activeTextIntensity = 0.96f;
};

struct PhoneMenuRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct PhoneMenuRow {
    PhoneMenuRowKind kind = PhoneMenuRowKind::Item;
    PhoneMenuAction action = PhoneMenuAction::None;
    int selectableIndex = -1;
    int bindingAction = -1;
    std::string label;
    std::string value;
    PhoneMenuRect visual;
    PhoneMenuRect hit;
    float labelX = 0.0f;
    float valueRightX = 0.0f;
    float textY = 0.0f;
    float scale = 0.0f;
    bool selectable = false;
};

struct PhoneMenuLayout {
    static constexpr int MaxRows = 16;
    PhoneMenuTokens tokens;
    float screenW = PHONE_SCREEN_WIDTH;
    float screenH = PHONE_SCREEN_HEIGHT;
    PhoneMenuRect panel;
    PhoneMenuRect safe;
    PhoneMenuRect header;
    PhoneMenuRect content;
    PhoneMenuRect footer;
    std::string title;
    float titleY = 0.0f;
    float titleScale = 0.0f;
    float labelX = 0.0f;
    float valueRightX = 0.0f;
    std::array<PhoneMenuRow, MaxRows> rows{};
    int rowCount = 0;
    int selectableCount = 0;
    bool paletteTitle = false;
    bool joinCode = false;
};

inline float phoneMenuFitScale(float preferred, float maxWidth, const std::string& text) {
    if (text.empty()) return preferred;
    return std::min(preferred, maxWidth / (static_cast<float>(text.size()) * 6.0f));
}

inline void addPhoneMenuRow(PhoneMenuLayout& layout, PhoneMenuRow row) {
    if (layout.rowCount >= PhoneMenuLayout::MaxRows) return;
    if (row.selectable) row.selectableIndex = layout.selectableCount++;
    layout.rows[layout.rowCount++] = row;
}

inline void addPhoneMenuLayoutSection(PhoneMenuLayout& layout, const std::string& label) {
    PhoneMenuRow row;
    row.kind = PhoneMenuRowKind::Section;
    row.label = label;
    row.scale = layout.screenW * layout.tokens.sectionScale;
    addPhoneMenuRow(layout, row);
}

inline void addPhoneMenuLayoutItem(PhoneMenuLayout& layout, const std::string& label, PhoneMenuAction action, int bindingAction = -1) {
    PhoneMenuRow row;
    row.kind = PhoneMenuRowKind::Item;
    row.action = action;
    row.bindingAction = bindingAction;
    row.label = label;
    row.scale = layout.screenW * layout.tokens.itemScale;
    row.selectable = true;
    addPhoneMenuRow(layout, row);
}

inline void addPhoneMenuLayoutValue(PhoneMenuLayout& layout, const std::string& label, const std::string& value, PhoneMenuAction action, int bindingAction = -1) {
    PhoneMenuRow row;
    row.kind = PhoneMenuRowKind::TwoColumn;
    row.action = action;
    row.bindingAction = bindingAction;
    row.label = label;
    row.value = value;
    row.scale = layout.screenW * layout.tokens.metadataScale;
    row.selectable = true;
    addPhoneMenuRow(layout, row);
}

inline void finishPhoneMenuRows(PhoneMenuLayout& layout, bool tablePage) {
    if (layout.rowCount <= 0) return;
    const float rowStep = layout.screenH * (tablePage ? layout.tokens.denseRowSpacing : layout.tokens.rowSpacing);
    const float rowH = layout.screenH * (tablePage ? layout.tokens.denseRowHeight : layout.tokens.rowHeight);
    const float maxBlockH = layout.content.h;
    const float usableStep = layout.rowCount > 1 ? std::min(rowStep, (maxBlockH - rowH) / static_cast<float>(layout.rowCount - 1)) : rowStep;
    const float usedBlockH = usableStep * static_cast<float>(layout.rowCount - 1) + rowH;
    const float firstY = layout.content.y + usedBlockH * 0.5f - rowH * 0.50f;
    for (int i = 0; i < layout.rowCount; ++i) {
        PhoneMenuRow& row = layout.rows[i];
        const float y = firstY - static_cast<float>(i) * usableStep;
        row.visual = {0.0f, y - rowH * 0.34f, layout.safe.w, rowH};
        row.hit = row.visual;
        row.textY = y;
        row.labelX = tablePage ? layout.labelX : 0.0f;
        row.valueRightX = layout.valueRightX;
        if (tablePage && row.kind == PhoneMenuRowKind::Item) {
            row.scale = phoneMenuFitScale(layout.screenW * layout.tokens.metadataScale, layout.safe.w * 0.74f, row.label);
        } else if (row.kind == PhoneMenuRowKind::TwoColumn) {
            const int totalChars = static_cast<int>(row.label.size() + row.value.size() + 1u);
            row.scale = phoneMenuFitScale(row.scale, layout.safe.w * 0.92f, std::string(static_cast<std::size_t>(std::max(1, totalChars)), 'X'));
        }
        if (row.kind == PhoneMenuRowKind::Section) {
            row.visual.w = layout.safe.w * 0.92f;
            row.hit.w = 0.0f;
            row.hit.h = 0.0f;
        }
    }
}

inline PhoneMenuLayout makePhoneMenuLayout(const GameState& state) {
    const PhoneMenuPageViewModel page = makePhoneMenuPageModel(state);
    PhoneMenuLayout layout;
    layout.screenW = PHONE_SCREEN_WIDTH * std::max(0.65f, state.phoneVisual.screenScale.x);
    layout.screenH = PHONE_SCREEN_HEIGHT * std::max(0.65f, state.phoneVisual.screenScale.y);
    layout.panel = {0.0f, 0.0f, layout.screenW * (1.0f - layout.tokens.panelInset), layout.screenH * (1.0f - layout.tokens.panelInset)};
    layout.safe = {
        0.0f,
        (layout.tokens.bottomSafeMargin - layout.tokens.topSafeMargin) * layout.screenH * 0.5f,
        layout.screenW * (1.0f - layout.tokens.horizontalSafeMargin * 2.0f),
        layout.screenH * (1.0f - layout.tokens.topSafeMargin - layout.tokens.bottomSafeMargin)
    };
    const float safeTop = layout.safe.y + layout.safe.h * 0.5f;
    const float headerH = layout.screenH * layout.tokens.titleZone;
    const float footerH = layout.screenH * layout.tokens.footerZone;
    layout.header = {0.0f, safeTop - headerH * 0.5f, layout.safe.w, headerH};
    layout.footer = {0.0f, layout.safe.y - layout.safe.h * 0.5f + footerH * 0.5f, layout.safe.w, footerH};
    layout.content = {0.0f, layout.safe.y - (headerH - footerH) * 0.5f, layout.safe.w, layout.safe.h - headerH - footerH};
    layout.titleY = layout.header.y - layout.header.h * 0.14f;
    layout.titleScale = layout.screenW * layout.tokens.titleScale;
    layout.labelX = -layout.safe.w * 0.46f;
    layout.valueRightX = layout.safe.w * 0.46f;

    layout.title = page.title;
    layout.paletteTitle = page.paletteTitle;
    layout.joinCode = page.joinCode;
    for (int i = 0; i < page.elementCount; ++i) {
        const PhoneMenuElement& element = page.elements[i];
        if (element.kind == PhoneMenuRowKind::Section) addPhoneMenuLayoutSection(layout, element.label);
        else if (element.kind == PhoneMenuRowKind::TwoColumn) addPhoneMenuLayoutValue(layout, element.label, element.value, element.action, element.bindingAction);
        else addPhoneMenuLayoutItem(layout, element.label, element.action, element.bindingAction);
    }

    layout.titleScale = phoneMenuFitScale(layout.titleScale, layout.safe.w * 0.94f, layout.title);
    finishPhoneMenuRows(layout, page.tablePage);
    return layout;
}

inline const PhoneMenuRow* phoneMenuRowForSelection(const PhoneMenuLayout& layout, int selection) {
    for (int i = 0; i < layout.rowCount; ++i) {
        if (layout.rows[i].selectable && layout.rows[i].selectableIndex == selection) return &layout.rows[i];
    }
    return nullptr;
}
