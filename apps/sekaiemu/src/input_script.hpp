#pragma once

#include "input_state.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sekaiemu::spike {

class InputScriptPlayer {
 public:
  bool LoadFromFile(const std::filesystem::path& path, std::string& error);

  std::vector<InputState::LogicalControl> ControlsForFrame(std::uint64_t frame) const;
  bool Empty() const { return entries_.empty(); }
  std::uint64_t LastFrame() const { return last_frame_; }

 private:
  struct Entry {
    std::uint64_t start_frame = 0;
    std::uint64_t end_frame = 0;
    std::vector<InputState::LogicalControl> controls;
  };

  std::vector<Entry> entries_;
  std::uint64_t last_frame_ = 0;
};

}  // namespace sekaiemu::spike
