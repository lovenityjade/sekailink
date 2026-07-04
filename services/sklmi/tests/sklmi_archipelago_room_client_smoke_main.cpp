#include "sekailink_sklmi/api.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace sekailink::sklmi;

namespace {

class ScriptedTransport final : public ArchipelagoTransport {
  public:
    explicit ScriptedTransport(std::vector<std::string> inbound) : inbound_(std::move(inbound)) {}

    bool send_text(std::string_view payload, std::string*) override {
        sent_.emplace_back(payload);
        return true;
    }

    std::optional<std::string> receive_text(std::string*) override {
        if (next_ >= inbound_.size()) {
            return std::nullopt;
        }
        return inbound_[next_++];
    }

    const std::vector<std::string>& sent() const {
        return sent_;
    }

  private:
    std::vector<std::string> inbound_;
    std::vector<std::string> sent_;
    std::size_t next_ = 0;
};

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    auto transport = std::make_unique<ScriptedTransport>(std::vector<std::string>{
        "[{\"cmd\":\"RoomInfo\",\"seed_name\":\"BETA3-ALTTP-AP\",\"games\":[\"A Link to the Past\",\"Super Mario World\"],\"datapackage_checksums\":{\"A Link to the Past\":\"abc\",\"Super Mario World\":\"def\"}}]",
        "[{\"cmd\":\"DataPackage\",\"data\":{\"games\":{\"A Link to the Past\":{\"item_name_to_id\":{\"Hookshot\":10,\"Rupees (20)\":54},\"location_name_to_id\":{\"Sanctuary\":60025,\"Link's House\":59836}},\"Super Mario World\":{\"item_name_to_id\":{\"Swim\":1234},\"location_name_to_id\":{}}}}}]",
        "[{\"cmd\":\"Connected\",\"team\":0,\"slot\":1,\"checked_locations\":[59836],\"slot_data\":{\"mode\":\"open\",\"goal\":\"ganon\"},\"slot_info\":{\"1\":{\"name\":\"Jade\",\"game\":\"A Link to the Past\",\"type\":\"player\"},\"2\":{\"name\":\"Mario\",\"game\":\"Super Mario World\",\"type\":\"player\"}}},"
        "{\"cmd\":\"ReceivedItems\",\"index\":0,\"items\":[{\"item\":10,\"location\":60025,\"player\":1,\"flags\":0}]}]",
        "[{\"cmd\":\"PrintJSON\",\"data\":[{\"text\":\"Jade\",\"type\":\"player_id\",\"player\":1},{\"text\":\": hello sync\"}],\"type\":\"Chat\"}]",
        "[{\"cmd\":\"PrintJSON\",\"data\":[{\"text\":\"Jade\",\"type\":\"player_id\",\"player\":1},"
        "{\"text\":\" sent \"},{\"text\":\"Hookshot\",\"type\":\"item_id\",\"item\":10},"
        "{\"text\":\" to \"},{\"text\":\"Jade\",\"type\":\"player_id\",\"player\":1},"
        "{\"text\":\" at \"},{\"text\":\"Sanctuary\",\"type\":\"location_id\",\"location\":60025}],\"type\":\"ItemSend\"}]",
        "[{\"cmd\":\"PrintJSON\",\"data\":[{\"text\":\"1\",\"type\":\"player_id\"},"
        "{\"text\":\" found their \"},{\"text\":\"54\",\"type\":\"item_id\"},"
        "{\"text\":\" (\"},{\"text\":\"59836\",\"type\":\"location_id\"},{\"text\":\")\"}],\"type\":\"ItemCheat\"}]",
        "[{\"cmd\":\"PrintJSON\",\"data\":[{\"text\":\"Jade\",\"type\":\"player_id\",\"player\":1},"
        "{\"text\":\" sent \"},{\"text\":\"Swim\",\"type\":\"item_id\",\"item\":1234},"
        "{\"text\":\" to \"},{\"text\":\"Mario\",\"type\":\"player_id\",\"player\":2},"
        "{\"text\":\" at \"},{\"text\":\"Sanctuary\",\"type\":\"location_id\",\"location\":60025}],\"type\":\"ItemSend\"}]",
        "[{\"cmd\":\"PrintJSON\",\"data\":[{\"text\":\"thelov-supe-3557\",\"type\":\"player_id\",\"player\":2},"
        "{\"text\":\" (Team #1) Tracking a Link to the Past connected.\"}],\"type\":\"Join\"}]",
        "[{\"cmd\":\"PrintJSON\",\"data\":[{\"text\":\"Jade\"},{\"text\":\": raw echo\"}],\"type\":\"Chat\",\"team\":0,\"slot\":1,\"message\":\"raw echo\"}]",
    });
    auto* raw_transport = transport.get();

    ArchipelagoConnectOptions options;
    options.game = "A Link to the Past";
    options.slot_name = "Jade";
    options.password = "secret";
    options.uuid = "sekailink-test-uuid";
    options.tags = {"AP", "SekaiLink", "SKLMI"};
    options.player_aliases_by_slot_name = {{"thelov-supe-3557", "Certo"}, {"Mario", "Certo"}};

    ArchipelagoRoomClient client(std::move(transport), options);
    std::string error;
    if (!client.connect(&error)) {
        std::cerr << "connect_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    const auto metadata = client.metadata_snapshot();
    if (metadata.at("room_mode") != "archipelago" ||
        metadata.at("connected") != "1" ||
        metadata.at("game") != "A Link to the Past" ||
        metadata.at("slot_name") != "Jade" ||
        metadata.at("seed_name") != "BETA3-ALTTP-AP" ||
        metadata.at("team") != "0" ||
        metadata.at("slot") != "1" ||
        metadata.at("checked_locations") != "[59836]" ||
        !contains(metadata.at("slot_data"), "\"goal\":\"ganon\"")) {
        std::cerr << "metadata_mismatch\n";
        return EXIT_FAILURE;
    }

    if (raw_transport->sent().size() != 2 ||
        !contains(raw_transport->sent()[0], "\"cmd\":\"GetDataPackage\"") ||
        !contains(raw_transport->sent()[0], "\"A Link to the Past\"") ||
        !contains(raw_transport->sent()[1], "\"cmd\":\"Connect\"") ||
        !contains(raw_transport->sent()[1], "\"game\":\"A Link to the Past\"") ||
        !contains(raw_transport->sent()[1], "\"name\":\"Jade\"") ||
        !contains(raw_transport->sent()[1], "\"items_handling\":7") ||
        !contains(raw_transport->sent()[1], "\"SekaiLink\"")) {
        std::cerr << "connect_packet_mismatch\n";
        return EXIT_FAILURE;
    }

    Event check;
    check.type = EventType::location_checked;
    check.key = "Sanctuary";
    check.value = "Sanctuary";
    check.canonical_id = 60025;
    if (!client.report_location_checked(check, &error)) {
        std::cerr << "location_check_failed:" << error << "\n";
        return EXIT_FAILURE;
    }

    if (raw_transport->sent().size() != 3 ||
        !contains(raw_transport->sent()[2], "\"cmd\":\"LocationChecks\"") ||
        !contains(raw_transport->sent()[2], "\"locations\":[60025]")) {
        std::cerr << "location_checks_packet_mismatch\n";
        return EXIT_FAILURE;
    }

    const auto items = client.poll_pending_items(&error);
    if (items.size() != 1 ||
        items[0].item_id != "ap:0:10:60025:1" ||
        items[0].ap_item_id != 10 ||
        items[0].ap_location_id != 60025 ||
        items[0].ap_player_id != 1 ||
        items[0].value_u64 != 10 ||
        items[0].event_key != "10" ||
        items[0].mapped_value != "10" ||
        items[0].item_name != "Hookshot") {
        std::cerr << "received_items_mismatch\n";
        return EXIT_FAILURE;
    }

    if (!client.acknowledge_item(items[0].item_id, &error)) {
        std::cerr << "ack_failed:" << error << "\n";
        return EXIT_FAILURE;
    }
    if (!client.poll_pending_items(&error).empty()) {
        std::cerr << "acknowledged_item_reappeared\n";
        return EXIT_FAILURE;
    }

    const auto chat = client.poll_pending_chat(&error);
    if (chat.size() != 6 ||
        chat[0].author != "Jade" ||
        chat[0].text != "hello sync" ||
        chat[0].id == 0 ||
        chat[1].author != "ITEM" ||
        chat[1].text != "Jade found their Hookshot in Sanctuary" ||
        chat[1].id <= chat[0].id ||
        chat[2].author != "ITEM" ||
        chat[2].text != "Jade found their Rupees (20) in Link's House" ||
        chat[2].id <= chat[1].id ||
        chat[3].author != "ITEM" ||
        chat[3].text != "Jade sent Swim to Certo's Super Mario World (Sanctuary)" ||
        chat[3].id <= chat[2].id) {
        std::cerr << "chat_message_mismatch\n";
        return EXIT_FAILURE;
    }
    if (chat[4].kind != "connection" ||
        chat[4].text != "Certo connected." ||
        chat[4].id <= chat[3].id) {
        std::cerr << "chat_message_mismatch\n";
        return EXIT_FAILURE;
    }
    if (chat[5].author != "Jade" ||
        chat[5].text != "raw echo" ||
        chat[5].id <= chat[4].id) {
        std::cerr << "chat_message_mismatch\n";
        return EXIT_FAILURE;
    }

    if (!client.send_chat_message("reply from sekaiemu", &error) ||
        raw_transport->sent().size() != 4 ||
        !contains(raw_transport->sent()[3], "\"cmd\":\"Say\"") ||
        !contains(raw_transport->sent()[3], "\"text\":\"reply from sekaiemu\"")) {
        std::cerr << "chat_send_mismatch:" << error << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "sklmi_archipelago_room_client_smoke_ok\n";
    return EXIT_SUCCESS;
}
