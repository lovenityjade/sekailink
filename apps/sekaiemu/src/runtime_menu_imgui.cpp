#include "runtime_menu.hpp"

#include "tracker_runtime.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace sekaiemu::spike {
namespace {

struct RuntimeMenuTab {
  const char* label = "";
  RuntimeMenu::Page page = RuntimeMenu::Page::Main;
};

std::vector<RuntimeMenuTab> VisibleTabs(RuntimeSettingsMode mode) {
  std::vector<RuntimeMenuTab> tabs{
      {"Main", RuntimeMenu::Page::Main},
      {"Settings", RuntimeMenu::Page::Settings},
      {"Input", RuntimeMenu::Page::InputSettings},
  };
  if (mode == RuntimeSettingsMode::Advanced) {
    tabs.push_back({"Core", RuntimeMenu::Page::CoreSettings});
  }
  tabs.push_back({"Bridge", RuntimeMenu::Page::BridgeStatus});
  tabs.push_back({"Sync", RuntimeMenu::Page::SyncInfo});
  return tabs;
}

bool ContainsPage(const std::vector<RuntimeMenuTab>& tabs, RuntimeMenu::Page page) {
  return std::any_of(tabs.begin(), tabs.end(), [page](const RuntimeMenuTab& tab) {
    return tab.page == page;
  });
}

void HelpMarker(const char* text) {
  ImGui::SameLine();
  ImGui::TextDisabled("?");
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    ImGui::SetTooltip("%s", text);
  }
}

void ActionButton(const char* label, RuntimeMenuAction action, RuntimeMenuAction& pending) {
  if (ImGui::Button(label, ImVec2(-1.0f, 0.0f))) {
    pending = action;
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
                              bool bridge_terminal_enabled) {
  if (!visible_) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(28.0f, 28.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(std::max(720.0f, io.DisplaySize.x * 0.54f),
                                  std::max(560.0f, io.DisplaySize.y - 56.0f)),
                           ImGuiCond_Always);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
  if (ImGui::Begin("Sekaiemu", nullptr, flags)) {
    ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "Sekaiemu Runtime");
    ImGui::SameLine();
    ImGui::TextDisabled("%.*s", static_cast<int>(game_name.size()), game_name.data());
    ImGui::Separator();

    const auto tabs = VisibleTabs(settings_mode_);
    if (!ContainsPage(tabs, page_)) {
      page_ = Page::Main;
      selected_index_ = 0;
      scroll_offset_ = 0;
      main_slot_mode_ = MainSlotMode::None;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    for (std::size_t index = 0; index < tabs.size(); ++index) {
      const auto& tab = tabs[index];
      const bool active = page_ == tab.page;
      ImGui::PushID(static_cast<int>(index));
      if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.31f, 0.80f, 0.77f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.31f, 0.80f, 0.77f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.02f, 0.05f, 0.07f, 1.0f));
      }
      if (ImGui::Button(tab.label, ImVec2(112.0f, 34.0f)) && !active) {
        page_ = tab.page;
        selected_index_ = 0;
        scroll_offset_ = 0;
        main_slot_mode_ = MainSlotMode::None;
      }
      if (active) {
        ImGui::PopStyleColor(3);
      }
      ImGui::PopID();
      if (index + 1 < tabs.size()) {
        ImGui::SameLine();
      }
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    ImGui::Spacing();
    if (page_ == Page::Main) {
      ImGui::Columns(2, nullptr, false);
      ImGui::Text("Game");
      ImGui::TextDisabled("%.*s", static_cast<int>(game_name.size()), game_name.data());
      ImGui::Text("Core");
      ImGui::TextDisabled("%.*s", static_cast<int>(core_name.size()), core_name.data());
      ImGui::Text("Tracker");
      ImGui::TextDisabled("%s", tracker_runtime != nullptr && tracker_runtime->Bundle() != nullptr ? "loaded" : "offline");
      ImGui::NextColumn();
      ActionButton("Resume", RuntimeMenuAction::CloseMenu, pending_action_);
      ActionButton("Reset Core", RuntimeMenuAction::ResetCore, pending_action_);
      ActionButton("Load Autosave", RuntimeMenuAction::LoadState, pending_action_);
      ActionButton("Save Battery", RuntimeMenuAction::SaveBattery, pending_action_);
      ActionButton("Quit Sekaiemu", RuntimeMenuAction::QuitRuntime, pending_action_);
      ImGui::Columns(1);

      ImGui::SeparatorText("Save States");
      for (const auto& slot : save_slots) {
        ImGui::PushID(slot.slot_index);
        ImGui::Text("Slot %d", slot.slot_index);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", slot.has_state ? slot.created_at.c_str() : "empty");
        ImGui::SameLine();
        if (ImGui::SmallButton("Save")) {
          pending_action_ = static_cast<RuntimeMenuAction>(
              static_cast<int>(RuntimeMenuAction::SaveStateSlot1) + std::clamp(slot.slot_index - 1, 0, 3));
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Load")) {
          pending_action_ = static_cast<RuntimeMenuAction>(
              static_cast<int>(RuntimeMenuAction::LoadStateSlot1) + std::clamp(slot.slot_index - 1, 0, 3));
        }
        ImGui::PopID();
      }
    } else if (page_ == Page::Settings) {
      bool advanced = settings_mode_ == RuntimeSettingsMode::Advanced;
      if (ImGui::Checkbox("Advanced mode", &advanced)) {
        SetSettingsMode(advanced ? RuntimeSettingsMode::Advanced : RuntimeSettingsMode::Easy);
      }
      ImGui::Text("Master volume");
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "%d%%", master_volume_percent);
      if (ImGui::Button("-")) pending_action_ = RuntimeMenuAction::DecreaseMasterVolume;
      ImGui::SameLine();
      if (ImGui::Button("+")) pending_action_ = RuntimeMenuAction::IncreaseMasterVolume;
      ImGui::Separator();
      if (ImGui::Checkbox("Chat overlay", &chat_overlay_enabled)) pending_action_ = RuntimeMenuAction::ToggleChatOverlay;
      if (ImGui::Checkbox("Notifications", &notifications_enabled)) pending_action_ = RuntimeMenuAction::ToggleNotifications;
      ActionButton("Cycle tracker display mode", RuntimeMenuAction::CycleTrackerDisplayMode, pending_action_);
      ActionButton("Toggle tracker screen", RuntimeMenuAction::ToggleTrackerScreen, pending_action_);
      ActionButton("Toggle tracker auto-follow", RuntimeMenuAction::ToggleTrackerAutoFollow, pending_action_);
    } else if (page_ == Page::CoreSettings) {
      if (core_options.DefinitionCount() == 0) {
        ImGui::TextDisabled("This core did not expose configurable options.");
      }
      for (std::size_t index = 0; index < core_options.DefinitionCount(); ++index) {
        const auto* definition = core_options.DefinitionAt(index);
        if (definition == nullptr) continue;
        ImGui::PushID(definition->key.c_str());
        ImGui::TextWrapped("%s", definition->desc.c_str());
        if (!definition->info.empty()) HelpMarker(definition->info.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("<")) {
          const_cast<CoreOptionManager&>(core_options).StepValue(definition->key, -1);
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "%s",
                           core_options.CurrentValueFor(definition->key).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) {
          const_cast<CoreOptionManager&>(core_options).StepValue(definition->key, 1);
        }
        ImGui::PopID();
      }
      ImGui::Separator();
      ActionButton("Apply core settings", RuntimeMenuAction::ApplyCoreSettings, pending_action_);
      ActionButton("Reset core settings to defaults", RuntimeMenuAction::ResetCoreSettingsToDefaults, pending_action_);
    } else if (page_ == Page::InputSettings) {
      if (input_state.CaptureActive()) {
        ImGui::TextColored(ImVec4(0.92f, 0.38f, 0.22f, 1.0f), "%s", input_state.CapturePrompt().c_str());
      }
      for (std::size_t index = 0; index < input_state.InputMenuRowCount(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        ImGui::Text("%s", input_state.InputMenuRowLabelAt(index).c_str());
        HelpMarker(input_state.InputMenuTooltipAt(index).c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "%s", input_state.InputMenuRowValueAt(index).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Change")) {
          input_state.ActivateInputMenuRow(index);
        }
        ImGui::PopID();
      }
    } else if (page_ == Page::BridgeStatus) {
      ImGui::Text("Bridge: %s", bridge_status.owner == BridgeOwner::Sklmi ? "SKLMI" : "Legacy");
      ImGui::Text("Game: %s", bridge_status.game_name.c_str());
      ImGui::Text("AP: %s:%u%s", bridge_status.ap_host.c_str(), bridge_status.ap_port, bridge_status.ap_path.c_str());
      ImGui::Text("Slot: %s", bridge_status.ap_slot_name.c_str());
      if (!bridge_status.last_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", bridge_status.last_error.c_str());
      }
      ActionButton(bridge_terminal_enabled ? "Close bridge terminal" : "Open bridge terminal",
                   RuntimeMenuAction::ToggleBridgeTerminal,
                   pending_action_);
      ActionButton("Restart bridge", RuntimeMenuAction::RestartBridge, pending_action_);
    } else if (page_ == Page::SyncInfo) {
      ImGui::TextWrapped("Runtime memory socket");
      ImGui::TextDisabled("%s", bridge_status.runtime_memory_socket_path.c_str());
      ImGui::TextWrapped("Room state");
      ImGui::TextDisabled("%s", bridge_status.room_state_path.c_str());
      ImGui::TextWrapped("Companion log");
      ImGui::TextDisabled("%s", bridge_status.companion_log_path.c_str());
    }
  }
  ImGui::End();
}

}  // namespace sekaiemu::spike
