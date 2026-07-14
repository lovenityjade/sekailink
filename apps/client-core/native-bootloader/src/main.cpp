#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif
#include <curl/curl.h>
#include <font8x8_basic.h>
#include <miniz.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <tlhelp32.h>
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr const char *kApiBase = "https://sekailink.com";
constexpr const char *kChannel = "canonical";
constexpr const char *kBuild = "release";
constexpr const char *kIssuer = "sekailink-bootstrapper";
constexpr const char *kAudience = "sekailink-client";
constexpr const char *kReleaseSigningPublicKeyBase64 = "F2d63y8lUu+/+3Cjqw3gk/toNhHBbJUdMACb6zGwHcY=";

struct Options {
  bool ui = true;
  bool launch = true;
  bool force = false;
  std::string apiBase = kApiBase;
  std::string channel = kChannel;
  std::string build = kBuild;
  fs::path installDir;
  fs::path zipTest;
  fs::path zipTestOut;
  std::string zipTestPlatform;
};

struct ReleaseInfo {
  std::string version;
  std::string platform;
  std::string artifactType;
  std::string downloadUrl;
  std::string sha256;
  std::string fileName;
  std::string channel;
  std::string build;
  std::string signature;
  std::uint64_t releaseSequence = 0;
  std::uintmax_t size = 0;
};

std::vector<unsigned char> decodeBase64(const std::string &value) {
  if (value.empty() || value.size() % 4 != 0) throw std::runtime_error("release_signature_invalid_base64");
  std::vector<unsigned char> decoded((value.size() / 4) * 3 + 1);
  const int size = EVP_DecodeBlock(decoded.data(),
      reinterpret_cast<const unsigned char *>(value.data()), static_cast<int>(value.size()));
  if (size < 0) throw std::runtime_error("release_signature_invalid_base64");
  std::size_t padding = 0;
  if (!value.empty() && value.back() == '=') ++padding;
  if (value.size() > 1 && value[value.size() - 2] == '=') ++padding;
  decoded.resize(static_cast<std::size_t>(size) - padding);
  return decoded;
}

std::string releaseSignaturePayload(const ReleaseInfo &info) {
  return "sekailink-release-v1\nversion=" + info.version +
      "\nchannel=" + info.channel +
      "\nplatform=" + info.platform +
      "\nbuild=" + info.build +
      "\nartifact_type=" + info.artifactType +
      "\ndownload_url=" + info.downloadUrl +
      "\nsha256=" + info.sha256 +
      "\nrelease_sequence=" + std::to_string(info.releaseSequence) + "\n";
}

void verifyReleaseSignature(const ReleaseInfo &info) {
  const auto publicKey = decodeBase64(kReleaseSigningPublicKeyBase64);
  const auto signature = decodeBase64(info.signature);
  if (publicKey.size() != 32 || signature.size() != 64) {
    throw std::runtime_error("release_signature_invalid_size");
  }
  EVP_PKEY *key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, publicKey.data(), publicKey.size());
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!key || !ctx) {
    EVP_PKEY_free(key);
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("release_signature_context_failed");
  }
  const std::string payload = releaseSignaturePayload(info);
  const bool valid = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
      EVP_DigestVerify(ctx, signature.data(), signature.size(),
          reinterpret_cast<const unsigned char *>(payload.data()), payload.size()) == 1;
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(key);
  if (!valid) throw std::runtime_error("release_signature_mismatch");
}

struct UiState {
  std::mutex mutex;
  std::string phase = "Checking release";
  std::string detail = "Merci de patienter quelques instants.";
  std::string error;
  double progress = 0.0;
  bool done = false;
  bool failed = false;
  bool copyRequested = false;
  bool reportInFlight = false;
  std::string reportStatus;
  std::string reportUrl;
  fs::path reportLogPath;
  std::string reportChannel;
  std::string reportBuild;
  fs::path reportInstallDir;
};

struct DownloadSink {
  std::ofstream file;
  UiState *ui = nullptr;
};

std::string trim(std::string value) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string nowStamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::int64_t epochMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  ).count();
}

void setUi(UiState *ui, std::string phase, std::string detail = {}, double progress = -1.0) {
  if (!ui) return;
  std::lock_guard lock(ui->mutex);
  ui->phase = std::move(phase);
  ui->detail = std::move(detail);
  if (progress >= 0.0) ui->progress = std::clamp(progress, 0.0, 1.0);
}

void failUi(UiState *ui, const std::string &message) {
  if (!ui) return;
  std::lock_guard lock(ui->mutex);
  ui->failed = true;
  ui->done = true;
  ui->error = message;
  ui->phase = "Error";
  ui->detail = "Copy the error from this window or check the log.";
}

void doneUi(UiState *ui, std::string phase, std::string detail) {
  if (!ui) return;
  std::lock_guard lock(ui->mutex);
  ui->done = true;
  ui->phase = std::move(phase);
  ui->detail = std::move(detail);
  ui->progress = 1.0;
}

fs::path homeDir() {
#ifdef _WIN32
  const char *user = std::getenv("USERPROFILE");
  if (user && *user) return fs::u8path(user);
  const char *drive = std::getenv("HOMEDRIVE");
  const char *path = std::getenv("HOMEPATH");
  if (drive && path) return fs::u8path(std::string(drive) + path);
#else
  const char *home = std::getenv("HOME");
  if (home && *home) return fs::u8path(home);
#endif
  return fs::current_path();
}

fs::path appDataDir() {
#ifdef _WIN32
  const char *appdata = std::getenv("APPDATA");
  if (appdata && *appdata) return fs::u8path(appdata);
  return homeDir() / "AppData" / "Roaming";
#else
  const char *state = std::getenv("XDG_STATE_HOME");
  if (state && *state) return fs::u8path(state);
  return homeDir() / ".local" / "state";
#endif
}

fs::path defaultInstallDir() {
#ifdef _WIN32
  const char *local = std::getenv("LOCALAPPDATA");
  if (local && *local) return fs::u8path(local) / "Programs" / "sekailink-client";
  return homeDir() / "AppData" / "Local" / "Programs" / "sekailink-client";
#else
  const char *data = std::getenv("XDG_DATA_HOME");
  if (data && *data) return fs::u8path(data) / "sekailink-client";
  return homeDir() / ".local" / "share" / "sekailink-client";
#endif
}

fs::path executableDir() {
#ifdef _WIN32
  std::wstring buffer(MAX_PATH, L'\0');
  DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  while (len == buffer.size()) {
    buffer.resize(buffer.size() * 2);
    len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  }
  if (len > 0) {
    buffer.resize(len);
    return fs::path(buffer).parent_path();
  }
#else
  std::error_code ec;
  const fs::path self = fs::read_symlink("/proc/self/exe", ec);
  if (!ec && !self.empty()) return self.parent_path();
#endif
  return fs::current_path();
}

std::string platformId() {
#ifdef _WIN32
  return "win32-x64";
#else
  return "linux-x64";
#endif
}

fs::path logDir() {
  return appDataDir() / "sekailink-bootloader" / "logs";
}

fs::path workDir() {
  return appDataDir() / "sekailink-bootloader" / "work";
}

class Logger {
public:
  Logger() {
    fs::create_directories(logDir());
    path_ = logDir() / ("bootloader-" + nowStampForFile() + ".log");
    out_.open(path_, std::ios::out | std::ios::app);
  }

  void line(const std::string &scope, const std::string &message) {
    std::lock_guard lock(mutex_);
    const std::string row = nowStamp() + " [" + scope + "] " + message;
    if (out_) out_ << row << "\n";
    std::cout << row << std::endl;
  }

  fs::path path() const { return path_; }

private:
  static std::string nowStampForFile() {
    std::string s = nowStamp();
    for (char &c : s) {
      if (c == ':' || c == '.') c = '-';
    }
    return s;
  }

  std::mutex mutex_;
  fs::path path_;
  std::ofstream out_;
};

std::string readText(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string limitTail(std::string value, std::size_t maxLen) {
  if (value.size() <= maxLen) return value;
  return value.substr(value.size() - maxLen);
}

std::string firstLine(std::string value, std::size_t maxLen) {
  const auto newline = value.find_first_of("\r\n");
  if (newline != std::string::npos) value.resize(newline);
  value = trim(value);
  if (value.size() > maxLen) value.resize(maxLen);
  return value;
}

void writeText(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("write_failed:" + path.u8string());
  out << text;
}

std::string hexLower(const unsigned char *data, std::size_t len) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < len; ++i) out << std::setw(2) << static_cast<int>(data[i]);
  return out.str();
}

std::string sha256File(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("sha256_open_failed:" + path.u8string());
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) throw std::runtime_error("sha256_ctx_failed");
  unsigned char digest[SHA256_DIGEST_LENGTH]{};
  unsigned int digestLen = 0;
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  std::vector<char> buf(1024 * 1024);
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const auto n = in.gcount();
    if (n > 0) EVP_DigestUpdate(ctx, buf.data(), static_cast<std::size_t>(n));
  }
  EVP_DigestFinal_ex(ctx, digest, &digestLen);
  EVP_MD_CTX_free(ctx);
  return hexLower(digest, digestLen);
}

std::string randomSecret() {
  unsigned char bytes[32]{};
  if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("rng_failed");
  static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (std::size_t i = 0; i < sizeof(bytes); i += 3) {
    const unsigned int b0 = bytes[i];
    const unsigned int b1 = (i + 1 < sizeof(bytes)) ? bytes[i + 1] : 0;
    const unsigned int b2 = (i + 2 < sizeof(bytes)) ? bytes[i + 2] : 0;
    out.push_back(table[(b0 >> 2) & 63]);
    out.push_back(table[((b0 & 3) << 4) | ((b1 >> 4) & 15)]);
    out.push_back(i + 1 < sizeof(bytes) ? table[((b1 & 15) << 2) | ((b2 >> 6) & 3)] : '=');
    out.push_back(i + 2 < sizeof(bytes) ? table[b2 & 63] : '=');
  }
  return out;
}

std::string base64Url(const std::string &input) {
  static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  const auto *data = reinterpret_cast<const unsigned char *>(input.data());
  const std::size_t len = input.size();
  for (std::size_t i = 0; i < len; i += 3) {
    const unsigned int b0 = data[i];
    const unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0;
    const unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0;
    out.push_back(table[(b0 >> 2) & 63]);
    out.push_back(table[((b0 & 3) << 4) | ((b1 >> 4) & 15)]);
    if (i + 1 < len) out.push_back(table[((b1 & 15) << 2) | ((b2 >> 6) & 3)]);
    if (i + 2 < len) out.push_back(table[b2 & 63]);
  }
  return out;
}

std::string hmacSha256Hex(const std::string &secret, const std::string &body) {
  unsigned char digest[EVP_MAX_MD_SIZE]{};
  unsigned int len = 0;
  HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
       reinterpret_cast<const unsigned char *>(body.data()), body.size(), digest, &len);
  return hexLower(digest, len);
}

std::string ensureLauncherSecret(const fs::path &stateDir) {
  const fs::path path = stateDir / "launcher-secret.key";
  std::string existing = trim(readText(path));
  if (existing.size() >= 32) return existing;
  const std::string secret = randomSecret();
  writeText(path, secret + "\n");
  return secret;
}

std::string newLaunchToken(const std::string &secret) {
  const auto now = epochMillis();
  json payload = {
    {"iss", kIssuer},
    {"aud", kAudience},
    {"iat", now},
    {"exp", now + 5 * 60 * 1000},
  };
  const std::string body = base64Url(payload.dump());
  return body + "." + hmacSha256Hex(secret, body);
}

std::size_t writeToString(char *ptr, std::size_t size, std::size_t nmemb, void *userdata) {
  auto *s = static_cast<std::string *>(userdata);
  s->append(ptr, size * nmemb);
  return size * nmemb;
}

std::size_t writeToFile(char *ptr, std::size_t size, std::size_t nmemb, void *userdata) {
  auto *sink = static_cast<DownloadSink *>(userdata);
  sink->file.write(ptr, static_cast<std::streamsize>(size * nmemb));
  return sink->file ? size * nmemb : 0;
}

int curlProgress(void *clientp, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
  auto *ui = static_cast<UiState *>(clientp);
  if (ui && total > 0) setUi(ui, "Downloading update", std::to_string(now / (1024 * 1024)) + " / " + std::to_string(total / (1024 * 1024)) + " MiB", static_cast<double>(now) / static_cast<double>(total));
  return 0;
}

void configureCurl(CURL *curl, const std::string &url, char *err) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "SekaiLinkNativeBootloader/1.0");
#ifdef _WIN32
  static const std::string caInfo = (executableDir() / "ca-bundle.crt").u8string();
  if (fs::exists(fs::u8path(caInfo))) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, caInfo.c_str());
  }
#endif
}

std::string httpPostJson(const std::string &url, const json &payload, Logger &log) {
  CURL *curl = curl_easy_init();
  if (!curl) throw std::runtime_error("curl_init_failed");
  std::string body;
  const std::string requestBody = payload.dump();
  char err[CURL_ERROR_SIZE]{};
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  configureCurl(curl, url, err);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
  const CURLcode res = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  log.line("http", "POST " + url + " status=" + std::to_string(status));
  if (res != CURLE_OK) throw std::runtime_error(std::string("http_post_failed:") + err);
  if (status < 200 || status >= 300) throw std::runtime_error("http_post_status:" + std::to_string(status) + ":" + body);
  return body;
}

std::string normalizedApiBase(std::string value) {
  value = trim(value);
  while (!value.empty() && value.back() == '/') value.pop_back();
  return value.empty() ? std::string(kApiBase) : value;
}

std::string envString(const char *name) {
  const char *value = std::getenv(name);
  return value && *value ? std::string(value) : std::string{};
}

std::string normalizeReleaseChannel(std::string value) {
  value = lower(trim(std::move(value)));
  if (value == "canary") return "canari";
  if (value == "stable" || value == "release" || value == "test") return "canonical";
  if (value == "canari" || value == "canonical") return value;
  return "canonical";
}

fs::path releaseChannelPreferencePath() {
  return appDataDir() / "sekailink-bootloader" / "release-channel.json";
}

std::string readReleaseChannelPreference() {
  const std::string fromEnv = trim(envString("SEKAILINK_RELEASE_CHANNEL"));
  if (!fromEnv.empty()) return normalizeReleaseChannel(fromEnv);
  const fs::path path = releaseChannelPreferencePath();
  try {
    const std::string text = trim(readText(path));
    if (text.empty()) return "";
    if (!text.empty() && text.front() == '{') {
      const auto parsed = json::parse(text);
      return normalizeReleaseChannel(parsed.value("channel", ""));
    }
    return normalizeReleaseChannel(text);
  } catch (...) {
  }
  return "";
}

void submitBootloaderBugReport(
    const std::string &url,
    const std::string &error,
    const fs::path &logPath,
    const std::string &channel,
    const std::string &build,
    const fs::path &installDir) {
  Logger log;
  std::string reporter = envString("USERNAME");
  if (reporter.empty()) reporter = envString("USER");
  if (reporter.empty()) reporter = "SekaiLink Bootloader";
  if (reporter.size() > 80) reporter.resize(80);
  std::string description = firstLine(error, 200);
  if (description.empty()) description = "Bootloader error";
  const std::string logs = limitTail(readText(logPath), 700 * 1024);
  json payload = {
      {"title", "Bootloader error"},
      {"description", description},
      {"reporter_name", reporter},
      {"screenshot_base64", ""},
      {"logs_text", logs},
      {"system_info", {
          {"platform", platformId()},
          {"user", reporter},
          {"computer_name", envString("COMPUTERNAME")},
          {"hostname", envString("HOSTNAME")},
      }},
      {"app_info", {
          {"source", "bootloader"},
          {"component", "native-bootloader"},
          {"channel", channel},
          {"build", build},
          {"install_dir", installDir.u8string()},
          {"log_path", logPath.u8string()},
      }},
  };
  httpPostJson(url, payload, log);
}

std::string httpGet(const std::string &url, Logger &log) {
  CURL *curl = curl_easy_init();
  if (!curl) throw std::runtime_error("curl_init_failed");
  std::string body;
  char err[CURL_ERROR_SIZE]{};
  configureCurl(curl, url, err);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  const CURLcode res = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  log.line("http", "GET " + url + " status=" + std::to_string(status));
  if (res != CURLE_OK) throw std::runtime_error(std::string("http_get_failed:") + err);
  if (status < 200 || status >= 300) throw std::runtime_error("http_status:" + std::to_string(status));
  return body;
}

void downloadFile(const std::string &url, const fs::path &outPath, UiState *ui, Logger &log) {
  fs::create_directories(outPath.parent_path());
  DownloadSink sink;
  sink.file.open(outPath, std::ios::binary | std::ios::trunc);
  sink.ui = ui;
  if (!sink.file) throw std::runtime_error("download_open_failed:" + outPath.u8string());
  CURL *curl = curl_easy_init();
  if (!curl) throw std::runtime_error("curl_init_failed");
  char err[CURL_ERROR_SIZE]{};
  configureCurl(curl, url, err);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, ui);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  const CURLcode res = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  sink.file.close();
  log.line("download", "GET " + url + " status=" + std::to_string(status) + " file=" + outPath.u8string());
  if (res != CURLE_OK) throw std::runtime_error(std::string("download_failed:") + err);
  if (status < 200 || status >= 300) throw std::runtime_error("download_status:" + std::to_string(status));
}

ReleaseInfo fetchRelease(const Options &options, Logger &log) {
  std::string url = options.apiBase;
  while (!url.empty() && url.back() == '/') url.pop_back();
  url += "/api/client/release-latest?channel=" + options.channel + "&platform=" + platformId() + "&build=" + options.build;
  const auto body = httpGet(url, log);
  const auto parsed = json::parse(body);
  ReleaseInfo info;
  info.version = parsed.value("version", "");
  info.platform = parsed.value("platform", "");
  info.artifactType = parsed.value("artifact_type", parsed.value("artifactType", ""));
  info.downloadUrl = parsed.value("download_url", parsed.value("downloadUrl", ""));
  info.sha256 = lower(parsed.value("sha256", ""));
  info.fileName = parsed.value("file_name", parsed.value("fileName", ""));
  info.channel = parsed.value("channel", options.channel);
  info.build = parsed.value("build", options.build);
  info.signature = parsed.value("signature", "");
  info.releaseSequence = parsed.value("release_sequence", 0ull);
  info.size = parsed.value("size", 0ull);
  if (info.version.empty() || info.downloadUrl.empty() || info.sha256.empty() || info.signature.empty() ||
      info.releaseSequence == 0) {
    throw std::runtime_error("release_manifest_incomplete");
  }
  verifyReleaseSignature(info);
  log.line("release", "version=" + info.version + " url=" + info.downloadUrl + " sha256=" + info.sha256);
  return info;
}

std::string installedVersion(const fs::path &stateDir, const fs::path &installDir) {
  for (const fs::path &candidate : {stateDir / "install-state.json", installDir / ".sekailink" / "install-state.json"}) {
    try {
      const std::string text = readText(candidate);
      if (!text.empty()) {
        const auto parsed = json::parse(text);
        const std::string version = parsed.value("version", parsed.value("manifestVersion", ""));
        if (!version.empty()) return version;
      }
    } catch (...) {
    }
  }
  return {};
}

std::uint64_t installedReleaseSequence(const fs::path &stateDir, const fs::path &installDir) {
  std::uint64_t sequence = 0;
  for (const fs::path &candidate : {stateDir / "install-state.json", installDir / ".sekailink" / "install-state.json"}) {
    try {
      const std::string text = readText(candidate);
      if (!text.empty()) {
        const auto candidate = json::parse(text).value("releaseSequence", std::uint64_t{0});
        sequence = std::max(sequence, candidate);
      }
    } catch (...) {
    }
  }
  return sequence;
}

bool isSafeZipPath(const std::string &name) {
  if (name.empty()) return false;
  if (name.find('\0') != std::string::npos) return false;
  const std::string n = name;
  if (n[0] == '/' || n[0] == '\\') return false;
  if (n.size() > 1 && std::isalpha(static_cast<unsigned char>(n[0])) && n[1] == ':') return false;
  fs::path p = fs::u8path(n);
  for (const auto &part : p) {
    if (part == "..") return false;
  }
  return true;
}

bool isZipDirectoryLike(const mz_zip_archive &zip, mz_uint index, const mz_zip_archive_file_stat &st) {
  const std::string name = st.m_filename;
  return mz_zip_reader_is_file_a_directory(const_cast<mz_zip_archive *>(&zip), index) ||
         (!name.empty() && (name.back() == '/' || name.back() == '\\') && st.m_uncomp_size == 0);
}

void extractZip(const fs::path &zipPath, const fs::path &outDir, UiState *ui, Logger &log) {
  fs::remove_all(outDir);
  fs::create_directories(outDir);
  mz_zip_archive zip{};
  if (!mz_zip_reader_init_file(&zip, zipPath.u8string().c_str(), 0)) {
    throw std::runtime_error("zip_open_failed:" + zipPath.u8string());
  }
  const mz_uint count = mz_zip_reader_get_num_files(&zip);
  log.line("zip", "extract count=" + std::to_string(count) + " to=" + outDir.u8string());
  for (mz_uint i = 0; i < count; ++i) {
    mz_zip_archive_file_stat st{};
    if (!mz_zip_reader_file_stat(&zip, i, &st)) {
      mz_zip_reader_end(&zip);
      throw std::runtime_error("zip_stat_failed:" + std::to_string(i));
    }
    const std::string name = st.m_filename;
    if (!isSafeZipPath(name)) {
      mz_zip_reader_end(&zip);
      throw std::runtime_error("zip_unsafe_path:" + name);
    }
    const fs::path target = outDir / fs::u8path(name);
    if (isZipDirectoryLike(zip, i, st)) {
      fs::create_directories(target);
    } else {
      fs::create_directories(target.parent_path());
      if (!mz_zip_reader_extract_to_file(&zip, i, target.u8string().c_str(), 0)) {
        mz_zip_reader_end(&zip);
        throw std::runtime_error("zip_extract_failed:" + name);
      }
    }
    if (ui && count > 0 && i % 8 == 0) {
      setUi(ui, "Extracting update", name, static_cast<double>(i) / static_cast<double>(count));
    }
  }
  mz_zip_reader_end(&zip);
}

fs::path resolveBundleRoot(const fs::path &extractDir) {
  if (fs::exists(extractDir / "resources" / "app.asar")) return extractDir;
  fs::path only;
  int count = 0;
  for (const auto &entry : fs::directory_iterator(extractDir)) {
    if (entry.path().filename() == "__MACOSX") continue;
    only = entry.path();
    ++count;
  }
  if (count == 1 && fs::is_directory(only) && fs::exists(only / "resources" / "app.asar")) return only;
  return extractDir;
}

fs::path clientExeName() {
#ifdef _WIN32
  return "SekaiLink Client.exe";
#else
  return "sekailink-client";
#endif
}

fs::path clientExeNameForPlatform(const std::string &platform) {
  if (platform.rfind("win32", 0) == 0) return "SekaiLink Client.exe";
  if (platform.rfind("linux", 0) == 0) return "sekailink-client";
  return clientExeName();
}

void validateBundle(const fs::path &bundleRoot, const std::string &platform = platformId()) {
  const fs::path appAsar = bundleRoot / "resources" / "app.asar";
  const fs::path resourcesPak = bundleRoot / "resources.pak";
  const fs::path exe = bundleRoot / clientExeNameForPlatform(platform);
  if (!fs::exists(exe)) throw std::runtime_error("bundle_missing_client_executable:" + exe.u8string());
  if (!fs::exists(appAsar)) throw std::runtime_error("bundle_missing_app_asar:" + appAsar.u8string());
  if (fs::file_size(appAsar) < 1024 * 1024) throw std::runtime_error("bundle_app_asar_too_small:" + std::to_string(fs::file_size(appAsar)));
  if (!fs::exists(resourcesPak)) throw std::runtime_error("bundle_missing_resources_pak:" + resourcesPak.u8string());
}

bool processRunningByName(const std::vector<std::string> &names) {
#ifdef _WIN32
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool found = false;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      char narrow[MAX_PATH]{};
      WideCharToMultiByte(CP_UTF8, 0, entry.szExeFile, -1, narrow, MAX_PATH, nullptr, nullptr);
      const std::string exe = lower(narrow);
      for (const auto &name : names) {
        if (exe == lower(name)) found = true;
      }
    } while (!found && Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return found;
#else
  for (const auto &entry : fs::directory_iterator("/proc")) {
    if (!entry.is_directory()) continue;
    const std::string pid = entry.path().filename().string();
    if (!std::all_of(pid.begin(), pid.end(), ::isdigit)) continue;
    const std::string comm = lower(trim(readText(entry.path() / "comm")));
    for (const auto &name : names) {
      if (comm == lower(name)) return true;
    }
  }
  return false;
#endif
}

bool shouldPreserveTargetEntry(const fs::path &path) {
  const std::string name = lower(path.filename().u8string());
  return name == ".sekailink" ||
         name == "sekailink bootloader" ||
         name == "sekailink bootloader linux" ||
         name == "sekailink bootloader.exe" ||
         name == "sekailink-bootloader" ||
         name == "readme.md";
}

void clearInstallDir(const fs::path &installDir) {
  fs::create_directories(installDir);
  for (const auto &entry : fs::directory_iterator(installDir)) {
    if (shouldPreserveTargetEntry(entry.path())) continue;
    fs::remove_all(entry.path());
  }
}

bool isBootloaderTopLevelPath(const fs::path &relativePath) {
  auto it = relativePath.begin();
  if (it == relativePath.end()) return false;
  const std::string first = lower(it->u8string());
  return first == "sekailink bootloader" || first == "sekailink bootloader linux";
}

void copyTree(const fs::path &source, const fs::path &target, bool preserveExistingBootloader = false) {
  fs::create_directories(target);
  for (const auto &entry : fs::recursive_directory_iterator(source)) {
    const fs::path rel = fs::relative(entry.path(), source);
    if (preserveExistingBootloader && isBootloaderTopLevelPath(rel) && fs::exists(target / *rel.begin())) {
      continue;
    }
    const fs::path dst = target / rel;
    if (entry.is_directory()) {
      fs::create_directories(dst);
    } else if (entry.is_regular_file()) {
      fs::create_directories(dst.parent_path());
      fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
#ifndef _WIN32
      std::error_code ec;
      fs::permissions(dst, entry.status().permissions(), ec);
#endif
    }
  }
}

void writeInstallState(const fs::path &stateDir, const fs::path &installDir, const ReleaseInfo &release, const Options &options) {
  json state = {
    {"version", release.version},
    {"manifestVersion", release.version},
    {"channel", options.channel},
    {"build", options.build},
    {"platform", platformId()},
    {"artifactType", release.artifactType.empty() ? "client-bundle" : release.artifactType},
    {"releaseSequence", release.releaseSequence},
    {"installDir", installDir.u8string()},
    {"updatedAt", nowStamp()},
  };
  writeText(stateDir / "install-state.json", state.dump(2) + "\n");
  fs::create_directories(installDir / ".sekailink");
  writeText(installDir / ".sekailink" / "install-state.json", state.dump(2) + "\n");
}

void installBundle(const fs::path &bundleRoot, const fs::path &installDir, const fs::path &stateDir, const ReleaseInfo &release, const Options &options, UiState *ui, Logger &log) {
  if (processRunningByName({"SekaiLink Client.exe", "sekailink-client", "sekailink client"})) {
    throw std::runtime_error("client_running_close_sekailink_and_retry");
  }
  setUi(ui, "Installing update", "Preparing install directory", 0.0);
  clearInstallDir(installDir);
  setUi(ui, "Installing update", "Copying client files", 0.35);
  copyTree(bundleRoot, installDir, true);
  setUi(ui, "Installing update", "Writing install state", 0.85);
  writeInstallState(stateDir, installDir, release, options);
  log.line("install", "installed version=" + release.version + " dir=" + installDir.u8string());
}

void launchClient(const fs::path &installDir, const fs::path &stateDir, Logger &log) {
  const std::string secret = ensureLauncherSecret(stateDir);
  fs::create_directories(installDir / ".sekailink");
  writeText(installDir / ".sekailink" / "launcher-secret.key", secret + "\n");
  const std::string token = newLaunchToken(secret);
  const fs::path exe = installDir / clientExeName();
  if (!fs::exists(exe)) throw std::runtime_error("client_executable_missing:" + exe.u8string());
  log.line("launch", "starting " + exe.u8string());
#ifdef _WIN32
  std::wstring command = L"\"" + exe.wstring() + L"\"";
  std::wstring cwd = installDir.wstring();
  SetEnvironmentVariableA("SEKAILINK_BOOTSTRAP_INSTALL_DIR", installDir.u8string().c_str());
  SetEnvironmentVariableA("SEKAILINK_BOOTSTRAP_STATE_DIR", stateDir.u8string().c_str());
  SetEnvironmentVariableA("SEKAILINK_BOOTSTRAP_TOKEN", token.c_str());
  SetEnvironmentVariableA("SEKAILINK_REQUIRE_BOOTSTRAP", "1");
  STARTUPINFOW si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi)) {
    throw std::runtime_error("create_process_failed:" + std::to_string(GetLastError()));
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
#else
  std::vector<std::string> envStrings;
  for (char **env = environ; *env; ++env) envStrings.emplace_back(*env);
  envStrings.emplace_back("SEKAILINK_BOOTSTRAP_INSTALL_DIR=" + installDir.u8string());
  envStrings.emplace_back("SEKAILINK_BOOTSTRAP_STATE_DIR=" + stateDir.u8string());
  envStrings.emplace_back("SEKAILINK_BOOTSTRAP_TOKEN=" + token);
  envStrings.emplace_back("SEKAILINK_REQUIRE_BOOTSTRAP=1");
  std::vector<char *> envp;
  for (auto &s : envStrings) envp.push_back(s.data());
  envp.push_back(nullptr);
  std::string exeString = exe.u8string();
  char *argv[] = {exeString.data(), nullptr};
  pid_t pid = 0;
  const int rc = posix_spawn(&pid, exeString.c_str(), nullptr, nullptr, argv, envp.data());
  if (rc != 0) throw std::runtime_error("posix_spawn_failed:" + std::to_string(rc));
#endif
}

void runUpdate(const Options &options, UiState *ui, Logger &log) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  const fs::path installDir = options.installDir.empty() ? defaultInstallDir() : options.installDir;
  const fs::path stateDir = installDir / ".sekailink";
  setUi(ui, "Checking release", platformId(), 0.02);
  ReleaseInfo release = fetchRelease(options, log);
  const std::string current = installedVersion(stateDir, installDir);
  const std::uint64_t currentSequence = installedReleaseSequence(stateDir, installDir);
  if (currentSequence > 0 && release.releaseSequence < currentSequence) {
    throw std::runtime_error("release_rollback_rejected:" + std::to_string(release.releaseSequence) +
        "<" + std::to_string(currentSequence));
  }
  if (!options.force && !current.empty() && current == release.version && fs::exists(installDir / clientExeName())) {
    log.line("update", "already current version=" + current);
    setUi(ui, "SekaiLink is up to date", release.version, 1.0);
    if (options.launch) launchClient(installDir, stateDir, log);
    doneUi(ui, "SekaiLink launched", release.version);
    return;
  }
  fs::remove_all(workDir());
  fs::create_directories(workDir());
  const fs::path zipPath = workDir() / (release.fileName.empty() ? "sekailink-client.zip" : release.fileName);
  setUi(ui, "Downloading update", release.version, 0.05);
  downloadFile(release.downloadUrl, zipPath, ui, log);
  setUi(ui, "Verifying download", "SHA256", 0.0);
  const std::string actual = sha256File(zipPath);
  log.line("verify", "expected=" + release.sha256 + " actual=" + actual);
  if (actual != release.sha256) throw std::runtime_error("sha256_mismatch:" + actual);
  const fs::path extractDir = workDir() / "extract";
  setUi(ui, "Extracting update", zipPath.filename().u8string(), 0.0);
  extractZip(zipPath, extractDir, ui, log);
  const fs::path bundleRoot = resolveBundleRoot(extractDir);
  setUi(ui, "Validating bundle", bundleRoot.u8string(), 0.0);
  validateBundle(bundleRoot);
  installBundle(bundleRoot, installDir, stateDir, release, options, ui, log);
  fs::remove_all(workDir());
  if (options.launch) {
    setUi(ui, "Launching SekaiLink", release.version, 1.0);
    launchClient(installDir, stateDir, log);
    doneUi(ui, "SekaiLink launched", release.version);
  } else {
    doneUi(ui, "Update installed", release.version);
  }
  curl_global_cleanup();
}

Options parseOptions(int argc, char **argv) {
  Options options;
  const std::string preferredChannel = readReleaseChannelPreference();
  if (!preferredChannel.empty()) options.channel = preferredChannel;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) throw std::runtime_error("missing_argument:" + arg);
      return argv[++i];
    };
    if (arg == "--no-ui") options.ui = false;
    else if (arg == "--no-launch") options.launch = false;
    else if (arg == "--force") options.force = true;
    else if (arg == "--api-base") options.apiBase = next();
    else if (arg == "--channel") options.channel = normalizeReleaseChannel(next());
    else if (arg == "--build") options.build = next();
    else if (arg == "--install-dir") options.installDir = fs::u8path(next());
    else if (arg == "--zip-test") options.zipTest = fs::u8path(next());
    else if (arg == "--zip-test-out") options.zipTestOut = fs::u8path(next());
    else if (arg == "--zip-test-platform") options.zipTestPlatform = next();
    else throw std::runtime_error("unknown_argument:" + arg);
  }
  return options;
}

void drawText(SDL_Renderer *renderer, const std::string &text, int x, int y, SDL_Color color, int scale = 2, int wrap = 86) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  int cx = x;
  int cy = y;
  int col = 0;
  for (char raw : text) {
    unsigned char ch = static_cast<unsigned char>(raw);
    if (raw == '\n' || (wrap > 0 && col >= wrap && raw == ' ')) {
      cx = x;
      cy += 10 * scale;
      col = 0;
      continue;
    }
    if (wrap > 0 && col >= wrap) {
      cx = x;
      cy += 10 * scale;
      col = 0;
    }
    if (ch >= 128) ch = '?';
    for (int row = 0; row < 8; ++row) {
      for (int bit = 0; bit < 8; ++bit) {
        if ((font8x8_basic[ch][row] >> bit) & 1) {
          SDL_Rect pixel{cx + bit * scale, cy + row * scale, scale, scale};
          SDL_RenderFillRect(renderer, &pixel);
        }
      }
    }
    cx += 8 * scale;
    ++col;
  }
}

void fillRect(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void strokeRect(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void fillRoundedRect(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color color) {
  radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
  fillRect(renderer, SDL_Rect{rect.x + radius, rect.y, rect.w - radius * 2, rect.h}, color);
  fillRect(renderer, SDL_Rect{rect.x, rect.y + radius, rect.w, rect.h - radius * 2}, color);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int dy = 0; dy < radius; ++dy) {
    for (int dx = 0; dx < radius; ++dx) {
      if (dx * dx + dy * dy <= radius * radius) {
        SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + radius - dy);
        SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + radius - dy);
        SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + rect.h - radius + dy - 1);
        SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + rect.h - radius + dy - 1);
      }
    }
  }
}

void strokeRoundedRect(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color color) {
  radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawLine(renderer, rect.x + radius, rect.y, rect.x + rect.w - radius, rect.y);
  SDL_RenderDrawLine(renderer, rect.x + radius, rect.y + rect.h - 1, rect.x + rect.w - radius, rect.y + rect.h - 1);
  SDL_RenderDrawLine(renderer, rect.x, rect.y + radius, rect.x, rect.y + rect.h - radius);
  SDL_RenderDrawLine(renderer, rect.x + rect.w - 1, rect.y + radius, rect.x + rect.w - 1, rect.y + rect.h - radius);
  for (int i = 0; i <= radius; ++i) {
    const int y = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - i * i)));
    SDL_RenderDrawPoint(renderer, rect.x + radius - i, rect.y + radius - y);
    SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + i - 1, rect.y + radius - y);
    SDL_RenderDrawPoint(renderer, rect.x + radius - i, rect.y + rect.h - radius + y - 1);
    SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + i - 1, rect.y + rect.h - radius + y - 1);
  }
}

void drawSoftBackground(SDL_Renderer *renderer, int w, int h) {
  for (int y = 0; y < h; ++y) {
    const double t = static_cast<double>(y) / static_cast<double>(std::max(1, h - 1));
    const Uint8 r = static_cast<Uint8>(11 + 8 * t);
    const Uint8 g = static_cast<Uint8>(16 + 7 * t);
    const Uint8 b = static_cast<Uint8>(24 + 18 * t);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderDrawLine(renderer, 0, y, w, y);
  }
  fillRoundedRect(renderer, SDL_Rect{544, -44, 210, 150}, 42, SDL_Color{65, 38, 102, 96});
  fillRoundedRect(renderer, SDL_Rect{-64, 260, 190, 118}, 40, SDL_Color{20, 84, 90, 80});
}

void applyRoundedWindowShape(SDL_Window *window, int width, int height, int radius) {
#ifdef _WIN32
  SDL_SysWMinfo wmInfo;
  SDL_VERSION(&wmInfo.version);
  if (!window || !SDL_GetWindowWMInfo(window, &wmInfo)) return;
  HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
  if (!region) return;
  if (!SetWindowRgn(wmInfo.info.win.window, region, TRUE)) {
    DeleteObject(region);
  }
#else
  (void)window;
  (void)width;
  (void)height;
  (void)radius;
#endif
}

std::string friendlyPhase(const std::string &phase, bool failed) {
  if (failed) return "Quelque chose bloque la mise a jour";
  if (phase.find("Checking") != std::string::npos) return "Verification de la version";
  if (phase.find("Downloading") != std::string::npos) return "Mise a jour en telechargement";
  if (phase.find("Verifying") != std::string::npos) return "Verification du telechargement";
  if (phase.find("Extracting") != std::string::npos) return "Preparation des fichiers";
  if (phase.find("Validating") != std::string::npos) return "Validation du client";
  if (phase.find("Installing") != std::string::npos) return "Installation de SekaiLink";
  if (phase.find("Launching") != std::string::npos) return "Lancement de SekaiLink";
  if (phase.find("up to date") != std::string::npos) return "SekaiLink est deja a jour";
  if (phase.find("launched") != std::string::npos) return "SekaiLink demarre";
  if (phase.find("installed") != std::string::npos) return "Mise a jour terminee";
  return phase;
}

std::string friendlyDetail(const std::string &phase, const std::string &detail, bool failed) {
  if (failed) return "Le journal de diagnostic est pret si on doit regarder.";
  if (phase.find("Checking") != std::string::npos) return "On regarde si une nouvelle version est disponible.";
  if (phase.find("Downloading") != std::string::npos) return "Une mise a jour de SekaiLink est disponible. Merci de patienter.";
  if (phase.find("Verifying") != std::string::npos) return "On confirme que le fichier recu est intact.";
  if (phase.find("Extracting") != std::string::npos) return "On prepare la nouvelle version avant installation.";
  if (phase.find("Validating") != std::string::npos) return "On verifie que tous les fichiers essentiels sont presents.";
  if (phase.find("Installing") != std::string::npos) return "SekaiLink remplace les anciens fichiers proprement.";
  if (phase.find("Launching") != std::string::npos) return "La mise a jour est terminee. SekaiLink va s'ouvrir.";
  if (phase.find("up to date") != std::string::npos) return "Aucune action necessaire, tout est pret.";
  if (phase.find("launched") != std::string::npos) return "Tu peux reprendre la ou tu en etais.";
  if (!detail.empty()) return detail;
  return "Merci de patienter quelques instants.";
}

void runUi(UiState &state, std::thread &worker, const fs::path &logPath) {
  (void)logPath;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    if (worker.joinable()) worker.join();
    return;
  }
  constexpr int kWindowWidth = 704;
  constexpr int kWindowHeight = 344;
  SDL_Window *window = SDL_CreateWindow(
      "SekaiLink Bootloader",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      kWindowWidth,
      kWindowHeight,
      SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
  if (window) SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  applyRoundedWindowShape(window, kWindowWidth, kWindowHeight, 28);
  SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
  bool running = window && renderer;
  SDL_Rect copyButton{520, 292, 150, 38};
  std::vector<std::thread> reportWorkers;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_MOUSEBUTTONUP) {
        int x = event.button.x;
        int y = event.button.y;
        if (x >= copyButton.x && x <= copyButton.x + copyButton.w && y >= copyButton.y && y <= copyButton.y + copyButton.h) {
          std::string error;
          std::string reportUrl;
          std::string channel;
          std::string build;
          fs::path reportLogPath;
          fs::path installDir;
          bool canReport = false;
          {
            std::lock_guard lock(state.mutex);
            error = state.error;
            if (!state.error.empty()) SDL_SetClipboardText(state.error.c_str());
            canReport = !state.error.empty() && !state.reportInFlight;
            if (canReport) {
              state.reportInFlight = true;
              state.reportStatus = "Sending...";
              reportUrl = state.reportUrl;
              channel = state.reportChannel;
              build = state.reportBuild;
              reportLogPath = state.reportLogPath;
              installDir = state.reportInstallDir;
            }
          }
          if (canReport) {
            reportWorkers.emplace_back([&state, error, reportUrl, reportLogPath, channel, build, installDir]() {
              try {
                submitBootloaderBugReport(reportUrl, error, reportLogPath, channel, build, installDir);
                std::lock_guard lock(state.mutex);
                state.reportInFlight = false;
                state.reportStatus = "Reported";
              } catch (const std::exception &err) {
                std::lock_guard lock(state.mutex);
                state.reportInFlight = false;
                state.reportStatus = std::string("Report failed: ") + err.what();
              } catch (...) {
                std::lock_guard lock(state.mutex);
                state.reportInFlight = false;
                state.reportStatus = "Report failed";
              }
            });
          }
        }
      }
    }
    std::string phase, detail, error, reportStatus;
    double progress = 0.0;
    bool done = false;
    bool failed = false;
    {
      std::lock_guard lock(state.mutex);
      phase = state.phase;
      detail = state.detail;
      error = state.error;
      reportStatus = state.reportStatus;
      progress = state.progress;
      done = state.done;
      failed = state.failed;
    }
    SDL_SetWindowTitle(window, ("SekaiLink Bootloader - " + phase).c_str());
    const Uint32 ticks = SDL_GetTicks();
    SDL_SetRenderDrawColor(renderer, 18, 24, 34, 255);
    SDL_RenderClear(renderer);

    SDL_Color fg{242, 248, 248, 255};
    SDL_Color dim{165, 179, 188, 255};
    SDL_Color muted{111, 126, 138, 255};
    SDL_Color accent{83, 225, 211, 255};
    SDL_Color purple{169, 119, 255, 255};
    SDL_Color warn{255, 112, 96, 255};
    SDL_Color panel{18, 24, 34, 238};
    SDL_Color panelBorder{64, 77, 93, 255};
    SDL_Color trackColor{35, 43, 56, 255};

    fillRoundedRect(renderer, SDL_Rect{0, 0, kWindowWidth, kWindowHeight}, 18, panel);
    strokeRoundedRect(renderer, SDL_Rect{0, 0, kWindowWidth, kWindowHeight}, 18, panelBorder);

    fillRoundedRect(renderer, SDL_Rect{26, 28, 46, 46}, 12, SDL_Color{28, 49, 59, 255});
    fillRoundedRect(renderer, SDL_Rect{38, 40, 22, 22}, 6, failed ? warn : accent);
    strokeRoundedRect(renderer, SDL_Rect{36, 38, 26, 26}, 7, purple);

    drawText(renderer, "SekaiLink", 90, 26, fg, 3, 28);
    drawText(renderer, "Bootloader", 92, 58, dim, 1, 40);

    const std::string mainLine = friendlyPhase(phase, failed);
    const std::string subLine = friendlyDetail(phase, detail, failed);
    drawText(renderer, mainLine, 28, 112, failed ? warn : fg, 2, 48);
    drawText(renderer, subLine, 28, 146, dim, 1, 92);

    SDL_Rect track{28, 202, 648, 24};
    fillRoundedRect(renderer, track, 12, trackColor);
    SDL_Rect fill = track;
    fill.w = std::max(12, static_cast<int>(track.w * std::clamp(progress, 0.0, 1.0)));
    fillRoundedRect(renderer, fill, 12, failed ? warn : accent);
    strokeRoundedRect(renderer, track, 12, SDL_Color{77, 91, 108, 255});

    const int percent = static_cast<int>(std::round(std::clamp(progress, 0.0, 1.0) * 100.0));
    drawText(renderer, std::to_string(percent) + "%", 632, 234, failed ? warn : accent, 1, 10);
    if (!failed) {
      drawText(renderer, "Merci de garder cette fenetre ouverte pendant la mise a jour.", 28, 264, muted, 1, 90);
    }

    if (!error.empty()) {
      fillRoundedRect(renderer, SDL_Rect{20, 282, 478, 56}, 10, SDL_Color{45, 25, 29, 255});
      strokeRoundedRect(renderer, SDL_Rect{20, 282, 478, 56}, 10, SDL_Color{125, 58, 62, 255});
      drawText(renderer, error, 34, 297, warn, 1, 68);
      fillRoundedRect(renderer, copyButton, 8, SDL_Color{37, 45, 56, 255});
      strokeRoundedRect(renderer, copyButton, 8, accent);
      const std::string buttonText = reportStatus == "Sending..." ? "Sending..." : reportStatus == "Reported" ? "Reported" : "Report Bug";
      drawText(renderer, buttonText, copyButton.x + 28, copyButton.y + 10, fg, 1, 20);
      if (!reportStatus.empty() && reportStatus != "Sending..." && reportStatus != "Reported") {
        drawText(renderer, reportStatus, 34, 322, muted, 1, 60);
      }
    }
    SDL_RenderPresent(renderer);
    if (done && !failed) {
      SDL_Delay(900);
      running = false;
    }
  }
  if (worker.joinable()) worker.join();
  for (auto &reportWorker : reportWorkers) {
    if (reportWorker.joinable()) reportWorker.join();
  }
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_Quit();
}

int realMain(int argc, char **argv) {
  Options options = parseOptions(argc, argv);
  Logger log;
  options.channel = normalizeReleaseChannel(options.channel);
  log.line("boot", "SekaiLink native bootloader platform=" + platformId() + " channel=" + options.channel);

  if (!options.zipTest.empty()) {
    const fs::path out = options.zipTestOut.empty() ? (workDir() / "zip-test") : options.zipTestOut;
    extractZip(options.zipTest, out, nullptr, log);
    validateBundle(resolveBundleRoot(out), options.zipTestPlatform.empty() ? platformId() : options.zipTestPlatform);
    log.line("zip-test", "ok out=" + out.u8string());
    return 0;
  }

  UiState ui;
  ui.reportUrl = normalizedApiBase(options.apiBase) + "/api/client/bug-report";
  ui.reportLogPath = log.path();
  ui.reportChannel = options.channel;
  ui.reportBuild = options.build;
  ui.reportInstallDir = options.installDir.empty() ? defaultInstallDir() : options.installDir;
  if (!options.ui) {
    runUpdate(options, nullptr, log);
    return 0;
  }

  std::thread worker([&]() {
    try {
      runUpdate(options, &ui, log);
    } catch (const std::exception &err) {
      log.line("error", err.what());
      failUi(&ui, err.what());
    } catch (...) {
      log.line("error", "unknown_error");
      failUi(&ui, "unknown_error");
    }
  });
  runUi(ui, worker, log.path());
  return ui.failed ? 1 : 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    return realMain(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << "SekaiLink Bootloader Error: " << err.what() << std::endl;
    return 1;
  }
}
