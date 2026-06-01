#include "api_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace sekailink::sklmi {

namespace {

void close_socket_fd(int fd) {
#if defined(_WIN32)
    if (fd >= 0) closesocket(fd);
#else
    if (fd >= 0) close(fd);
#endif
}

}  // namespace

OfflineRoomClient::OfflineRoomClient(std::filesystem::path state_path) : state_path_(std::move(state_path)) {}

bool OfflineRoomClient::connect(std::string* error) {
    connected_ = load_state(error);
    if (!connected_ && error != nullptr && error->empty()) {
        *error = "offline_room_connect_failed";
    }
    return connected_;
}

void OfflineRoomClient::disconnect() {
    connected_ = false;
}

bool OfflineRoomClient::connected() const {
    return connected_;
}

std::unordered_map<std::string, std::string> OfflineRoomClient::metadata_snapshot() const {
    auto metadata = metadata_;
    metadata["room_mode"] = "offline";
    metadata["connected"] = connected_ ? "1" : "0";
    return metadata;
}

bool OfflineRoomClient::report_location_checked(const Event& event, std::string* error) {
    if (!connected_ && !connect(error)) {
        return false;
    }
    checked_locations_[event.key] = event.value;
    return save_state(error);
}

std::vector<RoomItem> OfflineRoomClient::poll_pending_items(std::string* error) {
    if (!connected_ && !connect(error)) {
        return {};
    }
    if (!load_state(error)) {
        return {};
    }
    std::vector<RoomItem> out;
    out.reserve(pending_items_.size());
    for (const auto& [id, item] : pending_items_) {
        if (!consumed_items_.contains(id)) {
            out.push_back(item);
        }
    }
    return out;
}

bool OfflineRoomClient::acknowledge_item(std::string_view item_id, std::string* error) {
    if (!connected_ && !connect(error)) {
        return false;
    }
    const auto pending = pending_items_.find(std::string(item_id));
    if (pending == pending_items_.end()) {
        return true;
    }
    consumed_items_[pending->first] = pending->second;
    pending_items_.erase(pending);
    return save_state(error);
}

std::vector<RoomChatMessage> OfflineRoomClient::poll_pending_chat(std::string*) {
    return {};
}

bool OfflineRoomClient::send_chat_message(std::string_view, std::string*) {
    return true;
}

bool OfflineRoomClient::load_state(std::string* error) {
    metadata_.clear();
    checked_locations_.clear();
    pending_items_.clear();
    consumed_items_.clear();
    if (!std::filesystem::exists(state_path_)) {
        std::filesystem::create_directories(state_path_.parent_path());
        std::ofstream output(state_path_, std::ios::trunc);
        if (!output) {
            if (error) *error = "offline_room_state_create_failed";
            return false;
        }
        output << "meta|connected|1\n";
    }
    std::ifstream input(state_path_);
    if (!input) {
        if (error) *error = "offline_room_state_open_failed";
        return false;
    }
    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "meta") {
            metadata_[key] = value;
        } else if (kind == "checked") {
            checked_locations_[key] = value;
        } else if (kind == "pending" || kind == "consumed") {
            RoomItem item;
            item.item_id = key;
            std::stringstream fields(value);
            std::getline(fields, item.event_key, '|');
            std::getline(fields, item.mapped_value, '|');
            const auto raw_value = [&]() {
                std::string part;
                std::getline(fields, part, '|');
                return part;
            }();
            item.value_u64 = detail::parse_u64(raw_value).value_or(0);
            const auto raw_ap_item_id = [&]() {
                std::string part;
                std::getline(fields, part, '|');
                return part;
            }();
            item.ap_item_id = detail::parse_u64(raw_ap_item_id).value_or(0);
            std::getline(fields, item.item_name, '|');
            if (kind == "pending") {
                pending_items_[item.item_id] = std::move(item);
            } else {
                consumed_items_[item.item_id] = std::move(item);
            }
        }
    }
    return true;
}

bool OfflineRoomClient::save_state(std::string* error) const {
    if (state_path_.empty()) {
        if (error) *error = "offline_room_state_missing_path";
        return false;
    }
    std::filesystem::create_directories(state_path_.parent_path());
    std::ofstream output(state_path_, std::ios::trunc);
    if (!output) {
        if (error) *error = "offline_room_state_write_failed";
        return false;
    }
    auto metadata = metadata_;
    metadata["connected"] = "1";
    std::vector<std::string> metadata_keys;
    metadata_keys.reserve(metadata.size());
    for (const auto& [key, _] : metadata) {
        metadata_keys.push_back(key);
    }
    std::sort(metadata_keys.begin(), metadata_keys.end());
    for (const auto& key : metadata_keys) {
        output << "meta|" << detail::state_file_escape(key) << "|" << detail::state_file_escape(metadata.at(key)) << "\n";
    }
    for (const auto& [key, value] : checked_locations_) {
        output << "checked|" << detail::state_file_escape(key) << "|" << detail::state_file_escape(value) << "\n";
    }
    for (const auto& [id, item] : pending_items_) {
        if (consumed_items_.contains(id)) continue;
        output << "pending|" << detail::state_file_escape(id) << "|"
               << detail::state_file_escape(item.event_key) << "|"
               << detail::state_file_escape(item.mapped_value) << "|"
               << item.value_u64 << "|"
               << item.ap_item_id << "|"
               << detail::state_file_escape(item.item_name) << "\n";
    }
    for (const auto& [id, item] : consumed_items_) {
        output << "consumed|" << detail::state_file_escape(id) << "|"
               << detail::state_file_escape(item.event_key) << "|"
               << detail::state_file_escape(item.mapped_value) << "|"
               << item.value_u64 << "|"
               << item.ap_item_id << "|"
               << detail::state_file_escape(item.item_name) << "\n";
    }
    return true;
}

GameServerRoomClient::GameServerRoomClient(std::string host,
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
                                           std::string runtime_session_token)
    : host_(std::move(host)),
      control_port_(control_port),
      runtime_port_(runtime_port),
      session_name_(std::move(session_name)),
      slot_id_(slot_id),
      control_channel_(std::move(control_channel)),
      control_auth_token_(std::move(control_auth_token)),
      runtime_auth_token_(std::move(runtime_auth_token)),
      driver_instance_id_(std::move(driver_instance_id)),
      linkedworld_id_(std::move(linkedworld_id)),
      core_profile_(std::move(core_profile)),
      runtime_session_token_(std::move(runtime_session_token)) {}

bool GameServerRoomClient::connect(std::string* error) {
    if (connected_) {
        return true;
    }
    if (!runtime_session_token_.empty()) {
        connected_ = true;
        return true;
    }
    if (!issue_runtime_ticket(error)) {
        return false;
    }
    connected_ = true;
    return true;
}

void GameServerRoomClient::disconnect() {
    connected_ = false;
}

bool GameServerRoomClient::connected() const {
    return connected_;
}

std::unordered_map<std::string, std::string> GameServerRoomClient::metadata_snapshot() const {
    auto metadata = metadata_;
    metadata["room_mode"] = "sekailink_game_server";
    metadata["room_session_name"] = session_name_;
    metadata["room_slot_id"] = std::to_string(slot_id_);
    metadata["driver_instance_id"] = driver_instance_id_;
    metadata["linkedworld_id"] = linkedworld_id_;
    metadata["core_profile"] = core_profile_;
    metadata["room_ticket_issued"] = runtime_session_token_.empty() ? "0" : "1";
    return metadata;
}

bool GameServerRoomClient::report_location_checked(const Event& event, std::string* error) {
    if (!connected_ && !connect(error)) {
        return false;
    }
    const auto canonical_id = canonical_id_from_event(event);
    if (!canonical_id.has_value()) {
        if (error) *error = "missing_event_canonical_id";
        return false;
    }
    const auto response = send_request(
        runtime_port_,
        "runtime",
        runtime_auth_token_,
        "{"
        "\"cmd\":\"runtime_event\","
        "\"session_name\":\"" + detail::escape_json(session_name_) + "\","
        "\"session_token\":\"" + detail::escape_json(runtime_session_token_) + "\","
        "\"slot_id\":" + std::to_string(slot_id_) + ","
        "\"driver_instance_id\":\"" + detail::escape_json(driver_instance_id_) + "\","
        "\"linkedworld_id\":\"" + detail::escape_json(linkedworld_id_) + "\","
        "\"core_profile\":\"" + detail::escape_json(core_profile_) + "\","
        "\"event_type\":\"location_checked\","
        "\"canonical_id\":" + *canonical_id +
        "}",
        error);
    if (!response.has_value()) {
        return false;
    }
    const auto ok = detail::extract_bool_field(*response, "ok");
    if (!ok.value_or(false)) {
        if (error && error->empty()) {
            *error = detail::extract_string_field(*response, "reason").value_or("runtime_event_rejected");
        }
        return false;
    }
    return true;
}

std::vector<RoomItem> GameServerRoomClient::poll_pending_items(std::string* error) {
    if (!connected_ && !connect(error)) {
        return {};
    }
    const auto response = send_request(
        runtime_port_,
        "runtime",
        runtime_auth_token_,
        "{"
        "\"cmd\":\"pending_items\","
        "\"session_name\":\"" + detail::escape_json(session_name_) + "\","
        "\"slot_id\":" + std::to_string(slot_id_) + ","
        "\"session_token\":\"" + detail::escape_json(runtime_session_token_) + "\""
        "}",
        error);
    if (!response.has_value()) {
        return {};
    }
    if (!detail::extract_bool_field(*response, "ok").value_or(false)) {
        if (error && error->empty()) {
            *error = detail::extract_string_field(*response, "error").value_or("pending_items_failed");
        }
        return {};
    }

    std::vector<RoomItem> items;
    for (const auto& block : detail::extract_object_blocks(*response, "pending_items")) {
        const auto delivery_id = detail::extract_uint_field(block, "delivery_id");
        const auto ap_item_id = detail::extract_uint_field(block, "item_id").value_or(0);
        const auto item_name = detail::extract_string_field(block, "item_name").value_or("");
        const auto event_key = detail::extract_string_field(block, "event_key")
                                   .value_or(detail::extract_string_field(block, "tracker_semantic_id").value_or(""));
        const auto mapped_value = detail::extract_string_field(block, "mapped_value").value_or("");
        if (!delivery_id.has_value()) {
            continue;
        }
        RoomItem item;
        item.item_id = "delivery:" + std::to_string(*delivery_id);
        item.ap_item_id = ap_item_id;
        item.item_name = item_name;
        item.event_key = event_key;
        item.mapped_value = mapped_value;
        items.push_back(std::move(item));
    }
    return items;
}

bool GameServerRoomClient::acknowledge_item(std::string_view item_id, std::string* error) {
    if (!connected_ && !connect(error)) {
        return false;
    }
    std::string id_text(item_id);
    constexpr std::string_view kPrefix = "delivery:";
    if (id_text.rfind(kPrefix.data(), 0) == 0) {
        id_text.erase(0, kPrefix.size());
    }
    const auto delivery_id = detail::parse_u64(id_text);
    if (!delivery_id.has_value()) {
        if (error) *error = "invalid_delivery_id";
        return false;
    }
    const auto response = send_request(
        runtime_port_,
        "runtime",
        runtime_auth_token_,
        "{"
        "\"cmd\":\"acknowledge_delivery\","
        "\"session_name\":\"" + detail::escape_json(session_name_) + "\","
        "\"slot_id\":" + std::to_string(slot_id_) + ","
        "\"delivery_id\":" + std::to_string(*delivery_id) + ","
        "\"session_token\":\"" + detail::escape_json(runtime_session_token_) + "\""
        "}",
        error);
    if (!response.has_value()) {
        return false;
    }
    if (!detail::extract_bool_field(*response, "ok").value_or(false)) {
        if (error && error->empty()) {
            *error = detail::extract_string_field(*response, "error").value_or("acknowledge_failed");
        }
        return false;
    }
    return true;
}

std::vector<RoomChatMessage> GameServerRoomClient::poll_pending_chat(std::string*) {
    return {};
}

bool GameServerRoomClient::send_chat_message(std::string_view, std::string* error) {
    if (error) *error = "game_server_chat_not_implemented";
    return false;
}

bool GameServerRoomClient::issue_runtime_ticket(std::string* error) {
    const auto response = send_request(
        control_port_,
        control_channel_,
        control_auth_token_,
        "{"
        "\"cmd\":\"issue_ticket\","
        "\"session_name\":\"" + detail::escape_json(session_name_) + "\","
        "\"slot_id\":" + std::to_string(slot_id_) + ","
        "\"client_kind\":\"runtime\","
        "\"driver_instance_id\":\"" + detail::escape_json(driver_instance_id_) + "\","
        "\"linkedworld_id\":\"" + detail::escape_json(linkedworld_id_) + "\","
        "\"core_profile\":\"" + detail::escape_json(core_profile_) + "\""
        "}",
        error);
    if (!response.has_value()) {
        return false;
    }
    const auto token = detail::extract_string_field(*response, "session_token");
    if (!detail::extract_bool_field(*response, "ok").value_or(false) || !token.has_value()) {
        if (error && error->empty()) {
            *error = detail::extract_string_field(*response, "error").value_or("issue_ticket_failed");
        }
        return false;
    }
    runtime_session_token_ = *token;
    metadata_["room_id"] = detail::extract_string_field(*response, "room_id").value_or(session_name_);
    metadata_["slot_name"] = detail::extract_string_field(*response, "slot_name").value_or("");
    metadata_["player_alias"] = detail::extract_string_field(*response, "slot_alias").value_or("");
    if (const auto value = detail::extract_string_field(*response, "seed_id"); value.has_value()) {
        metadata_["seed_id"] = *value;
    }
    if (const auto value = detail::extract_string_field(*response, "seed_hash"); value.has_value()) {
        metadata_["seed_hash"] = *value;
    }
    if (const auto value = detail::extract_string_field(*response, "tracker_pack"); value.has_value()) {
        metadata_["tracker_pack"] = *value;
    }
    if (const auto value = detail::extract_string_field(*response, "tracker_variant"); value.has_value()) {
        metadata_["tracker_variant"] = *value;
    }
    if (const auto block = detail::extract_object_field_block(*response, "slot_data"); block.has_value()) {
        metadata_["slot_data"] = *block;
    } else if (const auto value = detail::extract_string_field(*response, "slot_data"); value.has_value()) {
        metadata_["slot_data"] = *value;
    }
    return true;
}

std::optional<std::string> GameServerRoomClient::canonical_id_from_event(const Event& event) const {
    if (event.canonical_id != 0) {
        return std::to_string(event.canonical_id);
    }
    return detail::parse_u64(event.key).has_value() ? std::optional<std::string>(event.key) : std::nullopt;
}

std::optional<std::string> GameServerRoomClient::send_request(std::uint16_t port,
                                                              const std::string& channel,
                                                              const std::string& auth_token,
                                                              const std::string& command_json,
                                                              std::string* error) const {
#if defined(_WIN32)
    if (error) *error = "game_server_room_client_not_supported_on_windows";
    return std::nullopt;
#else
    if (port == 0) {
        if (error) *error = "room_port_missing";
        return std::nullopt;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    const auto port_text = std::to_string(port);
    const int resolve_status = ::getaddrinfo(host_.c_str(), port_text.c_str(), &hints, &results);
    if (resolve_status != 0 || results == nullptr) {
        if (error) *error = "room_name_resolution_failed";
        return std::nullopt;
    }

    int fd = -1;
    for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
        fd = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (::connect(fd, current->ai_addr, static_cast<socklen_t>(current->ai_addrlen)) == 0) {
            break;
        }
        close_socket_fd(fd);
        fd = -1;
    }
    ::freeaddrinfo(results);

    if (fd < 0) {
        if (error) *error = "room_connect_failed";
        return std::nullopt;
    }

    std::string payload = "{\"channel\":\"" + detail::escape_json(channel) + "\",";
    if (!auth_token.empty()) {
        payload += "\"auth_token\":\"" + detail::escape_json(auth_token) + "\",";
    }
    payload += "\"command\":" + command_json + "}\n";

    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto written = ::send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (written <= 0) {
            close_socket_fd(fd);
            if (error) *error = "room_send_failed";
            return std::nullopt;
        }
        sent += static_cast<std::size_t>(written);
    }

    std::string response;
    char ch = '\0';
    while (true) {
        const auto received = ::recv(fd, &ch, 1, 0);
        if (received <= 0) {
            close_socket_fd(fd);
            if (error) *error = "room_recv_failed";
            return std::nullopt;
        }
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            response.push_back(ch);
        }
    }
    close_socket_fd(fd);
    return response;
#endif
}

}  // namespace sekailink::sklmi
