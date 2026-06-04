#include "libretro_host_internal.hpp"

#include "tracker_pin_context_menu.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string_view>

namespace sekaiemu::spike {
namespace {

struct TrackerInteractionGeometry {
  unsigned overlay_width = 0;
  unsigned overlay_height = 0;
  unsigned window_width = 0;
  unsigned window_height = 0;
  TrackerPanelLayout layout;
  bool compact = false;
};

struct OverlayPoint {
  int x = 0;
  int y = 0;
};

TrackerInteractionGeometry BuildTrackerInteractionGeometry(const TrackerRuntime& runtime,
                                                           const VideoGeometry& geometry,
                                                           unsigned separate_width,
                                                           unsigned separate_height) {
  const unsigned game_width = std::max(geometry.width, 256u);
  const unsigned game_height = std::max(geometry.height, 224u);
  TrackerInteractionGeometry result;
  result.overlay_width = std::max(game_width, 640u);
  result.overlay_height = std::max(game_height, 360u);
  result.window_width = game_width * 3u;
  result.window_height = game_height * 3u;

  switch (runtime.UiState().display_mode) {
    case TrackerDisplayMode::SeparateWindow:
      result.overlay_width = std::max(360u, separate_width == 0 ? 520u : separate_width);
      result.overlay_height = std::max(360u, separate_height == 0 ? 760u : separate_height);
      result.window_width = result.overlay_width;
      result.window_height = result.overlay_height;
      result.layout =
          TrackerPanelLayout{0, 0, static_cast<int>(result.overlay_width), static_cast<int>(result.overlay_height)};
      break;
    case TrackerDisplayMode::PipOverlay:
      result.compact = true;
      result.layout.width = static_cast<int>(result.overlay_width * 0.42);
      result.layout.height = static_cast<int>(result.overlay_height * 0.45);
      result.layout.x = static_cast<int>(result.overlay_width) - result.layout.width - 12;
      result.layout.y = static_cast<int>(result.overlay_height) - result.layout.height - 12;
      break;
    case TrackerDisplayMode::ToggleScreen:
      result.overlay_width = game_width * 3u;
      result.overlay_height = game_height * 3u;
      result.window_width = result.overlay_width;
      result.window_height = result.overlay_height;
      result.layout =
          TrackerPanelLayout{0, 0, static_cast<int>(result.overlay_width), static_cast<int>(result.overlay_height)};
      break;
    case TrackerDisplayMode::SplitScreen:
    default: {
      const unsigned tracker_sidebar_width = std::max(360u, game_height * 2u);
      result.overlay_width = game_width * 3u + tracker_sidebar_width;
      result.overlay_height = game_height * 3u;
      result.window_width = result.overlay_width;
      result.window_height = result.overlay_height;
      result.layout = TrackerPanelLayout{static_cast<int>(game_width * 3u),
                                         0,
                                         static_cast<int>(tracker_sidebar_width),
                                         static_cast<int>(result.overlay_height)};
      break;
    }
  }
  return result;
}

std::optional<OverlayPoint> MouseToOverlayPoint(const TrackerInteractionGeometry& geometry,
                                                int mouse_x,
                                                int mouse_y) {
  if (geometry.layout.width <= 0 || geometry.layout.height <= 0 ||
      geometry.window_width == 0 || geometry.window_height == 0) {
    return std::nullopt;
  }
  return OverlayPoint{
      static_cast<int>((static_cast<long long>(mouse_x) *
                        static_cast<long long>(geometry.overlay_width)) /
                       static_cast<long long>(geometry.window_width)),
      static_cast<int>((static_cast<long long>(mouse_y) *
                        static_cast<long long>(geometry.overlay_height)) /
                       static_cast<long long>(geometry.window_height)),
  };
}

bool PointInsideTrackerPanel(OverlayPoint point, const TrackerPanelLayout& layout) {
  return point.x >= layout.x && point.x < layout.x + layout.width &&
         point.y >= layout.y && point.y < layout.y + layout.height;
}

int TrackerMenuBodyY(const TrackerInteractionGeometry& geometry) {
  const int header_height = geometry.compact ? 24 : 44;
  return geometry.layout.y + header_height + 6;
}

template <typename Host>
bool SameGeometry(const Host& host, const TrackerInteractionGeometry& geometry) {
  return host.tracker_hit_cache_overlay_width_ == geometry.overlay_width &&
         host.tracker_hit_cache_overlay_height_ == geometry.overlay_height &&
         host.tracker_hit_cache_window_width_ == geometry.window_width &&
         host.tracker_hit_cache_window_height_ == geometry.window_height &&
         host.tracker_hit_cache_compact_ == geometry.compact &&
         host.tracker_hit_cache_layout_.x == geometry.layout.x &&
         host.tracker_hit_cache_layout_.y == geometry.layout.y &&
         host.tracker_hit_cache_layout_.width == geometry.layout.width &&
         host.tracker_hit_cache_layout_.height == geometry.layout.height;
}

void MixHash(std::uint64_t& seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

void MixString(std::uint64_t& seed, std::string_view value) {
  MixHash(seed, static_cast<std::uint64_t>(std::hash<std::string_view>{}(value)));
}

std::uint64_t TrackerHitCacheKey(const TrackerRuntime& runtime,
                                 const TrackerResolvedViewState& resolved) {
  std::uint64_t key = 1469598103934665603ULL;
  const auto& snapshot = runtime.AuthoritativeState().snapshot;
  if (snapshot.is_object() && snapshot.contains("revision") && snapshot["revision"].is_number_unsigned()) {
    MixHash(key, snapshot["revision"].get<std::uint64_t>());
  }
  MixString(key, resolved.active_view_id);
  MixString(key, resolved.active_tab_id);
  MixString(key, resolved.active_map_id);
  MixString(key, runtime.UiState().pin_context_menu_pin_id);
  return key;
}

template <typename Host>
void EnsureTrackerHitTargets(Host& host,
                             const TrackerInteractionGeometry& geometry,
                             const TrackerResolvedViewState& resolved,
                             int body_y) {
  const auto hit_cache_key = TrackerHitCacheKey(host.tracker_runtime_, resolved);
  if (host.tracker_hit_cache_mutation_serial_ == hit_cache_key && SameGeometry(host, geometry)) {
    return;
  }
  host.tracker_hit_targets_ = BuildPackLayoutHitTargets(host.tracker_runtime_,
                                                        resolved,
                                                        geometry.layout.x + 8,
                                                        body_y,
                                                        geometry.layout.width - 16,
                                                        geometry.layout.height - (body_y - geometry.layout.y) - 8,
                                                        nullptr);
  host.tracker_hit_cache_mutation_serial_ = hit_cache_key;
  host.tracker_hit_cache_overlay_width_ = geometry.overlay_width;
  host.tracker_hit_cache_overlay_height_ = geometry.overlay_height;
  host.tracker_hit_cache_window_width_ = geometry.window_width;
  host.tracker_hit_cache_window_height_ = geometry.window_height;
  host.tracker_hit_cache_layout_ = geometry.layout;
  host.tracker_hit_cache_compact_ = geometry.compact;
}

template <typename Host>
std::optional<TrackerInteractionGeometry> ResolveTrackerPoint(Host& host,
                                                              int mouse_x,
                                                              int mouse_y,
                                                              OverlayPoint* point) {
  if (!host.tracker_active_) {
    return std::nullopt;
  }
  auto geometry = BuildTrackerInteractionGeometry(host.tracker_runtime_,
                                                  host.CurrentVideoGeometry(),
                                                  host.tracker_window_presenter_.Width(),
                                                  host.tracker_window_presenter_.Height());
  const auto overlay_point = MouseToOverlayPoint(geometry, mouse_x, mouse_y);
  if (!overlay_point.has_value() || !PointInsideTrackerPanel(*overlay_point, geometry.layout)) {
    return std::nullopt;
  }
  *point = *overlay_point;
  return geometry;
}

template <typename Host>
void EmitClickPin(Host& host,
                  std::string_view location_id,
                  std::string_view button) {
  if (location_id.empty()) {
    return;
  }
  host.EmitTrackerCommand(nlohmann::json{{"cmd", "tracker.click_pin"},
                                         {"location_id", std::string(location_id)},
                                         {"button", std::string(button)}});
}

}  // namespace

bool LibretroHost::Impl::ClickTrackerAt(int mouse_x, int mouse_y, std::string_view button) {
  OverlayPoint point;
  const auto geometry = ResolveTrackerPoint(*this, mouse_x, mouse_y, &point);
  if (!geometry.has_value()) {
    return false;
  }

  const auto resolved = tracker_runtime_.ResolvedViewState();
  const int body_y = TrackerMenuBodyY(*geometry);
  if (tracker_runtime_.UiState().pin_context_menu_visible) {
    const auto entries = BuildTrackerPinContextMenuEntries(tracker_runtime_, resolved);
    const auto hit = HitTestTrackerPinContextMenu(tracker_runtime_,
                                                  resolved,
                                                  geometry->layout,
                                                  body_y,
                                                  point.x,
                                                  point.y);
    if (!hit.has_value()) {
      tracker_runtime_.ClosePinContextMenu();
      tracker_dirty_ = true;
      tracker_force_next_render_ = true;
      return true;
    }
    if (*hit < entries.size()) {
      EmitClickPin(*this, entries[*hit].location_id, button);
    }
    tracker_runtime_.ClosePinContextMenu();
    tracker_dirty_ = true;
    tracker_force_next_render_ = true;
    return true;
  }

  if (tracker_runtime_.UiState().map_context_menu_visible) {
    return false;
  }

  EnsureTrackerHitTargets(*this, *geometry, resolved, body_y);
  const auto* target = FindPackLayoutHitTarget(tracker_hit_targets_, point.x, point.y);
  if (target == nullptr) {
    tracker_runtime_.ClearHoverTooltip();
    return false;
  }

  if (target->kind == TrackerPackHitTargetKind::Item && !target->code.empty()) {
    EmitTrackerCommand(nlohmann::json{{"cmd", "tracker.click_item"},
                                      {"code", target->code},
                                      {"button", std::string(button)}});
    return true;
  }

  if (target->kind == TrackerPackHitTargetKind::Pin && !target->pin_id.empty()) {
    tracker_runtime_.OpenPinContextMenuAt(target->pin_id, point.x, point.y);
    tracker_dirty_ = true;
    tracker_force_next_render_ = true;
    return true;
  }
  return false;
}

bool LibretroHost::Impl::HoverTrackerAt(int mouse_x, int mouse_y) {
  OverlayPoint point;
  const auto geometry = ResolveTrackerPoint(*this, mouse_x, mouse_y, &point);
  if (!geometry.has_value()) {
    const auto before = tracker_runtime_.MutationSerial();
    tracker_runtime_.ClearHoverTooltip();
    tracker_dirty_ = tracker_dirty_ || tracker_runtime_.MutationSerial() != before;
    return false;
  }

  const auto resolved = tracker_runtime_.ResolvedViewState();
  const int body_y = TrackerMenuBodyY(*geometry);
  if (tracker_runtime_.UiState().pin_context_menu_visible) {
    const auto hit = HitTestTrackerPinContextMenu(tracker_runtime_,
                                                  resolved,
                                                  geometry->layout,
                                                  body_y,
                                                  point.x,
                                                  point.y);
    const auto before = tracker_runtime_.MutationSerial();
    if (hit.has_value()) {
      tracker_runtime_.SetPinContextMenuSelectedIndex(*hit);
    }
    tracker_dirty_ = tracker_dirty_ || tracker_runtime_.MutationSerial() != before;
    return hit.has_value();
  }

  if (tracker_runtime_.UiState().map_context_menu_visible) {
    return false;
  }

  EnsureTrackerHitTargets(*this, *geometry, resolved, body_y);
  const auto before = tracker_runtime_.MutationSerial();
  const auto* target = FindPackLayoutHitTarget(tracker_hit_targets_, point.x, point.y);
  if (target != nullptr && target->kind == TrackerPackHitTargetKind::Pin && !target->label.empty()) {
    tracker_runtime_.SetHoverTooltip(target->label,
                                     target->x + target->width / 2,
                                     target->y + target->height / 2);
  } else {
    tracker_runtime_.ClearHoverTooltip();
  }
  tracker_dirty_ = tracker_dirty_ || tracker_runtime_.MutationSerial() != before;
  return target != nullptr;
}

}  // namespace sekaiemu::spike
