#pragma once

#include <cstdint>
#include <string>

namespace sekaiemu::spike {

bool EnsureSocketRuntime();
bool WouldBlock();
void CloseFd(std::intptr_t fd);
bool SetNonBlocking(std::intptr_t fd);
bool SendAll(std::intptr_t fd, const std::string& payload);

}  // namespace sekaiemu::spike
