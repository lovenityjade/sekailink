#include "core_option_manager.hpp"

#include "libretro_core_utils.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

namespace sekaiemu::spike {

namespace {

constexpr const char* kCoreConfigDir = "core-config";

bool ContainsRestartHint(std::string_view text) {
  const std::string lowered = Lowercase(std::string(text));
  return lowered.find("restart") != std::string::npos ||
         lowered.find("reset") != std::string::npos ||
         lowered.find("takes effect after") != std::string::npos;
}

}

void CoreOptionManager::Initialize(const std::filesystem::path& save_root,
                                   const std::filesystem::path& core_path,
                                   const std::filesystem::path& game_path) {
  save_root_ = save_root / kCoreConfigDir;
  core_key_ = NormalizeCoreKey(core_path.stem().string());
  game_key_ = NormalizeCoreKey(game_path.stem().string());
  core_config_path_ = save_root_ / (core_key_ + ".cfg");
  game_config_path_ = save_root_ / (core_key_ + "__" + game_key_ + ".cfg");
  schema_path_ = save_root_ / (core_key_ + ".options.txt");
  initialized_ = true;
}

void CoreOptionManager::RegisterCoreOptionsV2(const retro_core_option_v2_definition* definitions) {
  if (!definitions) {
    return;
  }

  definitions_.clear();
  definition_index_.clear();
  values_.clear();
  pending_values_.clear();

  for (auto* definition = definitions; definition && definition->key; ++definition) {
    CoreOptionDefinitionView view;
    view.key = definition->key;
    view.desc = definition->desc ? definition->desc : "";
    view.info = definition->info ? definition->info : "";
    view.default_value = definition->default_value ? definition->default_value : "";
    view.requires_restart = ContainsRestartHint(view.info);
    for (const auto& value : definition->values) {
      if (!value.value) {
        break;
      }
      view.values.push_back(CoreOptionChoice{
          value.value,
          value.label ? value.label : "",
      });
    }
    RegisterDefinition(std::move(view));
  }

  FinalizeDefinitions();
}

void CoreOptionManager::RegisterCoreOptionsLegacy(const retro_core_option_definition* definitions) {
  if (!definitions) {
    return;
  }

  definitions_.clear();
  definition_index_.clear();
  values_.clear();
  pending_values_.clear();

  for (auto* definition = definitions; definition && definition->key; ++definition) {
    CoreOptionDefinitionView view;
    view.key = definition->key;
    view.desc = definition->desc ? definition->desc : "";
    view.info = definition->info ? definition->info : "";
    view.default_value = definition->default_value ? definition->default_value : "";
    view.requires_restart = ContainsRestartHint(view.info);
    for (const auto& value : definition->values) {
      if (!value.value) {
        break;
      }
      view.values.push_back(CoreOptionChoice{
          value.value,
          value.label ? value.label : "",
      });
    }
    RegisterDefinition(std::move(view));
  }

  FinalizeDefinitions();
}

void CoreOptionManager::RegisterVariables(const retro_variable* variables) {
  if (!variables) {
    return;
  }

  definitions_.clear();
  definition_index_.clear();
  values_.clear();
  pending_values_.clear();

  for (auto* variable = variables; variable && variable->key; ++variable) {
    if (!variable->value) {
      continue;
    }

    const std::string descriptor = variable->value;
    const auto semicolon = descriptor.find(';');
    if (semicolon == std::string::npos) {
      continue;
    }

    CoreOptionDefinitionView view;
    view.key = variable->key;
    view.desc = Trim(descriptor.substr(0, semicolon));

    std::string values_text = descriptor.substr(semicolon + 1);
    std::stringstream stream(values_text);
    std::string entry;
    while (std::getline(stream, entry, '|')) {
      entry = Trim(entry);
      if (entry.empty()) {
        continue;
      }
      view.values.push_back(CoreOptionChoice{entry, ""});
      if (view.default_value.empty()) {
        view.default_value = entry;
      }
    }

    RegisterDefinition(std::move(view));
  }

  FinalizeDefinitions();
}

const char* CoreOptionManager::GetValue(const char* key) const {
  if (!key) {
    return nullptr;
  }
  const auto found = values_.find(key);
  if (found == values_.end()) {
    return nullptr;
  }
  return found->second.c_str();
}

bool CoreOptionManager::ConsumeVariableUpdate() {
  MaybeReloadExternalChanges();
  const bool updated = variable_update_pending_;
  variable_update_pending_ = false;
  return updated;
}

const CoreOptionDefinitionView* CoreOptionManager::DefinitionAt(std::size_t index) const {
  if (index >= definitions_.size()) {
    return nullptr;
  }
  return &definitions_[index];
}

std::string CoreOptionManager::CurrentValueFor(std::string_view key) const {
  const auto found = pending_values_.find(std::string(key));
  if (found == pending_values_.end()) {
    return {};
  }
  return found->second;
}

bool CoreOptionManager::StepValue(std::string_view key, int delta) {
  if (delta == 0) {
    return false;
  }

  const auto found_index = definition_index_.find(std::string(key));
  if (found_index == definition_index_.end()) {
    return false;
  }

  auto& definition = definitions_[found_index->second];
  if (definition.values.empty()) {
    return false;
  }

  const auto current_value = CurrentValueFor(key);
  std::size_t current_index = 0;
  for (std::size_t index = 0; index < definition.values.size(); ++index) {
    if (definition.values[index].value == current_value) {
      current_index = index;
      break;
    }
  }

  const auto count = static_cast<int>(definition.values.size());
  int next_index = static_cast<int>(current_index) + delta;
  while (next_index < 0) {
    next_index += count;
  }
  while (next_index >= count) {
    next_index -= count;
  }

  const auto& next_value = definition.values[static_cast<std::size_t>(next_index)].value;
  if (next_value == current_value) {
    return false;
  }

  pending_values_[std::string(key)] = next_value;
  return true;
}

bool CoreOptionManager::ApplyPendingChanges() {
  if (!HasPendingChanges()) {
    return false;
  }

  values_ = pending_values_;
  std::string error;
  if (!SaveConfigFile(core_config_path_, values_, error)) {
    std::cerr << "[sekaiemu] core config update failed: " << error << "\n";
  }
  core_config_timestamp_ = ReadTimestamp(core_config_path_);
  variable_update_pending_ = true;
  return true;
}

bool CoreOptionManager::ResetPendingToDefaults() {
  bool changed = false;
  for (const auto& definition : definitions_) {
    const auto found = pending_values_.find(definition.key);
    if (found == pending_values_.end()) {
      continue;
    }
    if (found->second != definition.default_value) {
      found->second = definition.default_value;
      changed = true;
    }
  }
  return changed;
}

bool CoreOptionManager::DiscardPendingChanges() {
  if (!HasPendingChanges()) {
    return false;
  }
  pending_values_ = values_;
  return true;
}

bool CoreOptionManager::HasPendingChanges() const {
  return pending_values_ != values_;
}

bool CoreOptionManager::PendingChangesRequireRestart() const {
  for (const auto& definition : definitions_) {
    const auto applied = values_.find(definition.key);
    const auto pending = pending_values_.find(definition.key);
    if (applied == values_.end() || pending == pending_values_.end()) {
      continue;
    }
    if (applied->second != pending->second && definition.requires_restart) {
      return true;
    }
  }
  return false;
}

void CoreOptionManager::RegisterDefinition(CoreOptionDefinitionView definition) {
  if (definition.key.empty()) {
    return;
  }
  definition_index_[definition.key] = definitions_.size();
  values_[definition.key] = definition.default_value;
  pending_values_[definition.key] = definition.default_value;
  definitions_.push_back(std::move(definition));
}

void CoreOptionManager::FinalizeDefinitions() {
  finalized_ = true;
  ApplyRecommendedOverrides();
  ApplyPersistedOverrides();
  SaveArtifacts();
}

void CoreOptionManager::ApplyRecommendedOverrides() {
  if (core_key_.find("mupen64plus") == std::string::npos) {
    return;
  }

  for (auto& [key, value] : values_) {
    if (EndsWith(key, "-rdp-plugin")) {
      value = "gliden64";
    } else if (EndsWith(key, "-rsp-plugin")) {
      value = "hle";
    }
  }
  pending_values_ = values_;
}

void CoreOptionManager::ApplyPersistedOverrides() {
  std::unordered_map<std::string, std::string> loaded_values;
  if (LoadConfigFile(core_config_path_, loaded_values)) {
    MergeOverridesInto(loaded_values, values_);
  }
  loaded_values.clear();
  if (LoadConfigFile(game_config_path_, loaded_values)) {
    MergeOverridesInto(loaded_values, values_);
  }
  pending_values_ = values_;
  core_config_timestamp_ = ReadTimestamp(core_config_path_);
  game_config_timestamp_ = ReadTimestamp(game_config_path_);
}

void CoreOptionManager::SaveArtifacts() {
  std::string error;
  if (!SaveConfigFile(core_config_path_, values_, error)) {
    std::cerr << "[sekaiemu] core config save failed: " << error << "\n";
  }
  error.clear();
  if (!SaveSchemaFile(schema_path_, error)) {
    std::cerr << "[sekaiemu] core schema save failed: " << error << "\n";
  }
  core_config_timestamp_ = ReadTimestamp(core_config_path_);
  game_config_timestamp_ = ReadTimestamp(game_config_path_);
}

void CoreOptionManager::MaybeReloadExternalChanges() {
  if (!initialized_ || !finalized_) {
    return;
  }

  const auto latest_core = ReadTimestamp(core_config_path_);
  const auto latest_game = ReadTimestamp(game_config_path_);
  const bool core_changed = latest_core != core_config_timestamp_;
  const bool game_changed = latest_game != game_config_timestamp_;
  if (!core_changed && !game_changed) {
    return;
  }

  ApplyRecommendedOverrides();

  std::unordered_map<std::string, std::string> loaded_values;
  if (core_changed && LoadConfigFile(core_config_path_, loaded_values)) {
    MergeOverridesInto(loaded_values, values_);
    loaded_values.clear();
  }
  if (game_changed && LoadConfigFile(game_config_path_, loaded_values)) {
    MergeOverridesInto(loaded_values, values_);
  }

  pending_values_ = values_;
  core_config_timestamp_ = latest_core;
  game_config_timestamp_ = latest_game;
  variable_update_pending_ = true;
}

bool CoreOptionManager::LoadConfigFile(
    const std::filesystem::path& path,
    std::unordered_map<std::string, std::string>& output) const {
  if (!std::filesystem::exists(path)) {
    return false;
  }

  std::ifstream stream(path);
  if (!stream) {
    return false;
  }

  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const auto key = Trim(line.substr(0, equals));
    const auto value = Trim(line.substr(equals + 1));
    if (!key.empty()) {
      output[key] = value;
    }
  }
  return true;
}

bool CoreOptionManager::SaveConfigFile(
    const std::filesystem::path& path,
    const std::unordered_map<std::string, std::string>& values,
    std::string& error) const {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "Failed to create config directory: " + ec.message();
    return false;
  }

  std::ofstream stream(path, std::ios::trunc);
  if (!stream) {
    error = "Failed to open core config for writing.";
    return false;
  }

  stream << "# Sekaiemu core settings\n";
  stream << "# Edit values and use Apply in the Sekaiemu menu.\n";
  stream << "# Options marked with restart notes in the menu may require a core restart.\n";
  for (const auto& definition : definitions_) {
    const auto found = values.find(definition.key);
    if (found == values.end()) {
      continue;
    }
    stream << definition.key << "=" << found->second << "\n";
  }

  if (!stream) {
    error = "Failed to write core config.";
    return false;
  }
  return true;
}

bool CoreOptionManager::SaveSchemaFile(const std::filesystem::path& path,
                                       std::string& error) const {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "Failed to create schema directory: " + ec.message();
    return false;
  }

  std::ofstream stream(path, std::ios::trunc);
  if (!stream) {
    error = "Failed to open core schema for writing.";
    return false;
  }

  stream << "# Sekaiemu core option schema\n";
  stream << "# Generated from libretro option metadata exposed by the core.\n\n";
  for (const auto& definition : definitions_) {
    stream << "[" << definition.key << "]\n";
    if (!definition.desc.empty()) {
      stream << "desc=" << definition.desc << "\n";
    }
    if (!definition.info.empty()) {
      stream << "info=" << definition.info << "\n";
    }
    stream << "requires_restart=" << (definition.requires_restart ? "true" : "false") << "\n";
    stream << "default=" << definition.default_value << "\n";
    stream << "values=";
    for (std::size_t index = 0; index < definition.values.size(); ++index) {
      if (index > 0) {
        stream << " | ";
      }
      stream << definition.values[index].value;
      if (!definition.values[index].label.empty()) {
        stream << " (" << definition.values[index].label << ")";
      }
    }
    stream << "\n\n";
  }

  if (!stream) {
    error = "Failed to write core schema.";
    return false;
  }
  return true;
}

void CoreOptionManager::MergeOverridesInto(
    const std::unordered_map<std::string, std::string>& values,
    std::unordered_map<std::string, std::string>& target) const {
  for (const auto& [key, value] : values) {
    if (!definition_index_.contains(key)) {
      continue;
    }
    target[key] = value;
  }
}

}  // namespace sekaiemu::spike
