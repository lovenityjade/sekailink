#include "sekailink_server/room_server_protocol.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    sekailink_server::RoomRegistry registry;
    sekailink_server::RoomServerAuthPolicy auth_policy{
        .admin_token = std::string("admin-secret"),
        .runtime_token = std::string("runtime-secret"),
        .client_report_token = std::string("report-secret"),
    };

    const auto unauthorized = sekailink_server::handle_protocol_json(
        registry,
        nullptr,
        nullptr,
        &auth_policy,
        {
            {"channel", "admin"},
            {"command", {{"cmd", "list_rooms"}}},
        });
    const auto authorized = sekailink_server::handle_protocol_json(
        registry,
        nullptr,
        nullptr,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command", {{"cmd", "list_rooms"}}},
        });
    const auto wrong_runtime = sekailink_server::handle_protocol_json(
        registry,
        nullptr,
        nullptr,
        &auth_policy,
        {
            {"channel", "runtime"},
            {"auth_token", "admin-secret"},
            {"command", {{"cmd", "room_summary"}, {"room_id", "missing"}}},
        });

    require(unauthorized.at("ok").get<bool>() == false, "unauthorized_expected");
    require(unauthorized.at("error").get<std::string>() == "unauthorized", "unauthorized_error");
    require(authorized.at("ok").get<bool>(), "authorized_expected");
    require(wrong_runtime.at("error").get<std::string>() == "unauthorized", "wrong_runtime_error");

    std::cout << "unauthorized=" << unauthorized.at("error").get<std::string>() << "\n";
    std::cout << "authorized=" << authorized.at("ok").get<bool>() << "\n";
    std::cout << "runtime_wrong_token=" << wrong_runtime.at("error").get<std::string>() << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_protocol_auth_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
