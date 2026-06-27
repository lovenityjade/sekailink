#include "launch_options.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

int main() {
  namespace fs = std::filesystem;
  using namespace sekaiemu::spike;

  const fs::path cwd = "/tmp/sekaiemu-launch-options-smoke";
  const std::vector<std::string> args{
      "--core",
      "/tmp/core.so",
      "--game",
      "/tmp/game.sfc",
      "--tracker-required",
      "--tracker-pack",
      "/tmp/pack.zip",
      "--tracker-variant",
      "Map Tracker - AP",
      "--tracker-snapshot",
      "/tmp/tracker.snapshot.json",
      "--tracker-command-log",
      "/tmp/tracker.commands.jsonl",
      "--tracker-assets-root",
      "/tmp/tracker-assets",
      "--tracker-bundle",
      "/tmp/default.bundle",
      "--tracker-state",
      "/tmp/state.json",
  };

  const auto parsed = ParseLaunchArgs(args, cwd);
  if (!parsed.ok) {
    std::cerr << "launch_options_parse_failed:" << parsed.user_message << "\n";
    return 1;
  }

  if (parsed.request.tracker_pack_path != fs::path("/tmp/pack.zip") ||
      parsed.request.tracker_variant != "Map Tracker - AP" ||
      parsed.request.tracker_snapshot_path != fs::path("/tmp/tracker.snapshot.json") ||
      parsed.request.tracker_command_log_path != fs::path("/tmp/tracker.commands.jsonl") ||
      parsed.request.tracker_assets_root != fs::path("/tmp/tracker-assets")) {
    std::cerr << "launch_options_tracker_fields_failed\n";
    return 1;
  }
  if (!parsed.request.tracker_required) {
    std::cerr << "launch_options_tracker_required_failed\n";
    return 1;
  }

  const auto host = BuildHostOptions(parsed.request);
  if (host.tracker_pack_path != fs::path("/tmp/pack.zip") ||
      host.tracker_variant != "Map Tracker - AP" ||
      host.tracker_snapshot_path != fs::path("/tmp/tracker.snapshot.json") ||
      host.tracker_command_log_path != fs::path("/tmp/tracker.commands.jsonl") ||
      host.tracker_assets_root != fs::path("/tmp/tracker-assets") ||
      host.tracker_required) {
    std::cerr << "launch_options_host_tracker_companion_fields_failed\n";
    return 1;
  }

  std::cout << "launch_options_smoke_ok\n";
  return 0;
}
