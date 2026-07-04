#pragma once

#include <libretro.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike {

struct CoreOptionChoice {
  std::string value;
  std::string label;
};

struct CoreOptionDefinitionView {
  std::string key;
  std::string desc;
  std::string info;
  std::string default_value;
  std::vector<CoreOptionChoice> values;
  bool requires_restart = false;
};

class CoreOptionManager {
 public:
  void Initialize(const std::filesystem::path& save_root,
                  const std::filesystem::path& core_path,
                  const std::filesystem::path& game_path);

  void RegisterCoreOptionsV2(const retro_core_option_v2_definition* definitions);
  void RegisterCoreOptionsLegacy(const retro_core_option_definition* definitions);
  void RegisterVariables(const retro_variable* variables);

  const char* GetValue(const char* key) const;
  bool ConsumeVariableUpdate();
  std::size_t DefinitionCount() const { return definitions_.size(); }
  const CoreOptionDefinitionView* DefinitionAt(std::size_t index) const;
  std::string CurrentValueFor(std::string_view key) const;
  bool StepValue(std::string_view key, int delta);
  bool ApplyPendingChanges();
  bool ResetPendingToDefaults();
  bool DiscardPendingChanges();
  bool HasPendingChanges() const;
  bool PendingChangesRequireRestart() const;

  const std::filesystem::path& CoreConfigPath() const { return core_config_path_; }
  const std::filesystem::path& GameConfigPath() const { return game_config_path_; }
  const std::filesystem::path& SchemaPath() const { return schema_path_; }

 private:
  void RegisterDefinition(CoreOptionDefinitionView definition);
  void FinalizeDefinitions();
  void ApplyRecommendedOverrides();
  void ApplyPersistedOverrides();
  void SaveArtifacts();

  void MaybeReloadExternalChanges();
  bool LoadConfigFile(const std::filesystem::path& path,
                      std::unordered_map<std::string, std::string>& output) const;
  bool SaveConfigFile(const std::filesystem::path& path,
                      const std::unordered_map<std::string, std::string>& values,
                      std::string& error) const;
  bool SaveSchemaFile(const std::filesystem::path& path, std::string& error) const;
  void MergeOverridesInto(const std::unordered_map<std::string, std::string>& values,
                          std::unordered_map<std::string, std::string>& target) const;

  static std::string NormalizeCoreKey(std::string_view name);
  static std::string Trim(std::string_view text);
  static std::optional<std::filesystem::file_time_type> ReadTimestamp(
      const std::filesystem::path& path);

  std::filesystem::path save_root_;
  std::filesystem::path core_config_path_;
  std::filesystem::path game_config_path_;
  std::filesystem::path schema_path_;
  std::string core_key_;
  std::string game_key_;

  std::vector<CoreOptionDefinitionView> definitions_;
  std::unordered_map<std::string, std::size_t> definition_index_;
  std::unordered_map<std::string, std::string> values_;
  std::unordered_map<std::string, std::string> pending_values_;

  std::optional<std::filesystem::file_time_type> core_config_timestamp_;
  std::optional<std::filesystem::file_time_type> game_config_timestamp_;
  bool initialized_ = false;
  bool finalized_ = false;
  bool variable_update_pending_ = false;
};

}  // namespace sekaiemu::spike
