#pragma once

#include "sekailink_sklmi/api.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sekailink::sklmi {

struct TrackerHeadlessRuntimeConfig {
    std::filesystem::path bundle_path;
    std::filesystem::path snapshot_path;
    std::filesystem::path command_log_path;
    std::filesystem::path room_state_path;
    std::filesystem::path assets_root;
    std::string tracker_variant;
    std::function<bool(std::string_view, std::string*)> send_chat_message;
};

struct TrackerTabDefinition {
    std::string id;
    std::string label;
    std::string map_id;
};

struct TrackerMapDefinition {
    std::string id;
    std::string label;
    std::string poptracker_map_name;
    std::string image;
    std::string art_origin;
};

struct TrackerItemIdentity {
    std::uint64_t item_id = 0;
    std::string event_key;
    std::string item_name;
    std::string mapped_value;
};

struct TrackerItemSlotDefinition {
    std::string slot_id;
    std::string label;
    std::string group_id;
    std::string behavior;
    std::uint64_t max_stage = 0;
    std::vector<TrackerItemIdentity> items;
};

struct TrackerLocationDefinition {
    std::uint64_t location_id = 0;
    std::string location_name;
    std::string event_key;
    std::string domain_id;
};

struct TrackerLocationGroupDefinition {
    std::string group_id;
    std::string label;
    std::string preferred_tab;
    std::vector<TrackerLocationDefinition> locations;
};

struct TrackerPinLayerDefinition {
    std::string group_id;
    std::string preferred_tab;
    std::string map_id;
    std::uint64_t pin_count = 0;
    std::string pin_kind;
};

struct TrackerItemIconGroupDefinition {
    std::string group_id;
    std::string default_palette;
};

struct TrackerItemIconBindingDefinition {
    std::string slot_id;
    std::string icon_key;
    std::string render_hint;
    std::vector<std::string> asset_candidates;
};

struct TrackerPackItemStageDefinition {
    std::string image;
    std::string disabled_image;
    std::string image_mods;
    std::string disabled_image_mods;
    std::vector<std::string> codes;
    std::vector<std::string> secondary_codes;
};

struct TrackerPackItemImageDefinition {
    std::string image;
    std::string image_mods;
    bool left = false;
    bool right = false;
    std::vector<std::string> codes;
    std::vector<std::string> secondary_codes;
};

struct TrackerPackItemVisualDefinition {
    std::string primary_code;
    std::string name;
    std::string type;
    std::string image;
    std::string disabled_image;
    std::string image_mods;
    std::string disabled_image_mods;
    std::string base_item;
    std::string item_left;
    std::string item_right;
    std::uint64_t initial_quantity = 0;
    std::uint64_t min_quantity = 0;
    std::uint64_t max_quantity = 0;
    std::uint64_t increment = 1;
    std::uint64_t initial_stage = 0;
    bool loop_stages = false;
    bool allow_disabled = true;
    bool initial_active_state = false;
    std::vector<std::string> codes;
    std::vector<std::string> secondary_codes;
    std::vector<std::string> aliases;
    std::vector<TrackerPackItemStageDefinition> stages;
    std::vector<TrackerPackItemImageDefinition> images;
};

struct TrackerPackMapDefinition {
    std::string name;
    std::string image;
};

struct TrackerPackLayoutDefinition {
    std::string file_name;
    std::vector<std::string> layout_ids;
    std::string json;
};

struct TrackerPackItemMappingDefinition {
    std::uint64_t item_id = 0;
    std::vector<std::string> codes;
    std::string behavior;
};

struct TrackerDetailedPinDefinition {
    std::string pin_id;
    std::string group_id;
    std::string pack_location_id;
    std::string section_id;
    std::uint64_t location_id = 0;
    std::string location_name;
    std::string map_id;
    std::string pack_map;
    std::string map_asset;
    std::string pin_kind;
    double x = 0.0;
    double y = 0.0;
    double size = 0.0;
};

struct TrackerBundleModel {
    std::string linkedworld_id;
    std::string display_name;
    std::string default_tab_id;
    std::string default_map_id;
    std::vector<TrackerTabDefinition> tabs;
    std::vector<TrackerMapDefinition> maps;
    std::vector<std::string> tab_order;
    std::vector<TrackerItemSlotDefinition> item_slots;
    std::vector<TrackerLocationGroupDefinition> location_groups;
    std::vector<TrackerPinLayerDefinition> pin_layers;
    std::vector<TrackerItemIconGroupDefinition> item_icon_groups;
    std::vector<TrackerItemIconBindingDefinition> item_icon_bindings;
    std::vector<TrackerPackItemVisualDefinition> pack_item_visuals;
    std::vector<TrackerPackMapDefinition> pack_maps;
    std::vector<TrackerPackLayoutDefinition> pack_layouts;
    std::vector<std::string> supported_pack_maps;
    std::vector<TrackerPackItemMappingDefinition> pack_item_mappings;
    std::vector<TrackerDetailedPinDefinition> detailed_pins;
    bool has_pack_location_mapping = false;
};

struct TrackerItemSlotState {
    std::uint64_t stage = 0;
    std::uint64_t count = 0;
    bool owned = false;
    std::unordered_set<std::string> owned_sources;
};

struct TrackerLogicGroupState {
    std::string color = "red";
    std::uint64_t normal_count = 0;
    std::uint64_t sequence_break_count = 0;
    std::uint64_t inspect_count = 0;
    std::uint64_t none_count = 0;
};

struct TrackerHeadlessRuntimeState {
    std::string slot_id;
    std::string seed_id;
    std::string slot_name;
    std::string player_alias;
    std::string room_id;
    std::string tracker_pack;
    std::string tracker_variant;
    std::string active_tab_id;
    std::string active_map_id;
    std::unordered_map<std::string, std::string> room_metadata;
    std::string slot_data_json;
    bool auto_follow = true;
    std::uint64_t revision = 0;
    std::unordered_map<std::string, TrackerItemSlotState> item_slots;
    std::unordered_map<std::string, TrackerItemSlotState> pack_code_states;
    std::unordered_map<std::string, TrackerLogicGroupState> logic_groups;
    std::unordered_map<std::string, TrackerLogicGroupState> pack_location_states;
    std::unordered_map<std::string, TrackerLogicGroupState> pack_section_states;
    std::vector<TrackerDetailedPinDefinition> runtime_detailed_pins;
    std::unordered_set<std::uint64_t> checked_locations;
    std::unordered_set<std::string> received_delivery_ids;
    std::string last_received_label;
    std::string last_check_label;
    bool logic_ready = false;
};

class TrackerHeadlessRuntime {
  public:
    bool Initialize(const TrackerHeadlessRuntimeConfig& config, std::string* error);

    void ApplyEvent(const Event& event);
    void PollCommands();
    bool PublishSnapshotIfChanged(std::string* error);
    void SetChatMessageSender(std::function<bool(std::string_view, std::string*)> sender);

    const TrackerBundleModel& bundle() const { return bundle_; }
    const TrackerHeadlessRuntimeState& state() const { return state_; }

  private:
    bool LoadBundleModel(const std::filesystem::path& bundle_path, std::string* error);
    void LoadRoomMetadata();
    void ApplyRoomItemIdentity(const TrackerItemIdentity& identity, std::string delivery_id);
    bool ApplyPackItemMapping(const TrackerItemIdentity& identity);
    void ApplyLocationCheck(std::uint64_t canonical_id, std::string_view event_key, std::string_view value);
    void ApplyCommandLine(const std::string& line);
    void ApplyClickItemCommand(std::string_view slot_id, std::string_view button);
    void ApplyClickPinCommand(std::string_view location_or_group_id, std::string_view button);
    std::string ResolveActiveMapId() const;
    std::filesystem::path AutosavePath() const;
    void LoadAutosaveState();
    bool SaveAutosaveState(std::string* error);
    bool ResolveBundlePath(std::string* error);
    bool EvaluatePackLogic(std::string* error);
    std::string BuildSnapshotJson() const;

    TrackerHeadlessRuntimeConfig config_;
    std::filesystem::path resolved_bundle_path_;
    std::filesystem::path extracted_bundle_root_;
    TrackerBundleModel bundle_;
    TrackerHeadlessRuntimeState state_;
    std::uintmax_t command_log_offset_ = 0;
    std::string last_snapshot_json_;
    std::string last_logic_state_text_;
    bool logic_state_cached_ = false;
    bool autosave_dirty_ = false;
};

class TrackerForwardingEventSink final : public EventSink {
  public:
    TrackerForwardingEventSink(EventSink& inner, TrackerHeadlessRuntime& tracker)
        : inner_(inner), tracker_(tracker) {}

    void emit(const Event& event) override;
    void trace(const TraceRecord& record) override;

  private:
    EventSink& inner_;
    TrackerHeadlessRuntime& tracker_;
};

}  // namespace sekailink::sklmi
