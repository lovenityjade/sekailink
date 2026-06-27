#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekailink::sklmi {

enum class Endianness {
    little,
    big,
};

struct MemoryDomainDescriptor {
    std::string id;
    std::string display_name;
    std::size_t size_bytes = 0;
    Endianness endianness = Endianness::little;
    bool writable = false;
};

class MemoryProvider {
  public:
    virtual ~MemoryProvider() = default;

    virtual std::vector<MemoryDomainDescriptor> domains() const = 0;
    virtual bool has_domain(std::string_view domain_id) const = 0;
    virtual bool is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const = 0;
    virtual bool read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const = 0;
    virtual bool write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) = 0;
};

enum class RuntimeConnectionState {
    stopped,
    starting,
    connected,
    disconnected,
    error,
};

struct RuntimeStatus {
    RuntimeConnectionState state = RuntimeConnectionState::stopped;
    std::string detail;
};

enum class LogLevel {
    trace,
    info,
    warning,
    error,
};

struct TraceRecord {
    LogLevel level = LogLevel::info;
    std::string source;
    std::string event;
    std::string detail;
    std::uint64_t tick_index = 0;
    std::uint64_t monotonic_ms = 0;
};

enum class EventType {
    location_checked,
    item_received,
    map_changed,
    slot_connected,
    runtime_reset,
    trace,
    error,
};

struct Event {
    EventType type = EventType::trace;
    std::string key;
    std::string value;
    std::string driver_instance_id;
    std::string linkedworld_id;
    std::string core_profile;
    std::uint64_t canonical_id = 0;
    std::uint64_t player_number = 0;
    std::string tab_id;
    std::string map_id;
    std::string zone_id;
};

enum class CompareOp {
    equals,
    not_equals,
    greater_than,
    greater_or_equal,
    less_than,
    less_or_equal,
    mask_any,
    mask_all,
};

class EventSink {
  public:
    virtual ~EventSink() = default;
    virtual void emit(const Event& event) = 0;
    virtual void trace(const TraceRecord& record) = 0;
};

class BufferedForwardingEventSink final : public EventSink {
  public:
    explicit BufferedForwardingEventSink(EventSink& inner);

    void emit(const Event& event) override;
    void trace(const TraceRecord& record) override;
    const std::vector<Event>& emitted_events() const;
    void clear();

  private:
    EventSink& inner_;
    std::vector<Event> emitted_events_;
};

struct TickContext {
    std::uint64_t tick_index = 0;
    std::uint64_t monotonic_ms = 0;
};

class BridgeSession {
  public:
    virtual ~BridgeSession() = default;

    virtual RuntimeStatus start(MemoryProvider& provider, EventSink& sink) = 0;
    virtual RuntimeStatus tick(const TickContext& context) = 0;
    virtual RuntimeStatus reset() = 0;
    virtual RuntimeStatus stop() = 0;
    virtual bool load_state(const std::filesystem::path& path) = 0;
    virtual bool save_state(const std::filesystem::path& path) const = 0;
};

class RuntimeSession {
  public:
    virtual ~RuntimeSession() = default;

    virtual RuntimeStatus start() = 0;
    virtual RuntimeStatus tick(const TickContext& context) = 0;
    virtual RuntimeStatus reconnect() = 0;
    virtual RuntimeStatus reset() = 0;
    virtual RuntimeStatus stop() = 0;
    virtual RuntimeStatus status() const = 0;
};

struct RoomItem {
    std::string item_id;
    std::string event_key;
    std::string mapped_value;
    std::uint64_t value_u64 = 0;
    std::uint64_t ap_item_id = 0;
    std::uint64_t ap_location_id = 0;
    std::uint64_t ap_player_id = 0;
    std::string item_name;
};

struct RoomChatMessage {
    std::uint64_t id = 0;
    std::string author;
    std::string text;
    std::string kind;
};

struct ArchipelagoConnectOptions {
    std::string game;
    std::string slot_name;
    std::string player_alias;
    std::string password;
    std::string uuid;
    std::uint64_t version_major = 0;
    std::uint64_t version_minor = 6;
    std::uint64_t version_build = 3;
    std::uint64_t items_handling = 7;
    std::vector<std::string> tags = {"AP"};
};

class ArchipelagoTransport {
  public:
    virtual ~ArchipelagoTransport() = default;
    virtual void disconnect() {}
    virtual bool send_text(std::string_view payload, std::string* error = nullptr) = 0;
    virtual std::optional<std::string> receive_text(std::string* error = nullptr) = 0;
};

class TcpWebSocketArchipelagoTransport final : public ArchipelagoTransport {
  public:
    TcpWebSocketArchipelagoTransport(std::string host, std::uint16_t port, std::string path = "/");
    ~TcpWebSocketArchipelagoTransport() override;

    bool connect(std::string* error = nullptr);
    void disconnect();
    [[nodiscard]] bool connected() const;

    bool send_text(std::string_view payload, std::string* error = nullptr) override;
    std::optional<std::string> receive_text(std::string* error = nullptr) override;

  private:
    bool ensure_connected(std::string* error = nullptr);
    bool write_all(const std::byte* data, std::size_t size, std::string* error = nullptr);
    std::optional<std::vector<std::byte>> read_exact(std::size_t size, std::string* error = nullptr);

    std::string host_;
    std::uint16_t port_ = 0;
    std::string path_;
    std::intptr_t socket_ = -1;
    bool handshake_complete_ = false;
};

class RoomClient {
  public:
    virtual ~RoomClient() = default;

    virtual bool connect(std::string* error = nullptr) = 0;
    virtual void disconnect() = 0;
    [[nodiscard]] virtual bool connected() const = 0;
    virtual bool report_location_checked(const Event& event, std::string* error = nullptr) = 0;
    virtual std::vector<RoomItem> poll_pending_items(std::string* error = nullptr) = 0;
    virtual bool acknowledge_item(std::string_view item_id, std::string* error = nullptr) = 0;
    virtual std::vector<RoomChatMessage> poll_pending_chat(std::string* error = nullptr) = 0;
    virtual bool send_chat_message(std::string_view text, std::string* error = nullptr) = 0;
    [[nodiscard]] virtual std::unordered_map<std::string, std::string> metadata_snapshot() const = 0;
};

class ArchipelagoRoomClient final : public RoomClient {
  public:
    ArchipelagoRoomClient(std::unique_ptr<ArchipelagoTransport> transport, ArchipelagoConnectOptions options);

    bool connect(std::string* error = nullptr) override;
    void disconnect() override;
    [[nodiscard]] bool connected() const override;
    bool report_location_checked(const Event& event, std::string* error = nullptr) override;
    std::vector<RoomItem> poll_pending_items(std::string* error = nullptr) override;
    bool acknowledge_item(std::string_view item_id, std::string* error = nullptr) override;
    std::vector<RoomChatMessage> poll_pending_chat(std::string* error = nullptr) override;
    bool send_chat_message(std::string_view text, std::string* error = nullptr) override;
    [[nodiscard]] std::unordered_map<std::string, std::string> metadata_snapshot() const override;

  private:
    bool process_packet(std::string_view packet, std::string* error = nullptr);
    bool send_connect_packet(std::string* error = nullptr);
    bool send_data_package_request(std::string* error = nullptr);

    std::unique_ptr<ArchipelagoTransport> transport_;
    ArchipelagoConnectOptions options_;
    bool connected_ = false;
    bool connect_sent_ = false;
    bool data_package_request_sent_ = false;
    std::uint64_t received_index_ = 0;
    std::uint64_t team_ = 0;
    std::uint64_t slot_ = 0;
    std::unordered_map<std::string, std::string> metadata_;
    std::unordered_map<std::uint64_t, std::string> item_id_to_name_;
    std::unordered_map<std::uint64_t, std::string> location_id_to_name_;
    std::unordered_map<std::uint64_t, std::string> player_id_to_name_;
    std::vector<RoomItem> pending_items_;
    std::vector<RoomChatMessage> pending_chat_;
    std::uint64_t chat_message_counter_ = 0;
    std::unordered_map<std::string, bool> acknowledged_items_;
};

struct WatchRule {
    std::string domain_id;
    std::uint64_t address = 0;
    std::uint64_t size = 1;
    std::string dynamic_source;
    std::optional<std::uint64_t> dynamic_flag_id;
    CompareOp compare = CompareOp::equals;
    std::uint64_t operand_u64 = 0;
    EventType event_type = EventType::location_checked;
    std::string event_key;
    std::string mapped_value;
    std::uint64_t location_id = 0;
    std::string location_name;
};

struct ContextValueMapping {
    std::uint64_t value = 0;
    std::optional<std::uint64_t> min_value;
    std::optional<std::uint64_t> max_value;
    std::string event_key;
    std::string mapped_value;
    std::string tab_id;
    std::string map_id;
    std::string zone_id;
};

struct ContextWatchRule {
    std::string domain_id;
    std::uint64_t address = 0;
    std::uint64_t size = 1;
    std::string context_key;
    EventType event_type = EventType::map_changed;
    std::vector<ContextValueMapping> values;
};

struct InjectRule {
    struct WriteStep {
        std::string domain_id;
        std::uint64_t address = 0;
        std::uint64_t value_u64 = 0;
        std::uint64_t size = 1;
        std::string source = "constant";
        std::uint64_t delta_u64 = 0;
    };

    std::string domain_id;
    std::uint64_t address = 0;
    std::uint64_t value_u64 = 0;
    std::uint64_t size = 1;
    std::string dynamic_source;
    std::string event_key;
    std::string mapped_value;
    std::uint64_t item_id = 0;
    std::string item_name;
    bool room_controlled = false;
    std::vector<WriteStep> writes;
};

struct CoreProfileManifest {
    std::string name;
    std::vector<std::string> domain_ids;
};

struct ArchipelagoClientWrapperConfig {
    bool enabled = false;
    std::string game_key;
    std::string game;
    std::string platform;
    std::string world;
    std::string wrapper;
    std::string module;
    std::string client_file;
    std::string status;
    std::vector<std::string> dependency_hints;
};

struct BridgeManifest {
    std::string contract_version;
    std::string linkedworld_id;
    std::string linkedworld_version;
    std::string bridge_id;
    std::string driver_instance_id;
    std::string source_module;
    std::string module;
    CoreProfileManifest core_profile;
    ArchipelagoClientWrapperConfig archipelago_client_wrapper;
    std::string state_file;
    std::uint64_t poll_interval_ms = 16;
    std::vector<WatchRule> checks;
    std::vector<ContextWatchRule> context_watches;
    std::vector<InjectRule> injections;
};

class LinkedWorldBridge {
  public:
    virtual ~LinkedWorldBridge() = default;
    virtual const BridgeManifest& manifest() const = 0;
    virtual std::unique_ptr<BridgeSession> create_session() const = 0;
};

class FakeMemoryProvider final : public MemoryProvider {
  public:
    void add_domain(MemoryDomainDescriptor descriptor);
    bool set_domain_bytes(std::string_view domain_id, const std::vector<std::byte>& bytes);

    std::vector<MemoryDomainDescriptor> domains() const override;
    bool has_domain(std::string_view domain_id) const override;
    bool is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const override;
    bool read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const override;
    bool write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) override;
    std::optional<std::uint64_t> read_unsigned(std::string_view domain_id, std::uint64_t address, std::size_t size) const;
    bool write_unsigned(std::string_view domain_id, std::uint64_t address, std::uint64_t value, std::size_t size);

  private:
    std::unordered_map<std::string, MemoryDomainDescriptor> descriptors_;
    std::unordered_map<std::string, std::vector<std::byte>> memory_;
};

class RuntimeSocketMemoryProvider final : public MemoryProvider {
  public:
    RuntimeSocketMemoryProvider(std::string host, std::uint16_t port);
    ~RuntimeSocketMemoryProvider();

    bool connect(std::string* error = nullptr);
    void disconnect();
    [[nodiscard]] bool connected() const;
    [[nodiscard]] std::optional<int> protocol_version() const;
    [[nodiscard]] const std::string& system_name() const;

    std::vector<MemoryDomainDescriptor> domains() const override;
    bool has_domain(std::string_view domain_id) const override;
    bool is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const override;
    bool read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const override;
    bool write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) override;

  private:
    bool refresh_remote_metadata(std::string* error);

    std::string host_;
    std::uint16_t port_ = 0;
    std::intptr_t socket_ = -1;
    std::optional<int> protocol_version_;
    std::string system_name_;
    std::unordered_map<std::string, MemoryDomainDescriptor> descriptors_;
};

class UnixSocketMemoryProvider final : public MemoryProvider {
  public:
    explicit UnixSocketMemoryProvider(std::filesystem::path socket_path);
    ~UnixSocketMemoryProvider();

    bool connect(std::string* error = nullptr);
    void disconnect();
    [[nodiscard]] bool connected() const;
    [[nodiscard]] std::optional<int> protocol_version() const;
    [[nodiscard]] const std::string& system_name() const;

    std::vector<MemoryDomainDescriptor> domains() const override;
    bool has_domain(std::string_view domain_id) const override;
    bool is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const override;
    bool read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const override;
    bool write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) override;

  private:
    bool refresh_remote_metadata(std::string* error);

    std::filesystem::path socket_path_;
    int socket_ = -1;
    std::optional<int> protocol_version_;
    std::string system_name_;
    std::unordered_map<std::string, MemoryDomainDescriptor> descriptors_;
};

class OfflineRoomClient final : public RoomClient {
  public:
    explicit OfflineRoomClient(std::filesystem::path state_path);

    bool connect(std::string* error = nullptr) override;
    void disconnect() override;
    [[nodiscard]] bool connected() const override;
    bool report_location_checked(const Event& event, std::string* error = nullptr) override;
    std::vector<RoomItem> poll_pending_items(std::string* error = nullptr) override;
    bool acknowledge_item(std::string_view item_id, std::string* error = nullptr) override;
    std::vector<RoomChatMessage> poll_pending_chat(std::string* error = nullptr) override;
    bool send_chat_message(std::string_view text, std::string* error = nullptr) override;
    [[nodiscard]] std::unordered_map<std::string, std::string> metadata_snapshot() const override;

  private:
    bool load_state(std::string* error = nullptr);
    bool save_state(std::string* error = nullptr) const;

    std::filesystem::path state_path_;
    bool connected_ = false;
    std::unordered_map<std::string, std::string> metadata_;
    std::unordered_map<std::string, std::string> checked_locations_;
    std::unordered_map<std::string, RoomItem> pending_items_;
    std::unordered_map<std::string, RoomItem> consumed_items_;
};

class GameServerRoomClient final : public RoomClient {
  public:
    GameServerRoomClient(std::string host,
                         std::uint16_t control_port,
                         std::uint16_t runtime_port,
                         std::string session_name,
                         int slot_id,
                         std::string control_channel,
                         std::string control_auth_token,
                         std::string runtime_auth_token,
                         std::string driver_instance_id,
                         std::string linkedworld_id,
                         std::string core_profile,
                         std::string runtime_session_token = {});

    bool connect(std::string* error = nullptr) override;
    void disconnect() override;
    [[nodiscard]] bool connected() const override;
    bool report_location_checked(const Event& event, std::string* error = nullptr) override;
    std::vector<RoomItem> poll_pending_items(std::string* error = nullptr) override;
    bool acknowledge_item(std::string_view item_id, std::string* error = nullptr) override;
    std::vector<RoomChatMessage> poll_pending_chat(std::string* error = nullptr) override;
    bool send_chat_message(std::string_view text, std::string* error = nullptr) override;
    [[nodiscard]] std::unordered_map<std::string, std::string> metadata_snapshot() const override;

  private:
    bool issue_runtime_ticket(std::string* error);
    std::optional<std::string> canonical_id_from_event(const Event& event) const;
    std::optional<std::string> send_request(std::uint16_t port,
                                            const std::string& channel,
                                            const std::string& auth_token,
                                            const std::string& command_json,
                                            std::string* error) const;

    std::string host_;
    std::uint16_t control_port_ = 0;
    std::uint16_t runtime_port_ = 0;
    std::string session_name_;
    int slot_id_ = 0;
    std::string control_channel_;
    std::string control_auth_token_;
    std::string runtime_auth_token_;
    std::string driver_instance_id_;
    std::string linkedworld_id_;
    std::string core_profile_;
    std::string runtime_session_token_;
    std::unordered_map<std::string, std::string> metadata_;
    bool connected_ = false;
};

class VectorEventSink final : public EventSink {
  public:
    void emit(const Event& event) override;
    void trace(const TraceRecord& record) override;
    const std::vector<Event>& events() const;
    const std::vector<TraceRecord>& traces() const;

  private:
    std::vector<Event> events_;
    std::vector<TraceRecord> traces_;
};

class MinimalBridgeSession final : public BridgeSession {
  public:
    MinimalBridgeSession(std::string domain_id, std::uint64_t watch_address, std::byte trigger_value, std::uint64_t inject_address);

    RuntimeStatus start(MemoryProvider& provider, EventSink& sink) override;
    RuntimeStatus tick(const TickContext& context) override;
    RuntimeStatus reset() override;
    RuntimeStatus stop() override;
    bool load_state(const std::filesystem::path& path) override;
    bool save_state(const std::filesystem::path& path) const override;

  private:
    std::string domain_id_;
    std::uint64_t watch_address_ = 0;
    std::byte trigger_value_{};
    std::uint64_t inject_address_ = 0;
    MemoryProvider* provider_ = nullptr;
    EventSink* sink_ = nullptr;
    bool connected_ = false;
    bool check_emitted_ = false;
    bool item_injected_ = false;
};

class ManifestBridgeSession final : public BridgeSession {
  public:
    explicit ManifestBridgeSession(BridgeManifest manifest);

    RuntimeStatus start(MemoryProvider& provider, EventSink& sink) override;
    RuntimeStatus tick(const TickContext& context) override;
    RuntimeStatus reset() override;
    RuntimeStatus stop() override;
    bool load_state(const std::filesystem::path& path) override;
    bool save_state(const std::filesystem::path& path) const override;

  private:
    BridgeManifest manifest_;
    MemoryProvider* provider_ = nullptr;
    EventSink* sink_ = nullptr;
    bool connected_ = false;
    std::unordered_map<std::string, bool> emitted_checks_;
    std::unordered_map<std::string, std::uint64_t> context_values_;
    std::unordered_map<std::string, bool> injected_items_;
    std::uint64_t last_tick_ms_ = 0;
};

class ManifestLinkedWorldBridge final : public LinkedWorldBridge {
  public:
    explicit ManifestLinkedWorldBridge(BridgeManifest manifest);

    const BridgeManifest& manifest() const override;
    std::unique_ptr<BridgeSession> create_session() const override;

  private:
    BridgeManifest manifest_;
};

class BasicRuntimeSession final : public RuntimeSession {
  public:
    BasicRuntimeSession(MemoryProvider& provider, EventSink& sink, std::unique_ptr<BridgeSession> bridge, std::filesystem::path state_path = {});

    RuntimeStatus start() override;
    RuntimeStatus tick(const TickContext& context) override;
    RuntimeStatus reconnect() override;
    RuntimeStatus reset() override;
    RuntimeStatus stop() override;
    RuntimeStatus status() const override;

  private:
    MemoryProvider& provider_;
    EventSink& sink_;
    std::unique_ptr<BridgeSession> bridge_;
    std::filesystem::path state_path_;
    RuntimeStatus status_{};
    std::uint64_t last_state_save_tick_ = 0;
};

class RoomSynchronizedRuntimeSession final : public RuntimeSession {
  public:
    RoomSynchronizedRuntimeSession(MemoryProvider& provider,
                                   EventSink& sink,
                                   std::unique_ptr<BridgeSession> bridge,
                                   std::unique_ptr<RoomClient> room_client,
                                   std::vector<InjectRule> room_injections,
                                   std::filesystem::path bridge_state_path = {},
                                   std::filesystem::path room_state_path = {},
                                   std::string driver_instance_id = {},
                                   std::string linkedworld_id = {},
                                   std::string core_profile = {});

    RuntimeStatus start() override;
    RuntimeStatus tick(const TickContext& context) override;
    RuntimeStatus reconnect() override;
    RuntimeStatus reset() override;
    RuntimeStatus stop() override;
    RuntimeStatus status() const override;
    bool send_chat_message(std::string_view text, std::string* error = nullptr);

  private:
    bool load_room_sync_state();
    bool save_room_sync_state() const;
    bool apply_room_item(const RoomItem& item, const TickContext& context);
    bool drain_room_items(const TickContext& context);
    bool drain_room_chat(const TickContext& context);
    void refresh_room_sync_metadata();

    MemoryProvider& provider_;
    EventSink& sink_;
    std::unique_ptr<BridgeSession> bridge_;
    std::unique_ptr<RoomClient> room_client_;
    std::vector<InjectRule> room_injections_;
    std::filesystem::path bridge_state_path_;
    std::filesystem::path room_state_path_;
    std::unique_ptr<BufferedForwardingEventSink> bridge_sink_;
    RuntimeStatus status_{};
    std::unordered_map<std::string, std::string> room_sync_metadata_;
    std::unordered_map<std::string, std::string> reported_checks_;
    std::unordered_map<std::string, std::string> applied_items_;
    std::vector<RoomChatMessage> room_chat_messages_;
    std::uint64_t next_room_chat_id_ = 1;
    std::string driver_instance_id_;
    std::string linkedworld_id_;
    std::string core_profile_;
    std::uint64_t last_bridge_state_save_tick_ = 0;
    std::uint64_t last_room_sync_state_save_tick_ = 0;
};

std::optional<BridgeManifest> load_bridge_manifest(const std::filesystem::path& path, std::string* error = nullptr);
bool validate_bridge_manifest(const BridgeManifest& manifest, std::string* error = nullptr);
std::string format_trace_record(const TraceRecord& record);
std::string format_compare_op(CompareOp value);
std::optional<CompareOp> parse_compare_op(std::string_view value);
std::string format_event_type(EventType value);
std::optional<EventType> parse_event_type(std::string_view value);

}  // namespace sekailink::sklmi
