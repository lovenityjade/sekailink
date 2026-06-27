#pragma once

#include "api_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sekailink::sklmi {

inline constexpr std::uint64_t kReadSnapshotMaxBytes = 4096;

inline std::string DescribeRoomMetadataSnapshot(const std::unordered_map<std::string, std::string>& metadata) {
    std::vector<std::string> keys;
    keys.reserve(metadata.size());
    for (const auto& [key, _] : metadata) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());

    std::ostringstream detail;
    detail << "keys=";
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            detail << ",";
        }
        detail << keys[index];
    }

    const auto append_value = [&](std::string_view key) {
        detail << " " << key << "=";
        const auto it = metadata.find(std::string(key));
        if (it == metadata.end() || it->second.empty()) {
            detail << "missing";
            return;
        }
        if (key == "slot_data") {
            detail << "present(" << it->second.size() << "_bytes)";
            return;
        }
        detail << it->second;
    };

    append_value("room_mode");
    append_value("world_id");
    append_value("seed_id");
    append_value("seed_hash");
    append_value("slot_data");
    return detail.str();
}

inline std::string DescribeInjectionRule(const InjectRule& rule) {
    std::ostringstream detail;
    detail << "{item_id=" << rule.item_id
           << ",item_name=" << (rule.item_name.empty() ? "-" : rule.item_name)
           << ",event_key=" << (rule.event_key.empty() ? "-" : rule.event_key)
           << ",mapped_value=" << (rule.mapped_value.empty() ? "-" : rule.mapped_value)
           << ",mode=" << (!rule.dynamic_source.empty() ? rule.dynamic_source : rule.writes.empty() ? "direct" : "write_sequence")
           << "}";
    return detail.str();
}

inline std::string DescribeRoomItem(const RoomItem& item) {
    std::ostringstream detail;
    detail << "delivery_id=" << item.item_id
           << " ap_item_id=" << item.ap_item_id
           << " ap_location_id=" << item.ap_location_id
           << " ap_player_id=" << item.ap_player_id
           << " item_name=" << (item.item_name.empty() ? "-" : item.item_name)
           << " event_key=" << (item.event_key.empty() ? "-" : item.event_key)
           << " mapped_value=" << (item.mapped_value.empty() ? "-" : item.mapped_value);
    return detail.str();
}

inline std::string BuildRoomChatMessagesJson(const std::vector<RoomChatMessage>& messages) {
    std::ostringstream out;
    out << "[";
    for (std::size_t index = 0; index < messages.size(); ++index) {
        if (index != 0) out << ",";
        out << "{"
            << "\"id\":" << messages[index].id << ","
            << "\"author\":\"" << detail::escape_json(messages[index].author) << "\","
            << "\"text\":\"" << detail::escape_json(messages[index].text) << "\","
            << "\"kind\":\"" << detail::escape_json(messages[index].kind.empty() ? "chat" : messages[index].kind) << "\""
            << "}";
    }
    out << "]";
    return out.str();
}

inline std::string RoomReportKey(const Event& event) {
    if (event.type == EventType::location_checked && event.canonical_id != 0) {
        return std::to_string(event.canonical_id);
    }
    return event.key;
}

inline bool IsSelfOriginAlreadyReportedItem(const RoomItem& item,
                                     const std::unordered_map<std::string, std::string>& metadata,
                                     const std::unordered_map<std::string, std::string>& reported_checks) {
    if (item.ap_player_id == 0 || item.ap_location_id == 0) {
        return false;
    }
    const auto slot_it = metadata.find("slot");
    if (slot_it == metadata.end()) {
        return false;
    }
    const auto slot = detail::parse_u64(slot_it->second).value_or(0);
    if (slot == 0 || slot != item.ap_player_id) {
        return false;
    }
    return reported_checks.contains(std::to_string(item.ap_location_id));
}

inline bool IsSelfOriginItem(const RoomItem& item, const std::unordered_map<std::string, std::string>& metadata) {
    if (item.ap_player_id == 0 || item.ap_location_id == 0) {
        return false;
    }
    const auto slot_it = metadata.find("slot");
    if (slot_it == metadata.end()) {
        return false;
    }
    const auto slot = detail::parse_u64(slot_it->second).value_or(0);
    return slot != 0 && slot == item.ap_player_id;
}

inline std::unordered_set<std::string> ParseCheckedLocationSet(std::string_view raw) {
    std::unordered_set<std::string> out;
    std::string token;
    const auto flush = [&]() {
        if (token.empty()) return;
        if (const auto parsed = detail::parse_u64(token); parsed.has_value()) {
            out.insert(std::to_string(*parsed));
        }
        token.clear();
    };
    for (const auto ch : raw) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            token.push_back(ch);
        } else {
            flush();
        }
    }
    flush();
    return out;
}

inline bool ReconcileReportedChecksWithRoomMetadata(std::unordered_map<std::string, std::string>& reported_checks,
                                             const std::unordered_map<std::string, std::string>& metadata) {
    const auto checked_it = metadata.find("checked_locations");
    if (checked_it == metadata.end()) {
        return false;
    }
    const auto server_checked = ParseCheckedLocationSet(checked_it->second);
    bool changed = false;
    for (auto it = reported_checks.begin(); it != reported_checks.end();) {
        const auto normalized = detail::parse_u64(it->first);
        if (normalized.has_value() && !server_checked.contains(std::to_string(*normalized))) {
            it = reported_checks.erase(it);
            changed = true;
            continue;
        }
        ++it;
    }
    return changed;
}

inline std::string RoomItemTrackerKey(const RoomItem& item, std::string_view fallback_key = {}) {
    if (item.ap_item_id != 0 && !item.item_id.empty()) {
        return item.item_id;
    }
    if (!item.event_key.empty()) {
        return item.event_key;
    }
    if (!fallback_key.empty()) {
        return std::string(fallback_key);
    }
    if (item.ap_item_id != 0) {
        return std::to_string(item.ap_item_id);
    }
    return item.item_name;
}

inline std::string RoomItemTrackerValue(const RoomItem& item, std::string_view fallback_value = {}) {
    if (!item.item_name.empty()) {
        return item.item_name;
    }
    if (!item.mapped_value.empty()) {
        return item.mapped_value;
    }
    if (!fallback_value.empty()) {
        return std::string(fallback_value);
    }
    if (!item.event_key.empty()) {
        return item.event_key;
    }
    return std::to_string(item.value_u64);
}

inline Event MakeRoomItemTrackerEvent(const RoomItem& item,
                               std::string_view driver_instance_id,
                               std::string_view linkedworld_id,
                               std::string_view core_profile,
                               std::string_view fallback_key = {},
                               std::string_view fallback_value = {},
                               std::uint64_t fallback_canonical_id = 0) {
    return {
        EventType::item_received,
        RoomItemTrackerKey(item, fallback_key),
        RoomItemTrackerValue(item, fallback_value),
        std::string(driver_instance_id),
        std::string(linkedworld_id),
        std::string(core_profile),
        item.ap_item_id != 0 ? item.ap_item_id : fallback_canonical_id,
        item.ap_player_id,
    };
}

struct ReadSnapshotSpan {
    std::string domain_id;
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    Endianness endianness = Endianness::little;
    std::vector<std::byte> bytes;
    bool readable = false;
};

using DomainDescriptorMap = std::unordered_map<std::string, MemoryDomainDescriptor>;

inline DomainDescriptorMap BuildDomainDescriptorMap(const MemoryProvider& provider) {
    DomainDescriptorMap descriptors;
    for (const auto& descriptor : provider.domains()) {
        descriptors[descriptor.id] = descriptor;
    }
    return descriptors;
}

inline bool IsSupportedUnsignedSize(std::uint64_t size) {
    return size == 1 || size == 2 || size == 4 || size == 8;
}

inline std::vector<ReadSnapshotSpan> BuildCheckReadSnapshotSpans(const BridgeManifest& manifest,
                                                          const DomainDescriptorMap& descriptors) {
    std::unordered_map<std::string, std::vector<std::pair<std::uint64_t, std::uint64_t>>> ranges_by_domain;
    for (const auto& rule : manifest.checks) {
        if (!rule.dynamic_source.empty()) {
            continue;
        }
        if (!IsSupportedUnsignedSize(rule.size)) {
            continue;
        }
        const auto descriptor_it = descriptors.find(rule.domain_id);
        if (descriptor_it == descriptors.end()) {
            continue;
        }
        if (rule.address > descriptor_it->second.size_bytes ||
            rule.size > descriptor_it->second.size_bytes - static_cast<std::size_t>(rule.address)) {
            continue;
        }
        ranges_by_domain[rule.domain_id].push_back({rule.address, rule.address + rule.size});
    }

    std::vector<ReadSnapshotSpan> spans;
    for (auto& [domain_id, ranges] : ranges_by_domain) {
        std::sort(ranges.begin(), ranges.end());
        const auto descriptor = descriptors.at(domain_id);
        for (const auto& [start, end] : ranges) {
            if (spans.empty() || spans.back().domain_id != domain_id ||
                end - spans.back().start > kReadSnapshotMaxBytes) {
                spans.push_back(ReadSnapshotSpan{
                    .domain_id = domain_id,
                    .start = start,
                    .end = end,
                    .endianness = descriptor.endianness,
                });
                continue;
            }
            spans.back().end = std::max(spans.back().end, end);
        }
    }
    return spans;
}

inline void ReadCheckSnapshot(MemoryProvider& provider, std::vector<ReadSnapshotSpan>& spans) {
    for (auto& span : spans) {
        const auto size = static_cast<std::size_t>(span.end - span.start);
        span.bytes.resize(size);
        span.readable = size > 0 && provider.read(span.domain_id, span.start, span.bytes.data(), size);
        if (!span.readable) {
            span.bytes.clear();
        }
    }
}

inline std::optional<std::uint64_t> BytesToUnsigned(const std::byte* bytes, std::size_t size, Endianness endianness) {
    if (bytes == nullptr || !IsSupportedUnsignedSize(size)) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    if (endianness == Endianness::little) {
        for (std::size_t index = 0; index < size; ++index) {
            value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(bytes[index])) << (index * 8U);
        }
        return value;
    }
    for (std::size_t index = 0; index < size; ++index) {
        value = (value << 8U) | std::to_integer<unsigned char>(bytes[index]);
    }
    return value;
}

inline std::optional<std::uint64_t> ReadUnsignedFromSnapshot(const std::vector<ReadSnapshotSpan>& spans,
                                                      const WatchRule& rule) {
    for (const auto& span : spans) {
        if (!span.readable || span.domain_id != rule.domain_id ||
            rule.address < span.start || rule.address + rule.size > span.end) {
            continue;
        }
        const auto offset = static_cast<std::size_t>(rule.address - span.start);
        return BytesToUnsigned(span.bytes.data() + offset, static_cast<std::size_t>(rule.size), span.endianness);
    }
    return std::nullopt;
}

inline bool ContextMappingMatches(const ContextValueMapping& mapping, std::uint64_t current) {
    if (mapping.min_value.has_value() || mapping.max_value.has_value()) {
        if (mapping.min_value.has_value() && current < *mapping.min_value) {
            return false;
        }
        if (mapping.max_value.has_value() && current > *mapping.max_value) {
            return false;
        }
        return true;
    }
    return mapping.value == current;
}

inline std::optional<std::uint64_t> ReadFireredSaveBlock1Flag(MemoryProvider& provider, const WatchRule& rule) {
    if (rule.dynamic_source != "firered_saveblock1_flag" || !rule.dynamic_flag_id.has_value()) {
        return std::nullopt;
    }

    const auto overworld_state = detail::read_unsigned_from_provider(provider, rule.domain_id, 0x03003078u, 1);
    if (!overworld_state.has_value() || *overworld_state != 1u) {
        return std::nullopt;
    }

    const auto sb1_address = detail::read_unsigned_from_provider(provider, rule.domain_id, 0x03004F58u, 4);
    if (!sb1_address.has_value() || *sb1_address < 0x02000000u || *sb1_address >= 0x02040000u) {
        return std::nullopt;
    }

    const auto byte_index = *rule.dynamic_flag_id / 8u;
    const auto bit_index = static_cast<std::uint8_t>(*rule.dynamic_flag_id % 8u);
    const auto flag_byte = detail::read_unsigned_from_provider(provider, rule.domain_id, *sb1_address + 0x1130u + byte_index, 1);
    if (!flag_byte.has_value()) {
        return std::nullopt;
    }
    return (*flag_byte & (1u << bit_index)) != 0 ? 1u : 0u;
}

inline bool ApplyFireredItemQueueInjection(MemoryProvider& provider, const InjectRule& rule, std::uint64_t value) {
    if (rule.dynamic_source != "firered_item_queue") {
        return false;
    }

    const auto overworld_state = detail::read_unsigned_from_provider(provider, rule.domain_id, 0x03003078u, 1);
    if (!overworld_state.has_value() || *overworld_state != 1u) {
        return false;
    }

    const auto sb1_address = detail::read_unsigned_from_provider(provider, rule.domain_id, 0x03004F58u, 4);
    if (!sb1_address.has_value() || *sb1_address < 0x02000000u || *sb1_address >= 0x02040000u) {
        return false;
    }

    const auto received_count = detail::read_unsigned_from_provider(provider, rule.domain_id, *sb1_address + 0x3DE8u, 2);
    const auto pending = detail::read_unsigned_from_provider(provider, rule.domain_id, 0x0203F718u, 1);
    if (!received_count.has_value() || !pending.has_value() || *pending != 0u) {
        return false;
    }

    const auto progress = (*received_count + 1u) & 0xFFFFu;
    return detail::write_unsigned_to_provider(provider, rule.domain_id, 0x0203F714u, value & 0xFFFFu, 2) &&
           detail::write_unsigned_to_provider(provider, rule.domain_id, 0x0203F716u, progress, 2) &&
           detail::write_unsigned_to_provider(provider, rule.domain_id, 0x0203F718u, 1u, 1) &&
           detail::write_unsigned_to_provider(provider, rule.domain_id, 0x0203F719u, 1u, 1);
}

inline const ContextValueMapping* FindContextMapping(const ContextWatchRule& rule, std::uint64_t current) {
    for (const auto& mapping : rule.values) {
        if (ContextMappingMatches(mapping, current)) {
            return &mapping;
        }
    }
    return nullptr;
}

inline std::string ContextWatchStateKey(const ContextWatchRule& rule) {
    if (!rule.context_key.empty()) {
        return rule.context_key;
    }
    return rule.domain_id + ":" + std::to_string(rule.address) + ":" + std::to_string(rule.size);
}

inline bool IsTickDue(std::uint64_t tick_index, std::uint64_t last_tick, std::uint64_t interval) {
    return last_tick == 0 || tick_index <= last_tick || tick_index - last_tick >= interval;
}

}  // namespace sekailink::sklmi
