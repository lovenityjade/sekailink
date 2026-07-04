#include "sekailink_server/seed_config_api.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

std::string required_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || !body.at(key).is_string() || body.at(key).get<std::string>().empty()) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  return body.at(key).get<std::string>();
}

std::int64_t required_int64_field(const nlohmann::json& body, const char* key) {
  if (!body.contains(key)) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  if (!body.at(key).is_number_integer() && !body.at(key).is_number_unsigned()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return body.at(key).get<std::int64_t>();
}

std::string preset_hash(const nlohmann::json& value) {
  const auto dumped = value.dump();
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(dumped.data()), dumped.size(), hash);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : hash) {
    out << std::setw(2) << static_cast<int>(byte);
  }
  return out.str();
}

nlohmann::json persistent_preset_to_json(const SeedConfigCommonPresetSnapshot& preset) {
  return {
      {"preset_id", preset.preset_id},
      {"version_id", preset.version_id},
      {"game_key", preset.game_key},
      {"preset_key", preset.preset_key},
      {"name", preset.name},
      {"description", preset.description},
      {"category", preset.category},
      {"visibility", preset.visibility},
      {"sort_order", preset.sort_order},
      {"values", preset.values},
      {"values_hash", preset.values_hash},
  };
}

}  // namespace

nlohmann::json SeedConfigApiService::handle_import_common_preset(const std::string& game_key, const nlohmann::json& body) {
  const auto preset_key = required_string(body, "preset_key");
  const auto name = required_string(body, "name");
  if (!body.contains("values")) {
    return {{"ok", false}, {"status", 400}, {"error", "missing_values"}};
  }

  if (persistent()) {
    const auto schema = store_->find_active_option_schema(game_key);
    if (!schema.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
    }
    const auto definitions = store_->load_option_definitions(schema->id);
    auto validation = validate_seed_config_values(definitions, body.at("values"));
    if (!validation.ok) {
      return {
          {"ok", false},
          {"status", 400},
          {"error", "invalid_seed_preset"},
          {"issues", seed_config_validation_issues_to_json(validation.issues)},
      };
    }
    const auto hash = preset_hash(validation.canonical_values);
    const auto preset_id = store_->upsert_common_preset(
        schema->game_id,
        preset_key,
        name,
        body.value("description", ""),
        body.value("category", ""),
        body.value("visibility", "official"),
        body.value("sort_order", 0),
        body.value("status", "active"));
    const auto version_id = store_->append_common_preset_version(
        preset_id,
        schema->id,
        validation.canonical_values,
        hash,
        "valid",
        nlohmann::json(nullptr));
    store_->set_current_common_preset_version(preset_id, version_id);
    return {
        {"ok", true},
        {"persistent", true},
        {"preset",
         {
             {"preset_id", preset_id},
             {"version_id", version_id},
             {"game_key", game_key},
             {"preset_key", preset_key},
             {"name", name},
             {"description", body.value("description", "")},
             {"category", body.value("category", "")},
             {"visibility", body.value("visibility", "official")},
             {"sort_order", body.value("sort_order", 0)},
             {"values", validation.canonical_values},
             {"values_hash", hash},
         }},
    };
  }

  const auto game_it = games_.find(game_key);
  if (game_it == games_.end()) {
    return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
  }
  auto validation = validate_seed_config_values(game_it->second.definitions, body.at("values"));
  if (!validation.ok) {
    return {
        {"ok", false},
        {"status", 400},
        {"error", "invalid_seed_preset"},
        {"issues", seed_config_validation_issues_to_json(validation.issues)},
    };
  }
  const auto existing = std::find_if(common_presets_.begin(), common_presets_.end(), [&](const auto& preset) {
    return preset.game_key == game_key && preset.preset_key == preset_key;
  });
  CommonPresetEntry preset{
      .preset_id = existing == common_presets_.end() ? next_common_preset_id_++ : existing->preset_id,
      .version_id = next_common_preset_version_id_++,
      .game_key = game_key,
      .preset_key = preset_key,
      .name = name,
      .description = body.value("description", ""),
      .category = body.value("category", ""),
      .visibility = body.value("visibility", "official"),
      .sort_order = body.value("sort_order", 0),
      .values = validation.canonical_values,
      .values_hash = preset_hash(validation.canonical_values),
  };
  if (existing == common_presets_.end()) {
    common_presets_.push_back(preset);
  } else {
    *existing = preset;
  }
  return {{"ok", true}, {"preset", common_preset_to_json(preset)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::handle_list_common_presets(const std::string& game_key) const {
  nlohmann::json presets = nlohmann::json::array();
  if (persistent()) {
    const auto game = store_->find_game_by_key(game_key);
    if (!game.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
    }
    for (const auto& preset : store_->list_common_preset_snapshots(game_key)) {
      presets.push_back(persistent_preset_to_json(preset));
    }
    return {{"ok", true}, {"game_key", game_key}, {"presets", std::move(presets)}, {"persistent", true}};
  }
  if (!games_.contains(game_key)) {
    return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
  }
  for (const auto& preset : common_presets_) {
    if (preset.game_key == game_key) {
      presets.push_back(common_preset_to_json(preset));
    }
  }
  return {{"ok", true}, {"game_key", game_key}, {"presets", std::move(presets)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::handle_delete_common_preset(const std::string& game_key, const std::string& preset_key) {
  if (persistent()) {
    const auto game = store_->find_game_by_key(game_key);
    if (!game.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
    }
    if (!store_->delete_common_preset(game_key, preset_key)) {
      return {{"ok", false}, {"status", 404}, {"error", "preset_not_found"}};
    }
    return {{"ok", true}, {"persistent", true}, {"game_key", game_key}, {"preset_key", preset_key}, {"deleted", true}};
  }

  if (!games_.contains(game_key)) {
    return {{"ok", false}, {"status", 404}, {"error", "game_not_found"}};
  }
  const auto before = common_presets_.size();
  common_presets_.erase(
      std::remove_if(common_presets_.begin(), common_presets_.end(), [&](const auto& preset) {
        return preset.game_key == game_key && preset.preset_key == preset_key;
      }),
      common_presets_.end());
  if (common_presets_.size() == before) {
    return {{"ok", false}, {"status", 404}, {"error", "preset_not_found"}};
  }
  return {{"ok", true}, {"persistent", false}, {"game_key", game_key}, {"preset_key", preset_key}, {"deleted", true}};
}

nlohmann::json SeedConfigApiService::handle_duplicate_common_preset(std::int64_t user_id, const nlohmann::json& body) {
  const auto preset_id = required_int64_field(body, "preset_id");
  if (persistent()) {
    const auto preset = store_->find_common_preset_snapshot(preset_id);
    if (!preset.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "preset_not_found"}};
    }
    const auto name = body.value("name", preset->name);
    const auto hash = preset_hash(preset->values);
    try {
      const auto config_id = store_->create_user_config(
          user_id,
          preset->game_id,
          name,
          body.value("description", preset->description),
          body.value("is_default", false));
      const auto version_id = store_->append_user_config_version(
          config_id,
          preset->schema_version_id,
          preset->values,
          hash,
          std::nullopt,
          "valid",
          nlohmann::json(nullptr));
      store_->set_current_config_version(config_id, version_id);
      return {
          {"ok", true},
          {"persistent", true},
          {"source_preset_id", preset_id},
          {"config",
           {
               {"config_id", config_id},
               {"version_id", version_id},
               {"user_id", user_id},
               {"game_key", preset->game_key},
               {"name", name},
               {"values", preset->values},
               {"values_hash", hash},
           }},
      };
    } catch (const std::exception& exception) {
      if (std::string(exception.what()).find("Duplicate") != std::string::npos) {
        return {{"ok", false}, {"status", 409}, {"error", "config_name_conflict"}};
      }
      throw;
    }
  }

  const auto preset = find_common_preset(preset_id);
  if (!preset.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "preset_not_found"}};
  }
  const auto name = body.value("name", preset->name);
  const auto duplicate = std::any_of(user_configs_.begin(), user_configs_.end(), [&](const auto& config) {
    return config.user_id == user_id && config.game_key == preset->game_key && config.name == name;
  });
  if (duplicate) {
    return {{"ok", false}, {"status", 409}, {"error", "config_name_conflict"}};
  }
  UserConfigEntry config{
      .config_id = next_config_id_++,
      .version_id = next_version_id_++,
      .user_id = user_id,
      .game_key = preset->game_key,
      .name = name,
      .values = preset->values,
      .values_hash = preset_hash(preset->values),
  };
  user_configs_.push_back(config);
  return {{"ok", true}, {"source_preset_id", preset_id}, {"config", user_config_to_json(config)}, {"persistent", false}};
}

nlohmann::json SeedConfigApiService::common_preset_to_json(const CommonPresetEntry& preset) const {
  return {
      {"preset_id", preset.preset_id},
      {"version_id", preset.version_id},
      {"game_key", preset.game_key},
      {"preset_key", preset.preset_key},
      {"name", preset.name},
      {"description", preset.description},
      {"category", preset.category},
      {"visibility", preset.visibility},
      {"sort_order", preset.sort_order},
      {"values", preset.values},
      {"values_hash", preset.values_hash},
  };
}

std::optional<SeedConfigApiService::CommonPresetEntry> SeedConfigApiService::find_common_preset(
    std::int64_t preset_id) const {
  const auto it = std::find_if(common_presets_.begin(), common_presets_.end(), [&](const auto& preset) {
    return preset.preset_id == preset_id;
  });
  if (it == common_presets_.end()) {
    return std::nullopt;
  }
  return *it;
}

}  // namespace sekailink_server
