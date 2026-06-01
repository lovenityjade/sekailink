#include "tracker_overlay_detail_sections.hpp"

#include "tracker_overlay_render_state.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

namespace sekaiemu::spike {

void DrawInfoPanels(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    const TrackerResolvedViewState& resolved,
                    int x,
                    int y,
                    int width,
                    int height) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "DETAILS");

  int cursor_y = y + 24;
  const int bottom = y + height - 10;
  std::vector<const nlohmann::json*> visible_panels;
  for (const auto& panel : resolved.info_panels) {
    if (!panel.is_object()) {
      continue;
    }
    const auto surface = PanelSurface(panel);
    if (surface == "summary" || surface == "primary" || surface == "header" || surface == "hidden") {
      continue;
    }
    visible_panels.push_back(&panel);
  }
  std::stable_sort(visible_panels.begin(),
                   visible_panels.end(),
                   [](const nlohmann::json* lhs, const nlohmann::json* rhs) {
                     return std::tuple(PanelPriority(*lhs), lhs->value("label", std::string{})) <
                            std::tuple(PanelPriority(*rhs), rhs->value("label", std::string{}));
                   });

  auto panel_height_for = [](const nlohmann::json& panel, int panel_width) {
    const auto fields = panel.value("fields", nlohmann::json::array());
    int field_count = 0;
    for (const auto& field : fields) {
      if (field.is_object()) {
        ++field_count;
      }
    }
    const int row_height = panel_width < 110 ? 11 : 12;
    return 18 + std::max(18, field_count * row_height) + 6;
  };

  auto draw_panel_card = [&](const nlohmann::json& panel, int panel_x, int panel_y, int panel_width) {
    const int field_x = panel_x + 6;
    const int value_x = panel_x + std::min(54, std::max(42, panel_width / 3));
    const int row_height = panel_width < 110 ? 11 : 12;
    const int panel_height = panel_height_for(panel, panel_width);
    canvas.FillRect(panel_x, panel_y, panel_width, panel_height, palette.header_background);
    canvas.DrawRect(panel_x, panel_y, panel_width, panel_height, palette.section_border);
    canvas.DrawText(panel_x + 6,
                    panel_y + 5,
                    TruncateText(panel.value("label", panel.value("id", std::string{"Panel"})), panel_width < 110 ? 10 : 14),
                    palette.accent_soft,
                    1);
    int field_y = panel_y + 20;
    const auto fields = panel.value("fields", nlohmann::json::array());
    for (const auto& field : fields) {
      if (!field.is_object()) {
        continue;
      }
      const auto label = TruncateText(field.value("label", field.value("id", std::string{"Field"})), panel_width < 110 ? 8 : 10);
      const auto value = TruncateText(field.value("value", std::string{"UNKNOWN"}), panel_width < 110 ? 12 : 14);
      canvas.DrawText(field_x, field_y, label, palette.text_secondary, 1);
      canvas.DrawText(value_x, field_y, value, palette.text_primary, 1);
      field_y += row_height;
    }
  };

  bool drew_panel = false;
  std::size_t rendered_panels = 0;
  if (width >= 220 && !visible_panels.empty()) {
    const int card_gap = 6;
    const int card_width = (width - 18 - card_gap) / 2;
    int row_y = cursor_y;
    for (std::size_t index = 0; index < visible_panels.size(); index += 2) {
      const int left_height = panel_height_for(*visible_panels[index], card_width);
      const int right_height =
          index + 1 < visible_panels.size() ? panel_height_for(*visible_panels[index + 1], card_width) : 0;
      const int row_height = std::max(left_height, right_height);
      if (row_y + row_height > bottom) {
        break;
      }
      draw_panel_card(*visible_panels[index], x + 6, row_y, card_width);
      ++rendered_panels;
      if (index + 1 < visible_panels.size()) {
        draw_panel_card(*visible_panels[index + 1], x + 6 + card_width + card_gap, row_y, card_width);
        ++rendered_panels;
      }
      drew_panel = true;
      row_y += row_height + 6;
    }
  } else {
    for (const auto* panel : visible_panels) {
      const int panel_height = panel_height_for(*panel, width - 12);
      if (cursor_y + panel_height > bottom) {
        break;
      }
      draw_panel_card(*panel, x + 6, cursor_y, width - 12);
      drew_panel = true;
      ++rendered_panels;
      cursor_y += panel_height + 6;
    }
  }
  const std::size_t hidden_panels =
      visible_panels.size() > rendered_panels ? visible_panels.size() - rendered_panels : 0;
  if (!drew_panel) {
    canvas.DrawWrappedText(x + 10,
                           y + 28,
                           width - 20,
                           "WAITING FOR SEED OR SESSION METADATA. THE TRACKER SURFACE STAYS STABLE BEFORE ROOM DATA ARRIVES.",
                           palette.text_secondary,
                           1,
                           3);
  } else if (hidden_panels > 0 && bottom - 12 >= cursor_y) {
    canvas.DrawText(x + 10,
                    bottom - 10,
                    TruncateText("+" + std::to_string(hidden_panels) + " MORE PANELS", 22),
                    palette.text_secondary,
                    1);
  }
}

void DrawRecentEvents(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerResolvedViewState& resolved,
                      int x,
                      int y,
                      int width,
                      int height) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "RECENT");

  int cursor_y = y + 24;
  const auto recent_rows = BuildRecentRows(runtime, resolved);
  if (recent_rows.empty()) {
    canvas.DrawWrappedText(
        x + 8, cursor_y, width - 16, "NO LIVE EVENTS YET", palette.text_secondary, 1, 3);
    return;
  }

  const int bottom = y + height - 10;
  std::size_t rendered_events = 0;
  for (auto it = recent_rows.rbegin(); it != recent_rows.rend(); ++it) {
    if (cursor_y + 26 > bottom) {
      break;
    }
    const std::string prefix = it->event_type == "item_received" ? "ITEM" : "CHECK";
    const auto label = TruncateText(it->label.empty() ? "UNKNOWN" : it->label, 18);
    const auto timestamp = TruncateText(it->timestamp, 9);

    canvas.DrawText(x + 8, cursor_y, prefix, palette.accent_soft, 1);
    canvas.DrawText(x + 40, cursor_y, label, palette.text_primary, 1);
    if (!timestamp.empty()) {
      canvas.DrawText(x + width - 58, cursor_y, timestamp, palette.text_secondary, 1);
    }
    cursor_y += 14;

    if (!it->canonical_id.empty()) {
      canvas.DrawText(x + 40, cursor_y, TruncateText(it->canonical_id, 16), palette.text_secondary, 1);
    } else if (!it->detail.empty()) {
      canvas.DrawText(x + 40, cursor_y, TruncateText(it->detail, 18), palette.text_secondary, 1);
    }
    cursor_y += 12;
    ++rendered_events;
  }
  const std::size_t hidden_events =
      recent_rows.size() > rendered_events ? recent_rows.size() - rendered_events : 0;
  if (hidden_events > 0 && cursor_y + 10 <= bottom) {
    canvas.DrawText(x + 8,
                    bottom - 8,
                    TruncateText("+" + std::to_string(hidden_events) + " MORE EVENTS", 24),
                    palette.text_secondary,
                    1);
  }
}

void DrawSessionStrip(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerResolvedViewState& resolved,
                      int x,
                      int y,
                      int width) {
  DrawSectionBox(canvas, palette, x, y, width, 60);
  DrawChipRows(canvas, palette, BuildMetadataChips(runtime, resolved), x + 6, y + 7, width - 12, 2, 0);
  canvas.DrawText(x + 8,
                  y + 44,
                  TruncateText(BuildSessionHeadline(runtime, resolved), 48),
                  palette.text_secondary,
                  1);
}

void DrawMetadataOverview(OverlayCanvas& canvas,
                          const UiPalette& palette,
                          const TrackerRuntime& runtime,
                          const TrackerResolvedViewState& resolved,
                          int x,
                          int y,
                          int width,
                          int height) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "LIVE METADATA");

  const auto chips = BuildMetadataChips(runtime, resolved);
  DrawChipRows(canvas, palette, chips, x + 8, y + 24, width - 16, 3, 0);
}

void DrawItemsSection(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerResolvedViewState& resolved,
                      int x,
                      int y,
                      int width,
                      int height,
                      const TrackerOverlayAssetResolver* asset_resolver) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "ITEM FLOW");

  const auto& observed = runtime.ObservedState();
  int cursor_y = y + 24;
  DrawMetricRow(canvas, palette, x + 8, cursor_y, "RECEIVED", std::to_string(ReceivedCount(runtime)));
  cursor_y += 14;
  DrawMetricRow(canvas, palette, x + 8, cursor_y, "TAB", TruncateText(ResolveActiveTabLabel(resolved), 14));
  cursor_y += 14;

  const auto bundle_items = BuildBundleItems(runtime);
  if (!bundle_items.empty() && cursor_y + 18 <= y + height - 8) {
    const auto received_ids = ReceivedItemIds(runtime);
    canvas.DrawText(x + 8, cursor_y, "BUNDLE ITEMS", palette.text_secondary, 1);
    bool has_received_metadata = false;
    bool has_pending_metadata = false;
    for (const auto& item : bundle_items) {
      const bool received =
          item.has_explicit_state ? item.acquired : received_ids.find(item.id) != received_ids.end();
      if (received) {
        has_received_metadata = true;
      } else {
        has_pending_metadata = true;
      }
    }
    if (has_received_metadata) {
      canvas.FillRect(x + width - 34, cursor_y + 1, 10, 6, UiColor{86, 214, 142, 255});
    }
    if (has_pending_metadata) {
      canvas.FillRect(x + width - 20, cursor_y + 1, 10, 6, UiColor{48, 58, 72, 255});
    }
    cursor_y += 12;
    const int badge_size = height < 90 ? 12 : 16;
    const int badge_gap = 4;
    int badge_x = x + 8;
    int badge_y = cursor_y;
    std::size_t rendered = 0;
    for (const auto& item : bundle_items) {
      if (rendered >= 12 || badge_y + badge_size > y + height - 8) {
        break;
      }
      if (badge_x + badge_size > x + width - 8) {
        badge_x = x + 8;
        badge_y += badge_size + badge_gap;
        if (badge_y + badge_size > y + height - 8) {
          break;
        }
      }
      DrawItemBadge(canvas,
                    palette,
                    badge_x,
                    badge_y,
                    badge_size,
                    item,
                    item.has_explicit_state ? item.acquired
                                            : received_ids.find(item.id) != received_ids.end(),
                    asset_resolver);
      badge_x += badge_size + badge_gap;
      ++rendered;
    }
    cursor_y = badge_y + badge_size + 8;
  } else {
    DrawMetricRow(canvas, palette, x + 8, cursor_y, "LOCAL ITM", std::to_string(observed.locally_received_items.size()));
    cursor_y += 14;
    DrawMetricRow(canvas, palette, x + 8, cursor_y, "CHECKED", std::to_string(CheckedCount(runtime)));
    cursor_y += 18;
  }

  const std::string last_item_label = LastReceivedLabel(runtime, resolved);
  const std::string last_check_label = LastCheckedLabel(runtime, resolved);
  const std::string last_item = last_item_label.empty() ? "NONE YET" : last_item_label;
  const std::string last_from = LastReceivedFrom(runtime);
  const std::string last_check = last_check_label.empty() ? "NONE YET" : last_check_label;

  if (cursor_y + 20 > y + height - 8) {
    return;
  }
  canvas.DrawText(x + 8, cursor_y, "LAST ITEM", palette.text_secondary, 1);
  cursor_y += 12;
  canvas.DrawWrappedText(x + 8,
                         cursor_y,
                         width - 16,
                         TruncateText(last_item, 34),
                         last_item == "NONE YET" ? palette.text_secondary : palette.text_primary,
                         1,
                         2);
  cursor_y += 24;
  if (!last_from.empty() && cursor_y + 12 <= y + height - 8) {
    canvas.DrawText(x + 8, cursor_y, TruncateText("FROM " + last_from, 26), palette.accent_soft, 1);
    cursor_y += 14;
  }
  if (cursor_y + 20 <= y + height - 8) {
    canvas.DrawText(x + 8, cursor_y, "LAST CHECK", palette.text_secondary, 1);
    cursor_y += 12;
    canvas.DrawWrappedText(x + 8,
                           cursor_y,
                           width - 16,
                           TruncateText(last_check, 34),
                           last_check == "NONE YET" ? palette.text_secondary : palette.text_primary,
                           1,
                           2);
  }
}

void DrawCompactBody(OverlayCanvas& canvas,
                     const UiPalette& palette,
                     const TrackerRuntime& runtime,
                     const TrackerResolvedViewState& resolved,
                     int x,
                     int y,
                     int width,
                     int height) {
  DrawSectionBox(canvas, palette, x, y, width, height);
  DrawSectionHeader(canvas, palette, x, y, width, "TRACKER");

  int cursor_y = y + 24;
  DrawMetricRow(canvas,
                palette,
                x + 8,
                cursor_y,
                "SLOT",
                runtime.AuthoritativeState().slot_id.empty() ? "UNKNOWN"
                                                             : runtime.AuthoritativeState().slot_id);
  cursor_y += 14;
  DrawMetricRow(canvas,
                palette,
                x + 8,
                cursor_y,
                "SEED",
                runtime.AuthoritativeState().seed_id.empty() ? "UNKNOWN"
                                                             : runtime.AuthoritativeState().seed_id);
  cursor_y += 14;
  DrawMetricRow(canvas, palette, x + 8, cursor_y, "TAB", resolved.active_tab_id.empty() ? "DEFAULT" : resolved.active_tab_id);
  cursor_y += 14;
  DrawMetricRow(canvas, palette, x + 8, cursor_y, "MAP", resolved.active_map_id.empty() ? "DEFAULT" : resolved.active_map_id);
  cursor_y += 14;
  DrawMetricRow(canvas,
                palette,
                x + 8,
                cursor_y,
                "CHECKS",
                std::to_string(runtime.AuthoritativeState().checked_locations.size()));
  cursor_y += 14;
  DrawMetricRow(canvas,
                palette,
                x + 8,
                cursor_y,
                "ITEMS",
                std::to_string(runtime.AuthoritativeState().received_items.size()));
  cursor_y += 18;

  canvas.DrawWrappedText(x + 8,
                         cursor_y,
                         width - 16,
                         "VISIBLE " + TruncateText(resolved.active_map_id.empty() ? "MAPS" : resolved.active_map_id, 18),
                         palette.text_secondary,
                         1,
                         3);
}

}  // namespace sekaiemu::spike
