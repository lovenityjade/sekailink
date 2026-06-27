#include "bug_report_client.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace sekailink::sklmi {
namespace {

constexpr const char* kDefaultApiBase = "https://sekailink.com";

std::string EnvString(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string{};
}

bool EnvEnabled() {
    const auto value = EnvString("SEKAILINK_BUG_REPORT_DISABLED");
    return !(value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "YES");
}

std::string ApiBase() {
    auto value = EnvString("SEKAILINK_BUG_REPORT_API_BASE");
    if (value.empty()) value = kDefaultApiBase;
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

std::string TrimForField(std::string value, std::size_t max_len) {
    value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
    while (!value.empty() && (value.front() == '\r' || value.front() == '\n' || value.front() == ' ' || value.front() == '\t')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    if (value.size() > max_len) value.resize(max_len);
    return value;
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (const unsigned char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    out += "\\u00";
                    constexpr char hex[] = "0123456789abcdef";
                    out.push_back(hex[(ch >> 4) & 0x0f]);
                    out.push_back(hex[ch & 0x0f]);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    return out;
}

std::string ReadTail(const std::filesystem::path& path, std::size_t max_bytes) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(std::max<std::streamoff>(0, in.tellg()));
    const auto offset = size > max_bytes ? size - max_bytes : 0;
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string ReporterName() {
    auto reporter = EnvString("SEKAILINK_REPORTER_NAME");
    if (reporter.empty()) reporter = EnvString("USERNAME");
    if (reporter.empty()) reporter = EnvString("USER");
    if (reporter.empty()) reporter = "SKLMI";
    return TrimForField(reporter, 80);
}

std::size_t WriteToString(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string BuildPayload(const BugReportContext& context) {
    const std::string title = TrimForField(context.title.empty() ? "SKLMI runtime error" : context.title, 100);
    const std::string description = TrimForField(context.description.empty() ? "SKLMI runtime error" : context.description, 200);
    std::ostringstream json;
    json << "{";
    json << "\"title\":\"" << JsonEscape(title.size() < 3 ? "SKLMI runtime error" : title) << "\",";
    json << "\"description\":\"" << JsonEscape(description.empty() ? "SKLMI runtime error" : description) << "\",";
    json << "\"reporter_name\":\"" << JsonEscape(ReporterName()) << "\",";
    json << "\"screenshot_base64\":\"\",";
    json << "\"logs_text\":\"" << JsonEscape(ReadTail(context.log_path, 700 * 1024)) << "\",";
    json << "\"system_info\":{";
    json << "\"platform\":\"sklmi-native\",";
    json << "\"os\":\"" << JsonEscape(EnvString("OS")) << "\",";
    json << "\"computer_name\":\"" << JsonEscape(EnvString("COMPUTERNAME")) << "\",";
    json << "\"hostname\":\"" << JsonEscape(EnvString("HOSTNAME")) << "\"";
    json << "},";
    json << "\"app_info\":{";
    json << "\"source\":\"" << JsonEscape(context.source) << "\",";
    json << "\"component\":\"" << JsonEscape(context.component) << "\",";
    json << "\"mode\":\"" << JsonEscape(context.mode) << "\",";
    json << "\"bridge_id\":\"" << JsonEscape(context.bridge_id) << "\",";
    json << "\"linkedworld_id\":\"" << JsonEscape(context.linkedworld_id) << "\",";
    json << "\"core_profile\":\"" << JsonEscape(context.core_profile) << "\",";
    json << "\"archipelago_client_wrapper\":\"" << JsonEscape(context.archipelago_client_wrapper) << "\",";
    json << "\"player_alias\":\"" << JsonEscape(context.player_alias) << "\",";
    json << "\"trace_log_path\":\"" << JsonEscape(context.log_path.string()) << "\"";
    json << "}";
    json << "}";
    return json.str();
}

}  // namespace

BugReportContext MakeBugReportContext(const RuntimeOptions& options,
                                      const std::string& title,
                                      const std::string& description) {
    BugReportContext context;
    context.title = title;
    context.description = description;
    context.log_path = options.trace_log_path;
    context.mode = options.mode;
    context.player_alias = options.player_alias.empty() ? options.ap_slot_name : options.player_alias;
    return context;
}

bool SubmitBugReport(const BugReportContext& context, std::string* error) {
    if (!EnvEnabled()) return true;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        if (error) *error = "curl_init_failed";
        return false;
    }
    const std::string url = ApiBase() + "/api/client/bug-report";
    const std::string payload = BuildPayload(context);
    std::string response;
    char curl_error[CURL_ERROR_SIZE]{};
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SKLMIBugReport/1.0");
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) {
        if (error) *error = std::string("curl_failed:") + curl_error;
        return false;
    }
    if (status < 200 || status >= 300) {
        if (error) *error = "http_status:" + std::to_string(status) + ":" + response;
        return false;
    }
    return true;
}

}  // namespace sekailink::sklmi
