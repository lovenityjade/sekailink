#include "sekailink_server/seed_config_api.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    sekailink_server::SeedConfigApiService service({.admin_token = "admin-secret", .user_token = "user-secret"});

    const auto import = service.handle(
        "POST",
        "/admin/seed-configs/games",
        "admin-secret",
        nlohmann::json{
            {"game_key", "alttp"},
            {"display_name", "A Link to the Past"},
            {"system_key", "snes"},
            {"linkedworld_id", "linkedworld.alttp.v1"},
            {"schema_version", "alttp-options-v1"},
            {"source_hash", "linkedworld-hash"},
            {"options",
             nlohmann::json::array({
                 {
                     {"option_key", "goal"},
                     {"yaml_key", "goal"},
                     {"label", "Goal"},
                     {"type", "enum"},
                     {"default_value", "ganon"},
                     {"choices",
                      nlohmann::json::array({
                          {{"choice_key", "ganon"}, {"yaml_value", "ganon"}, {"label", "Defeat Ganon"}},
                          {{"choice_key", "triforce_hunt"}, {"yaml_value", "triforce_hunt"}, {"label", "Triforce Hunt"}},
                      })},
                 },
                 {
                     {"option_key", "triforce_pieces_required"},
                     {"yaml_key", "triforce_pieces_required"},
                     {"label", "Triforce pieces required"},
                     {"type", "integer"},
                     {"default_value", 20},
                 },
                 {
                     {"option_key", "swordless"},
                     {"yaml_key", "swordless"},
                     {"label", "Swordless"},
                     {"type", "boolean"},
                     {"default_value", false},
                 },
             })},
        });
    require(import.at("ok").get<bool>(), "import_failed");
    require(import.at("game").at("options").size() == 3, "import_options_count");

    const auto games = service.handle("GET", "/seed-configs/games", "user-secret", std::nullopt);
    require(games.at("ok").get<bool>(), "games_failed");
    require(games.at("games").size() == 1, "games_count");

    const auto options = service.handle("GET", "/seed-configs/games/alttp/options", "user-secret", std::nullopt);
    require(options.at("ok").get<bool>(), "options_failed");
    require(options.at("game").at("schema_version").get<std::string>() == "alttp-options-v1", "options_schema");

    const auto preset_import = service.handle(
        "POST",
        "/admin/seed-configs/games/alttp/presets",
        "admin-secret",
        nlohmann::json{
            {"preset_key", "keysanity_showcase"},
            {"name", "Keysanity Showcase"},
            {"description", "Shared starter preset for Keysanity-style testing."},
            {"category", "Showcase"},
            {"visibility", "official"},
            {"sort_order", 10},
            {"values",
             {
                 {"goal", "triforce_hunt"},
                 {"triforce_pieces_required", 40},
             }},
        });
    require(preset_import.at("ok").get<bool>(), "preset_import_failed");
    require(preset_import.at("preset").at("values").at("swordless").get<bool>() == false, "preset_default_missing");

    const auto presets = service.handle("GET", "/seed-configs/games/alttp/presets", "user-secret", std::nullopt);
    require(presets.at("ok").get<bool>(), "presets_failed");
    require(presets.at("presets").size() == 1, "presets_count");
    require(presets.at("presets").at(0).at("preset_key").get<std::string>() == "keysanity_showcase", "preset_key");

    const auto admin_presets = service.handle("GET", "/admin/seed-configs/games/alttp/presets", "admin-secret", std::nullopt);
    require(admin_presets.at("ok").get<bool>(), "admin_presets_failed");
    require(admin_presets.at("presets").size() == 1, "admin_presets_count");

    const auto preset_update = service.handle(
        "PUT",
        "/admin/seed-configs/games/alttp/presets/keysanity_showcase",
        "admin-secret",
        nlohmann::json{
            {"name", "Keysanity Showcase Updated"},
            {"description", "Updated shared starter preset."},
            {"category", "Showcase"},
            {"visibility", "official"},
            {"sort_order", 5},
            {"values",
             {
                 {"goal", "ganon"},
                 {"triforce_pieces_required", 20},
             }},
        });
    require(preset_update.at("ok").get<bool>(), "preset_update_failed");
    require(preset_update.at("preset").at("name").get<std::string>() == "Keysanity Showcase Updated", "preset_update_name");

    const auto preset_copy = service.handle(
        "POST",
        "/users/42/seed-configs/from-preset",
        "user-secret",
        nlohmann::json{
            {"preset_id", presets.at("presets").at(0).at("preset_id")},
            {"name", "Copied Keysanity Showcase"},
        });
    require(preset_copy.at("ok").get<bool>(), "preset_copy_failed");
    require(preset_copy.at("config").at("values").at("triforce_pieces_required").get<int>() == 20, "preset_copy_values");

    const auto save = service.handle(
        "POST",
        "/users/42/seed-configs",
        "user-secret",
        nlohmann::json{
            {"game_key", "alttp"},
            {"name", "Casual Triforce Hunt"},
            {"values",
             {
                 {"goal", "triforce_hunt"},
                 {"triforce_pieces_required", 30},
             }},
        });
    require(save.at("ok").get<bool>(), "save_failed");
    require(save.at("config").at("values").at("swordless").get<bool>() == false, "default_missing");
    require(save.at("config").at("values_hash").get<std::string>().size() == 64, "sha256_hash_size");

    const auto duplicate = service.handle(
        "POST",
        "/users/42/seed-configs",
        "user-secret",
        nlohmann::json{
            {"game_key", "alttp"},
            {"name", "Casual Triforce Hunt"},
            {"values", {{"goal", "ganon"}}},
        });
    require(!duplicate.at("ok").get<bool>(), "duplicate_should_fail");
    require(duplicate.at("status").get<int>() == 409, "duplicate_status");

    const auto invalid = service.handle(
        "POST",
        "/users/42/seed-configs",
        "user-secret",
        nlohmann::json{
            {"game_key", "alttp"},
            {"name", "Broken"},
            {"values", {{"goal", "not_a_goal"}, {"unknown", true}}},
        });
    require(!invalid.at("ok").get<bool>(), "invalid_should_fail");
    require(invalid.at("error").get<std::string>() == "invalid_seed_config", "invalid_error");
    require(invalid.at("issues").size() >= 2, "invalid_issue_count");

    const auto list = service.handle("GET", "/users/42/seed-configs?game_key=alttp", "user-secret", std::nullopt);
    require(list.at("ok").get<bool>(), "list_failed");
    require(list.at("configs").size() == 2, "list_count");

    const auto yaml = service.handle(
        "POST",
        "/users/42/seed-configs/" + std::to_string(save.at("config").at("config_id").get<std::int64_t>()) + "/export-yaml",
        "user-secret",
        std::nullopt);
    require(yaml.at("ok").get<bool>(), "yaml_failed");
    require(yaml.at("yaml").get<std::string>().find("goal: triforce_hunt") != std::string::npos, "yaml_goal");
    require(yaml.at("yaml").get<std::string>().find("swordless: false") != std::string::npos, "yaml_default");

    const auto delete_config = service.handle(
        "DELETE",
        "/users/42/seed-configs/" + std::to_string(save.at("config").at("config_id").get<std::int64_t>()),
        "user-secret",
        std::nullopt);
    require(delete_config.at("ok").get<bool>(), "delete_config_failed");
    const auto yaml_after_delete = service.handle(
        "POST",
        "/users/42/seed-configs/" + std::to_string(save.at("config").at("config_id").get<std::int64_t>()) + "/export-yaml",
        "user-secret",
        std::nullopt);
    require(!yaml_after_delete.at("ok").get<bool>(), "deleted_config_should_not_export");
    require(yaml_after_delete.at("status").get<int>() == 404, "deleted_config_export_status");

    const auto manifest = service.handle("GET", "/users/42/sklconf/manifest", "user-secret", std::nullopt);
    require(manifest.at("ok").get<bool>(), "manifest_failed");
    require(manifest.at("manifest").at("format").get<std::string>() == "sklconf-sync-manifest-v1", "manifest_format");
    require(manifest.at("manifest").at("entries").size() == 3, "manifest_entries");
    for (const auto& entry : manifest.at("manifest").at("entries")) {
      require(
          entry.value("entity_id", std::string{}) != std::to_string(save.at("config").at("config_id").get<std::int64_t>()),
          "deleted_config_in_manifest");
    }

    const auto preset_delete = service.handle(
        "DELETE",
        "/admin/seed-configs/games/alttp/presets/keysanity_showcase",
        "admin-secret",
        std::nullopt);
    require(preset_delete.at("ok").get<bool>(), "preset_delete_failed");
    const auto presets_after_delete = service.handle("GET", "/admin/seed-configs/games/alttp/presets", "admin-secret", std::nullopt);
    require(presets_after_delete.at("presets").empty(), "preset_delete_count");

    const auto unauthorized = service.handle("GET", "/seed-configs/games", "bad-token", std::nullopt);
    require(!unauthorized.at("ok").get<bool>(), "unauthorized_should_fail");
    require(unauthorized.at("status").get<int>() == 401, "unauthorized_status");

    std::cout << "seed_config_api_smoke_ok\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "seed_config_api_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
