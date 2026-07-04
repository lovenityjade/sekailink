#pragma once

#include "tracker_pack_layout_model.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike::tracker_pack_layout_detail {

bool ResolvePackMapId(const TrackerBundle& bundle, std::string_view pack_name, std::string* out_map_id);
bool ResolveContextPackMapId(const PackStateContext& context,
                             std::string_view pack_name,
                             std::string* out_map_id);
std::string ResolveContextPackMapAsset(const PackStateContext& context,
                                       std::string_view map_id,
                                       std::string_view pack_name);
std::string ResolveDominantContextPackMapName(const PackStateContext& context,
                                              std::string_view map_id);

std::optional<nlohmann::json> ResolveLayoutReference(const PackStateContext& context,
                                                     const nlohmann::json& node);
Rect Inset(Rect rect, int amount);
Margins ParseMargins(std::string_view text);
Rect ApplyMargins(Rect rect, const Margins& margins);
Rect ApplySizeConstraints(Rect rect, const nlohmann::json& node);
int ResolveAlignedOrigin(int origin, int available, int content, std::string_view alignment);
std::vector<nlohmann::json> NodeContentArray(const nlohmann::json& node);
Rect ClampRectToPositive(Rect rect);
bool ShouldSuppressPackLayoutNode(const nlohmann::json& node);
bool NodeTargetsActiveMap(const PackStateContext& context, const nlohmann::json& node);
bool NodeTargetsActiveTrackerTab(const PackStateContext& context, const nlohmann::json& node);
Size EstimateNodePreferredSize(const PackStateContext& context, const nlohmann::json& node);

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
