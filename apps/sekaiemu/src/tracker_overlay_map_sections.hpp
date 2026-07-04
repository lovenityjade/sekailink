#pragma once

#include "tracker_overlay_style.hpp"

namespace sekaiemu::spike {

void DrawTabStrip(OverlayCanvas& canvas,
                  const UiPalette& palette,
                  const TrackerResolvedViewState& resolved,
                  int x,
                  int y,
                  int width);

void DrawMapSection(OverlayCanvas& canvas,
                    const UiPalette& palette,
                    const TrackerRuntime& runtime,
                    const TrackerResolvedViewState& resolved,
                    int x,
                    int y,
                    int width,
                    int height,
                    const TrackerOverlayAssetResolver* asset_resolver);

void DrawSummarySection(OverlayCanvas& canvas,
                        const UiPalette& palette,
                        const TrackerRuntime& runtime,
                        const TrackerResolvedViewState& resolved,
                        int x,
                        int y,
                        int width,
                        int height);

}  // namespace sekaiemu::spike
