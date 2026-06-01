#pragma once

#include <SDL.h>
#include <libretro.h>

#include <cstdint>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike {

enum class InputMenuRowKind {
  Controller,
  BindAll,
  Binding,
};

struct InputMenuRow {
  InputMenuRowKind kind = InputMenuRowKind::Binding;
  int binding_index = -1;
};

class InputState {
 public:
  enum class BindingKind {
    None,
    Keyboard,
    ControllerButton,
    ControllerAxisPositive,
    ControllerAxisNegative,
  };

  enum class LogicalControl : int {
    Up,
    Down,
    Left,
    Right,
    A,
    B,
    X,
    Y,
    L,
    R,
    Start,
    Select,
    AnalogLeft,
    AnalogRight,
    AnalogUp,
    AnalogDown,
    Count,
  };

  struct InputBinding {
    BindingKind kind = BindingKind::None;
    int code = -1;
  };

  void Initialize(const std::filesystem::path& save_root,
                  const std::filesystem::path& core_path,
                  const std::filesystem::path& game_path);

  void BeginFrame();
  void PollRequested();
  void SetScriptedControls(const std::vector<LogicalControl>& controls);
  bool HandleSdlEvent(const SDL_Event& event, bool menu_visible);
  int16_t Read(unsigned port, unsigned device, unsigned index, unsigned id) const;

  std::size_t InputMenuRowCount() const;
  InputMenuRow InputMenuRowAt(std::size_t index) const;
  std::string InputMenuRowLabelAt(std::size_t index) const;
  std::string InputMenuRowValueAt(std::size_t index) const;
  std::string InputMenuTooltipAt(std::size_t index) const;
  bool ActivateInputMenuRow(std::size_t index);
  bool StepInputMenuRow(std::size_t index, int delta);

  bool CaptureActive() const { return capture_active_; }
  std::string CapturePrompt() const;

 private:
  struct ControlBinding {
    LogicalControl control = LogicalControl::Up;
    std::string label;
    std::string tooltip;
    InputBinding keyboard;
    InputBinding controller;
  };

  struct ControllerDevice {
    SDL_GameController* handle = nullptr;
    SDL_JoystickID instance_id = -1;
    std::string name;
    std::string guid;
  };

  void InitializeDefaults();
  void RefreshDefaultSelection();
  void LoadBindings();
  void SaveBindings() const;

  void AddController(int device_index);
  void RemoveController(SDL_JoystickID instance_id);
  void UpdateControllerButton(const SDL_ControllerButtonEvent& event);
  void UpdateControllerAxis(const SDL_ControllerAxisEvent& event);

  bool HandleCaptureEvent(const SDL_Event& event);
  void StartCapture(std::size_t binding_index);
  void StartBindAll();
  void CompleteCapture(InputBinding binding, bool from_controller);
  void AdvanceBindAllCapture();
  void CancelCapture();

  bool ControlPressed(LogicalControl control) const;
  int16_t AnalogValue(LogicalControl negative, LogicalControl positive) const;
  bool BindingPressed(const InputBinding& binding) const;
  std::string BindingLabel(const InputBinding& binding) const;
  bool SelectControllerDelta(int delta);

  static std::string NormalizeKey(std::string_view text);
  static std::string ControlKey(LogicalControl control);
  static std::string ScancodeToken(SDL_Scancode scancode);
  static std::string ControllerBindingToken(const InputBinding& binding);
  static std::optional<InputBinding> ParseBindingToken(std::string_view token);

  std::filesystem::path config_path_;
  std::vector<ControlBinding> bindings_;
  std::vector<ControllerDevice> controllers_;
  int selected_controller_index_ = -1;
  std::string selected_controller_guid_;

  bool poll_seen_this_frame_ = false;
  std::array<bool, static_cast<std::size_t>(LogicalControl::Count)> scripted_controls_{};
  std::unordered_map<SDL_Scancode, bool> key_state_;
  std::unordered_map<int, bool> controller_button_state_;
  std::unordered_map<int, int16_t> controller_axis_state_;

  bool capture_active_ = false;
  bool bind_all_active_ = false;
  std::size_t capture_binding_index_ = 0;
  std::size_t bind_all_index_ = 0;
};

}  // namespace sekaiemu::spike
