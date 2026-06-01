#include "tracker_headless_runtime.hpp"

#include "api_internal.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <functional>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace sekailink::sklmi {
namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("tracker_file_open_failed:" + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<std::string> ExtractArrayBlock(const std::string& text, const std::string& key) {
    const auto key_pos = text.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return std::nullopt;
    const auto open = text.find('[', key_pos);
    if (open == std::string::npos) return std::nullopt;
    int depth = 0;
    for (std::size_t index = open; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return text.substr(open, index - open + 1);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> ExtractStringArray(const std::string& text, const std::string& key) {
    const auto block = ExtractArrayBlock(text, key);
    if (!block.has_value()) return {};
    std::vector<std::string> values;
    bool in_string = false;
    bool escaping = false;
    std::string current;
    for (const char ch : *block) {
        if (!in_string) {
            if (ch == '"') {
                in_string = true;
                current.clear();
            }
            continue;
        }
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            values.push_back(current);
            in_string = false;
            continue;
        }
        current.push_back(ch);
    }
    return values;
}

std::vector<std::string> ExtractTopLevelObjectBlocks(const std::string& text) {
    std::vector<std::string> blocks;
    const auto open = text.find('[');
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

bool IsRoomDeliveryKey(std::string_view key) {
    return key.rfind("ap:", 0) == 0 ||
           key.rfind("room:", 0) == 0 ||
           key.rfind("delivery:", 0) == 0;
}

std::optional<std::string> ParseJsonStringLiteralAt(const std::string& text, std::size_t* index) {
    if (index == nullptr || *index >= text.size() || text[*index] != '"') return std::nullopt;
    std::string out;
    ++(*index);
    bool escaping = false;
    while (*index < text.size()) {
        const char ch = text[*index];
        ++(*index);
        if (escaping) {
            if (ch == '"' || ch == '\\' || ch == '/') {
                out.push_back(ch);
            } else if (ch == 'b') {
                out.push_back('\b');
            } else if (ch == 'f') {
                out.push_back('\f');
            } else if (ch == 'n') {
                out.push_back('\n');
            } else if (ch == 'r') {
                out.push_back('\r');
            } else if (ch == 't') {
                out.push_back('\t');
            } else {
                out.push_back(ch);
            }
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            return out;
        }
        out.push_back(ch);
    }
    return std::nullopt;
}

std::string TrimViewCopy(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::optional<std::string> ExtractTopLevelFieldRaw(const std::string& object, const std::string& key) {
    int object_depth = 0;
    int array_depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (std::size_t index = 0; index < object.size(); ++index) {
        const char ch = object[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            if (object_depth == 1 && array_depth == 0) {
                std::size_t key_index = index;
                const auto parsed_key = ParseJsonStringLiteralAt(object, &key_index);
                if (!parsed_key.has_value()) return std::nullopt;
                std::size_t cursor = key_index;
                while (cursor < object.size() && std::isspace(static_cast<unsigned char>(object[cursor])) != 0) {
                    ++cursor;
                }
                if (cursor >= object.size() || object[cursor] != ':') {
                    index = key_index - 1;
                    continue;
                }
                ++cursor;
                while (cursor < object.size() && std::isspace(static_cast<unsigned char>(object[cursor])) != 0) {
                    ++cursor;
                }
                if (*parsed_key != key || cursor >= object.size()) {
                    index = key_index - 1;
                    continue;
                }

                const std::size_t value_start = cursor;
                if (object[cursor] == '"') {
                    std::size_t value_cursor = cursor;
                    if (!ParseJsonStringLiteralAt(object, &value_cursor).has_value()) return std::nullopt;
                    return object.substr(value_start, value_cursor - value_start);
                }

                int value_object_depth = 0;
                int value_array_depth = 0;
                bool value_in_string = false;
                bool value_escaping = false;
                for (; cursor < object.size(); ++cursor) {
                    const char value_ch = object[cursor];
                    if (value_in_string) {
                        if (value_escaping) {
                            value_escaping = false;
                        } else if (value_ch == '\\') {
                            value_escaping = true;
                        } else if (value_ch == '"') {
                            value_in_string = false;
                        }
                        continue;
                    }
                    if (value_ch == '"') {
                        value_in_string = true;
                    } else if (value_ch == '{') {
                        ++value_object_depth;
                    } else if (value_ch == '}') {
                        if (value_object_depth == 0 && value_array_depth == 0) {
                            return TrimViewCopy(std::string_view(object).substr(value_start, cursor - value_start));
                        }
                        --value_object_depth;
                    } else if (value_ch == '[') {
                        ++value_array_depth;
                    } else if (value_ch == ']') {
                        --value_array_depth;
                    } else if (value_ch == ',' && value_object_depth == 0 && value_array_depth == 0) {
                        return TrimViewCopy(std::string_view(object).substr(value_start, cursor - value_start));
                    }
                }
                return TrimViewCopy(std::string_view(object).substr(value_start));
            }
            in_string = true;
            continue;
        }
        if (ch == '{') {
            ++object_depth;
        } else if (ch == '}') {
            --object_depth;
        } else if (ch == '[') {
            ++array_depth;
        } else if (ch == ']') {
            --array_depth;
        }
    }
    return std::nullopt;
}

std::optional<std::string> ExtractTopLevelStringField(const std::string& object, const std::string& key) {
    auto raw = ExtractTopLevelFieldRaw(object, key);
    if (!raw.has_value()) return std::nullopt;
    *raw = TrimViewCopy(*raw);
    if (raw->empty() || raw->front() != '"') return std::nullopt;
    std::size_t index = 0;
    return ParseJsonStringLiteralAt(*raw, &index);
}

std::optional<bool> ExtractTopLevelBoolField(const std::string& object,
                                             const std::string& key,
                                             std::optional<bool> fallback = std::nullopt) {
    auto raw = ExtractTopLevelFieldRaw(object, key);
    if (!raw.has_value()) return fallback;
    *raw = TrimViewCopy(*raw);
    if (*raw == "true") return true;
    if (*raw == "false") return false;
    return fallback;
}

std::uint64_t ExtractTopLevelUintField(const std::string& object,
                                       const std::string& key,
                                       std::uint64_t fallback = 0) {
    auto raw = ExtractTopLevelFieldRaw(object, key);
    if (!raw.has_value()) return fallback;
    *raw = TrimViewCopy(*raw);
    if (raw->empty()) return fallback;
    if (raw->front() == '"') {
        std::size_t index = 0;
        raw = ParseJsonStringLiteralAt(*raw, &index);
        if (!raw.has_value()) return fallback;
    }
    return detail::parse_u64(*raw).value_or(fallback);
}

std::vector<std::string> ParseCodeListFromRaw(const std::optional<std::string>& raw_value) {
    std::vector<std::string> codes;
    if (!raw_value.has_value()) return codes;
    const auto raw = TrimViewCopy(*raw_value);
    auto append_csv = [&](const std::string& value) {
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, ',')) {
            token = detail::trim_copy(token);
            if (!token.empty() && std::find(codes.begin(), codes.end(), token) == codes.end()) {
                codes.push_back(std::move(token));
            }
        }
    };
    if (raw.empty()) return codes;
    if (raw.front() == '"') {
        std::size_t index = 0;
        if (const auto parsed = ParseJsonStringLiteralAt(raw, &index); parsed.has_value()) {
            append_csv(*parsed);
        }
        return codes;
    }
    if (raw.front() == '[') {
        for (std::size_t index = 0; index < raw.size(); ++index) {
            if (raw[index] != '"') continue;
            if (const auto parsed = ParseJsonStringLiteralAt(raw, &index); parsed.has_value()) {
                append_csv(*parsed);
            }
        }
    }
    return codes;
}

std::vector<std::string> ParseTopLevelCodeList(const std::string& object, const std::string& key) {
    return ParseCodeListFromRaw(ExtractTopLevelFieldRaw(object, key));
}

std::string StripLuaLineComments(std::string text) {
    std::string out;
    out.reserve(text.size());
    bool in_string = false;
    bool escaping = false;
    char quote = '\0';
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        const char next = index + 1 < text.size() ? text[index + 1] : '\0';
        if (in_string) {
            out.push_back(ch);
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == quote) {
                in_string = false;
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            in_string = true;
            quote = ch;
            out.push_back(ch);
            continue;
        }
        if (ch == '-' && next == '-') {
            while (index < text.size() && text[index] != '\n') {
                ++index;
            }
            if (index < text.size()) {
                out.push_back('\n');
            }
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::optional<std::size_t> FindMatchingLuaBrace(const std::string& text, std::size_t open_index) {
    if (open_index >= text.size() || text[open_index] != '{') return std::nullopt;
    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    char quote = '\0';
    for (std::size_t index = open_index; index < text.size(); ++index) {
        const char ch = text[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == quote) {
                in_string = false;
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            in_string = true;
            quote = ch;
            continue;
        }
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) return index;
        }
    }
    return std::nullopt;
}

std::vector<std::string> ExtractLuaStringLiterals(const std::string& text) {
    std::vector<std::string> values;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char quote = text[index];
        if (quote != '"' && quote != '\'') continue;
        std::string value;
        bool escaping = false;
        ++index;
        for (; index < text.size(); ++index) {
            const char ch = text[index];
            if (escaping) {
                value.push_back(ch);
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
                continue;
            }
            if (ch == quote) {
                values.push_back(std::move(value));
                break;
            }
            value.push_back(ch);
        }
    }
    return values;
}

std::string StripJsonComments(std::string text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    std::string out;
    out.reserve(text.size());
    bool in_string = false;
    bool escaping = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        const char next = index + 1 < text.size() ? text[index + 1] : '\0';
        if (in_string) {
            out.push_back(ch);
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            out.push_back(ch);
            continue;
        }
        if (ch == '/' && next == '/') {
            while (index < text.size() && text[index] != '\n') {
                ++index;
            }
            if (index < text.size()) {
                out.push_back(text[index]);
            }
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::vector<std::string> ExtractTopLevelObjectKeys(const std::string& object) {
    std::vector<std::string> keys;
    int object_depth = 0;
    int array_depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (std::size_t index = 0; index < object.size(); ++index) {
        const char ch = object[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            if (object_depth == 1 && array_depth == 0) {
                std::size_t key_index = index;
                const auto parsed_key = ParseJsonStringLiteralAt(object, &key_index);
                if (!parsed_key.has_value()) return keys;
                std::size_t cursor = key_index;
                while (cursor < object.size() && std::isspace(static_cast<unsigned char>(object[cursor])) != 0) {
                    ++cursor;
                }
                if (cursor < object.size() && object[cursor] == ':') {
                    if (!parsed_key->empty() && std::find(keys.begin(), keys.end(), *parsed_key) == keys.end()) {
                        keys.push_back(*parsed_key);
                    }
                    index = key_index - 1;
                    continue;
                }
                index = key_index - 1;
            }
            in_string = true;
            continue;
        }
        if (ch == '{') {
            ++object_depth;
        } else if (ch == '}') {
            --object_depth;
        } else if (ch == '[') {
            ++array_depth;
        } else if (ch == ']') {
            --array_depth;
        }
    }
    return keys;
}

void AppendUnique(std::vector<std::string>& values, std::string value) {
    value = detail::trim_copy(std::move(value));
    if (value.empty() || std::find(values.begin(), values.end(), value) != values.end()) return;
    values.push_back(std::move(value));
}

void AppendUniqueList(std::vector<std::string>& values, const std::vector<std::string>& incoming) {
    for (const auto& value : incoming) {
        AppendUnique(values, value);
    }
}

std::optional<std::string> ExtractBracketedValueAt(const std::string& text,
                                                   std::size_t open,
                                                   char open_ch,
                                                   char close_ch) {
    if (open >= text.size() || text[open] != open_ch) return std::nullopt;
    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (std::size_t index = open; index < text.size(); ++index) {
        const char ch = text[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
        } else if (ch == open_ch) {
            ++depth;
        } else if (ch == close_ch) {
            --depth;
            if (depth == 0) {
                return text.substr(open, index - open + 1);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> ExtractAllStringArrayValues(const std::string& text, const std::string& key) {
    std::vector<std::string> values;
    const std::string needle = "\"" + key + "\"";
    for (std::size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) {
        std::size_t cursor = pos + needle.size();
        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= text.size() || text[cursor] != ':') continue;
        ++cursor;
        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= text.size() || text[cursor] != '[') continue;
        const auto block = ExtractBracketedValueAt(text, cursor, '[', ']');
        if (!block.has_value()) continue;
        for (std::size_t index = 0; index < block->size(); ++index) {
            if ((*block)[index] != '"') continue;
            if (const auto parsed = ParseJsonStringLiteralAt(*block, &index); parsed.has_value()) {
                AppendUnique(values, *parsed);
            }
        }
    }
    return values;
}

std::vector<std::string> ExtractAllStringFieldValues(const std::string& text, const std::string& key) {
    std::vector<std::string> values;
    const std::string needle = "\"" + key + "\"";
    for (std::size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) {
        std::size_t cursor = pos + needle.size();
        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= text.size() || text[cursor] != ':') continue;
        ++cursor;
        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= text.size() || text[cursor] != '"') continue;
        if (const auto parsed = ParseJsonStringLiteralAt(text, &cursor); parsed.has_value()) {
            AppendUnique(values, *parsed);
        }
    }
    return values;
}

std::unordered_map<std::string, std::string> BuildLayoutBlocksById(
    const std::vector<TrackerPackLayoutDefinition>& layouts) {
    std::unordered_map<std::string, std::string> blocks_by_id;
    for (const auto& layout : layouts) {
        for (const auto& layout_id : layout.layout_ids) {
            auto block = ExtractTopLevelFieldRaw(layout.json, layout_id);
            if (!block.has_value()) continue;
            *block = detail::trim_copy(*block);
            if (block->empty() || block->front() != '{') continue;
            blocks_by_id[layout_id] = std::move(*block);
        }
    }
    return blocks_by_id;
}

void CollectPackMapsFromLayoutBlock(const std::string& block,
                                    const std::unordered_map<std::string, std::string>& blocks_by_id,
                                    std::vector<std::string>& maps,
                                    std::unordered_set<std::string>& visited_layouts) {
    AppendUniqueList(maps, ExtractAllStringArrayValues(block, "maps"));
    for (const auto& layout_key : ExtractAllStringFieldValues(block, "key")) {
        if (!visited_layouts.insert(layout_key).second) continue;
        const auto block_it = blocks_by_id.find(layout_key);
        if (block_it == blocks_by_id.end()) continue;
        CollectPackMapsFromLayoutBlock(block_it->second, blocks_by_id, maps, visited_layouts);
    }
}

std::vector<std::string> DeriveSupportedPackMaps(const std::filesystem::path& bundle_path,
                                                 const std::vector<TrackerPackLayoutDefinition>& layouts) {
    const auto root_layout = bundle_path / "poptracker-adapted" / "layouts" / "tabs.json";
    if (!std::filesystem::exists(root_layout)) {
        return {};
    }
    auto root_text = detail::trim_copy(StripJsonComments(ReadTextFile(root_layout)));
    if (root_text.empty()) {
        return {};
    }
    const auto blocks_by_id = BuildLayoutBlocksById(layouts);
    std::vector<std::string> maps;
    std::unordered_set<std::string> visited_layouts;
    CollectPackMapsFromLayoutBlock(root_text, blocks_by_id, maps, visited_layouts);
    return maps;
}

bool IsSupportedPackMap(const TrackerBundleModel& bundle, const std::string& pack_map) {
    return bundle.supported_pack_maps.empty() ||
           std::find(bundle.supported_pack_maps.begin(), bundle.supported_pack_maps.end(), pack_map) !=
               bundle.supported_pack_maps.end();
}

std::string NormalizePackAssetPath(std::string path) {
    if (path.empty()) return {};
    while (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    if (path.rfind("poptracker-adapted/", 0) == 0) {
        return path;
    }
    return "poptracker-adapted/" + path;
}

std::string CanonicalToken(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

std::vector<std::string> CanonicalNameWords(std::string_view value) {
    std::vector<std::string> words;
    std::string current;
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            current.push_back(static_cast<char>(std::tolower(ch)));
            continue;
        }
        if (!current.empty()) {
            if (current != "s") {
                words.push_back(std::move(current));
            }
            current.clear();
        }
    }
    if (!current.empty() && current != "s") {
        words.push_back(std::move(current));
    }
    return words;
}

std::string JoinWords(const std::vector<std::string>& words) {
    std::string out;
    for (const auto& word : words) {
        out += word;
    }
    return out;
}

bool IsBroadLocationWord(std::string_view word) {
    static constexpr std::array<std::string_view, 17> broad_words{
        "back",
        "big",
        "bottom",
        "chest",
        "chests",
        "compass",
        "item",
        "items",
        "key",
        "left",
        "map",
        "middle",
        "right",
        "shop",
        "small",
        "top",
        "upgrade",
    };
    return std::find(broad_words.begin(), broad_words.end(), word) != broad_words.end();
}

bool ContainsAllWords(const std::vector<std::string>& haystack,
                      const std::vector<std::string>& needles) {
    if (haystack.empty() || needles.empty()) return false;
    std::size_t needle_chars = 0;
    for (const auto& needle : needles) {
        if (IsBroadLocationWord(needle)) return false;
        needle_chars += needle.size();
        if (std::find(haystack.begin(), haystack.end(), needle) == haystack.end()) {
            return false;
        }
    }
    return needle_chars >= 4;
}

struct MatchedTrackerLocation {
    std::string group_id;
    const TrackerLocationDefinition* location = nullptr;
};

struct TrackerGroupLabel {
    std::string group_id;
    std::string label;
};

void AppendUniqueLocationMatch(std::vector<MatchedTrackerLocation>& matches,
                               std::string group_id,
                               const TrackerLocationDefinition* location) {
    if (location == nullptr || location->location_id == 0) return;
    for (const auto& match : matches) {
        if (match.location != nullptr && match.location->location_id == location->location_id &&
            match.group_id == group_id) {
            return;
        }
    }
    matches.push_back(MatchedTrackerLocation{std::move(group_id), location});
}

bool StrongLocationNameMatch(std::string_view text, std::string_view location_name) {
    const auto text_words = CanonicalNameWords(text);
    const auto location_words = CanonicalNameWords(location_name);
    const auto text_token = JoinWords(text_words);
    const auto location_token = JoinWords(location_words);
    if (text_token.empty() || location_token.empty()) return false;
    if (text_token == location_token) return true;
    if ((text_token.size() >= 5 && location_token.find(text_token) != std::string::npos) ||
        (location_token.size() >= 5 && text_token.find(location_token) != std::string::npos)) {
        return true;
    }
    return ContainsAllWords(text_words, location_words) ||
           ContainsAllWords(location_words, text_words);
}

bool LocationStartsWithLabel(std::string_view label, std::string_view location_name) {
    const auto label_token = JoinWords(CanonicalNameWords(label));
    const auto location_token = JoinWords(CanonicalNameWords(location_name));
    return label_token.size() >= 5 && location_token.rfind(label_token, 0) == 0;
}

struct PopTrackerAccessTargets {
    std::vector<std::string> targets;
    bool had_access_rules = false;
    bool had_location_rule = false;
};

bool IsVisualOnlyRuleFunction(std::string_view part) {
    if (part.rfind("^$", 0) == 0) {
        part.remove_prefix(2);
    }
    return CanonicalToken(part) == "changelocationcolor";
}

PopTrackerAccessTargets ExtractPopTrackerAccessTargets(const std::string& block) {
    PopTrackerAccessTargets result;
    for (const auto& rule : ExtractStringArray(block, "access_rules")) {
        std::stringstream stream(rule);
        std::string part;
        bool saw_function = false;
        bool visual_only_rule = false;
        result.had_access_rules = true;
        while (std::getline(stream, part, '|')) {
            part = detail::trim_copy(std::move(part));
            if (part.empty()) continue;
            if (!saw_function && part.rfind("^$", 0) == 0) {
                visual_only_rule = IsVisualOnlyRuleFunction(part);
                saw_function = true;
                continue;
            }
            if (visual_only_rule) continue;
            result.had_location_rule = true;
            if (part.front() == '@') {
                part.erase(part.begin());
                std::replace(part.begin(), part.end(), '/', ' ');
            }
            if (!part.empty()) AppendUnique(result.targets, std::move(part));
        }
    }
    return result;
}

bool ShouldUseSectionNameFallback(std::string_view section_name, std::string_view group_hint) {
    const auto token = CanonicalToken(section_name);
    if (token.empty()) return false;
    if (token == "chest" || token == "chests") return false;
    if (group_hint.empty()) {
        static constexpr std::array<std::string_view, 8> broad_tokens{
            "bigchest",
            "dungeonchest",
            "dungeonchests",
            "bossitem",
            "keydrop",
            "keydrops",
            "checks",
            "locations",
        };
        return std::find(broad_tokens.begin(), broad_tokens.end(), token) == broad_tokens.end();
    }
    return true;
}

using LocationCatalog = std::vector<MatchedTrackerLocation>;

void AppendStrongLocationMatches(std::vector<MatchedTrackerLocation>& matches,
                                 const LocationCatalog& catalog,
                                 std::string_view text,
                                 std::string_view group_filter = {}) {
    const auto before = matches.size();
    for (const auto& candidate : catalog) {
        if (!group_filter.empty() && candidate.group_id != group_filter) continue;
        if (candidate.location != nullptr &&
            StrongLocationNameMatch(text, candidate.location->location_name)) {
            AppendUniqueLocationMatch(matches, candidate.group_id, candidate.location);
        }
    }
    if (group_filter.empty() || matches.size() != before) {
        return;
    }
    for (const auto& candidate : catalog) {
        if (candidate.location != nullptr &&
            StrongLocationNameMatch(text, candidate.location->location_name)) {
            AppendUniqueLocationMatch(matches, candidate.group_id, candidate.location);
        }
    }
}

void AppendChildPrefixLocationMatches(std::vector<MatchedTrackerLocation>& matches,
                                      const LocationCatalog& catalog,
                                      std::string_view child_name) {
    for (const auto& candidate : catalog) {
        if (candidate.location != nullptr &&
            LocationStartsWithLabel(child_name, candidate.location->location_name)) {
            AppendUniqueLocationMatch(matches, candidate.group_id, candidate.location);
        }
    }
}

std::vector<MatchedTrackerLocation> ResolvePopTrackerChildLocations(const LocationCatalog& catalog,
                                                                    const std::string& child_block,
                                                                    std::string_view child_name,
                                                                    std::string_view group_hint) {
    std::vector<MatchedTrackerLocation> matches;
    const auto child_targets = ExtractPopTrackerAccessTargets(child_block);
    bool has_location_signal = child_targets.had_location_rule;
    bool has_any_access_rules = child_targets.had_access_rules;
    for (const auto& target : child_targets.targets) {
        AppendStrongLocationMatches(matches, catalog, target, group_hint);
    }

    bool has_multi_check_section = false;
    for (const auto& section_block : detail::extract_object_blocks(child_block, "sections")) {
        const auto before_section = matches.size();
        const auto section_targets = ExtractPopTrackerAccessTargets(section_block);
        has_location_signal = has_location_signal || section_targets.had_location_rule;
        has_any_access_rules = has_any_access_rules || section_targets.had_access_rules;
        for (const auto& target : section_targets.targets) {
            AppendStrongLocationMatches(matches, catalog, target, group_hint);
        }
        const auto section_name = detail::extract_string_field(section_block, "name").value_or("");
        if ((section_targets.had_location_rule || !section_targets.had_access_rules) &&
            ShouldUseSectionNameFallback(section_name, group_hint)) {
            AppendStrongLocationMatches(matches, catalog, section_name, group_hint);
        }
        const bool section_allows_name_fallback =
            has_location_signal || (!child_targets.had_access_rules && !section_targets.had_access_rules);
        if (section_allows_name_fallback && matches.size() == before_section) {
            if ((section_targets.had_location_rule || !section_targets.had_access_rules) &&
                ShouldUseSectionNameFallback(section_name, group_hint)) {
                AppendStrongLocationMatches(matches, catalog, section_name, group_hint);
            }
        }
        const auto item_count = detail::extract_uint_field(section_block, "item_count").value_or(0);
        if (item_count > 1) {
            has_multi_check_section = true;
        }
    }

    const bool allows_name_fallback = has_location_signal || !has_any_access_rules;
    if (allows_name_fallback && (has_multi_check_section || matches.empty())) {
        AppendChildPrefixLocationMatches(matches, catalog, child_name);
    }
    if (allows_name_fallback && matches.empty()) {
        AppendStrongLocationMatches(matches, catalog, child_name, group_hint);
    }
    return matches;
}

std::string ResolvePopTrackerGroupHint(const std::vector<TrackerGroupLabel>& groups,
                                       std::string_view child_name,
                                       std::string_view group_name) {
    for (const auto& source : {child_name, group_name}) {
        for (const auto& group : groups) {
            if (StrongLocationNameMatch(source, group.label)) {
                return group.group_id;
            }
        }
    }
    return {};
}

bool RoomMetadataConnected(const std::unordered_map<std::string, std::string>& metadata) {
    const auto found = metadata.find("connected");
    if (found == metadata.end()) return false;
    const auto token = CanonicalToken(TrimViewCopy(found->second));
    return token == "1" || token == "true" || token == "yes" || token == "on" ||
           token == "connected";
}

std::string JsonEscape(std::string_view value) {
    return detail::escape_json(std::string(value));
}

std::string JsonString(std::string_view value) {
    return "\"" + JsonEscape(value) + "\"";
}

void WriteJsonStringArray(std::ostringstream& out, const std::vector<std::string>& values) {
    out << "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) out << ",";
        first = false;
        out << JsonString(value);
    }
    out << "]";
}

std::string ShellEscapeSingleQuotes(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    out.push_back('\'');
    for (const char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
}

std::string BuildLogicStateFileText(const std::filesystem::path& resolved_bundle_path,
                                    const TrackerHeadlessRuntimeState& state,
                                    const TrackerBundleModel& bundle) {
    std::ostringstream output;
    output << "meta|bundle_root|" << detail::state_file_escape(resolved_bundle_path.string()) << "\n";
    output << "meta|variant|" << detail::state_file_escape(state.tracker_variant) << "\n";
    if (!state.slot_data_json.empty()) {
        output << "meta|slot_data|" << detail::state_file_escape(state.slot_data_json) << "\n";
    }

    const auto locations_root = resolved_bundle_path / "poptracker-adapted" / "locations";
    if (std::filesystem::exists(locations_root)) {
        std::vector<std::filesystem::path> location_files;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(locations_root, ec)) {
            if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
            location_files.push_back(entry.path().filename());
        }
        std::sort(location_files.begin(), location_files.end());
        for (const auto& file : location_files) {
            output << "location_file|locations/" << detail::state_file_escape(file.string()) << "|1\n";
        }
    }

    std::vector<std::string> slot_ids;
    slot_ids.reserve(state.item_slots.size());
    for (const auto& [slot_id, _] : state.item_slots) {
        slot_ids.push_back(slot_id);
    }
    std::sort(slot_ids.begin(), slot_ids.end());
    for (const auto& slot_id : slot_ids) {
        const auto& item_state = state.item_slots.at(slot_id);
        output << "slot|" << detail::state_file_escape(slot_id) << "|"
               << (item_state.owned ? "1" : "0") << "|"
               << item_state.stage << "|"
               << item_state.count << "\n";

        std::vector<std::string> sources(item_state.owned_sources.begin(), item_state.owned_sources.end());
        std::sort(sources.begin(), sources.end());
        for (const auto& source : sources) {
            output << "source|" << detail::state_file_escape(slot_id) << "|"
                   << detail::state_file_escape(source) << "\n";
        }
    }
    std::vector<std::string> pack_codes;
    pack_codes.reserve(state.pack_code_states.size());
    for (const auto& [code, _] : state.pack_code_states) {
        pack_codes.push_back(code);
    }
    std::sort(pack_codes.begin(), pack_codes.end());
    for (const auto& code : pack_codes) {
        const auto& item_state = state.pack_code_states.at(code);
        output << "slot|" << detail::state_file_escape(code) << "|"
               << (item_state.owned ? "1" : "0") << "|"
               << item_state.stage << "|"
               << item_state.count << "\n";
    }

    std::vector<std::uint64_t> checked_locations(state.checked_locations.begin(), state.checked_locations.end());
    std::sort(checked_locations.begin(), checked_locations.end());
    for (const auto location_id : checked_locations) {
        output << "check|" << location_id << "|1\n";
    }
    for (const auto& pin : bundle.detailed_pins) {
        if (pin.pack_location_id.empty() || pin.location_id == 0 || pin.location_name.empty()) continue;
        output << "pin_location|" << detail::state_file_escape(pin.pack_location_id) << "|"
               << pin.location_id << "|"
               << detail::state_file_escape(pin.location_name) << "\n";
    }
    return output.str();
}

bool IsSafeArchivePath(const std::filesystem::path& relative_path) {
    if (relative_path.empty() || relative_path.is_absolute()) {
        return false;
    }
    for (const auto& part : relative_path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::filesystem::path StripSingleArchiveRoot(const std::vector<std::filesystem::path>& entries,
                                             const std::filesystem::path& extraction_root) {
    std::filesystem::path common_root;
    bool initialized = false;
    for (const auto& entry : entries) {
        if (entry.empty()) continue;
        const auto first = *entry.begin();
        if (!initialized) {
            common_root = first;
            initialized = true;
            continue;
        }
        if (common_root != first) {
            return extraction_root;
        }
    }
    if (!initialized || common_root.empty()) {
        return extraction_root;
    }
    const auto nested_root = extraction_root / common_root;
    return std::filesystem::exists(nested_root / "manifest.json") ? nested_root : extraction_root;
}

bool ExtractTrackerArchive(const std::filesystem::path& archive_path,
                           const std::filesystem::path& extraction_root,
                           std::filesystem::path* resolved_root,
                           std::string* error) {
    archive* reader = archive_read_new();
    archive_read_support_format_zip(reader);
    archive_read_support_filter_all(reader);

    std::error_code ec;
    std::filesystem::remove_all(extraction_root, ec);
    ec.clear();
    std::filesystem::create_directories(extraction_root, ec);
    if (ec) {
        if (error) *error = "tracker_archive_extract_mkdir_failed";
        archive_read_free(reader);
        return false;
    }

    if (archive_read_open_filename(reader, archive_path.c_str(), 10240) != ARCHIVE_OK) {
        if (error) *error = "tracker_archive_open_failed";
        archive_read_free(reader);
        return false;
    }

    std::vector<std::filesystem::path> extracted_entries;
    archive_entry* entry = nullptr;
    while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
        const char* raw_path = archive_entry_pathname(entry);
        if (raw_path == nullptr) {
            archive_read_data_skip(reader);
            continue;
        }
        std::filesystem::path relative_path(raw_path);
        if (!IsSafeArchivePath(relative_path)) {
            if (error) *error = "tracker_archive_unsafe_path";
            archive_read_free(reader);
            return false;
        }
        const auto output_path = extraction_root / relative_path;
        if (archive_entry_filetype(entry) == AE_IFDIR) {
            std::filesystem::create_directories(output_path, ec);
            if (ec) {
                if (error) *error = "tracker_archive_dir_create_failed";
                archive_read_free(reader);
                return false;
            }
            archive_read_data_skip(reader);
            continue;
        }
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec) {
            if (error) *error = "tracker_archive_parent_create_failed";
            archive_read_free(reader);
            return false;
        }
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "tracker_archive_file_open_failed";
            archive_read_free(reader);
            return false;
        }
        std::array<char, 16384> buffer{};
        while (true) {
            const auto bytes = archive_read_data(reader, buffer.data(), buffer.size());
            if (bytes == 0) break;
            if (bytes < 0) {
                if (error) *error = "tracker_archive_read_failed";
                archive_read_free(reader);
                return false;
            }
            output.write(buffer.data(), bytes);
        }
        extracted_entries.push_back(relative_path);
    }
    archive_read_free(reader);

    if (resolved_root != nullptr) {
        *resolved_root = StripSingleArchiveRoot(extracted_entries, extraction_root);
    }
    return true;
}

bool AtomicWriteTextFile(const std::filesystem::path& path, const std::string& text, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        if (error) *error = "tracker_snapshot_mkdir_failed";
        return false;
    }
    const auto temp_path = path.string() + ".tmp";
    {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "tracker_snapshot_open_failed";
            return false;
        }
        output << text;
        output.flush();
        if (!output) {
            if (error) *error = "tracker_snapshot_write_failed";
            return false;
        }
    }
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temp_path, path, ec);
    }
    if (ec) {
        if (error) *error = "tracker_snapshot_rename_failed";
        return false;
    }
    return true;
}

std::string CanonicalIdentityForItem(const TrackerItemIdentity& item) {
    if (item.item_id != 0) return std::to_string(item.item_id);
    if (!item.event_key.empty()) return item.event_key;
    if (!item.item_name.empty()) return item.item_name;
    return item.mapped_value;
}

std::string DisplayIdentityForItem(const TrackerItemIdentity& item) {
    if (!item.item_name.empty()) return item.item_name;
    if (!item.mapped_value.empty()) return item.mapped_value;
    if (!item.event_key.empty()) return item.event_key;
    if (item.item_id != 0) return std::to_string(item.item_id);
    return {};
}

bool MatchesIdentity(const TrackerItemIdentity& slot_item,
                     std::uint64_t canonical_id,
                     std::string_view event_key,
                     std::string_view value) {
    if (canonical_id != 0 && slot_item.item_id == canonical_id) return true;
    if (!event_key.empty() && slot_item.event_key == event_key) return true;
    if (!value.empty() && (slot_item.item_name == value || slot_item.mapped_value == value)) return true;
    return false;
}

const TrackerTabDefinition* FindTab(const TrackerBundleModel& bundle, std::string_view id) {
    for (const auto& tab : bundle.tabs) {
        if (tab.id == id) return &tab;
    }
    return nullptr;
}

const TrackerPinLayerDefinition* FindPinLayer(const TrackerBundleModel& bundle, std::string_view group_id) {
    for (const auto& layer : bundle.pin_layers) {
        if (layer.group_id == group_id) return &layer;
    }
    return nullptr;
}

std::string CanonicalPathStem(std::string_view path) {
    if (path.empty()) return {};
    return CanonicalToken(std::filesystem::path(std::string(path)).stem().string());
}

std::string ResolvePackMapId(const TrackerBundleModel& bundle, std::string_view pack_map) {
    const auto wanted = CanonicalToken(pack_map);
    if (wanted.empty()) return {};

    for (const auto& map : bundle.maps) {
        if (!map.poptracker_map_name.empty() && CanonicalToken(map.poptracker_map_name) == wanted) {
            return map.id;
        }
    }
    for (const auto& map : bundle.maps) {
        if (CanonicalToken(map.id) == wanted || CanonicalToken(map.label) == wanted) {
            return map.id;
        }
    }

    std::string pack_image_stem;
    for (const auto& map : bundle.pack_maps) {
        if (CanonicalToken(map.name) == wanted) {
            pack_image_stem = CanonicalPathStem(map.image);
            break;
        }
    }
    if (!pack_image_stem.empty()) {
        for (const auto& map : bundle.maps) {
            if (CanonicalPathStem(map.image) == pack_image_stem) {
                return map.id;
            }
        }
    }
    return {};
}

std::string ResolveDetailedPinMapId(const TrackerBundleModel& bundle,
                                    std::string_view pack_map,
                                    std::string_view group_id) {
    if (const auto map_id = ResolvePackMapId(bundle, pack_map); !map_id.empty()) {
        return map_id;
    }
    if (const auto* layer = FindPinLayer(bundle, group_id); layer != nullptr) {
        return layer->map_id;
    }
    return {};
}

const TrackerLocationGroupDefinition* FindLocationGroupByLocation(const TrackerBundleModel& bundle,
                                                                  std::uint64_t canonical_id,
                                                                  std::string_view event_key) {
    for (const auto& group : bundle.location_groups) {
        for (const auto& location : group.locations) {
            if (canonical_id != 0 && location.location_id == canonical_id) return &group;
            if (!event_key.empty() && location.event_key == event_key) return &group;
        }
    }
    return nullptr;
}

const TrackerLocationDefinition* FindLocationByAnyId(const TrackerBundleModel& bundle, std::string_view location_or_group_id) {
    for (const auto& group : bundle.location_groups) {
        for (const auto& location : group.locations) {
            if (std::to_string(location.location_id) == location_or_group_id || location.event_key == location_or_group_id ||
                location.location_name == location_or_group_id) {
                return &location;
            }
        }
    }
    return nullptr;
}

std::optional<bool> ParseCommandBool(const std::string& line, const std::string& key) {
    return detail::extract_bool_field(line, key);
}

std::optional<std::string> ParseCommandString(const std::string& line, const std::string& key) {
    return detail::extract_string_field(line, key);
}

std::optional<std::uint64_t> ParseCommandUint(const std::string& line, const std::string& key) {
    return detail::extract_uint_field(line, key);
}

const TrackerItemIconBindingDefinition* FindItemIconBinding(const TrackerBundleModel& bundle, std::string_view slot_id) {
    for (const auto& binding : bundle.item_icon_bindings) {
        if (binding.slot_id == slot_id) return &binding;
    }
    return nullptr;
}

const TrackerPackItemVisualDefinition* FindPackItemVisual(const TrackerBundleModel& bundle,
                                                          std::string_view code_or_name) {
    const auto raw = std::string(code_or_name);
    const auto canonical = CanonicalToken(code_or_name);
    for (const auto& visual : bundle.pack_item_visuals) {
        if (visual.primary_code == raw || visual.name == raw || CanonicalToken(visual.name) == canonical) {
            return &visual;
        }
        for (const auto& alias : visual.aliases) {
            if (alias == raw || CanonicalToken(alias) == canonical) {
                return &visual;
            }
        }
    }
    return nullptr;
}

const TrackerPackItemVisualDefinition* FindPackItemVisualForSlot(
    const TrackerBundleModel& bundle,
    const TrackerItemSlotDefinition& slot,
    const TrackerItemIconBindingDefinition* icon_binding) {
    auto find_candidate = [&](std::string_view candidate) -> const TrackerPackItemVisualDefinition* {
        if (candidate.empty()) return nullptr;
        return FindPackItemVisual(bundle, candidate);
    };

    if (const auto* visual = find_candidate(slot.slot_id); visual != nullptr) return visual;
    if (const auto* visual = find_candidate(slot.label); visual != nullptr) return visual;
    if (icon_binding != nullptr) {
        if (const auto* visual = find_candidate(icon_binding->icon_key); visual != nullptr) return visual;
        const auto dot = icon_binding->icon_key.find_last_of('.');
        if (dot != std::string::npos) {
            if (const auto* visual = find_candidate(icon_binding->icon_key.substr(dot + 1)); visual != nullptr) {
                return visual;
            }
        }
    }
    for (const auto& item : slot.items) {
        if (const auto* visual = find_candidate(item.item_name); visual != nullptr) return visual;
        if (const auto* visual = find_candidate(item.mapped_value); visual != nullptr) return visual;
        if (const auto* visual = find_candidate(item.event_key); visual != nullptr) return visual;
    }
    return nullptr;
}

bool IsNormalizedItemSlotCode(const TrackerBundleModel& bundle, std::string_view code) {
    for (const auto& slot : bundle.item_slots) {
        if (slot.slot_id == code) {
            return true;
        }
    }
    return false;
}

bool IsKnownPackItemCode(const TrackerBundleModel& bundle, std::string_view code) {
    if (FindPackItemVisual(bundle, code) != nullptr) {
        return true;
    }
    for (const auto& mapping : bundle.pack_item_mappings) {
        if (std::find(mapping.codes.begin(), mapping.codes.end(), code) != mapping.codes.end()) {
            return true;
        }
    }
    return false;
}

std::string PackCodeLabel(const TrackerBundleModel& bundle, std::string_view code) {
    if (const auto* visual = FindPackItemVisual(bundle, code); visual != nullptr && !visual->name.empty()) {
        return visual->name;
    }
    return std::string(code);
}

void ApplyPackCodeStateForBehavior(TrackerItemSlotState& state, std::string_view behavior) {
    if (behavior == "consumable" || behavior == "combined_consumable") {
        state.count += 1;
        state.owned = state.count > 0;
        return;
    }
    if (behavior == "progressive" || behavior == "progressive_toggle" ||
        behavior == "progressive-toggle") {
        state.owned = true;
        state.stage = state.stage == 0 ? 1 : state.stage + 1;
        return;
    }
    state.owned = true;
    state.stage = state.stage == 0 ? 1 : state.stage;
}

const TrackerDetailedPinDefinition* FindDetailedPinByLocationId(const TrackerBundleModel& bundle, std::uint64_t location_id) {
    for (const auto& pin : bundle.detailed_pins) {
        if (pin.location_id == location_id) return &pin;
    }
    return nullptr;
}

std::string FindPackMapAsset(const TrackerBundleModel& bundle, std::string_view pack_map) {
    for (const auto& map : bundle.pack_maps) {
        if (map.name == pack_map) {
            return "poptracker-adapted/" + map.image;
        }
    }
    return {};
}

double ParseDoubleOrZero(const std::string& value) {
    try {
        std::size_t parsed = 0;
        const double result = std::stod(value, &parsed);
        return parsed == value.size() ? result : 0.0;
    } catch (const std::exception&) {
        return 0.0;
    }
}

std::vector<TrackerPackItemVisualDefinition> LoadPackItemVisualDefinitions(const std::filesystem::path& bundle_path) {
    std::vector<TrackerPackItemVisualDefinition> visuals;
    const auto items_root = bundle_path / "poptracker-adapted" / "items";
    if (!std::filesystem::exists(items_root)) {
        return visuals;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(items_root, ec)) {
        if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
        const auto file_text = ReadTextFile(entry.path());
        for (const auto& item_block : ExtractTopLevelObjectBlocks(file_text)) {
            TrackerPackItemVisualDefinition visual;
            visual.name = ExtractTopLevelStringField(item_block, "name").value_or("");
            visual.type = ExtractTopLevelStringField(item_block, "type").value_or("");
            visual.image = NormalizePackAssetPath(ExtractTopLevelStringField(item_block, "img").value_or(""));
            visual.disabled_image =
                NormalizePackAssetPath(ExtractTopLevelStringField(item_block, "disabled_img").value_or(""));
            visual.image_mods = ExtractTopLevelStringField(item_block, "img_mods").value_or("");
            visual.disabled_image_mods = ExtractTopLevelStringField(item_block, "disabled_img_mods").value_or("");
            visual.base_item = ExtractTopLevelStringField(item_block, "base_item").value_or("");
            visual.item_left = ExtractTopLevelStringField(item_block, "item_left").value_or("");
            visual.item_right = ExtractTopLevelStringField(item_block, "item_right").value_or("");
            visual.initial_quantity = ExtractTopLevelUintField(item_block, "initial_quantity", 0);
            visual.min_quantity = ExtractTopLevelUintField(item_block, "min_quantity", 0);
            visual.max_quantity = ExtractTopLevelUintField(item_block, "max_quantity", 0);
            visual.increment = std::max<std::uint64_t>(1, ExtractTopLevelUintField(item_block, "increment", 1));
            visual.initial_stage = ExtractTopLevelUintField(item_block, "initial_stage_idx", 0);
            visual.loop_stages = ExtractTopLevelBoolField(item_block, "loop", false).value_or(false);
            visual.allow_disabled = ExtractTopLevelBoolField(item_block, "allow_disabled", true).value_or(true);
            visual.initial_active_state =
                ExtractTopLevelBoolField(item_block, "initial_active_state", false).value_or(false);
            visual.codes = ParseTopLevelCodeList(item_block, "codes");
            visual.secondary_codes = ParseTopLevelCodeList(item_block, "secondary_codes");
            AppendUniqueList(visual.aliases, visual.codes);
            AppendUniqueList(visual.aliases, visual.secondary_codes);

            for (const auto& stage_block : detail::extract_object_blocks(item_block, "stages")) {
                TrackerPackItemStageDefinition stage;
                stage.image = NormalizePackAssetPath(ExtractTopLevelStringField(stage_block, "img").value_or(""));
                stage.disabled_image =
                    NormalizePackAssetPath(ExtractTopLevelStringField(stage_block, "disabled_img").value_or(""));
                stage.image_mods = ExtractTopLevelStringField(stage_block, "img_mods").value_or("");
                stage.disabled_image_mods = ExtractTopLevelStringField(stage_block, "disabled_img_mods").value_or("");
                stage.codes = ParseTopLevelCodeList(stage_block, "codes");
                stage.secondary_codes = ParseTopLevelCodeList(stage_block, "secondary_codes");
                AppendUniqueList(visual.aliases, stage.codes);
                AppendUniqueList(visual.aliases, stage.secondary_codes);
                visual.stages.push_back(std::move(stage));
            }

            for (const auto& image_block : detail::extract_object_blocks(item_block, "images")) {
                TrackerPackItemImageDefinition image;
                image.image = NormalizePackAssetPath(ExtractTopLevelStringField(image_block, "img").value_or(""));
                image.image_mods = ExtractTopLevelStringField(image_block, "img_mods").value_or("");
                image.left = ExtractTopLevelBoolField(image_block, "left", false).value_or(false);
                image.right = ExtractTopLevelBoolField(image_block, "right", false).value_or(false);
                image.codes = ParseTopLevelCodeList(image_block, "codes");
                image.secondary_codes = ParseTopLevelCodeList(image_block, "secondary_codes");
                AppendUniqueList(visual.aliases, image.codes);
                AppendUniqueList(visual.aliases, image.secondary_codes);
                visual.images.push_back(std::move(image));
            }

            if (visual.aliases.empty() && !visual.name.empty()) {
                AppendUnique(visual.aliases, CanonicalToken(visual.name));
            }
            if (!visual.aliases.empty()) {
                visual.primary_code = visual.aliases.front();
            }
            if (!visual.primary_code.empty() || !visual.name.empty()) {
                visuals.push_back(std::move(visual));
            }
        }
    }
    return visuals;
}

std::vector<TrackerPackItemMappingDefinition> LoadPackItemMappings(const std::filesystem::path& bundle_path) {
    std::vector<TrackerPackItemMappingDefinition> mappings;
    std::vector<std::filesystem::path> candidates;
    const auto preferred = bundle_path / "poptracker-adapted" / "scripts" / "autotracking" / "item_mapping.lua";
    if (std::filesystem::exists(preferred)) {
        candidates.push_back(preferred);
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(bundle_path, ec)) {
        if (ec || !entry.is_regular_file() || entry.path().filename() != "item_mapping.lua") {
            continue;
        }
        if (std::find(candidates.begin(), candidates.end(), entry.path()) == candidates.end()) {
            candidates.push_back(entry.path());
        }
    }

    std::unordered_set<std::uint64_t> seen_item_ids;
    for (const auto& path : candidates) {
        const auto text = StripLuaLineComments(ReadTextFile(path));
        std::size_t cursor = 0;
        while (cursor < text.size()) {
            const auto open_bracket = text.find('[', cursor);
            if (open_bracket == std::string::npos) break;
            const auto close_bracket = text.find(']', open_bracket + 1);
            if (close_bracket == std::string::npos) break;
            const auto raw_id = detail::trim_copy(text.substr(open_bracket + 1, close_bracket - open_bracket - 1));
            const auto item_id = detail::parse_u64(raw_id);
            if (!item_id.has_value()) {
                cursor = close_bracket + 1;
                continue;
            }
            const auto equals = text.find('=', close_bracket + 1);
            if (equals == std::string::npos) break;
            const auto outer_open = text.find('{', equals + 1);
            if (outer_open == std::string::npos) break;
            const auto outer_close = FindMatchingLuaBrace(text, outer_open);
            if (!outer_close.has_value()) break;

            const auto inner_open = text.find('{', outer_open + 1);
            if (inner_open != std::string::npos && inner_open < *outer_close) {
                const auto inner_close = FindMatchingLuaBrace(text, inner_open);
                if (inner_close.has_value() && *inner_close < *outer_close && !seen_item_ids.contains(*item_id)) {
                    TrackerPackItemMappingDefinition mapping;
                    mapping.item_id = *item_id;
                    mapping.codes = ExtractLuaStringLiterals(text.substr(inner_open + 1, *inner_close - inner_open - 1));
                    const auto tail = text.substr(*inner_close + 1, *outer_close - *inner_close - 1);
                    const auto behavior = ExtractLuaStringLiterals(tail);
                    if (!behavior.empty()) {
                        mapping.behavior = behavior.front();
                    }
                    if (!mapping.codes.empty()) {
                        mappings.push_back(std::move(mapping));
                        seen_item_ids.insert(*item_id);
                    }
                }
            }
            cursor = *outer_close + 1;
        }
    }
    return mappings;
}

std::uint64_t VisualStageIndex(const TrackerItemSlotDefinition& slot,
                               const TrackerPackItemVisualDefinition* visual,
                               const TrackerItemSlotState& state) {
    if (visual == nullptr || visual->stages.empty()) {
        return state.stage;
    }
    if (state.stage == 0) {
        return 0;
    }
    if (slot.behavior == "progressive" || slot.behavior == "progressive-toggle" ||
        slot.behavior == "progressive_toggle") {
        if (!visual->allow_disabled || visual->initial_active_state) {
            return std::min<std::uint64_t>(visual->stages.size() - 1, state.stage);
        }
        return std::min<std::uint64_t>(visual->stages.size() - 1, state.stage - 1);
    }
    return std::min<std::uint64_t>(visual->stages.size() - 1, state.stage);
}

std::string SelectPackVisualImage(const TrackerItemSlotDefinition& slot,
                                  const TrackerPackItemVisualDefinition* visual,
                                  const TrackerItemSlotState& state,
                                  bool disabled) {
    if (visual == nullptr) return {};
    if (!visual->stages.empty()) {
        const auto index = static_cast<std::size_t>(VisualStageIndex(slot, visual, state));
        if (disabled && index < visual->stages.size() && !visual->stages[index].disabled_image.empty()) {
            return visual->stages[index].disabled_image;
        }
        if (index < visual->stages.size()) {
            return visual->stages[index].image;
        }
    }
    if (disabled && !visual->disabled_image.empty()) {
        return visual->disabled_image;
    }
    return visual->image;
}

}  // namespace

bool TrackerHeadlessRuntime::Initialize(const TrackerHeadlessRuntimeConfig& config, std::string* error) {
    config_ = config;
    resolved_bundle_path_.clear();
    extracted_bundle_root_.clear();
    bundle_ = TrackerBundleModel{};
    state_ = TrackerHeadlessRuntimeState{};
    command_log_offset_ = 0;
    last_snapshot_json_.clear();
    last_logic_state_text_.clear();
    logic_state_cached_ = false;
    autosave_dirty_ = false;
    if (!ResolveBundlePath(error)) {
        return false;
    }
    if (!LoadBundleModel(resolved_bundle_path_, error)) {
        return false;
    }
    state_.tracker_pack = config.bundle_path.filename().string();
    state_.tracker_variant = config.tracker_variant;
    state_.active_tab_id = bundle_.default_tab_id;
    state_.active_map_id = bundle_.default_map_id;
    for (const auto& slot : bundle_.item_slots) {
        state_.item_slots.emplace(slot.slot_id, TrackerItemSlotState{});
    }
    LoadRoomMetadata();
    LoadAutosaveState();
    return true;
}

void TrackerHeadlessRuntime::SetChatMessageSender(std::function<bool(std::string_view, std::string*)> sender) {
    config_.send_chat_message = std::move(sender);
}

bool TrackerHeadlessRuntime::ResolveBundlePath(std::string* error) {
    resolved_bundle_path_ = config_.bundle_path;
    extracted_bundle_root_.clear();
    if (config_.bundle_path.empty()) {
        if (error) *error = "tracker_bundle_missing";
        return false;
    }
    if (std::filesystem::is_directory(config_.bundle_path)) {
        return true;
    }
    if (!std::filesystem::is_regular_file(config_.bundle_path)) {
        if (error) *error = "tracker_bundle_path_missing";
        return false;
    }
    const auto extension = config_.bundle_path.extension().string();
    if (extension != ".zip" && extension != ".pt" && extension != ".bundle") {
        if (error) *error = "tracker_bundle_path_unsupported";
        return false;
    }

    const auto root = !config_.snapshot_path.empty()
                          ? config_.snapshot_path.parent_path() / "tracker.pack.extracted"
                          : std::filesystem::temp_directory_path() / "sklmi-tracker-pack.extracted";
    extracted_bundle_root_ = root;
    return ExtractTrackerArchive(config_.bundle_path, extracted_bundle_root_, &resolved_bundle_path_, error);
}

void TrackerHeadlessRuntime::LoadRoomMetadata() {
    state_.room_metadata.clear();
    state_.slot_data_json.clear();
    std::vector<std::filesystem::path> candidates;
    if (!config_.room_state_path.empty()) {
        candidates.push_back(config_.room_state_path);
        const auto bridge_root = config_.room_state_path.parent_path();
        const auto runtime_root = bridge_root / "runtime";
        if (std::filesystem::exists(runtime_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(runtime_root, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto filename = entry.path().filename().string();
                if (filename.find(".room-sync.state") != std::string::npos) {
                    candidates.push_back(entry.path());
                }
            }
        }
    }

    for (const auto& path : candidates) {
        if (path.empty() || !std::filesystem::exists(path)) {
            continue;
        }
        std::ifstream input(path);
        if (!input) {
            continue;
        }

        std::string line;
        while (std::getline(input, line)) {
            std::string kind;
            std::string key;
            std::string value;
            if (!detail::parse_state_line(line, &kind, &key, &value) || kind != "meta") continue;
            state_.room_metadata[key] = value;
            if (key == "slot_id" || key == "slot") {
                state_.slot_id = value;
            } else if (key == "seed_id") {
                state_.seed_id = value;
            } else if (key == "seed_name" && state_.seed_id.empty()) {
                state_.seed_id = value;
            } else if (key == "slot_name") {
                state_.slot_name = value;
            } else if (key == "player_alias" || key == "slot_alias") {
                state_.player_alias = value;
            } else if (key == "room_id") {
                state_.room_id = value;
            } else if (key == "slot_data") {
                state_.slot_data_json = value;
            } else if (key == "tracker_pack") {
                state_.tracker_pack = value;
            } else if (key == "tracker_variant" && state_.tracker_variant.empty()) {
                state_.tracker_variant = value;
            }
        }
    }
}

std::filesystem::path TrackerHeadlessRuntime::AutosavePath() const {
    if (!config_.snapshot_path.empty()) {
        return config_.snapshot_path.parent_path() / "tracker.autosave.state";
    }
    if (!config_.command_log_path.empty()) {
        return config_.command_log_path.parent_path() / "tracker.autosave.state";
    }
    return {};
}

void TrackerHeadlessRuntime::LoadAutosaveState() {
    const auto path = AutosavePath();
    if (path.empty() || !std::filesystem::exists(path)) {
        return;
    }
    std::ifstream input(path);
    if (!input) {
        return;
    }

    std::string saved_slot_id;
    std::string saved_seed_id;
    std::string active_tab_id;
    std::string active_map_id;
    std::optional<bool> auto_follow;
    std::unordered_map<std::string, TrackerItemSlotState> item_slots;
    std::unordered_set<std::uint64_t> checked_locations;

    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "meta") {
            if (key == "slot_id") saved_slot_id = value;
            if (key == "seed_id") saved_seed_id = value;
            continue;
        }
        if (kind == "ui") {
            if (key == "active_tab_id") active_tab_id = value;
            if (key == "active_map_id") active_map_id = value;
            if (key == "auto_follow") auto_follow = (value == "1" || value == "true");
            continue;
        }
        if (kind == "item") {
            std::stringstream fields(value);
            std::string owned;
            std::string stage;
            std::string count;
            std::getline(fields, owned, '|');
            std::getline(fields, stage, '|');
            std::getline(fields, count, '|');
            auto& slot = item_slots[key];
            slot.owned = owned == "1" || owned == "true";
            slot.stage = detail::parse_u64(stage).value_or(0);
            slot.count = detail::parse_u64(count).value_or(0);
            continue;
        }
        if (kind == "source") {
            item_slots[key].owned_sources.insert(value);
            continue;
        }
        if (kind == "check") {
            if (auto location_id = detail::parse_u64(key); location_id.has_value() && (value == "1" || value == "true")) {
                checked_locations.insert(*location_id);
            }
        }
    }

    if (!saved_slot_id.empty() && !state_.slot_id.empty() && saved_slot_id != state_.slot_id) {
        return;
    }
    if (!saved_seed_id.empty() && !state_.seed_id.empty() && saved_seed_id != state_.seed_id) {
        return;
    }

    for (auto& [slot_id, saved_state] : item_slots) {
        if (state_.item_slots.contains(slot_id)) {
            state_.item_slots[slot_id] = std::move(saved_state);
        } else if (IsKnownPackItemCode(bundle_, slot_id)) {
            state_.pack_code_states[slot_id] = std::move(saved_state);
        }
    }
    for (const auto location_id : checked_locations) {
        state_.checked_locations.insert(location_id);
    }
    if (!active_tab_id.empty()) state_.active_tab_id = active_tab_id;
    if (!active_map_id.empty()) state_.active_map_id = active_map_id;
    if (auto_follow.has_value()) state_.auto_follow = *auto_follow;
    autosave_dirty_ = false;
}

bool TrackerHeadlessRuntime::SaveAutosaveState(std::string* error) {
    if (!autosave_dirty_) {
        return true;
    }
    const auto path = AutosavePath();
    if (path.empty()) {
        autosave_dirty_ = false;
        return true;
    }

    std::ostringstream output;
    output << "meta|schema|sekailink.tracker.autosave.v1\n";
    output << "meta|slot_id|" << detail::state_file_escape(state_.slot_id) << "\n";
    output << "meta|seed_id|" << detail::state_file_escape(state_.seed_id) << "\n";
    output << "ui|active_tab_id|" << detail::state_file_escape(state_.active_tab_id) << "\n";
    output << "ui|active_map_id|" << detail::state_file_escape(state_.active_map_id) << "\n";
    output << "ui|auto_follow|" << (state_.auto_follow ? "1" : "0") << "\n";
    for (const auto& slot : bundle_.item_slots) {
        const auto state_it = state_.item_slots.find(slot.slot_id);
        if (state_it == state_.item_slots.end()) continue;
        const auto& item_state = state_it->second;
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0 || !item_state.owned_sources.empty())) {
            continue;
        }
        output << "item|" << detail::state_file_escape(slot.slot_id) << "|"
               << (item_state.owned ? "1" : "0") << "|"
               << item_state.stage << "|"
               << item_state.count << "\n";
        for (const auto& source : item_state.owned_sources) {
            output << "source|" << detail::state_file_escape(slot.slot_id) << "|"
                   << detail::state_file_escape(source) << "\n";
        }
    }
    std::vector<std::string> pack_codes;
    pack_codes.reserve(state_.pack_code_states.size());
    for (const auto& [code, _] : state_.pack_code_states) {
        pack_codes.push_back(code);
    }
    std::sort(pack_codes.begin(), pack_codes.end());
    for (const auto& code : pack_codes) {
        const auto& item_state = state_.pack_code_states.at(code);
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0 || !item_state.owned_sources.empty())) {
            continue;
        }
        output << "item|" << detail::state_file_escape(code) << "|"
               << (item_state.owned ? "1" : "0") << "|"
               << item_state.stage << "|"
               << item_state.count << "\n";
    }
    for (const auto location_id : state_.checked_locations) {
        output << "check|" << location_id << "|1\n";
    }

    if (!AtomicWriteTextFile(path, output.str(), error)) {
        return false;
    }
    autosave_dirty_ = false;
    return true;
}

bool TrackerHeadlessRuntime::LoadBundleModel(const std::filesystem::path& bundle_path, std::string* error) {
    try {
        const auto manifest_text = ReadTextFile(bundle_path / "manifest.json");
        const auto item_slots_text = ReadTextFile(bundle_path / "item-slots.complete.json");
        const auto location_groups_text = ReadTextFile(bundle_path / "location-groups.complete.json");
        const auto pin_metadata_text = ReadTextFile(bundle_path / "map-pin-metadata.json");
        const auto surface_text = ReadTextFile(bundle_path / "surface.complete.json");
        const auto item_icon_metadata_text = std::filesystem::exists(bundle_path / "item-icon-metadata.json")
                                                 ? ReadTextFile(bundle_path / "item-icon-metadata.json")
                                                 : std::string{};
        const auto poptracker_maps_path = bundle_path / "poptracker-adapted" / "maps" / "maps.json";
        const auto poptracker_maps_text =
            std::filesystem::exists(poptracker_maps_path) ? ReadTextFile(poptracker_maps_path) : std::string{};
        bundle_.has_pack_location_mapping =
            std::filesystem::exists(bundle_path / "poptracker-adapted" / "scripts" / "autotracking" / "location_mapping.lua");

        bundle_.linkedworld_id = detail::extract_string_field(manifest_text, "linkedworld_id").value_or("");
        bundle_.display_name = detail::extract_string_field(manifest_text, "display_name").value_or(bundle_.linkedworld_id);
        bundle_.default_tab_id = detail::extract_string_field(manifest_text, "default_tab_id").value_or("");
        bundle_.default_map_id = detail::extract_string_field(manifest_text, "default_map_id").value_or("");

        for (const auto& block : detail::extract_object_blocks(manifest_text, "tabs")) {
            TrackerTabDefinition tab;
            tab.id = detail::extract_string_field(block, "id").value_or("");
            tab.label = detail::extract_string_field(block, "label").value_or(tab.id);
            tab.map_id = detail::extract_string_field(block, "map_id")
                             .value_or(detail::extract_string_field(block, "map").value_or(""));
            if (!tab.id.empty()) bundle_.tabs.push_back(std::move(tab));
        }

        for (const auto& block : detail::extract_object_blocks(manifest_text, "maps")) {
            TrackerMapDefinition map;
            map.id = detail::extract_string_field(block, "id").value_or("");
            map.label = detail::extract_string_field(block, "label").value_or(map.id);
            map.poptracker_map_name =
                detail::extract_string_field(block, "poptracker_map_name")
                    .value_or(detail::extract_string_field(block, "pack_map_name")
                                  .value_or(detail::extract_string_field(block, "packMapName").value_or("")));
            map.image = detail::extract_string_field(block, "image").value_or("");
            map.art_origin = detail::extract_string_field(block, "art_origin").value_or("");
            if (!map.id.empty()) bundle_.maps.push_back(std::move(map));
        }

        bundle_.tab_order = ExtractStringArray(surface_text, "tab_order");
        if (bundle_.tab_order.empty()) {
            for (const auto& tab : bundle_.tabs) bundle_.tab_order.push_back(tab.id);
        }

        for (const auto& block : detail::extract_object_blocks(item_slots_text, "slots")) {
            TrackerItemSlotDefinition slot;
            slot.slot_id = detail::extract_string_field(block, "slot_id").value_or("");
            slot.label = detail::extract_string_field(block, "label").value_or(slot.slot_id);
            slot.group_id = detail::extract_string_field(block, "group_id").value_or("");
            slot.behavior = detail::extract_string_field(block, "behavior").value_or("toggle");
            slot.max_stage = detail::extract_uint_field(block, "max_stage").value_or(0);
            for (const auto& item_block : detail::extract_object_blocks(block, "items")) {
                TrackerItemIdentity item;
                item.item_id = detail::extract_uint_field(item_block, "item_id").value_or(0);
                item.event_key = detail::extract_string_field(item_block, "event_key").value_or("");
                item.item_name = detail::extract_string_field(item_block, "item_name").value_or("");
                item.mapped_value = detail::extract_string_field(item_block, "mapped_value").value_or("");
                slot.items.push_back(std::move(item));
            }
            if (!slot.slot_id.empty()) bundle_.item_slots.push_back(std::move(slot));
        }

        for (const auto& block : detail::extract_object_blocks(location_groups_text, "groups")) {
            TrackerLocationGroupDefinition group;
            group.group_id = detail::extract_string_field(block, "group_id").value_or("");
            group.label = detail::extract_string_field(block, "label").value_or(group.group_id);
            group.preferred_tab = detail::extract_string_field(block, "preferred_tab").value_or("");
            for (const auto& location_block : detail::extract_object_blocks(block, "locations")) {
                TrackerLocationDefinition location;
                location.location_id = detail::extract_uint_field(location_block, "location_id").value_or(0);
                location.location_name = detail::extract_string_field(location_block, "location_name").value_or("");
                location.event_key = detail::extract_string_field(location_block, "event_key").value_or("");
                location.domain_id = detail::extract_string_field(location_block, "domain_id").value_or("");
                group.locations.push_back(std::move(location));
            }
            if (!group.group_id.empty()) bundle_.location_groups.push_back(std::move(group));
        }

        for (const auto& block : detail::extract_object_blocks(pin_metadata_text, "pin_layers")) {
            TrackerPinLayerDefinition layer;
            layer.group_id = detail::extract_string_field(block, "group_id").value_or("");
            layer.preferred_tab = detail::extract_string_field(block, "preferred_tab").value_or("");
            layer.map_id = detail::extract_string_field(block, "map_id").value_or("");
            layer.pin_count = detail::extract_uint_field(block, "pin_count").value_or(0);
            layer.pin_kind = detail::extract_string_field(block, "pin_kind").value_or("");
            if (!layer.group_id.empty()) bundle_.pin_layers.push_back(std::move(layer));
        }

        for (const auto& block : detail::extract_object_blocks(item_icon_metadata_text, "icon_groups")) {
            TrackerItemIconGroupDefinition group;
            group.group_id = detail::extract_string_field(block, "group_id").value_or("");
            group.default_palette = detail::extract_string_field(block, "default_palette").value_or("");
            if (!group.group_id.empty()) bundle_.item_icon_groups.push_back(std::move(group));
        }

        std::vector<std::filesystem::path> item_asset_files;
        const auto item_asset_root = bundle_path / "poptracker-adapted" / "images" / "items";
        if (std::filesystem::exists(item_asset_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(item_asset_root, ec)) {
                if (ec) break;
                if (entry.is_regular_file()) item_asset_files.push_back(entry.path());
            }
        }
        for (const auto& block : detail::extract_object_blocks(item_icon_metadata_text, "slot_icon_bindings")) {
            TrackerItemIconBindingDefinition binding;
            binding.slot_id = detail::extract_string_field(block, "slot_id").value_or("");
            binding.icon_key = detail::extract_string_field(block, "icon_key").value_or("");
            binding.render_hint = detail::extract_string_field(block, "render_hint").value_or("");
            if (binding.slot_id.empty()) continue;
            const auto leaf =
                binding.icon_key.find_last_of('.') == std::string::npos
                    ? binding.icon_key
                    : binding.icon_key.substr(binding.icon_key.find_last_of('.') + 1);
            const auto canonical_leaf = CanonicalToken(leaf);
            for (const auto& asset_path : item_asset_files) {
                const auto canonical_asset = CanonicalToken(asset_path.stem().string());
                if (canonical_leaf.empty() || canonical_asset.find(canonical_leaf) == std::string::npos) {
                    continue;
                }
                binding.asset_candidates.push_back(
                    asset_path.lexically_relative(bundle_path).generic_string());
            }
            bundle_.item_icon_bindings.push_back(std::move(binding));
        }

        bundle_.pack_item_visuals = LoadPackItemVisualDefinitions(bundle_path);
        bundle_.pack_item_mappings = LoadPackItemMappings(bundle_path);

        const auto layouts_root = bundle_path / "poptracker-adapted" / "layouts";
        if (std::filesystem::exists(layouts_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(layouts_root, ec)) {
                if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
                auto layout_json = detail::trim_copy(StripJsonComments(ReadTextFile(entry.path())));
                if (layout_json.empty() || layout_json.front() != '{' || layout_json.back() != '}') {
                    continue;
                }
                TrackerPackLayoutDefinition layout;
                layout.file_name = entry.path().filename().string();
                layout.layout_ids = ExtractTopLevelObjectKeys(layout_json);
                layout.json = std::move(layout_json);
                bundle_.pack_layouts.push_back(std::move(layout));
            }
        }
        bundle_.supported_pack_maps = DeriveSupportedPackMaps(bundle_path, bundle_.pack_layouts);

        for (const auto& block : ExtractTopLevelObjectBlocks(poptracker_maps_text)) {
            TrackerPackMapDefinition map;
            map.name = detail::extract_string_field(block, "name").value_or("");
            map.image = detail::extract_string_field(block, "img").value_or(
                detail::extract_string_field(block, "image").value_or(""));
            if (!map.name.empty() && !map.image.empty()) {
                if (map.image.front() == '/') {
                    map.image.erase(map.image.begin());
                }
                bundle_.pack_maps.push_back(std::move(map));
            }
        }

        std::unordered_map<std::string, std::string> map_asset_by_name;
        for (const auto& map : bundle_.pack_maps) {
            map_asset_by_name[map.name] = "poptracker-adapted/" + map.image;
        }
        LocationCatalog location_catalog;
        std::vector<TrackerGroupLabel> group_labels;
        std::unordered_map<std::uint64_t, std::string> group_id_by_location_id;
        for (const auto& group : bundle_.location_groups) {
            if (!group.group_id.empty() && !group.label.empty()) {
                group_labels.push_back(TrackerGroupLabel{group.group_id, group.label});
            }
            for (const auto& location : group.locations) {
                location_catalog.push_back(MatchedTrackerLocation{group.group_id, &location});
                group_id_by_location_id[location.location_id] = group.group_id;
            }
        }
        std::unordered_map<std::string, const TrackerPinLayerDefinition*> layer_by_group;
        for (const auto& layer : bundle_.pin_layers) {
            layer_by_group[layer.group_id] = &layer;
        }

        const auto locations_root = bundle_path / "poptracker-adapted" / "locations";
        if (std::filesystem::exists(locations_root)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(locations_root, ec)) {
                if (ec || !entry.is_regular_file() || entry.path().extension() != ".json") continue;
                const auto file_text = ReadTextFile(entry.path());
                for (const auto& group_block : ExtractTopLevelObjectBlocks(file_text)) {
                    const auto group_name = detail::extract_string_field(group_block, "name").value_or("");
                    std::vector<std::string> child_blocks = detail::extract_object_blocks(group_block, "children");
                    const bool has_children = !child_blocks.empty();
                    if (child_blocks.empty()) child_blocks.push_back(group_block);
                    for (const auto& child_block : child_blocks) {
                        const auto child_name = detail::extract_string_field(child_block, "name").value_or("");
                        const auto pack_location_id = has_children && !group_name.empty()
                                                          ? group_name + "/" + child_name
                                                          : (child_name.empty() ? group_name : child_name);
                        const auto group_hint = ResolvePopTrackerGroupHint(group_labels, child_name, group_name);
                        const auto matched_locations =
                            ResolvePopTrackerChildLocations(location_catalog, child_block, child_name, group_hint);
                        if (matched_locations.empty()) continue;

                        for (const auto& map_location_block : detail::extract_object_blocks(child_block, "map_locations")) {
                            const auto pack_map = detail::extract_string_field(map_location_block, "map").value_or("");
                            if (!IsSupportedPackMap(bundle_, pack_map)) continue;
                            const auto x = detail::extract_uint_field(map_location_block, "x").value_or(0);
                            const auto y = detail::extract_uint_field(map_location_block, "y").value_or(0);
                            const auto size = detail::extract_uint_field(map_location_block, "size").value_or(0);
                            for (const auto& match : matched_locations) {
                                if (match.location == nullptr) continue;
                                const auto group_id = !match.group_id.empty()
                                                          ? match.group_id
                                                          : group_id_by_location_id[match.location->location_id];
                                bool duplicate = false;
                                for (const auto& existing : bundle_.detailed_pins) {
                                    if (existing.group_id == group_id &&
                                        existing.pack_location_id == pack_location_id &&
                                        existing.location_id == match.location->location_id &&
                                        existing.pack_map == pack_map &&
                                        existing.x == static_cast<double>(x) &&
                                        existing.y == static_cast<double>(y)) {
                                        duplicate = true;
                                        break;
                                    }
                                }
                                if (duplicate) continue;

                                TrackerDetailedPinDefinition pin;
                                pin.pin_id = group_id.empty() ? std::to_string(match.location->location_id)
                                                              : group_id + ":" + std::to_string(match.location->location_id);
                                pin.group_id = group_id;
                                pin.pack_location_id = pack_location_id;
                                pin.location_id = match.location->location_id;
                                pin.location_name = match.location->location_name;
                                pin.pack_map = pack_map;
                                pin.map_asset = map_asset_by_name.contains(pack_map)
                                                    ? map_asset_by_name.at(pack_map)
                                                    : std::string{};
                                pin.pin_kind = layer_by_group.contains(group_id)
                                                   ? layer_by_group.at(group_id)->pin_kind
                                                   : std::string{};
                                pin.map_id = ResolveDetailedPinMapId(bundle_, pack_map, group_id);
                                pin.x = static_cast<double>(x);
                                pin.y = static_cast<double>(y);
                                pin.size = static_cast<double>(size);
                                bundle_.detailed_pins.push_back(std::move(pin));
                            }
                        }
                    }
                }
            }
        }

        if (bundle_.tabs.empty()) {
            for (const auto& tab_id : bundle_.tab_order) {
                TrackerTabDefinition tab;
                tab.id = tab_id;
                tab.label = tab_id;
                for (const auto& layer : bundle_.pin_layers) {
                    if (layer.preferred_tab == tab_id && !layer.map_id.empty()) {
                        tab.map_id = layer.map_id;
                        break;
                    }
                }
                bundle_.tabs.push_back(std::move(tab));
            }
        }

        if (bundle_.linkedworld_id.empty() || bundle_.default_tab_id.empty() || bundle_.item_slots.empty() ||
            bundle_.location_groups.empty()) {
            if (error) *error = "tracker_bundle_incomplete";
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        if (error) *error = exception.what();
        return false;
    }
}

void TrackerHeadlessRuntime::ApplyRoomItemIdentity(const TrackerItemIdentity& identity, std::string delivery_id) {
    bool applied = false;
    for (const auto& slot : bundle_.item_slots) {
        for (const auto& slot_item : slot.items) {
            if (!MatchesIdentity(slot_item, identity.item_id, identity.event_key,
                                 !identity.item_name.empty() ? identity.item_name : identity.mapped_value)) {
                continue;
            }
            auto& state = state_.item_slots[slot.slot_id];
            if (slot.behavior == "progressive") {
                const auto max_stage = slot.max_stage == 0 ? 1 : slot.max_stage;
                state.stage = std::min<std::uint64_t>(max_stage, state.stage + 1);
                state.owned = state.stage > 0;
            } else if (slot.behavior == "toggle") {
                state.owned = true;
                state.stage = 1;
            } else if (slot.behavior == "multi-source-toggle") {
                state.owned = true;
                state.stage = 1;
                state.owned_sources.insert(DisplayIdentityForItem(slot_item));
                state.count = state.owned_sources.size();
            } else if (slot.behavior == "counter" || slot.behavior == "resource") {
                state.count += 1;
                state.owned = state.count > 0;
            } else {
                state.owned = true;
                state.stage = state.stage == 0 ? 1 : state.stage;
            }
            state_.last_received_label = slot.label;
            applied = true;
            break;
        }
        if (applied) {
            break;
        }
    }
    const bool pack_applied = ApplyPackItemMapping(identity);
    if (applied || pack_applied) {
        if (!delivery_id.empty()) {
            state_.received_delivery_ids.insert(delivery_id);
        }
        autosave_dirty_ = true;
    }
}

bool TrackerHeadlessRuntime::ApplyPackItemMapping(const TrackerItemIdentity& identity) {
    if (identity.item_id == 0) {
        return false;
    }
    bool applied = false;
    for (const auto& mapping : bundle_.pack_item_mappings) {
        if (mapping.item_id != identity.item_id) {
            continue;
        }
        for (const auto& code : mapping.codes) {
            if (code.empty() || IsNormalizedItemSlotCode(bundle_, code)) {
                continue;
            }
            auto& state = state_.pack_code_states[code];
            ApplyPackCodeStateForBehavior(state, mapping.behavior);
            const auto label = PackCodeLabel(bundle_, code);
            if (!label.empty()) {
                state.owned_sources.insert(label);
                state_.last_received_label = label;
            }
            applied = true;
        }
        break;
    }
    return applied;
}

void TrackerHeadlessRuntime::ApplyLocationCheck(std::uint64_t canonical_id,
                                                std::string_view event_key,
                                                std::string_view value) {
    if (canonical_id != 0) {
        state_.checked_locations.insert(canonical_id);
    } else {
        const auto* location = FindLocationByAnyId(bundle_, std::string(event_key));
        if (location != nullptr && location->location_id != 0) {
            state_.checked_locations.insert(location->location_id);
        }
    }
    state_.last_check_label = std::string(value);
    autosave_dirty_ = true;
}

void TrackerHeadlessRuntime::ApplyEvent(const Event& event) {
    if (event.type == EventType::location_checked) {
        ApplyLocationCheck(event.canonical_id, event.key, event.value);
        return;
    }
    if (event.type != EventType::item_received) {
        return;
    }
    std::string delivery_id;
    if (IsRoomDeliveryKey(event.key)) {
        delivery_id = event.key;
    } else if (event.canonical_id != 0) {
        delivery_id = std::to_string(event.canonical_id);
    } else {
        delivery_id = event.key;
    }
    if (!delivery_id.empty() && state_.received_delivery_ids.contains(delivery_id)) {
        return;
    }
    ApplyRoomItemIdentity({event.canonical_id, event.key, event.value, event.value}, delivery_id);
}

void TrackerHeadlessRuntime::ApplyClickItemCommand(std::string_view slot_id, std::string_view button) {
    auto found = state_.item_slots.find(std::string(slot_id));
    if (found == state_.item_slots.end()) return;
    const auto it = std::find_if(bundle_.item_slots.begin(), bundle_.item_slots.end(),
                                 [&](const auto& slot) { return slot.slot_id == slot_id; });
    if (it == bundle_.item_slots.end()) return;
    auto& state = found->second;
    if (it->behavior == "progressive") {
        const auto max_stage = it->max_stage == 0 ? 1 : it->max_stage;
        if (button == "right") {
            state.stage = state.stage == 0 ? 0 : state.stage - 1;
        } else {
            state.stage = std::min<std::uint64_t>(max_stage, state.stage + 1);
        }
        state.owned = state.stage > 0;
    } else if (it->behavior == "counter" || it->behavior == "resource") {
        if (button == "right") {
            state.count = state.count == 0 ? 0 : state.count - 1;
        } else {
            state.count += 1;
        }
        state.owned = state.count > 0;
    } else {
        state.owned = button == "right" ? false : !state.owned;
        state.stage = state.owned ? 1 : 0;
    }
    autosave_dirty_ = true;
}

void TrackerHeadlessRuntime::ApplyClickPinCommand(std::string_view location_or_group_id, std::string_view button) {
    const auto* location = FindLocationByAnyId(bundle_, location_or_group_id);
    if (location != nullptr && location->location_id != 0) {
        if (button == "right") {
            state_.checked_locations.erase(location->location_id);
        } else {
            state_.checked_locations.insert(location->location_id);
            ApplyLocationCheck(location->location_id, location->event_key, location->location_name);
        }
        autosave_dirty_ = true;
        return;
    }

    for (const auto& group : bundle_.location_groups) {
        if (group.group_id != location_or_group_id) continue;
        if (button == "right") {
            for (const auto& candidate : group.locations) {
                if (state_.checked_locations.erase(candidate.location_id) > 0) {
                    break;
                }
            }
        } else {
            for (const auto& candidate : group.locations) {
                if (!state_.checked_locations.contains(candidate.location_id)) {
                    state_.checked_locations.insert(candidate.location_id);
                    ApplyLocationCheck(candidate.location_id, candidate.event_key, candidate.location_name);
                    break;
                }
            }
        }
        autosave_dirty_ = true;
        return;
    }
}

void TrackerHeadlessRuntime::ApplyCommandLine(const std::string& line) {
    const auto command = ParseCommandString(line, "cmd").value_or("");
    if (command == "tracker.set_tab") {
        const auto tab = ParseCommandString(line, "tab").value_or("");
        if (!tab.empty()) {
            state_.active_tab_id = tab;
            if (state_.auto_follow) {
                state_.active_map_id = ResolveActiveMapId();
            }
            autosave_dirty_ = true;
        }
        return;
    }
    if (command == "tracker.set_auto_follow") {
        state_.auto_follow = ParseCommandBool(line, "enabled").value_or(true);
        if (state_.auto_follow) {
            state_.active_map_id = ResolveActiveMapId();
        }
        autosave_dirty_ = true;
        return;
    }
    if (command == "chat.say") {
        const auto text = ParseCommandString(line, "text").value_or("");
        if (!text.empty() && config_.send_chat_message) {
            std::string error;
            config_.send_chat_message(text, &error);
        }
        return;
    }
    if (command == "tracker.click_item") {
        const auto code = ParseCommandString(line, "code").value_or(ParseCommandString(line, "slot_id").value_or(""));
        const auto button = ParseCommandString(line, "button").value_or("left");
        if (!code.empty()) ApplyClickItemCommand(code, button);
        return;
    }
    if (command == "tracker.click_pin") {
        const auto location_id = ParseCommandUint(line, "location_id");
        const auto group_id = ParseCommandString(line, "group_id");
        const auto button = ParseCommandString(line, "button").value_or("left");
        if (location_id.has_value()) {
            ApplyClickPinCommand(std::to_string(*location_id), button);
        } else if (group_id.has_value()) {
            ApplyClickPinCommand(*group_id, button);
        }
    }
}

void TrackerHeadlessRuntime::PollCommands() {
    if (config_.command_log_path.empty() || !std::filesystem::exists(config_.command_log_path)) {
        return;
    }
    std::ifstream input(config_.command_log_path, std::ios::binary);
    if (!input) return;
    input.seekg(static_cast<std::streamoff>(command_log_offset_), std::ios::beg);
    std::string line;
    while (std::getline(input, line)) {
        ApplyCommandLine(line);
    }
    const auto end = input.tellg();
    if (end >= 0) {
        command_log_offset_ = static_cast<std::uintmax_t>(end);
    } else {
        command_log_offset_ = std::filesystem::file_size(config_.command_log_path);
    }
}

std::string TrackerHeadlessRuntime::ResolveActiveMapId() const {
    if (const auto* tab = FindTab(bundle_, state_.active_tab_id); tab != nullptr && !tab->map_id.empty()) {
        return tab->map_id;
    }
    return state_.active_map_id.empty() ? bundle_.default_map_id : state_.active_map_id;
}

bool TrackerHeadlessRuntime::EvaluatePackLogic(std::string* error) {
    const auto poptracker_root = resolved_bundle_path_ / "poptracker-adapted";
    const auto logic_root = poptracker_root / "scripts" / "logic";
    if (!std::filesystem::exists(logic_root)) {
        state_.logic_ready = false;
        state_.logic_groups.clear();
        state_.pack_location_states.clear();
        state_.pack_section_states.clear();
        state_.runtime_detailed_pins.clear();
        return true;
    }

    const auto script_path = std::filesystem::path(__FILE__).parent_path() / "tracker_poptracker_eval.lua";
    if (!std::filesystem::exists(script_path)) {
        if (error) *error = "tracker_logic_runner_missing";
        return false;
    }

    const auto logic_state_text = BuildLogicStateFileText(resolved_bundle_path_, state_, bundle_);
    if (logic_state_cached_ && logic_state_text == last_logic_state_text_) {
        return true;
    }

    const auto state_path = config_.snapshot_path.parent_path() / "tracker.logic.state";
    const auto output_path = config_.snapshot_path.parent_path() / "tracker.logic.result";
    const auto log_path = config_.snapshot_path.parent_path() / "tracker.logic.log";
    {
        std::ofstream output(state_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "tracker_logic_state_open_failed";
            return false;
        }
        output << logic_state_text;
    }

    std::error_code ec;
    std::filesystem::remove(output_path, ec);

    const std::string command =
        "luajit " + ShellEscapeSingleQuotes(script_path.string()) +
        " " + ShellEscapeSingleQuotes(resolved_bundle_path_.string()) +
        " " + ShellEscapeSingleQuotes(state_path.string()) +
        " " + ShellEscapeSingleQuotes(output_path.string()) +
        " > " + ShellEscapeSingleQuotes(log_path.string()) + " 2>&1";
    const int result = std::system(command.c_str());
    if (result != 0 || !std::filesystem::exists(output_path)) {
        state_.logic_ready = false;
        state_.logic_groups.clear();
        state_.pack_location_states.clear();
        state_.pack_section_states.clear();
        state_.runtime_detailed_pins.clear();
        if (error) *error = "tracker_logic_runner_failed";
        return false;
    }

    state_.logic_groups.clear();
    state_.pack_location_states.clear();
    state_.pack_section_states.clear();
    state_.runtime_detailed_pins.clear();
    std::ifstream input(output_path);
    std::string line;
    while (std::getline(input, line)) {
        std::string kind;
        std::string key;
        std::string value;
        if (!detail::parse_state_line(line, &kind, &key, &value)) continue;
        if (kind == "logic_ready") {
            state_.logic_ready = (key == "1" || value == "1");
            continue;
        }
        if (kind == "pin") {
            std::stringstream fields(value);
            std::string group_id;
            std::string pack_location_id;
            std::string section_id;
            std::string location_id;
            std::string location_name;
            std::string pack_map;
            std::string x;
            std::string y;
            std::string size;
            std::getline(fields, group_id, '|');
            std::getline(fields, pack_location_id, '|');
            std::getline(fields, section_id, '|');
            std::getline(fields, location_id, '|');
            std::getline(fields, location_name, '|');
            std::getline(fields, pack_map, '|');
            std::getline(fields, x, '|');
            std::getline(fields, y, '|');
            std::getline(fields, size, '|');
            auto parsed_location_id = detail::parse_u64(location_id);
            if (key.empty() || pack_location_id.empty() || pack_map.empty() || !parsed_location_id.has_value()) {
                continue;
            }
            TrackerDetailedPinDefinition pin;
            pin.pin_id = key;
            pin.group_id = group_id;
            pin.pack_location_id = pack_location_id;
            pin.section_id = section_id;
            pin.location_id = *parsed_location_id;
            pin.location_name = location_name;
            pin.pack_map = pack_map;
            pin.map_asset = FindPackMapAsset(bundle_, pack_map);
            pin.map_id = ResolveDetailedPinMapId(bundle_, pack_map, group_id);
            if (const auto* layer = FindPinLayer(bundle_, group_id); layer != nullptr) {
                pin.pin_kind = layer->pin_kind;
            }
            if (pin.pin_kind.empty()) {
                pin.pin_kind = "check";
            }
            pin.x = ParseDoubleOrZero(x);
            pin.y = ParseDoubleOrZero(y);
            pin.size = ParseDoubleOrZero(size);
            state_.runtime_detailed_pins.push_back(std::move(pin));
            continue;
        }
        if (kind != "group" && kind != "pack_location" && kind != "section") continue;
        std::stringstream fields(value);
        TrackerLogicGroupState group;
        std::string part;
        std::getline(fields, group.color, '|');
        std::getline(fields, part, '|');
        group.normal_count = detail::parse_u64(part).value_or(0);
        std::getline(fields, part, '|');
        group.sequence_break_count = detail::parse_u64(part).value_or(0);
        std::getline(fields, part, '|');
        group.inspect_count = detail::parse_u64(part).value_or(0);
        std::getline(fields, part, '|');
        group.none_count = detail::parse_u64(part).value_or(0);
        if (kind == "group") {
            state_.logic_groups[key] = std::move(group);
        } else if (kind == "pack_location") {
            state_.pack_location_states[key] = std::move(group);
        } else {
            state_.pack_section_states[key] = std::move(group);
        }
    }

    if (!state_.logic_ready) {
        state_.logic_groups.clear();
        state_.pack_location_states.clear();
        state_.pack_section_states.clear();
        state_.runtime_detailed_pins.clear();
    }
    last_logic_state_text_ = logic_state_text;
    logic_state_cached_ = true;
    return true;
}

struct SnapshotPinCheck {
    std::uint64_t location_id = 0;
    std::string label;
    bool checked = false;
};

struct SnapshotPinSegment {
    std::string section_id;
    std::string label;
    std::string color = "red";
    std::uint64_t normal_count = 0;
    std::uint64_t sequence_break_count = 0;
    std::uint64_t inspect_count = 0;
    std::uint64_t none_count = 0;
    std::vector<SnapshotPinCheck> checks;
};

struct SnapshotGroupedPin {
    TrackerDetailedPinDefinition base;
    std::vector<SnapshotPinSegment> segments;
    std::vector<std::uint64_t> location_ids;
    std::uint64_t checked_count = 0;
    std::uint64_t total_count = 0;
};

std::string PinGroupingKey(const TrackerDetailedPinDefinition& pin) {
    std::ostringstream key;
    key << pin.pack_location_id << '\x1f'
        << pin.map_id << '\x1f'
        << pin.pack_map << '\x1f'
        << pin.map_asset << '\x1f'
        << pin.x << '\x1f'
        << pin.y << '\x1f'
        << pin.size;
    return key.str();
}

std::string LastPathComponent(std::string_view value) {
    const auto slash = value.find_last_of('/');
    if (slash == std::string_view::npos || slash + 1 >= value.size()) {
        return std::string(value);
    }
    return std::string(value.substr(slash + 1));
}

std::string SectionDisplayName(std::string_view section_id, std::string_view pack_location_id) {
    if (!pack_location_id.empty() && section_id.size() > pack_location_id.size() + 1 &&
        section_id.substr(0, pack_location_id.size()) == pack_location_id &&
        section_id[pack_location_id.size()] == '/') {
        return std::string(section_id.substr(pack_location_id.size() + 1));
    }
    return LastPathComponent(section_id);
}

int LogicStateBits(const TrackerLogicGroupState& state) {
    if (state.color == "hidden") return -1;
    if (state.color == "black") return 0;
    int bits = 0;
    if (state.normal_count > 0) bits |= 1;
    if (state.none_count > 0) bits |= 2;
    if (state.sequence_break_count > 0) bits |= 4;
    if (state.inspect_count > 0) bits |= 8;
    if (bits != 0) return bits;
    if (state.color == "green") return 1;
    if (state.color == "red") return 2;
    if (state.color == "yellow") return 4;
    if (state.color == "blue") return 8;
    return 2;
}

void CopyLogicStateToSegment(const TrackerLogicGroupState& state, SnapshotPinSegment* segment) {
    segment->color = state.color;
    segment->normal_count = state.normal_count;
    segment->sequence_break_count = state.sequence_break_count;
    segment->inspect_count = state.inspect_count;
    segment->none_count = state.none_count;
}

std::string SegmentColorForPin(const SnapshotPinSegment& segment) {
    if (!segment.checks.empty()) {
        const auto checked = static_cast<std::uint64_t>(
            std::count_if(segment.checks.begin(), segment.checks.end(), [](const SnapshotPinCheck& check) {
                return check.checked;
            }));
        if (checked >= segment.checks.size()) {
            return "black";
        }
    }
    return segment.color.empty() ? std::string("red") : segment.color;
}

std::string AggregatePinColor(const SnapshotGroupedPin& pin,
                              const TrackerHeadlessRuntimeState& state) {
    if (pin.total_count > 0 && pin.checked_count >= pin.total_count) {
        return "black";
    }
    if (const auto pack_it = state.pack_location_states.find(pin.base.pack_location_id);
        pack_it != state.pack_location_states.end() && pack_it->second.color != "hidden") {
        return pack_it->second.color;
    }
    std::string first_color;
    for (const auto& segment : pin.segments) {
        const auto color = SegmentColorForPin(segment);
        if (color == "hidden") continue;
        if (first_color.empty()) {
            first_color = color;
        } else if (first_color != color) {
            return "mixed";
        }
    }
    return first_color.empty() ? std::string("red") : first_color;
}

std::vector<SnapshotGroupedPin> BuildGroupedSnapshotPins(const TrackerBundleModel& bundle,
                                                        const TrackerHeadlessRuntimeState& state) {
    const bool runtime_pack = !bundle.pack_maps.empty() || !bundle.pack_layouts.empty();
    if (runtime_pack && bundle.has_pack_location_mapping && !state.logic_ready) {
        return {};
    }
    const auto& detailed_pins =
        (state.runtime_detailed_pins.empty() && (!runtime_pack || !bundle.has_pack_location_mapping))
            ? bundle.detailed_pins
            : state.runtime_detailed_pins;
    std::vector<SnapshotGroupedPin> grouped;
    std::unordered_map<std::string, std::size_t> group_index_by_key;
    for (const auto& pin : detailed_pins) {
        if (!IsSupportedPackMap(bundle, pin.pack_map)) continue;
        const auto section_it = state.pack_section_states.find(pin.section_id);
        if (section_it != state.pack_section_states.end() && section_it->second.color == "hidden") {
            continue;
        }
        const auto pack_it = state.pack_location_states.find(pin.pack_location_id);
        if (pack_it != state.pack_location_states.end() && pack_it->second.color == "hidden") {
            continue;
        }

        const auto group_key = PinGroupingKey(pin);
        auto grouped_it = group_index_by_key.find(group_key);
        if (grouped_it == group_index_by_key.end()) {
            grouped_it = group_index_by_key.emplace(group_key, grouped.size()).first;
            SnapshotGroupedPin group;
            group.base = pin;
            std::ostringstream pin_id;
            pin_id << pin.pack_location_id << ":" << pin.pack_map << ":" << pin.x << ":" << pin.y;
            group.base.pin_id = pin_id.str();
            group.base.location_name = LastPathComponent(pin.pack_location_id);
            grouped.push_back(std::move(group));
        }
        auto& group = grouped[grouped_it->second];
        if (std::find(group.location_ids.begin(), group.location_ids.end(), pin.location_id) == group.location_ids.end()) {
            group.location_ids.push_back(pin.location_id);
        }

        auto segment_it = std::find_if(group.segments.begin(), group.segments.end(),
                                       [&](const SnapshotPinSegment& segment) {
                                           return segment.section_id == pin.section_id;
                                       });
        if (segment_it == group.segments.end()) {
            SnapshotPinSegment segment;
            segment.section_id = pin.section_id;
            segment.label = SectionDisplayName(pin.section_id, pin.pack_location_id);
            if (section_it != state.pack_section_states.end()) {
                CopyLogicStateToSegment(section_it->second, &segment);
            } else if (pack_it != state.pack_location_states.end()) {
                CopyLogicStateToSegment(pack_it->second, &segment);
            }
            group.segments.push_back(std::move(segment));
            segment_it = std::prev(group.segments.end());
        }
        if (std::none_of(segment_it->checks.begin(), segment_it->checks.end(), [&](const SnapshotPinCheck& check) {
                return check.location_id == pin.location_id;
            })) {
            const bool checked = state.checked_locations.contains(pin.location_id);
            segment_it->checks.push_back(SnapshotPinCheck{pin.location_id, pin.location_name, checked});
            group.total_count += 1;
            if (checked) {
                group.checked_count += 1;
            }
        }
    }
    for (auto& group : grouped) {
        if (!group.location_ids.empty()) {
            const auto first_unchecked = std::find_if(group.location_ids.begin(), group.location_ids.end(),
                                                      [&](std::uint64_t location_id) {
                                                          return !state.checked_locations.contains(location_id);
                                                      });
            group.base.location_id =
                first_unchecked != group.location_ids.end() ? *first_unchecked : group.location_ids.front();
        }
    }
    return grouped;
}

std::string TrackerHeadlessRuntime::BuildSnapshotJson() const {
    std::ostringstream out;
    const auto active_map_id = ResolveActiveMapId();
    const bool room_connected = RoomMetadataConnected(state_.room_metadata);
    std::size_t total_locations = 0;
    std::size_t checked_locations = 0;
    std::size_t received_count = 0;
    for (const auto& slot : state_.item_slots) {
        if (slot.second.owned || slot.second.stage > 0 || slot.second.count > 0) {
            ++received_count;
        }
    }
    for (const auto& group : bundle_.location_groups) {
        total_locations += group.locations.size();
        for (const auto& location : group.locations) {
            if (state_.checked_locations.contains(location.location_id)) {
                ++checked_locations;
            }
        }
    }

    out << "{";
    out << "\"schema\":\"sekailink.tracker.snapshot.v1\",";
    out << "\"revision\":" << state_.revision << ",";
    out << "\"game\":" << JsonString(bundle_.display_name) << ",";
    out << "\"connected\":" << (room_connected ? "true" : "false") << ",";
    out << "\"ap_connected\":" << (room_connected ? "true" : "false") << ",";
    out << "\"assets_root\":"
        << JsonString(!config_.assets_root.empty() ? config_.assets_root.string() : resolved_bundle_path_.string())
        << ",";
    out << "\"slot\":" << JsonString(state_.slot_id) << ",";
    out << "\"slot_id\":" << JsonString(state_.slot_id) << ",";
    out << "\"seed\":" << JsonString(state_.seed_id) << ",";
    out << "\"seed_id\":" << JsonString(state_.seed_id) << ",";
    out << "\"slot_name\":" << JsonString(state_.slot_name) << ",";
    out << "\"player_alias\":" << JsonString(state_.player_alias) << ",";
    out << "\"room_id\":" << JsonString(state_.room_id) << ",";
    out << "\"active_map\":" << JsonString(active_map_id) << ",";
    out << "\"active_tab\":" << JsonString(state_.active_tab_id) << ",";

    out << "\"room_metadata\":{";
    bool first_meta = true;
    for (const auto& [key, value] : state_.room_metadata) {
        if (key == "slot_data") continue;
        if (key == "chat_messages") continue;
        if (!first_meta) out << ",";
        first_meta = false;
        out << JsonString(key) << ":" << JsonString(value);
    }
    out << "},";

    out << "\"slot_data\":";
    if (!state_.slot_data_json.empty() &&
        (state_.slot_data_json.front() == '{' || state_.slot_data_json.front() == '[')) {
        out << state_.slot_data_json;
    } else {
        out << "{}";
    }
    out << ",";

    out << "\"chat_messages\":";
    if (const auto chat = state_.room_metadata.find("chat_messages");
        chat != state_.room_metadata.end() && !chat->second.empty() && chat->second.front() == '[') {
        out << chat->second;
    } else {
        out << "[]";
    }
    out << ",";

    out << "\"checked_locations\":[";
    bool first = true;
    for (const auto& group : bundle_.location_groups) {
        for (const auto& location : group.locations) {
            if (!state_.checked_locations.contains(location.location_id)) continue;
            if (!first) out << ",";
            first = false;
            out << "{"
                << "\"location_id\":" << location.location_id << ","
                << "\"location_name\":" << JsonString(location.location_name) << ","
                << "\"event_key\":" << JsonString(location.event_key) << ","
                << "\"group_id\":" << JsonString(group.group_id) << ","
                << "\"group_label\":" << JsonString(group.label)
                << "}";
        }
    }
    out << "],";

    out << "\"received_items\":[";
    first = true;
    for (const auto& slot : bundle_.item_slots) {
        const auto state_it = state_.item_slots.find(slot.slot_id);
        if (state_it == state_.item_slots.end()) continue;
        const auto& item_state = state_it->second;
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0)) continue;
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(slot.slot_id) << ","
            << "\"slot_id\":" << JsonString(slot.slot_id) << ","
            << "\"item_name\":" << JsonString(slot.label) << ","
            << "\"group_id\":" << JsonString(slot.group_id) << ","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count
            << "}";
    }
    std::vector<std::string> received_pack_codes;
    received_pack_codes.reserve(state_.pack_code_states.size());
    for (const auto& [code, item_state] : state_.pack_code_states) {
        if (item_state.owned || item_state.stage > 0 || item_state.count > 0) {
            received_pack_codes.push_back(code);
        }
    }
    std::sort(received_pack_codes.begin(), received_pack_codes.end());
    for (const auto& code : received_pack_codes) {
        const auto& item_state = state_.pack_code_states.at(code);
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(code) << ","
            << "\"slot_id\":" << JsonString(code) << ","
            << "\"code\":" << JsonString(code) << ","
            << "\"item_name\":" << JsonString(PackCodeLabel(bundle_, code)) << ","
            << "\"group_id\":\"pack_items\","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count
            << "}";
    }
    out << "],";

    out << "\"tabs\":[";
    first = true;
    for (const auto& tab_id : bundle_.tab_order) {
        const auto* tab = FindTab(bundle_, tab_id);
        if (tab == nullptr) continue;
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(tab->id) << ","
            << "\"label\":" << JsonString(tab->label) << ","
            << "\"map_id\":" << JsonString(tab->map_id) << ","
            << "\"active\":" << (tab->id == state_.active_tab_id ? "true" : "false")
            << "}";
    }
    out << "],";

    out << "\"maps\":[";
    first = true;
    for (const auto& map : bundle_.maps) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(map.id) << ","
            << "\"label\":" << JsonString(map.label) << ","
            << "\"image\":" << JsonString(map.image) << ","
            << "\"art_origin\":" << JsonString(map.art_origin)
            << "}";
    }
    out << "],";

    out << "\"pack_maps\":[";
    first = true;
    for (const auto& map : bundle_.pack_maps) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"name\":" << JsonString(map.name) << ","
            << "\"image\":" << JsonString(map.image)
            << "}";
    }
    out << "],";

    out << "\"pack_layouts\":[";
    first = true;
    for (const auto& layout : bundle_.pack_layouts) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"file\":" << JsonString(layout.file_name) << ","
            << "\"layout_ids\":";
        WriteJsonStringArray(out, layout.layout_ids);
        out << ",\"json\":";
        if (!layout.json.empty() && layout.json.front() == '{' && layout.json.back() == '}') {
            out << layout.json;
        } else {
            out << "{}";
        }
        out << "}";
    }
    out << "],";

    out << "\"item_icon_groups\":[";
    first = true;
    for (const auto& group : bundle_.item_icon_groups) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"group_id\":" << JsonString(group.group_id) << ","
            << "\"default_palette\":" << JsonString(group.default_palette)
            << "}";
    }
    out << "],";

    out << "\"pack_item_visuals\":[";
    first = true;
    for (const auto& visual : bundle_.pack_item_visuals) {
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"primary_code\":" << JsonString(visual.primary_code) << ","
            << "\"name\":" << JsonString(visual.name) << ","
            << "\"type\":" << JsonString(visual.type) << ","
            << "\"image\":" << JsonString(visual.image) << ","
            << "\"disabled_image\":" << JsonString(visual.disabled_image) << ","
            << "\"image_mods\":" << JsonString(visual.image_mods) << ","
            << "\"disabled_image_mods\":" << JsonString(visual.disabled_image_mods) << ","
            << "\"base_item\":" << JsonString(visual.base_item) << ","
            << "\"item_left\":" << JsonString(visual.item_left) << ","
            << "\"item_right\":" << JsonString(visual.item_right) << ","
            << "\"initial_quantity\":" << visual.initial_quantity << ","
            << "\"min_quantity\":" << visual.min_quantity << ","
            << "\"max_quantity\":" << visual.max_quantity << ","
            << "\"increment\":" << visual.increment << ","
            << "\"initial_stage\":" << visual.initial_stage << ","
            << "\"loop_stages\":" << (visual.loop_stages ? "true" : "false") << ","
            << "\"allow_disabled\":" << (visual.allow_disabled ? "true" : "false") << ","
            << "\"initial_active_state\":" << (visual.initial_active_state ? "true" : "false") << ","
            << "\"codes\":";
        WriteJsonStringArray(out, visual.codes);
        out << ",\"secondary_codes\":";
        WriteJsonStringArray(out, visual.secondary_codes);
        out << ",\"aliases\":";
        WriteJsonStringArray(out, visual.aliases);
        out << ",\"stages\":[";
        bool first_stage = true;
        for (const auto& stage : visual.stages) {
            if (!first_stage) out << ",";
            first_stage = false;
            out << "{"
                << "\"image\":" << JsonString(stage.image) << ","
                << "\"disabled_image\":" << JsonString(stage.disabled_image) << ","
                << "\"image_mods\":" << JsonString(stage.image_mods) << ","
                << "\"disabled_image_mods\":" << JsonString(stage.disabled_image_mods) << ","
                << "\"codes\":";
            WriteJsonStringArray(out, stage.codes);
            out << ",\"secondary_codes\":";
            WriteJsonStringArray(out, stage.secondary_codes);
            out << "}";
        }
        out << "],\"images\":[";
        bool first_image = true;
        for (const auto& image : visual.images) {
            if (!first_image) out << ",";
            first_image = false;
            out << "{"
                << "\"image\":" << JsonString(image.image) << ","
                << "\"image_mods\":" << JsonString(image.image_mods) << ","
                << "\"left\":" << (image.left ? "true" : "false") << ","
                << "\"right\":" << (image.right ? "true" : "false") << ","
                << "\"codes\":";
            WriteJsonStringArray(out, image.codes);
            out << ",\"secondary_codes\":";
            WriteJsonStringArray(out, image.secondary_codes);
            out << "}";
        }
        out << "]"
            << "}";
    }
    out << "],";

    out << "\"items\":[";
    first = true;
    for (const auto& slot : bundle_.item_slots) {
        const auto state_it = state_.item_slots.find(slot.slot_id);
        const auto empty_state = TrackerItemSlotState{};
        const auto& item_state = state_it != state_.item_slots.end() ? state_it->second : empty_state;
        const auto* icon_binding = FindItemIconBinding(bundle_, slot.slot_id);
        const auto* pack_visual = FindPackItemVisualForSlot(bundle_, slot, icon_binding);
        const bool item_disabled = !(item_state.owned || item_state.stage > 0 || item_state.count > 0);
        const auto visual_stage_index = VisualStageIndex(slot, pack_visual, item_state);
        const auto visual_image = SelectPackVisualImage(slot, pack_visual, item_state, item_disabled);
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(slot.slot_id) << ","
            << "\"code\":" << JsonString(slot.slot_id) << ","
            << "\"label\":" << JsonString(slot.label) << ","
            << "\"group_id\":" << JsonString(slot.group_id) << ","
            << "\"behavior\":" << JsonString(slot.behavior) << ","
            << "\"icon_key\":" << JsonString(icon_binding != nullptr ? icon_binding->icon_key : std::string{}) << ","
            << "\"render_hint\":" << JsonString(icon_binding != nullptr ? icon_binding->render_hint : std::string{}) << ","
            << "\"pack_visual_code\":" << JsonString(pack_visual != nullptr ? pack_visual->primary_code : std::string{}) << ","
            << "\"image\":" << JsonString(visual_image) << ","
            << "\"stage_index\":" << visual_stage_index << ","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count << ","
            << "\"acquired\":" << (item_state.owned ? "true" : "false") << ","
            << "\"asset_candidates\":[";
        bool first_asset = true;
        if (icon_binding != nullptr) {
            for (const auto& asset : icon_binding->asset_candidates) {
                if (!first_asset) out << ",";
                first_asset = false;
                out << JsonString(asset);
            }
        }
        out << "]"
            << "}";
    }
    std::vector<std::string> active_pack_codes;
    active_pack_codes.reserve(state_.pack_code_states.size());
    for (const auto& [code, item_state] : state_.pack_code_states) {
        if (item_state.owned || item_state.stage > 0 || item_state.count > 0) {
            active_pack_codes.push_back(code);
        }
    }
    std::sort(active_pack_codes.begin(), active_pack_codes.end());
    for (const auto& code : active_pack_codes) {
        const auto& item_state = state_.pack_code_states.at(code);
        const auto* pack_visual = FindPackItemVisual(bundle_, code);
        const auto label = PackCodeLabel(bundle_, code);
        std::string visual_image;
        if (pack_visual != nullptr) {
            TrackerItemSlotDefinition synthetic_slot;
            synthetic_slot.slot_id = code;
            synthetic_slot.label = label;
            visual_image = SelectPackVisualImage(
                synthetic_slot,
                pack_visual,
                item_state,
                !(item_state.owned || item_state.stage > 0 || item_state.count > 0));
        }
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(code) << ","
            << "\"code\":" << JsonString(code) << ","
            << "\"label\":" << JsonString(label) << ","
            << "\"group_id\":\"pack_items\","
            << "\"behavior\":\"pack-code\","
            << "\"icon_key\":\"\","
            << "\"render_hint\":\"pack-code\","
            << "\"pack_visual_code\":" << JsonString(code) << ","
            << "\"image\":" << JsonString(visual_image) << ","
            << "\"stage_index\":" << item_state.stage << ","
            << "\"stage\":" << item_state.stage << ","
            << "\"count\":" << item_state.count << ","
            << "\"acquired\":" << (item_state.owned ? "true" : "false") << ","
            << "\"asset_candidates\":[]"
            << "}";
    }
    out << "],";

    out << "\"pins\":[";
    first = true;
    for (const auto& group : bundle_.location_groups) {
        const auto* layer = FindPinLayer(bundle_, group.group_id);
        const auto pin_map_id = layer != nullptr && !layer->map_id.empty() ? layer->map_id : bundle_.default_map_id;
        std::size_t group_checked = 0;
        for (const auto& location : group.locations) {
            if (state_.checked_locations.contains(location.location_id)) {
                ++group_checked;
            }
        }
        const auto remaining = group.locations.size() - group_checked;
        std::string color = "red";
        if (remaining == 0) {
            color = "black";
        } else if (const auto logic_it = state_.logic_groups.find(group.group_id); logic_it != state_.logic_groups.end()) {
            color = logic_it->second.color;
        }
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(group.group_id) << ","
            << "\"location_id\":" << JsonString(group.group_id) << ","
            << "\"label\":" << JsonString(group.label) << ","
            << "\"map_id\":" << JsonString(pin_map_id) << ","
            << "\"tab_id\":" << JsonString(group.preferred_tab) << ","
            << "\"kind\":" << JsonString(layer != nullptr ? layer->pin_kind : std::string("location-group")) << ","
            << "\"checked_count\":" << group_checked << ","
            << "\"total_count\":" << group.locations.size() << ","
            << "\"remaining_count\":" << remaining << ","
            << "\"mixed\":" << (group_checked > 0 && remaining > 0 ? "true" : "false") << ","
            << "\"color\":" << JsonString(color)
            << "}";
    }
    out << "],";

    out << "\"pins_detailed\":[";
    first = true;
    const auto grouped_pins = BuildGroupedSnapshotPins(bundle_, state_);
    for (const auto& grouped_pin : grouped_pins) {
        const auto& pin = grouped_pin.base;
        const auto color = AggregatePinColor(grouped_pin, state_);
        const auto pack_it = state_.pack_location_states.find(pin.pack_location_id);
        const int state_bits = pack_it != state_.pack_location_states.end() ? LogicStateBits(pack_it->second)
                                                                            : (color == "black" ? 0 : 2);
        const bool checked = grouped_pin.total_count > 0 && grouped_pin.checked_count >= grouped_pin.total_count;
        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"id\":" << JsonString(pin.pin_id) << ","
            << "\"group_id\":" << JsonString(pin.group_id) << ","
            << "\"pack_location_id\":" << JsonString(pin.pack_location_id) << ","
            << "\"section_id\":\"\","
            << "\"location_id\":" << pin.location_id << ","
            << "\"label\":" << JsonString(pin.location_name) << ","
            << "\"map_id\":" << JsonString(pin.map_id) << ","
            << "\"pack_map\":" << JsonString(pin.pack_map) << ","
            << "\"map_asset\":" << JsonString(pin.map_asset) << ","
            << "\"kind\":" << JsonString(pin.pin_kind) << ","
            << "\"x\":" << pin.x << ","
            << "\"y\":" << pin.y << ","
            << "\"size\":" << pin.size << ","
            << "\"state\":" << state_bits << ","
            << "\"checked\":" << (checked ? "true" : "false") << ","
            << "\"checked_count\":" << grouped_pin.checked_count << ","
            << "\"total_count\":" << grouped_pin.total_count << ","
            << "\"remaining_count\":" << (grouped_pin.total_count - grouped_pin.checked_count) << ","
            << "\"mixed\":" << (!checked && (grouped_pin.segments.size() > 1 || grouped_pin.checked_count > 0) ? "true" : "false") << ","
            << "\"color\":" << JsonString(color) << ",";

        out << "\"location_ids\":[";
        bool first_location = true;
        for (const auto location_id : grouped_pin.location_ids) {
            if (!first_location) out << ",";
            first_location = false;
            out << location_id;
        }
        out << "],";

        out << "\"segments\":[";
        bool first_segment = true;
        for (const auto& segment : grouped_pin.segments) {
            const auto segment_checked = static_cast<std::uint64_t>(
                std::count_if(segment.checks.begin(), segment.checks.end(), [](const SnapshotPinCheck& check) {
                    return check.checked;
                }));
            const auto segment_color = SegmentColorForPin(segment);
            TrackerLogicGroupState segment_state;
            segment_state.color = segment_color;
            segment_state.normal_count = segment.normal_count;
            segment_state.sequence_break_count = segment.sequence_break_count;
            segment_state.inspect_count = segment.inspect_count;
            segment_state.none_count = segment.none_count;
            if (!first_segment) out << ",";
            first_segment = false;
            out << "{"
                << "\"section_id\":" << JsonString(segment.section_id) << ","
                << "\"label\":" << JsonString(segment.label) << ","
                << "\"state\":" << LogicStateBits(segment_state) << ","
                << "\"color\":" << JsonString(segment_color) << ","
                << "\"checked_count\":" << segment_checked << ","
                << "\"total_count\":" << segment.checks.size() << ","
                << "\"remaining_count\":" << (segment.checks.size() - segment_checked) << ","
                << "\"mixed\":" << (segment_checked > 0 && segment_checked < segment.checks.size() ? "true" : "false")
                << ",\"checks\":[";
            bool first_check = true;
            for (const auto& check : segment.checks) {
                if (!first_check) out << ",";
                first_check = false;
                out << "{"
                    << "\"location_id\":" << check.location_id << ","
                    << "\"label\":" << JsonString(check.label) << ","
                    << "\"checked\":" << (check.checked ? "true" : "false")
                    << "}";
            }
            out << "]}";
        }
        out << "]}";
    }
    out << "],";

    out << "\"summary\":{"
        << "\"checked\":" << checked_locations << ","
        << "\"total\":" << total_locations << ","
        << "\"received\":" << received_count
        << "},";

    out << "\"status\":{"
        << "\"ap_connected\":" << (room_connected ? "true" : "false") << ","
        << "\"logic_ready\":" << (state_.logic_ready ? "true" : "false") << ","
        << "\"pack\":" << JsonString(state_.tracker_pack) << ","
        << "\"variant\":" << JsonString(state_.tracker_variant) << ","
        << "\"assets_root\":"
        << JsonString(!config_.assets_root.empty() ? config_.assets_root.string() : resolved_bundle_path_.string())
        << "}";
    out << "}";
    return out.str();
}

bool TrackerHeadlessRuntime::PublishSnapshotIfChanged(std::string* error) {
    LoadRoomMetadata();
    if (!EvaluatePackLogic(error)) {
        return false;
    }
    const auto json = BuildSnapshotJson();
    if (json == last_snapshot_json_) {
        return SaveAutosaveState(error);
    }
    state_.revision += 1;
    const auto revised_json = BuildSnapshotJson();
    if (!AtomicWriteTextFile(config_.snapshot_path, revised_json, error)) {
        return false;
    }
    last_snapshot_json_ = revised_json;
    return SaveAutosaveState(error);
}

void TrackerForwardingEventSink::emit(const Event& event) {
    inner_.emit(event);
    tracker_.ApplyEvent(event);
}

void TrackerForwardingEventSink::trace(const TraceRecord& record) {
    inner_.trace(record);
}

}  // namespace sekailink::sklmi
