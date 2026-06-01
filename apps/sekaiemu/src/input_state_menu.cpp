#include "input_state.hpp"

namespace sekaiemu::spike {

std::size_t InputState::InputMenuRowCount() const { return bindings_.size() + 2; }

InputMenuRow InputState::InputMenuRowAt(std::size_t index) const {
  if (index == 0) {
    return InputMenuRow{InputMenuRowKind::Controller, -1};
  }
  if (index == 1) {
    return InputMenuRow{InputMenuRowKind::BindAll, -1};
  }
  return InputMenuRow{InputMenuRowKind::Binding, static_cast<int>(index - 2)};
}

std::string InputState::InputMenuRowLabelAt(std::size_t index) const {
  const auto row = InputMenuRowAt(index);
  switch (row.kind) {
    case InputMenuRowKind::Controller:
      return "ACTIVE CONTROLLER";
    case InputMenuRowKind::BindAll:
      return "BIND ALL";
    case InputMenuRowKind::Binding:
      if (row.binding_index >= 0 &&
          row.binding_index < static_cast<int>(bindings_.size())) {
        return bindings_[static_cast<std::size_t>(row.binding_index)].label;
      }
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string InputState::InputMenuRowValueAt(std::size_t index) const {
  const auto row = InputMenuRowAt(index);
  switch (row.kind) {
    case InputMenuRowKind::Controller:
      if (selected_controller_index_ >= 0 &&
          selected_controller_index_ < static_cast<int>(controllers_.size())) {
        return controllers_[static_cast<std::size_t>(selected_controller_index_)].name;
      }
      return "KEYBOARD ONLY";
    case InputMenuRowKind::BindAll:
      return bind_all_active_ ? "CAPTURING" : "START";
    case InputMenuRowKind::Binding:
      if (row.binding_index >= 0 &&
          row.binding_index < static_cast<int>(bindings_.size())) {
        const auto& binding = bindings_[static_cast<std::size_t>(row.binding_index)];
        return BindingLabel(binding.keyboard) + " / " + BindingLabel(binding.controller);
      }
      return "";
  }
  return {};
}

std::string InputState::InputMenuTooltipAt(std::size_t index) const {
  const auto row = InputMenuRowAt(index);
  switch (row.kind) {
    case InputMenuRowKind::Controller:
      return "SELECT WHICH SDL GAME CONTROLLER FEEDS THE CURRENT CORE.";
    case InputMenuRowKind::BindAll:
      return "CAPTURE ALL CORE CONTROLS ONE AFTER ANOTHER. PRESS THE DESIRED KEY OR CONTROLLER INPUT.";
    case InputMenuRowKind::Binding:
      if (row.binding_index >= 0 &&
          row.binding_index < static_cast<int>(bindings_.size())) {
        return bindings_[static_cast<std::size_t>(row.binding_index)].tooltip;
      }
      return "";
  }
  return {};
}

bool InputState::ActivateInputMenuRow(std::size_t index) {
  const auto row = InputMenuRowAt(index);
  switch (row.kind) {
    case InputMenuRowKind::Controller:
      return false;
    case InputMenuRowKind::BindAll:
      StartBindAll();
      return true;
    case InputMenuRowKind::Binding:
      if (row.binding_index >= 0 &&
          row.binding_index < static_cast<int>(bindings_.size())) {
        StartCapture(static_cast<std::size_t>(row.binding_index));
        return true;
      }
      return false;
  }
  return false;
}

bool InputState::StepInputMenuRow(std::size_t index, int delta) {
  const auto row = InputMenuRowAt(index);
  if (row.kind == InputMenuRowKind::Controller) {
    return SelectControllerDelta(delta);
  }
  return false;
}

std::string InputState::CapturePrompt() const {
  if (!capture_active_) {
    return {};
  }
  if (capture_binding_index_ >= bindings_.size()) {
    return "PRESS A KEY OR CONTROLLER INPUT.";
  }
  return std::string("PRESS INPUT FOR ") + bindings_[capture_binding_index_].label + ".";
}

}  // namespace sekaiemu::spike
