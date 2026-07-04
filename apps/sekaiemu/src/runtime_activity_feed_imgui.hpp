#pragma once

#include "tracker_runtime.hpp"

#include <functional>
#include <string_view>

namespace sekaiemu::spike {

class RuntimeMenu;

void RenderRuntimeActivityFeedImGui(const TrackerRuntime& tracker_runtime);
bool RuntimeReceivedItemsModalOpen();
void RenderRuntimeContextMenuImGui(RuntimeMenu& runtime_menu,
                                   const TrackerRuntime* tracker_runtime,
                                   const std::function<void(std::string_view)>& send_chat_command);

}  // namespace sekaiemu::spike
