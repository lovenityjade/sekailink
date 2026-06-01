#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {

std::string ReadTextFile(const std::filesystem::path& path);
std::string StripJsonComments(std::string text);
nlohmann::json ParseJsonWithComments(const std::filesystem::path& path);

std::string CanonicalToken(std::string_view value);
std::vector<std::string> ParseCodeList(const nlohmann::json& raw, const char* key);
void AppendUniqueCode(std::vector<std::string>& codes, std::string code);
void AppendCodeList(std::vector<std::string>& codes, const std::vector<std::string>& incoming);

std::string JsonStringFlexible(const nlohmann::json& root, const char* key);
int JsonIntFlexible(const nlohmann::json& root, const char* key, int fallback = 0);
bool JsonBoolFlexible(const nlohmann::json& root, const char* key, bool fallback = false);
std::string NormalizePackAssetPath(std::string path);

}  // namespace sekaiemu::spike
