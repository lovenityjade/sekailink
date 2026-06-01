#include "sekailink_server/generation_server.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void write_json_file(const std::filesystem::path& path, const nlohmann::json& value) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("write_json_file_failed");
  }
  stream << value.dump(2) << "\n";
}

nlohmann::json read_json_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("read_json_file_failed:" + path.string());
  }
  return nlohmann::json::parse(stream);
}

std::filesystem::path make_complete_linkedworld_fixture(const std::filesystem::path& root,
                                                        bool include_item_pool = true,
                                                        const std::string& game_key = "smoke",
                                                        const std::string& linkedworld_id = "linkedworld.smoke") {
  const auto linkedworld_root = root / "linkedworld";
  write_json_file(linkedworld_root / "manifest" / "manifest.json", {
      {"module_id", "sekailink." + game_key},
      {"game_id", game_key},
      {"version", "1"},
      {"module_blocks", {{"generation_ir", {{"path", "generation/generation-ir.json"}}}}},
  });
  write_json_file(linkedworld_root / "tracker" / "locations.json", nlohmann::json::array({
      {{"id", "loc.one"}, {"name", "Location One"}},
  }));
  write_json_file(linkedworld_root / "tracker" / "items.json", nlohmann::json::array({
      {{"id", "item.one"}, {"name", "Item One"}, {"classification", "progression"}},
  }));
  if (include_item_pool) {
    write_json_file(linkedworld_root / "generation" / "item-pool.json", {
        {"items", nlohmann::json::array({
            {{"id", "item.one"}, {"name", "Item One"}, {"classification", "progression"}, {"advancement", true}},
        })},
    });
  }
  write_json_file(linkedworld_root / "generation" / "logic.json", {
      {"schema_version", "sekailink-logic-rules-v1"},
      {"rule_language", "smoke-rule-graph"},
      {"regions", nlohmann::json::array()},
      {"rules", nlohmann::json::object()},
  });
  write_json_file(linkedworld_root / "generation" / "placement.json", {
      {"schema_version", "sekailink-placement-rules-v1"},
      {"fillable_locations_ref", "tracker/locations.json"},
      {"preplacements", nlohmann::json::array()},
      {"reserved_locations", nlohmann::json::array()},
      {"item_constraints", nlohmann::json::array()},
      {"location_constraints", nlohmann::json::array()},
  });
  auto catalog = nlohmann::json{
      {"locations_ref", "tracker/locations.json"},
      {"items_ref", "tracker/items.json"},
      {"logic_rules_ref", "generation/logic.json"},
      {"logic_rules_shape", "region_rule_graph"},
      {"placement_rules_ref", "generation/placement.json"},
      {"placement_rules_shape", "placement_constraints"},
  };
  if (include_item_pool) {
    catalog["item_pool_ref"] = "generation/item-pool.json";
  }
  write_json_file(linkedworld_root / "generation" / "generation-ir.json", {
      {"linkedworld_id", linkedworld_id},
      {"game_key", game_key},
      {"version", "1"},
      {"capabilities", {
          {"can_validate_options", true},
          {"can_build_item_pool", true},
          {"can_solve_logic", true},
          {"can_place_items", true},
          {"can_emit_patch", true},
          {"can_emit_room_contract", true},
          {"external_tools_required", nlohmann::json::array()},
          {"unsupported_options", nlohmann::json::array()},
      }},
      {"catalog", catalog},
      {"patch", {{"mode", "contract_only"}}},
      {"runtime", {{"host", "sekaiemu"}}},
  });
  return linkedworld_root;
}

}  // namespace

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_generation_server_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_generation_server_smoke";
    fs::remove_all(root);
    fs::create_directories(root / "output");

    const fs::path yaml_path = root / "input.yaml";
    const fs::path artifact = root / "output" / "job-1.done";
    const auto linkedworld_root = make_complete_linkedworld_fixture(root);
    const auto missing_pool_linkedworld_root = make_complete_linkedworld_fixture(
        root / "missing-pool", false, "smoke-missing-pool", "linkedworld.smoke-missing-pool");
    {
      std::ofstream yaml_stream(yaml_path);
      yaml_stream << "game: alttp\n";
    }

    sekailink_server::GenerationTcpServer server(sekailink_server::GenerationServerConfig{
        .tcp_port = 0,
        .auth_token = std::string("generation-secret"),
        .service_config = sekailink_server::GenerationServiceConfig{
            .command_template = {
                "/usr/bin/touch",
                "{expected_artifact}",
            },
        },
    });
    if (!server.start()) {
      throw std::runtime_error("generation_server_start_failed");
    }

    const auto submit = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "generation-secret"},
            {"cmd", "submit_job"},
            {"job_id", "job-1"},
            {"yaml_path", yaml_path.string()},
            {"output_dir", (root / "output").string()},
            {"expected_artifact", artifact.string()},
        }));
    if (submit.at("ok") != true) {
      throw std::runtime_error("generation_submit_failed");
    }

    bool completed = false;
    for (int i = 0; i < 50; ++i) {
      const auto status = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
          "127.0.0.1",
          server.port(),
          {
              {"auth_token", "generation-secret"},
              {"cmd", "job_status"},
              {"job_id", "job-1"},
          }));
      if (status.at("ok") == true && status.at("job").at("status") == "succeeded") {
        completed = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    const auto unauthorized = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "wrong-secret"},
            {"cmd", "list_jobs"},
        }));

    const auto filtered = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "generation-secret"},
            {"cmd", "list_jobs"},
            {"limit", 1},
            {"query", "job"},
            {"status", "succeeded"},
        }));
    if (filtered.at("job_ids").size() != 1 || filtered.at("job_ids").at(0) != "job-1") {
      throw std::runtime_error("generation_filtered_list_failed");
    }

    const auto package_response = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "generation-secret"},
            {"cmd", "submit_seed_package"},
            {"job_id", "package-job-1"},
            {"room_id", "room-smoke"},
            {"seed_id", "seed-smoke"},
            {"rng_seed", "deterministic-seed-smoke"},
            {"output_root", (root / "packages").string()},
            {"linkedworld_roots", nlohmann::json::array({linkedworld_root.string()})},
            {"slots", nlohmann::json::array({
                {
                    {"slot_id", 1},
                    {"user_id", 1001},
                    {"display_name", "Jade"},
                    {"game_key", "smoke"},
                    {"linkedworld_id", "linkedworld.smoke"},
                    {"config_version_id", 501},
                },
            })},
        }));
    if (package_response.at("ok") != true ||
        package_response.at("manifest").at("schema_version") != "sekailink-seed-package-v1" ||
        package_response.at("manifest").at("artifact_hashes").at("patch_contracts/slot-1.patch.contract.json").get<std::string>().empty()) {
      throw std::runtime_error("generation_seed_package_command_failed:" + package_response.dump());
    }
    if (!fs::exists(fs::path(package_response.at("package_dir").get<std::string>()) / "patch_contracts" / "slot-1.patch.contract.json")) {
      throw std::runtime_error("generation_seed_package_patch_contract_missing");
    }
    const auto package_dir = fs::path(package_response.at("package_dir").get<std::string>());
    const auto audit = read_json_file(package_dir / "audit.json");
    const auto placements = read_json_file(package_dir / "placements.json");
    if (audit.at("replay_validation") != "full-logic-replay-passed" || placements.size() != 1 ||
        !placements.at(0).contains("requires") || !placements.at(0).contains("grants")) {
      throw std::runtime_error("generation_seed_package_replay_audit_mismatch");
    }
    const auto blocked_package_response = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "generation-secret"},
            {"cmd", "submit_seed_package"},
            {"job_id", "package-job-missing-pool"},
            {"room_id", "room-smoke"},
            {"seed_id", "seed-missing-pool"},
            {"rng_seed", "deterministic-seed-smoke"},
            {"output_root", (root / "packages").string()},
            {"linkedworld_roots", nlohmann::json::array({missing_pool_linkedworld_root.string()})},
            {"slots", nlohmann::json::array({
                {
                    {"slot_id", 1},
                    {"user_id", 1001},
                    {"display_name", "Jade"},
                    {"game_key", "smoke-missing-pool"},
                    {"linkedworld_id", "linkedworld.smoke-missing-pool"},
                    {"config_version_id", 501},
                },
            })},
        }));
    if (blocked_package_response.at("ok") != false ||
        blocked_package_response.at("error") != "missing_generation_item_pool:linkedworld.smoke-missing-pool") {
      throw std::runtime_error("generation_server_missing_item_pool_error_mismatch:" +
                               blocked_package_response.dump());
    }
    server.stop();

    if (!completed) {
      throw std::runtime_error("generation_job_not_completed");
    }
    if (!fs::exists(artifact)) {
      throw std::runtime_error("generation_artifact_missing");
    }
    if (unauthorized.at("error") != "unauthorized") {
      throw std::runtime_error("generation_unauthorized_failed");
    }

    std::cout << "generation_server_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_generation_server_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
