#pragma once

#include "sekailink_sklmi/api.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sekailink::sklmi::detail {

std::string trim_copy(std::string value);
std::string strip_quotes(std::string value);
std::optional<std::uint64_t> parse_u64(std::string value);

std::string log_level_name(LogLevel level);
std::string compare_op_name(CompareOp value);
std::string event_type_name(EventType value);
bool evaluate_compare(CompareOp op, std::uint64_t left, std::uint64_t right);

std::string default_check_event_key(const WatchRule& rule);
std::string default_check_value(const WatchRule& rule, std::uint64_t current);
std::string default_injection_event_key(const InjectRule& rule);
std::string default_injection_value(const InjectRule& rule, std::uint64_t value);

Event make_watch_event(const BridgeManifest& manifest, const WatchRule& rule, std::uint64_t current);
Event make_injection_event(const BridgeManifest& manifest, const InjectRule& rule, std::uint64_t value);

std::optional<std::string> extract_string_field(const std::string& text, const std::string& key);
std::optional<bool> extract_bool_field(const std::string& text, const std::string& key);
std::optional<std::uint64_t> extract_uint_field(const std::string& text, const std::string& key);
std::vector<std::string> extract_object_blocks(const std::string& text, const std::string& key);
std::optional<std::string> extract_object_field_block(const std::string& text, const std::string& key);
bool contains_forbidden_programming_fields(const std::string& text);

std::string state_file_escape(std::string value);
std::string escape_json(const std::string& value);
std::string base64_encode_bytes(const std::byte* data, std::size_t size);
std::optional<std::vector<std::byte>> base64_decode_bytes(const std::string& input);

bool parse_state_line(const std::string& line, std::string* kind, std::string* key, std::string* value);

std::optional<std::uint64_t> read_unsigned_from_provider(const MemoryProvider& provider,
                                                         std::string_view domain_id,
                                                         std::uint64_t address,
                                                         std::size_t size);
bool write_unsigned_to_provider(MemoryProvider& provider,
                                std::string_view domain_id,
                                std::uint64_t address,
                                std::uint64_t value,
                                std::size_t size);
bool apply_inject_write(MemoryProvider& provider, const InjectRule::WriteStep& step);

}  // namespace sekailink::sklmi::detail
