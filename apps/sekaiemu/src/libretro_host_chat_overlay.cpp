#include "libretro_host_internal.hpp"

#include "libretro_tracker_presentation.hpp"

#include <SDL.h>

#include <algorithm>

namespace sekaiemu::spike {
namespace {

struct ChatOverlayGeometry {
  unsigned width = 0;
  unsigned height = 0;
  int game_area_width = 0;
  int game_area_height = 0;
};

ChatOverlayGeometry ResolveChatOverlayGeometry(TrackerDisplayMode mode, const VideoGeometry& geometry) {
  const unsigned game_width = std::max(geometry.width, 256u);
  const unsigned game_height = std::max(geometry.height, 224u);
  if (mode == TrackerDisplayMode::SplitScreen) {
    const unsigned tracker_sidebar_width = std::max(360u, game_height * 2u);
    return ChatOverlayGeometry{game_width * 3u + tracker_sidebar_width,
                               game_height * 3u,
                               static_cast<int>(game_width * 3u),
                               static_cast<int>(game_height * 3u)};
  }
  const unsigned width = std::max(game_width, 640u);
  const unsigned height = std::max(game_height, 360u);
  return ChatOverlayGeometry{width, height, static_cast<int>(width), static_cast<int>(height)};
}

}  // namespace

void LibretroHost::Impl::RefreshChatOverlayFromSnapshot() {
  if (!tracker_active_) {
    return;
  }
  const auto mutation_serial = tracker_runtime_.MutationSerial();
  if (mutation_serial == chat_snapshot_mutation_serial_) {
    return;
  }
  chat_snapshot_mutation_serial_ = mutation_serial;
  const auto& snapshot = tracker_runtime_.AuthoritativeState().snapshot;
  const auto messages = snapshot.find("chat_messages");
  if (messages == snapshot.end() || !messages->is_array()) {
    return;
  }
  for (const auto& message : *messages) {
    if (!message.is_object()) {
      continue;
    }
    chat_overlay_.AddExternalMessage(message.value("id", 0ULL),
                                     message.value("author", "ROOM"),
                                     message.value("text", ""),
                                     frame_counter);
  }
}

void LibretroHost::Impl::RenderTrackerPresentation() {
  if (!video_backend) {
    return;
  }
  RefreshChatOverlayFromSnapshot();
  const bool chat_visible = chat_overlay_.Visible(frame_counter);
  const bool chat_should_render = chat_overlay_.ShouldRender(frame_counter, kChatRenderFrameInterval);
  if (!tracker_active_ && !chat_should_render) {
    return;
  }
  const bool force_tracker_render =
      tracker_active_ &&
      (tracker_force_next_render_ ||
       tracker_runtime_.MutationSerial() != tracker_last_render_mutation_serial_);

  if (tracker_active_) {
    if (!audio_output.ShouldDeferOptionalWork(48000) || force_tracker_render) {
      sekaiemu::spike::RenderTrackerPresentation(tracker_active_,
                                                 tracker_runtime_,
                                                 tracker_window_presenter_,
                                                 *video_backend,
                                                 TrackerAssetResolver(),
                                                 CurrentVideoGeometry(),
                                                 frame_counter,
                                                 tracker_last_render_frame_,
                                                 kTrackerRenderFrameInterval,
                                                 force_tracker_render);
      tracker_last_render_mutation_serial_ = tracker_runtime_.MutationSerial();
      tracker_force_next_render_ = false;
    }
  } else {
    video_backend->ClearOverlay();
    tracker_force_next_render_ = false;
  }

  if (chat_should_render) {
    if (chat_visible) {
      const auto chat_geometry =
          ResolveChatOverlayGeometry(tracker_runtime_.UiState().display_mode, CurrentVideoGeometry());
      OverlayCanvas canvas(chat_geometry.width, chat_geometry.height);
      canvas.Clear({0, 0, 0, 0});
      chat_overlay_.Render(canvas, frame_counter, chat_geometry.game_area_width, chat_geometry.game_area_height);
      video_backend->UploadChatOverlayFrame(canvas.Data(), canvas.Width(), canvas.Height());
    } else {
      video_backend->ClearChatOverlay();
    }
    chat_overlay_.MarkRendered(frame_counter);
  }
}

void LibretroHost::Impl::RenderBridgeTerminal() {
  if (!bridge_terminal_presenter_.Enabled()) {
    return;
  }
  const auto mutation_serial = tracker_active_ ? tracker_runtime_.MutationSerial() : 0;
  const bool first_render = bridge_terminal_last_render_frame_ == 0;
  const bool periodic_refresh =
      frame_counter - bridge_terminal_last_render_frame_ >= 60;
  if (mutation_serial == bridge_terminal_last_mutation_serial_ && !periodic_refresh && !first_render) {
    return;
  }
  if (audio_output.ShouldDeferOptionalWork(48000) && !periodic_refresh && !first_render) {
    return;
  }
  bridge_terminal_presenter_.Render(CurrentBridgeRuntimeStatus(),
                                    tracker_active_ ? &tracker_runtime_ : nullptr);
  bridge_terminal_last_mutation_serial_ = mutation_serial;
  bridge_terminal_last_render_frame_ = frame_counter;
}

void LibretroHost::Impl::ToggleChatOverlay() {
  chat_overlay_.ToggleEnabled();
  if (!chat_overlay_.Enabled()) {
    SDL_StopTextInput();
  }
  SaveFrontendSettingsNow();
}

void LibretroHost::Impl::OpenChatInput() {
  if (!chat_overlay_.Enabled()) {
    return;
  }
  chat_overlay_.BeginTyping(frame_counter);
  chat_ignore_open_key_text_ = true;
  SDL_StartTextInput();
}

void LibretroHost::Impl::CancelChatInput() {
  chat_overlay_.CancelTyping();
  chat_ignore_open_key_text_ = false;
  SDL_StopTextInput();
}

void LibretroHost::Impl::SubmitChatInput() {
  auto submitted = chat_overlay_.Submit(frame_counter, options.ap_slot_name.empty() ? "ME" : options.ap_slot_name);
  chat_ignore_open_key_text_ = false;
  SDL_StopTextInput();
  if (!submitted.has_value()) {
    return;
  }
  if (core_chat_bridge_.HasOutbox() && core_chat_bridge_.SendChat(*submitted)) {
    return;
  }
  if (!tracker_command_log_path_.empty()) {
    EmitTrackerCommand(nlohmann::json{{"cmd", "chat.say"}, {"text", *submitted}});
  }
}

void LibretroHost::Impl::BackspaceChatInput() {
  chat_overlay_.Backspace();
}

void LibretroHost::Impl::AppendChatInput(std::string_view text) {
  if (chat_ignore_open_key_text_) {
    chat_ignore_open_key_text_ = false;
    if (text == "t" || text == "T") {
      return;
    }
  }
  chat_overlay_.AppendText(text);
}

}  // namespace sekaiemu::spike
