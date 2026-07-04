#pragma once

#include "runtime_options.hpp"

#include <filesystem>
#include <string>

namespace sekailink::sklmi {

struct BugReportContext {
    std::string title;
    std::string description;
    std::filesystem::path log_path;
    std::string source = "sklmi";
    std::string component = "sklmi-runtime";
    std::string mode;
    std::string bridge_id;
    std::string linkedworld_id;
    std::string core_profile;
    std::string archipelago_client_wrapper;
    std::string player_alias;
};

BugReportContext MakeBugReportContext(const RuntimeOptions& options,
                                      const std::string& title,
                                      const std::string& description);
bool SubmitBugReport(const BugReportContext& context, std::string* error = nullptr);

}  // namespace sekailink::sklmi
