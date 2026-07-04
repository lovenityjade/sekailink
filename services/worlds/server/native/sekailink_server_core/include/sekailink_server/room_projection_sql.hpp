#pragma once

#include "sekailink_server/room_projection.hpp"

#include <string>
#include <vector>

namespace sekailink_server {

struct SqlStatement {
  std::string sql;
  std::vector<std::string> bindings;
};

std::string projection_mysql_schema_sql();
SqlStatement build_room_record_insert_sql(const nlohmann::json& record);
SqlStatement build_room_event_record_insert_sql(const nlohmann::json& record);
SqlStatement build_client_report_record_insert_sql(const nlohmann::json& record);

}  // namespace sekailink_server
