#include "sekailink/snes_runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <string>

extern "C" {
#include <target-bsnescore/bsnescore.hpp>
#include <target-bsnescore/callbacks.h>

void snes_set_callbacks(SnesCallbacks* callbacks);
void snes_init(SnesInitData* init_data);
void snes_power(void);
void snes_term(void);
void snes_reset(void);
bool snes_run(bool breakOnLatch);
void snes_load_cartridge_normal(const std::uint8_t* rom_data, int rom_size);
int snes_serialized_size();
void snes_serialize(std::uint8_t* data, int size);
void snes_unserialize(const std::uint8_t* data, int size);
short* snes_get_audiobuffer_and_size(int& out_size);
void* snes_get_memory_region(int id, int* size, int* word_size);
std::uint8_t snes_bus_read(unsigned addr);
void snes_bus_write(unsigned addr, std::uint8_t value);
std::uint8_t snes_read_oam(std::uint16_t addr);
void snes_write_oam(std::uint16_t addr, std::uint8_t value);
}

namespace sekailink {

namespace {

constexpr unsigned kSampleRate = 44100U;
constexpr unsigned kChannels = 2U;
constexpr std::uint32_t kSnesSystemBusSize = 0x1000000U;
constexpr std::uint32_t kLogicalSramBase = 0x2000U;
constexpr std::uint32_t kLogicalSramSize = 0x100000U;
constexpr unsigned kLegacyKeyCount = 12U;

constexpr unsigned kLegacyJoypadB = 0U;
constexpr unsigned kLegacyJoypadY = 1U;
constexpr unsigned kLegacyJoypadSelect = 2U;
constexpr unsigned kLegacyJoypadStart = 3U;
constexpr unsigned kLegacyJoypadUp = 4U;
constexpr unsigned kLegacyJoypadDown = 5U;
constexpr unsigned kLegacyJoypadLeft = 6U;
constexpr unsigned kLegacyJoypadRight = 7U;
constexpr unsigned kLegacyJoypadA = 8U;
constexpr unsigned kLegacyJoypadX = 9U;
constexpr unsigned kLegacyJoypadL = 10U;
constexpr unsigned kLegacyJoypadR = 11U;

constexpr unsigned kBsnesInputUp = 0U;
constexpr unsigned kBsnesInputDown = 1U;
constexpr unsigned kBsnesInputLeft = 2U;
constexpr unsigned kBsnesInputRight = 3U;
constexpr unsigned kBsnesInputB = 4U;
constexpr unsigned kBsnesInputA = 5U;
constexpr unsigned kBsnesInputY = 6U;
constexpr unsigned kBsnesInputX = 7U;
constexpr unsigned kBsnesInputL = 8U;
constexpr unsigned kBsnesInputR = 9U;
constexpr unsigned kBsnesInputSelect = 10U;
constexpr unsigned kBsnesInputStart = 11U;

SnesRuntime* g_active_runtime = nullptr;

std::vector<std::uint8_t> read_binary_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("file_open_failed:" + path);
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
}

std::vector<std::uint8_t> read_optional_binary_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
}

void strip_smc_header(std::vector<std::uint8_t>& rom) {
    if ((rom.size() & 0x7fffU) == 512U) {
        rom.erase(rom.begin(), rom.begin() + 512);
    }
}

std::string upper_hex(const std::uint32_t value) {
    return std::format("{:08X}", value);
}

std::uint8_t expand_5_to_8(const std::uint16_t value) {
    return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
}

std::uint32_t decode_bsnes_color(const std::uint16_t color) {
    const auto b = expand_5_to_8(color & 0x1FU);
    const auto g = expand_5_to_8((color >> 5U) & 0x1FU);
    const auto r = expand_5_to_8((color >> 10U) & 0x1FU);
    return 0xFF000000U |
        (static_cast<std::uint32_t>(r) << 16U) |
        (static_cast<std::uint32_t>(g) << 8U) |
        static_cast<std::uint32_t>(b);
}

}  // namespace

char* snes_path_request_cb(const int, const char* hint, const bool) {
    static thread_local std::string path;
    if (g_active_runtime == nullptr) {
        return nullptr;
    }

    const std::string requested = hint ? hint : "";
    if (requested == "save.ram" && g_active_runtime->save_path_.has_value()) {
        path = *g_active_runtime->save_path_;
        return path.data();
    }

    if (requested == "save.ram") {
        auto save_path = std::filesystem::path(g_active_runtime->rom_path_);
        save_path.replace_extension(".srm");
        path = save_path.string();
        return path.data();
    }

    path = (std::filesystem::path(g_active_runtime->rom_path_).parent_path() / requested).string();
    return path.empty() ? nullptr : path.data();
}

void snes_video_frame_cb(const std::uint16_t* data, int width, int height, int pitch) {
    if (g_active_runtime == nullptr || data == nullptr || width <= 0 || height <= 0 || pitch <= 0) {
        return;
    }

    const auto source_pitch = static_cast<std::size_t>(pitch);
    std::vector<std::uint32_t> packed(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0xFF000000U);
    for (int y = 0; y < height; ++y) {
        const auto* row = data + static_cast<std::size_t>(y) * source_pitch;
        for (int x = 0; x < width; ++x) {
            packed[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                decode_bsnes_color(row[x]);
        }
    }

    g_active_runtime->video_width_ = static_cast<unsigned>(width);
    g_active_runtime->video_height_ = static_cast<unsigned>(height);
    g_active_runtime->video_buffer_ = std::move(packed);
}

std::int16_t snes_input_poll_cb(const int port, const int, const int id) {
    if (g_active_runtime == nullptr || port != 0 || id < 0 || static_cast<unsigned>(id) >= kLegacyKeyCount) {
        return 0;
    }

    return (g_active_runtime->keys_ & (1U << static_cast<unsigned>(id))) != 0 ? 1 : 0;
}

void snes_controller_latch_cb() {
}

void snes_no_lag_cb(bool) {
}

void snes_trace_cb(const char*, const char*) {
}

void snes_read_hook_cb(std::uint32_t, std::uint8_t&) {
}

void snes_write_hook_cb(std::uint32_t, std::uint8_t&) {
}

void snes_exec_hook_cb(std::uint32_t) {
}

std::int64_t snes_time_cb() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void snes_msu_open_cb(std::uint16_t) {
}

void snes_msu_seek_cb(long, bool) {
}

std::uint8_t snes_msu_read_cb() {
    return 0;
}

bool snes_msu_end_cb() {
    return true;
}

SnesRuntime::SnesRuntime() {
    owner_thread_ = std::thread(&SnesRuntime::owner_loop, this);
}

SnesRuntime::~SnesRuntime() {
    stop();
}

void SnesRuntime::load_rom(const std::string& rom_path) {
    rom_path_ = rom_path;
    rom_data_ = read_binary_file(rom_path);
    strip_smc_header(rom_data_);
    dispatch([this]() {
        destroy_core();
        g_active_runtime = this;

        SnesCallbacks callbacks{};
        callbacks.snes_video_frame = &snes_video_frame_cb;
        callbacks.snes_input_poll = &snes_input_poll_cb;
        callbacks.snes_controller_latch = &snes_controller_latch_cb;
        callbacks.snes_no_lag = &snes_no_lag_cb;
        callbacks.snes_path_request = &snes_path_request_cb;
        callbacks.snes_trace = &snes_trace_cb;
        callbacks.snes_read_hook = &snes_read_hook_cb;
        callbacks.snes_write_hook = &snes_write_hook_cb;
        callbacks.snes_exec_hook = &snes_exec_hook_cb;
        callbacks.snes_time = &snes_time_cb;
        callbacks.snes_msu_open = &snes_msu_open_cb;
        callbacks.snes_msu_seek = &snes_msu_seek_cb;
        callbacks.snes_msu_read = &snes_msu_read_cb;
        callbacks.snes_msu_end = &snes_msu_end_cb;
        snes_set_callbacks(&callbacks);

        SnesInitData init_data{};
        init_data.entropy = 0;
        init_data.left_port = 1;
        init_data.right_port = 0;
        init_data.hotfixes = true;
        init_data.fast_ppu = true;
        init_data.fast_dsp = true;
        init_data.fast_coprocessors = true;
        init_data.region_override = 0;
        snes_init(&init_data);
        snes_load_cartridge_normal(rom_data_.data(), static_cast<int>(rom_data_.size()));
        snes_power();
        load_sram_if_present();
        rebuild_domains();
        audio_samples_.clear();
        frame_count_ = 0;
        keys_ = 0;
        video_buffer_.clear();
        video_width_ = 256;
        video_height_ = 224;
    });
}

void SnesRuntime::set_save_path(std::optional<std::string> save_path) {
    save_path_ = std::move(save_path);
}

void SnesRuntime::start() {
    dispatch([this]() {
        if (domains_.empty()) {
            throw std::runtime_error("runtime_not_loaded");
        }
    });
}

void SnesRuntime::stop() {
    if (!owner_thread_.joinable()) {
        return;
    }
    dispatch([this]() {
        flush_save();
        destroy_core();
    });
    {
        std::scoped_lock lock(queue_mutex_);
        owner_stop_requested_.store(true);
    }
    queue_cv_.notify_one();
    owner_thread_.join();
}

void SnesRuntime::flush_save() {
    dispatch([this]() {
        if (!save_path_.has_value() || save_path_->empty()) {
            return;
        }
        int size = 0;
        int word_size = 0;
        auto* memory = static_cast<std::uint8_t*>(snes_get_memory_region(CARTRIDGE_RAM, &size, &word_size));
        if (memory == nullptr || size <= 0) {
            return;
        }
        std::filesystem::create_directories(std::filesystem::path(*save_path_).parent_path());
        std::ofstream output(*save_path_, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(memory), static_cast<std::streamsize>(size));
    });
}

std::string SnesRuntime::system() const {
    return "SNES";
}

std::string SnesRuntime::rom_hash() const {
    return const_cast<SnesRuntime*>(this)->dispatch([this]() { return compute_crc32_hash(); });
}

std::uint64_t SnesRuntime::frame_count() const {
    return const_cast<SnesRuntime*>(this)->dispatch([this]() { return frame_count_; });
}

std::size_t SnesRuntime::memory_size(const std::string& domain) const {
    return const_cast<SnesRuntime*>(this)->dispatch([this, domain]() { return require_domain(domain).size; });
}

std::vector<SnesDomainInfo> SnesRuntime::memory_domains() const {
    return const_cast<SnesRuntime*>(this)->dispatch([this]() {
        std::vector<SnesDomainInfo> out;
        out.reserve(domains_.size());
        for (const auto& [_, info] : domains_) {
            out.push_back(info);
        }
        return out;
    });
}

std::optional<std::string> SnesRuntime::save_path() const {
    return save_path_;
}

std::vector<std::uint32_t> SnesRuntime::video_frame() const {
    return const_cast<SnesRuntime*>(this)->dispatch([this]() { return video_buffer_; });
}

unsigned SnesRuntime::video_width() const {
    return const_cast<SnesRuntime*>(this)->dispatch([this]() { return video_width_; });
}

unsigned SnesRuntime::video_height() const {
    return const_cast<SnesRuntime*>(this)->dispatch([this]() { return video_height_; });
}

void SnesRuntime::run_frame() {
    dispatch([this]() {
        if (domains_.empty()) {
            throw std::runtime_error("runtime_not_loaded");
        }
        snes_run(false);
        int sample_count = 0;
        if (auto* audio = snes_get_audiobuffer_and_size(sample_count); audio != nullptr && sample_count > 0) {
            audio_samples_.insert(audio_samples_.end(), audio, audio + sample_count);
        }
        ++frame_count_;
    });
}

void SnesRuntime::set_keys(std::uint32_t keys) {
    dispatch([this, keys]() { keys_ = keys; });
}

void SnesRuntime::reset() {
    dispatch([this]() {
        snes_reset();
        keys_ = 0;
    });
}

std::size_t SnesRuntime::state_size() const {
    return const_cast<SnesRuntime*>(this)->dispatch([]() {
        const auto size = snes_serialized_size();
        if (size < 0) {
            throw std::runtime_error("snes_state_size_failed");
        }
        return static_cast<std::size_t>(size);
    });
}

std::vector<std::uint8_t> SnesRuntime::save_state() const {
    return const_cast<SnesRuntime*>(this)->dispatch([]() {
        const auto size = snes_serialized_size();
        if (size <= 0) {
            throw std::runtime_error("snes_state_size_failed");
        }
        std::vector<std::uint8_t> state(static_cast<std::size_t>(size));
        snes_serialize(state.data(), size);
        return state;
    });
}

void SnesRuntime::load_state(const std::vector<std::uint8_t>& state) {
    dispatch([this, state]() {
        if (state.empty()) {
            throw std::runtime_error("snes_state_empty");
        }
        snes_unserialize(state.data(), static_cast<int>(state.size()));
        rebuild_domains();
    });
}

unsigned SnesRuntime::audio_sample_rate() const {
    return kSampleRate;
}

unsigned SnesRuntime::audio_channels() const {
    return kChannels;
}

std::vector<std::int16_t> SnesRuntime::read_audio_samples(const std::size_t max_samples) {
    return dispatch([this, max_samples]() {
        if (audio_samples_.empty() || max_samples == 0) {
            return std::vector<std::int16_t>{};
        }
        const auto count = std::min(max_samples, audio_samples_.size());
        std::vector<std::int16_t> out(
            audio_samples_.begin(),
            audio_samples_.begin() + static_cast<std::ptrdiff_t>(count));
        audio_samples_.erase(
            audio_samples_.begin(),
            audio_samples_.begin() + static_cast<std::ptrdiff_t>(count));
        return out;
    });
}

SnesFrameSnapshot SnesRuntime::consume_frame_snapshot(const std::size_t max_audio_samples) {
    return dispatch([this, max_audio_samples]() {
        SnesFrameSnapshot out;
        out.width = video_width_;
        out.height = video_height_;
        out.video = std::move(video_buffer_);
        video_buffer_.clear();
        if (!audio_samples_.empty() && max_audio_samples > 0) {
            const auto count = std::min(max_audio_samples, audio_samples_.size());
            out.audio.assign(
                audio_samples_.begin(),
                audio_samples_.begin() + static_cast<std::ptrdiff_t>(count));
            audio_samples_.erase(
                audio_samples_.begin(),
                audio_samples_.begin() + static_cast<std::ptrdiff_t>(count));
        }
        return out;
    });
}

SnesFrameSnapshot SnesRuntime::run_frame_and_consume_snapshot(const std::size_t max_audio_samples) {
    return dispatch([this, max_audio_samples]() {
        if (domains_.empty()) {
            throw std::runtime_error("runtime_not_loaded");
        }
        snes_run(false);
        int sample_count = 0;
        if (auto* audio = snes_get_audiobuffer_and_size(sample_count); audio != nullptr && sample_count > 0) {
            audio_samples_.insert(audio_samples_.end(), audio, audio + sample_count);
        }
        ++frame_count_;

        SnesFrameSnapshot out;
        out.width = video_width_;
        out.height = video_height_;
        out.video = std::move(video_buffer_);
        video_buffer_.clear();
        if (!audio_samples_.empty() && max_audio_samples > 0) {
            const auto count = std::min(max_audio_samples, audio_samples_.size());
            out.audio.assign(
                audio_samples_.begin(),
                audio_samples_.begin() + static_cast<std::ptrdiff_t>(count));
            audio_samples_.erase(
                audio_samples_.begin(),
                audio_samples_.begin() + static_cast<std::ptrdiff_t>(count));
        }
        return out;
    });
}

std::vector<std::uint8_t> SnesRuntime::read_memory(
    const std::uint32_t address,
    const std::size_t size,
    const std::string& domain) {
    return dispatch([this, address, size, domain]() {
        if (domain == "System Bus") {
            std::vector<std::uint8_t> out(size, 0);
            for (std::size_t i = 0; i < size; ++i) {
                out[i] = snes_bus_read(address + static_cast<std::uint32_t>(i));
            }
            return out;
        }
        if (domain == "OAM") {
            std::vector<std::uint8_t> out(size, 0);
            for (std::size_t i = 0; i < size; ++i) {
                out[i] = snes_read_oam(static_cast<std::uint16_t>(address + static_cast<std::uint32_t>(i)));
            }
            return out;
        }
        const auto& info = require_domain(domain);
        if (domain == "SRAM" || domain == "CartRAM") {
            if (address < kLogicalSramBase) {
                throw std::out_of_range("memory_read_out_of_range");
            }
            const auto physical = static_cast<std::size_t>(address - kLogicalSramBase);
            if (physical + size > info.size) {
                throw std::out_of_range("memory_read_out_of_range");
            }
            return std::vector<std::uint8_t>(
                info.pointer + physical,
                info.pointer + physical + size);
        }
        if (address + size > info.size) {
            throw std::out_of_range("memory_read_out_of_range");
        }
        return std::vector<std::uint8_t>(
            info.pointer + address,
            info.pointer + address + size);
    });
}

void SnesRuntime::write_memory(
    const std::uint32_t address,
    const std::vector<std::uint8_t>& value,
    const std::string& domain) {
    dispatch([this, address, value, domain]() {
        if (domain == "System Bus") {
            for (std::size_t i = 0; i < value.size(); ++i) {
                snes_bus_write(address + static_cast<std::uint32_t>(i), value[i]);
            }
            return;
        }
        if (domain == "OAM") {
            for (std::size_t i = 0; i < value.size(); ++i) {
                snes_write_oam(static_cast<std::uint16_t>(address + static_cast<std::uint32_t>(i)), value[i]);
            }
            return;
        }
        auto& info = require_domain(domain);
        if (domain == "SRAM" || domain == "CartRAM") {
            if (address < kLogicalSramBase) {
                throw std::out_of_range("memory_write_out_of_range");
            }
            const auto physical = static_cast<std::size_t>(address - kLogicalSramBase);
            if (physical + value.size() > info.size) {
                throw std::out_of_range("memory_write_out_of_range");
            }
            std::memcpy(info.pointer + physical, value.data(), value.size());
            return;
        }
        if (!info.writable) {
            throw std::runtime_error("memory_domain_not_writable");
        }
        if (address + value.size() > info.size) {
            throw std::out_of_range("memory_write_out_of_range");
        }
        std::memcpy(info.pointer + address, value.data(), value.size());
    });
}

void SnesRuntime::owner_loop() {
    owner_thread_id_ = std::this_thread::get_id();
    while (!owner_stop_requested_.load()) {
        std::function<void()> task;
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return owner_stop_requested_.load() || !queue_.empty();
            });
            if (owner_stop_requested_.load() && queue_.empty()) {
                break;
            }
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        if (task) {
            task();
        }
    }
}

void SnesRuntime::destroy_core() {
    domains_.clear();
    video_buffer_.clear();
    audio_samples_.clear();
    if (g_active_runtime == this) {
        snes_term();
        g_active_runtime = nullptr;
    }
}

void SnesRuntime::rebuild_domains() {
    domains_.clear();
    const auto add_domain = [&](const std::string& name, const int id, const bool writable) {
        int size = 0;
        int word_size = 0;
        auto* data = static_cast<std::uint8_t*>(snes_get_memory_region(id, &size, &word_size));
        if (data == nullptr || size <= 0) {
            return;
        }
        domains_.emplace(name, SnesDomainInfo{name, static_cast<std::size_t>(size), data, writable});
    };

    add_domain("SRAM", CARTRIDGE_RAM, true);
    add_domain("CartRAM", CARTRIDGE_RAM, true);
    add_domain("WRAM", WRAM, true);
    add_domain("APURAM", APURAM, true);
    add_domain("VRAM", VRAM, true);
    add_domain("CGRAM", CGRAM, true);
    add_domain("ROM", CARTRIDGE_ROM, false);
    domains_.emplace("OAM", SnesDomainInfo{"OAM", (128U * 32U + 128U * 2U) / 8U, nullptr, true});
    domains_.emplace("System Bus", SnesDomainInfo{"System Bus", kSnesSystemBusSize, nullptr, true});
    if (domains_.contains("SRAM")) {
        domains_["SRAM"].size = kLogicalSramSize;
        domains_["CartRAM"].size = kLogicalSramSize;
    }
}

const SnesDomainInfo& SnesRuntime::require_domain(const std::string& domain) const {
    const auto it = domains_.find(domain);
    if (it == domains_.end()) {
        throw std::runtime_error("memory_domain_not_found");
    }
    return it->second;
}

SnesDomainInfo& SnesRuntime::require_domain(const std::string& domain) {
    const auto it = domains_.find(domain);
    if (it == domains_.end()) {
        throw std::runtime_error("memory_domain_not_found");
    }
    return it->second;
}

std::string SnesRuntime::compute_crc32_hash() const {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : rom_data_) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return upper_hex(crc ^ 0xFFFFFFFFU);
}

void SnesRuntime::load_sram_if_present() {
    if (!save_path_.has_value() || save_path_->empty()) {
        return;
    }
    int size = 0;
    int word_size = 0;
    auto* memory = static_cast<std::uint8_t*>(snes_get_memory_region(CARTRIDGE_RAM, &size, &word_size));
    if (memory == nullptr || size <= 0) {
        return;
    }
    const auto save = read_optional_binary_file(*save_path_);
    if (save.empty()) {
        return;
    }
    std::memcpy(memory, save.data(), std::min<std::size_t>(save.size(), static_cast<std::size_t>(size)));
}

}  // namespace sekailink
