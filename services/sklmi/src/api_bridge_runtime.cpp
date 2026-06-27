#include "api_internal.hpp"
#include "api_bridge_runtime_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sekailink::sklmi {

namespace {

constexpr std::uint64_t kStateSaveTickInterval = 60;
constexpr std::uint64_t kTraceHeartbeatTickInterval = 300;
constexpr std::size_t kMaxRoomChatMessages = 64;

}  // namespace

MinimalBridgeSession::MinimalBridgeSession(std::string domain_id, std::uint64_t watch_address, std::byte trigger_value, std::uint64_t inject_address)
    : domain_id_(std::move(domain_id)),
      watch_address_(watch_address),
      trigger_value_(trigger_value),
      inject_address_(inject_address) {}

RuntimeStatus MinimalBridgeSession::start(MemoryProvider& provider, EventSink& sink) {
    provider_ = &provider;
    sink_ = &sink;
    connected_ = provider.has_domain(domain_id_) && provider.is_address_valid(domain_id_, watch_address_, 1);
    if (!connected_) return {RuntimeConnectionState::error, "missing_or_invalid_domain"};
    sink.trace({LogLevel::info, "MinimalBridgeSession", "start", "connected_to_domain", 0, 0});
    sink.emit({EventType::slot_connected, "domain", domain_id_});
    return {RuntimeConnectionState::connected, "started"};
}

RuntimeStatus MinimalBridgeSession::tick(const TickContext& context) {
    if (!connected_ || provider_ == nullptr || sink_ == nullptr) return {RuntimeConnectionState::disconnected, "not_started"};

    std::byte current{};
    if (!provider_->read(domain_id_, watch_address_, &current, 1)) {
        connected_ = false;
        sink_->trace({LogLevel::error, "MinimalBridgeSession", "tick", "watch_read_failed", context.tick_index, context.monotonic_ms});
        sink_->emit({EventType::error, "read", "watch_failed"});
        return {RuntimeConnectionState::error, "watch_read_failed"};
    }

    if (!check_emitted_ && current == trigger_value_) {
        check_emitted_ = true;
        sink_->emit({EventType::location_checked, "watch_address", std::to_string(watch_address_)});
    }

    if (!item_injected_ && inject_address_ > 0) {
        const std::byte injected = std::byte{0x01};
        if (provider_->write(domain_id_, inject_address_, &injected, 1)) {
            item_injected_ = true;
            sink_->emit({EventType::item_received, "inject_address", std::to_string(inject_address_)});
        }
    }

    sink_->trace({LogLevel::trace, "MinimalBridgeSession", "tick", "tick_ok", context.tick_index, context.monotonic_ms});
    sink_->emit({EventType::trace, "tick", std::to_string(context.tick_index)});
    return {RuntimeConnectionState::connected, "tick_ok"};
}

RuntimeStatus MinimalBridgeSession::reset() {
    check_emitted_ = false;
    item_injected_ = false;
    if (sink_ != nullptr) {
        sink_->trace({LogLevel::info, "MinimalBridgeSession", "reset", "bridge_state_reset", 0, 0});
        sink_->emit({EventType::runtime_reset, "state", "reset"});
    }
    return {connected_ ? RuntimeConnectionState::connected : RuntimeConnectionState::stopped, "reset"};
}

RuntimeStatus MinimalBridgeSession::stop() {
    connected_ = false;
    provider_ = nullptr;
    sink_ = nullptr;
    return {RuntimeConnectionState::stopped, "stopped"};
}

bool MinimalBridgeSession::load_state(const std::filesystem::path&) {
    return true;
}

bool MinimalBridgeSession::save_state(const std::filesystem::path&) const {
    return true;
}

ManifestBridgeSession::ManifestBridgeSession(BridgeManifest manifest) : manifest_(std::move(manifest)) {}

RuntimeStatus ManifestBridgeSession::start(MemoryProvider& provider, EventSink& sink) {
    provider_ = &provider;
    sink_ = &sink;
    connected_ = true;
    for (const auto& rule : manifest_.checks) {
        if (rule.dynamic_source == "firered_saveblock1_flag") {
            if (!provider.has_domain(rule.domain_id) ||
                !provider.is_address_valid(rule.domain_id, 0x03003078u, 1) ||
                !provider.is_address_valid(rule.domain_id, 0x03004F58u, 4)) {
                connected_ = false;
                sink.trace({LogLevel::error, manifest_.bridge_id, "start", "invalid_dynamic_check_rule_domain_or_address", 0, 0});
                return {RuntimeConnectionState::error, "invalid_check_rule"};
            }
            continue;
        }
        if (!provider.has_domain(rule.domain_id) || !provider.is_address_valid(rule.domain_id, rule.address, static_cast<std::size_t>(rule.size))) {
            connected_ = false;
            sink.trace({LogLevel::error, manifest_.bridge_id, "start", "invalid_check_rule_domain_or_address", 0, 0});
            return {RuntimeConnectionState::error, "invalid_check_rule"};
        }
    }
    for (const auto& rule : manifest_.context_watches) {
        if (!provider.has_domain(rule.domain_id) ||
            !provider.is_address_valid(rule.domain_id, rule.address, static_cast<std::size_t>(rule.size))) {
            connected_ = false;
            sink.trace({LogLevel::error, manifest_.bridge_id, "start", "invalid_context_rule_domain_or_address", 0, 0});
            return {RuntimeConnectionState::error, "invalid_context_rule"};
        }
    }
    for (const auto& rule : manifest_.injections) {
        if (rule.dynamic_source == "firered_item_queue") {
            if (!provider.has_domain(rule.domain_id) ||
                !provider.is_address_valid(rule.domain_id, 0x03003078u, 1) ||
                !provider.is_address_valid(rule.domain_id, 0x03004F58u, 4) ||
                !provider.is_address_valid(rule.domain_id, 0x0203F714u, 6)) {
                connected_ = false;
                sink.trace({LogLevel::error, manifest_.bridge_id, "start", "invalid_dynamic_injection_rule_domain_or_address", 0, 0});
                return {RuntimeConnectionState::error, "invalid_injection_rule"};
            }
        } else if (!rule.writes.empty()) {
            for (const auto& step : rule.writes) {
                if (!provider.has_domain(step.domain_id) ||
                    !provider.is_address_valid(step.domain_id, step.address, static_cast<std::size_t>(step.size))) {
                    connected_ = false;
                    sink.trace({LogLevel::error, manifest_.bridge_id, "start", "invalid_injection_rule_domain_or_address", 0, 0});
                    return {RuntimeConnectionState::error, "invalid_injection_rule"};
                }
            }
        } else if (!provider.has_domain(rule.domain_id) || !provider.is_address_valid(rule.domain_id, rule.address, static_cast<std::size_t>(rule.size))) {
            connected_ = false;
            sink.trace({LogLevel::error, manifest_.bridge_id, "start", "invalid_injection_rule_domain_or_address", 0, 0});
            return {RuntimeConnectionState::error, "invalid_injection_rule"};
        }
    }
    sink.trace({LogLevel::info, manifest_.bridge_id, "start", "manifest_bridge_started", 0, 0});
    sink.emit({EventType::slot_connected,
               "linkedworld_id",
               manifest_.linkedworld_id,
               manifest_.driver_instance_id,
               manifest_.linkedworld_id,
               manifest_.core_profile.name,
               0});
    return {RuntimeConnectionState::connected, "started"};
}

RuntimeStatus ManifestBridgeSession::tick(const TickContext& context) {
    if (!connected_ || provider_ == nullptr || sink_ == nullptr) return {RuntimeConnectionState::disconnected, "not_started"};
    if (manifest_.poll_interval_ms > 0 && last_tick_ms_ != 0 && context.monotonic_ms < last_tick_ms_ + manifest_.poll_interval_ms) {
        return {RuntimeConnectionState::connected, "tick_skipped"};
    }
    last_tick_ms_ = context.monotonic_ms;

    const auto domain_descriptors = BuildDomainDescriptorMap(*provider_);
    auto read_snapshot = BuildCheckReadSnapshotSpans(manifest_, domain_descriptors);
    ReadCheckSnapshot(*provider_, read_snapshot);

    for (const auto& rule : manifest_.checks) {
        auto current = rule.dynamic_source == "firered_saveblock1_flag"
            ? ReadFireredSaveBlock1Flag(*provider_, rule)
            : ReadUnsignedFromSnapshot(read_snapshot, rule);
        if (!current.has_value() && rule.dynamic_source.empty()) {
            current = detail::read_unsigned_from_provider(*provider_, rule.domain_id, rule.address, static_cast<std::size_t>(rule.size));
        }
        const auto event_key = detail::default_check_event_key(rule);
        if (current.has_value() && !event_key.empty() && detail::evaluate_compare(rule.compare, *current, rule.operand_u64) &&
            !emitted_checks_[event_key]) {
            emitted_checks_[event_key] = true;
            sink_->emit(detail::make_watch_event(manifest_, rule, *current));
        }
    }

    for (const auto& rule : manifest_.context_watches) {
        const auto current = detail::read_unsigned_from_provider(*provider_,
                                                                 rule.domain_id,
                                                                 rule.address,
                                                                 static_cast<std::size_t>(rule.size));
        if (!current.has_value()) {
            continue;
        }
        const auto state_key = ContextWatchStateKey(rule);
        const auto seen = context_values_.find(state_key);
        if (seen != context_values_.end() && seen->second == *current) {
            continue;
        }
        context_values_[state_key] = *current;
        const auto* mapping = FindContextMapping(rule, *current);
        if (mapping == nullptr) {
            continue;
        }
        sink_->emit(detail::make_context_event(manifest_, rule, *mapping, *current));
    }

    for (const auto& rule : manifest_.injections) {
        if (rule.room_controlled) continue;
        const auto event_key = detail::default_injection_event_key(rule);
        if (event_key.empty() || injected_items_[event_key]) continue;
        bool injected = false;
        if (rule.dynamic_source == "firered_item_queue") {
            injected = ApplyFireredItemQueueInjection(*provider_, rule, rule.value_u64);
        } else if (!rule.writes.empty()) {
            injected = true;
            for (const auto& step : rule.writes) {
                if (!detail::apply_inject_write(*provider_, step)) {
                    injected = false;
                    break;
                }
            }
        } else {
            injected = detail::write_unsigned_to_provider(*provider_, rule.domain_id, rule.address, rule.value_u64,
                                                          static_cast<std::size_t>(rule.size));
        }
        if (injected) {
            injected_items_[event_key] = true;
            sink_->emit(detail::make_injection_event(manifest_, rule, rule.value_u64));
        }
    }

    if (context.tick_index % kTraceHeartbeatTickInterval == 0) {
        sink_->trace({LogLevel::trace, manifest_.bridge_id, "tick", "manifest_tick_ok", context.tick_index, context.monotonic_ms});
    }
    return {RuntimeConnectionState::connected, "tick_ok"};
}

RuntimeStatus ManifestBridgeSession::reset() {
    emitted_checks_.clear();
    context_values_.clear();
    injected_items_.clear();
    last_tick_ms_ = 0;
    if (sink_ != nullptr) {
        sink_->trace({LogLevel::info, manifest_.bridge_id, "reset", "manifest_bridge_state_reset", 0, 0});
        sink_->emit({EventType::runtime_reset,
                     "state",
                     "reset",
                     manifest_.driver_instance_id,
                     manifest_.linkedworld_id,
                     manifest_.core_profile.name,
                     0});
    }
    return {connected_ ? RuntimeConnectionState::connected : RuntimeConnectionState::stopped, "reset"};
}

RuntimeStatus ManifestBridgeSession::stop() {
    connected_ = false;
    provider_ = nullptr;
    sink_ = nullptr;
    return {RuntimeConnectionState::stopped, "stopped"};
}

bool ManifestBridgeSession::load_state(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path)) return false;
    std::ifstream input(path);
    if (!input) return false;
    emitted_checks_.clear();
    context_values_.clear();
    injected_items_.clear();
    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "check") emitted_checks_[key] = (value == "1");
        if (kind == "inject") injected_items_[key] = (value == "1");
        if (kind == "meta" && key == "last_tick_ms") {
            const auto parsed = detail::parse_u64(value);
            if (parsed.has_value()) last_tick_ms_ = *parsed;
        }
    }
    return true;
}

bool ManifestBridgeSession::save_state(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    if (!output) return false;
    output << "meta|bridge_id|" << detail::state_file_escape(manifest_.bridge_id) << "\n";
    output << "meta|last_tick_ms|" << last_tick_ms_ << "\n";
    for (const auto& [key, value] : emitted_checks_) output << "check|" << detail::state_file_escape(key) << "|" << (value ? "1" : "0") << "\n";
    for (const auto& [key, value] : injected_items_) output << "inject|" << detail::state_file_escape(key) << "|" << (value ? "1" : "0") << "\n";
    return true;
}

ManifestLinkedWorldBridge::ManifestLinkedWorldBridge(BridgeManifest manifest) : manifest_(std::move(manifest)) {}

const BridgeManifest& ManifestLinkedWorldBridge::manifest() const {
    return manifest_;
}

std::unique_ptr<BridgeSession> ManifestLinkedWorldBridge::create_session() const {
    return std::make_unique<ManifestBridgeSession>(manifest_);
}

BasicRuntimeSession::BasicRuntimeSession(MemoryProvider& provider, EventSink& sink, std::unique_ptr<BridgeSession> bridge, std::filesystem::path state_path)
    : provider_(provider), sink_(sink), bridge_(std::move(bridge)), state_path_(std::move(state_path)) {}

RuntimeStatus BasicRuntimeSession::start() {
    status_ = {RuntimeConnectionState::starting, "starting"};
    status_ = bridge_->start(provider_, sink_);
    if (status_.state == RuntimeConnectionState::connected && !state_path_.empty()) {
        bridge_->load_state(state_path_);
    }
    return status_;
}

RuntimeStatus BasicRuntimeSession::tick(const TickContext& context) {
    status_ = bridge_->tick(context);
    if (status_.state == RuntimeConnectionState::connected && !state_path_.empty() &&
        IsTickDue(context.tick_index, last_state_save_tick_, kStateSaveTickInterval)) {
        bridge_->save_state(state_path_);
        last_state_save_tick_ = context.tick_index;
    }
    return status_;
}

RuntimeStatus BasicRuntimeSession::reconnect() {
    bridge_->stop();
    status_ = bridge_->start(provider_, sink_);
    if (status_.state == RuntimeConnectionState::connected && !state_path_.empty()) {
        bridge_->load_state(state_path_);
    }
    last_state_save_tick_ = 0;
    sink_.trace({LogLevel::info, "BasicRuntimeSession", "reconnect", status_.detail, 0, 0});
    return status_;
}

RuntimeStatus BasicRuntimeSession::reset() {
    status_ = bridge_->reset();
    if (!state_path_.empty()) {
        bridge_->save_state(state_path_);
        last_state_save_tick_ = 0;
    }
    return status_;
}

RuntimeStatus BasicRuntimeSession::stop() {
    if (!state_path_.empty()) {
        bridge_->save_state(state_path_);
    }
    status_ = bridge_->stop();
    return status_;
}

RuntimeStatus BasicRuntimeSession::status() const {
    return status_;
}

RoomSynchronizedRuntimeSession::RoomSynchronizedRuntimeSession(MemoryProvider& provider,
                                                               EventSink& sink,
                                                               std::unique_ptr<BridgeSession> bridge,
                                                               std::unique_ptr<RoomClient> room_client,
                                                               std::vector<InjectRule> room_injections,
                                                               std::filesystem::path bridge_state_path,
                                                               std::filesystem::path room_state_path,
                                                               std::string driver_instance_id,
                                                               std::string linkedworld_id,
                                                               std::string core_profile)
    : provider_(provider),
      sink_(sink),
      bridge_(std::move(bridge)),
      room_client_(std::move(room_client)),
      room_injections_(std::move(room_injections)),
      bridge_state_path_(std::move(bridge_state_path)),
      room_state_path_(std::move(room_state_path)),
      driver_instance_id_(std::move(driver_instance_id)),
      linkedworld_id_(std::move(linkedworld_id)),
      core_profile_(std::move(core_profile)) {}

RuntimeStatus RoomSynchronizedRuntimeSession::start() {
    status_ = {RuntimeConnectionState::starting, "starting"};
    std::string room_error;
    if (room_client_ != nullptr && !room_client_->connect(&room_error)) {
        sink_.trace({LogLevel::error, "RoomSynchronizedRuntimeSession", "room_connect", room_error, 0, 0});
        status_ = {RuntimeConnectionState::error, room_error.empty() ? "room_connect_failed" : room_error};
        return status_;
    }
    load_room_sync_state();
    refresh_room_sync_metadata();
    ReconcileReportedChecksWithRoomMetadata(reported_checks_, room_sync_metadata_);
    sink_.trace({LogLevel::info, "RoomSynchronizedRuntimeSession", "room_metadata_ready",
                 DescribeRoomMetadataSnapshot(room_sync_metadata_), 0, 0});
    bridge_sink_ = std::make_unique<BufferedForwardingEventSink>(sink_);
    status_ = bridge_->start(provider_, *bridge_sink_);
    return status_;
}

RuntimeStatus RoomSynchronizedRuntimeSession::tick(const TickContext& context) {
    if (bridge_sink_ == nullptr) {
        sink_.trace({LogLevel::error, "RoomSynchronizedRuntimeSession", "tick", "missing_bridge_sink",
                     context.tick_index, context.monotonic_ms});
        status_ = {RuntimeConnectionState::error, "missing_bridge_sink"};
        return status_;
    }

    // Inbound room items should not wait behind a full memory scan.
    bool room_state_dirty = drain_room_items(context);
    room_state_dirty = drain_room_chat(context) || room_state_dirty;

    bridge_sink_->clear();
    status_ = bridge_->tick(context);
    if (status_.state != RuntimeConnectionState::connected) {
        return status_;
    }
    const bool bridge_state_dirty = !bridge_sink_->emitted_events().empty();

    for (const auto& event : bridge_sink_->emitted_events()) {
        if (event.type != EventType::location_checked) {
            continue;
        }
        const auto report_key = RoomReportKey(event);
        if (reported_checks_.contains(report_key)) {
            continue;
        }
        std::string error;
        if (room_client_ != nullptr && room_client_->report_location_checked(event, &error)) {
            reported_checks_[report_key] = event.value;
            room_state_dirty = true;
        } else {
            sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "report_location_checked",
                         error.empty() ? "room_report_failed" : error, context.tick_index, context.monotonic_ms});
        }
    }

    room_state_dirty = drain_room_items(context) || room_state_dirty;
    room_state_dirty = drain_room_chat(context) || room_state_dirty;

    if (!bridge_state_path_.empty() &&
        (bridge_state_dirty || IsTickDue(context.tick_index, last_bridge_state_save_tick_, kStateSaveTickInterval))) {
        bridge_->save_state(bridge_state_path_);
        last_bridge_state_save_tick_ = context.tick_index;
    }
    refresh_room_sync_metadata();
    if (room_state_dirty ||
        IsTickDue(context.tick_index, last_room_sync_state_save_tick_, kStateSaveTickInterval)) {
        save_room_sync_state();
        last_room_sync_state_save_tick_ = context.tick_index;
    }
    return status_;
}

bool RoomSynchronizedRuntimeSession::drain_room_items(const TickContext& context) {
    if (room_client_ == nullptr) return false;

    std::string error;
    bool dirty = false;
    for (const auto& item : room_client_->poll_pending_items(&error)) {
        sink_.trace({LogLevel::info, "RoomSynchronizedRuntimeSession", "room_item_pending",
                     DescribeRoomItem(item), context.tick_index, context.monotonic_ms});
        if (IsSelfOriginItem(item, room_sync_metadata_) &&
            !reported_checks_.contains(std::to_string(item.ap_location_id))) {
            sink_.trace({LogLevel::trace, "RoomSynchronizedRuntimeSession", "room_item_self_origin_deferred",
                         DescribeRoomItem(item), context.tick_index, context.monotonic_ms});
            continue;
        }
        if (IsSelfOriginAlreadyReportedItem(item, room_sync_metadata_, reported_checks_)) {
            if (!room_client_->acknowledge_item(item.item_id, &error)) {
                sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "acknowledge_item",
                             error.empty() ? "room_ack_failed" : error, context.tick_index, context.monotonic_ms});
            } else {
                sink_.trace({LogLevel::info, "RoomSynchronizedRuntimeSession", "room_item_self_origin_suppressed",
                             DescribeRoomItem(item), context.tick_index, context.monotonic_ms});
            }
            sink_.emit(MakeRoomItemTrackerEvent(item,
                                                driver_instance_id_,
                                                linkedworld_id_,
                                                core_profile_));
            applied_items_[item.item_id] = "self_origin_suppressed";
            dirty = true;
            continue;
        }
        if (!apply_room_item(item, context)) {
            continue;
        }
        if (!room_client_->acknowledge_item(item.item_id, &error)) {
            sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "acknowledge_item",
                         error.empty() ? "room_ack_failed" : error, context.tick_index, context.monotonic_ms});
        } else {
            sink_.trace({LogLevel::info, "RoomSynchronizedRuntimeSession", "room_item_acknowledged",
                         DescribeRoomItem(item), context.tick_index, context.monotonic_ms});
        }
        applied_items_[item.item_id] = !item.mapped_value.empty() ? item.mapped_value
                                       : !item.item_name.empty() ? item.item_name
                                                                 : std::to_string(item.value_u64);
        dirty = true;
    }
    if (!error.empty()) {
        sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "poll_pending_items", error,
                     context.tick_index, context.monotonic_ms});
    }
    return dirty;
}

bool RoomSynchronizedRuntimeSession::drain_room_chat(const TickContext& context) {
    if (room_client_ == nullptr) return false;

    std::string error;
    bool dirty = false;
    for (auto message : room_client_->poll_pending_chat(&error)) {
        if (message.text.empty()) {
            continue;
        }
        if (message.id == 0) {
            message.id = next_room_chat_id_++;
        } else {
            next_room_chat_id_ = std::max(next_room_chat_id_, message.id + 1);
        }
        room_chat_messages_.push_back(std::move(message));
        while (room_chat_messages_.size() > kMaxRoomChatMessages) {
            room_chat_messages_.erase(room_chat_messages_.begin());
        }
        dirty = true;
    }
    if (dirty) {
        room_sync_metadata_["chat_messages"] = BuildRoomChatMessagesJson(room_chat_messages_);
    }
    if (!error.empty()) {
        sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "poll_pending_chat", error,
                     context.tick_index, context.monotonic_ms});
    }
    return dirty;
}

RuntimeStatus RoomSynchronizedRuntimeSession::reconnect() {
    if (room_client_ != nullptr) {
        room_client_->disconnect();
        std::string error;
        room_client_->connect(&error);
        if (!error.empty()) {
            sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "room_reconnect", error, 0, 0});
        }
    }
    bridge_->stop();
    if (bridge_sink_ == nullptr) {
        bridge_sink_ = std::make_unique<BufferedForwardingEventSink>(sink_);
    } else {
        bridge_sink_->clear();
    }
    status_ = bridge_->start(provider_, *bridge_sink_);
    load_room_sync_state();
    refresh_room_sync_metadata();
    ReconcileReportedChecksWithRoomMetadata(reported_checks_, room_sync_metadata_);
    last_bridge_state_save_tick_ = 0;
    last_room_sync_state_save_tick_ = 0;
    sink_.trace({LogLevel::info, "RoomSynchronizedRuntimeSession", "reconnect", status_.detail, 0, 0});
    return status_;
}

RuntimeStatus RoomSynchronizedRuntimeSession::reset() {
    status_ = bridge_->reset();
    if (!bridge_state_path_.empty()) {
        bridge_->save_state(bridge_state_path_);
        last_bridge_state_save_tick_ = 0;
    }
    save_room_sync_state();
    last_room_sync_state_save_tick_ = 0;
    return status_;
}

RuntimeStatus RoomSynchronizedRuntimeSession::stop() {
    if (room_client_ != nullptr) {
        room_client_->disconnect();
    }
    if (!bridge_state_path_.empty()) {
        bridge_->save_state(bridge_state_path_);
        last_bridge_state_save_tick_ = 0;
    }
    save_room_sync_state();
    last_room_sync_state_save_tick_ = 0;
    status_ = bridge_->stop();
    bridge_sink_.reset();
    return status_;
}

RuntimeStatus RoomSynchronizedRuntimeSession::status() const {
    return status_;
}

bool RoomSynchronizedRuntimeSession::send_chat_message(std::string_view text, std::string* error) {
    if (room_client_ == nullptr) {
        if (error) *error = "room_client_missing";
        return false;
    }
    return room_client_->send_chat_message(text, error);
}

bool RoomSynchronizedRuntimeSession::load_room_sync_state() {
    room_sync_metadata_.clear();
    reported_checks_.clear();
    applied_items_.clear();
    if (room_state_path_.empty() || !std::filesystem::exists(room_state_path_)) {
        return false;
    }
    std::ifstream input(room_state_path_);
    if (!input) {
        return false;
    }
    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "meta") {
            room_sync_metadata_[key] = value;
        } else if (kind == "reported") {
            reported_checks_[key] = value;
        } else if (kind == "applied") {
            applied_items_[key] = value;
        }
    }
    return true;
}

bool RoomSynchronizedRuntimeSession::save_room_sync_state() const {
    if (room_state_path_.empty()) {
        return false;
    }
    std::filesystem::create_directories(room_state_path_.parent_path());
    std::ofstream output(room_state_path_, std::ios::trunc);
    if (!output) {
        return false;
    }
    std::vector<std::string> metadata_keys;
    metadata_keys.reserve(room_sync_metadata_.size());
    for (const auto& [key, _] : room_sync_metadata_) {
        metadata_keys.push_back(key);
    }
    std::sort(metadata_keys.begin(), metadata_keys.end());
    for (const auto& key : metadata_keys) {
        output << "meta|" << detail::state_file_escape(key) << "|" << detail::state_file_escape(room_sync_metadata_.at(key)) << "\n";
    }
    for (const auto& [key, value] : reported_checks_) {
        output << "reported|" << detail::state_file_escape(key) << "|" << detail::state_file_escape(value) << "\n";
    }
    for (const auto& [key, value] : applied_items_) {
        output << "applied|" << detail::state_file_escape(key) << "|" << detail::state_file_escape(value) << "\n";
    }
    return true;
}

void RoomSynchronizedRuntimeSession::refresh_room_sync_metadata() {
    if (!driver_instance_id_.empty()) {
        room_sync_metadata_["driver_instance_id"] = driver_instance_id_;
    }
    if (!linkedworld_id_.empty()) {
        room_sync_metadata_["linkedworld_id"] = linkedworld_id_;
    }
    if (!core_profile_.empty()) {
        room_sync_metadata_["core_profile"] = core_profile_;
    }
    if (room_client_ == nullptr) {
        return;
    }
    for (const auto& [key, value] : room_client_->metadata_snapshot()) {
        if (!key.empty()) {
            room_sync_metadata_[key] = value;
        }
    }
}

bool RoomSynchronizedRuntimeSession::apply_room_item(const RoomItem& item, const TickContext& context) {
    if (applied_items_.contains(item.item_id)) {
        return false;
    }
    const auto it = std::find_if(room_injections_.begin(), room_injections_.end(), [&](const auto& rule) {
        if (!rule.room_controlled) {
            return false;
        }
        if (item.ap_item_id != 0 && rule.item_id == item.ap_item_id) {
            return true;
        }
        if (!item.item_name.empty() && rule.item_name == item.item_name) {
            return true;
        }
        if (!item.event_key.empty() && rule.event_key == item.event_key) {
            return true;
        }
        return !item.mapped_value.empty() && rule.mapped_value == item.mapped_value;
    });
    if (it == room_injections_.end()) {
        std::ostringstream detail;
        detail << "no_matching_injection_rule "
               << DescribeRoomItem(item)
               << " candidates=";
        bool first = true;
        for (const auto& rule : room_injections_) {
            if (!rule.room_controlled) {
                continue;
            }
            if (!first) {
                detail << ";";
            }
            first = false;
            detail << DescribeInjectionRule(rule);
        }
        sink_.trace({LogLevel::warning, "RoomSynchronizedRuntimeSession", "apply_room_item", detail.str(),
                     context.tick_index, context.monotonic_ms});
        return false;
    }

    const auto value = item.value_u64 == 0 ? it->value_u64 : item.value_u64;
    const bool uses_write_sequence = !it->writes.empty();
    const bool uses_dynamic_injection = !it->dynamic_source.empty();
    if (uses_dynamic_injection) {
        if (!ApplyFireredItemQueueInjection(provider_, *it, value)) {
            std::ostringstream detail;
            detail << "provider_write_failed"
                   << " mode=" << it->dynamic_source
                   << " item_id=" << item.item_id
                   << " domain_id=" << it->domain_id;
            sink_.trace({LogLevel::error, "RoomSynchronizedRuntimeSession", "apply_room_item", detail.str(),
                         context.tick_index, context.monotonic_ms});
            return false;
        }
    } else if (uses_write_sequence) {
        for (std::size_t step_index = 0; step_index < it->writes.size(); ++step_index) {
            const auto& step = it->writes[step_index];
            if (detail::apply_inject_write(provider_, step)) {
                continue;
            }

            std::ostringstream detail;
            detail << "provider_write_failed"
                   << " mode=write_sequence"
                   << " item_id=" << item.item_id
                   << " step_index=" << step_index
                   << " domain_id=" << step.domain_id
                   << " address=" << step.address
                   << " source=" << step.source;
            sink_.trace({LogLevel::error, "RoomSynchronizedRuntimeSession", "apply_room_item", detail.str(),
                         context.tick_index, context.monotonic_ms});
            return false;
        }
    } else if (!detail::write_unsigned_to_provider(
                   provider_, it->domain_id, it->address, value, static_cast<std::size_t>(it->size))) {
        std::ostringstream detail;
        detail << "provider_write_failed"
               << " mode=direct"
               << " item_id=" << item.item_id
               << " domain_id=" << it->domain_id
               << " address=" << it->address
               << " size=" << it->size;
        sink_.trace({LogLevel::error, "RoomSynchronizedRuntimeSession", "apply_room_item", detail.str(),
                     context.tick_index, context.monotonic_ms});
        return false;
    }

    std::ostringstream applied_detail;
    applied_detail << "item_id=" << item.item_id
                   << " event_key=" << (item.event_key.empty() ? detail::default_injection_event_key(*it) : item.event_key)
                   << " mapped_value=" << (item.mapped_value.empty() ? detail::default_injection_value(*it, value) : item.mapped_value)
                   << " mode=" << (uses_dynamic_injection ? it->dynamic_source : uses_write_sequence ? "write_sequence" : "direct")
                   << " steps=" << (uses_write_sequence ? it->writes.size() : 1);
    sink_.trace({LogLevel::info, "RoomSynchronizedRuntimeSession", "room_item_applied", applied_detail.str(),
                 context.tick_index, context.monotonic_ms});

    const auto fallback_key = detail::default_injection_event_key(*it);
    const auto fallback_value = detail::default_injection_value(*it, value);
    sink_.emit(MakeRoomItemTrackerEvent(item,
                                        driver_instance_id_,
                                        linkedworld_id_,
                                        core_profile_,
                                        fallback_key,
                                        fallback_value,
                                        it->item_id));
    return true;
}

}  // namespace sekailink::sklmi
