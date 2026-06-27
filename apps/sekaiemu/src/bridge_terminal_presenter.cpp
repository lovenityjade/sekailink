#include "bridge_terminal_presenter.hpp"

#include "opengl_loader.hpp"
#include "overlay_canvas.hpp"
#include "tracker_overlay_render_state.hpp"
#include "tracker_runtime.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {
namespace {

void RestoreGlContext(SDL_Window* window, SDL_GLContext context) {
  SDL_GL_MakeCurrent(context != nullptr ? window : nullptr, context);
}

std::string Truncate(std::string_view text, std::size_t max_chars) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  if (max_chars <= 3) {
    return std::string(text.substr(0, max_chars));
  }
  return std::string(text.substr(0, max_chars - 3)) + "...";
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
  if (value->is_boolean()) {
    return value->get<bool>() ? "YES" : "NO";
  }
  if (value->is_number_integer()) {
    return std::to_string(value->get<long long>());
  }
  if (value->is_number_unsigned()) {
    return std::to_string(value->get<unsigned long long>());
  }
  return {};
}

std::string SnapshotOrMeta(const nlohmann::json& snapshot, std::string_view key) {
  auto value = JsonStringAt(snapshot, key);
  if (!value.empty()) {
    return value;
  }
  if (const auto* metadata = JsonAt(snapshot, "room_metadata"); metadata != nullptr && metadata->is_object()) {
    value = JsonStringAt(*metadata, key);
  }
  return value;
}

unsigned JsonUnsignedAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr) {
    return 0;
  }
  if (value->is_number_unsigned()) {
    return static_cast<unsigned>(value->get<unsigned long long>());
  }
  if (value->is_number_integer()) {
    return static_cast<unsigned>(std::max<long long>(0, value->get<long long>()));
  }
  return 0;
}

unsigned JsonArrayCountAt(const nlohmann::json& root, std::string_view key) {
  const auto* value = JsonAt(root, key);
  if (value == nullptr || !value->is_array()) {
    return 0;
  }
  return static_cast<unsigned>(value->size());
}

std::string EndpointLabel(const BridgeRuntimeStatus& status) {
  std::string endpoint = status.ap_host.empty() ? "LOCAL ROOM" : status.ap_host;
  if (status.ap_port != 0) {
    endpoint += ":" + std::to_string(status.ap_port);
  }
  if (!status.ap_path.empty() && status.ap_path != "/") {
    endpoint += status.ap_path;
  }
  return endpoint;
}

void AppendChatLines(const nlohmann::json& snapshot, std::vector<std::string>& lines) {
  const auto* messages = JsonAt(snapshot, "chat_messages");
  if (messages == nullptr || !messages->is_array()) {
    return;
  }
  const std::size_t start = messages->size() > 18 ? messages->size() - 18 : 0;
  for (std::size_t index = start; index < messages->size(); ++index) {
    const auto& message = (*messages)[index];
    if (!message.is_object()) {
      continue;
    }
    auto author = JsonStringAt(message, "author");
    auto text = JsonStringAt(message, "text");
    if (author.empty()) {
      author = "ROOM";
    }
    if (!text.empty()) {
      lines.push_back(author + ": " + text);
    }
  }
}

void AppendReceivedItemLines(const nlohmann::json& snapshot, std::vector<std::string>& lines) {
  const auto* items = JsonAt(snapshot, "received_items");
  if (items == nullptr || !items->is_array() || items->empty()) {
    return;
  }
  lines.push_back("");
  lines.push_back("ITEM SUMMARY");
  const std::size_t start = items->size() > 10 ? items->size() - 10 : 0;
  for (std::size_t index = start; index < items->size(); ++index) {
    const auto& item = (*items)[index];
    if (!item.is_object()) {
      continue;
    }
    auto name = JsonStringAt(item, "item_name");
    if (name.empty()) {
      name = JsonStringAt(item, "slot_id");
    }
    if (name.empty()) {
      name = "UNKNOWN ITEM";
    }
    const unsigned count = JsonUnsignedAt(item, "count");
    const unsigned stage = JsonUnsignedAt(item, "stage");
    std::string suffix;
    if (count > 0) {
      suffix = " x" + std::to_string(count);
    } else if (stage > 0) {
      suffix = " +" + std::to_string(stage);
    }
    lines.push_back("ITEM: " + name + suffix);
  }
}

UiColor SeverityColor(RuntimeDebugSeverity severity) {
  switch (severity) {
    case RuntimeDebugSeverity::Error:
      return UiColor{255, 110, 120, 255};
    case RuntimeDebugSeverity::Warning:
      return UiColor{255, 190, 100, 255};
    case RuntimeDebugSeverity::Trace:
      return UiColor{145, 165, 190, 255};
    case RuntimeDebugSeverity::Info:
      return UiColor{205, 225, 235, 255};
  }
  return UiColor{205, 225, 235, 255};
}

UiColor CategoryColor(RuntimeDebugCategory category) {
  switch (category) {
    case RuntimeDebugCategory::Connection:
      return UiColor{91, 215, 205, 255};
    case RuntimeDebugCategory::Check:
      return UiColor{150, 235, 175, 255};
    case RuntimeDebugCategory::Item:
      return UiColor{255, 205, 120, 255};
    case RuntimeDebugCategory::Memory:
    case RuntimeDebugCategory::Persistence:
      return UiColor{175, 160, 255, 255};
    case RuntimeDebugCategory::Tracker:
      return UiColor{210, 150, 240, 255};
    case RuntimeDebugCategory::Command:
      return UiColor{255, 150, 110, 255};
    case RuntimeDebugCategory::Error:
      return UiColor{255, 110, 120, 255};
    case RuntimeDebugCategory::Other:
      return UiColor{180, 195, 210, 255};
  }
  return UiColor{180, 195, 210, 255};
}

void DrawPill(OverlayCanvas& canvas,
              int x,
              int y,
              int w,
              int h,
              std::string_view label,
              bool active,
              UiColor accent) {
  canvas.FillRect(x, y, w, h, active ? accent : UiColor{23, 31, 42, 255});
  canvas.DrawRect(x, y, w, h, active ? UiColor{235, 250, 255, 255} : UiColor{65, 82, 104, 255});
  canvas.DrawText(x + 9, y + 7, std::string(label), active ? UiColor{8, 12, 16, 255}
                                                           : UiColor{210, 225, 235, 255}, 1);
}

struct FilterButton {
  RuntimeDebugFilter filter;
  const char* label;
  int width;
};

constexpr std::array<FilterButton, 8> kFilterButtons{{
    {RuntimeDebugFilter::All, "All", 54},
    {RuntimeDebugFilter::Errors, "Errors", 82},
    {RuntimeDebugFilter::Connections, "Connect", 92},
    {RuntimeDebugFilter::Checks, "Checks", 82},
    {RuntimeDebugFilter::Items, "Items", 70},
    {RuntimeDebugFilter::Memory, "Memory", 82},
    {RuntimeDebugFilter::Tracker, "Tracker", 86},
    {RuntimeDebugFilter::Commands, "Commands", 104},
}};

constexpr std::array<std::string_view, 17> kDebugCommands{{
    "help", "status", "clear", "give", "say", "ap-say", "hint", "collect", "remaining",
    "admin", "ap-info", "ap-log", "players", "items", "check", "ap-check", "raw",
}};

std::string EndpointLabelShort(const BridgeRuntimeStatus& status) {
  auto endpoint = EndpointLabel(status);
  return endpoint.empty() ? "-" : endpoint;
}

std::string TrimSpaces(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.erase(text.begin());
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }
  return text;
}

std::string LowerCopy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

bool StartsWithCommand(std::string_view text, std::string_view command) {
  if (text.size() < command.size()) {
    return false;
  }
  if (text.substr(0, command.size()) != command) {
    return false;
  }
  return text.size() == command.size() || std::isspace(static_cast<unsigned char>(text[command.size()]));
}

std::vector<std::string> SplitWords(std::string_view text) {
  std::istringstream stream{std::string(text)};
  std::vector<std::string> words;
  std::string word;
  while (stream >> word) {
    words.push_back(word);
  }
  return words;
}

void AddUniqueSuggestion(std::vector<std::string>& out,
                         std::unordered_set<std::string>& seen,
                         std::string value) {
  value = TrimSpaces(std::move(value));
  if (value.empty()) {
    return;
  }
  const auto lowered = value;
  if (seen.insert(lowered).second) {
    out.push_back(std::move(value));
  }
}

void CollectStringArraySuggestions(const nlohmann::json& root,
                                   std::string_view key,
                                   std::vector<std::string>& out,
                                   std::unordered_set<std::string>& seen) {
  if (!root.is_object()) {
    return;
  }
  const auto found = root.find(std::string(key));
  if (found == root.end() || !found->is_array()) {
    return;
  }
  for (const auto& entry : *found) {
    if (entry.is_string()) {
      AddUniqueSuggestion(out, seen, entry.get<std::string>());
    } else if (entry.is_object()) {
      for (const char* candidate_key : {"name", "slot_name", "player", "item_name", "location_name", "label"}) {
        const auto value = entry.find(candidate_key);
        if (value != entry.end() && value->is_string()) {
          AddUniqueSuggestion(out, seen, value->get<std::string>());
        }
      }
    }
  }
}

std::vector<std::string> PlayerSuggestions(const BridgeRuntimeStatus& status, const nlohmann::json& snapshot) {
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  AddUniqueSuggestion(out, seen, status.ap_slot_name);
  CollectStringArraySuggestions(snapshot, "players", out, seen);
  CollectStringArraySuggestions(snapshot, "slot_names", out, seen);
  if (const auto* metadata = JsonAt(snapshot, "room_metadata"); metadata != nullptr && metadata->is_object()) {
    CollectStringArraySuggestions(*metadata, "players", out, seen);
    CollectStringArraySuggestions(*metadata, "slot_names", out, seen);
  }
  return out;
}

std::vector<std::string> ItemSuggestions(const nlohmann::json& snapshot) {
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  CollectStringArraySuggestions(snapshot, "hint_item_names", out, seen);
  CollectStringArraySuggestions(snapshot, "item_names", out, seen);
  CollectStringArraySuggestions(snapshot, "received_items", out, seen);
  if (const auto* metadata = JsonAt(snapshot, "room_metadata"); metadata != nullptr && metadata->is_object()) {
    CollectStringArraySuggestions(*metadata, "hint_item_names", out, seen);
    CollectStringArraySuggestions(*metadata, "item_names", out, seen);
  }
  return out;
}

std::vector<std::string> LocationSuggestions(const nlohmann::json& snapshot) {
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  CollectStringArraySuggestions(snapshot, "checked_locations", out, seen);
  CollectStringArraySuggestions(snapshot, "location_names", out, seen);
  if (const auto* metadata = JsonAt(snapshot, "room_metadata"); metadata != nullptr && metadata->is_object()) {
    CollectStringArraySuggestions(*metadata, "location_names", out, seen);
  }
  return out;
}

std::string JoinWords(const std::vector<std::string>& words, std::size_t start) {
  std::string out;
  for (std::size_t index = start; index < words.size(); ++index) {
    if (!out.empty()) {
      out += " ";
    }
    out += words[index];
  }
  return out;
}

std::vector<std::string> FilterPrefix(std::vector<std::string> candidates, std::string prefix) {
  prefix = LowerCopy(TrimSpaces(std::move(prefix)));
  if (prefix.empty()) {
    if (candidates.size() > 12) {
      candidates.resize(12);
    }
    return candidates;
  }
  std::vector<std::string> out;
  for (const auto& candidate : candidates) {
    if (LowerCopy(candidate).find(prefix) != std::string::npos) {
      out.push_back(candidate);
      if (out.size() >= 12) {
        break;
      }
    }
  }
  return out;
}

std::vector<std::string> BuildSuggestionsForInput(std::string_view input,
                                                  const BridgeRuntimeStatus& status,
                                                  const nlohmann::json& snapshot) {
  const auto ends_with_space = !input.empty() && std::isspace(static_cast<unsigned char>(input.back()));
  auto words = SplitWords(input);
  if (words.empty()) {
    std::vector<std::string> commands;
    for (const auto command : kDebugCommands) {
      commands.push_back(std::string(command));
    }
    return commands;
  }
  const auto command = LowerCopy(words.front());
  if (words.size() == 1 && !ends_with_space) {
    std::vector<std::string> commands;
    for (const auto candidate : kDebugCommands) {
      commands.push_back(std::string(candidate));
    }
    return FilterPrefix(std::move(commands), words.front());
  }
  if (command == "give") {
    if (words.size() <= 2 && !ends_with_space) {
      return FilterPrefix(PlayerSuggestions(status, snapshot), words.size() == 2 ? words[1] : "");
    }
    return FilterPrefix(ItemSuggestions(snapshot), JoinWords(words, 2));
  }
  if (command == "hint" || command == "items") {
    return FilterPrefix(ItemSuggestions(snapshot), JoinWords(words, 1));
  }
  if (command == "check" || command == "ap-check") {
    return FilterPrefix(LocationSuggestions(snapshot), JoinWords(words, 1));
  }
  if (command == "ap-say") {
    if (words.size() <= 2 && !ends_with_space) {
      return FilterPrefix(PlayerSuggestions(status, snapshot), words.size() == 2 ? words[1] : "");
    }
  }
  return {};
}

std::string ReplaceCurrentCompletionToken(std::string input, std::string completion) {
  const bool ends_with_space = !input.empty() && std::isspace(static_cast<unsigned char>(input.back()));
  auto words = SplitWords(input);
  if (words.empty()) {
    return completion + " ";
  }
  const auto command = LowerCopy(words.front());
  if (words.size() == 1 && !ends_with_space) {
    return completion + " ";
  }
  if (command == "give") {
    if (words.size() <= 2 && !ends_with_space) {
      return "give " + completion + " ";
    }
    return words.size() >= 2 ? "give " + words[1] + " " + completion : "give " + completion;
  }
  if (command == "hint" || command == "items" || command == "check" || command == "ap-check") {
    return words.front() + " " + completion;
  }
  if (command == "ap-say" && words.size() <= 2 && !ends_with_space) {
    return "ap-say " + completion + " ";
  }
  return input;
}

std::vector<std::string> BuildChatCommandsFromDebugInput(std::string_view raw_input,
                                                         std::string& readable_error,
                                                         std::string& redacted_description) {
  auto input = TrimSpaces(std::string(raw_input));
  if (input.empty()) {
    readable_error = "Empty command.";
    return {};
  }
  if (StartsWithCommand(input, "say")) {
    auto text = TrimSpaces(input.substr(3));
    if (text.empty()) {
      readable_error = "Usage: say <message>";
      return {};
    }
    redacted_description = text;
    return {text};
  }
  if (StartsWithCommand(input, "ap-say")) {
    auto text = TrimSpaces(input.substr(6));
    if (text.empty()) {
      readable_error = "Usage: ap-say <message>";
      return {};
    }
    redacted_description = text;
    return {text};
  }
  if (StartsWithCommand(input, "hint")) {
    auto item = TrimSpaces(input.substr(4));
    if (item.empty()) {
      readable_error = "Usage: hint <item name>";
      return {};
    }
    redacted_description = "!hint " + item;
    return {"!hint " + item};
  }
  if (input == "collect") {
    redacted_description = "!collect";
    return {"!collect"};
  }
  if (input == "remaining") {
    redacted_description = "!remaining";
    return {"!remaining"};
  }
  if (StartsWithCommand(input, "admin")) {
    auto text = TrimSpaces(input.substr(5));
    if (text.empty()) {
      readable_error = "Usage: admin <server command>";
      return {};
    }
    const char* admin_password = std::getenv("SEKAILINK_AP_ADMIN_PASSWORD");
    if (admin_password == nullptr || std::string_view(admin_password).empty()) {
      readable_error = "SEKAILINK_AP_ADMIN_PASSWORD is missing; admin commands need !admin login.";
      return {};
    }
    redacted_description = "!admin login ***redacted*** ; !admin " + text;
    return {"!admin login " + std::string(admin_password), "!admin " + text};
  }
  if (StartsWithCommand(input, "give")) {
    std::istringstream stream(input);
    std::string command;
    std::string slot;
    stream >> command >> slot;
    std::string item;
    std::getline(stream, item);
    item = TrimSpaces(item);
    if (slot.empty() || item.empty()) {
      readable_error = "Usage: give <slotname> <item name>";
      return {};
    }
    const char* admin_password = std::getenv("SEKAILINK_AP_ADMIN_PASSWORD");
    if (admin_password == nullptr || std::string_view(admin_password).empty()) {
      readable_error = "SEKAILINK_AP_ADMIN_PASSWORD is missing; give needs !admin login before /send.";
      return {};
    }
    redacted_description = "!admin login ***redacted*** ; !admin /send " + slot + " " + item;
    return {"!admin login " + std::string(admin_password), "!admin /send " + slot + " " + item};
  }
  if (StartsWithCommand(input, "check") || StartsWithCommand(input, "ap-check")) {
    readable_error = "check/ap-check needs a direct LocationChecks backend; this runtime bar currently supports give and say.";
    return {};
  }
  if (input == "help" || input == "status" || input == "players" || input == "items" ||
      input == "ap-info" || input == "ap-log" || input == "clear") {
    readable_error = "local";
    return {};
  }
  if (StartsWithCommand(input, "raw")) {
    readable_error = "raw JSON packets are not sent from Runtime Debug yet; use skl-room for raw room server commands.";
    return {};
  }
  if (!input.empty() && input.front() == '/') {
    redacted_description = RedactRuntimeDebugText(input);
    return {input};
  }
  readable_error = "Unknown command. Available: give <slot> <item>, say <message>.";
  return {};
}

std::string JoinSuggestions(const std::vector<std::string>& values, std::size_t max_count = 12) {
  if (values.empty()) {
    return "-";
  }
  std::string out;
  const auto count = std::min(max_count, values.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (!out.empty()) {
      out += ", ";
    }
    out += values[index];
  }
  if (values.size() > count) {
    out += ", ...";
  }
  return out;
}

std::string LocalCommandResponse(std::string_view raw_input,
                                 const BridgeRuntimeStatus& status,
                                 const nlohmann::json& snapshot) {
  const auto input = TrimSpaces(std::string(raw_input));
  if (input == "help") {
    return "Commands: help, status, players, items, give <slot> <item>, say <message>, ap-say <message>, hint <item>, collect, remaining, admin <cmd>, check <loc>, ap-check <loc>, raw <json>, clear.";
  }
  if (input == "status" || input == "ap-info") {
    std::ostringstream out;
    out << "AP " << EndpointLabelShort(status)
        << " slot=" << (status.ap_slot_name.empty() ? "-" : status.ap_slot_name)
        << " game=" << (status.game_name.empty() ? "-" : status.game_name)
        << " sklmi=" << (status.sklmi_active ? "active" : "inactive")
        << " memory=" << (status.runtime_memory_socket_path.empty() ? "-" : status.runtime_memory_socket_path);
    return out.str();
  }
  if (input == "players") {
    return "Players: " + JoinSuggestions(PlayerSuggestions(status, snapshot), 20);
  }
  if (input == "items") {
    return "Items: " + JoinSuggestions(ItemSuggestions(snapshot), 20);
  }
  if (input == "ap-log") {
    return "Use the Readable/RAW tabs above: they are the AP/runtime log for this session.";
  }
  return {};
}

}  // namespace

BridgeTerminalPresenter::~BridgeTerminalPresenter() {
  Shutdown();
}

void BridgeTerminalPresenter::SetEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled_) {
    Shutdown();
  }
}

bool BridgeTerminalPresenter::EnsureWindow(unsigned width, unsigned height) {
  if (!enabled_) {
    return false;
  }
  if (!window_) {
    window_ = SDL_CreateWindow("SekaiLink Runtime Debug",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               static_cast<int>(width),
                               static_cast<int>(height),
                               SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) {
      return false;
    }
  }
  if (!gl_context_) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
      return false;
    }
    auto* previous_window = SDL_GL_GetCurrentWindow();
    const auto previous_context = SDL_GL_GetCurrentContext();
    if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
      return false;
    }
    std::string error;
    if (!LoadOpenGlFunctions(error)) {
      RestoreGlContext(previous_window, previous_context);
      return false;
    }
    SDL_GL_SetSwapInterval(0);
    RestoreGlContext(previous_window, previous_context);
  }
  return true;
}

bool BridgeTerminalPresenter::HandleSdlEvent(const SDL_Event& event) {
  if (!enabled_ || !window_) {
    return false;
  }
  const auto window_id = SDL_GetWindowID(window_);
  if (event.type == SDL_WINDOWEVENT && event.window.windowID == window_id) {
    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
      SetEnabled(false);
      return true;
    }
    return false;
  }
  if (event.type == SDL_TEXTINPUT && command_focused_) {
    if (command_input_.size() < 320) {
      command_input_ += event.text.text;
    }
    command_suggestions_ = BuildSuggestionsForInput(command_input_, last_status_, last_snapshot_);
    dirty_ = true;
    return true;
  }
  if (event.type == SDL_KEYDOWN && command_focused_) {
    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
      command_focused_ = false;
      SDL_StopTextInput();
      dirty_ = true;
      return true;
    }
    if (event.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
      if (!command_input_.empty()) {
        command_input_.pop_back();
      }
      command_suggestions_ = BuildSuggestionsForInput(command_input_, last_status_, last_snapshot_);
      dirty_ = true;
      return true;
    }
    if (event.key.keysym.scancode == SDL_SCANCODE_TAB) {
      command_suggestions_ = BuildSuggestionsForInput(command_input_, last_status_, last_snapshot_);
      if (!command_suggestions_.empty()) {
        command_input_ = ReplaceCurrentCompletionToken(command_input_, command_suggestions_.front());
        debug_log_.AddLocalEvent(RuntimeDebugSeverity::Info,
                                 RuntimeDebugCategory::Command,
                                 "Runtime Debug",
                                 "Autocomplete: " + JoinSuggestions(command_suggestions_),
                                 nlohmann::json{{"suggestions", command_suggestions_}}.dump());
      } else {
        debug_log_.AddLocalEvent(RuntimeDebugSeverity::Trace,
                                 RuntimeDebugCategory::Command,
                                 "Runtime Debug",
                                 "Autocomplete: no suggestions.",
                                 {});
      }
      dirty_ = true;
      return true;
    }
    if (event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
        event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) {
      std::string error;
      std::string redacted_description;
      const auto chat_commands = BuildChatCommandsFromDebugInput(command_input_, error, redacted_description);
      if (!chat_commands.empty()) {
        for (const auto& chat_command : chat_commands) {
          pending_chat_commands_.push_back(chat_command);
        }
        debug_log_.AddLocalEvent(RuntimeDebugSeverity::Info,
                                 RuntimeDebugCategory::Command,
                                 "Runtime Debug",
                                 "Queued command: " + command_input_,
                                 nlohmann::json{{"input", command_input_},
                                                {"chat_text", redacted_description},
                                                {"message_count", chat_commands.size()}}.dump());
        command_input_.clear();
        command_suggestions_.clear();
      } else if (error == "local") {
        if (TrimSpaces(command_input_) == "clear") {
          debug_log_.Clear();
        } else {
          debug_log_.AddLocalEvent(RuntimeDebugSeverity::Info,
                                   RuntimeDebugCategory::Command,
                                   "Runtime Debug",
                                   LocalCommandResponse(command_input_, last_status_, last_snapshot_),
                                   nlohmann::json{{"input", command_input_}, {"local", true}}.dump());
        }
        command_input_.clear();
        command_suggestions_.clear();
      } else {
        debug_log_.AddLocalEvent(RuntimeDebugSeverity::Warning,
                                 RuntimeDebugCategory::Command,
                                 "Runtime Debug",
                                 error,
                                 nlohmann::json{{"input", command_input_}, {"error", error}}.dump());
      }
      dirty_ = true;
      return true;
    }
    if (event.key.keysym.scancode == SDL_SCANCODE_V && (event.key.keysym.mod & KMOD_CTRL) != 0) {
      char* clipboard = SDL_GetClipboardText();
      if (clipboard != nullptr) {
        command_input_ += clipboard;
        SDL_free(clipboard);
      }
      command_suggestions_ = BuildSuggestionsForInput(command_input_, last_status_, last_snapshot_);
      dirty_ = true;
      return true;
    }
    return true;
  }
  if (event.type != SDL_MOUSEBUTTONDOWN || event.button.windowID != window_id ||
      event.button.button != SDL_BUTTON_LEFT) {
    return false;
  }

  const int x = event.button.x;
  const int y = event.button.y;
  const int command_y = static_cast<int>(height_ > 76 ? height_ - 56 : 504);
  if (y >= command_y && y <= command_y + 34) {
    command_focused_ = true;
    command_suggestions_ = BuildSuggestionsForInput(command_input_, last_status_, last_snapshot_);
    SDL_StartTextInput();
    dirty_ = true;
    return true;
  }
  command_focused_ = false;
  SDL_StopTextInput();
  if (y >= 58 && y <= 88) {
    if (x >= 16 && x <= 126) {
      raw_view_ = false;
      dirty_ = true;
      return true;
    }
    if (x >= 136 && x <= 246) {
      raw_view_ = true;
      dirty_ = true;
      return true;
    }
  }

  int filter_x = 16;
  if (y >= 98 && y <= 126) {
    for (const auto& button : kFilterButtons) {
      if (x >= filter_x && x <= filter_x + button.width) {
        filter_ = button.filter;
        dirty_ = true;
        return true;
      }
      filter_x += button.width + 8;
    }
  }

  const int copy_x = static_cast<int>(width_ > 190 ? width_ - 184 : 620);
  if (y >= 98 && y <= 126 && x >= copy_x && x <= copy_x + 168) {
    SDL_SetClipboardText(last_report_.c_str());
    return true;
  }
  const int clear_x = copy_x - 88;
  if (y >= 98 && y <= 126 && x >= clear_x && x <= clear_x + 76) {
    debug_log_.Clear();
    dirty_ = true;
    return true;
  }
  return false;
}

std::vector<std::string> BridgeTerminalPresenter::DrainPendingChatCommands() {
  auto out = std::move(pending_chat_commands_);
  pending_chat_commands_.clear();
  return out;
}

void BridgeTerminalPresenter::RecordCommandDelivery(std::string_view command,
                                                    bool sent,
                                                    std::string_view detail) {
  debug_log_.AddLocalEvent(sent ? RuntimeDebugSeverity::Info : RuntimeDebugSeverity::Error,
                           RuntimeDebugCategory::Command,
                           "Runtime Debug",
                           sent ? "Command sent to SKLMI/AP chat bridge."
                                : "Command delivery failed.",
                           nlohmann::json{{"chat_text", std::string(command)},
                                          {"sent", sent},
                                          {"detail", std::string(detail)}}.dump());
  dirty_ = true;
}

void BridgeTerminalPresenter::Render(const BridgeRuntimeStatus& bridge_status,
                                     const TrackerRuntime* tracker_runtime) {
  constexpr unsigned kWindowWidth = 820;
  constexpr unsigned kWindowHeight = 560;
  if (!EnsureWindow(kWindowWidth, kWindowHeight)) {
    return;
  }

  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  const unsigned render_width =
      static_cast<unsigned>(std::max<int>(520, drawable_width > 0 ? drawable_width : kWindowWidth));
  const unsigned render_height =
      static_cast<unsigned>(std::max<int>(360, drawable_height > 0 ? drawable_height : kWindowHeight));

  const nlohmann::json snapshot = tracker_runtime != nullptr
                                      ? tracker_runtime->AuthoritativeState().snapshot
                                      : nlohmann::json::object();
  last_status_ = bridge_status;
  last_snapshot_ = snapshot;
  if (command_focused_) {
    command_suggestions_ = BuildSuggestionsForInput(command_input_, last_status_, last_snapshot_);
  }
  debug_log_.Refresh(bridge_status, snapshot);
  last_report_ = debug_log_.BuildRedactedReport(bridge_status, snapshot, filter_);

  const bool connected = SnapshotOrMeta(snapshot, "connected") == "YES" ||
                         SnapshotOrMeta(snapshot, "connected") == "1" ||
                         SnapshotOrMeta(snapshot, "ap_connected") == "YES";
  const auto summary = JsonAt(snapshot, "summary");
  const unsigned checked_from_summary = summary != nullptr ? JsonUnsignedAt(*summary, "checked") : 0;
  const unsigned missing_from_summary = summary != nullptr ? JsonUnsignedAt(*summary, "missing") : 0;
  const unsigned total_from_summary = summary != nullptr ? JsonUnsignedAt(*summary, "total") : 0;
  const unsigned checked_from_snapshot = JsonArrayCountAt(snapshot, "checked_locations");
  const unsigned missing_from_snapshot = JsonArrayCountAt(snapshot, "missing_locations");
  const unsigned checked = std::max(checked_from_summary, checked_from_snapshot);
  const unsigned total = total_from_summary > 0
                             ? total_from_summary
                             : checked + std::max(missing_from_summary, missing_from_snapshot);

  OverlayCanvas canvas(render_width, render_height);
  canvas.Clear({6, 8, 12, 255});
  canvas.FillRect(0, 0, static_cast<int>(render_width), 48, UiColor{13, 26, 34, 255});
  canvas.DrawText(16, 14, "SEKAILINK RUNTIME DEBUG", UiColor{91, 240, 216, 255}, 2);
  canvas.DrawText(static_cast<int>(render_width) - 128,
                  17,
                  connected ? "CONNECTED" : "WAITING",
                  connected ? UiColor{170, 230, 180, 255} : UiColor{255, 215, 150, 255},
                  1);

  DrawPill(canvas, 16, 58, 110, 30, "Readable", !raw_view_, UiColor{91, 240, 216, 255});
  DrawPill(canvas, 136, 58, 110, 30, "RAW I/O", raw_view_, UiColor{255, 125, 94, 255});

  int filter_x = 16;
  for (const auto& button : kFilterButtons) {
    DrawPill(canvas,
             filter_x,
             98,
             button.width,
             28,
             button.label,
             filter_ == button.filter,
             UiColor{155, 128, 220, 255});
    filter_x += button.width + 8;
  }
  const int copy_x = static_cast<int>(render_width) - 184;
  DrawPill(canvas, copy_x - 88, 98, 76, 28, "Clear", false, UiColor{65, 82, 104, 255});
  DrawPill(canvas, copy_x, 98, 168, 28, "Copy Debug Report", false, UiColor{65, 82, 104, 255});

  const int info_y = 140;
  const std::string player = tracker_runtime != nullptr
                                 ? SnapshotDisplayPlayerName(*tracker_runtime, bridge_status.ap_slot_name)
                                 : bridge_status.ap_slot_name;
  const std::string game = SnapshotOrMeta(snapshot, "game").empty()
                               ? bridge_status.game_name
                               : SnapshotOrMeta(snapshot, "game");
  const std::string players = SnapshotOrMeta(snapshot, "player_count").empty()
                                  ? "1 known"
                                  : SnapshotOrMeta(snapshot, "player_count");
  const std::string sync_id = SnapshotOrMeta(snapshot, "seed_name").empty()
                                  ? SnapshotOrMeta(snapshot, "seed")
                                  : SnapshotOrMeta(snapshot, "seed_name");

  canvas.DrawText(16, info_y, "ROOM  " + Truncate(EndpointLabelShort(bridge_status), 72),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(16, info_y + 18, "PLAYER  " + Truncate(player.empty() ? "-" : player, 36),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(300, info_y + 18, "GAME  " + Truncate(game.empty() ? "-" : game, 36),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(16, info_y + 36, "PLAYERS  " + players,
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(300, info_y + 36,
                  "CHECKS  " + std::to_string(checked) + "/" + std::to_string(total),
                  UiColor{180, 205, 255, 255},
                  1);
  canvas.DrawText(16, info_y + 54, "SYNC  " + Truncate(sync_id.empty() ? "-" : sync_id, 80),
                  UiColor{180, 205, 255, 255}, 1);
  canvas.DrawText(300, info_y + 54,
                  "MEMORY  " + Truncate(bridge_status.runtime_memory_socket_path.empty()
                                             ? "-"
                                             : bridge_status.runtime_memory_socket_path,
                                         56),
                  UiColor{180, 205, 255, 255},
                  1);

  const int log_x = 16;
  const int log_y = 222;
  const int log_width = static_cast<int>(render_width) - 32;
  const int command_height = 42;
  const int log_height = static_cast<int>(render_height) - log_y - command_height - 22;
  canvas.FillRect(log_x, log_y, log_width, log_height, UiColor{10, 14, 22, 255});
  canvas.DrawRect(log_x, log_y, log_width, log_height, UiColor{65, 85, 120, 255});
  canvas.DrawText(log_x + 10,
                  log_y + 10,
                  raw_view_ ? "RAW I/O EVENTS" : "READABLE EVENTS",
                  UiColor{255, 245, 225, 255},
                  1);

  const auto events = debug_log_.FilteredEvents(filter_, 120);
  std::vector<std::string> lines;
  std::vector<RuntimeDebugSeverity> severities;
  std::vector<RuntimeDebugCategory> categories;
  if (events.empty()) {
    lines.push_back(raw_view_ ? "No raw runtime events yet." : "Waiting for runtime events...");
    severities.push_back(RuntimeDebugSeverity::Info);
    categories.push_back(RuntimeDebugCategory::Other);
  } else {
    for (const auto* event : events) {
      std::ostringstream line;
      if (raw_view_) {
        line << "#" << event->sequence << " " << event->source << " " << event->raw;
      } else {
        line << "#" << event->sequence << " [" << RuntimeDebugCategoryName(event->category) << "] "
             << event->source << " - " << event->summary;
      }
      lines.push_back(line.str());
      severities.push_back(event->severity);
      categories.push_back(event->category);
    }
  }

  const int line_height = 15;
  const int max_lines = std::max(1, (log_height - 36) / line_height);
  const std::size_t start = lines.size() > static_cast<std::size_t>(max_lines)
                                ? lines.size() - static_cast<std::size_t>(max_lines)
                                : 0;
  int y = log_y + 30;
  for (std::size_t index = start; index < lines.size(); ++index) {
    const auto color = raw_view_ ? SeverityColor(severities[index]) : CategoryColor(categories[index]);
    canvas.DrawText(log_x + 10,
                    y,
                    Truncate(lines[index], static_cast<std::size_t>(std::max(20, (log_width - 20) / 6))),
                    color,
                    1);
    y += line_height;
  }

  const int command_y = log_y + log_height + 10;
  if (command_focused_ && !command_suggestions_.empty()) {
    const int suggestions_y = command_y - 22;
    canvas.DrawText(log_x + 10,
                    suggestions_y,
                    "Tab: " + Truncate(JoinSuggestions(command_suggestions_, 8),
                                       static_cast<std::size_t>(std::max(20, (log_width - 20) / 6))),
                    UiColor{91, 240, 216, 255},
                    1);
  }
  canvas.FillRect(log_x, command_y, log_width, 34, UiColor{15, 21, 30, 255});
  canvas.DrawRect(log_x, command_y, log_width, 34, UiColor{65, 85, 120, 255});
  canvas.DrawText(log_x + 10,
                  command_y + 10,
                  (command_focused_ ? "> " : "  ") +
                      Truncate(command_input_.empty()
                                   ? "give <slot> <item>  |  say <message>  |  give uses SEKAILINK_AP_ADMIN_PASSWORD"
                                   : command_input_,
                               static_cast<std::size_t>(std::max(20, (log_width - 20) / 6))),
                  command_focused_ ? UiColor{255, 245, 225, 255} : UiColor{150, 165, 180, 255},
                  1);

  pixels_.assign(canvas.Data(),
                 canvas.Data() + static_cast<std::size_t>(canvas.Width()) * canvas.Height() * 4u);
  width_ = render_width;
  height_ = render_height;
  dirty_ = true;
}

void BridgeTerminalPresenter::Present() {
  if (!enabled_ || !window_ || !gl_context_ || pixels_.empty() || !dirty_) {
    return;
  }
  auto* previous_window = SDL_GL_GetCurrentWindow();
  const auto previous_context = SDL_GL_GetCurrentContext();
  SDL_GL_MakeCurrent(window_, gl_context_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  glViewport(0, 0, std::max(1, drawable_width), std::max(1, drawable_height));
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  std::string error;
  renderer_.Draw(RETRO_HW_CONTEXT_OPENGL_CORE,
                 pixels_,
                 width_,
                 height_,
                 std::max(1, drawable_width),
                 std::max(1, drawable_height),
                 true,
                 error);
  SDL_GL_SwapWindow(window_);
  RestoreGlContext(previous_window, previous_context);
  dirty_ = false;
}

void BridgeTerminalPresenter::Shutdown() {
  auto* previous_window = SDL_GL_GetCurrentWindow();
  const auto previous_context = SDL_GL_GetCurrentContext();
  const bool previous_context_is_ours = previous_context == gl_context_;
  if (window_ && gl_context_) {
    SDL_GL_MakeCurrent(window_, gl_context_);
  }
  renderer_.Destroy();
  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  pixels_.clear();
  width_ = 0;
  height_ = 0;
  dirty_ = false;
  enabled_ = false;
  if (!previous_context_is_ours) {
    RestoreGlContext(previous_window, previous_context);
  } else {
    RestoreGlContext(nullptr, nullptr);
  }
}

}  // namespace sekaiemu::spike
