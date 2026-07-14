#include "runtime_client_core_hud.hpp"

#include "bridge_runtime_status.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace sekaiemu::spike {
namespace {

constexpr std::uint64_t kPollFrameInterval = 60;
constexpr double kToastLifetimeSeconds = 4.2;
constexpr std::size_t kMaxVisibleToasts = 4;

double MonotonicSeconds() {
  static const auto start = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

double SystemSeconds() {
  return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string TrimText(std::string text, std::size_t max_size) {
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
  while (!text.empty() && text.front() == ' ') {
    text.erase(text.begin());
  }
  while (!text.empty() && text.back() == ' ') {
    text.pop_back();
  }
  if (text.size() > max_size) {
    text.resize(max_size);
  }
  return text;
}

int JsonInt(const nlohmann::json& root, const char* key) {
  const auto found = root.find(key);
  if (found == root.end()) {
    return 0;
  }
  if (found->is_number_integer() || found->is_number_unsigned()) {
    return std::max(0, found->get<int>());
  }
  if (found->is_string()) {
    try {
      return std::max(0, std::stoi(found->get<std::string>()));
    } catch (...) {
      return 0;
    }
  }
  return 0;
}

bool JsonBool(const nlohmann::json& root, const char* key, bool fallback) {
  const auto found = root.find(key);
  if (found == root.end()) {
    return fallback;
  }
  if (found->is_boolean()) {
    return found->get<bool>();
  }
  if (found->is_string()) {
    const auto value = found->get<std::string>();
    return value == "1" || value == "true" || value == "yes" || value == "on";
  }
  return fallback;
}

double ToastCreatedAt(const nlohmann::json& toast) {
  const double now = MonotonicSeconds();
  double created_at_epoch = 0.0;
  if (const auto created_ms = toast.find("createdAtMs");
      created_ms != toast.end() && created_ms->is_number()) {
    created_at_epoch = created_ms->get<double>() / 1000.0;
  } else if (const auto created = toast.find("createdAt");
             created != toast.end() && created->is_number()) {
    created_at_epoch = created->get<double>();
  }
  if (created_at_epoch > 0.0) {
    const double age = SystemSeconds() - created_at_epoch;
    if (age >= 0.0 && age <= kToastLifetimeSeconds + 60.0) {
      return now - age;
    }
  }
  return now;
}

void DrawBadge(int count, const ImVec2& button_min, const ImVec2& button_max) {
  if (count <= 0) {
    return;
  }
  const std::string text = count > 99 ? "99+" : std::to_string(count);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
  const float width = std::max(20.0f, text_size.x + 10.0f);
  const ImVec2 min(button_max.x - width + 3.0f, button_min.y - 7.0f);
  const ImVec2 max(button_max.x + 5.0f, button_min.y + 13.0f);
  draw->AddRectFilled(min, max, IM_COL32(255, 107, 53, 235), 10.0f);
  draw->AddText(ImVec2(min.x + (width - text_size.x) * 0.5f, min.y + 2.0f),
                IM_COL32(255, 255, 255, 255),
                text.c_str());
}

enum class HudIcon {
  Chat,
  Activity,
  Hint,
};

void DrawHudIcon(ImDrawList* draw, HudIcon icon, const ImVec2& center, ImU32 color) {
  switch (icon) {
    case HudIcon::Chat: {
      const ImVec2 min(center.x - 10.0f, center.y - 8.0f);
      const ImVec2 max(center.x + 10.0f, center.y + 6.0f);
      draw->AddRect(min, max, color, 5.0f, 0, 2.0f);
      draw->AddTriangleFilled(ImVec2(center.x - 3.0f, center.y + 6.0f),
                              ImVec2(center.x - 9.0f, center.y + 12.0f),
                              ImVec2(center.x + 3.0f, center.y + 6.0f),
                              color);
      draw->AddCircleFilled(ImVec2(center.x - 5.5f, center.y - 1.0f), 1.4f, color);
      draw->AddCircleFilled(ImVec2(center.x, center.y - 1.0f), 1.4f, color);
      draw->AddCircleFilled(ImVec2(center.x + 5.5f, center.y - 1.0f), 1.4f, color);
      break;
    }
    case HudIcon::Activity: {
      for (int index = 0; index < 3; ++index) {
        const float y = center.y - 8.0f + static_cast<float>(index) * 8.0f;
        draw->AddCircleFilled(ImVec2(center.x - 9.0f, y), 2.0f, color);
        draw->AddLine(ImVec2(center.x - 3.0f, y),
                      ImVec2(center.x + 11.0f - static_cast<float>(index) * 2.0f, y),
                      color,
                      2.2f);
      }
      break;
    }
    case HudIcon::Hint: {
      draw->AddCircle(ImVec2(center.x, center.y - 3.0f), 8.0f, color, 24, 2.0f);
      draw->AddLine(ImVec2(center.x - 4.5f, center.y + 6.0f),
                    ImVec2(center.x + 4.5f, center.y + 6.0f),
                    color,
                    2.0f);
      draw->AddLine(ImVec2(center.x - 3.0f, center.y + 10.0f),
                    ImVec2(center.x + 3.0f, center.y + 10.0f),
                    color,
                    2.0f);
      draw->AddLine(ImVec2(center.x, center.y - 16.0f),
                    ImVec2(center.x, center.y - 20.0f),
                    color,
                    1.8f);
      draw->AddLine(ImVec2(center.x - 12.0f, center.y - 13.0f),
                    ImVec2(center.x - 15.0f, center.y - 16.0f),
                    color,
                    1.8f);
      draw->AddLine(ImVec2(center.x + 12.0f, center.y - 13.0f),
                    ImVec2(center.x + 15.0f, center.y - 16.0f),
                    color,
                    1.8f);
      break;
    }
  }
}

bool HudButton(const char* label, HudIcon icon, int unread) {
  ImGui::PushID(label);
  ImGui::InvisibleButton(label, ImVec2(38.0f, 38.0f));
  const bool pressed = ImGui::IsItemClicked();
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 fill = active
                         ? IM_COL32(25, 160, 178, 184)
                         : (hovered ? IM_COL32(21, 82, 94, 154) : IM_COL32(8, 25, 32, 104));
  const ImU32 border = hovered ? IM_COL32(143, 232, 222, 178) : IM_COL32(143, 232, 222, 64);
  const ImU32 icon_color = IM_COL32(235, 250, 250, hovered ? 235 : 186);
  draw->AddCircleFilled(center, 17.0f, fill, 36);
  draw->AddCircle(center, 17.0f, border, 36, hovered ? 1.5f : 1.1f);
  DrawHudIcon(draw, icon, center, icon_color);
  DrawBadge(unread, min, max);
  if (hovered) {
    ImGui::SetTooltip("%s", label);
  }
  ImGui::PopID();
  return pressed;
}

const char* OnlineOffline(bool online) {
  return online ? "Online" : "Offline";
}

enum class StatusTone {
  Offline,
  Online,
  Neutral,
};

StatusTone OnlineTone(bool online) {
  return online ? StatusTone::Online : StatusTone::Offline;
}

bool SklmiRelevant(const BridgeRuntimeStatus& bridge_status) {
  return bridge_status.owner == BridgeOwner::Sklmi || bridge_status.migrated_game;
}

void DrawStatusDot(ImDrawList* draw, const ImVec2& center, StatusTone tone) {
  const ImU32 fill = tone == StatusTone::Online
                         ? IM_COL32(94, 234, 212, 255)
                         : (tone == StatusTone::Offline ? IM_COL32(255, 107, 107, 255)
                                                        : IM_COL32(148, 163, 184, 220));
  const ImU32 glow = tone == StatusTone::Online
                         ? IM_COL32(94, 234, 212, 70)
                         : (tone == StatusTone::Offline ? IM_COL32(255, 107, 107, 70)
                                                        : IM_COL32(148, 163, 184, 48));
  draw->AddCircleFilled(center, 7.0f, glow, 24);
  draw->AddCircleFilled(center, 3.3f, fill, 18);
}

void DrawTitleBar(const BridgeRuntimeStatus& bridge_status) {
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  const float height = 34.0f;
  const bool server_online = !bridge_status.ap_host.empty() && bridge_status.ap_port != 0;
  const bool sklmi_relevant = SklmiRelevant(bridge_status);
  const bool sklmi_online = sklmi_relevant && bridge_status.sklmi_active;
  const char* sklmi_status_text = sklmi_relevant ? OnlineOffline(sklmi_online) : "N/A";
  const std::string title = std::string("Sekaiemu - [Server: ") + OnlineOffline(server_online) +
                            " - SKLMI: " + sklmi_status_text + "]";

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(std::max(320.0f, display.x), height), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.0f);
  if (ImGui::Begin("##sekaiemu-titlebar",
                   nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_NoBringToFrontOnFocus)) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + height);
    draw->AddRectFilled(min, max, IM_COL32(8, 13, 18, 224), 0.0f);
    draw->AddLine(ImVec2(min.x, max.y - 1.0f),
                  ImVec2(max.x, max.y - 1.0f),
                  IM_COL32(94, 234, 212, 88),
                  1.0f);
    draw->AddRectFilled(ImVec2(min.x + 10.0f, min.y + 8.0f),
                        ImVec2(min.x + 28.0f, min.y + 26.0f),
                        IM_COL32(94, 234, 212, 210),
                        4.0f);
    draw->AddText(ImVec2(min.x + 36.0f, min.y + 9.0f), IM_COL32(236, 245, 246, 255), title.c_str());

    const float right = max.x - 18.0f;
    const char* sklmi_text = sklmi_status_text;
    const char* server_text = OnlineOffline(server_online);
    const ImVec2 sklmi_size = ImGui::CalcTextSize(sklmi_text);
    const ImVec2 server_size = ImGui::CalcTextSize(server_text);
    const float sklmi_x = right - sklmi_size.x;
    const float server_x = sklmi_x - 58.0f - server_size.x;
    DrawStatusDot(draw, ImVec2(server_x - 11.0f, min.y + 17.0f), OnlineTone(server_online));
    draw->AddText(ImVec2(server_x, min.y + 9.0f), IM_COL32(194, 211, 214, 230), server_text);
    DrawStatusDot(draw,
                  ImVec2(sklmi_x - 11.0f, min.y + 17.0f),
                  sklmi_relevant ? OnlineTone(sklmi_online) : StatusTone::Neutral);
    draw->AddText(ImVec2(sklmi_x, min.y + 9.0f), IM_COL32(194, 211, 214, 230), sklmi_text);
  }
  ImGui::End();
}

}  // namespace

void RuntimeClientCoreHud::Configure(std::filesystem::path state_path,
                                     std::filesystem::path events_path,
                                     bool buttons_visible) {
  state_path_ = std::move(state_path);
  events_path_ = std::move(events_path);
  buttons_visible_setting_ = buttons_visible;
  last_poll_frame_ = 0;
  last_state_size_ = 0;
  toasts_.clear();
}

void RuntimeClientCoreHud::Tick(std::uint64_t frame) {
  if (state_path_.empty()) {
    return;
  }
  if (last_poll_frame_ != 0 && frame >= last_poll_frame_ &&
      frame - last_poll_frame_ < kPollFrameInterval) {
    return;
  }
  last_poll_frame_ = frame;
  PollState();
}

void RuntimeClientCoreHud::PollState() {
  std::error_code ec;
  const auto size = std::filesystem::file_size(state_path_, ec);
  if (ec) {
    return;
  }
  const auto write_time = std::filesystem::last_write_time(state_path_, ec);
  if (!ec && size == last_state_size_ && write_time == last_state_write_) {
    return;
  }
  std::ifstream input(state_path_);
  if (!input) {
    return;
  }
  try {
    const auto parsed = nlohmann::json::parse(input);
    if (!parsed.is_object()) {
      return;
    }
    chat_unread_ = JsonInt(parsed, "chatUnread");
    activity_unread_ = JsonInt(parsed, "activityUnread");
    buttons_visible_from_core_ = JsonBool(parsed, "buttonsVisible", true);
    toasts_.clear();
    const auto toasts = parsed.find("toasts");
    if (toasts != parsed.end() && toasts->is_array()) {
      for (const auto& toast : *toasts) {
        if (!toast.is_object()) {
          continue;
        }
        const auto text = TrimText(toast.value("text", ""), 120);
        if (text.empty()) {
          continue;
        }
        toasts_.push_back(RuntimeClientCoreToast{
            .id = TrimText(toast.value("id", ""), 80),
            .text = text,
            .created_at = ToastCreatedAt(toast),
        });
      }
    }
    if (toasts_.size() > kMaxVisibleToasts) {
      toasts_.erase(toasts_.begin(),
                    toasts_.begin() + static_cast<std::ptrdiff_t>(toasts_.size() - kMaxVisibleToasts));
    }
    last_state_size_ = size;
    if (!ec) {
      last_state_write_ = write_time;
    }
  } catch (...) {
  }
}

void RuntimeClientCoreHud::Render(const BridgeRuntimeStatus& bridge_status) {
  const double now = MonotonicSeconds();
  toasts_.erase(std::remove_if(toasts_.begin(),
                              toasts_.end(),
                              [now](const RuntimeClientCoreToast& toast) {
                                return now - toast.created_at > kToastLifetimeSeconds;
                              }),
                toasts_.end());

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  DrawTitleBar(bridge_status);
  if (buttons_visible_setting_ && buttons_visible_from_core_) {
    ImGui::SetNextWindowPos(ImVec2(std::max(12.0f, display.x - 136.0f), 42.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(124.0f, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("##client-core-hud-buttons",
                     nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
      if (HudButton("Chat", HudIcon::Chat, chat_unread_)) {
        WriteEvent("open_chat");
      }
      ImGui::SameLine(0.0f, 4.0f);
      if (HudButton("Activity", HudIcon::Activity, activity_unread_)) {
        WriteEvent("open_activity");
      }
      ImGui::SameLine(0.0f, 4.0f);
      if (HudButton("Hint", HudIcon::Hint, 0)) {
        WriteEvent("open_hint");
      }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  std::size_t visible_index = 0;
  for (const auto& toast : toasts_) {
    const double age = now - toast.created_at;
    if (age < 0.0 || age > kToastLifetimeSeconds || visible_index >= kMaxVisibleToasts) {
      continue;
    }
    float alpha = 1.0f;
    if (age > 3.0) {
      alpha = std::max(0.0f, 1.0f - static_cast<float>((age - 3.0) / 1.2));
    }
    const float width = std::clamp(display.x * 0.24f, 230.0f, 320.0f);
    const float height = 38.0f;
    const float x = 14.0f;
    const float bottom_margin = 18.0f;
    const float y = std::max(92.0f,
                             display.y - bottom_margin - height -
                                 static_cast<float>(visible_index) * (height + 7.0f));
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.56f * alpha);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.92f, 0.82f, 0.20f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.97f, 0.98f, 0.86f * alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    const auto name = "##client-core-toast-" + std::to_string(visible_index);
    if (ImGui::Begin(name.c_str(),
                     nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
      ImGui::SetWindowFontScale(0.82f);
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 22.0f);
      ImGui::TextUnformatted(toast.text.c_str());
      ImGui::PopTextWrapPos();
      ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ++visible_index;
  }
}

void RuntimeClientCoreHud::WriteEvent(const char* type) {
  if (events_path_.empty() || type == nullptr) {
    return;
  }
  try {
    const auto parent = events_path_.parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent);
    }
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::ofstream output(events_path_, std::ios::app);
    if (!output) {
      return;
    }
    output << nlohmann::json{{"type", type}, {"at", millis}}.dump() << "\n";
  } catch (...) {
  }
}

}  // namespace sekaiemu::spike
