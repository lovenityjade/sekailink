#include "runtime_menu.hpp"

namespace sekaiemu::spike {

void RuntimeMenu::SetSettingsMode(RuntimeSettingsMode mode) {
  if (settings_mode_ == mode) {
    return;
  }
  settings_mode_ = mode;
  settings_mode_changed_ = true;
}

bool RuntimeMenu::ConsumeSettingsModeChanged() {
  const bool changed = settings_mode_changed_;
  settings_mode_changed_ = false;
  return changed;
}

}  // namespace sekaiemu::spike
