#include "input_state.hpp"
#include "input_state_internal.hpp"

#include <array>
#include <algorithm>

namespace sekaiemu::spike {

using namespace input_state_internal;

void InputState::Initialize(const std::filesystem::path& save_root,
                            const std::filesystem::path& core_path,
                            const std::filesystem::path&) {
  const auto core_key = NormalizeKey(core_path.stem().string());
  config_path_ = save_root / kInputConfigDir / (core_key + ".cfg");
  InitializeDefaults();

  if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
  }

  const int joystick_count = SDL_NumJoysticks();
  for (int index = 0; index < joystick_count; ++index) {
    AddController(index);
  }
  LoadBindings();
  RefreshDefaultSelection();
}

void InputState::BeginFrame() {
  poll_seen_this_frame_ = false;
  scripted_controls_.fill(false);
}

void InputState::PollRequested() { poll_seen_this_frame_ = true; }

void InputState::SetScriptedControls(const std::vector<LogicalControl>& controls) {
  scripted_controls_.fill(false);
  for (const auto control : controls) {
    const auto index = static_cast<std::size_t>(control);
    if (index < scripted_controls_.size()) {
      scripted_controls_[index] = true;
    }
  }
}

bool InputState::HandleSdlEvent(const SDL_Event& event, bool) {
  if (capture_active_ && HandleCaptureEvent(event)) {
    return true;
  }

  switch (event.type) {
    case SDL_KEYDOWN:
      if (!event.key.repeat) {
        key_state_[event.key.keysym.scancode] = true;
      }
      return false;
    case SDL_KEYUP:
      key_state_[event.key.keysym.scancode] = false;
      return false;
    case SDL_CONTROLLERDEVICEADDED:
      AddController(event.cdevice.which);
      return false;
    case SDL_CONTROLLERDEVICEREMOVED:
      RemoveController(event.cdevice.which);
      return false;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
      UpdateControllerButton(event.cbutton);
      return false;
    case SDL_CONTROLLERAXISMOTION:
      UpdateControllerAxis(event.caxis);
      return false;
    default:
      return false;
  }
}

int16_t InputState::Read(unsigned port, unsigned device, unsigned index, unsigned id) const {
  if (port != 0 || index != 0) {
    return 0;
  }

  if (device == RETRO_DEVICE_JOYPAD) {
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
      std::uint16_t mask = 0;
      const std::array<LogicalControl, 12> controls = {
          LogicalControl::B,      LogicalControl::Y,     LogicalControl::Select,
          LogicalControl::Start,  LogicalControl::Up,    LogicalControl::Down,
          LogicalControl::Left,   LogicalControl::Right, LogicalControl::A,
          LogicalControl::X,      LogicalControl::L,     LogicalControl::R,
      };
      for (std::size_t bit = 0; bit < controls.size(); ++bit) {
        if (ControlPressed(controls[bit])) {
          mask |= static_cast<std::uint16_t>(1u << bit);
        }
      }
      return static_cast<int16_t>(mask);
    }

    switch (id) {
      case RETRO_DEVICE_ID_JOYPAD_UP: return ControlPressed(LogicalControl::Up) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_DOWN: return ControlPressed(LogicalControl::Down) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_LEFT: return ControlPressed(LogicalControl::Left) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_RIGHT: return ControlPressed(LogicalControl::Right) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_A: return ControlPressed(LogicalControl::A) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_B: return ControlPressed(LogicalControl::B) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_X: return ControlPressed(LogicalControl::X) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_Y: return ControlPressed(LogicalControl::Y) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_L: return ControlPressed(LogicalControl::L) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_R: return ControlPressed(LogicalControl::R) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_START: return ControlPressed(LogicalControl::Start) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_SELECT: return ControlPressed(LogicalControl::Select) ? 1 : 0;
      default: return 0;
    }
  }

  if (device == RETRO_DEVICE_ANALOG && index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
    if (id == RETRO_DEVICE_ID_ANALOG_X) {
      return AnalogValue(LogicalControl::AnalogLeft, LogicalControl::AnalogRight);
    }
    if (id == RETRO_DEVICE_ID_ANALOG_Y) {
      return AnalogValue(LogicalControl::AnalogUp, LogicalControl::AnalogDown);
    }
  }

  return 0;
}

void InputState::RefreshDefaultSelection() {
  if (controllers_.empty()) {
    selected_controller_index_ = -1;
    selected_controller_guid_.clear();
    return;
  }

  if (!selected_controller_guid_.empty()) {
    for (std::size_t index = 0; index < controllers_.size(); ++index) {
      if (controllers_[index].guid == selected_controller_guid_) {
        selected_controller_index_ = static_cast<int>(index);
        return;
      }
    }
  }

  selected_controller_index_ = 0;
  selected_controller_guid_ = controllers_[0].guid;
}

void InputState::AddController(int device_index) {
  if (!SDL_IsGameController(device_index)) {
    return;
  }
  SDL_GameController* controller = SDL_GameControllerOpen(device_index);
  if (!controller) {
    return;
  }

  SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
  if (!joystick) {
    SDL_GameControllerClose(controller);
    return;
  }

  ControllerDevice device;
  device.handle = controller;
  device.instance_id = SDL_JoystickInstanceID(joystick);
  device.name = SDL_GameControllerName(controller) ? SDL_GameControllerName(controller) : "CONTROLLER";
  char guid_buffer[64] = {};
  SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid_buffer, sizeof(guid_buffer));
  device.guid = guid_buffer;
  controllers_.push_back(std::move(device));
  RefreshDefaultSelection();
}

void InputState::RemoveController(SDL_JoystickID instance_id) {
  controllers_.erase(
      std::remove_if(controllers_.begin(), controllers_.end(), [&](ControllerDevice& device) {
        if (device.instance_id != instance_id) {
          return false;
        }
        if (device.handle) {
          SDL_GameControllerClose(device.handle);
          device.handle = nullptr;
        }
        return true;
      }),
      controllers_.end());
  RefreshDefaultSelection();
}

void InputState::UpdateControllerButton(const SDL_ControllerButtonEvent& event) {
  if (selected_controller_index_ < 0 ||
      selected_controller_index_ >= static_cast<int>(controllers_.size())) {
    return;
  }
  const auto& controller = controllers_[static_cast<std::size_t>(selected_controller_index_)];
  if (event.which != controller.instance_id) {
    return;
  }
  controller_button_state_[static_cast<int>(event.button)] = event.state == SDL_PRESSED;
}

void InputState::UpdateControllerAxis(const SDL_ControllerAxisEvent& event) {
  if (selected_controller_index_ < 0 ||
      selected_controller_index_ >= static_cast<int>(controllers_.size())) {
    return;
  }
  const auto& controller = controllers_[static_cast<std::size_t>(selected_controller_index_)];
  if (event.which != controller.instance_id) {
    return;
  }
  controller_axis_state_[static_cast<int>(event.axis)] = event.value;
}

bool InputState::HandleCaptureEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_KEYDOWN:
      if (!event.key.repeat) {
        CompleteCapture(Keyboard(event.key.keysym.scancode), false);
        return true;
      }
      return false;
    case SDL_CONTROLLERBUTTONDOWN:
      if (selected_controller_index_ >= 0 &&
          selected_controller_index_ < static_cast<int>(controllers_.size()) &&
          event.cbutton.which ==
              controllers_[static_cast<std::size_t>(selected_controller_index_)].instance_id) {
        CompleteCapture(ControllerButton(static_cast<SDL_GameControllerButton>(event.cbutton.button)),
                        true);
        return true;
      }
      return false;
    case SDL_CONTROLLERAXISMOTION:
      if (selected_controller_index_ >= 0 &&
          selected_controller_index_ < static_cast<int>(controllers_.size()) &&
          event.caxis.which ==
              controllers_[static_cast<std::size_t>(selected_controller_index_)].instance_id &&
          std::abs(event.caxis.value) >= kAxisThreshold) {
        CompleteCapture(
            event.caxis.value > 0
                ? ControllerAxisPos(static_cast<SDL_GameControllerAxis>(event.caxis.axis))
                : ControllerAxisNeg(static_cast<SDL_GameControllerAxis>(event.caxis.axis)),
            true);
        return true;
      }
      return false;
    default:
      return false;
  }
}

void InputState::StartCapture(std::size_t binding_index) {
  capture_active_ = true;
  bind_all_active_ = false;
  capture_binding_index_ = binding_index;
}

void InputState::StartBindAll() {
  capture_active_ = true;
  bind_all_active_ = true;
  bind_all_index_ = 0;
  capture_binding_index_ = 0;
}

void InputState::CompleteCapture(InputBinding binding, bool from_controller) {
  if (capture_binding_index_ >= bindings_.size()) {
    CancelCapture();
    return;
  }

  auto& target = bindings_[capture_binding_index_];
  if (from_controller) {
    target.controller = binding;
  } else {
    target.keyboard = binding;
  }
  SaveBindings();

  if (bind_all_active_) {
    AdvanceBindAllCapture();
  } else {
    CancelCapture();
  }
}

void InputState::AdvanceBindAllCapture() {
  ++bind_all_index_;
  if (bind_all_index_ >= bindings_.size()) {
    CancelCapture();
    return;
  }
  capture_binding_index_ = bind_all_index_;
}

void InputState::CancelCapture() {
  capture_active_ = false;
  bind_all_active_ = false;
}

bool InputState::ControlPressed(LogicalControl control) const {
  const auto index = static_cast<std::size_t>(control);
  if (index >= bindings_.size()) {
    return false;
  }
  if (index < scripted_controls_.size() && scripted_controls_[index]) {
    return true;
  }
  const auto& binding = bindings_[index];
  return BindingPressed(binding.keyboard) || BindingPressed(binding.controller);
}

int16_t InputState::AnalogValue(LogicalControl negative, LogicalControl positive) const {
  constexpr int16_t kFullTilt = 0x7fff;
  const bool neg = ControlPressed(negative);
  const bool pos = ControlPressed(positive);
  if (neg == pos) {
    return 0;
  }
  return pos ? kFullTilt : static_cast<int16_t>(-kFullTilt);
}

bool InputState::BindingPressed(const InputBinding& binding) const {
  switch (binding.kind) {
    case BindingKind::None:
      return false;
    case BindingKind::Keyboard: {
      const auto found = key_state_.find(static_cast<SDL_Scancode>(binding.code));
      return found != key_state_.end() && found->second;
    }
    case BindingKind::ControllerButton: {
      const auto found = controller_button_state_.find(binding.code);
      return found != controller_button_state_.end() && found->second;
    }
    case BindingKind::ControllerAxisPositive: {
      const auto found = controller_axis_state_.find(binding.code);
      return found != controller_axis_state_.end() && found->second >= kAxisThreshold;
    }
    case BindingKind::ControllerAxisNegative: {
      const auto found = controller_axis_state_.find(binding.code);
      return found != controller_axis_state_.end() && found->second <= -kAxisThreshold;
    }
  }
  return false;
}

std::string InputState::BindingLabel(const InputBinding& binding) const {
  switch (binding.kind) {
    case BindingKind::None:
      return "UNBOUND";
    case BindingKind::Keyboard:
      return SDL_GetScancodeName(static_cast<SDL_Scancode>(binding.code));
    case BindingKind::ControllerButton:
      return std::string("PAD ") +
             SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(binding.code));
    case BindingKind::ControllerAxisPositive:
      return std::string("PAD ") +
             SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(binding.code)) +
             "+";
    case BindingKind::ControllerAxisNegative:
      return std::string("PAD ") +
             SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(binding.code)) +
             "-";
  }
  return "UNBOUND";
}

bool InputState::SelectControllerDelta(int delta) {
  if (controllers_.empty() || delta == 0) {
    return false;
  }
  int next = selected_controller_index_;
  if (next < 0) {
    next = 0;
  } else {
    next += delta;
    while (next < 0) {
      next += static_cast<int>(controllers_.size());
    }
    while (next >= static_cast<int>(controllers_.size())) {
      next -= static_cast<int>(controllers_.size());
    }
  }
  selected_controller_index_ = next;
  selected_controller_guid_ = controllers_[static_cast<std::size_t>(selected_controller_index_)].guid;
  SaveBindings();
  return true;
}

}  // namespace sekaiemu::spike
