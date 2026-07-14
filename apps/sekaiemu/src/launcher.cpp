#include "launcher.hpp"

#include "bug_report_client.hpp"
#include "input_capture_mode.hpp"
#include "layout_preview.hpp"
#include "libretro_host.hpp"
#include "logger.hpp"
#include "patch_materializer.hpp"

#include <sstream>

namespace sekaiemu::spike {

namespace {

bool ValidateLaunchRequest(const LaunchRequest& request, LaunchResult& result) {
  if (!std::filesystem::exists(request.core_path)) {
    result.user_message = "The selected libretro core file does not exist.";
    result.technical_message = "Missing core path: " + request.core_path.string();
    return false;
  }
  if (request.patch_path.empty() && !std::filesystem::exists(request.game_path)) {
    result.user_message = "The selected game file does not exist.";
    result.technical_message = "Missing game path: " + request.game_path.string();
    return false;
  }
  if (!request.patch_path.empty() && !std::filesystem::exists(request.patch_path)) {
    result.user_message = "The selected patch file does not exist.";
    result.technical_message = "Missing patch path: " + request.patch_path.string();
    return false;
  }
  if (!request.patch_path.empty() && !std::filesystem::exists(request.base_rom_path)) {
    result.user_message = "The selected base ROM file does not exist.";
    result.technical_message = "Missing base ROM path: " + request.base_rom_path.string();
    return false;
  }
  if (!request.profile_path.empty() && !std::filesystem::exists(request.profile_path)) {
    result.user_message = "The selected memory profile file does not exist.";
    result.technical_message = "Missing profile path: " + request.profile_path.string();
    return false;
  }
  if (!request.system_directory.empty() && !std::filesystem::exists(request.system_directory)) {
    result.user_message = "The selected system directory does not exist.";
    result.technical_message = "Missing system directory: " + request.system_directory.string();
    return false;
  }
  if (!request.input_script_path.empty() && !std::filesystem::exists(request.input_script_path)) {
    result.user_message = "The selected input script file does not exist.";
    result.technical_message = "Missing input script path: " + request.input_script_path.string();
    return false;
  }
  return true;
}

std::filesystem::path ResolveLogPath(const LaunchRequest& request) {
  if (!request.log_directory.empty()) {
    return request.log_directory / "sekaiemu.log";
  }
  return request.save_directory / "logs" / "sekaiemu.log";
}

std::string DescribeRequest(const LaunchRequest& request) {
  std::ostringstream description;
  description << "Launch request"
              << " core=" << request.core_path
              << " system_dir=" << request.system_directory
              << " save_dir=" << request.save_directory
              << " probe_only=" << (request.probe_only ? "true" : "false");
  if (!request.game_path.empty()) {
    description << " game=" << request.game_path;
  }
  if (!request.patch_path.empty()) {
    description << " patch=" << request.patch_path;
  }
  if (!request.base_rom_path.empty()) {
    description << " base_rom=" << request.base_rom_path;
  }
  if (!request.profile_path.empty()) {
    description << " profile=" << request.profile_path;
  }
  if (!request.memory_socket_path.empty()) {
    description << " memory_socket=" << request.memory_socket_path;
  }
  if (!request.sklmi_runtime_path.empty()) {
    description << " sklmi_runtime=" << request.sklmi_runtime_path;
  }
  if (!request.sklmi_manifest_directory.empty()) {
    description << " sklmi_manifest_dir=" << request.sklmi_manifest_directory;
  }
  if (!request.tracker_bundle_path.empty()) {
    description << " tracker_bundle=" << request.tracker_bundle_path;
  }
  if (!request.tracker_pack_path.empty()) {
    description << " tracker_pack=" << request.tracker_pack_path;
  }
  if (!request.tracker_variant.empty()) {
    description << " tracker_variant=" << request.tracker_variant;
  }
  if (!request.tracker_snapshot_path.empty()) {
    description << " tracker_snapshot=" << request.tracker_snapshot_path;
  }
  if (!request.tracker_command_log_path.empty()) {
    description << " tracker_command_log=" << request.tracker_command_log_path;
  }
  if (!request.tracker_assets_root.empty()) {
    description << " tracker_assets_root=" << request.tracker_assets_root;
  }
  if (!request.tracker_state_path.empty()) {
    description << " tracker_state=" << request.tracker_state_path;
  }
  if (!request.input_script_path.empty()) {
    description << " input_script=" << request.input_script_path;
  }
  if (!request.dump_frame_path.empty()) {
    description << " dump_frame_path=" << request.dump_frame_path;
  }
  if (request.input_script_quit_after_end) {
    description << " input_script_quit_after_end=true";
  }
  if (request.load_state_on_start) {
    description << " load_state_on_start=true";
  }
  if (request.save_state_at_frame > 0) {
    description << " save_state_at_frame=" << request.save_state_at_frame;
  }
  if (request.quit_after_frame > 0) {
    description << " quit_after_frame=" << request.quit_after_frame;
  }
  if (request.dump_frame_at_frame > 0) {
    description << " dump_frame_at_frame=" << request.dump_frame_at_frame;
  }
  return description.str();
}

void ReportRuntimeFailure(const LaunchRequest& request,
                          const std::filesystem::path& log_path,
                          const std::string& title,
                          const std::string& detail) {
  BugReportContext report;
  report.title = title;
  report.description = detail;
  report.log_path = log_path;
  report.game = !request.game_path.empty() ? request.game_path.filename().string() : request.patch_path.filename().string();
  report.core = request.core_path.filename().string();
  report.linkedworld_id = request.ap_game;
  report.player_alias = request.player_alias.empty() ? request.ap_slot_name : request.player_alias;
  std::string report_error;
  if (!SubmitBugReport(report, &report_error)) {
    LogWarn("Sekaiemu bug report submission failed: " + report_error);
  } else {
    LogInfo("Sekaiemu bug report submitted.");
  }
}

}  // namespace

LaunchResult RunSekaiemu(const LaunchRequest& request) {
  LaunchResult result;
  result.log_path = ResolveLogPath(request);

  InitializeLogger(result.log_path);
  LogInfo("Sekaiemu launch requested.");
  LogInfo(DescribeRequest(request));

  if (request.layout_preview) {
    std::string error;
    result.exit_code = RunLayoutPreview(request, error);
    result.ok = result.exit_code == 0;
    result.technical_message = error;
    if (!result.ok) {
      result.user_message = "Sekaiemu could not open the layout preview.";
      LogError(error);
    } else {
      LogInfo("Sekaiemu layout preview exited cleanly.");
    }
    ShutdownLogger();
    return result;
  }

  if (request.input_capture) {
    std::string error;
    result.exit_code = RunInputCaptureMode(request, error);
    result.ok = result.exit_code == 0;
    result.technical_message = error;
    if (!result.ok) {
      result.user_message = "Sekaiemu could not complete controller capture.";
      LogError(error);
    } else {
      LogInfo("Sekaiemu input capture exited cleanly.");
    }
    ShutdownLogger();
    return result;
  }

  if (!ValidateLaunchRequest(request, result)) {
    LogError(result.technical_message);
    ShutdownLogger();
    return result;
  }

  auto materialized_request = request;
  const auto patch_result = MaterializePatchedGame(materialized_request);
  if (!patch_result.ok) {
    result.user_message = "Sekaiemu could not prepare the selected patch for launch.";
    result.technical_message = patch_result.technical_error;
    LogError(result.technical_message);
    ReportRuntimeFailure(materialized_request, result.log_path, "Sekaiemu patch preparation failed", result.technical_message);
    ShutdownLogger();
    return result;
  }
  materialized_request.game_path = patch_result.game_path;
  LogInfo("Runtime game image resolved to " + materialized_request.game_path.string());

  const HostOptions host_options = BuildHostOptions(materialized_request);
  LibretroHost host(host_options);
  if (!host.Initialize()) {
    result.user_message = "Sekaiemu could not initialize the selected core or game.";
    result.technical_message = host.LastError();
    LogError(result.technical_message);
    ReportRuntimeFailure(materialized_request, result.log_path, "Sekaiemu core initialization failed", result.technical_message);
    ShutdownLogger();
    return result;
  }

  result.exit_code = host.Run();
  result.ok = result.exit_code == 0;
  if (!result.ok) {
    result.user_message = "Sekaiemu exited with an error while the runtime was active.";
    result.technical_message = host.LastError();
    if (!result.technical_message.empty()) {
      LogError(result.technical_message);
    }
    ReportRuntimeFailure(materialized_request, result.log_path, "Sekaiemu runtime exited with an error", result.technical_message);
  } else {
    LogInfo("Sekaiemu runtime exited cleanly.");
  }

  ShutdownLogger();
  return result;
}

LaunchResult RunSekaiemuCli(const std::string& executable_name,
                            const std::vector<std::string>& args,
                            const std::filesystem::path& current_directory) {
  const auto parsed = ParseLaunchArgs(args, current_directory);
  if (!parsed.ok) {
    LaunchResult result;
    result.exit_code = parsed.exit_code;
    result.user_message = parsed.user_message;
    if (parsed.show_usage) {
      result.technical_message = BuildUsageText(executable_name);
    }
    return result;
  }

  return RunSekaiemu(parsed.request);
}

}  // namespace sekaiemu::spike
