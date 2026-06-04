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

