#include "api_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <regex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace sekailink::sklmi {
namespace {

std::string EscapeJson(const std::string& value) {
    return detail::escape_json(value);
}

std::string BuildTagsArray(const std::vector<std::string>& tags) {
    std::ostringstream out;
    out << "[";
    for (std::size_t index = 0; index < tags.size(); ++index) {
        if (index != 0) out << ",";
        out << "\"" << EscapeJson(tags[index]) << "\"";
    }
    out << "]";
    return out.str();
}

std::string BuildConnectPacket(const ArchipelagoConnectOptions& options) {
    std::ostringstream out;
    out << "[{"
        << "\"cmd\":\"Connect\","
        << "\"password\":\"" << EscapeJson(options.password) << "\","
        << "\"game\":\"" << EscapeJson(options.game) << "\","
        << "\"name\":\"" << EscapeJson(options.slot_name) << "\","
        << "\"uuid\":\"" << EscapeJson(options.uuid) << "\","
        << "\"version\":{"
        << "\"major\":" << options.version_major << ","
        << "\"minor\":" << options.version_minor << ","
        << "\"build\":" << options.version_build << ","
        << "\"class\":\"Version\""
        << "},"
        << "\"items_handling\":" << options.items_handling << ","
        << "\"tags\":" << BuildTagsArray(options.tags)
        << "}]";
    return out.str();
}

std::string BuildLocationChecksPacket(std::uint64_t location_id) {
    return "[{\"cmd\":\"LocationChecks\",\"locations\":[" + std::to_string(location_id) + "]}]";
}

std::string BuildSayPacket(std::string_view text) {
    return "[{\"cmd\":\"Say\",\"text\":\"" + EscapeJson(std::string(text)) + "\"}]";
}

std::string BuildDataPackageRequestPacket(const std::string& game) {
    std::ostringstream out;
    out << "[{\"cmd\":\"GetDataPackage\",\"games\":[\"";
    out << EscapeJson(game);
    out << "\",\"Archipelago\"]}]";
    return out.str();
}

std::string BuildStringArrayJson(std::vector<std::string> values, std::size_t limit) {
    values.erase(std::remove_if(values.begin(), values.end(), [](const auto& value) {
                     return value.empty();
                 }),
                 values.end());
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    if (values.size() > limit) {
        values.resize(limit);
    }
    std::ostringstream out;
    out << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out << ",";
        out << "\"" << EscapeJson(values[index]) << "\"";
    }
    out << "]";
    return out.str();
}

std::string BuildUintArrayJson(std::vector<std::uint64_t> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    std::ostringstream out;
    out << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out << ",";
        out << values[index];
    }
    out << "]";
    return out.str();
}

std::vector<std::uint64_t> ExtractUintArrayField(const std::string& text, const std::string& key) {
    std::vector<std::uint64_t> values;
    const auto key_pos = text.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return values;
    const auto open = text.find('[', key_pos);
    if (open == std::string::npos) return values;
    const auto close = text.find(']', open);
    if (close == std::string::npos || close <= open) return values;
    const auto body = text.substr(open + 1, close - open - 1);
    const std::regex number_pattern(R"((0x[0-9A-Fa-f]+|[0-9]+))");
    for (auto it = std::sregex_iterator(body.begin(), body.end(), number_pattern);
         it != std::sregex_iterator();
         ++it) {
        if (it->size() < 2) continue;
        if (auto parsed = detail::parse_u64((*it)[1].str()); parsed.has_value()) {
            values.push_back(*parsed);
        }
    }
    return values;
}

std::vector<std::string> ItemNamesFromIdMap(const std::unordered_map<std::uint64_t, std::string>& names) {
    std::vector<std::string> values;
    values.reserve(names.size());
    for (const auto& [id, name] : names) {
        (void)id;
        if (!name.empty()) {
            values.push_back(name);
        }
    }
    return values;
}

std::string FirstCommand(const std::string& packet) {
    return detail::extract_string_field(packet, "cmd").value_or("");
}

std::vector<std::string> TopLevelCommandBlocks(const std::string& packet) {
    std::vector<std::string> blocks;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    std::size_t block_start = std::string::npos;
    for (std::size_t index = 0; index < packet.size(); ++index) {
        const auto ch = packet[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '{') {
            if (depth == 0) block_start = index;
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && block_start != std::string::npos) {
                blocks.push_back(packet.substr(block_start, index - block_start + 1));
                block_start = std::string::npos;
            }
        }
    }
    if (blocks.empty() && packet.find("\"cmd\"") != std::string::npos) {
        blocks.push_back(packet);
    }
    return blocks;
}

std::unordered_map<std::uint64_t, std::string> ParseNameToIdObject(const std::string& block) {
    std::unordered_map<std::uint64_t, std::string> out;
    const std::regex pair_pattern("\"([^\"]+)\"\\s*:\\s*([0-9]+)");
    for (auto it = std::sregex_iterator(block.begin(), block.end(), pair_pattern);
         it != std::sregex_iterator();
         ++it) {
        if (it->size() < 3) {
            continue;
        }
        out[detail::parse_u64((*it)[2].str()).value_or(0)] = (*it)[1].str();
    }
    out.erase(0);
    return out;
}

std::string ResolveName(const std::unordered_map<std::uint64_t, std::string>& names,
                        std::uint64_t id,
                        std::string_view fallback) {
    if (id != 0) {
        if (const auto it = names.find(id); it != names.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return std::string(fallback);
}

std::uint64_t TokenIdFromFieldsOrText(const std::string& block,
                                      std::string_view primary_field,
                                      std::string_view secondary_field,
                                      std::string_view fallback_text) {
    if (const auto value = detail::extract_uint_field(block, std::string(primary_field)); value.has_value()) {
        return *value;
    }
    if (!secondary_field.empty()) {
        if (const auto value = detail::extract_uint_field(block, std::string(secondary_field)); value.has_value()) {
            return *value;
        }
    }
    return detail::parse_u64(std::string(fallback_text)).value_or(0);
}

RoomItem MakeRoomItemFromApBlock(const std::string& block,
                                 std::uint64_t fallback_index,
                                 const std::unordered_map<std::uint64_t, std::string>& item_names) {
    const auto ap_item_id = detail::extract_uint_field(block, "item").value_or(0);
    const auto location_id = detail::extract_uint_field(block, "location").value_or(0);
    const auto player_id = detail::extract_uint_field(block, "player").value_or(0);
    RoomItem item;
    item.item_id = "ap:" + std::to_string(fallback_index) + ":" + std::to_string(ap_item_id) + ":" + std::to_string(location_id) +
                   ":" + std::to_string(player_id);
    item.ap_item_id = ap_item_id;
    item.ap_location_id = location_id;
    item.ap_player_id = player_id;
    item.value_u64 = ap_item_id;
    item.event_key = std::to_string(ap_item_id);
    item.mapped_value = std::to_string(ap_item_id);
    if (const auto embedded_name = detail::extract_string_field(block, "item_name"); embedded_name.has_value()) {
        item.item_name = *embedded_name;
    } else if (const auto name = item_names.find(ap_item_id); name != item_names.end()) {
        item.item_name = name->second;
    }
    return item;
}

std::string TrimChatText(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.pop_back();
    }
    while (!text.empty() && (text.front() == ':' || text.front() == '-')) {
        text.erase(text.begin());
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
            text.erase(text.begin());
        }
    }
    return text;
}

std::string LocalDisplayName(const ArchipelagoConnectOptions& options) {
    auto alias = TrimChatText(options.player_alias);
    return alias.empty() ? options.slot_name : alias;
}

std::string StripArchipelagoClientDetails(std::string text) {
    static const std::regex client_with_tags(R"(\s*Client\([^)]+\),?\s*\[[^\]]*\]\.?\s*)");
    static const std::regex client_only(R"(\s*Client\([^)]+\)\.?\s*)");
    text = std::regex_replace(text, client_with_tags, " ");
    text = std::regex_replace(text, client_only, " ");
    return TrimChatText(std::move(text));
}

std::string SekaiLinkifyConnectionText(std::string text, std::string_view print_type) {
    text = StripArchipelagoClientDetails(std::move(text));
    const bool is_join = print_type == "Join";
    const bool is_part = print_type == "Part";
    if (!is_join && !is_part) {
        return text;
    }

    std::smatch match;
    if (is_join) {
        static const std::regex joined_with_game(
            R"(^(.+?)\s+\(Team\s+#?\d+\)\s+playing\s+.+?\s+has\s+joined\.?$)",
            std::regex_constants::icase);
        static const std::regex joined_plain(R"(^(.+?)\s+has\s+joined\.?$)", std::regex_constants::icase);
        if (std::regex_match(text, match, joined_with_game) || std::regex_match(text, match, joined_plain)) {
            auto name = TrimChatText(match[1].str());
            return name.empty() ? "A player connected." : name + " connected.";
        }
        return text.empty() ? "A player connected." : text;
    }

    static const std::regex left_with_team(
        R"(^(.+?)\s+\(Team\s+#?\d+\).*?(?:has\s+left|has\s+disconnected|left|disconnected).*$)",
        std::regex_constants::icase);
    static const std::regex left_plain(
        R"(^(.+?)\s+(?:has\s+left|has\s+disconnected|left|disconnected).*$)",
        std::regex_constants::icase);
    if (std::regex_match(text, match, left_with_team) || std::regex_match(text, match, left_plain)) {
        auto name = TrimChatText(match[1].str());
        return name.empty() ? "A player disconnected." : name + " disconnected.";
    }
    return text.empty() ? "A player disconnected." : text;
}

std::string PrintJsonTokenText(const std::string& block,
                               const std::unordered_map<std::uint64_t, std::string>& item_names,
                               const std::unordered_map<std::uint64_t, std::string>& location_names,
                               const std::unordered_map<std::uint64_t, std::string>& player_names,
                               std::uint64_t local_slot,
                               std::string_view local_slot_name) {
    const auto fallback = detail::extract_string_field(block, "text").value_or("");
    const auto token_type = detail::extract_string_field(block, "type").value_or("");
    if (token_type == "player_id") {
        const auto player_id = TokenIdFromFieldsOrText(block, "player", "slot", fallback);
        if (player_id != 0 && player_id == local_slot && !local_slot_name.empty()) {
            return std::string(local_slot_name);
        }
        return ResolveName(player_names, player_id, fallback);
    }
    if (token_type == "item_id") {
        return ResolveName(item_names, TokenIdFromFieldsOrText(block, "item", "id", fallback), fallback);
    }
    if (token_type == "location_id") {
        return ResolveName(location_names, TokenIdFromFieldsOrText(block, "location", "id", fallback), fallback);
    }
    return fallback;
}

std::string LastStringField(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::string value;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
         it != std::sregex_iterator();
         ++it) {
        if (it->size() > 1) {
            value = (*it)[1].str();
        }
    }
    return value;
}

std::string PrintJsonPlainText(const std::string& packet,
                               const std::unordered_map<std::uint64_t, std::string>& item_names,
                               const std::unordered_map<std::uint64_t, std::string>& location_names,
                               const std::unordered_map<std::uint64_t, std::string>& player_names,
                               std::uint64_t local_slot,
                               std::string_view local_slot_name) {
    std::string text;
    for (const auto& block : detail::extract_object_blocks(packet, "data")) {
        text += PrintJsonTokenText(block, item_names, location_names, player_names, local_slot, local_slot_name);
    }
    return TrimChatText(std::move(text));
}

std::optional<RoomChatMessage> MakeLogMessageFromPrintJson(
    const std::string& packet,
    std::uint64_t id,
    const std::unordered_map<std::uint64_t, std::string>& item_names,
    const std::unordered_map<std::uint64_t, std::string>& location_names,
    const std::unordered_map<std::uint64_t, std::string>& player_names,
    std::uint64_t local_slot,
    std::string_view local_slot_name) {
    const auto print_type = LastStringField(packet, "type");

    std::string author;
    if (const auto chat_text = detail::extract_string_field(packet, "message");
        chat_text.has_value() && (print_type == "Chat" || detail::extract_uint_field(packet, "slot").has_value())) {
        const auto player_id = detail::extract_uint_field(packet, "slot")
                                   .value_or(detail::extract_uint_field(packet, "player").value_or(0));
        if (player_id != 0 && player_id == local_slot && !local_slot_name.empty()) {
            author = std::string(local_slot_name);
        } else {
            author = ResolveName(player_names, player_id, "ROOM");
        }
        auto text = TrimChatText(*chat_text);
        if (text.empty()) {
            return std::nullopt;
        }
        RoomChatMessage message;
        message.id = id;
        message.author = author.empty() ? "ROOM" : std::move(author);
        message.text = std::move(text);
        message.kind = "chat";
        return message;
    }

    if (print_type == "Chat") {
        std::string text;
        for (const auto& block : detail::extract_object_blocks(packet, "data")) {
            const auto token = PrintJsonTokenText(block, item_names, location_names, player_names, local_slot, local_slot_name);
            if (token.empty()) {
                continue;
            }
            const auto token_type = detail::extract_string_field(block, "type").value_or("");
            if (author.empty() && token_type == "player_id") {
                author = token;
                continue;
            }
            text += token;
        }
        text = TrimChatText(std::move(text));
        if (text.empty()) {
            return std::nullopt;
        }
        RoomChatMessage message;
        message.id = id;
        message.author = author.empty() ? "ROOM" : author;
        message.text = std::move(text);
        message.kind = "chat";
        return message;
    }

    if (print_type == "Join" || print_type == "Part" || print_type == "CommandResult" ||
        print_type == "Hint" || print_type == "ServerChat") {
        auto text = PrintJsonPlainText(packet, item_names, location_names, player_names, local_slot, local_slot_name);
        text = SekaiLinkifyConnectionText(std::move(text), print_type);
        if (text.empty()) {
            if (print_type == "Join") {
                text = "A player connected.";
            } else if (print_type == "Part") {
                text = "A player disconnected.";
            }
        }
        if (text.empty()) {
            return std::nullopt;
        }
        RoomChatMessage message;
        message.id = id;
        message.author = "SYSTEM";
        message.text = std::move(text);
        if (print_type == "Join" || print_type == "Part") {
            message.kind = "connection";
        } else if (print_type == "Hint") {
            message.kind = "hint";
        } else {
            message.kind = "system";
        }
        return message;
    }

    if (print_type.find("Item") == std::string::npos) {
        return std::nullopt;
    }

    auto text = PrintJsonPlainText(packet, item_names, location_names, player_names, local_slot, local_slot_name);
    text = StripArchipelagoClientDetails(std::move(text));
    if (text.empty()) {
        return std::nullopt;
    }
    RoomChatMessage message;
    message.id = id;
    message.author = "ITEM";
    message.text = std::move(text);
    message.kind = "item";
    return message;
}

std::optional<RoomChatMessage> MakeDefeatMessageFromBounce(const std::string& packet,
                                                           std::uint64_t id,
                                                           std::string_view local_slot_name) {
    if (packet.find("DeathLink") == std::string::npos) {
        return std::nullopt;
    }
    auto source = detail::extract_string_field(packet, "source").value_or(std::string(local_slot_name));
    if (source.empty()) {
        source = "A player";
    }
    auto cause = detail::extract_string_field(packet, "cause").value_or("");
    RoomChatMessage message;
    message.id = id;
    message.author = "SYSTEM";
    message.text = source + " was defeated.";
    if (!cause.empty()) {
        message.text += " " + cause;
    }
    message.kind = "defeat";
    return message;
}

}  // namespace

ArchipelagoRoomClient::ArchipelagoRoomClient(std::unique_ptr<ArchipelagoTransport> transport, ArchipelagoConnectOptions options)
    : transport_(std::move(transport)), options_(std::move(options)) {}

bool ArchipelagoRoomClient::connect(std::string* error) {
    if (connected_) return true;
    if (transport_ == nullptr) {
        if (error) *error = "archipelago_transport_missing";
        return false;
    }

    for (int attempt = 0; attempt < 256 && !connected_; ++attempt) {
        auto packet = transport_->receive_text(error);
        if (!packet.has_value()) {
            if (error != nullptr && !error->empty()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (!process_packet(*packet, error)) {
            return false;
        }
    }

    if (!connected_) {
        if (error && error->empty()) *error = "archipelago_connect_incomplete";
        return false;
    }
    return true;
}

void ArchipelagoRoomClient::disconnect() {
    connected_ = false;
    connect_sent_ = false;
    data_package_request_sent_ = false;
    if (transport_ != nullptr) {
        transport_->disconnect();
    }
}

bool ArchipelagoRoomClient::connected() const {
    return connected_;
}

std::unordered_map<std::string, std::string> ArchipelagoRoomClient::metadata_snapshot() const {
    auto metadata = metadata_;
    metadata["room_mode"] = "archipelago";
    metadata["connected"] = connected_ ? "1" : "0";
    metadata["game"] = options_.game;
    metadata["slot_name"] = options_.slot_name;
    if (!options_.player_alias.empty()) {
        metadata["player_alias"] = options_.player_alias;
        metadata["player_display_name"] = options_.player_alias;
    }
    metadata["team"] = std::to_string(team_);
    metadata["slot"] = std::to_string(slot_);
    metadata["received_index"] = std::to_string(received_index_);
    if (!item_id_to_name_.empty()) {
        metadata["hint_item_names"] = BuildStringArrayJson(ItemNamesFromIdMap(item_id_to_name_), 512);
    }
    return metadata;
}

bool ArchipelagoRoomClient::report_location_checked(const Event& event, std::string* error) {
    if (!connected_ && !connect(error)) {
        return false;
    }
    const auto location_id = event.canonical_id != 0 ? event.canonical_id : detail::parse_u64(event.key).value_or(0);
    if (location_id == 0) {
        if (error) *error = "archipelago_location_id_missing";
        return false;
    }
    return transport_ != nullptr && transport_->send_text(BuildLocationChecksPacket(location_id), error);
}

std::vector<RoomItem> ArchipelagoRoomClient::poll_pending_items(std::string* error) {
    if (!connected_ && !connect(error)) {
        return {};
    }
    if (transport_ == nullptr) {
        if (error) *error = "archipelago_transport_missing";
        return {};
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        auto packet = transport_->receive_text(error);
        if (!packet.has_value()) break;
        if (!process_packet(*packet, error)) break;
    }
    std::vector<RoomItem> out;
    for (const auto& item : pending_items_) {
        if (!acknowledged_items_.contains(item.item_id)) {
            out.push_back(item);
        }
    }
    return out;
}

bool ArchipelagoRoomClient::acknowledge_item(std::string_view item_id, std::string*) {
    acknowledged_items_[std::string(item_id)] = true;
    return true;
}

std::vector<RoomChatMessage> ArchipelagoRoomClient::poll_pending_chat(std::string* error) {
    if (!connected_ && !connect(error)) {
        return {};
    }
    if (transport_ == nullptr) {
        if (error) *error = "archipelago_transport_missing";
        return {};
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        auto packet = transport_->receive_text(error);
        if (!packet.has_value()) break;
        if (!process_packet(*packet, error)) break;
    }
    std::vector<RoomChatMessage> out;
    out.swap(pending_chat_);
    return out;
}

bool ArchipelagoRoomClient::send_chat_message(std::string_view text, std::string* error) {
    if (text.empty()) {
        return true;
    }
    if (!connected_ && !connect(error)) {
        return false;
    }
    return transport_ != nullptr && transport_->send_text(BuildSayPacket(text), error);
}

bool ArchipelagoRoomClient::process_packet(std::string_view packet_view, std::string* error) {
    const std::string packet(packet_view);
    const auto command_blocks = TopLevelCommandBlocks(packet);
    if (command_blocks.size() > 1) {
        for (const auto& block : command_blocks) {
            if (!process_packet(block, error)) {
                return false;
            }
        }
        return true;
    }
    const auto& command_packet = command_blocks.empty() ? packet : command_blocks.front();
    const auto cmd = FirstCommand(command_packet);
    if (cmd == "RoomInfo") {
        if (const auto seed = detail::extract_string_field(command_packet, "seed_name"); seed.has_value()) {
            metadata_["seed_name"] = *seed;
        }
        if (const auto hint_cost = detail::extract_uint_field(command_packet, "hint_cost"); hint_cost.has_value()) {
            metadata_["hint_cost"] = std::to_string(*hint_cost);
        }
        if (const auto check_points = detail::extract_uint_field(command_packet, "location_check_points"); check_points.has_value()) {
            metadata_["location_check_points"] = std::to_string(*check_points);
        }
        if (!data_package_request_sent_ && !send_data_package_request(error)) {
            return false;
        }
        if (!connect_sent_ && !send_connect_packet(error)) {
            return false;
        }
        return true;
    }
    if (cmd == "Connected") {
        connected_ = true;
        team_ = detail::extract_uint_field(command_packet, "team").value_or(team_);
        slot_ = detail::extract_uint_field(command_packet, "slot").value_or(slot_);
        metadata_["checked_locations"] = BuildUintArrayJson(ExtractUintArrayField(command_packet, "checked_locations"));
        const auto player_blocks = detail::extract_object_blocks(command_packet, "players");
        if (!player_blocks.empty()) {
            metadata_["player_count"] = std::to_string(player_blocks.size());
            for (const auto& block : player_blocks) {
                const auto player_id = detail::extract_uint_field(block, "slot")
                                           .value_or(detail::extract_uint_field(block, "player")
                                                         .value_or(detail::extract_uint_field(block, "id").value_or(0)));
                auto name = detail::extract_string_field(block, "name")
                                .value_or(detail::extract_string_field(block, "alias")
                                              .value_or(detail::extract_string_field(block, "slot_name").value_or("")));
                if (player_id != 0 && !name.empty()) {
                    player_id_to_name_[player_id] = std::move(name);
                }
            }
        }
        const auto local_display_name = LocalDisplayName(options_);
        if (slot_ != 0 && !local_display_name.empty()) {
            player_id_to_name_[slot_] = local_display_name;
        }
        if (const auto block = detail::extract_object_field_block(command_packet, "slot_data"); block.has_value()) {
            metadata_["slot_data"] = *block;
        }
        return true;
    }
    if (cmd == "ReceivedItems") {
        const auto packet_index = detail::extract_uint_field(command_packet, "index").value_or(received_index_);
        std::uint64_t local_index = packet_index;
        for (const auto& block : detail::extract_object_blocks(command_packet, "items")) {
            pending_items_.push_back(MakeRoomItemFromApBlock(block, local_index, item_id_to_name_));
            ++local_index;
        }
        received_index_ = local_index;
        return true;
    }
    if (cmd == "DataPackage") {
        if (const auto game_block = detail::extract_object_field_block(command_packet, options_.game); game_block.has_value()) {
            if (const auto item_block = detail::extract_object_field_block(*game_block, "item_name_to_id"); item_block.has_value()) {
                item_id_to_name_ = ParseNameToIdObject(*item_block);
                metadata_["item_name_count"] = std::to_string(item_id_to_name_.size());
            }
            if (const auto location_block = detail::extract_object_field_block(*game_block, "location_name_to_id"); location_block.has_value()) {
                location_id_to_name_ = ParseNameToIdObject(*location_block);
                metadata_["location_name_count"] = std::to_string(location_id_to_name_.size());
            }
        }
        return true;
    }
    if (cmd == "PrintJSON") {
        if (auto message = MakeLogMessageFromPrintJson(command_packet,
                                                       ++chat_message_counter_,
                                                       item_id_to_name_,
                                                       location_id_to_name_,
                                                       player_id_to_name_,
                                                       slot_,
                                                       LocalDisplayName(options_));
            message.has_value()) {
            pending_chat_.push_back(std::move(*message));
        }
        return true;
    }
    if (cmd == "Bounced") {
        if (auto message = MakeDefeatMessageFromBounce(command_packet,
                                                       ++chat_message_counter_,
                                                       LocalDisplayName(options_));
            message.has_value()) {
            pending_chat_.push_back(std::move(*message));
        }
        return true;
    }
    if (cmd == "RoomUpdate") {
        if (command_packet.find("\"checked_locations\"") != std::string::npos) {
            metadata_["checked_locations"] = BuildUintArrayJson(ExtractUintArrayField(command_packet, "checked_locations"));
        }
        return true;
    }
    if (cmd == "ConnectionRefused") {
        if (error) *error = "archipelago_connection_refused";
        return false;
    }
    return true;
}

bool ArchipelagoRoomClient::send_connect_packet(std::string* error) {
    if (transport_ == nullptr) {
        if (error) *error = "archipelago_transport_missing";
        return false;
    }
    if (!transport_->send_text(BuildConnectPacket(options_), error)) {
        return false;
    }
    connect_sent_ = true;
    return true;
}

bool ArchipelagoRoomClient::send_data_package_request(std::string* error) {
    if (transport_ == nullptr) {
        if (error) *error = "archipelago_transport_missing";
        return false;
    }
    if (!transport_->send_text(BuildDataPackageRequestPacket(options_.game), error)) {
        return false;
    }
    data_package_request_sent_ = true;
    return true;
}

}  // namespace sekailink::sklmi
