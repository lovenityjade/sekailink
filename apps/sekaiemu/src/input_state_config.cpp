#include "input_state.hpp"
#include "input_state_internal.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>

namespace sekaiemu::spike {

using namespace input_state_internal;

void InputState::InitializeDefaults() {
  bindings_.clear();
  auto add = [&](LogicalControl control,
                 InputBinding keyboard,
                 InputBinding controller) {
    bindings_.push_back(ControlBinding{
        control,
        ControlLabel(control),
        ControlTooltip(control),
        keyboard,
        controller,
    });
  };

  add(LogicalControl::Up, Keyboard(SDL_SCANCODE_UP), ControllerButton(SDL_CONTROLLER_BUTTON_DPAD_UP));
  add(LogicalControl::Down, Keyboard(SDL_SCANCODE_DOWN), ControllerButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN));
  add(LogicalControl::Left, Keyboard(SDL_SCANCODE_LEFT), ControllerButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT));
  add(LogicalControl::Right, Keyboard(SDL_SCANCODE_RIGHT), ControllerButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
  add(LogicalControl::A, Keyboard(SDL_SCANCODE_X), ControllerButton(SDL_CONTROLLER_BUTTON_A));
  add(LogicalControl::B, Keyboard(SDL_SCANCODE_Z), ControllerButton(SDL_CONTROLLER_BUTTON_B));
  add(LogicalControl::X, Keyboard(SDL_SCANCODE_S), ControllerButton(SDL_CONTROLLER_BUTTON_X));
  add(LogicalControl::Y, Keyboard(SDL_SCANCODE_A), ControllerButton(SDL_CONTROLLER_BUTTON_Y));
  add(LogicalControl::L, Keyboard(SDL_SCANCODE_Q), ControllerButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
  add(LogicalControl::R, Keyboard(SDL_SCANCODE_W), ControllerButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
  add(LogicalControl::Start, Keyboard(SDL_SCANCODE_RETURN), ControllerButton(SDL_CONTROLLER_BUTTON_START));
  add(LogicalControl::Select, Keyboard(SDL_SCANCODE_RSHIFT), ControllerButton(SDL_CONTROLLER_BUTTON_BACK));
  add(LogicalControl::AnalogLeft, Keyboard(SDL_SCANCODE_LEFT), ControllerAxisNeg(SDL_CONTROLLER_AXIS_LEFTX));
  add(LogicalControl::AnalogRight, Keyboard(SDL_SCANCODE_RIGHT), ControllerAxisPos(SDL_CONTROLLER_AXIS_LEFTX));
  add(LogicalControl::AnalogUp, Keyboard(SDL_SCANCODE_UP), ControllerAxisNeg(SDL_CONTROLLER_AXIS_LEFTY));
  add(LogicalControl::AnalogDown, Keyboard(SDL_SCANCODE_DOWN), ControllerAxisPos(SDL_CONTROLLER_AXIS_LEFTY));
}

void InputState::LoadBindings() {
  if (!std::filesystem::exists(config_path_)) {
    return;
  }

  std::ifstream stream(config_path_);
  if (!stream) {
    return;
  }

  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const auto key = Trim(line.substr(0, equals));
    const auto value = Trim(line.substr(equals + 1));

    if (key == "selected_controller_guid") {
      selected_controller_guid_ = value;
      continue;
    }

    const auto dot = key.find('.');
    if (dot == std::string::npos) {
      continue;
    }
    const auto control_key = key.substr(0, dot);
    const auto channel = key.substr(dot + 1);
    const auto parsed = ParseBindingToken(value);
    if (!parsed) {
      continue;
    }

    for (auto& binding : bindings_) {
      if (ControlKey(binding.control) != control_key) {
        continue;
      }
      if (channel == "keyboard") {
        binding.keyboard = *parsed;
      } else if (channel == "controller") {
        binding.controller = *parsed;
      }
    }
  }
}

void InputState::SaveBindings() const {
  std::error_code ec;
  std::filesystem::create_directories(config_path_.parent_path(), ec);
  std::ofstream stream(config_path_, std::ios::trunc);
  if (!stream) {
    return;
  }

  stream << "# Sekaiemu core input configuration\n";
  stream << "selected_controller_guid=" << selected_controller_guid_ << "\n";
  for (const auto& binding : bindings_) {
    stream << ControlKey(binding.control) << ".keyboard=" << ScancodeToken(static_cast<SDL_Scancode>(binding.keyboard.code)) << "\n";
    stream << ControlKey(binding.control) << ".controller=" << ControllerBindingToken(binding.controller) << "\n";
  }
}

std::string InputState::NormalizeKey(std::string_view text) {
  std::string output;
  output.reserve(text.size());
  for (const unsigned char value : text) {
    if (std::isalnum(value)) {
      output.push_back(static_cast<char>(std::tolower(value)));
    } else {
      output.push_back('_');
    }
  }
  while (!output.empty() && output.back() == '_') {
    output.pop_back();
  }
  return output;
}

std::string InputState::ControlKey(LogicalControl control) {
  return NormalizeKey(ControlLabel(control));
}

std::string InputState::ScancodeToken(SDL_Scancode scancode) {
  return std::string("key:") + SDL_GetScancodeName(scancode);
}

std::string InputState::ControllerBindingToken(const InputBinding& binding) {
  switch (binding.kind) {
    case BindingKind::ControllerButton:
      return std::string("button:") +
             SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(binding.code));
    case BindingKind::ControllerAxisPositive:
      return std::string("axis+:") +
             SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(binding.code));
    case BindingKind::ControllerAxisNegative:
      return std::string("axis-:") +
             SDL_GameControllerGetStringForAxis(static_cast<SDL_GameControllerAxis>(binding.code));
    case BindingKind::Keyboard:
      return ScancodeToken(static_cast<SDL_Scancode>(binding.code));
    case BindingKind::None:
      return "none";
  }
  return "none";
}

std::optional<InputState::InputBinding> InputState::ParseBindingToken(std::string_view token) {
  const std::string normalized = Lower(Trim(token));
  if (normalized == "none") {
    return InputBinding{};
  }
  if (normalized.rfind("key:", 0) == 0) {
    const auto name = std::string(token.substr(4));
    const auto scancode = SDL_GetScancodeFromName(name.c_str());
    if (scancode != SDL_SCANCODE_UNKNOWN) {
      return Keyboard(scancode);
    }
  }
  if (normalized.rfind("button:", 0) == 0) {
    const auto button_name = std::string(token.substr(7));
    const auto button = SDL_GameControllerGetButtonFromString(button_name.c_str());
    if (button != SDL_CONTROLLER_BUTTON_INVALID) {
      return ControllerButton(button);
    }
  }
  if (normalized.rfind("axis+:", 0) == 0) {
    const auto axis_name = std::string(token.substr(6));
    const auto axis = SDL_GameControllerGetAxisFromString(axis_name.c_str());
    if (axis != SDL_CONTROLLER_AXIS_INVALID) {
      return ControllerAxisPos(axis);
    }
  }
  if (normalized.rfind("axis-:", 0) == 0) {
    const auto axis_name = std::string(token.substr(6));
    const auto axis = SDL_GameControllerGetAxisFromString(axis_name.c_str());
    if (axis != SDL_CONTROLLER_AXIS_INVALID) {
      return ControllerAxisNeg(axis);
    }
  }
  return std::nullopt;
}

}  // namespace sekaiemu::spike
