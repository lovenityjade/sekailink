bool TrackerHeadlessRuntime::Initialize(const TrackerHeadlessRuntimeConfig& config, std::string* error) {
    config_ = config;
    resolved_bundle_path_.clear();
    extracted_bundle_root_.clear();
    bundle_ = TrackerBundleModel{};
    state_ = TrackerHeadlessRuntimeState{};
    command_log_offset_ = 0;
    last_snapshot_json_.clear();
    last_logic_state_text_.clear();
    logic_state_cached_ = false;
    autosave_dirty_ = false;
    static_snapshot_payload_published_ = false;
    if (!ResolveBundlePath(error)) {
        return false;
    }
    if (!LoadBundleModel(resolved_bundle_path_, error)) {
        return false;
    }
    state_.tracker_pack = config.bundle_path.filename().string();
    state_.tracker_variant = config.tracker_variant;
    state_.player_alias = config.initial_player_alias;
    state_.active_tab_id = bundle_.default_tab_id;
    state_.active_map_id = bundle_.default_map_id;
    for (const auto& slot : bundle_.item_slots) {
        state_.item_slots.emplace(slot.slot_id, TrackerItemSlotState{});
    }
    LoadRoomMetadata();
    const bool restored_autosave = LoadAutosaveState();
    if (restored_autosave && !config_.command_log_path.empty()) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(config_.command_log_path, ec);
        if (!ec) {
            command_log_offset_ = size;
        }
    }
    return true;
}

void TrackerHeadlessRuntime::SetChatMessageSender(std::function<bool(std::string_view, std::string*)> sender) {
    config_.send_chat_message = std::move(sender);
}
