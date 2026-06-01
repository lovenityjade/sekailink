#pragma once

#include "input_state.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace sekaiemu::spike::input_state_internal {

inline constexpr const char* kInputConfigDir = "input-config";
inline constexpr int kAxisThreshold = 16000;

using LogicalControl = InputState::LogicalControl;
using BindingKind = InputState::BindingKind;
using InputBinding = InputState::InputBinding;

inline std::string Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

inline std::string Lower(std::string_view text) {
  std::string output(text);
  std::transform(output.begin(), output.end(), output.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return output;
}

inline const char* ControlLabel(LogicalControl control) {
  switch (control) {
    case LogicalControl::Up: return "DPAD UP";
    case LogicalControl::Down: return "DPAD DOWN";
    case LogicalControl::Left: return "DPAD LEFT";
    case LogicalControl::Right: return "DPAD RIGHT";
    case LogicalControl::A: return "A";
    case LogicalControl::B: return "B";
    case LogicalControl::X: return "X";
    case LogicalControl::Y: return "Y";
    case LogicalControl::L: return "L";
    case LogicalControl::R: return "R";
    case LogicalControl::Start: return "START";
    case LogicalControl::Select: return "SELECT";
    case LogicalControl::AnalogLeft: return "ANALOG LEFT";
    case LogicalControl::AnalogRight: return "ANALOG RIGHT";
    case LogicalControl::AnalogUp: return "ANALOG UP";
    case LogicalControl::AnalogDown: return "ANALOG DOWN";
    case LogicalControl::Count: break;
  }
  return "UNKNOWN";
}

inline const char* ControlTooltip(LogicalControl control) {
  switch (control) {
    case LogicalControl::Up:
    case LogicalControl::Down:
    case LogicalControl::Left:
    case LogicalControl::Right:
      return "DIGITAL D-PAD INPUT FOR LIBRETRO CORES.";
    case LogicalControl::A:
    case LogicalControl::B:
    case LogicalControl::X:
    case LogicalControl::Y:
    case LogicalControl::L:
    case LogicalControl::R:
    case LogicalControl::Start:
    case LogicalControl::Select:
      return "PRIMARY FACE OR SHOULDER BUTTON BINDING FOR THE CORE.";
    case LogicalControl::AnalogLeft:
    case LogicalControl::AnalogRight:
    case LogicalControl::AnalogUp:
    case LogicalControl::AnalogDown:
      return "ANALOG DIRECTION USED BY N64 AND OTHER ANALOG-AWARE CORES.";
    case LogicalControl::Count: break;
  }
  return "";
}

inline InputBinding Keyboard(SDL_Scancode scancode) {
  return InputBinding{BindingKind::Keyboard, static_cast<int>(scancode)};
}

inline InputBinding ControllerButton(SDL_GameControllerButton button) {
  return InputBinding{BindingKind::ControllerButton, static_cast<int>(button)};
}

inline InputBinding ControllerAxisPos(SDL_GameControllerAxis axis) {
  return InputBinding{BindingKind::ControllerAxisPositive, static_cast<int>(axis)};
}

inline InputBinding ControllerAxisNeg(SDL_GameControllerAxis axis) {
  return InputBinding{BindingKind::ControllerAxisNegative, static_cast<int>(axis)};
}

}  // namespace sekaiemu::spike::input_state_internal
