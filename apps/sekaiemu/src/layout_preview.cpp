#include "layout_preview.hpp"

#include "imgui_runtime.hpp"
#include "imgui_tracker_renderer.hpp"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <thread>

namespace sekaiemu::spike {
namespace {

TrackerRuntime BuildPreviewRuntime(const std::filesystem::path& bundle_path) {
  TrackerRuntime runtime;
  if (!bundle_path.empty()) {
    runtime.LoadBundle(TrackerBundle::LoadFromPath(bundle_path));
  }
  const bool alttp_bundle = runtime.Bundle() != nullptr && runtime.Bundle()->linkedworld_id == "alttp";
  if (alttp_bundle) {
    runtime.ApplyServerSnapshot({
        {"world_instance_id", "alttp-layout-preview-world"},
        {"linkedworld_id", "alttp"},
        {"slot_id", "slot-preview-alttp"},
        {"slot_name", "Jade-ALTTP"},
        {"player_alias", "Jade"},
        {"room_id", "offline-layout-preview"},
        {"room_name", "Sekaiemu Layout Lab"},
        {"room_type", "offline-lab"},
        {"session_id", "preview-session-001"},
        {"seed_id", "AP-PREVIEW-ALTTP"},
        {"room_metadata",
         {
             {"driver_instance_id", "alttp-sekaiemu-runtime"},
             {"core_profile", "snes_v1"},
         }},
        {"seed_metadata",
         {
             {"seed_hash", "PREVIEW"},
             {"tracker_pack", "alttp-native"},
             {"tracker_variant", "imgui-layout-preview"},
             {"slot_data",
              {
                  {"name", "beta3-preview"},
                  {"mode", "open"},
                  {"goal", "ganon"},
                  {"difficulty", "normal"},
                  {"weapons", "randomized"},
                  {"entrance_shuffle", "none"},
                  {"item_pool", "normal"},
                  {"item_functionality", "normal"},
                  {"dark_room_logic", "lamp"},
                  {"dungeon_counters", "pickup"},
                  {"open_pyramid", "goal"},
              }},
         }},
        {"checked_locations",
         nlohmann::json::array({
             nlohmann::json{{"location_id", 59836}, {"location_name", "Link's House"}},
             nlohmann::json{{"location_id", 59837}, {"location_name", "Secret Passage"}},
             nlohmann::json{{"location_id", 60025}, {"location_name", "Sanctuary"}},
             nlohmann::json{{"location_id", 60028}, {"location_name", "Escape - Boomerang Chest"}},
         })},
        {"missing_locations", nlohmann::json::array({60030, 60031, 60032, 60033, 60034})},
        {"received_items",
         nlohmann::json::array({
             nlohmann::json{{"item_id", 10}, {"item_name", "Hookshot"}, {"sender_alias", "Jade"}},
             nlohmann::json{{"item_id", 12}, {"item_name", "Lamp"}, {"sender_alias", "Raifu"}},
             nlohmann::json{{"item_id", 18}, {"item_name", "Hammer"}, {"sender_alias", "Guiz"}},
             nlohmann::json{{"item_id", 29}, {"item_name", "Ether"}, {"sender_alias", "Server"}},
         })},
    });
    runtime.ApplyRuntimeContext({
        {"zone_id", "alttp.light_world"},
        {"driver_instance_id", "alttp-sekaiemu-runtime"},
        {"linkedworld_id", "alttp"},
        {"slot_id", "slot-preview-alttp"},
        {"core_profile", "snes_v1"},
    });
    runtime.ApplySklmiEvent({
        {"event_type", "location_checked"},
        {"location_id", 59836},
        {"label", "Link's House"},
        {"timestamp", "00:01:12"},
    });
    runtime.ApplySklmiEvent({
        {"event_type", "item_received"},
        {"item_id", 10},
        {"label", "Hookshot"},
        {"timestamp", "00:01:18"},
    });
    return runtime;
  }
  runtime.ApplyServerSnapshot({
      {"world_instance_id", "sekaiemu-layout-preview-world"},
      {"linkedworld_id", "preview"},
      {"slot_id", "slot-preview"},
      {"slot_name", "Layout Preview"},
      {"player_alias", "SekaiLink"},
      {"room_id", "offline-layout-preview"},
      {"room_name", "Sekaiemu Layout Lab"},
      {"session_id", "preview-session"},
      {"seed_id", "OFFLINE-PREVIEW"},
      {"seed_metadata", {{"seed_hash", "PREVIEW"}, {"tracker_variant", "lab"}}},
      {"checked_locations", nlohmann::json::array()},
      {"missing_locations", nlohmann::json::array()},
      {"received_items", nlohmann::json::array({
                             nlohmann::json{{"item_id", 10}, {"item_name", "Hookshot"}, {"sender_alias", "Preview"}},
                             nlohmann::json{{"item_id", 12}, {"item_name", "Lamp"}, {"sender_alias", "Preview"}},
                         })},
  });
  runtime.ApplyRuntimeContext({
      {"zone_id", "preview.layout"},
      {"driver_instance_id", "sekaiemu-layout-preview"},
      {"linkedworld_id", "preview"},
      {"slot_id", "slot-preview"},
      {"core_profile", "preview"},
  });
  runtime.ApplySklmiEvent({
      {"event_type", "layout_preview"},
      {"label", "Tracker offline preview"},
      {"timestamp", "00:00:00"},
  });
  return runtime;
}

TrackerDisplayMode DisplayModeFromVariant(const std::string& variant) {
  if (variant == "toggle-screen" || variant == "toggle") {
    return TrackerDisplayMode::ToggleScreen;
  }
  if (variant == "pip-overlay" || variant == "pip") {
    return TrackerDisplayMode::PipOverlay;
  }
  return TrackerDisplayMode::SplitScreen;
}

void RenderNoCartridgePanel(TrackerDisplayMode tracker_mode) {
  ImGuiIO& io = ImGui::GetIO();
  const bool tracker_split = tracker_mode == TrackerDisplayMode::SplitScreen && io.DisplaySize.x >= 900.0f;
  const float reserved_tracker_width = tracker_split ? std::max(380.0f, io.DisplaySize.x * 0.34f) : 0.0f;
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - reserved_tracker_width, io.DisplaySize.y), ImGuiCond_Always);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (!ImGui::Begin("Sekaiemu No Cartridge", nullptr, flags)) {
    ImGui::End();
    return;
  }

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImU32 bg = IM_COL32(24, 18, 24, 255);
  const ImU32 tile_a = IM_COL32(46, 34, 40, 255);
  const ImU32 tile_b = IM_COL32(38, 28, 34, 255);
  draw->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), bg);
  const float tile = std::max(24.0f, std::min(avail.x, avail.y) / 12.0f);
  for (float y = origin.y; y < origin.y + avail.y; y += tile) {
    for (float x = origin.x; x < origin.x + avail.x; x += tile) {
      const bool alt = (static_cast<int>((x - origin.x) / tile) + static_cast<int>((y - origin.y) / tile)) % 2 == 0;
      draw->AddRectFilled(ImVec2(x, y), ImVec2(x + tile, y + tile), alt ? tile_a : tile_b);
    }
  }

  const ImVec2 center(origin.x + avail.x * 0.5f, origin.y + avail.y * 0.5f);
  const float cart_w = std::min(360.0f, avail.x * 0.42f);
  const float cart_h = std::min(250.0f, avail.y * 0.46f);
  const ImVec2 cart_min(center.x - cart_w * 0.5f, center.y - cart_h * 0.5f);
  const ImVec2 cart_max(center.x + cart_w * 0.5f, center.y + cart_h * 0.5f);
  draw->AddRectFilled(cart_min, cart_max, IM_COL32(10, 18, 25, 235), 18.0f);
  draw->AddRect(cart_min, cart_max, IM_COL32(78, 205, 196, 255), 18.0f, 0, 3.0f);
  draw->AddRectFilled(ImVec2(cart_min.x + 28.0f, cart_min.y + 30.0f),
                      ImVec2(cart_max.x - 28.0f, cart_min.y + 86.0f),
                      IM_COL32(255, 107, 53, 230), 10.0f);
  draw->AddRectFilled(ImVec2(cart_min.x + 44.0f, cart_max.y - 78.0f),
                      ImVec2(cart_max.x - 44.0f, cart_max.y - 34.0f),
                      IM_COL32(3, 7, 13, 230), 8.0f);

  ImGui::SetCursorScreenPos(ImVec2(cart_min.x + 44.0f, cart_min.y + 44.0f));
  ImGui::TextColored(ImVec4(0.04f, 0.06f, 0.08f, 1.0f), "SEKAIEMU");
  ImGui::SetCursorScreenPos(ImVec2(cart_min.x + 54.0f, cart_max.y - 66.0f));
  ImGui::TextDisabled("NO CARTRIDGE / LAYOUT PREVIEW");
  ImGui::SetCursorScreenPos(ImVec2(24.0f, io.DisplaySize.y - 40.0f));
  ImGui::TextDisabled("F11 fullscreen. 1 split, 2 pip, 3 toggle. ESC closes preview.");
  ImGui::End();
}

}  // namespace

int RunLayoutPreview(const LaunchRequest& request, std::string& error) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
    error = std::string("SDL_Init failed: ") + SDL_GetError();
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_Window* window = SDL_CreateWindow("Sekaiemu Layout Preview",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        1280,
                                        720,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
    SDL_Quit();
    return 1;
  }
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (gl_context == nullptr) {
    error = std::string("SDL_GL_CreateContext failed: ") + SDL_GetError();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);

  SekaiemuImGuiRuntime imgui;
  if (!imgui.Initialize(window, gl_context, error)) {
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  TrackerRuntime tracker = BuildPreviewRuntime(!request.tracker_pack_path.empty() ? request.tracker_pack_path : request.tracker_bundle_path);
  tracker.SetDisplayMode(DisplayModeFromVariant(request.tracker_variant));
  TrackerDisplayMode display_mode = DisplayModeFromVariant(request.tracker_variant);
  if (request.start_fullscreen) {
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  }

  bool running = true;
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
      imgui.ProcessEvent(event);
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
          running = false;
        } else if (event.key.keysym.sym == SDLK_F11 || event.key.keysym.sym == SDLK_f) {
          const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
          SDL_SetWindowFullscreen(window, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else if (event.key.keysym.sym == SDLK_1) {
          display_mode = TrackerDisplayMode::SplitScreen;
          tracker.SetDisplayMode(display_mode);
        } else if (event.key.keysym.sym == SDLK_2) {
          display_mode = TrackerDisplayMode::PipOverlay;
          tracker.SetDisplayMode(display_mode);
        } else if (event.key.keysym.sym == SDLK_3) {
          display_mode = TrackerDisplayMode::ToggleScreen;
          tracker.SetDisplayMode(display_mode);
        }
      }
    }

    glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
    glClearColor(0.01f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    imgui.Render([&]() {
      RenderNoCartridgePanel(display_mode);
      RenderTrackerImGui(tracker);
    });
    SDL_GL_SwapWindow(window);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }

  imgui.Shutdown();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace sekaiemu::spike
