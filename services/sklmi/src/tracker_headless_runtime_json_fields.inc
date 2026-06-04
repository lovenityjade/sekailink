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

