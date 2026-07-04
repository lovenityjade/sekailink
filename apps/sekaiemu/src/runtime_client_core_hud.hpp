#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct BridgeRuntimeStatus;

struct RuntimeClientCoreToast {
  std::string id;
  std::string text;
  double created_at = 0.0;
};

class RuntimeClientCoreHud {
 public:
  void Configure(std::filesystem::path state_path,
                 std::filesystem::path events_path,
                 bool buttons_visible);
  void Tick(std::uint64_t frame);
  void Render(const BridgeRuntimeStatus& bridge_status);
  void SetButtonsVisible(bool visible) { buttons_visible_setting_ = visible; }
  bool ButtonsVisible() const { return buttons_visible_setting_; }

 private:
  void PollState();
  void WriteEvent(const char* type);

  std::filesystem::path state_path_;
  std::filesystem::path events_path_;
  std::uint64_t last_poll_frame_ = 0;
  std::uintmax_t last_state_size_ = 0;
  std::filesystem::file_time_type last_state_write_{};
  int chat_unread_ = 0;
  int activity_unread_ = 0;
  bool buttons_visible_setting_ = true;
  bool buttons_visible_from_core_ = true;
  std::vector<RuntimeClientCoreToast> toasts_;
};

}  // namespace sekaiemu::spike
