#pragma once

#include "tracker_overlay_style.hpp"

namespace sekaiemu::spike {

void DrawInfoPanels(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    const TrackerResolvedViewState& resolved,
                    int x,
                    int y,
                    int width,
                    int height);

void DrawRecentEvents(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerResolvedViewState& resolved,
                      int x,
                      int y,
                      int width,
                      int height);

void DrawSessionStrip(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerResolvedViewState& resolved,
                      int x,
                      int y,
                      int width);

void DrawMetadataOverview(OverlayCanvas& canvas,
                          const UiPalette& palette,
                          const TrackerRuntime& runtime,
                          const TrackerResolvedViewState& resolved,
                          int x,
                          int y,
                          int width,
                          int height);

void DrawItemsSection(OverlayCanvas& canvas,
                      const UiPalette& palette,
                      const TrackerRuntime& runtime,
                      const TrackerResolvedViewState& resolved,
                      int x,
                      int y,
                      int width,
                      int height,
                      const TrackerOverlayAssetResolver* asset_resolver);

void DrawCompactBody(OverlayCanvas& canvas,
                     const UiPalette& palette,
                     const TrackerRuntime& runtime,
                     const TrackerResolvedViewState& resolved,
                     int x,
                     int y,
                     int width,
                     int height);

}  // namespace sekaiemu::spike
