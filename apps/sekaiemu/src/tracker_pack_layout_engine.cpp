#include "tracker_pack_layout_engine.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_pack_layout_json.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace sekaiemu::spike::tracker_pack_layout_detail {
namespace {

bool TokenMatchesAny(std::string_view candidate, const std::vector<std::string>& tokens) {
  const auto canonical_candidate = CanonicalToken(candidate);
  if (canonical_candidate.empty()) {
    return false;
  }
  for (const auto& token : tokens) {
    if (token.empty()) {
      continue;
    }
    if (canonical_candidate == token) {
      return true;
    }
    if (token.size() >= 3 &&
        (canonical_candidate.find(token) != std::string::npos ||
         token.find(canonical_candidate) != std::string::npos)) {
      return true;
    }
    if (token.size() == 2 && canonical_candidate.rfind(token, 0) == 0) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> ActiveTrackerTabTokens(const TrackerResolvedViewState& resolved) {
  std::vector<std::string> tokens;
  auto append = [&](std::string_view value) {
    auto token = CanonicalToken(value);
    if (!token.empty() && std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
      tokens.push_back(std::move(token));
    }
  };
  append(resolved.active_tab_id);
  for (const auto& tab : resolved.visible_tabs) {
    if (!tab.is_object() || tab.value("id", std::string{}) != resolved.active_tab_id) {
      continue;
    }
    append(tab.value("label", std::string{}));
    append(tab.value("map_id", std::string{}));
    break;
  }
  append(resolved.active_map_id);
  return tokens;
}

}  // namespace

std::optional<nlohmann::json> ResolveLayoutReference(const PackStateContext& context,
                                                     const nlohmann::json& node) {
  if (!node.is_object()) {
    return std::nullopt;
  }
  if (node.value("type", std::string{}) != "layout") {
    return std::nullopt;
  }
  const auto key = node.value("key", std::string{});
  if (key.empty()) {
    return std::nullopt;
  }
  const auto it = context.document->layouts.find(key);
  if (it == context.document->layouts.end()) {
    return std::nullopt;
  }
  return it->second;
}

Rect Inset(Rect rect, int amount) {
  rect.x += amount;
  rect.y += amount;
  rect.width = std::max(0, rect.width - amount * 2);
  rect.height = std::max(0, rect.height - amount * 2);
  return rect;
}

Margins ParseMargins(std::string_view text) {
  Margins margins;
  if (text.empty()) {
    return margins;
  }
  std::vector<int> values;
  std::stringstream stream{std::string(text)};
  std::string token;
  while (std::getline(stream, token, ',')) {
    token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
                  return std::isspace(ch) != 0;
                }),
                token.end());
    if (token.empty()) {
      continue;
    }
    try {
      values.push_back(std::stoi(token));
    } catch (const std::exception&) {
      return margins;
    }
  }
  if (values.size() == 1) {
    margins.left = margins.top = margins.right = margins.bottom = values[0];
  } else if (values.size() == 2) {
    margins.left = margins.right = values[0];
    margins.top = margins.bottom = values[1];
  } else if (values.size() == 4) {
    margins.left = values[0];
    margins.top = values[1];
    margins.right = values[2];
    margins.bottom = values[3];
  }
  return margins;
}

Rect ApplyMargins(Rect rect, const Margins& margins) {
  rect.x += margins.left;
  rect.y += margins.top;
  rect.width = std::max(0, rect.width - margins.left - margins.right);
  rect.height = std::max(0, rect.height - margins.top - margins.bottom);
  return rect;
}

Rect ApplySizeConstraints(Rect rect, const nlohmann::json& node) {
  if (!node.is_object()) {
    return rect;
  }
  const int requested_min_width = std::max(0, JsonIntFlexible(node, "min_width", 0));
  const int requested_min_height = std::max(0, JsonIntFlexible(node, "min_height", 0));
  const int requested_max_width = std::max(0, JsonIntFlexible(node, "max_width", rect.width));
  const int requested_max_height = std::max(0, JsonIntFlexible(node, "max_height", rect.height));
  const int min_width = std::min(rect.width, requested_min_width);
  const int min_height = std::min(rect.height, requested_min_height);
  const int max_width = std::max(min_width, std::min(rect.width, requested_max_width));
  const int max_height = std::max(min_height, std::min(rect.height, requested_max_height));
  const int desired_width = std::clamp(rect.width, min_width, max_width);
  const int desired_height = std::clamp(rect.height, min_height, max_height);

  const auto h_alignment = JsonStringFlexible(node, "h_alignment");
  const auto v_alignment = JsonStringFlexible(node, "v_alignment");
  if (desired_width < rect.width) {
    if (h_alignment == "center") {
      rect.x += (rect.width - desired_width) / 2;
    } else if (h_alignment == "right") {
      rect.x += rect.width - desired_width;
    }
    rect.width = desired_width;
  } else {
    rect.width = desired_width;
  }

  if (desired_height < rect.height) {
    if (v_alignment == "center") {
      rect.y += (rect.height - desired_height) / 2;
    } else if (v_alignment == "bottom") {
      rect.y += rect.height - desired_height;
    }
    rect.height = desired_height;
  } else {
    rect.height = desired_height;
  }
  rect.width = std::max(0, rect.width);
  rect.height = std::max(0, rect.height);
  return rect;
}

int ResolveAlignedOrigin(int origin, int available, int content, std::string_view alignment) {
  if (alignment == "center" || alignment == "middle") {
    return origin + std::max(0, (available - content) / 2);
  }
  if (alignment == "right" || alignment == "bottom") {
    return origin + std::max(0, available - content);
  }
  return origin;
}

std::vector<nlohmann::json> NodeContentArray(const nlohmann::json& node) {
  std::vector<nlohmann::json> content;
  if (node.contains("content") && node["content"].is_array()) {
    for (const auto& entry : node["content"]) {
      if (!ShouldSuppressPackLayoutNode(entry)) {
        content.push_back(entry);
      }
    }
  } else if (node.contains("content") && node["content"].is_object()) {
    if (!ShouldSuppressPackLayoutNode(node["content"])) {
      content.push_back(node["content"]);
    }
  }
  return content;
}

Rect ClampRectToPositive(Rect rect) {
  rect.width = std::max(0, rect.width);
  rect.height = std::max(0, rect.height);
  return rect;
}

Size ApplyPreferredSizeConstraints(Size size, const nlohmann::json& node) {
  if (!node.is_object()) {
    return size;
  }
  const int min_width = std::max(0, JsonIntFlexible(node, "min_width", 0));
  const int min_height = std::max(0, JsonIntFlexible(node, "min_height", 0));
  const int max_width = std::max(0, JsonIntFlexible(node, "max_width", 0));
  const int max_height = std::max(0, JsonIntFlexible(node, "max_height", 0));
  size.width = std::max(size.width, min_width);
  size.height = std::max(size.height, min_height);
  if (max_width > 0) {
    size.width = std::min(size.width, std::max(min_width, max_width));
  }
  if (max_height > 0) {
    size.height = std::min(size.height, std::max(min_height, max_height));
  }
  return size;
}

bool ShouldSuppressPackLayoutNode(const nlohmann::json& node) {
  if (!node.is_object()) {
    return false;
  }
  const auto type = node.value("type", std::string{});
  const auto key = CanonicalToken(node.value("key", std::string{}));
  const auto header = CanonicalToken(node.value("header", std::string{}));
  const auto suppressed = [](std::string_view token) {
    return token == "settings" ||
           token == "settinggrid" ||
           token == "pulltreedrops" ||
           token == "hoarderstundrops" ||
           token == "hoarderdrops" ||
           token == "erlegend";
  };
  if (type == "layout" && suppressed(key)) {
    return true;
  }
  if (type == "map") {
    bool saw_map = false;
    for (const auto& entry : node.value("maps", nlohmann::json::array())) {
      if (!entry.is_string()) {
        continue;
      }
      saw_map = true;
      if (!suppressed(CanonicalToken(entry.get<std::string>()))) {
        return false;
      }
    }
    if (saw_map) {
      return true;
    }
  }
  return suppressed(header);
}

bool NodeTargetsActiveMap(const PackStateContext& context, const nlohmann::json& node) {
  if (!node.is_object()) {
    return false;
  }
  if (ShouldSuppressPackLayoutNode(node)) {
    return false;
  }
  if (const auto resolved_layout = ResolveLayoutReference(context, node); resolved_layout.has_value()) {
    return NodeTargetsActiveMap(context, *resolved_layout);
  }
  const auto type = node.value("type", std::string{});
  if (type == "map") {
    for (const auto& entry : node.value("maps", nlohmann::json::array())) {
      if (!entry.is_string()) {
        continue;
      }
      std::string map_id;
      if (ResolveContextPackMapId(context, entry.get<std::string>(), &map_id) &&
          map_id == context.resolved.active_map_id) {
        return true;
      }
    }
    return false;
  }
  if (type == "tabbed") {
    for (const auto& tab : node.value("tabs", nlohmann::json::array())) {
      if (tab.is_object() && NodeTargetsActiveMap(context, tab.value("content", nlohmann::json::object()))) {
        return true;
      }
    }
  }
  for (const auto& child : NodeContentArray(node)) {
    if (NodeTargetsActiveMap(context, child)) {
      return true;
    }
  }
  return false;
}

bool NodeTargetsActiveTrackerTab(const PackStateContext& context, const nlohmann::json& node) {
  if (!node.is_object()) {
    return false;
  }
  if (ShouldSuppressPackLayoutNode(node)) {
    return false;
  }
  const auto tokens = ActiveTrackerTabTokens(context.resolved);
  if (TokenMatchesAny(node.value("title", std::string{}), tokens) ||
      TokenMatchesAny(node.value("header", std::string{}), tokens) ||
      TokenMatchesAny(node.value("key", std::string{}), tokens)) {
    return true;
  }
  if (const auto resolved_layout = ResolveLayoutReference(context, node); resolved_layout.has_value()) {
    return NodeTargetsActiveTrackerTab(context, *resolved_layout);
  }
  const auto type = node.value("type", std::string{});
  if (type == "map") {
    for (const auto& entry : node.value("maps", nlohmann::json::array())) {
      if (entry.is_string() && TokenMatchesAny(entry.get<std::string>(), tokens)) {
        return true;
      }
    }
    return false;
  }
  if (type == "tabbed") {
    for (const auto& tab : node.value("tabs", nlohmann::json::array())) {
      if (tab.is_object() && NodeTargetsActiveTrackerTab(context, tab)) {
        return true;
      }
    }
  }
  for (const auto& child : NodeContentArray(node)) {
    if (NodeTargetsActiveTrackerTab(context, child)) {
      return true;
    }
  }
  return false;
}

Size EstimateNodePreferredSize(const PackStateContext& context, const nlohmann::json& node) {
  if (!node.is_object()) {
    return {};
  }
  if (ShouldSuppressPackLayoutNode(node)) {
    return {};
  }
  if (const auto resolved_layout = ResolveLayoutReference(context, node); resolved_layout.has_value()) {
    return EstimateNodePreferredSize(context, *resolved_layout);
  }

  const auto type = node.value("type", std::string{});
  if (type == "container") {
    return ApplyPreferredSizeConstraints(
        EstimateNodePreferredSize(context, node.value("content", nlohmann::json::object())), node);
  }
  if (type == "group") {
    auto content = EstimateNodePreferredSize(context, node.value("content", nlohmann::json::object()));
    const int header_height = node.value("header", std::string{}).empty() ? 0 : 18;
    content.width += 8;
    content.height += header_height + 8;
    return ApplyPreferredSizeConstraints(content, node);
  }
  if (type == "itemgrid") {
    const auto rows = node.value("rows", nlohmann::json::array());
    int max_columns = 0;
    int row_count = 0;
    for (const auto& row : rows) {
      if (!row.is_array()) {
        continue;
      }
      ++row_count;
      max_columns = std::max(max_columns, static_cast<int>(row.size()));
    }
    const int desired_w = JsonIntFlexible(node, "item_width", JsonIntFlexible(node, "item_size", 16));
    const int desired_h = JsonIntFlexible(node, "item_height", JsonIntFlexible(node, "item_size", 16));
    const auto item_margins = ParseMargins(JsonStringFlexible(node, "item_margin"));
    const int gap_x = std::max(0, item_margins.left + item_margins.right);
    const int gap_y = std::max(0, item_margins.top + item_margins.bottom);
    Size size;
    size.width = max_columns > 0 ? desired_w * max_columns + gap_x * std::max(0, max_columns - 1) : 0;
    size.height = row_count > 0 ? desired_h * row_count + gap_y * std::max(0, row_count - 1) : 0;
    const auto outer = ParseMargins(JsonStringFlexible(node, "margin"));
    size.width += outer.left + outer.right;
    size.height += outer.top + outer.bottom;
    return ApplyPreferredSizeConstraints(size, node);
  }
  if (type == "item") {
    const int max_width = JsonIntFlexible(node, "max_width", 220);
    const int min_width = JsonIntFlexible(node, "min_width", 140);
    return ApplyPreferredSizeConstraints({std::max(min_width, max_width), 28}, node);
  }
  if (type == "array") {
    const auto orientation = node.value("orientation", std::string{"vertical"});
    Size size;
    for (const auto& child : NodeContentArray(node)) {
      const auto child_size = EstimateNodePreferredSize(context, child);
      if (orientation == "horizontal") {
        size.width += child_size.width;
        size.height = std::max(size.height, child_size.height);
      } else {
        size.width = std::max(size.width, child_size.width);
        size.height += child_size.height;
      }
    }
    return ApplyPreferredSizeConstraints(size, node);
  }
  if (type == "tabbed") {
    Size content;
    for (const auto& tab : node.value("tabs", nlohmann::json::array())) {
      if (!tab.is_object()) {
        continue;
      }
      const auto child = EstimateNodePreferredSize(context, tab.value("content", nlohmann::json::object()));
      content.width = std::max(content.width, child.width);
      content.height = std::max(content.height, child.height);
    }
    return ApplyPreferredSizeConstraints(content, node);
  }
  if (type == "map") {
    if (const auto maps = node.value("maps", nlohmann::json::array()); maps.is_array() && maps.size() == 1 &&
        maps.front().is_string()) {
      std::string map_id;
      if (ResolveContextPackMapId(context, maps.front().get<std::string>(), &map_id)) {
        if (const auto* map = context.bundle->FindMap(map_id); map != nullptr &&
            map->raster_image.has_value()) {
          return ApplyPreferredSizeConstraints({static_cast<int>(map->raster_image->width) + 8,
                                                static_cast<int>(map->raster_image->height) + 8},
                                               node);
        }
      }
    }
    return ApplyPreferredSizeConstraints({320, 240}, node);
  }
  if (type == "recentpins") {
    const auto orientation = node.value("orientation", std::string{"vertical"});
    const bool compact = node.value("compact", false);
    const int rows = compact ? 4 : 6;
    if (orientation == "horizontal") {
      return ApplyPreferredSizeConstraints({std::max(220, rows * 52), compact ? 52 : 64}, node);
    }
    return ApplyPreferredSizeConstraints({220, compact ? rows * 18 + 8 : rows * 20 + 10}, node);
  }
  if (type == "dock") {
    Size size;
    for (const auto& child : NodeContentArray(node)) {
      const auto child_dock = child.value("dock", std::string{});
      const auto child_size = EstimateNodePreferredSize(context, child);
      if (child_dock == "left" || child_dock == "right") {
        size.width += child_size.width;
        size.height = std::max(size.height, child_size.height);
      } else if (child_dock == "top" || child_dock == "bottom") {
        size.width = std::max(size.width, child_size.width);
        size.height += child_size.height;
      } else {
        size.width = std::max(size.width, child_size.width);
        size.height = std::max(size.height, child_size.height);
      }
    }
    return ApplyPreferredSizeConstraints(size, node);
  }
  return ApplyPreferredSizeConstraints({}, node);
}

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
