#include "sklmi_companion_runtime.hpp"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#else
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sekaiemu::spike {
namespace {

std::vector<std::string> BuildRuntimeArguments(const SklmiCompanionOptions& options,
                                               const std::filesystem::path& manifest_path,
                                               const std::filesystem::path& room_state_path,
                                               const std::filesystem::path& runtime_state_root,
                                               const std::filesystem::path& trace_log_path) {
  std::vector<std::string> arguments{
      options.runtime_binary_path.string(),
      "--memory-socket",
      options.memory_socket_path.string(),
      "--bridge-manifest",
      manifest_path.string(),
      "--room-state",
      room_state_path.string(),
      "--runtime-state",
      runtime_state_root.string(),
      "--trace-log",
      trace_log_path.string(),
      "--mode",
  };

  if (!options.ap_host.empty() && options.ap_port != 0 && !options.ap_game.empty() &&
      !options.ap_slot_name.empty()) {
    arguments.push_back("archipelago");
    const auto ap_port_text = std::to_string(options.ap_port);
    const auto ap_path_text = options.ap_path.empty() ? std::string("/") : options.ap_path;
    const auto ap_uuid_text =
        options.ap_uuid.empty() ? std::string("sekailink-sekaiemu") : options.ap_uuid;
    const auto ap_tags_text =
        options.ap_tags.empty() ? std::string("AP,SekaiLink,SKLMI") : options.ap_tags;
    arguments.insert(arguments.end(),
                     {"--ap-host",
                      options.ap_host,
                      "--ap-port",
                      ap_port_text,
                      "--ap-path",
                      ap_path_text,
                      "--ap-game",
                      options.ap_game,
                      "--ap-slot-name",
                      options.ap_slot_name,
                      "--ap-password",
                      options.ap_password,
                      "--ap-uuid",
                      ap_uuid_text,
                      "--ap-tags",
                      ap_tags_text});
    if (!options.player_alias.empty()) {
      arguments.push_back("--player-alias");
      arguments.push_back(options.player_alias);
    }
  } else {
    arguments.push_back("offline");
  }

  if (!options.tracker_pack_path.empty()) {
    arguments.push_back("--tracker-pack");
    arguments.push_back(options.tracker_pack_path.string());
  }
  if (!options.tracker_variant.empty()) {
    arguments.push_back("--tracker-variant");
    arguments.push_back(options.tracker_variant);
  }
  if (!options.tracker_snapshot_path.empty()) {
    arguments.push_back("--tracker-snapshot");
    arguments.push_back(options.tracker_snapshot_path.string());
  }
  if (!options.tracker_command_log_path.empty()) {
    arguments.push_back("--tracker-command-log");
    arguments.push_back(options.tracker_command_log_path.string());
  }
  if (!options.tracker_assets_root.empty()) {
    arguments.push_back("--tracker-assets-root");
    arguments.push_back(options.tracker_assets_root.string());
  }
  arguments.push_back("--tick-ms");
  arguments.push_back("16");
  return arguments;
}

#ifdef _WIN32
std::wstring WidenUtf8(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  const int needed = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (needed <= 0) {
    return std::wstring(text.begin(), text.end());
  }
  std::wstring out(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), needed);
  return out;
}

std::wstring QuoteWindowsArgument(const std::string& arg) {
  if (arg.empty()) {
    return L"\"\"";
  }
  const bool needs_quotes = arg.find_first_of(" \t\n\v\"") != std::string::npos;
  std::wstring wide = WidenUtf8(arg);
  if (!needs_quotes) {
    return wide;
  }
  std::wstring quoted = L"\"";
  std::size_t backslashes = 0;
  for (wchar_t ch : wide) {
    if (ch == L'\\') {
      ++backslashes;
    } else if (ch == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
      quoted.push_back(ch);
      backslashes = 0;
    } else {
      quoted.append(backslashes, L'\\');
      backslashes = 0;
      quoted.push_back(ch);
    }
  }
  quoted.append(backslashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

std::wstring BuildWindowsCommandLine(const std::vector<std::string>& arguments) {
  std::wstring command_line;
  for (const auto& argument : arguments) {
    if (!command_line.empty()) {
      command_line.push_back(L' ');
    }
    command_line += QuoteWindowsArgument(argument);
  }
  return command_line;
}
#endif

}  // namespace

SklmiCompanionRuntime::~SklmiCompanionRuntime() {
  Shutdown();
}

bool SklmiCompanionRuntime::Start(const SklmiCompanionOptions& options,
                                  std::string& error) {
  Shutdown();

  manifest_path_ = options.manifest_directory / options.bridge_spec.manifest_filename;
  if (!std::filesystem::exists(options.runtime_binary_path)) {
    error = "SKLMI runtime binary does not exist: " + options.runtime_binary_path.string();
    return false;
  }
  if (!std::filesystem::exists(manifest_path_)) {
    error = "SKLMI manifest does not exist: " + manifest_path_.string();
    return false;
  }

  bridge_id_ = options.bridge_spec.bridge_id;
  const auto bridge_root = options.save_directory / "sklmi" / bridge_id_;
  room_state_path_ = bridge_root / "room.state";
  runtime_state_root_ = bridge_root / "runtime";
  trace_log_path_ = bridge_root / "trace.jsonl";
  companion_log_path_ = bridge_root / "companion.log";
  last_exit_detail_.clear();
  std::error_code ec;
  std::filesystem::create_directories(runtime_state_root_, ec);
  if (ec) {
    error = "Could not create SKLMI runtime state directory.";
    return false;
  }

#ifdef _WIN32
  const auto arguments = BuildRuntimeArguments(options,
                                               manifest_path_,
                                               room_state_path_,
                                               runtime_state_root_,
                                               trace_log_path_);
  auto command_line = BuildWindowsCommandLine(arguments);
  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;
  HANDLE log_handle = CreateFileW(companion_log_path_.wstring().c_str(),
                                  FILE_APPEND_DATA,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &security_attributes,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
  if (log_handle == INVALID_HANDLE_VALUE) {
    error = "Could not open SKLMI companion log on Windows.";
    return false;
  }
  SetFilePointer(log_handle, 0, nullptr, FILE_END);

  STARTUPINFOW startup_info{};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup_info.hStdOutput = log_handle;
  startup_info.hStdError = log_handle;

  PROCESS_INFORMATION process_info{};
  const BOOL created = CreateProcessW(nullptr,
                                      command_line.data(),
                                      nullptr,
                                      nullptr,
                                      TRUE,
                                      CREATE_NO_WINDOW,
                                      nullptr,
                                      nullptr,
                                      &startup_info,
                                      &process_info);
  CloseHandle(log_handle);
  if (!created) {
    error = "CreateProcessW failed while launching the SKLMI runtime: " +
            std::to_string(GetLastError());
    return false;
  }
  CloseHandle(process_info.hThread);
  child_process_handle_ = process_info.hProcess;
  child_process_id_ = process_info.dwProcessId;
#else
  child_pid_ = fork();
  if (child_pid_ < 0) {
    error = "fork() failed while launching the SKLMI runtime.";
    child_pid_ = -1;
    return false;
  }
  if (child_pid_ == 0) {
    const int log_fd =
        open(companion_log_path_.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (log_fd >= 0) {
      dup2(log_fd, STDOUT_FILENO);
      dup2(log_fd, STDERR_FILENO);
      close(log_fd);
    }
    if (!options.ap_host.empty() && options.ap_port != 0 && !options.ap_game.empty() &&
        !options.ap_slot_name.empty()) {
      const auto ap_port_text = std::to_string(options.ap_port);
      const auto ap_path_text = options.ap_path.empty() ? std::string("/") : options.ap_path;
      const auto ap_uuid_text =
          options.ap_uuid.empty() ? std::string("sekailink-sekaiemu") : options.ap_uuid;
      const auto ap_tags_text =
          options.ap_tags.empty() ? std::string("AP,SekaiLink,SKLMI") : options.ap_tags;
      std::vector<std::string> arguments{
          options.runtime_binary_path.string(),
          "--memory-socket",
          options.memory_socket_path.string(),
          "--bridge-manifest",
          manifest_path_.string(),
          "--room-state",
          room_state_path_.string(),
          "--runtime-state",
          runtime_state_root_.string(),
          "--trace-log",
          trace_log_path_.string(),
          "--mode",
          "archipelago",
          "--ap-host",
          options.ap_host,
          "--ap-port",
          ap_port_text,
          "--ap-path",
          ap_path_text,
          "--ap-game",
          options.ap_game,
          "--ap-slot-name",
          options.ap_slot_name,
          "--ap-password",
          options.ap_password,
          "--ap-uuid",
          ap_uuid_text,
          "--ap-tags",
          ap_tags_text,
      };
      if (!options.player_alias.empty()) {
        arguments.push_back("--player-alias");
        arguments.push_back(options.player_alias);
      }
      if (!options.tracker_pack_path.empty()) {
        arguments.push_back("--tracker-pack");
        arguments.push_back(options.tracker_pack_path.string());
      }
      if (!options.tracker_variant.empty()) {
        arguments.push_back("--tracker-variant");
        arguments.push_back(options.tracker_variant);
      }
      if (!options.tracker_snapshot_path.empty()) {
        arguments.push_back("--tracker-snapshot");
        arguments.push_back(options.tracker_snapshot_path.string());
      }
      if (!options.tracker_command_log_path.empty()) {
        arguments.push_back("--tracker-command-log");
        arguments.push_back(options.tracker_command_log_path.string());
      }
      if (!options.tracker_assets_root.empty()) {
        arguments.push_back("--tracker-assets-root");
        arguments.push_back(options.tracker_assets_root.string());
      }
      arguments.push_back("--tick-ms");
      arguments.push_back("16");
      std::vector<char*> argv;
      argv.reserve(arguments.size() + 1);
      for (auto& argument : arguments) {
        argv.push_back(argument.data());
      }
      argv.push_back(nullptr);
      execv(options.runtime_binary_path.c_str(), argv.data());
    } else {
      std::vector<std::string> arguments{
          options.runtime_binary_path.string(),
          "--memory-socket",
          options.memory_socket_path.string(),
          "--bridge-manifest",
          manifest_path_.string(),
          "--room-state",
          room_state_path_.string(),
          "--runtime-state",
          runtime_state_root_.string(),
          "--trace-log",
          trace_log_path_.string(),
          "--mode",
          "offline",
      };
      if (!options.tracker_pack_path.empty()) {
        arguments.push_back("--tracker-pack");
        arguments.push_back(options.tracker_pack_path.string());
      }
      if (!options.tracker_variant.empty()) {
        arguments.push_back("--tracker-variant");
        arguments.push_back(options.tracker_variant);
      }
      if (!options.tracker_snapshot_path.empty()) {
        arguments.push_back("--tracker-snapshot");
        arguments.push_back(options.tracker_snapshot_path.string());
      }
      if (!options.tracker_command_log_path.empty()) {
        arguments.push_back("--tracker-command-log");
        arguments.push_back(options.tracker_command_log_path.string());
      }
      if (!options.tracker_assets_root.empty()) {
        arguments.push_back("--tracker-assets-root");
        arguments.push_back(options.tracker_assets_root.string());
      }
      arguments.push_back("--tick-ms");
      arguments.push_back("16");
      std::vector<char*> argv;
      argv.reserve(arguments.size() + 1);
      for (auto& argument : arguments) {
        argv.push_back(argument.data());
      }
      argv.push_back(nullptr);
      execv(options.runtime_binary_path.c_str(), argv.data());
    }
    _exit(127);
  }
#endif

  started_ = true;
  return true;
}

void SklmiCompanionRuntime::Tick(std::string& last_error) {
#ifdef _WIN32
  if (!started_ || child_process_handle_ == nullptr) {
    return;
  }
  const DWORD wait_result = WaitForSingleObject(
      reinterpret_cast<HANDLE>(child_process_handle_), 0);
  if (wait_result == WAIT_TIMEOUT) {
    return;
  }
  DWORD exit_code = 1;
  GetExitCodeProcess(reinterpret_cast<HANDLE>(child_process_handle_), &exit_code);
  CloseHandle(reinterpret_cast<HANDLE>(child_process_handle_));
  child_process_handle_ = nullptr;
  child_process_id_ = 0;
  started_ = false;
  last_exit_detail_ = "SKLMI companion runtime exited unexpectedly with code " +
                      std::to_string(static_cast<unsigned long>(exit_code)) + ".";
  if (!companion_log_path_.empty()) {
    last_exit_detail_ += " See companion log: " + companion_log_path_.string();
  }
  last_error = last_exit_detail_;
#else
  if (!started_ || child_pid_ <= 0) {
    return;
  }
  int status = 0;
  const auto wait_result = waitpid(child_pid_, &status, WNOHANG);
  if (wait_result == 0) {
    return;
  }
  if (wait_result != child_pid_) {
    return;
  }

  started_ = false;
  child_pid_ = -1;
  if (WIFEXITED(status)) {
    last_exit_detail_ = "SKLMI companion runtime exited unexpectedly with code " +
                        std::to_string(WEXITSTATUS(status)) + ".";
  } else if (WIFSIGNALED(status)) {
    last_exit_detail_ = "SKLMI companion runtime exited unexpectedly by signal " +
                        std::to_string(WTERMSIG(status)) + ".";
  } else {
    last_exit_detail_ = "SKLMI companion runtime exited unexpectedly.";
  }
  if (!companion_log_path_.empty()) {
    last_exit_detail_ += " See companion log: " + companion_log_path_.string();
  }
  last_error = last_exit_detail_;
#endif
}

void SklmiCompanionRuntime::Tick(bool& running,
                                 int& exit_code,
                                 std::string& last_error) {
  Tick(last_error);
  if (!last_error.empty()) {
    running = false;
    exit_code = 1;
  }
}

void SklmiCompanionRuntime::Shutdown() {
#ifdef _WIN32
  if (child_process_handle_ != nullptr) {
    HANDLE handle = reinterpret_cast<HANDLE>(child_process_handle_);
    const DWORD wait_result = WaitForSingleObject(handle, 1000);
    if (wait_result == WAIT_TIMEOUT) {
      TerminateProcess(handle, 1);
      WaitForSingleObject(handle, 2000);
    }
    CloseHandle(handle);
    child_process_handle_ = nullptr;
    child_process_id_ = 0;
  }
#else
  if (child_pid_ > 0) {
    kill(child_pid_, SIGTERM);
    int status = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
      const auto wait_result = waitpid(child_pid_, &status, WNOHANG);
      if (wait_result == child_pid_) {
        child_pid_ = -1;
        break;
      }
      if (wait_result < 0) {
        child_pid_ = -1;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (child_pid_ > 0) {
      kill(child_pid_, SIGKILL);
      waitpid(child_pid_, &status, 0);
      child_pid_ = -1;
    }
  }
#endif
  started_ = false;
}

bool SklmiCompanionRuntime::Active() const {
  return started_;
}

std::string SklmiCompanionRuntime::BridgeId() const {
  return bridge_id_;
}

const std::filesystem::path& SklmiCompanionRuntime::RoomStatePath() const {
  return room_state_path_;
}

const std::filesystem::path& SklmiCompanionRuntime::RuntimeStateRoot() const {
  return runtime_state_root_;
}

const std::filesystem::path& SklmiCompanionRuntime::TraceLogPath() const {
  return trace_log_path_;
}

const std::filesystem::path& SklmiCompanionRuntime::ManifestPath() const {
  return manifest_path_;
}

const std::filesystem::path& SklmiCompanionRuntime::CompanionLogPath() const {
  return companion_log_path_;
}

const std::string& SklmiCompanionRuntime::LastExitDetail() const {
  return last_exit_detail_;
}

}  // namespace sekaiemu::spike
