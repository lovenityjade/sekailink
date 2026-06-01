#include "api_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sekailink::sklmi {

namespace {

bool is_allowed_write_source(std::string_view source) {
    return source.empty() || source == "constant" || source == "current_plus" || source == "current_plus_delta";
}

bool domain_declared(const CoreProfileManifest& core_profile, std::string_view domain_id) {
    return core_profile.domain_ids.empty() ||
           std::find(core_profile.domain_ids.begin(), core_profile.domain_ids.end(), domain_id) != core_profile.domain_ids.end();
}

std::optional<BridgeManifest> parse_bridge_manifest_text(const std::string& text,
                                                         const std::filesystem::path& path,
                                                         std::string* error) {
    BridgeManifest manifest;
    std::string parse_source = text;
    const auto embedded_sklmi = detail::extract_object_field_block(text, "sklmi");
    const bool is_linkedworld = embedded_sklmi.has_value();
    if (is_linkedworld) {
        const auto sklmi_key_pos = text.find("\"sklmi\"");
        const auto linkedworld_header = sklmi_key_pos == std::string::npos ? text : text.substr(0, sklmi_key_pos);
        const auto linkedworld_type = detail::extract_string_field(linkedworld_header, "type");
        if (!linkedworld_type.has_value() || *linkedworld_type != "linkedworld") {
            if (error) *error = "linkedworld_invalid_type";
            return std::nullopt;
        }
        parse_source = *embedded_sklmi;
        manifest.linkedworld_id = detail::extract_string_field(linkedworld_header, "id").value_or(
            detail::extract_string_field(linkedworld_header, "linkedworld_id").value_or(""));
        manifest.linkedworld_version = detail::extract_string_field(linkedworld_header, "version").value_or("");
    } else {
        manifest.linkedworld_id = detail::extract_string_field(text, "linkedworld_id").value_or("");
        manifest.linkedworld_version = detail::extract_string_field(text, "linkedworld_version").value_or("");
    }

    if (detail::contains_forbidden_programming_fields(parse_source)) {
        if (error) *error = "manifest_contains_forbidden_programming_fields";
        return std::nullopt;
    }

    manifest.contract_version =
        detail::extract_string_field(parse_source, "contract_version").value_or(is_linkedworld ? "1.0" : "legacy-bridge-v1");
    manifest.bridge_id = detail::extract_string_field(parse_source, "bridge_id").value_or("default-bridge");
    manifest.driver_instance_id =
        detail::extract_string_field(parse_source, "driver_instance_id").value_or(manifest.bridge_id);
    manifest.source_module = path.filename().string();
    manifest.module = detail::extract_string_field(parse_source, "module").value_or(is_linkedworld ? "linkedworld.sklmi" : "manifest");
    manifest.state_file = detail::extract_string_field(parse_source, "state_file").value_or("");
    manifest.poll_interval_ms = detail::extract_uint_field(parse_source, "poll_interval_ms").value_or(16);
    manifest.core_profile.name = detail::extract_string_field(parse_source, "core_profile").value_or("");
    if (manifest.core_profile.name.empty()) {
        if (const auto core_profile_block = detail::extract_object_field_block(parse_source, "core_profile")) {
            manifest.core_profile.name = detail::extract_string_field(*core_profile_block, "name").value_or("");
            const auto domains = detail::extract_object_blocks(*core_profile_block, "domains");
            for (const auto& block : domains) {
                if (const auto domain_id = detail::extract_string_field(block, "id")) {
                    manifest.core_profile.domain_ids.push_back(*domain_id);
                }
            }
        }
    }
    if (manifest.core_profile.name.empty() && !is_linkedworld) {
        manifest.core_profile.name = "legacy.default";
    }

    for (const auto& block : detail::extract_object_blocks(parse_source, "checks")) {
        WatchRule rule;
        rule.domain_id = detail::extract_string_field(block, "domain_id").value_or("");
        rule.address = detail::extract_uint_field(block, "address").value_or(0);
        rule.size = detail::extract_uint_field(block, "size").value_or(1);
        if (const auto compare = detail::extract_string_field(block, "compare")) {
            const auto parsed = parse_compare_op(*compare);
            if (!parsed.has_value()) {
                if (error) *error = "manifest_check_invalid_compare";
                return std::nullopt;
            }
            rule.compare = *parsed;
        } else {
            rule.compare = CompareOp::equals;
        }
        rule.operand_u64 = detail::extract_uint_field(block, "operand_u64").value_or(
            detail::extract_uint_field(block, "equals_u64").value_or(0));
        if (const auto event_type = detail::extract_string_field(block, "event_type")) {
            const auto parsed = parse_event_type(*event_type);
            if (!parsed.has_value()) {
                if (error) *error = "manifest_check_invalid_event_type";
                return std::nullopt;
            }
            rule.event_type = *parsed;
        } else {
            rule.event_type = EventType::location_checked;
        }
        rule.location_id = detail::extract_uint_field(block, "location_id").value_or(0);
        rule.location_name = detail::extract_string_field(block, "location_name").value_or("");
        rule.event_key = detail::extract_string_field(block, "event_key").value_or("");
        rule.mapped_value = detail::extract_string_field(block, "mapped_value").value_or("");
        if (rule.event_key.empty()) {
            rule.event_key = detail::default_check_event_key(rule);
        }
        if (rule.mapped_value.empty() && !rule.location_name.empty()) {
            rule.mapped_value = rule.location_name;
        }
        manifest.checks.push_back(rule);
    }

    auto injection_blocks = detail::extract_object_blocks(parse_source, "injections");
    const auto action_blocks = detail::extract_object_blocks(parse_source, "actions");
    injection_blocks.insert(injection_blocks.end(), action_blocks.begin(), action_blocks.end());

    for (const auto& block : injection_blocks) {
        InjectRule rule;
        rule.domain_id = detail::extract_string_field(block, "domain_id").value_or("");
        rule.address = detail::extract_uint_field(block, "address").value_or(0);
        rule.size = detail::extract_uint_field(block, "size").value_or(1);
        rule.value_u64 = detail::extract_uint_field(block, "value_u64").value_or(0);
        rule.item_id = detail::extract_uint_field(block, "item_id").value_or(0);
        rule.item_name = detail::extract_string_field(block, "item_name").value_or("");
        rule.event_key = detail::extract_string_field(block, "event_key").value_or("");
        rule.mapped_value = detail::extract_string_field(block, "mapped_value").value_or("");
        rule.room_controlled = detail::extract_bool_field(block, "room_controlled").value_or(false);
        for (const auto& step_block : detail::extract_object_blocks(block, "writes")) {
            InjectRule::WriteStep step;
            step.domain_id = detail::extract_string_field(step_block, "domain_id").value_or("");
            step.address = detail::extract_uint_field(step_block, "address").value_or(0);
            step.value_u64 = detail::extract_uint_field(step_block, "value_u64").value_or(0);
            step.size = detail::extract_uint_field(step_block, "size").value_or(1);
            step.source = detail::extract_string_field(step_block, "source").value_or("constant");
            step.delta_u64 = detail::extract_uint_field(step_block, "delta_u64").value_or(0);
            rule.writes.push_back(std::move(step));
        }
        if (rule.event_key.empty()) {
            rule.event_key = detail::default_injection_event_key(rule);
        }
        if (rule.mapped_value.empty() && !rule.item_name.empty()) {
            rule.mapped_value = rule.item_name;
        }
        manifest.injections.push_back(rule);
    }

    return manifest;
}

}  // namespace

bool validate_bridge_manifest(const BridgeManifest& manifest, std::string* error) {
    if (manifest.linkedworld_id.empty()) {
        if (error) *error = "manifest_missing_linkedworld_id";
        return false;
    }
    if (manifest.bridge_id.empty()) {
        if (error) *error = "manifest_missing_bridge_id";
        return false;
    }
    if (manifest.driver_instance_id.empty()) {
        if (error) *error = "manifest_missing_driver_instance_id";
        return false;
    }
    if (manifest.contract_version.empty()) {
        if (error) *error = "manifest_missing_contract_version";
        return false;
    }
    if (manifest.core_profile.name.empty()) {
        if (error) *error = "manifest_missing_core_profile";
        return false;
    }
    if (manifest.poll_interval_ms == 0) {
        if (error) *error = "manifest_invalid_poll_interval";
        return false;
    }
    if (manifest.checks.empty() && manifest.injections.empty()) {
        if (error) *error = "manifest_missing_bridge_rules";
        return false;
    }

    for (const auto& rule : manifest.checks) {
        if (rule.domain_id.empty()) {
            if (error) *error = "manifest_check_missing_domain_id";
            return false;
        }
        if (!domain_declared(manifest.core_profile, rule.domain_id)) {
            if (error) *error = "manifest_check_unknown_domain";
            return false;
        }
        if (rule.size == 0 || rule.size > 8) {
            if (error) *error = "manifest_check_invalid_size";
            return false;
        }
        if (detail::default_check_event_key(rule).empty()) {
            if (error) *error = "manifest_check_missing_identity";
            return false;
        }
        if (rule.event_type == EventType::location_checked && rule.location_id == 0 && rule.location_name.empty()) {
            if (error) *error = "manifest_location_check_missing_canonical_identity";
            return false;
        }
    }

    for (const auto& rule : manifest.injections) {
        if (detail::default_injection_event_key(rule).empty()) {
            if (error) *error = "manifest_action_missing_identity";
            return false;
        }
        if (rule.writes.empty()) {
            if (rule.domain_id.empty() || rule.size == 0 || rule.size > 8) {
                if (error) *error = "manifest_action_invalid_direct_write";
                return false;
            }
            if (!domain_declared(manifest.core_profile, rule.domain_id)) {
                if (error) *error = "manifest_action_unknown_domain";
                return false;
            }
        } else {
            if (rule.writes.size() > 16) {
                if (error) *error = "manifest_action_sequence_too_large";
                return false;
            }
            for (const auto& step : rule.writes) {
                if (step.domain_id.empty() || step.size == 0 || step.size > 8) {
                    if (error) *error = "manifest_action_invalid_write_step";
                    return false;
                }
                if (!domain_declared(manifest.core_profile, step.domain_id)) {
                    if (error) *error = "manifest_action_unknown_domain";
                    return false;
                }
                if (!is_allowed_write_source(step.source)) {
                    if (error) *error = "manifest_action_invalid_write_source";
                    return false;
                }
            }
        }
    }

    return true;
}

std::optional<BridgeManifest> load_bridge_manifest(const std::filesystem::path& path, std::string* error) {
    std::ifstream input(path);
    if (!input) {
        if (error) *error = "manifest_open_failed";
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    const auto text = buffer.str();

    auto manifest = parse_bridge_manifest_text(text, path, error);
    if (!manifest.has_value()) {
        return std::nullopt;
    }
    if (!validate_bridge_manifest(*manifest, error)) {
        return std::nullopt;
    }
    return manifest;
}

}  // namespace sekailink::sklmi
