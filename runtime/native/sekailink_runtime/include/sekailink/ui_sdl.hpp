#pragma once

#include <atomic>

#include "sekailink/gba_runtime.hpp"

namespace sekailink {

int run_sdl_frontend(GbaRuntime& runtime, int scale, const std::atomic<bool>& stop_requested);

}  // namespace sekailink
