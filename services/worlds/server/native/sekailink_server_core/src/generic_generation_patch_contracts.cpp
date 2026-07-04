#include "sekailink_server/generic_generation_patch_contracts.hpp"

#include "sekailink_server/generic_generation_internal.hpp"

#include <stdexcept>

namespace sekailink_server::generation_internal {

std::string patch_mode_for_surface(const LinkedWorldGenerationSurface& surface) {
  if (!surface.patch.is_object()) {
    return "unspecified";
  }
  if (surface.patch.contains("mode") && surface.patch.at("mode").is_string()) {
    return surface.patch.at("mode").get<std::string>();
  }
  const auto manifest = surface.patch.value("manifest", nlohmann::json::object());
  const auto manifest_patch = manifest.value("patch", nlohmann::json::object());
  return manifest_patch.value("mode", std::string{"unspecified"});
}

bool is_supported_patch_mode(const std::string& mode) {
  return mode == "artifact" || mode == "contract_only" || mode == "server_dispatch" ||
         mode == "none" || mode == "external_import";
}

namespace {

std::string expand_slot_template(std::string value, int slot_id) {
  const auto token = std::string{"{slot_id}"};
  auto position = value.find(token);
  while (position != std::string::npos) {
    value.replace(position, token.size(), std::to_string(slot_id));
    position = value.find(token, position + 1);
  }
  return value;
}

nlohmann::json patch_emission_for_surface(const LinkedWorldGenerationSurface& surface) {
  return patch_manifest_patch_for_surface(surface).value("emission", nlohmann::json::object());
}

std::string artifact_path_from_contract_field(const nlohmann::json& artifact) {
  if (artifact.is_null()) {
    return {};
  }
  if (artifact.is_string()) {
    return artifact.get<std::string>();
  }
  if (artifact.is_object()) {
    return artifact.value("path", std::string{});
  }
  return {};
}

bool hash_entry_matches_json(const nlohmann::json& entry, const nlohmann::json& value) {
  const auto expected = sha256_hex(value.dump());
  if (entry.is_string()) {
    return entry.get<std::string>() == expected;
  }
  if (!entry.is_object()) {
    return false;
  }
  const auto algorithm = entry.value("hash_algorithm", std::string{"sha256"});
  return algorithm == "sha256" &&
         (entry.value("sha256", std::string{}) == expected ||
          entry.value("digest", std::string{}) == expected);
}

void require_hashed_package_file(const std::map<std::string, nlohmann::json>& files,
                                 const nlohmann::json& artifact_hashes,
                                 const std::string& path,
                                 const std::string& missing_error,
                                 const std::string& hash_missing_error,
                                 const std::string& hash_mismatch_error) {
  if (path.empty() || files.count(path) == 0) {
    throw std::runtime_error(missing_error + ":" + path);
  }
  if (!artifact_hashes.contains(path)) {
    throw std::runtime_error(hash_missing_error + ":" + path);
  }
  if (!hash_entry_matches_json(artifact_hashes.at(path), files.at(path))) {
    throw std::runtime_error(hash_mismatch_error + ":" + path);
  }
}

}  // namespace

nlohmann::json patch_manifest_for_surface(const LinkedWorldGenerationSurface& surface) {
  return surface.patch.value("manifest", nlohmann::json::object());
}

nlohmann::json patch_manifest_patch_for_surface(const LinkedWorldGenerationSurface& surface) {
  return patch_manifest_for_surface(surface).value("patch", nlohmann::json::object());
}

std::string patch_contract_ref_for_slot(const LinkedWorldGenerationSurface& surface, int slot_id) {
  const auto emission = patch_emission_for_surface(surface);
  const auto directory = emission.value("patch_contract_directory", std::string{"patch_contracts/"});
  const auto filename =
      expand_slot_template(emission.value("patch_contract_filename_template",
                                          std::string{"slot-{slot_id}.patch.contract.json"}),
                           slot_id);
  return directory + filename;
}

nlohmann::json patch_artifact_ref_for_slot(const LinkedWorldGenerationSurface& surface, int slot_id) {
  const auto mode = patch_mode_for_surface(surface);
  if (mode != "artifact") {
    return nullptr;
  }
  const auto emission = patch_emission_for_surface(surface);
  const auto directory = emission.value("package_directory", std::string{"patches/"});
  std::string filename_template = emission.value("package_filename_template", std::string{});
  if (filename_template.empty()) {
    const auto extension = surface.patch.value("patch_file_extension", std::string{".patch"});
    filename_template = "slot-{slot_id}" + extension;
  }
  return directory + expand_slot_template(filename_template, slot_id);
}

nlohmann::json patch_contract_artifact_for_slot(const LinkedWorldGenerationSurface& surface, int slot_id) {
  const auto artifact_ref = patch_artifact_ref_for_slot(surface, slot_id);
  if (artifact_ref.is_null()) {
    return nullptr;
  }
  const auto manifest_patch = patch_manifest_patch_for_surface(surface);
  const auto extension = surface.patch.value("patch_file_extension",
                                             manifest_patch.value("artifact_extension", std::string{}));
  return {
      {"path", artifact_ref},
      {"kind", manifest_patch.value("artifact_kind", std::string{})},
      {"extension", extension},
      {"state", "placeholder"},
  };
}

void verify_generated_seed_package_contracts(const GenerationPackageRequest& request,
                                             const std::map<std::string, nlohmann::json>& files,
                                             const nlohmann::json& seed_manifest) {
  if (!seed_manifest.contains("artifact_hashes") || !seed_manifest.at("artifact_hashes").is_object()) {
    throw std::runtime_error("artifact_hashes_missing");
  }
  const auto& artifact_hashes = seed_manifest.at("artifact_hashes");
  for (const auto& [path, value] : files) {
    require_hashed_package_file(files,
                                artifact_hashes,
                                path,
                                "package_file_missing",
                                "package_file_hash_missing",
                                "package_file_hash_mismatch");
  }
  for (const auto& [path, unused] : artifact_hashes.items()) {
    (void)unused;
    if (files.count(path) == 0) {
      throw std::runtime_error("package_hash_entry_without_file:" + path);
    }
  }

  for (const auto& slot : request.slots) {
    const auto slot_manifest_path = "slot_manifest." + std::to_string(slot.slot_id) + ".json";
    require_hashed_package_file(files,
                                artifact_hashes,
                                slot_manifest_path,
                                "slot_manifest_missing",
                                "slot_manifest_ref_missing_hash",
                                "package_file_hash_mismatch");
    const auto& slot_manifest = files.at(slot_manifest_path);
    const auto patch_mode = slot_manifest.value("patch_mode", std::string{});
    const auto patch_contract_ref = slot_manifest.value("patch_contract_ref", std::string{});
    if (patch_contract_ref.empty()) {
      throw std::runtime_error("patch_contract_ref_missing:" + slot_manifest_path);
    }
    if (patch_contract_ref.rfind("patch_contracts/", 0) != 0) {
      throw std::runtime_error("patch_contract_ref_invalid:" + patch_contract_ref);
    }
    require_hashed_package_file(files,
                                artifact_hashes,
                                patch_contract_ref,
                                "patch_contract_ref_missing_file",
                                "patch_contract_ref_missing_hash",
                                "patch_contract_hash_mismatch");

    for (const auto& ref_key : {"tracker_seed_state", "sklmi_contract_ref", "link_room_contract_ref"}) {
      const auto ref = slot_manifest.value(ref_key, std::string{});
      require_hashed_package_file(files,
                                  artifact_hashes,
                                  ref,
                                  std::string{"slot_manifest_ref_missing:"} + ref_key,
                                  "slot_manifest_ref_missing_hash",
                                  "package_file_hash_mismatch");
    }

    const auto& patch_contract = files.at(patch_contract_ref);
    if (patch_contract.value("schema_version", std::string{}) != "sekailink-patch-contract-v1") {
      throw std::runtime_error("patch_contract_schema_mismatch:" + patch_contract_ref);
    }
    if (patch_contract.value("slot_id", 0) != slot.slot_id ||
        patch_contract.value("linkedworld_id", std::string{}) != slot_manifest.value("linkedworld_id", std::string{})) {
      throw std::runtime_error("patch_contract_slot_mismatch:" + patch_contract_ref);
    }
    if (patch_contract.value("patch_mode", std::string{}) != patch_mode ||
        patch_contract.value("mode", std::string{}) != patch_mode) {
      throw std::runtime_error("patch_contract_mode_mismatch:" + patch_contract_ref);
    }

    const auto slot_artifact = slot_manifest.value("patch_artifact", nlohmann::json(nullptr));
    const auto contract_artifact = patch_contract.value("artifact", nlohmann::json(nullptr));
    if (patch_mode == "artifact") {
      if (slot_artifact.is_null() || artifact_path_from_contract_field(contract_artifact) !=
                                         string_from_number_or_string(slot_artifact)) {
        throw std::runtime_error("artifact_required_for_artifact_mode:" + slot_manifest_path);
      }
      const auto artifact_path = string_from_number_or_string(slot_artifact);
      if (files.count(artifact_path) > 0 && !artifact_hashes.contains(artifact_path)) {
        throw std::runtime_error("artifact_hash_required_when_materialized:" + artifact_path);
      }
      const auto artifact_state = contract_artifact.is_object()
          ? contract_artifact.value("state", std::string{})
          : std::string{};
      if (files.count(artifact_path) == 0 && artifact_state != "placeholder") {
        throw std::runtime_error("patch_artifact_missing:" + artifact_path);
      }
    } else {
      if (!slot_artifact.is_null() || !contract_artifact.is_null()) {
        throw std::runtime_error("artifact_forbidden_for_non_artifact_mode:" + slot_manifest_path);
      }
      const auto server_dispatch = patch_contract.value("server_dispatch", nlohmann::json(nullptr));
      if (patch_mode == "server_dispatch") {
        if (!server_dispatch.is_object() || !truthy_json_flag(server_dispatch.value("enabled", false))) {
          throw std::runtime_error("server_dispatch_contract_incomplete:" + patch_contract_ref);
        }
      } else if (server_dispatch.is_object() && truthy_json_flag(server_dispatch.value("enabled", false))) {
        throw std::runtime_error("native_patcher_forbidden_for_non_artifact_mode:" + patch_contract_ref);
      }
    }
  }
}

}  // namespace sekailink_server::generation_internal
