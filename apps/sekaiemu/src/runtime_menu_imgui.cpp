#include "runtime_menu.hpp"

#include "tracker_runtime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {
namespace {

constexpr ImVec2 kMenuSize{860.0f, 600.0f};
constexpr float kHeaderHeight = 44.0f;
constexpr float kNavHeight = 58.0f;
constexpr float kFooterHeight = 30.0f;

constexpr ImVec4 kBg{0.027f, 0.082f, 0.118f, 1.000f};
constexpr ImVec4 kHeader{0.047f, 0.145f, 0.200f, 1.000f};
constexpr ImVec4 kPanel{0.071f, 0.149f, 0.204f, 1.000f};
constexpr ImVec4 kPanelSoft{0.086f, 0.188f, 0.255f, 1.000f};
constexpr ImVec4 kBorder{0.184f, 0.384f, 0.467f, 1.000f};
constexpr ImVec4 kAccent{0.133f, 0.827f, 0.933f, 1.000f};
constexpr ImVec4 kAccentDark{0.055f, 0.455f, 0.565f, 1.000f};
constexpr ImVec4 kMuted{0.596f, 0.776f, 0.824f, 1.000f};
constexpr ImVec4 kButton{0.333f, 0.388f, 0.400f, 0.960f};

struct RuntimeMenuTab {
  const char* label = "";
  RuntimeMenu::Page page = RuntimeMenu::Page::Main;
};

std::vector<RuntimeMenuTab> VisibleTabs(RuntimeSettingsMode mode) {
  std::vector<RuntimeMenuTab> tabs{
      {"[G] General", RuntimeMenu::Page::Main},
      {"[S] Settings", RuntimeMenu::Page::Settings},
      {"[I] Input", RuntimeMenu::Page::InputSettings},
  };
  if (mode == RuntimeSettingsMode::Advanced) {
    tabs.push_back({"[C] Core", RuntimeMenu::Page::CoreSettings});
  }
  tabs.push_back({"[B] Bridge", RuntimeMenu::Page::BridgeStatus});
  tabs.push_back({"[Y] Sync", RuntimeMenu::Page::SyncInfo});
  return tabs;
}

bool ContainsPage(const std::vector<RuntimeMenuTab>& tabs, RuntimeMenu::Page page) {
  return std::any_of(tabs.begin(), tabs.end(), [page](const RuntimeMenuTab& tab) {
    return tab.page == page;
  });
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

std::string DisplayModeLabel(TrackerDisplayMode mode) {
  switch (mode) {
    case TrackerDisplayMode::SplitScreen:
      return "Side-by-side";
    case TrackerDisplayMode::SeparateWindow:
      return "Separate window";
    case TrackerDisplayMode::PipOverlay:
      return "Picture-in-picture";
    case TrackerDisplayMode::ToggleScreen:
      return "Toggle button";
  }
  return "Side-by-side";
}

void TextMuted(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
  ImGui::TextV(fmt, args);
  ImGui::PopStyleColor();
  va_end(args);
}

void SectionLine() {
  ImGui::Dummy(ImVec2(1.0f, 8.0f));
  ImGui::Separator();
  ImGui::Dummy(ImVec2(1.0f, 8.0f));
}

void WrappedMuted(std::string_view text, float wrap_width = 0.0f) {
  ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
  const float wrap = wrap_width > 0.0f ? ImGui::GetCursorPosX() + wrap_width : ImGui::GetWindowWidth() - 18.0f;
  ImGui::PushTextWrapPos(wrap);
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

void PushSekaiButton(ImVec4 color = kButton) {
  ImGui::PushStyleColor(ImGuiCol_Button, color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentDark);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
}

void PopSekaiButton() {
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
}

bool ActionButton(const char* label,
                  RuntimeMenuAction action,
                  RuntimeMenuAction& pending,
                  const ImVec2& size = ImVec2(-1.0f, 40.0f),
                  ImVec4 color = kButton) {
  PushSekaiButton(color);
  const bool pressed = ImGui::Button(label, size);
  PopSekaiButton();
  if (pressed) {
    pending = action;
  }
  return pressed;
}

bool PositionedActionButton(const char* label,
                            RuntimeMenuAction action,
                            RuntimeMenuAction& pending,
                            const ImVec2& pos,
                            const ImVec2& size,
                            ImVec4 color = kButton) {
  ImGui::SetCursorPos(pos);
  return ActionButton(label, action, pending, size, color);
}

bool DrawToggleBox(const char* label, bool current, RuntimeMenuAction action, RuntimeMenuAction& pending) {
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float square = 20.0f;
  const float label_width = ImGui::CalcTextSize(label).x;
  const ImVec2 total_size(square + 10.0f + label_width, 28.0f);
  ImGui::InvisibleButton(label, total_size);
  const bool pressed = ImGui::IsItemClicked();
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 fill = ImGui::ColorConvertFloat4ToU32(current ? kAccentDark : kPanelSoft);
  const ImU32 border = ImGui::ColorConvertFloat4ToU32(hovered ? kAccent : kBorder);
  const ImVec2 min = ImVec2(start.x, start.y + 4.0f);
  const ImVec2 max = ImVec2(start.x + square, start.y + 4.0f + square);
  draw->AddRectFilled(min, max, fill, 4.0f);
  draw->AddRect(min, max, border, 4.0f, 0, 1.2f);
  if (current) {
    const ImU32 check = ImGui::ColorConvertFloat4ToU32(ImVec4(0.82f, 1.0f, 1.0f, 1.0f));
    draw->AddLine(ImVec2(start.x + 5.0f, start.y + 14.0f), ImVec2(start.x + 9.0f, start.y + 18.0f), check, 2.2f);
    draw->AddLine(ImVec2(start.x + 9.0f, start.y + 18.0f), ImVec2(start.x + 16.0f, start.y + 9.0f), check, 2.2f);
  }
  draw->AddText(ImVec2(start.x + square + 10.0f, start.y + 5.0f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.937f, 0.992f, 1.0f, 1.0f)),
                label);
  if (pressed && action != RuntimeMenuAction::None) {
    pending = action;
  }
  return pressed;
}

void DrawPercentSlider(const char* label,
                       int percent,
                       RuntimeMenuAction decrease,
                       RuntimeMenuAction increase,
                       RuntimeMenuAction& pending) {
  ImGui::TextUnformatted(label);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float width = 332.0f;
  const float height = 16.0f;
  const float y = start.y + 8.0f;
  ImGui::InvisibleButton("##slider-hit", ImVec2(width, 30.0f));
  if (ImGui::IsItemClicked()) {
    const float local_x = std::clamp(ImGui::GetIO().MousePos.x - start.x, 0.0f, width);
    const int desired = static_cast<int>(std::round((local_x / width) * 150.0f));
    pending = desired >= percent ? increase : decrease;
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 rail_min(start.x, y);
  const ImVec2 rail_max(start.x + width, y + 8.0f);
  const float fill_width = width * static_cast<float>(std::clamp(percent, 0, 150)) / 150.0f;
  draw->AddRectFilled(rail_min, rail_max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.047f, 0.145f, 0.200f, 1.0f)), 4.0f);
  draw->AddRect(rail_min, rail_max, ImGui::ColorConvertFloat4ToU32(kBorder), 4.0f, 0, 1.2f);
  draw->AddRectFilled(rail_min, ImVec2(start.x + fill_width, rail_max.y), ImGui::ColorConvertFloat4ToU32(kAccent), 4.0f);
  draw->AddCircleFilled(ImVec2(start.x + fill_width, y + 4.0f), 7.0f, ImGui::ColorConvertFloat4ToU32(kAccent));
  const std::string value = std::to_string(percent) + "%";
  draw->AddText(ImVec2(start.x + width + 22.0f, start.y + 1.0f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.937f, 0.992f, 1.0f, 1.0f)),
                value.c_str());
}

void DrawTabButton(const RuntimeMenuTab& tab, RuntimeMenu::Page& page, int index) {
  const bool active = page == tab.page;
  ImGui::PushID(index);
  PushSekaiButton(active ? ImVec4(0.071f, 0.541f, 0.640f, 1.0f) : kBg);
  if (!active) {
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
  }
  if (ImGui::Button(tab.label, ImVec2(114.0f, 36.0f)) && !active) {
    page = tab.page;
  }
  if (!active) {
    ImGui::PopStyleColor();
  }
  PopSekaiButton();
  ImGui::PopID();
}

void SaveSlotCard(const SaveStateSlotMenuInfo& slot, RuntimeMenuAction& pending) {
  ImGui::PushID(slot.slot_index);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelSoft);
  ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
  ImGui::BeginChild("slot-card", ImVec2(545.0f, 88.0f), true, ImGuiWindowFlags_NoScrollbar);
  ImGui::Text("Slot %d", std::max(0, slot.slot_index - 1));
  TextMuted("%s", slot.has_state ? (slot.created_at.empty() ? "Ready" : slot.created_at.c_str()) : "Empty");
  if (slot.has_state) {
    PositionedActionButton("Load",
                           static_cast<RuntimeMenuAction>(static_cast<int>(RuntimeMenuAction::LoadStateSlot1) +
                                                          std::clamp(slot.slot_index - 1, 0, 3)),
                           pending,
                           ImVec2(442.0f, 26.0f),
                           ImVec2(86.0f, 26.0f),
                           kAccent);
  }
  PositionedActionButton("Save",
                         static_cast<RuntimeMenuAction>(static_cast<int>(RuntimeMenuAction::SaveStateSlot1) +
                                                        std::clamp(slot.slot_index - 1, 0, 3)),
                         pending,
                         ImVec2(442.0f, 55.0f),
                         ImVec2(86.0f, 26.0f),
                         kAccent);
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
  ImGui::PopID();
}

void DrawGeneralPage(RuntimeMenuAction& pending, std::span<const SaveStateSlotMenuInfo> save_slots) {
  ImGui::Columns(2, "general-columns", false);
  ImGui::SetColumnWidth(0, 580.0f);

  ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelSoft);
  ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
  ImGui::BeginChild("autosave-card", ImVec2(545.0f, 88.0f), true, ImGuiWindowFlags_NoScrollbar);
  ImGui::TextUnformatted("Auto Save");
  TextMuted("Latest automatic state");
  PositionedActionButton("Load", RuntimeMenuAction::LoadState, pending, ImVec2(442.0f, 55.0f), ImVec2(86.0f, 26.0f), kAccent);
  ImGui::EndChild();
  ImGui::PopStyleColor(2);

  int shown_slots = 0;
  for (const auto& slot : save_slots) {
    if (slot.slot_index >= 1 && slot.slot_index <= 4) {
      SaveSlotCard(slot, pending);
      ++shown_slots;
      if (shown_slots >= 3) {
        break;
      }
    }
  }

  ImGui::NextColumn();
  ImGui::Dummy(ImVec2(1.0f, 2.0f));
  ActionButton("Resume", RuntimeMenuAction::CloseMenu, pending, ImVec2(214.0f, 40.0f));
  ActionButton("Reset Core", RuntimeMenuAction::ResetCore, pending, ImVec2(214.0f, 40.0f));
  ActionButton("Switch Game", RuntimeMenuAction::ChangeGame, pending, ImVec2(214.0f, 40.0f));
  ActionButton("Quit", RuntimeMenuAction::QuitRuntime, pending, ImVec2(214.0f, 40.0f));
  ImGui::Columns(1);
}

void DrawToggle(const char* label, bool current, RuntimeMenuAction action, RuntimeMenuAction& pending) {
  DrawToggleBox(label, current, action, pending);
}

void DrawSettingsPage(RuntimeMenu& menu,
                      RuntimeMenuAction& pending,
                      int master_volume_percent,
                      bool chat_overlay_enabled,
                      bool notifications_enabled,
                      bool activity_feed_enabled,
                      bool bridge_terminal_enabled,
                      const TrackerRuntime* tracker_runtime) {
  DrawPercentSlider("Master Volume",
                    master_volume_percent,
                    RuntimeMenuAction::DecreaseMasterVolume,
                    RuntimeMenuAction::IncreaseMasterVolume,
                    pending);
  SectionLine();

  bool advanced = menu.SettingsMode() == RuntimeSettingsMode::Advanced;
  ImGui::SetCursorPosX(34.0f);
  if (DrawToggleBox("Advanced mode", advanced, RuntimeMenuAction::None, pending)) {
    menu.SetSettingsMode(!advanced ? RuntimeSettingsMode::Advanced : RuntimeSettingsMode::Easy);
  }
  ImGui::SameLine(260.0f);
  DrawToggle("Runtime HUD", chat_overlay_enabled, RuntimeMenuAction::ToggleChatOverlay, pending);
  ImGui::SameLine(492.0f);
  DrawToggle("Enable Notifications", notifications_enabled, RuntimeMenuAction::ToggleNotifications, pending);
  SectionLine();

  ImGui::SetCursorPosX(34.0f);
  DrawToggle("Activity Feed", activity_feed_enabled, RuntimeMenuAction::ToggleActivityFeed, pending);
  ImGui::SameLine(260.0f);
  if (tracker_runtime != nullptr) {
    const auto& ui = tracker_runtime->UiState();
    DrawToggle("Show Tracker", ui.show_tracker_screen, RuntimeMenuAction::ToggleTrackerScreen, pending);
    ImGui::SameLine(492.0f);
    DrawToggle("Tracker Auto-follow",
               tracker_runtime->LocalOverrideState().auto_follow_map,
               RuntimeMenuAction::ToggleTrackerAutoFollow,
               pending);
    ImGui::SetCursorPosX(34.0f);
    ActionButton("Cycle Tracker Layout", RuntimeMenuAction::CycleTrackerDisplayMode, pending, ImVec2(210.0f, 36.0f), kAccent);
    TextMuted("Tracker mode: %s", DisplayModeLabel(ui.display_mode).c_str());
  } else {
    TextMuted("Tracker is not loaded for this game.");
  }
  SectionLine();

  ImGui::SetCursorPosX(34.0f);
  ActionButton("Cycle Window Mode", RuntimeMenuAction::CycleWindowMode, pending, ImVec2(210.0f, 36.0f), kAccent);
  TextMuted("Hotkey: F12 or Alt+Enter. Default mode: Borderless Window.");
  SectionLine();

  ImGui::SetCursorPosX(34.0f);
  DrawToggle("Bridge Terminal", bridge_terminal_enabled, RuntimeMenuAction::ToggleBridgeTerminal, pending);
  ImGui::SameLine(260.0f);
  ActionButton("Restart Bridge", RuntimeMenuAction::RestartBridge, pending, ImVec2(180.0f, 36.0f), kAccent);
}

void DrawCorePage(const CoreOptionManager& core_options, RuntimeMenuAction& pending) {
  if (core_options.DefinitionCount() == 0) {
    TextMuted("This core did not expose configurable options.");
    return;
  }
  ImGui::BeginChild("core-options", ImVec2(0.0f, 374.0f), false);
  for (std::size_t index = 0; index < core_options.DefinitionCount(); ++index) {
    const auto* definition = core_options.DefinitionAt(index);
    if (definition == nullptr) {
      continue;
    }
    ImGui::PushID(definition->key.c_str());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelSoft);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
    ImGui::BeginChild("core-row", ImVec2(0.0f, 62.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextWrapped("%s%s", definition->desc.empty() ? definition->key.c_str() : definition->desc.c_str(),
                       definition->requires_restart ? " *" : "");
    if (!definition->info.empty()) {
      TextMuted("%s", Truncate(definition->info, 88).c_str());
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 248.0f);
    if (ImGui::SmallButton("<")) {
      const_cast<CoreOptionManager&>(core_options).StepValue(definition->key, -1);
    }
    ImGui::SameLine();
    ImGui::TextColored(kAccent, "%s", core_options.CurrentValueFor(definition->key).c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 36.0f);
    if (ImGui::SmallButton(">")) {
      const_cast<CoreOptionManager&>(core_options).StepValue(definition->key, 1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
  }
  ImGui::EndChild();
  ActionButton("Apply Core Settings", RuntimeMenuAction::ApplyCoreSettings, pending, ImVec2(200.0f, 38.0f), kAccent);
  ImGui::SameLine();
  ActionButton("Reset to Defaults", RuntimeMenuAction::ResetCoreSettingsToDefaults, pending, ImVec2(190.0f, 38.0f));
}

void DrawInputPage(InputState& input_state) {
  if (input_state.CaptureActive()) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.176f, 0.129f, 0.067f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.86f, 0.58f, 0.22f, 1.0f));
    ImGui::BeginChild("capture-prompt", ImVec2(0.0f, 44.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(ImVec4(0.96f, 0.72f, 0.34f, 1.0f), "%s", input_state.CapturePrompt().c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    SectionLine();
  } else {
    WrappedMuted("Choose a controller with Previous/Next, then change a single binding or bind all controls in order.");
    SectionLine();
  }

  ImGui::BeginChild("input-rows", ImVec2(0.0f, input_state.CaptureActive() ? 368.0f : 386.0f), true);
  constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("input-table", 3, flags, ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, 160.0f);
    ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableHeadersRow();

    for (std::size_t index = 0; index < input_state.InputMenuRowCount(); ++index) {
      const auto row = input_state.InputMenuRowAt(index);
      const std::string label = input_state.InputMenuRowLabelAt(index);
      const std::string value = input_state.InputMenuRowValueAt(index);
      const std::string tooltip = input_state.InputMenuTooltipAt(index);

      ImGui::PushID(static_cast<int>(index));
      ImGui::TableNextRow(ImGuiTableRowFlags_None, 42.0f);
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(label.c_str());
      if (!tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip.c_str());
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::PushStyleColor(ImGuiCol_Text, row.kind == InputMenuRowKind::Controller ? kAccent : kMuted);
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(value.c_str());
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();

      ImGui::TableSetColumnIndex(2);
      if (row.kind == InputMenuRowKind::Controller) {
        PushSekaiButton();
        if (ImGui::Button("Previous", ImVec2(82.0f, 30.0f))) {
          input_state.StepInputMenuRow(index, -1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next", ImVec2(82.0f, 30.0f))) {
          input_state.StepInputMenuRow(index, 1);
        }
        PopSekaiButton();
      } else if (row.kind == InputMenuRowKind::BindAll) {
        PushSekaiButton(kAccent);
        if (ImGui::Button("Bind all", ImVec2(172.0f, 30.0f))) {
          input_state.ActivateInputMenuRow(index);
        }
        PopSekaiButton();
      } else {
        PushSekaiButton(kAccent);
        if (ImGui::Button("Change", ImVec2(172.0f, 30.0f))) {
          input_state.ActivateInputMenuRow(index);
        }
        PopSekaiButton();
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
}

void DrawBridgePage(const BridgeRuntimeStatus& bridge_status, bool bridge_terminal_enabled, RuntimeMenuAction& pending) {
  ImGui::Columns(2, "bridge-columns", false);
  ImGui::SetColumnWidth(0, 190.0f);
  ImGui::TextUnformatted("Bridge");
  ImGui::NextColumn();
  TextMuted("%s", bridge_status.owner == BridgeOwner::Sklmi ? "SKLMI" : "Legacy");
  ImGui::NextColumn();
  ImGui::TextUnformatted("Game");
  ImGui::NextColumn();
  TextMuted("%s", bridge_status.game_name.c_str());
  ImGui::NextColumn();
  ImGui::TextUnformatted("Server");
  ImGui::NextColumn();
  TextMuted("%s:%u%s", bridge_status.ap_host.c_str(), bridge_status.ap_port, bridge_status.ap_path.c_str());
  ImGui::NextColumn();
  ImGui::TextUnformatted("Slot");
  ImGui::NextColumn();
  TextMuted("%s", bridge_status.ap_slot_name.c_str());
  ImGui::Columns(1);
  if (!bridge_status.last_error.empty()) {
    SectionLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.44f, 0.40f, 1.0f));
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 18.0f);
    ImGui::TextUnformatted(bridge_status.last_error.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  SectionLine();
  ActionButton("Restart Bridge", RuntimeMenuAction::RestartBridge, pending, ImVec2(190.0f, 38.0f), kAccent);
  ImGui::SameLine();
  ActionButton(bridge_terminal_enabled ? "Close Bridge Terminal" : "Open Bridge Terminal",
               RuntimeMenuAction::ToggleBridgeTerminal,
               pending,
               ImVec2(230.0f, 38.0f));
}

void DrawSyncPage(const BridgeRuntimeStatus& bridge_status) {
  const std::array<std::pair<const char*, const std::string*>, 3> rows{{
      {"Runtime memory socket", &bridge_status.runtime_memory_socket_path},
      {"Room state", &bridge_status.room_state_path},
      {"Companion log", &bridge_status.companion_log_path},
  }};
  for (const auto& [label, value] : rows) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanelSoft);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
    ImGui::BeginChild(label, ImVec2(0.0f, 74.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted(label);
    WrappedMuted(value != nullptr ? *value : std::string{});
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
  }
}

}  // namespace

RuntimeMenuAction RuntimeMenu::ConsumePendingAction() {
  const RuntimeMenuAction action = pending_action_;
  pending_action_ = RuntimeMenuAction::None;
  return action;
}

void RuntimeMenu::RenderImGui(const CoreOptionManager& core_options,
                              InputState& input_state,
                              const BridgeRuntimeStatus& bridge_status,
                              const TrackerRuntime* tracker_runtime,
                              const std::vector<SaveStateSlotMenuInfo>& save_slots,
                              std::string_view core_name,
                              std::string_view game_name,
                              int master_volume_percent,
                              bool chat_overlay_enabled,
                              bool notifications_enabled,
                              bool activity_feed_enabled,
                              bool bridge_terminal_enabled) {
  if (!visible_) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - kMenuSize.x) * 0.5f,
                                 (io.DisplaySize.y - kMenuSize.y) * 0.5f),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(kMenuSize, ImGuiCond_Always);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

  const std::string title = "Sekaiemu Settings [Core: " + std::string(core_name) + "]";
  if (ImGui::Begin("##sekaiemu-settings-window", nullptr, flags)) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(kBg), 10.0f);
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + kHeaderHeight), ImGui::ColorConvertFloat4ToU32(kHeader), 10.0f, ImDrawFlags_RoundCornersTop);
    draw->AddRectFilled(ImVec2(pos.x, pos.y + kHeaderHeight),
                        ImVec2(pos.x + size.x, pos.y + kHeaderHeight + kNavHeight),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(0.019f, 0.075f, 0.100f, 1.0f)));
    draw->AddLine(ImVec2(pos.x, pos.y + kHeaderHeight + kNavHeight),
                  ImVec2(pos.x + size.x, pos.y + kHeaderHeight + kNavHeight),
                  ImGui::ColorConvertFloat4ToU32(kBorder),
                  1.0f);
    draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(kBorder), 10.0f, 0, 1.4f);

    ImGui::SetCursorPos(ImVec2(18.0f, 12.0f));
    ImGui::TextUnformatted(title.c_str());

    ImGui::SetCursorPos(ImVec2(16.0f, kHeaderHeight + 10.0f));
    const auto tabs = VisibleTabs(settings_mode_);
    if (!ContainsPage(tabs, page_)) {
      page_ = Page::Main;
      selected_index_ = 0;
      scroll_offset_ = 0;
      main_slot_mode_ = MainSlotMode::None;
    }
    for (std::size_t index = 0; index < tabs.size(); ++index) {
      DrawTabButton(tabs[index], page_, static_cast<int>(index));
      if (index + 1 < tabs.size()) {
        ImGui::SameLine();
      }
    }
    ImGui::SetCursorPos(ImVec2(28.0f, kHeaderHeight + kNavHeight + 28.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.071f, 0.149f, 0.204f, 0.0f));
    ImGui::BeginChild("content", ImVec2(kMenuSize.x - 56.0f, kMenuSize.y - kHeaderHeight - kNavHeight - kFooterHeight - 44.0f), true);
    switch (page_) {
      case Page::Main:
        DrawGeneralPage(pending_action_, save_slots);
        break;
      case Page::Settings:
        DrawSettingsPage(*this,
                         pending_action_,
                         master_volume_percent,
                         chat_overlay_enabled,
                         notifications_enabled,
                         activity_feed_enabled,
                         bridge_terminal_enabled,
                         tracker_runtime);
        break;
      case Page::CoreSettings:
        DrawCorePage(core_options, pending_action_);
        break;
      case Page::InputSettings:
        DrawInputPage(input_state);
        break;
      case Page::BridgeStatus:
        DrawBridgePage(bridge_status, bridge_terminal_enabled, pending_action_);
        break;
      case Page::SyncInfo:
        DrawSyncPage(bridge_status);
        break;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(kMenuSize.x - 260.0f, kMenuSize.y - 32.0f));
    ImGui::TextUnformatted("Sekaiemu CANONICAL-BETA3-0.1.0");
    ImGui::SetCursorPos(ImVec2(28.0f, kMenuSize.y - 32.0f));
    TextMuted("%.*s", static_cast<int>(game_name.size()), game_name.data());
  }
  ImGui::End();
}

}  // namespace sekaiemu::spike
