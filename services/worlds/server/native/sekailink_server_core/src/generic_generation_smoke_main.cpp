#include "sekailink_server/generic_generation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {

void write_json(const std::filesystem::path& path, const nlohmann::json& value) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("write_failed:" + path.string());
  }
  stream << value.dump(2) << "\n";
}

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

nlohmann::json read_json(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("read_failed:" + path.string());
  }
  return nlohmann::json::parse(stream);
}

std::size_t count_with_slot(const nlohmann::json& values, int slot_id) {
  return static_cast<std::size_t>(std::count_if(values.begin(), values.end(), [&](const auto& value) {
    return value.value("slot_id", 0) == slot_id;
  }));
}

bool has_config_version(const nlohmann::json& config_versions,
                        int slot_id,
                        std::int64_t config_version_id,
                        const std::string& linkedworld_id) {
  return std::any_of(config_versions.begin(), config_versions.end(), [&](const auto& config) {
    return config.value("slot_id", 0) == slot_id &&
           config.value("config_version_id", std::int64_t{0}) == config_version_id &&
           config.value("linkedworld_id", std::string{}) == linkedworld_id;
  });
}

bool contains_string(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

void require_multiworld_contract_refs(const nlohmann::json& contract) {
  require(contract.at("generation_scope") == "multiworld", "contract_generation_scope_mismatch");
  require(contract.at("checks_ref") == "checks.json", "contract_checks_ref_mismatch");
  require(contract.at("items_ref") == "items.json", "contract_items_ref_mismatch");
  require(contract.at("placements_ref") == "placements.json", "contract_placements_ref_mismatch");
}

std::filesystem::path make_linkedworld_fixture(const std::filesystem::path& root,
                                               const std::string& game_key,
                                               bool complete,
                                               bool include_item_pool = true) {
  const auto linkedworld_root = root / game_key;
  write_json(linkedworld_root / "manifest" / "manifest.json", {
      {"module_id", "sekailink." + game_key},
      {"game_id", game_key},
      {"version", "smoke-1"},
      {"module_blocks", {{"generation_ir", {{"path", "generation/generation-ir.json"}}}}},
  });
  write_json(linkedworld_root / "tracker" / "location-groups.complete.json", {
      {"groups", nlohmann::json::array({
          {
              {"group_id", "light_world_surface"},
              {"label", "Light World Surface"},
              {"preferred_tab", "light-world"},
              {"locations", nlohmann::json::array({
                  {{"location_id", 1573194}, {"location_name", "Flute Spot"}, {"event_key", "0x18014A"}},
                  {{"location_id", 1573189}, {"location_name", "Sunken Treasure"}, {"event_key", "0x180145"}},
                  {{"location_id", 1573190}, {"location_name", "Reserved Smoke Location"}, {"event_key", "0x180146"}},
                  {{"location_id", 1573188}, {"location_name", "Silver Locked Smoke Location"}, {"event_key", "0x180144"}},
              })},
          },
      })},
  });
  write_json(linkedworld_root / "tracker" / "item-slots.complete.json", {
      {"slots", nlohmann::json::array({
          {
              {"slot_id", "bow"},
              {"label", "Bow"},
              {"group_id", "combat"},
              {"behavior", "progressive"},
              {"items", nlohmann::json::array({
                  {{"item_id", 100}, {"item_name", "Progressive Bow"}, {"event_key", "item.progressive_bow"}},
                  {{"item_id", 88}, {"item_name", "Silver Arrows"}, {"event_key", "item.silver_arrows"}},
              })},
          },
      })},
  });
  if (include_item_pool) {
    write_json(linkedworld_root / "generation" / "item-pool.complete.json", {
        {"items", nlohmann::json::array({
            {
                {"id", game_key + ".progressive_bow"},
                {"name", "Progressive Bow"},
                {"count", 2},
                {"classification", "progression"},
                {"advancement", true},
                {"tags", nlohmann::json::array({"progression"})},
            },
            {
                {"id", game_key + ".filler_rupee"},
                {"name", "Filler Rupee"},
                {"classification", "filler"},
                {"advancement", false},
                {"tags", nlohmann::json::array({"filler"})},
            },
        })},
    });
  }
  auto catalog = nlohmann::json{
      {"locations_ref", "tracker/location-groups.complete.json"},
      {"locations_shape", "location_groups"},
      {"items_ref", "tracker/item-slots.complete.json"},
      {"items_shape", "item_slots"},
  };
  if (include_item_pool) {
    catalog["item_pool_ref"] = "generation/item-pool.complete.json";
    catalog["item_pool_shape"] = "item_pool";
  }
  catalog["logic_rules_ref"] = "generation/logic.complete.json";
  catalog["logic_rules_shape"] = "region_rule_graph";
  catalog["placement_rules_ref"] = "generation/placement.complete.json";
  catalog["placement_rules_shape"] = "placement_constraints";
  write_json(linkedworld_root / "generation" / "logic.complete.json", {
      {"schema_version", "sekailink-logic-rules-v1"},
      {"rule_language", "sekailink-expr-v1"},
      {"option_profile", "smoke"},
      {"starting_state", {{"facts", nlohmann::json::array({"start"})}}},
      {"item_effects", {
          {game_key + ".progressive_bow", {
              {"type", "progressive"},
              {"stages", nlohmann::json::array({
                  {{"grants", nlohmann::json::array({"has_bow"})}},
                  {{"grants", nlohmann::json::array({"has_silver_bow"})}},
              })},
          }},
      }},
      {"starting_regions", nlohmann::json::array({"smoke.start"})},
      {"edges", nlohmann::json::array({
          {
              {"from_region", "smoke.start"},
              {"to_region", "smoke.bow_locked"},
              {"requires", {{"op", "fact"}, {"id", "has_bow"}}},
          },
          {
              {"from_region", "smoke.bow_locked"},
              {"to_region", "smoke.silver_locked"},
              {"requires", {{"op", "fact"}, {"id", "has_silver_bow"}}},
          },
      })},
      {"location_region_bindings", nlohmann::json::array({
          {{"location_id", "1573194"}, {"region", "smoke.start"}},
          {{"location_id", "1573189"}, {"region", "smoke.bow_locked"}},
          {{"location_id", "1573188"}, {"region", "smoke.silver_locked"}},
      })},
      {"fact_names", nlohmann::json::array({"start", "has_bow", "has_silver_bow"})},
      {"regions", nlohmann::json::array({"smoke.start", "smoke.bow_locked", "smoke.silver_locked"})},
      {"locations", nlohmann::json::array({
          {{"id", "1573194"}, {"requires", {{"op", "true"}}}},
          {{"id", "1573189"}, {"requires", {{"op", "fact"}, {"id", "has_bow"}}}},
          {{"id", "1573188"}, {"requires", {{"op", "true"}}}},
      })},
      {"region_graph", {
          {"status", "declared-consumable-smoke"},
          {"regions", nlohmann::json::array({
              {{"id", "region.smoke.open"}},
              {{"id", "region.smoke.silver"}},
          })},
          {"starting_regions", nlohmann::json::array({"region.smoke.open"})},
          {"edges", nlohmann::json::array({
              {
                  {"from_region", "region.smoke.open"},
                  {"to_region", "region.smoke.silver"},
                  {"requires", {{"op", "fact"}, {"id", "has_silver_bow"}}},
              },
          })},
          {"location_region_bindings", nlohmann::json::array({
              {{"location_id", "1573194"}, {"region", "region.smoke.open"}},
              {{"location_id", "1573189"}, {"region", "region.smoke.open"}},
              {{"location_id", "1573188"}, {"region", "region.smoke.silver"}},
          })},
          {"fact_names", nlohmann::json::array({"has_bow", "has_silver_bow"})},
      }},
      {"rules", nlohmann::json::object()},
  });
  write_json(linkedworld_root / "generation" / "placement.complete.json", {
      {"schema_version", "sekailink-placement-rules-v1"},
      {"option_profile", "smoke"},
      {"fillable_locations_ref", "tracker/location-groups.complete.json"},
      {"fillable_locations", nlohmann::json::array({"1573194", "1573189", "1573190", "1573188"})},
      {"preplacements", nlohmann::json::array({
          {{"location_id", "1573194"}, {"item_id", game_key + ".progressive_bow"}},
      })},
      {"reserved_locations", nlohmann::json::array({"1573190"})},
      {"item_constraints", nlohmann::json::array()},
      {"location_constraints", nlohmann::json::array({
          {{"id", "1573194"}, {"allow_item_tags", nlohmann::json::array({"progression"})}},
          {{"id", "1573189"}, {"allow_item_tags", nlohmann::json::array({"progression"})}},
          {{"id", "1573188"}, {"allow_item_tags", nlohmann::json::array({"useful", "filler"})}},
      })},
  });
  write_json(linkedworld_root / "patch" / "patch.manifest.json", {
      {"linkedworld_id", "linkedworld." + game_key},
      {"patch", {
          {"schema_version", "sekailink-patch-contract-v1"},
          {"mode", "contract_only"},
          {"artifact_kind", "contract"},
          {"artifact_extension", ".patch.json"},
          {"emission", {
              {"package_directory", "patches/"},
              {"package_filename_template", "slot-{slot_id}.patch.json"},
              {"patch_contract_directory", "patch_contracts/"},
              {"patch_contract_filename_template", "slot-{slot_id}.patch.contract.json"},
          }},
          {"apply_host", {{"host", "sekaiemu-smoke"}}},
          {"server_dispatch", {{"enabled", false}}},
      }},
  });
  write_json(linkedworld_root / "generation" / "generation-ir.json", {
      {"linkedworld_id", "linkedworld." + game_key},
      {"game_key", game_key},
      {"version", "smoke-1"},
      {"capabilities", {
          {"can_validate_options", complete},
          {"can_build_item_pool", complete},
          {"can_solve_logic", complete},
          {"can_place_items", complete},
          {"can_emit_patch", complete},
          {"can_emit_room_contract", complete},
          {"external_tools_required", nlohmann::json::array()},
          {"unsupported_options", nlohmann::json::array()},
      }},
      {"logic", {
          {"completion", {{"type", "collect_item"}, {"item_id", "triforce"}}},
          {"goals", nlohmann::json::array({{{"id", "goal.triforce"}, {"name", "Triforce"}}})},
      }},
      {"catalog", catalog},
      {"patch", {{"manifest_ref", "patch/patch.manifest.json"}, {"rom_family", "snes"}}},
      {"runtime", {{"memory_contract_ref", "runtime/memory.json"}}},
  });
  return linkedworld_root;
}

std::filesystem::path make_server_dispatch_linkedworld_fixture(const std::filesystem::path& root) {
  const auto linkedworld_root = root / "oot_soh";
  write_json(linkedworld_root / "manifest" / "manifest.json", {
      {"module_id", "sekailink-linkedworld-soh"},
      {"game_id", "ship_of_harkinian"},
      {"version", "cycle1"},
      {"module_blocks", {{"generation_ir", {{"path", "generation/generation-ir.json"}}}}},
      {"runtime_requirements", {
          {"host", "installed_runtime"},
          {"memory_interface", "server_dispatch"},
          {"system", "pc"},
          {"runner", "soh"},
      }},
  });
  write_json(linkedworld_root / "patch" / "patch.manifest.json", {
      {"patch", {
          {"schema_version", "sekailink-patch-contract-v1"},
          {"mode", "server_dispatch"},
          {"artifact_kind", "none"},
          {"requires_base_asset", false},
          {"emission", {
              {"patch_contract_directory", "patch_contracts/"},
              {"patch_contract_filename_template", "slot-{slot_id}.patch.contract.json"},
          }},
          {"server_dispatch", {
              {"enabled", true},
              {"target", "link_room"},
              {"transport", "room_contract"},
              {"dispatch_timing", "before_runtime_join"},
              {"payload_ref", "link_room_seed_contract.json"},
              {"ack", {{"required", true}, {"success_field", "accepted"}, {"failure_field", "error"}}},
          }},
      }},
  });
  write_json(linkedworld_root / "generation" / "generation-ir.json", {
      {"linkedworld_id", "oot_soh"},
      {"game_key", "ship_of_harkinian"},
      {"version", "cycle1"},
      {"capabilities", {
          {"can_validate_options", true},
          {"can_build_item_pool", false},
          {"can_solve_logic", false},
          {"can_place_items", false},
          {"can_emit_patch", true},
          {"can_emit_room_contract", true},
          {"external_tools_required", nlohmann::json::array()},
          {"unsupported_options", nlohmann::json::array()},
      }},
      {"catalog", {
          {"generation_scope", "server-first-minimal"},
      }},
      {"patch", {
          {"mode", "server_dispatch"},
          {"manifest_ref", "patch/patch.manifest.json"},
      }},
      {"runtime", {
          {"host", "installed_runtime"},
          {"memory_interface", "server_dispatch"},
          {"system", "pc"},
          {"runner", "soh"},
      }},
  });
  return linkedworld_root;
}

}  // namespace

int main() {
  try {
    using namespace sekailink_server;

    const auto root = std::filesystem::temp_directory_path() / "sekailink_generic_generation_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    std::string error;
    const auto complete_root = make_linkedworld_fixture(root / "linkedworlds", "alttp", true);
    const auto incomplete_root = make_linkedworld_fixture(root / "linkedworlds", "earthbound", false);
    const auto missing_pool_root = make_linkedworld_fixture(root / "linkedworlds", "smoke_missing_pool", true, false);

    const auto complete_surface = load_linkedworld_generation_surface(complete_root, &error);
    require(complete_surface.has_value(), "complete_linkedworld_load_failed:" + error);
    require(missing_required_generation_capabilities(complete_surface->capabilities).empty(),
            "complete_linkedworld_capabilities_failed");
    require(complete_surface->generation_rules.contains("logic_rules"), "complete_logic_rules_missing");
    require(complete_surface->generation_rules.contains("placement_rules"), "complete_placement_rules_missing");
    require(complete_surface->generation_rules.value("logic_rules_shape", std::string{}) == "region_rule_graph",
            "complete_logic_rules_shape_mismatch");
    require(complete_surface->generation_rules.value("placement_rules_shape", std::string{}) ==
                "placement_constraints",
            "complete_placement_rules_shape_mismatch");
    require(complete_surface->generation_rules.value("logic_rules_ref", std::string{}) ==
                "generation/logic.complete.json",
            "complete_logic_rules_ref_mismatch");
    require(complete_surface->generation_rules.value("placement_rules_ref", std::string{}) ==
                "generation/placement.complete.json",
            "complete_placement_rules_ref_mismatch");
    require(complete_surface->generation_rules.at("logic_rules").value("schema_version", std::string{}) ==
                "sekailink-logic-rules-v1",
            "complete_logic_rules_schema_mismatch");
    require(complete_surface->generation_rules.at("logic_rules").at("starting_regions").size() == 1,
            "complete_region_graph_starting_regions_mismatch");
    require(complete_surface->generation_rules.at("logic_rules").at("edges").size() == 2,
            "complete_region_graph_edges_mismatch");
    require(complete_surface->generation_rules.at("logic_rules").at("location_region_bindings").size() == 3,
            "complete_region_graph_location_bindings_mismatch");
    require(complete_surface->generation_rules.at("logic_rules").at("fact_names").size() == 3,
            "complete_region_graph_fact_names_mismatch");
    require(complete_surface->generation_rules.at("logic_rules").at("region_graph").at("regions").size() == 2,
            "complete_region_graph_declared_regions_mismatch");
    require(complete_surface->generation_rules.at("placement_rules").value("schema_version", std::string{}) ==
                "sekailink-placement-rules-v1",
            "complete_placement_rules_schema_mismatch");
    require(complete_surface->patch.value("mode", std::string{}) == "contract_only",
            "complete_patch_manifest_mode_not_resolved");
    require(complete_surface->patch.contains("manifest"), "complete_patch_manifest_not_loaded");
    require(complete_surface->patch.at("manifest").at("patch").value("schema_version", std::string{}) ==
                "sekailink-patch-contract-v1",
            "complete_patch_manifest_schema_mismatch");

    const auto incomplete_surface = load_linkedworld_generation_surface(incomplete_root, &error);
    require(incomplete_surface.has_value(), "incomplete_linkedworld_load_failed:" + error);
    require(!missing_required_generation_capabilities(incomplete_surface->capabilities).empty(),
            "incomplete_linkedworld_should_be_capability_gated");
    const auto missing_pool_surface = load_linkedworld_generation_surface(missing_pool_root, &error);
    require(missing_pool_surface.has_value(), "missing_pool_linkedworld_load_failed:" + error);
    require(missing_required_generation_capabilities(missing_pool_surface->capabilities).empty(),
            "missing_pool_capabilities_should_be_complete");
    const auto missing_pool_requirements =
        missing_required_generation_surface_requirements(*missing_pool_surface);
    require(std::find(missing_pool_requirements.begin(),
                      missing_pool_requirements.end(),
                      "missing_generation_item_pool") != missing_pool_requirements.end(),
            "missing_pool_requirement_not_reported");

    const auto server_dispatch_root = make_server_dispatch_linkedworld_fixture(root / "linkedworlds");
    const auto server_dispatch_contract_surface =
        load_linkedworld_generation_surface(server_dispatch_root, &error);
    require(server_dispatch_contract_surface.has_value(), "server_dispatch_contract_load_failed:" + error);
    require(!missing_required_generation_capabilities(server_dispatch_contract_surface->capabilities).empty(),
            "server_dispatch_native_capabilities_should_remain_gated");
    GenerationPackageRequest server_dispatch_contract_request;
    server_dispatch_contract_request.job_id = "job-soh-server-dispatch";
    server_dispatch_contract_request.room_id = "room-soh-server-dispatch";
    server_dispatch_contract_request.seed_id = "seed-soh-server-dispatch";
    server_dispatch_contract_request.rng_seed = "deterministic-soh-server-dispatch";
    server_dispatch_contract_request.output_root = root / "packages";
    server_dispatch_contract_request.slots.push_back({
        .slot_id = 1,
        .user_id = 1001,
        .display_name = "Jade",
        .game_key = "ship_of_harkinian",
        .linkedworld_id = "oot_soh",
        .config_version_id = 7001,
        .config_snapshot = {{"config_name", "Jade-SoH"}, {"values", {{"starting_age", "child"}}}},
    });
    const auto server_dispatch_contract_package =
        generate_seed_package_from_linkedworlds(server_dispatch_contract_request, {*server_dispatch_contract_surface});
    require(server_dispatch_contract_package.ok,
            "server_dispatch_contract_package_failed:" + server_dispatch_contract_package.error);
    require(server_dispatch_contract_package.manifest.at("location_count") == 0,
            "server_dispatch_contract_locations_should_be_zero");
    require(server_dispatch_contract_package.manifest.at("item_count") == 0,
            "server_dispatch_contract_items_should_be_zero");
    require(server_dispatch_contract_package.manifest.at("placement_count") == 0,
            "server_dispatch_contract_placements_should_be_zero");
    const auto server_dispatch_contract_slot_manifest =
        read_json(server_dispatch_contract_package.package_dir / "slot_manifest.1.json");
    const auto server_dispatch_contract_patch =
        read_json(server_dispatch_contract_package.package_dir /
                  server_dispatch_contract_slot_manifest.value("patch_contract_ref", std::string{}));
    require(server_dispatch_contract_slot_manifest.value("patch_mode", std::string{}) == "server_dispatch",
            "server_dispatch_contract_slot_mode_mismatch");
    require(server_dispatch_contract_slot_manifest.contains("config_snapshot"),
            "server_dispatch_contract_config_snapshot_missing");
    require(server_dispatch_contract_patch.at("server_dispatch").value("target", std::string{}) == "link_room",
            "server_dispatch_contract_target_mismatch");

    const auto rules_present_gated_root =
        make_linkedworld_fixture(root / "linkedworlds", "rules_present_gated", true);
    auto rules_present_gated_surface =
        load_linkedworld_generation_surface(rules_present_gated_root, &error);
    require(rules_present_gated_surface.has_value(), "rules_present_gated_load_failed:" + error);
    rules_present_gated_surface->capabilities.can_solve_logic = false;
    rules_present_gated_surface->capabilities.can_place_items = false;
    require(rules_present_gated_surface->generation_rules.contains("logic_rules"),
            "rules_present_logic_rules_missing");
    require(rules_present_gated_surface->generation_rules.contains("placement_rules"),
            "rules_present_placement_rules_missing");
    const auto rules_present_missing =
        missing_required_generation_surface_requirements(*rules_present_gated_surface);
    require(std::find(rules_present_missing.begin(), rules_present_missing.end(), "can_solve_logic") !=
                rules_present_missing.end(),
            "rules_present_solver_gate_missing");
    require(std::find(rules_present_missing.begin(), rules_present_missing.end(), "can_place_items") !=
                rules_present_missing.end(),
            "rules_present_placer_gate_missing");

    const auto contract_policy_root =
        make_linkedworld_fixture(root / "linkedworlds", "contract_policy_smoke", false);
    auto contract_policy_surface =
        load_linkedworld_generation_surface(contract_policy_root, &error);
    require(contract_policy_surface.has_value(), "contract_policy_load_failed:" + error);
    contract_policy_surface->generation_rules["logic_rules"]["region_graph"]["authorizes"] = {
        {"native_logic_solve", false},
    };
    contract_policy_surface->catalog["item_pool_source"]["dungeon_key_policy"] = {
        {"status", "declared-contract-not-consumed"},
        {"authorizes", {{"native_placement", false}}},
        {"separate_key_pool", nlohmann::json::array({
            {{"id", "contract_policy_smoke.key.alpha"}, {"count", 1}, {"role", "dungeon_key"}},
        })},
    };
    contract_policy_surface->generation_rules["logic_rules"]["dungeon_key_policy_binding"] = {
        {"status", "declared-contract-not-consumed"},
        {"authorizes", {{"native_logic_solve", false}}},
        {"requirements", nlohmann::json::array({
            {{"location_id", "1573188"}, {"requires_key", "contract_policy_smoke.key.alpha"}},
        })},
    };
    contract_policy_surface->generation_rules["logic_rules"]["reward_contract"] = {
        {"status", "declared-contract-not-consumed"},
        {"authorizes", {{"native_logic_solve", false}}},
        {"requirements", nlohmann::json::array({{{"id", "reward.alpha"}}})},
    };
    contract_policy_surface->generation_rules["placement_rules"]["medallion_contract"] = {
        {"status", "declared-contract-not-consumed"},
        {"authorizes", {{"native_placement", false}}},
        {"requirements", nlohmann::json::array({{{"id", "gate.alpha"}}})},
    };
    contract_policy_surface->logic["completion_contract"] = {
        {"status", "declared-contract-not-consumed"},
        {"authorizes", {{"native_logic_solve", false}}},
        {"requirements", nlohmann::json::array({{{"id", "goal.alpha"}}})},
    };
    contract_policy_surface->generation_rules["placement_rules"]["authorizes_native_placement"] = false;
    const auto contract_policy_missing =
        missing_required_generation_surface_requirements(*contract_policy_surface);
    require(contains_string(contract_policy_missing, "can_solve_logic"),
            "contract_policy_solver_capability_gate_missing");
    require(contains_string(contract_policy_missing, "can_place_items"),
            "contract_policy_placer_capability_gate_missing");
    require(contains_string(contract_policy_missing, "region_graph_not_authorizing"),
            "contract_policy_region_graph_surface_blocker_missing");
    require(contains_string(contract_policy_missing, "placement_rules_not_authorizing"),
            "contract_policy_placement_surface_blocker_missing");
    require(contains_string(contract_policy_missing, "dungeon_key_policy_not_consumed"),
            "contract_policy_dungeon_surface_blocker_missing");
    require(contains_string(contract_policy_missing, "reward_contract_not_consumed"),
            "contract_policy_reward_surface_blocker_missing");
    require(contains_string(contract_policy_missing, "medallion_contract_not_consumed"),
            "contract_policy_medallion_surface_blocker_missing");
    require(contains_string(contract_policy_missing, "completion_contract_not_consumed"),
            "contract_policy_completion_surface_blocker_missing");

    const auto logic_auth_root =
        make_linkedworld_fixture(root / "linkedworlds", "logic_auth_smoke", true);
    auto logic_auth_surface =
        load_linkedworld_generation_surface(logic_auth_root, &error);
    require(logic_auth_surface.has_value(), "logic_auth_load_failed:" + error);
    logic_auth_surface->generation_rules["logic_rules"]["ruleset"] = {
        {"status", "declared-consumable"},
        {"authorizes", {{"native_logic_solve", false}}},
    };
    const auto logic_auth_missing =
        missing_required_generation_surface_requirements(*logic_auth_surface);
    require(contains_string(logic_auth_missing, "logic_rules_not_authorizing"),
            "logic_rules_authorization_blocker_not_reported");
    logic_auth_surface->generation_rules["logic_rules"]["ruleset"].erase("authorizes");
    logic_auth_surface->generation_rules["logic_rules"]["ruleset"]["authorizes_native_logic_solve"] = true;
    const auto logic_auth_clear_missing =
        missing_required_generation_surface_requirements(*logic_auth_surface);
    require(!contains_string(logic_auth_clear_missing, "logic_rules_not_authorizing"),
            "logic_rules_authorization_alias_should_clear");

    auto candidate_logic_surface = *complete_surface;
    candidate_logic_surface.generation_rules["logic_rules"]["candidate_location_rules"] = {
        {"status", "candidate-not-consumed"},
        {"rules", nlohmann::json::array({{{"id", "1573194"}, {"requires", {{"op", "true"}}}}})},
    };
    const auto candidate_logic_missing =
        missing_required_generation_surface_requirements(candidate_logic_surface);
    require(contains_string(candidate_logic_missing, "logic_candidate_not_consumed"),
            "candidate_logic_blocker_not_reported");

    auto blocked_placement_surface = *complete_surface;
    blocked_placement_surface.generation_rules["placement_rules"]["blocks_can_place_items_until_audited"] = true;
    const auto blocked_placement_missing =
        missing_required_generation_surface_requirements(blocked_placement_surface);
    require(contains_string(blocked_placement_missing, "placement_blocked_until_audited"),
            "placement_audit_blocker_not_reported");

    auto unauthorizing_placement_surface = *complete_surface;
    unauthorizing_placement_surface.generation_rules["placement_rules"]["authorizes_native_placement"] = false;
    const auto unauthorizing_placement_missing =
        missing_required_generation_surface_requirements(unauthorizing_placement_surface);
    require(contains_string(unauthorizing_placement_missing, "placement_rules_not_authorizing"),
            "placement_authorization_blocker_not_reported");

    const auto placement_auth_root =
        make_linkedworld_fixture(root / "linkedworlds", "placement_auth_smoke", true);
    auto placement_auth_surface =
        load_linkedworld_generation_surface(placement_auth_root, &error);
    require(placement_auth_surface.has_value(), "placement_auth_load_failed:" + error);
    placement_auth_surface->generation_rules["placement_rules"]["authorizes_native_placement"] = false;
    const auto placement_auth_blocked =
        missing_required_generation_surface_requirements(*placement_auth_surface);
    require(contains_string(placement_auth_blocked, "placement_rules_not_authorizing"),
            "placement_auth_false_should_block");
    placement_auth_surface->generation_rules["placement_rules"]["authorizes_native_placement"] = true;
    const auto placement_auth_clear =
        missing_required_generation_surface_requirements(*placement_auth_surface);
    require(!contains_string(placement_auth_clear, "placement_rules_not_authorizing"),
            "placement_auth_true_should_clear");

    auto unauthorizing_region_graph_surface = *complete_surface;
    unauthorizing_region_graph_surface.generation_rules["logic_rules"]["region_graph"]["authorizes"] = {
        {"native_logic_solve", false},
        {"native_location_reachability", false},
    };
    const auto unauthorizing_region_graph_missing =
        missing_required_generation_surface_requirements(unauthorizing_region_graph_surface);
    require(contains_string(unauthorizing_region_graph_missing, "region_graph_not_authorizing"),
            "region_graph_authorization_blocker_not_reported");

    auto placeholder_region_graph_surface = *complete_surface;
    placeholder_region_graph_surface.generation_rules["logic_rules"]["region_graph"]["edges"][0]["requires"] = {
        {"op", "fact"},
        {"id", "predicate.region.traversal_declared_not_consumed:region.smoke.silver"},
    };
    const auto placeholder_region_graph_missing =
        missing_required_generation_surface_requirements(placeholder_region_graph_surface);
    require(contains_string(placeholder_region_graph_missing, "region_graph_placeholder_edges"),
            "region_graph_placeholder_blocker_not_reported");

    const auto edge_contract_root =
        make_linkedworld_fixture(root / "linkedworlds", "edge_contract_smoke", true);
    auto edge_contract_surface =
        load_linkedworld_generation_surface(edge_contract_root, &error);
    require(edge_contract_surface.has_value(), "edge_contract_load_failed:" + error);
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["edges"][0]["id"] =
        "edge_contract_smoke.route.silver";
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["edges"][0]["requires"] = {
        {"op", "fact"},
        {"id", "predicate.region.traversal_declared_not_consumed:region.smoke.silver"},
    };
    const auto unresolved_edge_contract_missing =
        missing_required_generation_surface_requirements(*edge_contract_surface);
    require(contains_string(unresolved_edge_contract_missing, "region_graph_placeholder_edges"),
            "edge_contract_unresolved_placeholder_not_reported");
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["edge_audit"]["consumed_edges"] =
        nlohmann::json::array({
            {
                {"edge_id", "edge_contract_smoke.route.silver"},
                {"status", "declared-consumed"},
                {"proof_required", true},
                {"authorizes", {{"native_logic_solve", false}}},
                {"requires", {{"op", "fact"}, {"id", "has_silver_bow"}}},
            },
        });
    const auto unauthorizing_edge_contract_missing =
        missing_required_generation_surface_requirements(*edge_contract_surface);
    require(contains_string(unauthorizing_edge_contract_missing, "region_graph_placeholder_edges"),
            "edge_contract_unauthorizing_consumed_edge_should_not_clear_placeholder");
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["edge_audit"]["consumed_edges"][0]
        ["authorizes"]["native_logic_solve"] = true;
    auto edge_contract_missing_proof =
        missing_required_generation_surface_requirements(*edge_contract_surface);
    require(contains_string(edge_contract_missing_proof, "region_graph_placeholder_edges"),
            "edge_contract_missing_fact_proof_should_not_clear_placeholder");
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["edge_audit"]["consumed_edges"][0]
        ["proof"] = {
            {"status", "complete-consumed"},
            {"consumed", true},
            {"authorizes", {{"native_logic_solve", false}}},
            {"produced_facts", nlohmann::json::array({"has_silver_bow"})},
            {"consumed_facts", nlohmann::json::array({"has_silver_bow"})},
        };
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["authorizes"] = {
        {"native_logic_solve", false},
    };
    const auto edge_contract_region_auth_blocked =
        missing_required_generation_surface_requirements(*edge_contract_surface);
    require(!contains_string(edge_contract_region_auth_blocked, "region_graph_placeholder_edges"),
            "edge_contract_complete_edge_proof_should_clear_placeholder_before_region_graph_auth");
    require(contains_string(edge_contract_region_auth_blocked, "region_graph_not_authorizing"),
            "edge_contract_region_graph_authorization_transition_should_block");
    edge_contract_surface->generation_rules["logic_rules"]["native_probe_contract_index"]["region_graph"] = {
        {"contracts", nlohmann::json::array({
            {
                {"id", "edge_contract_smoke.native.region"},
                {"surface_id", "region_graph"},
                {"facts", nlohmann::json::array({"has_silver_bow"})},
                {"expected_reachable_regions", nlohmann::json::array({"region.smoke.silver"})},
            },
        })},
    };
    const auto edge_contract_native_probe_authorized_missing =
        missing_required_generation_surface_requirements(*edge_contract_surface);
    require(!contains_string(edge_contract_native_probe_authorized_missing, "region_graph_not_authorizing"),
            "edge_contract_consumed_edges_and_native_probe_should_clear_region_graph_authorization");
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"].erase("authorizes");
    edge_contract_surface->generation_rules["logic_rules"]["region_graph"]["authorizes_native_logic_solve"] = true;
    const auto authorized_edge_contract_missing =
        missing_required_generation_surface_requirements(*edge_contract_surface);
    require(!contains_string(authorized_edge_contract_missing, "region_graph_placeholder_edges"),
            "edge_contract_authorized_consumed_edge_should_clear_placeholder");
    require(!contains_string(authorized_edge_contract_missing, "region_graph_not_authorizing"),
            "edge_contract_region_graph_authorization_should_clear");
    require(!contains_string(authorized_edge_contract_missing, "region_graph_unknown_fact_name"),
            "edge_contract_authorized_consumed_edge_should_replace_placeholder_fact");
    GenerationPackageRequest edge_contract_request;
    edge_contract_request.job_id = "job-edge-contract";
    edge_contract_request.room_id = "room-edge-contract";
    edge_contract_request.seed_id = "seed-edge-contract";
    edge_contract_request.rng_seed = "deterministic-edge-contract-seed";
    edge_contract_request.output_root = root / "packages";
    edge_contract_request.slots.push_back({
        .slot_id = 1,
        .user_id = 3001,
        .display_name = "Edge Contract Smoke",
        .game_key = "edge_contract_smoke",
        .linkedworld_id = "linkedworld.edge_contract_smoke",
        .config_version_id = 701,
    });
    const auto edge_contract_package =
        generate_seed_package_from_linkedworlds(edge_contract_request, {*edge_contract_surface});
    require(edge_contract_package.ok,
            "edge_contract_package_generation_failed:" + edge_contract_package.error);
    const auto edge_contract_checks =
        read_json(edge_contract_package.package_dir / "checks.json");
    require(edge_contract_checks.dump().find("traversal_declared_not_consumed") == std::string::npos,
            "edge_contract_package_should_emit_effective_edges_without_placeholder");
    require(edge_contract_checks.dump().find("has_silver_bow") != std::string::npos,
            "edge_contract_package_should_emit_replacement_edge_requirement");

    auto edge_audit_surface = *complete_surface;
    edge_audit_surface.generation_rules["logic_rules"]["region_graph"]["edge_audit"] = {
        {"status", "declared-consumable"},
        {"authorizes", {{"native_logic_solve", true}}},
        {"edge_blocker_requirements", {
            {"status", "declared-not-consumed"},
            {"authorizes", {{"native_logic_solve", false}}},
            {"requirements", nlohmann::json::array({
                {{"edge_id", "smoke.edge.silver"}, {"requires", {{"op", "fact"}, {"id", "has_silver_bow"}}}},
            })},
        }},
    };
    const auto edge_audit_missing =
        missing_required_generation_surface_requirements(edge_audit_surface);
    require(contains_string(edge_audit_missing, "region_graph_edge_blockers_not_consumed"),
            "region_graph_edge_blocker_requirements_not_reported");
    edge_audit_surface.generation_rules["logic_rules"]["region_graph"]["edge_audit"]
        ["edge_blocker_requirements"]["status"] = "declared-consumed";
    edge_audit_surface.generation_rules["logic_rules"]["region_graph"]["edge_audit"]
        ["edge_blocker_requirements"]["authorizes"]["native_logic_solve"] = true;
    const auto edge_audit_clear_missing =
        missing_required_generation_surface_requirements(edge_audit_surface);
    require(!contains_string(edge_audit_clear_missing, "region_graph_edge_blockers_not_consumed"),
            "region_graph_edge_blocker_requirements_should_clear_when_consumed_authorized");

    auto missing_contract_surface = *complete_surface;
    missing_contract_surface.generation_rules["logic_rules"]["region_graph"]["edge_audit"] = {
        {"status", "declared-consumable"},
        {"authorizes", {{"native_logic_solve", true}}},
        {"missing_generation_contract_surfaces", {
            {"schema_version", "sekailink-linkedworld-generation-contract-surfaces-v1"},
            {"status", "declared-contract-not-consumed"},
            {"authorizes", {{"native_logic_solve", false}}},
            {"surfaces", nlohmann::json::array({
                {
                    {"contract_id", "contract.smoke.route_state.v1"},
                    {"blocker_key", "route_state_missing"},
                    {"status", "declared-contract-not-consumed"},
                },
            })},
        }},
    };
    const auto missing_contract_missing =
        missing_required_generation_surface_requirements(missing_contract_surface);
    require(contains_string(missing_contract_missing, "region_graph_edge_blockers_not_consumed"),
            "region_graph_missing_generation_contract_surface_not_reported");

    auto edge_audit_invalid_surface = *complete_surface;
    edge_audit_invalid_surface.generation_rules["logic_rules"]["region_graph"]["edge_audit"]
        ["edge_blocker_requirements"] = "declared-not-consumed";
    edge_audit_invalid_surface.generation_rules["logic_rules"]["region_graph"]["edge_audit"]
        ["missing_generation_contract_surfaces"] = nlohmann::json::array();
    const auto edge_audit_invalid_missing =
        missing_required_generation_surface_requirements(edge_audit_invalid_surface);
    require(contains_string(edge_audit_invalid_missing, "region_graph_invalid_edge_blocker_requirements"),
            "region_graph_edge_blocker_requirements_invalid_not_reported");
    require(contains_string(edge_audit_invalid_missing, "region_graph_invalid_missing_generation_contract_surfaces"),
            "region_graph_missing_generation_contract_surfaces_invalid_not_reported");

    auto invalid_region_graph_surface = *complete_surface;
    invalid_region_graph_surface.generation_rules["logic_rules"]["region_graph"]["starting_regions"] =
        nlohmann::json::array();
    invalid_region_graph_surface.generation_rules["logic_rules"]["region_graph"]["edges"][0]["to_region"] =
        "region.smoke.missing";
    invalid_region_graph_surface.generation_rules["logic_rules"]["region_graph"]["location_region_bindings"][0]
        ["region"] = "region.smoke.missing";
    invalid_region_graph_surface.generation_rules["logic_rules"]["region_graph"]["edges"][0]["requires"] = {
        {"op", "fact"},
        {"id", "missing_fact"},
    };
    const auto invalid_region_graph_missing =
        missing_required_generation_surface_requirements(invalid_region_graph_surface);
    require(contains_string(invalid_region_graph_missing, "region_graph_missing_starting_regions"),
            "region_graph_empty_starting_regions_not_reported");
    require(contains_string(invalid_region_graph_missing, "region_graph_unknown_edge_region"),
            "region_graph_unknown_edge_region_not_reported");
    require(contains_string(invalid_region_graph_missing, "region_graph_unknown_location_region"),
            "region_graph_unknown_location_region_not_reported");
    require(contains_string(invalid_region_graph_missing, "region_graph_unknown_fact_name"),
            "region_graph_unknown_fact_name_not_reported");

    auto dungeon_key_policy_surface = *complete_surface;
    dungeon_key_policy_surface.catalog["item_pool_source"]["dungeon_key_policy"] = {
        {"status", "declared-not-consumed"},
        {"authorizes", {{"native_placement", false}}},
        {"separate_key_pool", nlohmann::json::array({
            {{"id", "alttp.big_key.smoke"}, {"count", 1}, {"role", "dungeon_key"}},
        })},
    };
    dungeon_key_policy_surface.generation_rules["logic_rules"]["dungeon_key_policy_binding"] = {
        {"status", "declared-not-consumed"},
        {"authorizes", {{"native_logic_solve", false}}},
        {"blocked_big_chest_rules", nlohmann::json::array({
            {
                {"location_id", "1573188"},
                {"requires_key", "alttp.big_key.smoke"},
                {"self_lock_policy", "blocked"},
            },
        })},
    };
    const auto dungeon_key_policy_missing =
        missing_required_generation_surface_requirements(dungeon_key_policy_surface);
    require(contains_string(dungeon_key_policy_missing, "dungeon_key_policy_not_consumed"),
            "dungeon_key_policy_blocker_not_reported");

    auto dungeon_key_policy_status_surface = *complete_surface;
    dungeon_key_policy_status_surface.catalog["item_pool_source"]["dungeon_key_policy"] = {
        {"status", "declared-partial-consumable-not-authorizing"},
    };
    dungeon_key_policy_status_surface.generation_rules["logic_rules"]["dungeon_key_policy_binding"] = {
        {"status", "declared-partial-consumable-not-authorizing"},
    };
    const auto dungeon_key_policy_status_missing =
        missing_required_generation_surface_requirements(dungeon_key_policy_status_surface);
    require(contains_string(dungeon_key_policy_status_missing, "dungeon_key_policy_not_consumed"),
            "dungeon_key_policy_not_authorizing_status_not_reported");

    auto dungeon_key_policy_invalid_surface = *complete_surface;
    dungeon_key_policy_invalid_surface.catalog["item_pool_source"]["dungeon_key_policy"] =
        nlohmann::json::array();
    dungeon_key_policy_invalid_surface.generation_rules["logic_rules"]["dungeon_key_policy_binding"] =
        nlohmann::json::array();
    const auto dungeon_key_policy_invalid_missing =
        missing_required_generation_surface_requirements(dungeon_key_policy_invalid_surface);
    require(contains_string(dungeon_key_policy_invalid_missing, "dungeon_key_policy_invalid"),
            "dungeon_key_policy_invalid_not_reported");
    require(contains_string(dungeon_key_policy_invalid_missing, "dungeon_key_policy_binding_invalid"),
            "dungeon_key_policy_binding_invalid_not_reported");

    const auto key_policy_root =
        make_linkedworld_fixture(root / "linkedworlds", "key_policy_smoke", true);
    auto key_policy_surface =
        load_linkedworld_generation_surface(key_policy_root, &error);
    require(key_policy_surface.has_value(), "key_policy_load_failed:" + error);
    key_policy_surface->catalog["item_pool_source"]["dungeon_key_policy"] = {
        {"status", "declared-consumed"},
        {"consumed", true},
        {"proof_required", true},
        {"authorizes", {
            {"native_placement", true},
            {"native_item_pool", true},
            {"native_dungeon_key_policy", true},
        }},
        {"separate_key_pool", nlohmann::json::array({
            {{"id", "key_policy_smoke.key.alpha"}, {"count", 1}, {"role", "dungeon_key"}},
            {{"id", "key_policy_smoke.small_key.alpha"},
             {"count", 1},
             {"role", "small_key"},
             {"tags", nlohmann::json::array({"filler"})}},
        })},
        {"proof", {
            {"status", "complete-consumed"},
            {"consumed", true},
            {"authorizes", {{"native_placement", true}}},
            {"small_key_proof", {
                {"proved_item_ids", nlohmann::json::array({"key_policy_smoke.small_key.alpha"})},
            }},
        }},
        {"native_probe_contract_index", {
            {"contracts", nlohmann::json::array({
                {
                    {"id", "key_policy_smoke.native.key_policy"},
                    {"family", "dungeon_key_policy"},
                    {"expected_placeable_items", nlohmann::json::array({"key_policy_smoke.small_key.alpha"})},
                },
            })},
        }},
    };
    key_policy_surface->generation_rules["logic_rules"]["dungeon_key_policy_binding"] = {
        {"status", "declared-consumed"},
        {"consumed", true},
        {"proof_required", true},
        {"authorizes", {{"native_logic_solve", true}}},
        {"requirements", nlohmann::json::array({
            {{"location_id", "1573188"}, {"requires_key", "key_policy_smoke.key.alpha"}},
        })},
        {"proof", {
            {"status", "complete-consumed"},
            {"consumed", true},
            {"authorizes", {{"native_logic_solve", true}}},
            {"self_lock_proof", {
                {"proved_location_ids", nlohmann::json::array({"1573188"})},
            }},
        }},
    };
    auto key_policy_authorization_blocked_surface = *key_policy_surface;
    key_policy_authorization_blocked_surface.catalog["item_pool_source"]["dungeon_key_policy"]["authorizes"]
                                            ["native_placement"] = false;
    key_policy_authorization_blocked_surface.catalog["item_pool_source"]["dungeon_key_policy"]["authorizes"]
                                            ["native_item_pool"] = false;
    key_policy_authorization_blocked_surface.catalog["item_pool_source"]["dungeon_key_policy"]["authorizes"]
                                            ["native_dungeon_key_policy"] = false;
    key_policy_authorization_blocked_surface.generation_rules["logic_rules"]["dungeon_key_policy_binding"]
                                            ["authorizes"]["native_logic_solve"] = false;
    const auto key_policy_authorization_blocked =
        missing_required_generation_surface_requirements(key_policy_authorization_blocked_surface);
    require(contains_string(key_policy_authorization_blocked, "dungeon_key_policy_not_consumed"),
            "key_policy_complete_proof_without_surface_authorization_should_block");
    require(!contains_string(key_policy_authorization_blocked, "native_probe_contract_failed"),
            "key_policy_authorization_block_should_not_require_failed_probe");
    const auto authorized_key_policy_missing =
        missing_required_generation_surface_requirements(*key_policy_surface);
    require(!contains_string(authorized_key_policy_missing, "dungeon_key_policy_not_consumed"),
            "authorized_key_policy_should_not_block");
    require(!contains_string(authorized_key_policy_missing, "native_probe_contract_failed"),
            "authorized_key_policy_native_probe_should_pass");
    auto key_policy_missing_small_key_proof_surface = *key_policy_surface;
    key_policy_missing_small_key_proof_surface.catalog["item_pool_source"]["dungeon_key_policy"]["proof"]
                                               ["small_key_proof"]["proved_item_ids"] = nlohmann::json::array();
    const auto key_policy_missing_small_key_proof =
        missing_required_generation_surface_requirements(key_policy_missing_small_key_proof_surface);
    require(contains_string(key_policy_missing_small_key_proof, "dungeon_key_policy_not_consumed"),
            "key_policy_missing_small_key_proof_should_block");
    auto key_policy_missing_self_lock_proof_surface = *key_policy_surface;
    key_policy_missing_self_lock_proof_surface.generation_rules["logic_rules"]["dungeon_key_policy_binding"]
                                               ["proof"]["self_lock_proof"]["proved_location_ids"] =
        nlohmann::json::array();
    const auto key_policy_missing_self_lock_proof =
        missing_required_generation_surface_requirements(key_policy_missing_self_lock_proof_surface);
    require(contains_string(key_policy_missing_self_lock_proof, "dungeon_key_policy_not_consumed"),
            "key_policy_missing_self_lock_proof_should_block");
    key_policy_surface->catalog["item_pool_source"]["dungeon_key_policy"]["authorizes"]["native_item_pool"] = false;
    const auto pool_blocking_key_policy_missing =
        missing_required_generation_surface_requirements(*key_policy_surface);
    require(contains_string(pool_blocking_key_policy_missing, "dungeon_key_policy_not_consumed"),
            "key_policy_native_item_pool_authorization_should_block");

    auto supported_progressive_effect_surface = *complete_surface;
    const auto supported_progressive_effect_missing =
        missing_required_generation_surface_requirements(supported_progressive_effect_surface);
    require(!contains_string(supported_progressive_effect_missing, "unsupported_progressive_effects"),
            "supported_progressive_effect_should_not_be_blocked");

    auto unsupported_progressive_effect_surface = *complete_surface;
    unsupported_progressive_effect_surface.generation_rules["logic_rules"]["item_effects"]["alttp.progressive_bow"] = {
        {"type", "progressive"},
        {"grant_by_count", {{"1", nlohmann::json::array({"has_bow"})}}},
    };
    const auto unsupported_progressive_effect_missing =
        missing_required_generation_surface_requirements(unsupported_progressive_effect_surface);
    require(contains_string(unsupported_progressive_effect_missing, "unsupported_progressive_effects"),
            "unsupported_progressive_effect_blocker_not_reported");

    for (const auto& patch_mode : {"artifact", "contract_only", "server_dispatch", "none", "external_import"}) {
      auto supported_patch_mode_surface = *complete_surface;
      supported_patch_mode_surface.patch["mode"] = patch_mode;
      const auto supported_patch_mode_missing =
          missing_required_generation_surface_requirements(supported_patch_mode_surface);
      require(!contains_string(supported_patch_mode_missing, "unsupported_patch_mode"),
              std::string{"supported_patch_mode_blocked:"} + patch_mode);
    }

    auto unsupported_patch_mode_surface = *complete_surface;
    unsupported_patch_mode_surface.patch["mode"] = "mystery_transport";
    const auto unsupported_patch_mode_missing =
        missing_required_generation_surface_requirements(unsupported_patch_mode_surface);
    require(contains_string(unsupported_patch_mode_missing, "unsupported_patch_mode"),
            "unsupported_patch_mode_not_reported");

    auto count_mismatch_surface = *complete_surface;
    count_mismatch_surface.catalog["item_pool"].erase(count_mismatch_surface.catalog["item_pool"].begin());
    const auto count_mismatch_missing =
        missing_required_generation_surface_requirements(count_mismatch_surface);
    require(contains_string(count_mismatch_missing, "generation_item_location_count_mismatch"),
            "generation_item_location_count_mismatch_not_reported");

    auto duplicate_location_rule_surface = *complete_surface;
    duplicate_location_rule_surface.generation_rules["logic_rules"]["locations"].push_back(
        {{"id", "1573189"}, {"requires", {{"op", "fact"}, {"id", "different_fact"}}}});
    const auto duplicate_location_rule_missing =
        missing_required_generation_surface_requirements(duplicate_location_rule_surface);
    require(contains_string(duplicate_location_rule_missing, "duplicate_location_rules"),
            "duplicate_location_rules_not_reported");
    require(contains_string(duplicate_location_rule_missing, "conflicting_location_rules"),
            "conflicting_location_rules_not_reported");

    auto catalog_requires_conflict_surface = *complete_surface;
    catalog_requires_conflict_surface.catalog["locations"][0]["requires"] = {{"op", "fact"}, {"id", "catalog_fact"}};
    catalog_requires_conflict_surface.generation_rules["logic_rules"]["locations"][0]["requires"] = {
        {"op", "fact"},
        {"id", "rule_fact"},
    };
    const auto catalog_requires_conflict_missing =
        missing_required_generation_surface_requirements(catalog_requires_conflict_surface);
    require(contains_string(catalog_requires_conflict_missing, "catalog_location_rule_requires_conflict"),
            "catalog_location_rule_requires_conflict_not_reported");

    auto unauthorized_refinement_surface = *complete_surface;
    unauthorized_refinement_surface.generation_rules["logic_rules"]["location_refinements"] = {
        {"status", "declared-consumable"},
        {"refinements", nlohmann::json::array({
            {{"location_id", "1573189"}, {"requires", {{"op", "fact"}, {"id", "refinement_fact"}}}},
        })},
    };
    const auto unauthorized_refinement_missing =
        missing_required_generation_surface_requirements(unauthorized_refinement_surface);
    require(contains_string(unauthorized_refinement_missing, "location_refinements_not_authorizing"),
            "location_refinements_without_authorization_not_reported");

    auto unauthorized_segmentation_surface = *complete_surface;
    unauthorized_segmentation_surface.generation_rules["logic_rules"]["location_rule_segmentation"] = {
        {"status", "declared-consumable"},
        {"segments", nlohmann::json::array({
            {
                {"id", "segment.auth_smoke.open"},
                {"location_ids", nlohmann::json::array({"1573188"})},
                {"requires", {{"op", "true"}}},
            },
        })},
    };
    const auto unauthorized_segmentation_missing =
        missing_required_generation_surface_requirements(unauthorized_segmentation_surface);
    require(contains_string(unauthorized_segmentation_missing, "location_rule_segmentation_not_authorizing"),
            "location_rule_segmentation_without_authorization_not_reported");

    const auto fillable_audit_root =
        make_linkedworld_fixture(root / "linkedworlds", "fillable_audit_smoke", true);
    auto fillable_audit_surface =
        load_linkedworld_generation_surface(fillable_audit_root, &error);
    require(fillable_audit_surface.has_value(), "fillable_audit_load_failed:" + error);
    fillable_audit_surface->generation_rules["placement_rules"]["fillable_locations_source"] = {
        {"validation", {
            {"status", "declared-not-consumed"},
            {"blocks_can_place_items_until_audited", true},
        }},
        {"risk_audit", {
            {"status", "candidate-not-consumed"},
            {"location_tags_pending", nlohmann::json::array({"needs_review"})},
        }},
    };
    const auto fillable_audit_missing =
        missing_required_generation_surface_requirements(*fillable_audit_surface);
    require(contains_string(fillable_audit_missing, "fillable_locations_pending_audit"),
            "fillable_locations_pending_audit_not_reported");
    require(contains_string(fillable_audit_missing, "risk_audit_not_consumed"),
            "risk_audit_not_consumed_not_reported");
    fillable_audit_surface->generation_rules["placement_rules"]["fillable_locations_source"]["validation"] = {
        {"status", "declared-consumed"},
        {"blocks_can_place_items_until_audited", true},
        {"authorizes", {{"native_placement", true}}},
    };
    fillable_audit_surface->generation_rules["placement_rules"]["fillable_locations_source"]["risk_audit"] = {
        {"status", "declared-consumed"},
        {"authorizes", {{"native_placement", true}}},
        {"location_tags_pending", nlohmann::json::array({"needs_review"})},
    };
    const auto fillable_audit_clear_missing =
        missing_required_generation_surface_requirements(*fillable_audit_surface);
    require(!contains_string(fillable_audit_clear_missing, "fillable_locations_pending_audit"),
            "fillable_locations_pending_audit_should_clear_when_consumed_authorized");
    require(!contains_string(fillable_audit_clear_missing, "risk_audit_not_consumed"),
            "risk_audit_not_consumed_should_clear_when_consumed_authorized");

    auto segment_unknown_fact_surface = *complete_surface;
    segment_unknown_fact_surface.generation_rules["logic_rules"]["location_rule_segmentation"] = {
        {"status", "declared-consumable"},
        {"authorizes", {{"native_location_reachability", true}}},
        {"segments", nlohmann::json::array({
            {
                {"id", "segment.smoke.unknown_fact"},
                {"location_ids", nlohmann::json::array({"1573188"})},
                {"requires", {{"op", "fact"}, {"id", "undeclared_segment_fact"}}},
            },
        })},
    };
    const auto segment_unknown_fact_missing =
        missing_required_generation_surface_requirements(segment_unknown_fact_surface);
    require(contains_string(segment_unknown_fact_missing, "location_rule_unknown_fact_name"),
            "location_rule_unknown_fact_name_not_reported");

    auto consumable_location_surface = *complete_surface;
    consumable_location_surface.generation_rules["logic_rules"]["starting_state"]["facts"].push_back(
        "refinement_fact");
    consumable_location_surface.generation_rules["logic_rules"]["starting_state"]["facts"].push_back(
        "segment_fact");
    consumable_location_surface.generation_rules["logic_rules"]["fact_names"].push_back("refinement_fact");
    consumable_location_surface.generation_rules["logic_rules"]["fact_names"].push_back("segment_fact");
    consumable_location_surface.generation_rules["logic_rules"]["region_graph"]["fact_names"].push_back(
        "refinement_fact");
    consumable_location_surface.generation_rules["logic_rules"]["region_graph"]["fact_names"].push_back(
        "segment_fact");
    consumable_location_surface.generation_rules["logic_rules"]["location_refinements"] = {
        {"status", "declared-consumable"},
        {"authorizes", {{"native_logic_solve", true}}},
        {"refinements", nlohmann::json::array({
            {{"location_id", "1573189"}, {"requires", {{"op", "fact"}, {"id", "refinement_fact"}}}},
        })},
    };
    consumable_location_surface.generation_rules["logic_rules"]["location_rule_segmentation"] = {
        {"status", "declared-consumable"},
        {"authorizes", {{"native_location_reachability", true}}},
        {"segments", nlohmann::json::array({
            {
                {"id", "segment.smoke.silver"},
                {"location_ids", nlohmann::json::array({"1573188"})},
                {"requires", {{"op", "fact"}, {"id", "segment_fact"}}},
            },
        })},
    };
    const auto consumable_location_missing =
        missing_required_generation_surface_requirements(consumable_location_surface);
    require(!contains_string(consumable_location_missing, "location_refinements_not_authorizing"),
            "authorized_location_refinements_should_not_block");
    require(!contains_string(consumable_location_missing, "location_rule_segmentation_not_authorizing"),
            "authorized_location_rule_segmentation_should_not_block");

    const auto proof_graph_root =
        make_linkedworld_fixture(root / "linkedworlds", "proof_graph_smoke", true);
    auto proof_graph_surface =
        load_linkedworld_generation_surface(proof_graph_root, &error);
    require(proof_graph_surface.has_value(), "proof_graph_load_failed:" + error);
    proof_graph_surface->generation_rules["logic_rules"]["fact_graph"] = {
        {"id", "proof_graph_smoke.fact_graph"},
    };
    proof_graph_surface->generation_rules["logic_rules"]["starting_state"]["facts"].push_back(
        "refinement_fact");
    proof_graph_surface->generation_rules["logic_rules"]["starting_state"]["facts"].push_back(
        "segment_fact");
    proof_graph_surface->generation_rules["logic_rules"]["fact_names"].push_back("refinement_fact");
    proof_graph_surface->generation_rules["logic_rules"]["fact_names"].push_back("segment_fact");
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["fact_names"].push_back(
        "refinement_fact");
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["fact_names"].push_back(
        "segment_fact");
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["status"] = "declared-consumed";
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["consumed"] = true;
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["authorizes"] = {
        {"native_logic_solve", false},
    };
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["edge_audit"] = {
        {"status", "declared-not-consumed"},
        {"authorizes", {{"native_logic_solve", false}}},
    };
    proof_graph_surface->generation_rules["placement_rules"]["status"] = "declared-consumed";
    proof_graph_surface->generation_rules["placement_rules"]["consumed"] = true;
    proof_graph_surface->generation_rules["placement_rules"]["authorizes_native_placement"] = false;
    proof_graph_surface->generation_rules["logic_rules"]["location_refinements"] = {
        {"status", "declared-consumed"},
        {"consumed", true},
        {"proof_required", true},
        {"proof", {
            {"status", "complete-consumed"},
            {"consumed", true},
            {"fact_graph_id", "proof_graph_smoke.fact_graph"},
            {"authorizes", {{"native_logic_solve", false}}},
            {"produced_facts", nlohmann::json::array({"refinement_fact"})},
            {"consumed_facts", nlohmann::json::array({"refinement_fact"})},
        }},
        {"refinements", nlohmann::json::array({
            {{"location_id", "1573189"}, {"requires", {{"op", "fact"}, {"id", "refinement_fact"}}}},
        })},
    };
    proof_graph_surface->generation_rules["logic_rules"]["location_rule_segmentation"] = {
        {"status", "declared-consumed"},
        {"consumed", true},
        {"proof_required", true},
        {"proof", {
            {"status", "complete-consumed"},
            {"consumed", true},
            {"fact_graph_id", "proof_graph_smoke.fact_graph"},
            {"authorizes", {{"native_location_reachability", false}}},
            {"produced_facts", nlohmann::json::array({"segment_fact"})},
            {"consumed_facts", nlohmann::json::array({"segment_fact"})},
            {"coverage", {
                {"expected_location_count", 1},
                {"covered_location_count", 1},
                {"uncovered_location_ids", nlohmann::json::array()},
            }},
        }},
        {"segments", nlohmann::json::array({
            {
                {"id", "segment.proof_graph_smoke.silver"},
                {"location_ids", nlohmann::json::array({"1573188"})},
                {"requires", {{"op", "fact"}, {"id", "segment_fact"}}},
            },
        })},
    };
    proof_graph_surface->generation_rules["logic_rules"]["native_probe_contract_index"] = {
        {"region_graph", {
            {"contracts", nlohmann::json::array({
              {
                {"id", "proof_graph_smoke.native.region"},
                {"surface_id", "region_graph"},
                {"facts", nlohmann::json::array({"has_silver_bow"})},
                {"expected_reachable_regions", nlohmann::json::array({"region.smoke.silver"})},
              },
            })},
        }},
        {"location_refinements", nlohmann::json::array({
            {
                {"id", "proof_graph_smoke.native.refinement"},
                {"surface_ref", "location_refinements"},
                {"facts", nlohmann::json::array({"has_bow", "refinement_fact"})},
                {"expected_reachable_locations", nlohmann::json::array({"1573189"})},
            },
        })},
        {"location_rule_segmentation", {
            {"id", "proof_graph_smoke.native.segmentation"},
            {"surface_id", "location_rule_segmentation"},
            {"facts", nlohmann::json::array({"segment_fact"})},
            {"requires", {{"op", "fact"}, {"id", "segment_fact"}}},
        }},
        {"placement", {
            {"contracts", nlohmann::json::array({
              {
                {"id", "proof_graph_smoke.native.placement"},
                {"surface_ref", "placement"},
                {"expected_placeable_items", nlohmann::json::array({"proof_graph_smoke.filler_rupee"})},
              },
            })},
        }},
        {"dungeon_key_policy", {
            {"contracts", nlohmann::json::array({
              {
                {"id", "proof_graph_smoke.native.key_policy.placeholder"},
                {"surface_id", "dungeon_key_policy"},
                {"expected_placeable_items", nlohmann::json::array({"proof_graph_smoke.filler_rupee"})},
              },
            })},
        }},
    };
    const auto proof_graph_authorization_blocked =
        missing_required_generation_surface_requirements(*proof_graph_surface);
    require(contains_string(proof_graph_authorization_blocked, "region_graph_not_authorizing"),
            "passing_region_graph_probe_without_logic_authorization_should_block");
    require(contains_string(proof_graph_authorization_blocked, "region_graph_edge_blockers_not_consumed"),
            "passing_region_graph_probe_without_edge_audit_authorization_should_block");
    require(contains_string(proof_graph_authorization_blocked, "location_refinements_not_authorizing"),
            "proof_backed_location_refinement_without_logic_authorization_should_block");
    require(contains_string(proof_graph_authorization_blocked, "location_rule_segmentation_not_authorizing"),
            "proof_backed_segmentation_without_logic_authorization_should_block");
    require(contains_string(proof_graph_authorization_blocked, "placement_rules_not_authorizing"),
            "passing_placement_probe_without_placement_authorization_should_block");
    require(!contains_string(proof_graph_authorization_blocked, "native_probe_contract_failed"),
            "authorization_blocked_surfaces_should_still_have_passing_native_probes");
    proof_graph_surface->generation_rules["logic_rules"]["region_graph"]["authorizes"] = {
        {"native_logic_solve", true},
    };
    proof_graph_surface->generation_rules["logic_rules"]["location_refinements"]["authorizes_native_logic_solve"] =
        true;
    proof_graph_surface->generation_rules["logic_rules"]["location_rule_segmentation"]
                       ["authorizes_native_location_reachability"] = true;
    proof_graph_surface->generation_rules["placement_rules"]["authorizes_native_placement"] = true;
    const auto proof_graph_missing =
        missing_required_generation_surface_requirements(*proof_graph_surface);
    require(!contains_string(proof_graph_missing, "region_graph_not_authorizing"),
            "passing_authorized_region_graph_probe_should_not_block");
    require(!contains_string(proof_graph_missing, "region_graph_edge_blockers_not_consumed"),
            "passing_authorized_region_graph_probe_should_clear_edge_audit_blocker");
    require(!contains_string(proof_graph_missing, "location_refinements_not_authorizing"),
            "proof_backed_location_refinements_should_not_block");
    require(!contains_string(proof_graph_missing, "location_rule_segmentation_not_authorizing"),
            "proof_backed_location_rule_segmentation_should_not_block");
    require(!contains_string(proof_graph_missing, "placement_rules_not_authorizing"),
            "passing_authorized_placement_probe_should_not_block");
    require(!contains_string(proof_graph_missing, "native_probe_contract_failed"),
            "passing_native_probe_contracts_should_not_block");
    auto proof_graph_failing_native_probe_surface = *proof_graph_surface;
    proof_graph_failing_native_probe_surface.generation_rules["logic_rules"]["native_probe_contract_index"]
                                            ["region_graph"]["contracts"].push_back({
        {"id", "proof_graph_smoke.native.fail"},
        {"surface_id", "region_graph"},
        {"expected_facts", nlohmann::json::array({"missing_native_probe_fact"})},
    });
    const auto proof_graph_failing_native_probe =
        missing_required_generation_surface_requirements(proof_graph_failing_native_probe_surface);
    require(contains_string(proof_graph_failing_native_probe, "native_probe_contract_failed"),
            "failing_native_probe_contract_should_block");
    auto proof_graph_missing_refinement_fact_surface = *proof_graph_surface;
    proof_graph_missing_refinement_fact_surface.generation_rules["logic_rules"]["location_refinements"]
                                                ["proof"]["produced_facts"] = nlohmann::json::array();
    proof_graph_missing_refinement_fact_surface.generation_rules["logic_rules"]["native_probe_contract_index"]
                                                ["location_refinements"] = nlohmann::json::array();
    const auto proof_graph_missing_refinement_fact =
        missing_required_generation_surface_requirements(proof_graph_missing_refinement_fact_surface);
    require(contains_string(proof_graph_missing_refinement_fact, "location_refinements_not_authorizing"),
            "location_refinement_missing_fact_without_native_probe_should_block");
    auto proof_graph_missing_segment_coverage_surface = *proof_graph_surface;
    proof_graph_missing_segment_coverage_surface.generation_rules["logic_rules"]["location_rule_segmentation"]
                                                ["proof"]["coverage"]["covered_location_count"] = 0;
    proof_graph_missing_segment_coverage_surface.generation_rules["logic_rules"]["native_probe_contract_index"]
                                                ["location_rule_segmentation"] = nlohmann::json::object();
    const auto proof_graph_missing_segment_coverage =
        missing_required_generation_surface_requirements(proof_graph_missing_segment_coverage_surface);
    require(contains_string(proof_graph_missing_segment_coverage, "location_rule_segmentation_not_authorizing"),
            "segmentation_missing_coverage_without_native_probe_should_block");
    auto proof_graph_missing_segment_fact_surface = *proof_graph_surface;
    proof_graph_missing_segment_fact_surface.generation_rules["logic_rules"]["location_rule_segmentation"]
                                          ["proof"]["consumed_facts"] = nlohmann::json::array();
    proof_graph_missing_segment_fact_surface.generation_rules["logic_rules"]["native_probe_contract_index"]
                                          ["location_rule_segmentation"] = nlohmann::json::object();
    const auto proof_graph_missing_segment_fact =
        missing_required_generation_surface_requirements(proof_graph_missing_segment_fact_surface);
    require(contains_string(proof_graph_missing_segment_fact, "location_rule_segmentation_not_authorizing"),
            "segmentation_missing_fact_without_native_probe_should_block");
    write_json(proof_graph_root / "generation" / "logic.complete.json",
               proof_graph_surface->generation_rules["logic_rules"]);
    GenerationPackageRequest proof_graph_request;
    proof_graph_request.job_id = "job-proof-graph";
    proof_graph_request.room_id = "room-proof-graph";
    proof_graph_request.seed_id = "seed-proof-graph";
    proof_graph_request.rng_seed = "deterministic-proof-graph-seed";
    proof_graph_request.output_root = root / "packages";
    proof_graph_request.slots.push_back({
        .slot_id = 1,
        .user_id = 4001,
        .display_name = "Proof Graph Smoke",
        .game_key = "proof_graph_smoke",
        .linkedworld_id = "linkedworld.proof_graph_smoke",
        .config_version_id = 801,
    });
    const auto proof_graph_package =
        generate_seed_package_from_linkedworlds(proof_graph_request, {*proof_graph_surface});
    require(proof_graph_package.ok,
            "proof_graph_package_generation_failed:" + proof_graph_package.error);
    const auto proof_graph_checks =
        read_json(proof_graph_package.package_dir / "checks.json");
    require(proof_graph_checks.dump().find("refinement_fact") != std::string::npos,
            "proof_backed_location_refinement_not_consumed");
    require(proof_graph_checks.dump().find("segment_fact") != std::string::npos,
            "proof_backed_location_segmentation_not_consumed");

    const auto native_fact_package_root =
        make_linkedworld_fixture(root / "linkedworlds", "native_fact_package_smoke", true);
    auto native_fact_package_surface =
        load_linkedworld_generation_surface(native_fact_package_root, &error);
    require(native_fact_package_surface.has_value(), "native_fact_package_load_failed:" + error);
    native_fact_package_surface->catalog["item_pool"] = nlohmann::json::array({
        {
            {"id", "native_fact_package_smoke.filler_rupee"},
            {"name", "Native Fact Filler"},
            {"classification", "filler"},
            {"advancement", false},
            {"tags", nlohmann::json::array({"filler"})},
        },
    });
    native_fact_package_surface->generation_rules["placement_rules"]["fillable_locations"] =
        nlohmann::json::array({"1573188"});
    native_fact_package_surface->generation_rules["placement_rules"]["reserved_locations"] =
        nlohmann::json::array();
    native_fact_package_surface->generation_rules["placement_rules"]["preplacements"] =
        nlohmann::json::array();
    native_fact_package_surface->generation_rules["placement_rules"]["location_constraints"] =
        nlohmann::json::array({
            {{"id", "1573188"}, {"allow_item_tags", nlohmann::json::array({"filler"})}},
        });
    native_fact_package_surface->generation_rules["placement_rules"]["authorizes_native_placement"] = true;
    native_fact_package_surface->generation_rules["placement_rules"]["status"] = "declared-consumed";
    native_fact_package_surface->generation_rules["placement_rules"]["consumed"] = true;
    native_fact_package_surface->generation_rules["logic_rules"]["fact_names"].push_back(
        "native_probe_unlock_fact");
    native_fact_package_surface->generation_rules["logic_rules"]["region_graph"]["fact_names"].push_back(
        "native_probe_unlock_fact");
    native_fact_package_surface->generation_rules["logic_rules"]["region_graph"]["status"] =
        "declared-consumed";
    native_fact_package_surface->generation_rules["logic_rules"]["region_graph"]["consumed"] = true;
    native_fact_package_surface->generation_rules["logic_rules"]["region_graph"]
                               ["authorizes_native_logic_solve"] = true;
    native_fact_package_surface->generation_rules["logic_rules"]["region_graph"]["edges"][0]["requires"] = {
        {"op", "fact"},
        {"id", "native_probe_unlock_fact"},
    };
    native_fact_package_surface->generation_rules["logic_rules"]["native_probe_contract_index"] = {
        {"region_graph", {
            {"contracts", nlohmann::json::array({
                {
                    {"id", "native_fact_package_smoke.native.region_fact"},
                    {"surface_id", "region_graph"},
                    {"authorizes_native_logic_solve", true},
                    {"consumed", true},
                    {"facts", nlohmann::json::array({"native_probe_unlock_fact"})},
                    {"expected_reachable_regions", nlohmann::json::array({"region.smoke.silver"})},
                },
            })},
        }},
    };
    const auto native_fact_package_missing =
        missing_required_generation_surface_requirements(*native_fact_package_surface);
    require(native_fact_package_missing.empty(),
            "native_fact_package_surface_should_be_ready:" + nlohmann::json(native_fact_package_missing).dump());
    GenerationPackageRequest native_fact_package_request;
    native_fact_package_request.job_id = "job-native-fact-package";
    native_fact_package_request.room_id = "room-native-fact-package";
    native_fact_package_request.seed_id = "seed-native-fact-package";
    native_fact_package_request.rng_seed = "deterministic-native-fact-package-seed";
    native_fact_package_request.output_root = root / "packages";
    native_fact_package_request.slots.push_back({
        .slot_id = 1,
        .user_id = 4002,
        .display_name = "Native Fact Package",
        .game_key = "native_fact_package_smoke",
        .linkedworld_id = "linkedworld.native_fact_package_smoke",
        .config_version_id = 802,
    });
    const auto native_fact_package =
        generate_seed_package_from_linkedworlds(native_fact_package_request, {*native_fact_package_surface});
    require(native_fact_package.ok,
            "native_fact_package_generation_failed:" + native_fact_package.error);
    const auto native_fact_checks =
        read_json(native_fact_package.package_dir / "checks.json");
    require(native_fact_checks.dump().find("native_probe_unlock_fact") != std::string::npos,
            "native_probe_contract_fact_not_materialized_for_package_solver");

    auto wrapped_rules_surface = *complete_surface;
    const auto wrapped_direct_effects = wrapped_rules_surface.generation_rules["logic_rules"]["item_effects"];
    const auto wrapped_direct_locations = wrapped_rules_surface.generation_rules["logic_rules"]["locations"];
    wrapped_rules_surface.generation_rules["logic_rules"]["item_effects"] = {
        {"status", "declared-partial-consumable-not-authorizing"},
        {"effects", wrapped_direct_effects},
    };
    wrapped_rules_surface.generation_rules["logic_rules"]["locations"] = {
        {"status", "declared-partial-consumable-not-authorizing"},
        {"rules", wrapped_direct_locations},
    };
    GenerationPackageRequest wrapped_request;
    wrapped_request.job_id = "job-wrapped";
    wrapped_request.room_id = "room-wrapped";
    wrapped_request.seed_id = "seed-wrapped";
    wrapped_request.rng_seed = "deterministic-wrapped-seed";
    wrapped_request.output_root = root / "packages";
    wrapped_request.slots.push_back({
        .slot_id = 1,
        .user_id = 1001,
        .display_name = "Jade",
        .game_key = "alttp",
        .linkedworld_id = "linkedworld.alttp",
        .config_version_id = 501,
    });
    const auto invalid_region_graph_package =
        generate_seed_package_from_linkedworlds(wrapped_request, {invalid_region_graph_surface});
    require(!invalid_region_graph_package.ok, "invalid_region_graph_package_should_fail");
    require(invalid_region_graph_package.error.find("missing_generation_surface_requirement:linkedworld.alttp") == 0,
            "invalid_region_graph_package_error_prefix_mismatch:" + invalid_region_graph_package.error);
    require(invalid_region_graph_package.error.find("region_graph_unknown_edge_region") != std::string::npos,
            "invalid_region_graph_package_error_missing_region_reason:" + invalid_region_graph_package.error);
    const auto wrapped_package = generate_seed_package_from_linkedworlds(wrapped_request, {wrapped_rules_surface});
    require(wrapped_package.ok, "wrapped_rules_package_generation_failed:" + wrapped_package.error);
    const auto wrapped_placements = read_json(wrapped_package.package_dir / "placements.json");
    require(wrapped_placements.size() == 3, "wrapped_rules_placement_count_mismatch");
    require(std::any_of(wrapped_placements.begin(), wrapped_placements.end(), [](const auto& placement) {
              return placement.value("location_id", std::string{}) == "1573188";
            }),
            "wrapped_rules_location_rules_not_consumed");
    require(std::any_of(wrapped_placements.begin(), wrapped_placements.end(), [](const auto& placement) {
              const auto grants = placement.value("grants", nlohmann::json::array());
              return grants.is_array() && std::find(grants.begin(), grants.end(), "has_silver_bow") != grants.end();
            }),
            "wrapped_rules_item_effects_not_consumed");

    const auto consumable_location_package =
        generate_seed_package_from_linkedworlds(wrapped_request, {consumable_location_surface});
    require(consumable_location_package.ok,
            "consumable_location_package_generation_failed:" + consumable_location_package.error);
    const auto consumable_location_checks =
        read_json(consumable_location_package.package_dir / "checks.json");
    require(std::any_of(consumable_location_checks.begin(),
                        consumable_location_checks.end(),
                        [](const auto& check) {
                          return check.value("id", std::string{}) == "1573189" &&
                                 check.value("requires", nlohmann::json::object()).dump().find(
                                     "refinement_fact") != std::string::npos;
                        }),
            "authorized_location_refinement_not_consumed");
    require(std::any_of(consumable_location_checks.begin(),
                        consumable_location_checks.end(),
                        [](const auto& check) {
                          return check.value("id", std::string{}) == "1573188" &&
                                 check.value("requires", nlohmann::json::object()).dump().find(
                                     "segment_fact") != std::string::npos;
                        }),
            "authorized_segmented_location_rule_not_consumed");

    auto location_id_rule_surface = *complete_surface;
    location_id_rule_surface.generation_rules["logic_rules"]["locations"][1].erase("id");
    location_id_rule_surface.generation_rules["logic_rules"]["locations"][1]["location_id"] = "1573189";
    const auto location_id_rule_package =
        generate_seed_package_from_linkedworlds(wrapped_request, {location_id_rule_surface});
    require(location_id_rule_package.ok,
            "location_id_rule_package_generation_failed:" + location_id_rule_package.error);
    const auto location_id_rule_checks =
        read_json(location_id_rule_package.package_dir / "checks.json");
    require(std::any_of(location_id_rule_checks.begin(),
                        location_id_rule_checks.end(),
                        [](const auto& check) {
                          return check.value("id", std::string{}) == "1573189" &&
                                 check.value("requires", nlohmann::json::object()).dump().find(
                                     "has_bow") != std::string::npos;
                        }),
            "location_id_rule_not_matched");

    auto stage_grants_surface = *complete_surface;
    stage_grants_surface.generation_rules["logic_rules"]["item_effects"]["alttp.progressive_bow"] = {
        {"status", "declared-consumable-stage-grants"},
        {"stage_grants", {
            {"mode", "incremental-progressive"},
            {"stages", nlohmann::json::array({
                {{"stage", 1}, {"grants", nlohmann::json::array({"has_bow"})}},
                {{"stage", 2}, {"grants", nlohmann::json::array({"has_silver_bow"})}},
            })},
        }},
    };
    const auto stage_grants_package = generate_seed_package_from_linkedworlds(wrapped_request, {stage_grants_surface});
    require(stage_grants_package.ok, "stage_grants_package_generation_failed:" + stage_grants_package.error);
    const auto stage_grants_placements = read_json(stage_grants_package.package_dir / "placements.json");
    require(std::any_of(stage_grants_placements.begin(), stage_grants_placements.end(), [](const auto& placement) {
              const auto grants = placement.value("grants", nlohmann::json::array());
              return grants.is_array() && std::find(grants.begin(), grants.end(), "has_silver_bow") != grants.end();
            }),
            "stage_grants_item_effects_not_consumed");

    auto count_grants_surface = *complete_surface;
    count_grants_surface.catalog["locations"].push_back(
        {{"id", "1573187"}, {"name", "Open Count Smoke Location"}});
    count_grants_surface.catalog["locations"].push_back(
        {{"id", "1573186"}, {"name", "Count Locked Smoke Location"}});
    count_grants_surface.catalog["item_pool"].push_back({
        {"id", "alttp.filler_rupee#2"},
        {"name", "Filler Rupee"},
        {"classification", "filler"},
        {"advancement", false},
        {"tags", nlohmann::json::array({"filler"})},
    });
    count_grants_surface.catalog["item_pool"].push_back({
        {"id", "alttp.filler_rupee#3"},
        {"name", "Filler Rupee"},
        {"classification", "filler"},
        {"advancement", false},
        {"tags", nlohmann::json::array({"filler"})},
    });
    count_grants_surface.generation_rules["placement_rules"]["fillable_locations"].push_back("1573187");
    count_grants_surface.generation_rules["placement_rules"]["fillable_locations"].push_back("1573186");
    count_grants_surface.generation_rules["placement_rules"]["location_constraints"].push_back(
        {{"id", "1573187"}, {"allow_item_tags", nlohmann::json::array({"filler"})}});
    count_grants_surface.generation_rules["placement_rules"]["location_constraints"].push_back(
        {{"id", "1573186"}, {"allow_item_tags", nlohmann::json::array({"filler"})}});
    count_grants_surface.generation_rules["logic_rules"]["locations"].push_back(
        {{"id", "1573187"}, {"requires", {{"op", "true"}}}});
    count_grants_surface.generation_rules["logic_rules"]["locations"].push_back(
        {{"id", "1573186"}, {"requires", {{"op", "fact"}, {"id", "has_two_rupees"}}}});
    count_grants_surface.generation_rules["logic_rules"]["item_effects"]["alttp.filler_rupee"] = {
        {"status", "declared-consumable-count-grants"},
        {"grants", nlohmann::json::array({"has_rupee"})},
        {"count_grants", {
            {"mode", "incremental-count"},
            {"thresholds", nlohmann::json::array({
                {{"count", 1}, {"grants", nlohmann::json::array({"rupee_count_1"})}},
                {{"count", 2}, {"grants", nlohmann::json::array({"has_two_rupees"})}},
            })},
        }},
    };
    const auto count_grants_package = generate_seed_package_from_linkedworlds(wrapped_request, {count_grants_surface});
    require(count_grants_package.ok, "count_grants_package_generation_failed:" + count_grants_package.error);
    const auto count_grants_placements = read_json(count_grants_package.package_dir / "placements.json");
    require(count_grants_placements.size() == 5, "count_grants_placement_count_mismatch");
    require(std::any_of(count_grants_placements.begin(), count_grants_placements.end(), [](const auto& placement) {
              return placement.value("location_id", std::string{}) == "1573186";
            }),
            "count_grants_locked_location_not_reached");
    require(std::any_of(count_grants_placements.begin(), count_grants_placements.end(), [](const auto& placement) {
              const auto grants = placement.value("grants", nlohmann::json::array());
              return grants.is_array() && std::find(grants.begin(), grants.end(), "has_two_rupees") != grants.end();
            }),
            "count_grants_threshold_not_consumed");

    auto matching_guard_surface = *complete_surface;
    matching_guard_surface.catalog["locations"] = nlohmann::json::array({
        {{"id", "scarce_filler_open"}, {"name", "Scarce Filler Open"}},
        {{"id", "bow_open"}, {"name", "Bow Open"}},
        {{"id", "bow_locked"}, {"name", "Bow Locked"}},
    });
    matching_guard_surface.catalog["item_pool"] = nlohmann::json::array({
        {
            {"id", "alttp.progressive_bow#1"},
            {"name", "Progressive Bow"},
            {"classification", "progression"},
            {"advancement", true},
            {"tags", nlohmann::json::array({"progression"})},
        },
        {
            {"id", "alttp.progressive_bow#2"},
            {"name", "Progressive Bow"},
            {"classification", "progression"},
            {"advancement", true},
            {"tags", nlohmann::json::array({"progression"})},
        },
        {
            {"id", "alttp.filler_rupee"},
            {"name", "Filler Rupee"},
            {"classification", "filler"},
            {"advancement", false},
            {"tags", nlohmann::json::array({"filler"})},
        },
    });
    matching_guard_surface.generation_rules["logic_rules"] = {
        {"schema_version", "sekailink-logic-rules-v1"},
        {"rule_language", "sekailink-expr-v1"},
        {"starting_state", {{"facts", nlohmann::json::array({"start"})}}},
        {"item_effects", {
            {"alttp.progressive_bow", {
                {"type", "progressive"},
                {"stages", nlohmann::json::array({
                    {{"grants", nlohmann::json::array({"has_bow"})}},
                    {{"grants", nlohmann::json::array({"has_silver_bow"})}},
                })},
            }},
        }},
        {"regions", nlohmann::json::array({"region.start", "region.bow_locked"})},
        {"starting_regions", nlohmann::json::array({"region.start"})},
        {"edges", nlohmann::json::array({
            {
                {"from_region", "region.start"},
                {"to_region", "region.bow_locked"},
                {"requires", {{"op", "fact"}, {"id", "has_bow"}}},
            },
        })},
        {"location_region_bindings", nlohmann::json::array({
            {{"location_id", "scarce_filler_open"}, {"region", "region.start"}},
            {{"location_id", "bow_open"}, {"region", "region.start"}},
            {{"location_id", "bow_locked"}, {"region", "region.bow_locked"}},
        })},
        {"fact_names", nlohmann::json::array({"start", "has_bow", "has_silver_bow"})},
        {"locations", nlohmann::json::array({
            {{"id", "scarce_filler_open"}, {"requires", {{"op", "true"}}}},
            {{"id", "bow_open"}, {"requires", {{"op", "true"}}}},
            {{"id", "bow_locked"}, {"requires", {{"op", "true"}}}},
        })},
    };
    matching_guard_surface.generation_rules["placement_rules"] = {
        {"schema_version", "sekailink-placement-rules-v1"},
        {"option_profile", "matching-guard-smoke"},
        {"fillable_locations", nlohmann::json::array({"scarce_filler_open", "bow_open", "bow_locked"})},
        {"location_constraints", nlohmann::json::array({
            {{"id", "scarce_filler_open"}, {"allow_item_tags", nlohmann::json::array({"filler", "progression"})}},
            {{"id", "bow_open"}, {"allow_item_tags", nlohmann::json::array({"progression"})}},
            {{"id", "bow_locked"}, {"allow_item_tags", nlohmann::json::array({"progression"})}},
        })},
    };
    GenerationPackageRequest matching_guard_request;
    matching_guard_request.job_id = "job-matching-guard";
    matching_guard_request.room_id = "room-matching-guard";
    matching_guard_request.seed_id = "seed-constrained";
    matching_guard_request.rng_seed = "deterministic-constrained-seed";
    matching_guard_request.output_root = root / "packages";
    matching_guard_request.slots.push_back({
        .slot_id = 1,
        .user_id = 1001,
        .display_name = "Jade",
        .game_key = "alttp",
        .linkedworld_id = "linkedworld.alttp",
        .config_version_id = 501,
    });
    const auto matching_guard_package =
        generate_seed_package_from_linkedworlds(matching_guard_request, {matching_guard_surface});
    require(matching_guard_package.ok,
            "matching_guard_package_generation_failed:" + matching_guard_package.error);
    const auto matching_guard_placements =
        read_json(matching_guard_package.package_dir / "placements.json");
    require(std::any_of(matching_guard_placements.begin(),
                        matching_guard_placements.end(),
                        [](const auto& placement) {
                          return placement.value("location_id", std::string{}) == "scarce_filler_open" &&
                                 placement.value("item_id", std::string{}) == "alttp.filler_rupee";
                        }),
            "matching_guard_static_placer_consumed_unique_filler_location");
    require(std::any_of(matching_guard_placements.begin(),
                        matching_guard_placements.end(),
                        [](const auto& placement) {
                          return placement.value("location_id", std::string{}) == "bow_locked";
                        }),
            "matching_guard_region_locked_location_not_reached");

    GenerationPackageRequest request;
    request.job_id = "job-smoke";
    request.room_id = "room-smoke";
    request.seed_id = "seed-smoke";
    request.rng_seed = "deterministic-smoke-seed";
    request.output_root = root / "packages";
    request.slots.push_back({
        .slot_id = 1,
        .user_id = 1001,
        .display_name = "Jade",
        .game_key = "alttp",
        .linkedworld_id = "linkedworld.alttp",
        .config_version_id = 501,
    });
    request.slots.push_back({
        .slot_id = 2,
        .user_id = 1002,
        .display_name = "Ness",
        .game_key = "earthbound",
        .linkedworld_id = "linkedworld.earthbound",
        .config_version_id = 777,
    });

    const auto complete_earthbound_root = make_linkedworld_fixture(root / "linkedworlds_complete", "earthbound", true);
    const auto complete_earthbound_surface =
        load_linkedworld_generation_surface(complete_earthbound_root, &error);
    require(complete_earthbound_surface.has_value(), "complete_earthbound_linkedworld_load_failed:" + error);

    const auto package = generate_seed_package_from_linkedworlds(request, {*complete_surface, *complete_earthbound_surface});
    require(package.ok, "complete_package_generation_failed:" + package.error);
    require(std::filesystem::exists(package.package_dir / "seed_manifest.json"), "missing_seed_manifest");
    require(std::filesystem::exists(package.package_dir / "room_manifest.json"), "missing_room_manifest");
    require(std::filesystem::exists(package.package_dir / "slot_manifest.1.json"), "missing_slot_manifest");
    require(std::filesystem::exists(package.package_dir / "slot_manifest.2.json"), "missing_second_slot_manifest");
    require(std::filesystem::exists(package.package_dir / "checks.json"), "missing_checks");
    require(std::filesystem::exists(package.package_dir / "items.json"), "missing_items");
    require(std::filesystem::exists(package.package_dir / "placements.json"), "missing_placements");
    require(package.manifest.at("generation_scope") == "multiworld", "generation_scope_mismatch");
    require(package.manifest.at("slot_count") == 2, "seed_slot_count_mismatch");
    require(package.manifest.at("linkedworld_count") == 2, "seed_linkedworld_count_mismatch");
    require(package.manifest.at("players").size() == 2, "player_count_mismatch");
    require(package.manifest.at("config_versions").size() == 2, "config_version_count_mismatch");
    require(package.manifest.at("linkedworlds").size() == 2, "linkedworld_count_mismatch");
    require(has_config_version(package.manifest.at("config_versions"), 1, 501, "linkedworld.alttp"),
            "missing_alttp_config_version");
    require(has_config_version(package.manifest.at("config_versions"), 2, 777, "linkedworld.earthbound"),
            "missing_earthbound_config_version");
    require(package.manifest.at("linkedworlds").at(0).value("patch_mode", std::string{}) == "contract_only",
            "seed_linkedworld_patch_mode_mismatch");

    const auto room_manifest = read_json(package.package_dir / "room_manifest.json");
    const auto slot_manifest_1 = read_json(package.package_dir / "slot_manifest.1.json");
    const auto patch_contract_1 =
        read_json(package.package_dir / slot_manifest_1.value("patch_contract_ref", std::string{}));
    const auto checks = read_json(package.package_dir / "checks.json");
    const auto items = read_json(package.package_dir / "items.json");
    const auto placements = read_json(package.package_dir / "placements.json");
    const auto link_room_contract = read_json(package.package_dir / "link_room_seed_contract.json");
    const auto audit = read_json(package.package_dir / "audit.json");
    require(room_manifest.at("generation_scope") == "multiworld", "room_generation_scope_mismatch");
    require(room_manifest.at("slot_count") == 2, "room_slot_count_mismatch");
    require(room_manifest.at("linkedworld_count") == 2, "room_linkedworld_count_mismatch");
    require(room_manifest.at("location_count") == checks.size(), "room_location_count_mismatch");
    require(room_manifest.at("item_count") == items.size(), "room_item_count_mismatch");
    require(slot_manifest_1.value("patch_mode", std::string{}) == "contract_only",
            "slot_patch_mode_mismatch");
    require(slot_manifest_1.at("patch_artifact").is_null(), "contract_only_slot_patch_artifact_should_be_null");
    require(slot_manifest_1.value("patch_contract_ref", std::string{}) ==
                "patch_contracts/slot-1.patch.contract.json",
            "slot_patch_contract_ref_mismatch");
    require(patch_contract_1.value("patch_mode", std::string{}) == "contract_only",
            "patch_contract_mode_mismatch");
    require(patch_contract_1.at("artifact").is_null(), "contract_only_patch_artifact_should_be_null");
    require(patch_contract_1.value("patch_manifest_ref", std::string{}) == "patch/patch.manifest.json",
            "patch_contract_manifest_ref_mismatch");
    require(patch_contract_1.at("patch_manifest").at("patch").value("schema_version", std::string{}) ==
                "sekailink-patch-contract-v1",
            "patch_contract_manifest_schema_mismatch");
    require(package.manifest.at("artifact_hashes").contains("slot_manifest.1.json"),
            "slot_manifest_hash_missing");
    require(package.manifest.at("artifact_hashes").contains(slot_manifest_1.value("patch_contract_ref", std::string{})),
            "patch_contract_hash_missing");
    require(package.manifest.at("artifact_hashes").contains(slot_manifest_1.value("tracker_seed_state", std::string{})),
            "tracker_seed_state_hash_missing");
    require(package.manifest.at("artifact_hashes").contains(slot_manifest_1.value("sklmi_contract_ref", std::string{})),
            "sklmi_contract_hash_missing");
    require(package.manifest.at("artifact_hashes").contains(slot_manifest_1.value("link_room_contract_ref", std::string{})),
            "link_room_contract_hash_missing");
    require(checks.size() == 6, "check_count_mismatch");
    require(items.size() == 6, "item_count_mismatch");
    require(count_with_slot(checks, 1) == 3, "slot_1_check_count_mismatch");
    require(count_with_slot(checks, 2) == 3, "slot_2_check_count_mismatch");
    require(count_with_slot(items, 1) == 3, "slot_1_item_count_mismatch");
    require(count_with_slot(items, 2) == 3, "slot_2_item_count_mismatch");
    require(placements.size() == 6, "placement_count_mismatch");
    std::set<int> seen_slots;
    bool saw_progression_sphere = false;
    bool saw_locked_location = false;
    bool saw_silver_locked_location = false;
    bool saw_second_progressive_grant = false;
    bool saw_preplacement = false;
    bool saw_reserved_location = false;
    for (const auto& placement : placements) {
      require(placement.contains("location_id"), "placement_missing_location_id");
      require(placement.contains("location_name"), "placement_missing_location_name");
      require(placement.contains("owning_slot"), "placement_missing_owning_slot");
      require(placement.contains("receiving_slot"), "placement_missing_receiving_slot");
      require(placement.contains("item_id"), "placement_missing_item_id");
      require(placement.contains("item_name"), "placement_missing_item_name");
      require(placement.contains("classification"), "placement_missing_classification");
      require(placement.contains("source_linkedworld_ref"), "placement_missing_source_linkedworld_ref");
      require(placement.contains("item_source_linkedworld_ref"), "placement_missing_item_source_linkedworld_ref");
      seen_slots.insert(placement.at("owning_slot").get<int>());
      seen_slots.insert(placement.at("receiving_slot").get<int>());
      if (placement.value("advancement", false)) {
        require(!placement.at("progression_sphere").is_null(), "advancement_missing_progression_sphere");
        saw_progression_sphere = true;
      }
      if (placement.value("location_id", std::string{}) == "1573189") {
        saw_locked_location = true;
      }
      if (placement.value("location_id", std::string{}) == "1573188") {
        saw_silver_locked_location = true;
      }
      if (placement.value("grants", nlohmann::json::array()).is_array() &&
          std::find(placement.at("grants").begin(), placement.at("grants").end(), "has_silver_bow") !=
              placement.at("grants").end()) {
        saw_second_progressive_grant = true;
      }
      if (placement.value("location_id", std::string{}) == "1573190") {
        saw_reserved_location = true;
      }
      if (placement.value("preplaced", false)) {
        saw_preplacement = true;
      }
    }
    require(seen_slots == std::set<int>({1, 2}), "placement_slot_domain_mismatch");
    require(saw_progression_sphere, "expected_progression_sphere");
    require(saw_locked_location, "expected_locked_location_placement");
    require(saw_silver_locked_location, "expected_silver_locked_location_placement");
    require(saw_second_progressive_grant, "expected_second_progressive_grant");
    require(saw_preplacement, "expected_preplacement");
    require(!saw_reserved_location, "reserved_location_should_not_be_placed");
    require_multiworld_contract_refs(link_room_contract);
    require(audit.at("solver_mode") == "generic-fact-sphere-v1", "audit_solver_mode_mismatch");
    require(audit.at("placement_algorithm") == "deterministic-constraint-sphere-v1",
            "audit_placement_algorithm_mismatch");
    require(audit.at("source_location_count") == 8, "audit_source_location_count_mismatch");
    require(audit.at("filtered_location_count") == checks.size(), "audit_filtered_location_count_mismatch");
    require(audit.at("item_count") == items.size(), "audit_item_count_mismatch");
    require(audit.at("placement_count") == placements.size(), "audit_placement_count_mismatch");
    require(audit.at("preplacement_count") == 2, "audit_preplacement_count_mismatch");
    const auto replay_validation = audit.at("replay_validation").get<std::string>();
    require(replay_validation == "full-logic-replay-passed" ||
                replay_validation == "structure-and-placement-constraints-passed-large-package",
            "audit_replay_validation_mismatch");
    require(!package.package_hash.empty(), "missing_package_hash");

    GenerationPackageRequest patch_mode_request = request;
    patch_mode_request.slots.resize(1);
    patch_mode_request.slots.front().game_key = "alttp";
    patch_mode_request.slots.front().linkedworld_id = "linkedworld.alttp";
    patch_mode_request.slots.front().config_version_id = 501;

    auto artifact_mode_surface = *complete_surface;
    artifact_mode_surface.patch["mode"] = "artifact";
    artifact_mode_surface.patch["patch_file_extension"] = ".aplttp";
    artifact_mode_surface.patch["manifest"]["patch"]["mode"] = "artifact";
    artifact_mode_surface.patch["manifest"]["patch"]["artifact_kind"] = "apdelta";
    artifact_mode_surface.patch["manifest"]["patch"]["artifact_extension"] = ".aplttp";
    artifact_mode_surface.patch["manifest"]["patch"]["emission"]["package_filename_template"] =
        "slot-{slot_id}.aplttp";
    patch_mode_request.seed_id = "seed-artifact-mode";
    const auto artifact_mode_package =
        generate_seed_package_from_linkedworlds(patch_mode_request, {artifact_mode_surface});
    require(artifact_mode_package.ok, "artifact_mode_package_generation_failed:" + artifact_mode_package.error);
    const auto artifact_slot_manifest =
        read_json(artifact_mode_package.package_dir / "slot_manifest.1.json");
    const auto artifact_patch_contract =
        read_json(artifact_mode_package.package_dir /
                  artifact_slot_manifest.value("patch_contract_ref", std::string{}));
    require(artifact_slot_manifest.value("patch_mode", std::string{}) == "artifact",
            "artifact_slot_mode_mismatch");
    require(artifact_slot_manifest.value("patch_artifact", std::string{}) == "patches/slot-1.aplttp",
            "artifact_slot_artifact_mismatch");
    require(artifact_patch_contract.at("artifact").value("path", std::string{}) == "patches/slot-1.aplttp",
            "artifact_contract_path_mismatch");
    require(artifact_patch_contract.at("artifact").value("state", std::string{}) == "placeholder",
            "artifact_contract_placeholder_missing");
    require(!artifact_mode_package.manifest.at("artifact_hashes").contains("patches/slot-1.aplttp"),
            "placeholder_artifact_should_not_be_hashed_as_json");

    auto none_mode_surface = *complete_surface;
    none_mode_surface.patch["mode"] = "none";
    none_mode_surface.patch["manifest"]["patch"]["mode"] = "none";
    patch_mode_request.seed_id = "seed-none-mode";
    const auto none_mode_package =
        generate_seed_package_from_linkedworlds(patch_mode_request, {none_mode_surface});
    require(none_mode_package.ok, "none_mode_package_generation_failed:" + none_mode_package.error);
    const auto none_slot_manifest =
        read_json(none_mode_package.package_dir / "slot_manifest.1.json");
    const auto none_patch_contract =
        read_json(none_mode_package.package_dir /
                  none_slot_manifest.value("patch_contract_ref", std::string{}));
    require(none_slot_manifest.at("patch_artifact").is_null(), "none_mode_slot_artifact_should_be_null");
    require(none_patch_contract.at("artifact").is_null(), "none_mode_contract_artifact_should_be_null");

    auto server_dispatch_surface = *complete_surface;
    server_dispatch_surface.patch["mode"] = "server_dispatch";
    server_dispatch_surface.patch["manifest"]["patch"]["mode"] = "server_dispatch";
    server_dispatch_surface.patch["manifest"]["patch"]["server_dispatch"] = {
        {"enabled", true},
        {"target", "sekailink.runtime.dispatch"},
        {"transport", "room_contract"},
        {"payload_ref", "link_room_seed_contract.json"},
    };
    patch_mode_request.seed_id = "seed-server-dispatch-mode";
    const auto server_dispatch_package =
        generate_seed_package_from_linkedworlds(patch_mode_request, {server_dispatch_surface});
    require(server_dispatch_package.ok,
            "server_dispatch_package_generation_failed:" + server_dispatch_package.error);
    const auto server_dispatch_slot_manifest =
        read_json(server_dispatch_package.package_dir / "slot_manifest.1.json");
    const auto server_dispatch_patch_contract =
        read_json(server_dispatch_package.package_dir /
                  server_dispatch_slot_manifest.value("patch_contract_ref", std::string{}));
    require(server_dispatch_slot_manifest.at("patch_artifact").is_null(),
            "server_dispatch_slot_artifact_should_be_null");
    require(server_dispatch_patch_contract.at("artifact").is_null(),
            "server_dispatch_contract_artifact_should_be_null");
    require(server_dispatch_patch_contract.at("server_dispatch").value("enabled", false),
            "server_dispatch_contract_incomplete");

    const auto impossible_root = make_linkedworld_fixture(root / "linkedworlds", "impossible_logic", true);
    auto impossible_surface = load_linkedworld_generation_surface(impossible_root, &error);
    require(impossible_surface.has_value(), "impossible_logic_linkedworld_load_failed:" + error);
    impossible_surface->generation_rules["logic_rules"]["locations"] = nlohmann::json::array({
        {{"id", "1573194"}, {"requires", {{"op", "fact"}, {"id", "never_granted"}}}},
        {{"id", "1573189"}, {"requires", {{"op", "fact"}, {"id", "never_granted"}}}},
    });
    GenerationPackageRequest impossible_request;
    impossible_request.job_id = "job-impossible";
    impossible_request.room_id = "room-impossible";
    impossible_request.seed_id = "seed-impossible";
    impossible_request.rng_seed = "deterministic-impossible-seed";
    impossible_request.output_root = root / "packages";
    impossible_request.slots.push_back({
        .slot_id = 1,
        .user_id = 1001,
        .display_name = "Jade",
        .game_key = "impossible_logic",
        .linkedworld_id = "linkedworld.impossible_logic",
        .config_version_id = 999,
    });
    const auto impossible = generate_seed_package_from_linkedworlds(impossible_request, {*impossible_surface});
    require(!impossible.ok, "impossible_logic_should_not_generate");
    require(impossible.error.find("unsolved_logic_or_placement_constraints:") == 0,
            "impossible_logic_error_mismatch:" + impossible.error);
    require(impossible.error.find("blocked_locations") != std::string::npos,
            "impossible_logic_missing_blocked_location_diagnostics:" + impossible.error);
    require(impossible.error.find("unplaceable_items") != std::string::npos,
            "impossible_logic_missing_item_diagnostics:" + impossible.error);
    require(impossible.error.find("never_granted") != std::string::npos,
            "impossible_logic_missing_requires_diagnostics:" + impossible.error);

    GenerationPackageRequest blocked_request = request;
    blocked_request.seed_id = "seed-blocked";
    blocked_request.slots.resize(1);
    blocked_request.slots.front().game_key = "earthbound";
    blocked_request.slots.front().linkedworld_id = "linkedworld.earthbound";
    const auto blocked = generate_seed_package_from_linkedworlds(blocked_request, {*incomplete_surface});
    require(!blocked.ok, "incomplete_package_should_not_generate");
    require(blocked.error.find("missing_generation_capability") != std::string::npos,
            "incomplete_package_error_mismatch:" + blocked.error);

    GenerationPackageRequest missing_pool_request = request;
    missing_pool_request.seed_id = "seed-missing-pool";
    missing_pool_request.slots.resize(1);
    missing_pool_request.slots.front().game_key = "smoke_missing_pool";
    missing_pool_request.slots.front().linkedworld_id = "linkedworld.smoke_missing_pool";
    const auto missing_pool_blocked =
        generate_seed_package_from_linkedworlds(missing_pool_request, {*missing_pool_surface});
    require(!missing_pool_blocked.ok, "missing_pool_package_should_not_generate");
    require(missing_pool_blocked.error == "missing_generation_item_pool:linkedworld.smoke_missing_pool",
            "missing_pool_package_error_mismatch:" + missing_pool_blocked.error);

    std::cout << "sekailink_generic_generation_smoke ok\n";
    std::cout << "package_dir=" << package.package_dir << "\n";
    std::cout << "package_hash=" << package.package_hash << "\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_generic_generation_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
