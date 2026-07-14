#pragma once

#include "launch_options.hpp"

#include <string>

namespace sekaiemu::spike {

int RunInputCaptureMode(const LaunchRequest& request, std::string& error);

}  // namespace sekaiemu::spike
