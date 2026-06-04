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
    output << "meta|slot_id|" << detail::state_file_escape(state.slot_id) << "\n";
    output << "meta|slot_name|" << detail::state_file_escape(state.slot_name) << "\n";
    output << "meta|seed_id|" << detail::state_file_escape(state.seed_id) << "\n";
    output << "meta|room_id|" << detail::state_file_escape(state.room_id) << "\n";
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
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0 || !item_state.owned_sources.empty())) {
            continue;
        }
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
        if (!(item_state.owned || item_state.stage > 0 || item_state.count > 0 || !item_state.owned_sources.empty())) {
            continue;
        }
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
    for (const auto& item : state.received_item_events) {
        output << "ap_item|" << detail::state_file_escape(item.delivery_id) << "|"
               << item.index << "|"
               << item.item_id << "|"
               << item.player_number << "|"
               << detail::state_file_escape(item.item_name) << "\n";
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
