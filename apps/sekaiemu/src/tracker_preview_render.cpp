#include "overlay_canvas.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

class FileAssetResolver final : public sekaiemu::spike::TrackerOverlayAssetResolver {
 public:
  explicit FileAssetResolver(std::filesystem::path bundle_root) : bundle_root_(std::move(bundle_root)) {}

  std::optional<sekaiemu::spike::TrackerOverlayResolvedAsset> ResolveTrackerAsset(
      std::string_view bundle_relative_path) const override {
    if (bundle_relative_path.empty()) {
      return std::nullopt;
    }
    const std::string key(bundle_relative_path);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
      auto image = Load(key);
      if (!image.has_value()) {
        failed_.insert(key);
        return std::nullopt;
      }
      it = cache_.emplace(key, std::move(*image)).first;
    }
    return sekaiemu::spike::TrackerOverlayResolvedAsset{
        it->second.width,
        it->second.height,
        it->second.pixels.data(),
    };
  }

 private:
  struct CachedImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::uint8_t> pixels;
  };

  std::optional<CachedImage> Load(const std::string& bundle_relative_path) const {
    if (failed_.contains(bundle_relative_path)) {
      return std::nullopt;
    }
    const auto full_path = bundle_root_ / std::filesystem::path(bundle_relative_path);
    SDL_Surface* surface = IMG_Load(full_path.string().c_str());
    if (surface == nullptr) {
      return std::nullopt;
    }
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (converted == nullptr) {
      return std::nullopt;
    }

    CachedImage image;
    image.width = static_cast<unsigned>(converted->w);
    image.height = static_cast<unsigned>(converted->h);
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * 4u, 0);
    const auto row_bytes = static_cast<std::size_t>(image.width) * 4u;
    const auto* source = static_cast<const std::uint8_t*>(converted->pixels);
    for (unsigned y = 0; y < image.height; ++y) {
      std::copy_n(source + static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch),
                  row_bytes,
                  image.pixels.data() + static_cast<std::size_t>(y) * row_bytes);
    }
    SDL_FreeSurface(converted);
    return image;
  }

  std::filesystem::path bundle_root_;
  mutable std::unordered_map<std::string, CachedImage> cache_;
  mutable std::unordered_set<std::string> failed_;
};

void WritePpm(const std::filesystem::path& path, const sekaiemu::spike::OverlayCanvas& canvas) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("preview_open_failed:" + path.string());
  }
  output << "P6\n" << canvas.Width() << ' ' << canvas.Height() << "\n255\n";
  const auto* pixels = canvas.Data();
  const std::size_t pixel_count = static_cast<std::size_t>(canvas.Width()) * canvas.Height();
  for (std::size_t index = 0; index < pixel_count; ++index) {
    const std::size_t offset = index * 4u;
    const char rgb[3] = {
        static_cast<char>(pixels[offset + 0]),
        static_cast<char>(pixels[offset + 1]),
        static_cast<char>(pixels[offset + 2]),
    };
    output.write(rgb, sizeof(rgb));
  }
}

sekaiemu::spike::TrackerRuntime BuildPreviewRuntime(const std::filesystem::path& bundle_path,
                                                    std::string_view tab_id,
                                                    std::string_view map_id) {
  using namespace sekaiemu::spike;

  TrackerRuntime runtime;
  runtime.LoadBundle(TrackerBundle::LoadFromPath(bundle_path));
  runtime.ApplyServerSnapshot({
      {"world_instance_id", "alttp-preview-world"},
      {"linkedworld_id", "alttp"},
      {"slot_id", "slot-2"},
      {"slot_name", "Jade-ALTTP"},
      {"player_alias", "Jade"},
      {"room_id", "beta3-preview-room"},
      {"room_name", "SekaiLink BETA-3 Preview"},
      {"room_type", "private-room"},
      {"session_id", "preview-session-001"},
      {"seed_id", "AP_78856210104802680998"},
      {"room_metadata",
       {
           {"driver_instance_id", "alttp-sekaiemu-runtime"},
           {"core_profile", "snes_v1"},
       }},
      {"seed_metadata",
       {
           {"seed_hash", "PREVIEW"},
           {"tracker_pack", "default.bundle"},
           {"tracker_variant", "side-by-side"},
           {"slot_data",
            {
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
                {"name", "beta3-preview"},
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
      {"slot_id", "slot-2"},
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

  if (!tab_id.empty()) {
    runtime.SetActiveTab(std::string(tab_id));
  }
  if (!map_id.empty()) {
    runtime.SetManualMap(std::string(map_id));
    runtime.SetAutoMapFollow(false);
  }
  return runtime;
}

}  // namespace

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  using namespace sekaiemu::spike;

  const char* canonical_root = std::getenv("SEKAILINK_CANONICAL_ROOT");
  const fs::path default_bundle =
      canonical_root != nullptr ? fs::path(canonical_root) / "linkedworlds/alttp/tracker/default.bundle"
                                : fs::path("linkedworlds/alttp/tracker/default.bundle");
  const fs::path bundle_path = argc > 1 ? fs::path(argv[1]) : default_bundle;
  const fs::path output_path =
      argc > 2 ? fs::path(argv[2]) : fs::path("/tmp/sekaiemu-tracker-preview/alttp-preview.ppm");
  const unsigned width = argc > 3 ? static_cast<unsigned>(std::stoul(argv[3])) : 1280u;
  const unsigned height = argc > 4 ? static_cast<unsigned>(std::stoul(argv[4])) : 720u;
  const std::string tab_id = argc > 5 ? argv[5] : "light-world";
  const std::string map_id = argc > 6 ? argv[6] : "light_world";
  const fs::path state_path = argc > 7 ? fs::path(argv[7]) : fs::path{};

  try {
    if (SDL_Init(0) != 0) {
      throw std::runtime_error(std::string("preview_sdl_init_failed:") + SDL_GetError());
    }
    const int img_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP;
    IMG_Init(img_flags);

    TrackerRuntime runtime;
    if (!state_path.empty()) {
      runtime.LoadBundle(TrackerBundle::LoadFromPath(bundle_path));
      runtime.LoadPersistedState(TrackerRuntime::ReadPersistedState(state_path));
      if (!tab_id.empty()) {
        runtime.SetActiveTab(tab_id);
      }
      if (!map_id.empty()) {
        runtime.SetManualMap(map_id);
        runtime.SetAutoMapFollow(false);
      }
    } else {
      runtime = BuildPreviewRuntime(bundle_path, tab_id, map_id);
    }
    const auto resolved = runtime.ResolvedViewState();
    const auto* bundle = runtime.Bundle();

    OverlayCanvas canvas(width, height);
    canvas.Clear({3, 7, 13, 255});
    std::optional<FileAssetResolver> asset_resolver;
    if (bundle != nullptr) {
      asset_resolver.emplace(bundle->bundle_root);
    }
    RenderTrackerPanel(canvas,
                       runtime,
                       resolved,
                       TrackerPanelLayout{0, 0, static_cast<int>(width), static_cast<int>(height)},
                       false,
                       "HEADLESS",
                       asset_resolver.has_value() ? &*asset_resolver : nullptr);
    WritePpm(output_path, canvas);
    IMG_Quit();
    SDL_Quit();
    std::cout << "tracker_preview_render_ok " << output_path << "\n";
    return 0;
  } catch (const std::exception& exception) {
    IMG_Quit();
    SDL_Quit();
    std::cerr << "tracker_preview_render_failed: " << exception.what() << "\n";
    return 1;
  }
}
