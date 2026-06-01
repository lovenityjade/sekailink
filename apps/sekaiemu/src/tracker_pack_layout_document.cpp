#include "tracker_pack_layout_document.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_pack_layout_json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike::tracker_pack_layout_detail {
namespace {

namespace fs = std::filesystem;

void RegisterVisualDefinition(PackLayoutDocument& document,
                              const std::string& code,
                              const PackVisualDefinition& definition,
                              int priority) {
  if (code.empty()) {
    return;
  }
  const auto existing_priority = document.visual_alias_priorities.find(code);
  if (existing_priority != document.visual_alias_priorities.end() && existing_priority->second >= priority) {
    return;
  }
  document.visuals[code] = definition;
  document.visual_alias_priorities[code] = priority;
}

void RegisterSnapshotVisualDefinition(PackStateContext& context,
                                      const std::string& code,
                                      const PackVisualDefinition& definition,
                                      int priority) {
  if (code.empty()) {
    return;
  }
  const auto existing_priority = context.snapshot_visual_alias_priorities.find(code);
  if (existing_priority != context.snapshot_visual_alias_priorities.end() &&
      existing_priority->second >= priority) {
    return;
  }
  context.snapshot_visuals[code] = definition;
  context.snapshot_visual_alias_priorities[code] = priority;
}

void LoadVisualDefinitions(PackLayoutDocument& document, const fs::path& bundle_root) {
  const fs::path items_root = bundle_root / "poptracker-adapted" / "items";
  std::error_code ec;
  if (!fs::exists(items_root, ec)) {
    return;
  }
  for (const auto& entry : fs::directory_iterator(items_root, ec)) {
    if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }
    const auto raw = ParseJsonWithComments(entry.path());
    if (!raw.is_array()) {
      continue;
    }
    for (const auto& item : raw) {
      if (!item.is_object()) {
        continue;
      }
      PackVisualDefinition definition;
      definition.label = JsonStringFlexible(item, "name");
      definition.type = JsonStringFlexible(item, "type");
      definition.image = NormalizePackAssetPath(JsonStringFlexible(item, "img"));
      definition.disabled_image = NormalizePackAssetPath(JsonStringFlexible(item, "disabled_img"));
      definition.image_mods = JsonStringFlexible(item, "img_mods");
      definition.disabled_image_mods = JsonStringFlexible(item, "disabled_img_mods");
      definition.base_item = JsonStringFlexible(item, "base_item");
      definition.item_left = JsonStringFlexible(item, "item_left");
      definition.item_right = JsonStringFlexible(item, "item_right");
      definition.initial_quantity = JsonIntFlexible(item, "initial_quantity", 0);
      definition.min_quantity = JsonIntFlexible(item, "min_quantity", 0);
      definition.max_quantity = JsonIntFlexible(item, "max_quantity", 0);
      definition.increment = std::max(1, JsonIntFlexible(item, "increment", 1));
      definition.initial_stage = JsonIntFlexible(item, "initial_stage_idx", 0);
      definition.loop_stages = JsonBoolFlexible(item, "loop", false);
      definition.static_only = definition.type == "static";
      definition.allow_disabled = JsonBoolFlexible(item, "allow_disabled", true);
      definition.initial_active_state = JsonBoolFlexible(item, "initial_active_state", false);
      std::vector<std::string> aliases;
      std::vector<std::string> item_aliases;
      std::vector<std::string> stage_aliases;
      std::vector<std::string> image_aliases;
      std::vector<std::string> label_aliases;
      AppendCodeList(item_aliases, ParseCodeList(item, "codes"));
      AppendCodeList(item_aliases, ParseCodeList(item, "secondary_codes"));
      AppendCodeList(aliases, item_aliases);
      for (const auto& stage : item.value("stages", nlohmann::json::array())) {
        if (!stage.is_object()) {
          continue;
        }
        const int stage_index = static_cast<int>(definition.stages.size());
        definition.stages.push_back(NormalizePackAssetPath(JsonStringFlexible(stage, "img")));
        definition.disabled_stages.push_back(NormalizePackAssetPath(JsonStringFlexible(stage, "disabled_img")));
        definition.stage_mods.push_back(JsonStringFlexible(stage, "img_mods"));
        definition.disabled_stage_mods.push_back(JsonStringFlexible(stage, "disabled_img_mods"));
        std::vector<std::string> current_stage_aliases;
        AppendCodeList(current_stage_aliases, ParseCodeList(stage, "codes"));
        AppendCodeList(current_stage_aliases, ParseCodeList(stage, "secondary_codes"));
        for (const auto& alias : current_stage_aliases) {
          AppendUniqueCode(stage_aliases, alias);
          if (!definition.stage_index_by_alias.contains(alias)) {
            definition.stage_index_by_alias[alias] = stage_index;
          }
        }
      }
      AppendCodeList(aliases, stage_aliases);
      for (const auto& image : item.value("images", nlohmann::json::array())) {
        if (!image.is_object()) {
          continue;
        }
        const auto normalized = NormalizePackAssetPath(JsonStringFlexible(image, "img"));
        const bool left = image.value("left", false);
        const bool right = image.value("right", false);
        if (left && right) {
          definition.composite_both_image = normalized;
          definition.composite_both_mods = JsonStringFlexible(image, "img_mods");
        } else if (left) {
          definition.composite_left_image = normalized;
          definition.composite_left_mods = JsonStringFlexible(image, "img_mods");
        } else if (right) {
          definition.composite_right_image = normalized;
          definition.composite_right_mods = JsonStringFlexible(image, "img_mods");
        } else {
          definition.composite_none_image = normalized;
          definition.composite_none_mods = JsonStringFlexible(image, "img_mods");
        }
        AppendCodeList(image_aliases, ParseCodeList(image, "codes"));
        AppendCodeList(image_aliases, ParseCodeList(image, "secondary_codes"));
      }
      AppendCodeList(aliases, image_aliases);
      if (!definition.label.empty()) {
        AppendUniqueCode(label_aliases, CanonicalToken(definition.label));
        AppendCodeList(aliases, label_aliases);
      }
      if (aliases.empty()) {
        continue;
      }
      definition.primary_code = aliases.front();
      for (const auto& code : item_aliases) {
        RegisterVisualDefinition(document, code, definition, 100);
      }
      for (const auto& code : stage_aliases) {
        RegisterVisualDefinition(document, code, definition, 80);
      }
      for (const auto& code : image_aliases) {
        RegisterVisualDefinition(document, code, definition, 70);
      }
      for (const auto& code : label_aliases) {
        RegisterVisualDefinition(document, code, definition, 50);
      }
    }
  }
}

void LoadLayoutDefinitions(PackLayoutDocument& document, const fs::path& bundle_root) {
  const fs::path layouts_root = bundle_root / "poptracker-adapted" / "layouts";
  std::error_code ec;
  if (!fs::exists(layouts_root, ec)) {
    return;
  }
  for (const auto& entry : fs::directory_iterator(layouts_root, ec)) {
    if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }
    const auto raw = ParseJsonWithComments(entry.path());
    if (!raw.is_object()) {
      continue;
    }
    for (auto it = raw.begin(); it != raw.end(); ++it) {
      if (it.value().is_object()) {
        document.layouts[it.key()] = it.value();
      }
    }
  }
}

void LoadItemMappings(PackLayoutDocument& document, const fs::path& bundle_root) {
  const fs::path mapping_path =
      bundle_root / "poptracker-adapted" / "scripts" / "autotracking" / "item_mapping.lua";
  if (!fs::exists(mapping_path)) {
    return;
  }
  const std::string text = ReadTextFile(mapping_path);
  std::regex pattern(R"(\[(\d+)\]\s*=\s*\{\{([^}]*)\}\s*,)");
  for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
    const auto item_id = static_cast<std::int64_t>(std::stoll((*it)[1].str()));
    std::stringstream stream((*it)[2].str());
    std::string token;
    while (std::getline(stream, token, ',')) {
      token.erase(std::remove(token.begin(), token.end(), '"'), token.end());
      token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
                   return std::isspace(ch) != 0;
                 }),
                 token.end());
      if (!token.empty()) {
        document.item_ids_by_code[token].push_back(item_id);
      }
    }
  }
}

bool SnapshotHasArrayAtAnyPath(const nlohmann::json& snapshot, std::initializer_list<const char*> paths) {
  for (const char* path : paths) {
    const auto* value = JsonValueAtPath(snapshot, path);
    if (value != nullptr && value->is_array() && !value->empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace

void LoadSnapshotVisualDefinitions(PackStateContext& context) {
  const nlohmann::json* visuals = nullptr;
  for (const char* path : {"pack_item_visuals", "packItemVisuals", "tracker.pack_item_visuals",
                           "tracker_state.pack_item_visuals"}) {
    const auto* value = JsonValueAtPath(context.runtime.AuthoritativeState().snapshot, path);
    if (value != nullptr && value->is_array()) {
      visuals = value;
      break;
    }
  }
  if (visuals == nullptr) {
    return;
  }

  for (const auto& item : *visuals) {
    if (!item.is_object()) {
      continue;
    }
    PackVisualDefinition definition;
    definition.primary_code = JsonStringAtAnyKey(item, {"primary_code", "primaryCode", "code", "id"});
    definition.label = JsonStringAtAnyKey(item, {"name", "label", "title"});
    definition.type = JsonStringFlexible(item, "type");
    definition.image = NormalizePackAssetPath(JsonStringAtAnyKey(item, {"image", "img"}));
    definition.disabled_image =
        NormalizePackAssetPath(JsonStringAtAnyKey(item, {"disabled_image", "disabledImage", "disabled_img"}));
    definition.image_mods = JsonStringAtAnyKey(item, {"image_mods", "imageMods", "img_mods"});
    definition.disabled_image_mods =
        JsonStringAtAnyKey(item, {"disabled_image_mods", "disabledImageMods", "disabled_img_mods"});
    definition.base_item = JsonStringAtAnyKey(item, {"base_item", "baseItem"});
    definition.item_left = JsonStringAtAnyKey(item, {"item_left", "itemLeft"});
    definition.item_right = JsonStringAtAnyKey(item, {"item_right", "itemRight"});
    definition.initial_quantity = JsonIntFlexible(item, "initial_quantity", 0);
    definition.min_quantity = JsonIntFlexible(item, "min_quantity", 0);
    definition.max_quantity = JsonIntFlexible(item, "max_quantity", 0);
    definition.increment = std::max(1, JsonIntFlexible(item, "increment", 1));
    definition.initial_stage = JsonIntFlexible(item, "initial_stage", JsonIntFlexible(item, "initial_stage_idx", 0));
    definition.loop_stages = JsonBoolFlexible(item, "loop_stages", JsonBoolFlexible(item, "loop", false));
    definition.static_only = definition.type == "static";
    definition.allow_disabled = JsonBoolFlexible(item, "allow_disabled", true);
    definition.initial_active_state = JsonBoolFlexible(item, "initial_active_state", false);

    std::vector<std::string> aliases;
    std::vector<std::string> item_aliases;
    std::vector<std::string> stage_aliases;
    std::vector<std::string> image_aliases;
    std::vector<std::string> label_aliases;
    AppendCodeList(item_aliases, ParseCodeList(item, "codes"));
    AppendCodeList(item_aliases, ParseCodeList(item, "secondary_codes"));
    AppendCodeList(item_aliases, ParseCodeList(item, "secondaryCodes"));
    AppendCodeList(aliases, ParseCodeList(item, "aliases"));
    AppendCodeList(aliases, item_aliases);
    if (!definition.primary_code.empty()) {
      AppendUniqueCode(aliases, definition.primary_code);
    }

    for (const auto& stage : item.value("stages", nlohmann::json::array())) {
      if (!stage.is_object()) {
        continue;
      }
      const int stage_index = static_cast<int>(definition.stages.size());
      definition.stages.push_back(NormalizePackAssetPath(JsonStringAtAnyKey(stage, {"image", "img"})));
      definition.disabled_stages.push_back(
          NormalizePackAssetPath(JsonStringAtAnyKey(stage, {"disabled_image", "disabledImage", "disabled_img"})));
      definition.stage_mods.push_back(JsonStringAtAnyKey(stage, {"image_mods", "imageMods", "img_mods"}));
      definition.disabled_stage_mods.push_back(
          JsonStringAtAnyKey(stage, {"disabled_image_mods", "disabledImageMods", "disabled_img_mods"}));
      std::vector<std::string> current_stage_aliases;
      AppendCodeList(current_stage_aliases, ParseCodeList(stage, "codes"));
      AppendCodeList(current_stage_aliases, ParseCodeList(stage, "secondary_codes"));
      AppendCodeList(current_stage_aliases, ParseCodeList(stage, "secondaryCodes"));
      for (const auto& alias : current_stage_aliases) {
        AppendUniqueCode(stage_aliases, alias);
        if (!definition.stage_index_by_alias.contains(alias)) {
          definition.stage_index_by_alias[alias] = stage_index;
        }
      }
    }
    AppendCodeList(aliases, stage_aliases);

    for (const auto& image : item.value("images", nlohmann::json::array())) {
      if (!image.is_object()) {
        continue;
      }
      const auto normalized = NormalizePackAssetPath(JsonStringAtAnyKey(image, {"image", "img"}));
      const bool left = JsonBoolFlexible(image, "left", false);
      const bool right = JsonBoolFlexible(image, "right", false);
      if (left && right) {
        definition.composite_both_image = normalized;
        definition.composite_both_mods = JsonStringAtAnyKey(image, {"image_mods", "imageMods", "img_mods"});
      } else if (left) {
        definition.composite_left_image = normalized;
        definition.composite_left_mods = JsonStringAtAnyKey(image, {"image_mods", "imageMods", "img_mods"});
      } else if (right) {
        definition.composite_right_image = normalized;
        definition.composite_right_mods = JsonStringAtAnyKey(image, {"image_mods", "imageMods", "img_mods"});
      } else {
        definition.composite_none_image = normalized;
        definition.composite_none_mods = JsonStringAtAnyKey(image, {"image_mods", "imageMods", "img_mods"});
      }
      AppendCodeList(image_aliases, ParseCodeList(image, "codes"));
      AppendCodeList(image_aliases, ParseCodeList(image, "secondary_codes"));
      AppendCodeList(image_aliases, ParseCodeList(image, "secondaryCodes"));
    }
    AppendCodeList(aliases, image_aliases);
    if (!definition.label.empty()) {
      AppendUniqueCode(label_aliases, CanonicalToken(definition.label));
      AppendCodeList(aliases, label_aliases);
    }
    if (definition.primary_code.empty() && !aliases.empty()) {
      definition.primary_code = aliases.front();
    }
    if (definition.primary_code.empty() && aliases.empty()) {
      continue;
    }

    RegisterSnapshotVisualDefinition(context, definition.primary_code, definition, 110);
    for (const auto& code : item_aliases) {
      RegisterSnapshotVisualDefinition(context, code, definition, 100);
    }
    for (const auto& code : stage_aliases) {
      RegisterSnapshotVisualDefinition(context, code, definition, 80);
    }
    for (const auto& code : image_aliases) {
      RegisterSnapshotVisualDefinition(context, code, definition, 70);
    }
    for (const auto& code : aliases) {
      RegisterSnapshotVisualDefinition(context, code, definition, 60);
    }
    for (const auto& code : label_aliases) {
      RegisterSnapshotVisualDefinition(context, code, definition, 50);
    }
  }
}

bool LoadSnapshotLayoutDefinitions(const nlohmann::json& snapshot, PackLayoutDocument& document) {
  const nlohmann::json* layouts = nullptr;
  for (const char* path : {"pack_layouts", "packLayouts", "tracker.pack_layouts",
                           "tracker_state.pack_layouts"}) {
    const auto* value = JsonValueAtPath(snapshot, path);
    if (value != nullptr && value->is_array()) {
      layouts = value;
      break;
    }
  }
  if (layouts == nullptr) {
    return false;
  }

  for (const auto& entry : *layouts) {
    if (!entry.is_object()) {
      continue;
    }
    const auto* raw_layouts = JsonValueAtPath(entry, "json");
    if (raw_layouts == nullptr || !raw_layouts->is_object()) {
      raw_layouts = JsonValueAtPath(entry, "layouts");
    }
    if (raw_layouts == nullptr || !raw_layouts->is_object()) {
      continue;
    }
    for (auto it = raw_layouts->begin(); it != raw_layouts->end(); ++it) {
      if (it.value().is_object()) {
        document.layouts[it.key()] = it.value();
      }
    }
  }
  return !document.layouts.empty();
}

bool SnapshotProvidesPackVisuals(const TrackerRuntime& runtime) {
  return SnapshotHasArrayAtAnyPath(runtime.AuthoritativeState().snapshot,
                                   {"pack_item_visuals", "packItemVisuals", "tracker.pack_item_visuals",
                                    "tracker_state.pack_item_visuals"});
}

const PackLayoutDocument& PackLayoutForBundle(const TrackerBundle& bundle, bool include_visual_fallbacks) {
  static std::unordered_map<std::string, PackLayoutDocument> cache;
  const auto key = bundle.bundle_root.string() + (include_visual_fallbacks ? "|full" : "|layout-only");
  if (const auto found = cache.find(key); found != cache.end()) {
    return found->second;
  }
  auto& document = cache[key];
  LoadLayoutDefinitions(document, bundle.bundle_root);
  if (include_visual_fallbacks) {
    LoadVisualDefinitions(document, bundle.bundle_root);
    LoadItemMappings(document, bundle.bundle_root);
  }
  return document;
}

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
