#pragma once

#include "nlohmann/json.hpp"

#include <string>

namespace sekailink_server::generation_internal {

bool region_graph_edge_has_placeholder(const nlohmann::json& edge);
std::string region_graph_edge_id(const nlohmann::json& edge);
std::string region_graph_edge_from(const nlohmann::json& edge);
std::string region_graph_edge_to(const nlohmann::json& edge);
bool region_graph_edges_match(const nlohmann::json& declared_edge, const nlohmann::json& consumed_edge);
bool authorizes_consumed_region_graph_edge(const nlohmann::json& edge);
nlohmann::json consumed_region_graph_edges(const nlohmann::json& region_graph);
bool has_authorized_consumed_region_graph_edges(const nlohmann::json& region_graph);
nlohmann::json consumed_edge_for_placeholder(const nlohmann::json& region_graph,
                                             const nlohmann::json& placeholder_edge);
nlohmann::json effective_region_graph_edges(const nlohmann::json& region_graph);
nlohmann::json effective_region_graph_object(const nlohmann::json& region_graph);
bool has_region_graph_placeholder_edges(const nlohmann::json& region_graph);

}  // namespace sekailink_server::generation_internal
