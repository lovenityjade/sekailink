#include "imgui_tracker_renderer.hpp"

#include "overlay_canvas.hpp"
#include "tracker_asset_resolver.hpp"
#include "tracker_pack_layout_renderer.hpp"

#include <SDL_opengl.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace sekaiemu::spike {
namespace {

struct ImGuiPackPreviewTexture {
  GLuint id = 0;
  int width = 0;
  int height = 0;
};

ImGuiPackPreviewTexture& PackPreviewTexture() {
  static ImGuiPackPreviewTexture texture;
  return texture;
}

std::string JsonString(const nlohmann::json& value, const char* key, const std::string& fallback = {}) {
  if (!value.is_object()) {
    return fallback;
  }
  const auto it = value.find(key);
  return it != value.end() && it->is_string() ? it->get<std::string>() : fallback;
}

int JsonInt(const nlohmann::json& value, const char* key, int fallback = 0) {
  if (!value.is_object()) {
    return fallback;
  }
  const auto it = value.find(key);
  return it != value.end() && it->is_number_integer() ? it->get<int>() : fallback;
}

void DrawChip(const char* label, const ImVec4& bg, const ImVec4& fg) {
  ImGui::PushStyleColor(ImGuiCol_Text, fg);
  ImGui::PushStyleColor(ImGuiCol_Button, bg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg);
  ImGui::SmallButton(label);
  ImGui::PopStyleColor(4);
}

bool DrawSegmentButton(const std::string& label, bool selected) {
  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.31f, 0.80f, 0.77f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.90f, 0.84f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.02f, 0.05f, 0.07f, 1.0f));
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.15f, 0.18f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.32f, 0.34f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.86f, 0.88f, 1.0f));
  }
  const bool pressed = ImGui::SmallButton(label.c_str());
  ImGui::PopStyleColor(3);
  return pressed;
}

void DrawLoadingOrError(const TrackerRuntime& runtime) {
  const auto& snapshot = runtime.AuthoritativeState().snapshot;
  const std::string state = JsonString(snapshot, "state");
  const std::string error = JsonString(snapshot, "error");
  if (state == "loading") {
    ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "Loading tracker...");
    ImGui::TextWrapped("%s", JsonString(snapshot, "message", "Tracker data is being prepared.").c_str());
  } else if (state == "error" || !error.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Tracker Error");
    ImGui::TextWrapped("%s", error.empty() ? "tracker_unavailable" : error.c_str());
    ImGui::TextWrapped("%s", JsonString(snapshot, "message", "Sekaiemu did not receive a compatible tracker snapshot.").c_str());
  }
}

void DrawInfoPanels(const TrackerResolvedViewState& resolved) {
  if (!resolved.info_panels.empty() && ImGui::BeginTabBar("tracker-info-tabs")) {
    for (const auto& panel : resolved.info_panels) {
      const std::string label = JsonString(panel, "label", JsonString(panel, "id", "Info"));
      if (!ImGui::BeginTabItem(label.c_str())) {
        continue;
      }
      const auto fields = panel.value("fields", nlohmann::json::array());
      if (fields.is_array()) {
        for (const auto& field : fields) {
          const std::string field_label = JsonString(field, "label", JsonString(field, "id", "Field"));
          const std::string value = JsonString(field, "value", JsonString(field, "fallback", "-"));
          ImGui::TextDisabled("%s", field_label.c_str());
          ImGui::SameLine();
          ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
        }
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void DrawRecentEvents(const TrackerResolvedViewState& resolved) {
  ImGui::SeparatorText("Live Feed");
  if (resolved.recent_events.empty()) {
    ImGui::TextDisabled("No recent tracker events.");
    return;
  }
  for (const auto& event : resolved.recent_events) {
    const std::string label = JsonString(event, "label", JsonString(event, "event_type", "event"));
    const std::string timestamp = JsonString(event, "timestamp");
    ImGui::BulletText("%s%s%s",
                      timestamp.empty() ? "" : timestamp.c_str(),
                      timestamp.empty() ? "" : "  ",
                      label.c_str());
  }
}

void DrawMapList(TrackerRuntime& runtime, const TrackerResolvedViewState& resolved) {
  ImGui::SeparatorText("Maps");
  if (resolved.visible_maps.empty()) {
    ImGui::TextDisabled("No maps available.");
    return;
  }
  for (const auto& map : resolved.visible_maps) {
    const std::string id = JsonString(map, "id");
    const std::string label = JsonString(map, "label", id.empty() ? "Map" : id);
    const bool selected = id == resolved.active_map_id;
    if (ImGui::Selectable(label.c_str(), selected) && !id.empty()) {
      runtime.SetManualMap(id);
    }
  }
}

void DrawTabList(TrackerRuntime& runtime, const TrackerResolvedViewState& resolved) {
  ImGui::SeparatorText("Tabs");
  if (resolved.visible_tabs.empty()) {
    ImGui::TextDisabled("No tabs available.");
    return;
  }
  for (const auto& tab : resolved.visible_tabs) {
    const std::string id = JsonString(tab, "id");
    const std::string label = JsonString(tab, "label", id.empty() ? "Tab" : id);
    const bool selected = id == resolved.active_tab_id;
    if (ImGui::Selectable(label.c_str(), selected) && !id.empty()) {
      runtime.SetActiveTab(id);
    }
  }
}

void DrawTrackerSummary(const TrackerRuntime& runtime, const TrackerResolvedViewState& resolved) {
  const auto& auth = runtime.AuthoritativeState();
  ImGui::BeginGroup();
  DrawChip("OFFLINE PREVIEW", ImVec4(0.92f, 0.38f, 0.22f, 0.95f), ImVec4(0.02f, 0.04f, 0.05f, 1.0f));
  ImGui::SameLine();
  DrawChip(resolved.active_map_id.empty() ? "MAP -" : resolved.active_map_id.c_str(),
           ImVec4(0.12f, 0.20f, 0.24f, 0.95f),
           ImVec4(0.78f, 0.88f, 0.90f, 1.0f));
  ImGui::SameLine();
  const std::string progress = std::to_string(auth.checked_locations.size()) + " checked / " +
                               std::to_string(auth.missing_locations.size()) + " missing";
  DrawChip(progress.c_str(), ImVec4(0.14f, 0.26f, 0.28f, 0.95f), ImVec4(0.78f, 0.96f, 0.94f, 1.0f));
  ImGui::TextDisabled("Slot");
  ImGui::SameLine();
  ImGui::Text("%s", JsonString(auth.snapshot, "slot_name", auth.slot_id.empty() ? "Offline Preview" : auth.slot_id).c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("Seed");
  ImGui::SameLine();
  ImGui::Text("%s", auth.seed_id.empty() ? "OFFLINE" : auth.seed_id.c_str());
  ImGui::EndGroup();
}

void UploadPackPreviewTexture(const OverlayCanvas& canvas) {
  auto& texture = PackPreviewTexture();
  if (texture.id == 0) {
    glGenTextures(1, &texture.id);
  }
  texture.width = static_cast<int>(canvas.Width());
  texture.height = static_cast<int>(canvas.Height());

  glBindTexture(GL_TEXTURE_2D, texture.id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               texture.width,
               texture.height,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               canvas.Data());
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool DrawPackDrivenPreview(TrackerRuntime& runtime, const TrackerResolvedViewState& resolved) {
  const auto* bundle = runtime.Bundle();
  if (bundle == nullptr) {
    return false;
  }

  const auto& auth = runtime.AuthoritativeState();
  ImGui::BeginGroup();
  ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "Pack Layout Preview");
  ImGui::SameLine();
  DrawChip("ALTTP PACK", ImVec4(0.30f, 0.23f, 0.43f, 0.95f), ImVec4(0.92f, 0.88f, 1.0f, 1.0f));
  ImGui::SameLine();
  DrawChip(std::string(std::to_string(auth.received_items.size()) + " items").c_str(),
           ImVec4(0.12f, 0.20f, 0.24f, 0.95f),
           ImVec4(0.78f, 0.88f, 0.90f, 1.0f));
  ImGui::TextDisabled("Rendered through the existing tracker pack while the native ImGui tracker is being rebuilt.");
  ImGui::EndGroup();
  if (!resolved.visible_tabs.empty()) {
    for (const auto& tab : resolved.visible_tabs) {
      const std::string id = JsonString(tab, "id");
      const std::string label = JsonString(tab, "label", id.empty() ? "Tab" : id);
      const bool selected = id == resolved.active_tab_id;
      ImGui::PushID(id.empty() ? label.c_str() : id.c_str());
      if (DrawSegmentButton(label, selected) && !id.empty()) {
        runtime.SetActiveTab(id);
      }
      ImGui::PopID();
      ImGui::SameLine();
    }
    ImGui::NewLine();
  }

  const ImVec2 available = ImGui::GetContentRegionAvail();
  const int canvas_width = std::max(320, static_cast<int>(available.x));
  const int canvas_height = std::max(240, static_cast<int>(available.y));
  OverlayCanvas canvas(static_cast<unsigned>(canvas_width), static_cast<unsigned>(canvas_height));
  canvas.Clear(UiColor{4, 9, 16, 255});

  HostTrackerAssetResolver asset_resolver;
  if (!bundle->bundle_root.empty()) {
    asset_resolver.SetRoots({bundle->bundle_root});
  }

  const bool rendered = RenderPackDrivenTrackerBody(canvas,
                                                    runtime,
                                                    runtime.ResolvedViewState(),
                                                    0,
                                                    0,
                                                    canvas_width,
                                                    canvas_height,
                                                    asset_resolver.HasRoots() ? &asset_resolver : nullptr);
  if (!rendered) {
    ImGui::TextDisabled("Pack layout did not render; falling back to generic ImGui tracker.");
    return false;
  }

  UploadPackPreviewTexture(canvas);
  const auto& texture = PackPreviewTexture();
  const ImVec2 image_min = ImGui::GetCursorScreenPos();
  ImGui::Image(static_cast<ImTextureID>(texture.id),
               ImVec2(static_cast<float>(texture.width), static_cast<float>(texture.height)));
  const ImVec2 image_max(image_min.x + static_cast<float>(texture.width), image_min.y + static_cast<float>(texture.height));
  ImGui::GetWindowDrawList()->AddRect(image_min, image_max, IM_COL32(78, 205, 196, 130), 6.0f, 0, 1.0f);
  return true;
}

}  // namespace

void RenderTrackerImGui(TrackerRuntime& tracker_runtime) {
  const auto resolved = tracker_runtime.ResolvedViewState();
  ImGuiIO& io = ImGui::GetIO();
  const auto mode = tracker_runtime.UiState().display_mode;

  ImVec2 pos(0.0f, 0.0f);
  ImVec2 size(io.DisplaySize.x, io.DisplaySize.y);
  if (mode == TrackerDisplayMode::SplitScreen) {
    size.x = std::max(360.0f, io.DisplaySize.x * 0.34f);
    pos.x = io.DisplaySize.x - size.x;
  } else if (mode == TrackerDisplayMode::PipOverlay) {
    size = ImVec2(std::max(380.0f, io.DisplaySize.x * 0.40f), std::max(260.0f, io.DisplaySize.y * 0.42f));
    pos = ImVec2(io.DisplaySize.x - size.x - 18.0f, io.DisplaySize.y - size.y - 18.0f);
  }

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoTitleBar;
  if (ImGui::Begin("Sekaiemu Tracker", nullptr, flags)) {
    const auto* bundle = tracker_runtime.Bundle();
    ImGui::TextColored(ImVec4(0.31f, 0.80f, 0.77f, 1.0f), "%s",
                       bundle != nullptr && !bundle->display_name.empty() ? bundle->display_name.c_str() : "Tracker");
    ImGui::SameLine();
    const std::string mode_label(ToString(mode));
    ImGui::TextDisabled("%s", mode_label.c_str());
    ImGui::Separator();
    DrawLoadingOrError(tracker_runtime);
    DrawTrackerSummary(tracker_runtime, resolved);

    if (DrawPackDrivenPreview(tracker_runtime, resolved)) {
      // The pack-driven canvas is the visual bridge while the tracker migrates to native ImGui widgets.
    } else if (mode == TrackerDisplayMode::PipOverlay) {
      DrawRecentEvents(resolved);
    } else {
      const float left_width = std::clamp(ImGui::GetContentRegionAvail().x * 0.42f, 170.0f, 320.0f);
      ImGui::BeginChild("tracker-left", ImVec2(left_width, 0.0f), true);
      DrawTabList(tracker_runtime, resolved);
      DrawMapList(tracker_runtime, resolved);
      ImGui::EndChild();
      ImGui::SameLine();
      ImGui::BeginChild("tracker-main", ImVec2(0.0f, 0.0f), true);
      DrawInfoPanels(resolved);
      DrawRecentEvents(resolved);
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

}  // namespace sekaiemu::spike
