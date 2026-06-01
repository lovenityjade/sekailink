#include "api_internal.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <regex>
#include <sstream>

namespace sekailink::sklmi::detail {

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string strip_quotes(std::string value) {
    value = trim_copy(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::optional<std::uint64_t> parse_u64(std::string value) {
    value = trim_copy(std::move(value));
    if (value.empty()) return std::nullopt;
    std::uint64_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const int base = (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) ? 16 : 10;
    auto result = std::from_chars(begin + (base == 16 ? 2 : 0), end, parsed, base);
    if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
    return parsed;
}

std::string log_level_name(LogLevel level) {
    switch (level) {
        case LogLevel::trace: return "trace";
        case LogLevel::info: return "info";
        case LogLevel::warning: return "warning";
        case LogLevel::error: return "error";
    }
    return "info";
}

std::string compare_op_name(CompareOp value) {
    switch (value) {
        case CompareOp::equals: return "equals";
        case CompareOp::not_equals: return "not_equals";
        case CompareOp::greater_than: return "greater_than";
        case CompareOp::greater_or_equal: return "greater_or_equal";
        case CompareOp::less_than: return "less_than";
        case CompareOp::less_or_equal: return "less_or_equal";
        case CompareOp::mask_any: return "mask_any";
        case CompareOp::mask_all: return "mask_all";
    }
    return "equals";
}

std::string event_type_name(EventType value) {
    switch (value) {
        case EventType::location_checked: return "location_checked";
        case EventType::item_received: return "item_received";
        case EventType::map_changed: return "map_changed";
        case EventType::slot_connected: return "slot_connected";
        case EventType::runtime_reset: return "runtime_reset";
        case EventType::trace: return "trace";
        case EventType::error: return "error";
    }
    return "trace";
}

bool evaluate_compare(CompareOp op, std::uint64_t left, std::uint64_t right) {
    switch (op) {
        case CompareOp::equals: return left == right;
        case CompareOp::not_equals: return left != right;
        case CompareOp::greater_than: return left > right;
        case CompareOp::greater_or_equal: return left >= right;
        case CompareOp::less_than: return left < right;
        case CompareOp::less_or_equal: return left <= right;
        case CompareOp::mask_any: return (left & right) != 0;
        case CompareOp::mask_all: return (left & right) == right;
    }
    return false;
}

std::string default_check_event_key(const WatchRule& rule) {
    if (!rule.event_key.empty()) return rule.event_key;
    if (rule.location_id != 0) return std::to_string(rule.location_id);
    return rule.location_name;
}

std::string default_check_value(const WatchRule& rule, std::uint64_t current) {
    if (!rule.mapped_value.empty()) return rule.mapped_value;
    if (!rule.location_name.empty()) return rule.location_name;
    return std::to_string(current);
}

std::string default_injection_event_key(const InjectRule& rule) {
    if (!rule.event_key.empty()) return rule.event_key;
    if (rule.item_id != 0) return std::to_string(rule.item_id);
    return rule.item_name;
}

std::string default_injection_value(const InjectRule& rule, std::uint64_t value) {
    if (!rule.mapped_value.empty()) return rule.mapped_value;
    if (!rule.item_name.empty()) return rule.item_name;
    return std::to_string(value);
}

Event make_watch_event(const BridgeManifest& manifest, const WatchRule& rule, std::uint64_t current) {
    Event event;
    event.type = rule.event_type;
    event.key = default_check_event_key(rule);
    event.value = default_check_value(rule, current);
    event.driver_instance_id = manifest.driver_instance_id;
    event.linkedworld_id = manifest.linkedworld_id;
    event.core_profile = manifest.core_profile.name;
    event.canonical_id = rule.location_id;
    return event;
}

Event make_injection_event(const BridgeManifest& manifest, const InjectRule& rule, std::uint64_t value) {
    Event event;
    event.type = EventType::item_received;
    event.key = default_injection_event_key(rule);
    event.value = default_injection_value(rule, value);
    event.driver_instance_id = manifest.driver_instance_id;
    event.linkedworld_id = manifest.linkedworld_id;
    event.core_profile = manifest.core_profile.name;
    event.canonical_id = rule.item_id;
    return event;
}

std::optional<std::string> extract_string_field(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(text, match, pattern) && match.size() > 1) return match[1].str();
    return std::nullopt;
}

std::optional<bool> extract_bool_field(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() < 2) return std::nullopt;
    return match[1].str() == "true";
}

std::optional<std::uint64_t> extract_uint_field(const std::string& text, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(\"?(0x[0-9A-Fa-f]+|[0-9]+)\"?)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() < 2) return std::nullopt;
    return parse_u64(strip_quotes(match[1].str()));
}

std::vector<std::string> extract_object_blocks(const std::string& text, const std::string& key) {
    std::vector<std::string> blocks;
    const auto key_pos = text.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return blocks;
    const auto open = text.find('[', key_pos);
    if (open == std::string::npos) return blocks;
    int depth = 0;
    std::size_t block_start = std::string::npos;
    for (std::size_t index = open; index < text.size(); ++index) {
        const auto ch = text[index];
        if (ch == '{') {
            if (depth == 0) block_start = index;
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && block_start != std::string::npos) {
                blocks.push_back(text.substr(block_start, index - block_start + 1));
                block_start = std::string::npos;
            }
        } else if (ch == ']' && depth == 0) {
            break;
        }
    }
    return blocks;
}

std::optional<std::string> extract_object_field_block(const std::string& text, const std::string& key) {
    const auto key_pos = text.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return std::nullopt;
    const auto open = text.find('{', key_pos);
    if (open == std::string::npos) return std::nullopt;
    int depth = 0;
    for (std::size_t index = open; index < text.size(); ++index) {
        const auto ch = text[index];
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return text.substr(open, index - open + 1);
            }
        }
    }
    return std::nullopt;
}

bool contains_forbidden_programming_fields(const std::string& text) {
    static constexpr std::string_view kForbiddenKeys[] = {
        "\"script\"",
        "\"lua\"",
        "\"code\"",
        "\"callback\"",
        "\"expression\"",
        "\"program\"",
    };
    return std::any_of(std::begin(kForbiddenKeys), std::end(kForbiddenKeys), [&](std::string_view key) {
        return text.find(key) != std::string::npos;
    });
}

std::string state_file_escape(std::string value) {
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

std::string escape_json(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string base64_encode_bytes(const std::byte* data, std::size_t size) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const auto b0 = static_cast<unsigned>(std::to_integer<unsigned char>(data[i]));
        const auto b1 = (i + 1 < size) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 1])) : 0U;
        const auto b2 = (i + 2 < size) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 2])) : 0U;
        const auto combined = (b0 << 16U) | (b1 << 8U) | b2;
        out.push_back(kAlphabet[(combined >> 18U) & 0x3FU]);
        out.push_back(kAlphabet[(combined >> 12U) & 0x3FU]);
        out.push_back(i + 1 < size ? kAlphabet[(combined >> 6U) & 0x3FU] : '=');
        out.push_back(i + 2 < size ? kAlphabet[combined & 0x3FU] : '=');
    }
    return out;
}

std::optional<std::vector<std::byte>> base64_decode_bytes(const std::string& input) {
    static constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto decode_char = [](char c) -> std::optional<unsigned> {
        const auto pos = kAlphabet.find(c);
        if (pos == std::string_view::npos) return std::nullopt;
        return static_cast<unsigned>(pos);
    };

    if (input.size() % 4 != 0) return std::nullopt;
    std::vector<std::byte> out;
    out.reserve((input.size() / 4) * 3);
    for (std::size_t i = 0; i < input.size(); i += 4) {
        const auto d0 = decode_char(input[i]);
        const auto d1 = decode_char(input[i + 1]);
        if (!d0.has_value() || !d1.has_value()) return std::nullopt;
        const auto d2 = input[i + 2] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 2]);
        const auto d3 = input[i + 3] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 3]);
        if (!d2.has_value() || !d3.has_value()) return std::nullopt;
        const auto combined = (*d0 << 18U) | (*d1 << 12U) | (*d2 << 6U) | *d3;
        out.push_back(static_cast<std::byte>((combined >> 16U) & 0xFFU));
        if (input[i + 2] != '=') out.push_back(static_cast<std::byte>((combined >> 8U) & 0xFFU));
        if (input[i + 3] != '=') out.push_back(static_cast<std::byte>(combined & 0xFFU));
    }
    return out;
}

template <typename T>
std::optional<T> read_value(const MemoryProvider& provider, std::string_view domain_id, std::uint64_t address, Endianness endianness) {
    T value{};
    if (!provider.read(domain_id, address, reinterpret_cast<std::byte*>(&value), sizeof(T))) {
        return std::nullopt;
    }
    if constexpr (sizeof(T) > 1) {
        if ((endianness == Endianness::big) == (std::endian::native == std::endian::little)) {
            auto* bytes = reinterpret_cast<std::byte*>(&value);
            std::reverse(bytes, bytes + sizeof(T));
        }
    }
    return value;
}

std::optional<MemoryDomainDescriptor> find_domain_descriptor(const MemoryProvider& provider, std::string_view domain_id) {
    for (const auto& descriptor : provider.domains()) {
        if (descriptor.id == domain_id) return descriptor;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> read_unsigned_from_provider(const MemoryProvider& provider,
                                                         std::string_view domain_id,
                                                         std::uint64_t address,
                                                         std::size_t size) {
    const auto descriptor = find_domain_descriptor(provider, domain_id);
    if (!descriptor.has_value()) return std::nullopt;
    switch (size) {
        case 1: {
            const auto value = read_value<std::uint8_t>(provider, domain_id, address, descriptor->endianness);
            if (!value.has_value()) return std::nullopt;
            return static_cast<std::uint64_t>(*value);
        }
        case 2: {
            const auto value = read_value<std::uint16_t>(provider, domain_id, address, descriptor->endianness);
            if (!value.has_value()) return std::nullopt;
            return static_cast<std::uint64_t>(*value);
        }
        case 4: {
            const auto value = read_value<std::uint32_t>(provider, domain_id, address, descriptor->endianness);
            if (!value.has_value()) return std::nullopt;
            return static_cast<std::uint64_t>(*value);
        }
        case 8:
            return read_value<std::uint64_t>(provider, domain_id, address, descriptor->endianness);
        default:
            return std::nullopt;
    }
}

bool write_unsigned_to_provider(MemoryProvider& provider,
                                std::string_view domain_id,
                                std::uint64_t address,
                                std::uint64_t value,
                                std::size_t size) {
    const auto descriptor = find_domain_descriptor(provider, domain_id);
    if (!descriptor.has_value() || !descriptor->writable) return false;
    auto write_endian = [&](auto typed_value) {
        using T = decltype(typed_value);
        auto tmp = typed_value;
        if constexpr (sizeof(T) > 1) {
            if ((descriptor->endianness == Endianness::big) == (std::endian::native == std::endian::little)) {
                auto* bytes = reinterpret_cast<std::byte*>(&tmp);
                std::reverse(bytes, bytes + sizeof(T));
            }
        }
        return provider.write(domain_id, address, reinterpret_cast<const std::byte*>(&tmp), sizeof(T));
    };
    switch (size) {
        case 1: return write_endian(static_cast<std::uint8_t>(value));
        case 2: return write_endian(static_cast<std::uint16_t>(value));
        case 4: return write_endian(static_cast<std::uint32_t>(value));
        case 8: return write_endian(static_cast<std::uint64_t>(value));
        default: return false;
    }
}

bool apply_inject_write(MemoryProvider& provider, const InjectRule::WriteStep& step) {
    if (step.source == "current_plus_delta" || step.source == "current_plus") {
        const auto current = read_unsigned_from_provider(provider, step.domain_id, step.address, static_cast<std::size_t>(step.size));
        if (!current.has_value()) return false;
        return write_unsigned_to_provider(provider, step.domain_id, step.address, *current + step.delta_u64,
                                          static_cast<std::size_t>(step.size));
    }
    return write_unsigned_to_provider(provider, step.domain_id, step.address, step.value_u64, static_cast<std::size_t>(step.size));
}

bool parse_state_line(const std::string& line, std::string* kind, std::string* key, std::string* value) {
    const auto first = line.find('|');
    if (first == std::string::npos) return false;
    const auto second = line.find('|', first + 1);
    if (second == std::string::npos) return false;
    if (kind) *kind = line.substr(0, first);
    if (key) *key = line.substr(first + 1, second - first - 1);
    if (value) *value = line.substr(second + 1);
    return true;
}

}  // namespace sekailink::sklmi::detail

namespace sekailink::sklmi {

BufferedForwardingEventSink::BufferedForwardingEventSink(EventSink& inner) : inner_(inner) {}

void BufferedForwardingEventSink::emit(const Event& event) {
    emitted_events_.push_back(event);
    inner_.emit(event);
}

void BufferedForwardingEventSink::trace(const TraceRecord& record) {
    inner_.trace(record);
}

const std::vector<Event>& BufferedForwardingEventSink::emitted_events() const {
    return emitted_events_;
}

void BufferedForwardingEventSink::clear() {
    emitted_events_.clear();
}

void FakeMemoryProvider::add_domain(MemoryDomainDescriptor descriptor) {
    const auto size = descriptor.size_bytes;
    memory_[descriptor.id] = std::vector<std::byte>(size);
    descriptors_[descriptor.id] = std::move(descriptor);
}

bool FakeMemoryProvider::set_domain_bytes(std::string_view domain_id, const std::vector<std::byte>& bytes) {
    auto it = memory_.find(std::string(domain_id));
    if (it == memory_.end() || it->second.size() != bytes.size()) return false;
    it->second = bytes;
    return true;
}

std::vector<MemoryDomainDescriptor> FakeMemoryProvider::domains() const {
    std::vector<MemoryDomainDescriptor> out;
    out.reserve(descriptors_.size());
    for (const auto& [_, descriptor] : descriptors_) out.push_back(descriptor);
    return out;
}

bool FakeMemoryProvider::has_domain(std::string_view domain_id) const {
    return descriptors_.contains(std::string(domain_id));
}

bool FakeMemoryProvider::is_address_valid(std::string_view domain_id, std::uint64_t address, std::size_t size) const {
    const auto it = memory_.find(std::string(domain_id));
    if (it == memory_.end()) return false;
    return address <= it->second.size() && size <= it->second.size() - static_cast<std::size_t>(address);
}

bool FakeMemoryProvider::read(std::string_view domain_id, std::uint64_t address, std::byte* buffer, std::size_t size) const {
    const auto it = memory_.find(std::string(domain_id));
    if (it == memory_.end() || buffer == nullptr || !is_address_valid(domain_id, address, size)) return false;
    std::memcpy(buffer, it->second.data() + address, size);
    return true;
}

bool FakeMemoryProvider::write(std::string_view domain_id, std::uint64_t address, const std::byte* buffer, std::size_t size) {
    const auto descriptor_it = descriptors_.find(std::string(domain_id));
    const auto memory_it = memory_.find(std::string(domain_id));
    if (descriptor_it == descriptors_.end() || memory_it == memory_.end() || buffer == nullptr) return false;
    if (!descriptor_it->second.writable || !is_address_valid(domain_id, address, size)) return false;
    std::memcpy(memory_it->second.data() + address, buffer, size);
    return true;
}

std::optional<std::uint64_t> FakeMemoryProvider::read_unsigned(std::string_view domain_id, std::uint64_t address, std::size_t size) const {
    return detail::read_unsigned_from_provider(*this, domain_id, address, size);
}

bool FakeMemoryProvider::write_unsigned(std::string_view domain_id, std::uint64_t address, std::uint64_t value, std::size_t size) {
    return detail::write_unsigned_to_provider(*this, domain_id, address, value, size);
}

void VectorEventSink::emit(const Event& event) {
    events_.push_back(event);
}

void VectorEventSink::trace(const TraceRecord& record) {
    traces_.push_back(record);
}

const std::vector<Event>& VectorEventSink::events() const {
    return events_;
}

const std::vector<TraceRecord>& VectorEventSink::traces() const {
    return traces_;
}

std::string format_trace_record(const TraceRecord& record) {
    std::ostringstream output;
    output << "[sklmi]"
           << " level=" << detail::log_level_name(record.level)
           << " source=" << record.source
           << " event=" << record.event
           << " tick=" << record.tick_index
           << " ms=" << record.monotonic_ms
           << " detail=" << record.detail;
    return output.str();
}

std::string format_compare_op(CompareOp value) {
    return detail::compare_op_name(value);
}

std::optional<CompareOp> parse_compare_op(std::string_view value) {
    const auto normalized = detail::trim_copy(std::string(value));
    if (normalized == "equals" || normalized == "eq") return CompareOp::equals;
    if (normalized == "not_equals" || normalized == "neq") return CompareOp::not_equals;
    if (normalized == "greater_than" || normalized == "gt") return CompareOp::greater_than;
    if (normalized == "greater_or_equal" || normalized == "gte") return CompareOp::greater_or_equal;
    if (normalized == "less_than" || normalized == "lt") return CompareOp::less_than;
    if (normalized == "less_or_equal" || normalized == "lte") return CompareOp::less_or_equal;
    if (normalized == "mask_any") return CompareOp::mask_any;
    if (normalized == "mask_all") return CompareOp::mask_all;
    return std::nullopt;
}

std::string format_event_type(EventType value) {
    return detail::event_type_name(value);
}

std::optional<EventType> parse_event_type(std::string_view value) {
    const auto normalized = detail::trim_copy(std::string(value));
    if (normalized == "location_checked") return EventType::location_checked;
    if (normalized == "item_received") return EventType::item_received;
    if (normalized == "map_changed") return EventType::map_changed;
    if (normalized == "slot_connected") return EventType::slot_connected;
    if (normalized == "runtime_reset") return EventType::runtime_reset;
    if (normalized == "trace") return EventType::trace;
    if (normalized == "error") return EventType::error;
    return std::nullopt;
}

}  // namespace sekailink::sklmi
