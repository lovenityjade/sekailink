#include "sekailink_server/seed_config_sql.hpp"

#include <iostream>

int main() {
  std::cout << sekailink_server::seed_config_mysql_schema_sql();
  return 0;
}
