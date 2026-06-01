#pragma once

#include "sekailink_server/generic_generation.hpp"

#include "nlohmann/json.hpp"

#include <map>
#include <string>

namespace sekailink_server::generation_internal {

std::string patch_mode_for_surface(const LinkedWorldGenerationSurface& surface);
bool is_supported_patch_mode(const std::string& mode);

nlohmann::json patch_manifest_for_surface(const LinkedWorldGenerationSurface& surface);
nlohmann::json patch_manifest_patch_for_surface(const LinkedWorldGenerationSurface& surface);
std::string patch_contract_ref_for_slot(const LinkedWorldGenerationSurface& surface, int slot_id);
nlohmann::json patch_artifact_ref_for_slot(const LinkedWorldGenerationSurface& surface, int slot_id);
nlohmann::json patch_contract_artifact_for_slot(const LinkedWorldGenerationSurface& surface, int slot_id);

void verify_generated_seed_package_contracts(const GenerationPackageRequest& request,
                                             const std::map<std::string, nlohmann::json>& files,
                                             const nlohmann::json& seed_manifest);

}  // namespace sekailink_server::generation_internal
