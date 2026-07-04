#pragma once

#include "tracker_pack_layout_model.hpp"

#include <string>
#include <string_view>
#include <unordered_set>

namespace sekaiemu::spike::tracker_pack_layout_detail {

void BuildPackStateContext(PackStateContext& context);
const PackVisualDefinition* FindPackVisualDefinition(const PackStateContext& context,
                                                     std::string_view code);
PackVisualState ResolvePackVisualState(const PackStateContext& context,
                                       std::string_view code,
                                       std::unordered_set<std::string>* visiting);
VisualAssetSelection SelectVisualAsset(const PackVisualDefinition& definition,
                                       const PackVisualState& state);

}  // namespace sekaiemu::spike::tracker_pack_layout_detail
