#include "sekailink_server/seed_config_service.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<sekailink_server::SeedConfigOptionDefinition> alttp_definitions() {
  return {
      sekailink_server::SeedConfigOptionDefinition{
          .option_key = "goal",
          .yaml_key = "goal",
          .label = "Goal",
          .description = "Main completion goal.",
          .type = "enum",
          .default_value = "ganon",
          .required = true,
          .choices = {
              {.choice_key = "ganon", .yaml_value = "ganon", .label = "Defeat Ganon", .description = ""},
              {.choice_key = "triforce_hunt", .yaml_value = "triforce_hunt", .label = "Triforce Hunt", .description = ""},
          },
      },
      sekailink_server::SeedConfigOptionDefinition{
          .option_key = "triforce_pieces_required",
          .yaml_key = "triforce_pieces_required",
          .label = "Triforce Pieces Required",
          .description = "Pieces required for Triforce Hunt.",
          .type = "integer",
          .default_value = 20,
          .required = true,
          .validation_rules = {{"range_start", 1}, {"range_end", 90}},
      },
      sekailink_server::SeedConfigOptionDefinition{
          .option_key = "swordless",
          .yaml_key = "swordless",
          .label = "Swordless",
          .description = "Enable swordless mode.",
          .type = "boolean",
          .default_value = false,
          .required = true,
      },
      sekailink_server::SeedConfigOptionDefinition{
          .option_key = "junk_fill_weights",
          .yaml_key = "junk_fill_weights",
          .label = "Junk fill weights",
          .description = "Weighted junk item fill map.",
          .type = "enum",
          .default_value = nlohmann::json{{"Missile Tank", 1}, {"Super Missile Tank", 0}, {"Power Bomb Tank", 0}, {"Nothing", 0}},
          .required = true,
      },
  };
}

}  // namespace

int main() {
  try {
    const auto definitions = alttp_definitions();
    const auto valid = sekailink_server::validate_seed_config_values(
        definitions,
        {
            {"goal", "triforce_hunt"},
            {"triforce_pieces_required", 30},
            {"junk_fill_weights", "Missile Tank"},
        });
    require(valid.ok, "valid_config_rejected");
    require(valid.canonical_values.at("goal") == "triforce_hunt", "goal_not_canonical");
    require(valid.canonical_values.at("swordless") == false, "default_not_applied");

    const auto unknown = sekailink_server::validate_seed_config_values(definitions, {{"bad_yaml_key", true}});
    require(!unknown.ok, "unknown_option_accepted");
    require(unknown.issues.front().code == "unknown_option", "wrong_unknown_error");

    const auto bad_enum = sekailink_server::validate_seed_config_values(definitions, {{"goal", "not_a_goal"}});
    require(!bad_enum.ok, "bad_enum_accepted");
    require(bad_enum.issues.front().code == "invalid_type", "wrong_bad_enum_error");

    const auto bad_range = sekailink_server::validate_seed_config_values(definitions, {{"triforce_pieces_required", 100}});
    require(!bad_range.ok, "bad_range_accepted");
    require(bad_range.issues.front().code == "above_maximum", "wrong_bad_range_error");

    const auto yaml = sekailink_server::export_seed_config_yaml(definitions, valid.canonical_values);
    require(yaml.find("goal: triforce_hunt") != std::string::npos, "yaml_goal_missing");
    require(yaml.find("triforce_pieces_required: 30") != std::string::npos, "yaml_integer_missing");
    require(yaml.find("swordless: false") != std::string::npos, "yaml_default_missing");
    require(yaml.find("junk_fill_weights: {") != std::string::npos, "yaml_object_option_missing");
    require(yaml.find("\"Missile Tank\":1") != std::string::npos, "yaml_weighted_choice_missing");

    const auto manifest = sekailink_server::seed_config_sync_manifest(
        42,
        {
            {.entity_type = "game_option_schema", .entity_id = "alttp:v1", .version = "v1", .hash = "hash-1", .updated_at = "2026-05-10T00:00:00Z"},
            {.entity_type = "user_game_config", .entity_id = "config-1", .version = "3", .hash = "hash-2", .updated_at = "2026-05-10T00:01:00Z"},
        },
        "cursor-1");
    require(manifest.at("format") == "sklconf-sync-manifest-v1", "manifest_format");
    require(manifest.at("entries").size() == 2, "manifest_entries");
    std::cout << "seed_config_service_smoke_ok\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "seed_config_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
