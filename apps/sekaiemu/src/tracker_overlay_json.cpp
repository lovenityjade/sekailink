#include "tracker_overlay_render_state.hpp"

#include <algorithm>
#include <sstream>

namespace sekaiemu::spike {

const nlohmann::json* JsonValueAtPath(const nlohmann::json& root, std::string_view path) {
  if (path.empty()) {
    return &root;
  }
  const nlohmann::json* current = &root;
  std::size_t start = 0;
  while (start < path.size()) {
    const std::size_t dot = path.find('.', start);
    const std::string key(
        path.substr(start, dot == std::string_view::npos ? path.size() - start : dot - start));
    if (!current->is_object()) {
      return nullptr;
    }
    const auto it = current->find(key);
    if (it == current->end()) {
      return nullptr;
    }
    current = &*it;
    if (dot == std::string_view::npos) {
      return current;
    }
    start = dot + 1;
  }
  return current;
}

std::string TruncateText(std::string_view text, std::size_t max_length) {
  if (text.size() <= max_length) {
    return std::string(text);
  }
  if (max_length <= 3) {
    return std::string(text.substr(0, max_length));
  }
  return std::string(text.substr(0, max_length - 3)) + "...";
}

std::string LabelOrId(const nlohmann::json& value) {
  if (!value.is_object()) {
    return "UNKNOWN";
  }
  const auto label = value.value("label", std::string{});
  if (!label.empty()) {
    return label;
  }
  return value.value("id", std::string{"UNKNOWN"});
}

const TrackerMapDefinition* ResolveActiveMap(const TrackerRuntime& runtime,
                                             const TrackerResolvedViewState& resolved) {
  const auto* bundle = runtime.Bundle();
  if (bundle == nullptr || resolved.active_map_id.empty()) {
    return nullptr;
  }
  return bundle->FindMap(resolved.active_map_id);
}

std::string ResolveActiveTabLabel(const TrackerResolvedViewState& resolved) {
  for (const auto& tab : resolved.visible_tabs) {
    if (tab.is_object() && tab.value("id", std::string{}) == resolved.active_tab_id) {
      return LabelOrId(tab);
    }
  }
  return resolved.active_tab_id.empty() ? std::string("Overview") : resolved.active_tab_id;
}

std::string PanelSurface(const nlohmann::json& panel) {
  return panel.value("surface", std::string{"details"});
}

int PanelPriority(const nlohmann::json& panel) {
  return panel.value("priority", 100);
}

std::string JsonScalarToText(const nlohmann::json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? "ON" : "OFF";
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<std::int64_t>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<std::uint64_t>());
  }
  if (value.is_number_float()) {
    std::ostringstream out;
    out << value.get<double>();
    return out.str();
  }
  return {};
}

std::string JsonStringAtAnyKey(const nlohmann::json& root,
                               std::initializer_list<const char*> keys) {
  if (!root.is_object()) {
    return {};
  }
  for (const char* key : keys) {
    const auto it = root.find(key);
    if (it == root.end()) {
      continue;
    }
    const auto rendered = JsonScalarToText(*it);
    if (!rendered.empty()) {
      return rendered;
    }
  }
  return {};
}

std::optional<double> JsonNumberAtAnyKey(const nlohmann::json& root,
                                         std::initializer_list<const char*> keys) {
  if (!root.is_object()) {
    return std::nullopt;
  }
  for (const char* key : keys) {
    const auto it = root.find(key);
    if (it == root.end()) {
      continue;
    }
    if (it->is_number()) {
      return it->get<double>();
    }
    if (it->is_string()) {
      try {
        return std::stod(it->get<std::string>());
      } catch (const std::exception&) {
      }
    }
  }
  return std::nullopt;
}

std::string MetadataStringAt(const nlohmann::json& root, std::string_view path) {
  if (const auto* value = JsonValueAtPath(root, path); value != nullptr) {
    return JsonScalarToText(*value);
  }
  return {};
}

}  // namespace sekaiemu::spike
