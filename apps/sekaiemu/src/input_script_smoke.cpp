#include "input_script.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
  namespace fs = std::filesystem;
  const auto path = fs::temp_directory_path() / "sekaiemu-input-script-smoke.txt";
  {
    std::ofstream script(path);
    script << "# frame range followed by one or more controls\n";
    script << "1..3 START\n";
    script << "4 A,B\n";
    script << "5 right+a\n";
  }

  sekaiemu::spike::InputScriptPlayer player;
  std::string error;
  if (!player.LoadFromFile(path, error)) {
    std::cerr << error << "\n";
    return 1;
  }
  if (player.LastFrame() != 5) {
    std::cerr << "unexpected last frame\n";
    return 1;
  }
  if (player.ControlsForFrame(0).size() != 0 || player.ControlsForFrame(2).size() != 1 ||
      player.ControlsForFrame(4).size() != 2 || player.ControlsForFrame(5).size() != 2) {
    std::cerr << "unexpected scripted controls\n";
    return 1;
  }
  fs::remove(path);
  return 0;
}
