#include "runtime_menu_save_slots.hpp"

#include "host_io_utils.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {
namespace {

std::string DefaultSlotLabel(int slot) {
  return slot <= 0 ? "Autosave" : "Slot " + std::to_string(slot);
}

const nlohmann::json* JsonAt(const nlohmann::json& root, std::string_view key) {
  if (!root.is_object()) {
    return nullptr;
  }
  const auto found = root.find(std::string(key));
  return found == root.end() ? nullptr : &*found;
}

std::string JsonStringAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr) {
    return {};
  }
  if (value->is_string()) {
    return value->get<std::string>();
  }
  if (value->is_number_integer()) {
    return std::to_string(value->get<long long>());
  }
  if (value->is_number_unsigned()) {
    return std::to_string(value->get<unsigned long long>());
  }
  return {};
}

bool ReadToken(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::string& token) {
  token.clear();
  while (offset < bytes.size()) {
    const unsigned char ch = bytes[offset];
    if (std::isspace(ch) != 0) {
      ++offset;
      continue;
    }
    if (ch == '#') {
      while (offset < bytes.size() && bytes[offset] != '\n') {
        ++offset;
      }
      continue;
    }
    break;
  }
  while (offset < bytes.size() && std::isspace(bytes[offset]) == 0) {
    token.push_back(static_cast<char>(bytes[offset++]));
  }
  return !token.empty();
}

bool LoadPpmPreview(const std::filesystem::path& path, SaveStateSlotMenuInfo& slot) {
  if (!std::filesystem::exists(path)) {
    return false;
  }
  const auto bytes = ReadWholeFile(path);
  if (bytes.empty()) {
    return false;
  }
  std::size_t offset = 0;
  std::string token;
  if (!ReadToken(bytes, offset, token) || token != "P6") {
    return false;
  }
  if (!ReadToken(bytes, offset, token)) {
    return false;
  }
  const unsigned width = static_cast<unsigned>(std::stoul(token));
  if (!ReadToken(bytes, offset, token)) {
    return false;
  }
  const unsigned height = static_cast<unsigned>(std::stoul(token));
  if (!ReadToken(bytes, offset, token) || token != "255") {
    return false;
  }
  while (offset < bytes.size() && std::isspace(bytes[offset]) != 0) {
    ++offset;
  }
  const std::size_t rgb_size = static_cast<std::size_t>(width) * height * 3u;
  if (width == 0 || height == 0 || offset + rgb_size > bytes.size()) {
    return false;
  }
  slot.screenshot_width = width;
  slot.screenshot_height = height;
  slot.screenshot_rgba.resize(static_cast<std::size_t>(width) * height * 4u);
  for (std::size_t index = 0; index < static_cast<std::size_t>(width) * height; ++index) {
    slot.screenshot_rgba[index * 4u + 0] = bytes[offset + index * 3u + 0];
    slot.screenshot_rgba[index * 4u + 1] = bytes[offset + index * 3u + 1];
    slot.screenshot_rgba[index * 4u + 2] = bytes[offset + index * 3u + 2];
    slot.screenshot_rgba[index * 4u + 3] = 255;
  }
  return true;
}

void LoadMetadata(const SaveStateManager& save_state_manager, SaveStateSlotMenuInfo& slot) {
  const auto path = save_state_manager.StateMetadataPath(slot.slot_index);
  if (!std::filesystem::exists(path)) {
    return;
  }
  const auto bytes = ReadWholeFile(path);
  if (bytes.empty()) {
    return;
  }
  try {
    const auto metadata = nlohmann::json::parse(bytes.begin(), bytes.end());
    slot.label = JsonStringAt(metadata, "slot_label");
    slot.created_at = JsonStringAt(metadata, "created_at_local");
    if (const auto* sync = JsonAt(metadata, "sync"); sync != nullptr && sync->is_object()) {
      const auto checked = JsonStringAt(*sync, "checked_locations");
      const auto total = JsonStringAt(*sync, "total_locations");
      const auto percent = JsonStringAt(*sync, "completion_percent");
      slot.completion = checked.empty() || total.empty() ? "" : checked + "/" + total + "  " + percent + "%";
      const auto seed = JsonStringAt(*sync, "seed_name").empty()
                            ? JsonStringAt(*sync, "seed_id")
                            : JsonStringAt(*sync, "seed_name");
      const auto player = JsonStringAt(*sync, "slot_name");
      slot.detail = player.empty() ? seed : player + (seed.empty() ? "" : " / " + seed);
    }
  } catch (const std::exception&) {
  }
}

}  // namespace

std::vector<SaveStateSlotMenuInfo> LoadSaveStateSlotMenuInfos(const SaveStateManager& save_state_manager) {
  std::vector<SaveStateSlotMenuInfo> slots;
  slots.reserve(SaveStateManager::kSaveSlotCount);
  for (int slot_index = 0; slot_index < SaveStateManager::kSaveSlotCount; ++slot_index) {
    SaveStateSlotMenuInfo slot;
    slot.slot_index = slot_index;
    slot.label = DefaultSlotLabel(slot_index);
    slot.has_state = std::filesystem::exists(save_state_manager.StatePath(slot_index));
    LoadMetadata(save_state_manager, slot);
    slot.has_screenshot = LoadPpmPreview(save_state_manager.StateScreenshotPath(slot_index), slot);
    if (slot.completion.empty()) {
      slot.completion = slot.has_state ? "No sync summary" : "Empty slot";
    }
    slots.push_back(std::move(slot));
  }
  return slots;
}

}  // namespace sekaiemu::spike
