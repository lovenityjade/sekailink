#include "tracker_pack_layout_visual_state.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_pack_layout_document.hpp"
#include "tracker_pack_layout_engine.hpp"
#include "tracker_pack_layout_json.hpp"

#include <algorithm>
#include <string>

namespace sekaiemu::spike::tracker_pack_layout_detail {
namespace {

bool CodeAcquiredFromMappings(const PackStateContext& context, std::string_view code, int* count) {
  int total = 0;
  if (const auto binding_it = context.document->item_ids_by_code.find(std::string(code));
      binding_it != context.document->item_ids_by_code.end()) {
    for (const auto item_id : binding_it->second) {
      const auto slot_it = context.slot_id_by_item_id.find(item_id);
      if (slot_it == context.slot_id_by_item_id.end()) {
        continue;
      }
      if (const auto snapshot_it = context.snapshot_items_by_id.find(slot_it->second);
          snapshot_it != context.snapshot_items_by_id.end()) {
        const auto& item = snapshot_it->second;
        const bool active = item.has_explicit_state
                                ? item.acquired
                                : (item.acquired || item.stage > 0 || item.count > 0);
        if (active) {
          if (item.count > 0) {
            total += static_cast<int>(item.count);
          } else if (item.stage > 0) {
            total = std::max(total, static_cast<int>(item.stage));
          } else {
            total += 1;
          }
        }
      } else if (context.received_item_ids.contains(std::to_string(item_id))) {
        total += 1;
      }
    }
  }
  if (count != nullptr) {
    *count = total;
  }
  return total > 0;
}

}  // namespace

void BuildPackStateContext(PackStateContext& context) {
  context.bundle = context.runtime.Bundle();
  if (context.document == nullptr && context.bundle != nullptr) {
    context.document = &PackLayoutForBundle(*context.bundle, !SnapshotProvidesPackVisuals(context.runtime));
  }
  LoadSnapshotVisualDefinitions(context);
  for (const auto& item : BuildBundleItems(context.runtime)) {
    const auto remember_item = [&](const std::string& id) {
      if (!id.empty()) {
        context.snapshot_items_by_id[id] = item;
      }
    };
    remember_item(item.id);
    remember_item(item.code);
      remember_item(item.pack_visual_code);
  }
  if (context.bundle == nullptr) {
    for (const auto& entry : context.runtime.AuthoritativeState().checked_locations) {
      context.checked_location_ids.insert(std::to_string(entry));
    }
    context.received_item_ids = ReceivedItemIds(context.runtime);
    return;
  }
  for (const auto& slot : context.bundle->raw.value("item_slots", nlohmann::json::array())) {
    if (!slot.is_object()) {
      continue;
    }
    const auto slot_id = JsonStringAtAnyKey(slot, {"slot_id", "slotId", "id"});
    if (slot_id.empty()) {
      continue;
    }
    for (const auto& item : slot.value("items", nlohmann::json::array())) {
      if (!item.is_object()) {
        continue;
      }
      const auto item_id = JsonIntFlexible(item, "item_id", 0);
      if (item_id == 0) {
        continue;
      }
      context.slot_item_ids[slot_id].push_back(item_id);
      context.slot_id_by_item_id[item_id] = slot_id;
    }
  }
  for (const auto& [code, item_ids] : context.document->item_ids_by_code) {
    for (const auto item_id : item_ids) {
      const auto slot_it = context.slot_id_by_item_id.find(item_id);
      if (slot_it != context.slot_id_by_item_id.end() &&
          !context.poptracker_code_by_slot_id.contains(slot_it->second)) {
        context.poptracker_code_by_slot_id[slot_it->second] = code;
      }
    }
  }
  for (const auto& entry : context.runtime.AuthoritativeState().checked_locations) {
    context.checked_location_ids.insert(std::to_string(entry));
  }
  context.received_item_ids = ReceivedItemIds(context.runtime);
  if (const auto* pack_maps = JsonValueAtPath(context.runtime.AuthoritativeState().snapshot, "pack_maps");
      pack_maps != nullptr && pack_maps->is_array()) {
    for (const auto& map : *pack_maps) {
      if (!map.is_object()) {
        continue;
      }
      const auto name = JsonStringAtAnyKey(map, {"name", "id"});
      if (name.empty()) {
        continue;
      }
      std::string bundle_map;
      if (ResolvePackMapId(*context.bundle, name, &bundle_map)) {
        context.bundle_map_by_pack_map[name] = bundle_map;
      }
    }
  }
}

const PackVisualDefinition* FindPackVisualDefinition(const PackStateContext& context,
                                                     std::string_view code) {
  const std::string key(code);
  if (const auto snapshot_it = context.snapshot_visuals.find(key);
      snapshot_it != context.snapshot_visuals.end()) {
    return &snapshot_it->second;
  }
  if (context.document != nullptr) {
    if (const auto document_it = context.document->visuals.find(key);
        document_it != context.document->visuals.end()) {
      return &document_it->second;
    }
  }
  return nullptr;
}

PackVisualState ResolvePackVisualState(const PackStateContext& context,
                                       std::string_view code,
                                       std::unordered_set<std::string>* visiting) {
  PackVisualState state;
  const std::string code_key(code);
  state.label = code_key;
  if (visiting != nullptr && !visiting->insert(code_key).second) {
    return state;
  }
  const auto unwind = [&]() {
    if (visiting != nullptr) {
      visiting->erase(code_key);
    }
  };

  const auto* definition_ptr = FindPackVisualDefinition(context, code_key);
  if (definition_ptr != nullptr && !definition_ptr->label.empty()) {
    state.label = definition_ptr->label;
  }

  if (definition_ptr != nullptr && definition_ptr->type == "composite_toggle") {
    const auto left = ResolvePackVisualState(context, definition_ptr->item_left, visiting);
    const auto right = ResolvePackVisualState(context, definition_ptr->item_right, visiting);
    state.composite_left = left.acquired;
    state.composite_right = right.acquired;
    state.acquired = state.composite_left || state.composite_right;
    state.count = (state.composite_left ? 1 : 0) + (state.composite_right ? 1 : 0);
    unwind();
    return state;
  }

  if (const auto snapshot_it = context.snapshot_items_by_id.find(code_key);
      snapshot_it != context.snapshot_items_by_id.end()) {
    const auto& item = snapshot_it->second;
    state.acquired = item.acquired;
    state.stage = static_cast<int>(item.stage);
    state.count = static_cast<int>(item.count);
    state.label = item.label;
    unwind();
    return state;
  }

  const auto slot_alias_it = context.poptracker_code_by_slot_id.find(code_key);
  if (slot_alias_it != context.poptracker_code_by_slot_id.end()) {
    if (const auto snapshot_it = context.snapshot_items_by_id.find(slot_alias_it->first);
        snapshot_it != context.snapshot_items_by_id.end()) {
      const auto& item = snapshot_it->second;
      state.acquired = item.acquired;
      state.stage = static_cast<int>(item.stage);
      state.count = static_cast<int>(item.count);
      state.label = item.label;
      unwind();
      return state;
    }
  }

  if (definition_ptr != nullptr) {
    const auto& definition = *definition_ptr;
    const auto stage_hint_it = definition.stage_index_by_alias.find(code_key);
    const bool has_stage_hint = stage_hint_it != definition.stage_index_by_alias.end();
    if (!definition.primary_code.empty() && definition.primary_code != code_key) {
      if (const auto snapshot_it = context.snapshot_items_by_id.find(definition.primary_code);
          snapshot_it != context.snapshot_items_by_id.end()) {
        const auto& item = snapshot_it->second;
        state.acquired = item.acquired;
        state.stage = static_cast<int>(item.stage);
        state.count = static_cast<int>(item.count);
        state.label = item.label.empty() ? state.label : item.label;
        if (has_stage_hint) {
          state.acquired = state.acquired && state.stage >= stage_hint_it->second;
          state.stage = stage_hint_it->second;
        }
        unwind();
        return state;
      }
    }
    int mapped_count = 0;
    state.acquired = CodeAcquiredFromMappings(context, code, &mapped_count);
    state.count = mapped_count;
    if (definition.initial_active_state && mapped_count == 0) {
      state.acquired = true;
    }
    if (definition.type == "toggle_badged" && !definition.base_item.empty()) {
      const auto base = ResolvePackVisualState(context, definition.base_item, visiting);
      state.stage = base.stage;
      state.count = std::max(state.count, base.count);
      if (state.label.empty() && base.acquired) {
        state.label = base.label;
      }
    }
    if (definition.type.find("progressive") != std::string::npos) {
      if (!definition.stages.empty()) {
        if (has_stage_hint) {
          state.stage =
              std::clamp(stage_hint_it->second, 0, static_cast<int>(definition.stages.size() - 1));
        } else if (definition.loop_stages) {
          state.stage = mapped_count % static_cast<int>(definition.stages.size());
        } else {
          state.stage =
              std::clamp(mapped_count, 0, static_cast<int>(definition.stages.size() - 1));
        }
      } else {
        state.stage = mapped_count;
      }
    } else if (definition.type == "consumable") {
      state.count = definition.initial_quantity + mapped_count * definition.increment;
      if (state.count < definition.min_quantity && (definition.initial_quantity > 0 || mapped_count > 0)) {
        state.count = definition.min_quantity;
      }
      state.acquired = state.acquired || state.count > 0 || definition.initial_active_state;
    } else if (definition.static_only) {
      state.acquired = true;
      state.count = std::max(definition.initial_quantity, definition.min_quantity);
    } else if (has_stage_hint) {
      state.stage = stage_hint_it->second;
    }
    if (definition.type == "progressive_toggle" && state.stage == 0 && !has_stage_hint) {
      state.stage = definition.initial_stage;
    }
  }
  unwind();
  return state;
}

VisualAssetSelection SelectVisualAsset(const PackVisualDefinition& definition, const PackVisualState& state) {
  if (definition.type == "composite_toggle") {
    if (state.composite_left && state.composite_right) {
      if (!definition.composite_both_image.empty()) {
        return {definition.composite_both_image, definition.composite_both_mods};
      }
      if (definition.stages.size() >= 4) {
        return {definition.stages[3], definition.stage_mods.size() >= 4 ? definition.stage_mods[3] : std::string{}};
      }
    }
    if (state.composite_left) {
      if (!definition.composite_left_image.empty()) {
        return {definition.composite_left_image, definition.composite_left_mods};
      }
      if (definition.stages.size() >= 2) {
        return {definition.stages[1], definition.stage_mods.size() >= 2 ? definition.stage_mods[1] : std::string{}};
      }
    }
    if (state.composite_right) {
      if (!definition.composite_right_image.empty()) {
        return {definition.composite_right_image, definition.composite_right_mods};
      }
      if (definition.stages.size() >= 3) {
        return {definition.stages[2], definition.stage_mods.size() >= 3 ? definition.stage_mods[2] : std::string{}};
      }
    }
    if (!definition.composite_none_image.empty()) {
      return {definition.composite_none_image, definition.composite_none_mods};
    }
  }
  if (!definition.stages.empty()) {
    const std::size_t stage_index =
        static_cast<std::size_t>(std::clamp(state.stage, 0, static_cast<int>(definition.stages.size() - 1)));
    const bool has_disabled_stage =
        stage_index < definition.disabled_stages.size() && !definition.disabled_stages[stage_index].empty();
    if (state.acquired || !has_disabled_stage) {
      return {definition.stages[stage_index],
              stage_index < definition.stage_mods.size() ? definition.stage_mods[stage_index] : std::string{}};
    }
    return {definition.disabled_stages[stage_index],
            stage_index < definition.disabled_stage_mods.size() ? definition.disabled_stage_mods[stage_index]
                                                                : std::string{}};
  }
  if (!state.acquired && !definition.disabled_image.empty()) {
    return {definition.disabled_image, definition.disabled_image_mods};
  }
  return {definition.image, definition.image_mods};
}

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
