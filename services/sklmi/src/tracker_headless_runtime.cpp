#include "tracker_headless_runtime.hpp"

#include "api_internal.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <functional>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace sekailink::sklmi {
namespace {

#include "tracker_headless_runtime_json_fields.inc"
#include "tracker_headless_runtime_lua_text.inc"
#include "tracker_headless_runtime_location_detail.inc"
#include "tracker_headless_runtime_file_detail.inc"
#include "tracker_headless_runtime_item_detail.inc"

}  // namespace

#include "tracker_headless_runtime_lifecycle.inc"
#include "tracker_headless_runtime_state_io.inc"
#include "tracker_headless_runtime_bundle_model.inc"
#include "tracker_headless_runtime_commands.inc"
#include "tracker_headless_runtime_logic_eval.inc"
#include "tracker_headless_runtime_snapshot_pins.inc"
#include "tracker_headless_runtime_snapshot_json.inc"
#include "tracker_headless_runtime_publish.inc"

}  // namespace sekailink::sklmi
