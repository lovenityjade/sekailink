#include "sekailink/nes_runtime.hpp"

#include <emu.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <thread>

namespace quickerNES {
class Emu;
}

extern "C" {
quickerNES::Emu* qn_new();
void qn_delete(quickerNES::Emu* e);
const char* qn_loadines(quickerNES::Emu* e, const std::uint8_t* data, int length);
const char* qn_set_sample_rate(quickerNES::Emu* e, int rate);
const char* qn_emulate_frame(quickerNES::Emu* e, std::uint32_t pad1, std::uint32_t pad2, std::uint8_t arkanoidPosition, std::uint8_t arkanoidFire, int controllerType);
void qn_blit(quickerNES::Emu* e, std::int32_t* dest, const std::int32_t* colors, int cropleft, int croptop, int cropright, int cropbottom);
const quickerNES::Emu::rgb_t* qn_get_default_colors();
void qn_get_audio_info(quickerNES::Emu* e, int* sample_count, int* chan_count);
int qn_read_audio(quickerNES::Emu* e, short* dest, int max_samples);
void qn_reset(quickerNES::Emu* e, int hard);
const char* qn_state_size(quickerNES::Emu* e, int* size);
const char* qn_state_save(quickerNES::Emu* e, void* dest, int size);
const char* qn_state_load(quickerNES::Emu* e, const void* src, int size);
int qn_has_battery_ram(quickerNES::Emu* e);
const char* qn_battery_ram_size(quickerNES::Emu* e, int* size);
const char* qn_battery_ram_save(quickerNES::Emu* e, void* dest, int size);
const char* qn_battery_ram_load(quickerNES::Emu* e, const void* src, int size);
const char* qn_battery_ram_clear(quickerNES::Emu* e);
int qn_get_memory_area(quickerNES::Emu* e, int which, const void** data, int* size, int* writable, const char** name);
std::uint8_t qn_peek_prgbus(quickerNES::Emu* e, int addr);
void qn_poke_prgbus(quickerNES::Emu* e, int addr, unsigned char val);
}

namespace sekailink {

namespace {

constexpr int kQuickNesJoypadController = 1;

std::string to_upper_hex(const std::uint32_t value) {
    return std::format("{:08X}", value);
}

std::uint32_t crc32_bytes(const std::vector<std::uint8_t>& data) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

std::uint32_t to_quickness_pad(const std::uint32_t keys) {
    return keys;
}

}  // namespace

NesRuntime::NesRuntime() {
    owner_thread_ = std::thread(&NesRuntime::owner_loop, this);
}

NesRuntime::~NesRuntime() {
    stop();
}

void NesRuntime::load_rom(const std::string& rom_path) {
    rom_path_ = rom_path;
    dispatch([this]() {
        destroy_core();
        rom_data_.clear();
        domains_.clear();
        frame_count_ = 0;

        std::ifstream input(rom_path_, std::ios::binary);
        if (!input) {
            throw std::runtime_error("nes_rom_open_failed");
        }
        rom_data_ = std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (rom_data_.empty()) {
            throw std::runtime_error("nes_rom_empty");
        }

        core_ = qn_new();
        if (core_ == nullptr) {
            throw std::runtime_error("nes_core_create_failed");
        }
        if (const char* err = qn_loadines(core_, rom_data_.data(), static_cast<int>(rom_data_.size())); err != nullptr) {
            destroy_core();
            throw std::runtime_error(err);
        }
        if (const char* err = qn_set_sample_rate(core_, 44100); err != nullptr) {
            destroy_core();
            throw std::runtime_error(err);
        }
        audio_sample_rate_ = 44100U;
        qn_reset(core_, 1);
        int audio_sample_count = 0;
        int audio_channel_count = 0;
        qn_get_audio_info(core_, &audio_sample_count, &audio_channel_count);
        audio_channels_ = audio_channel_count > 0 ? static_cast<unsigned>(audio_channel_count) : 1U;

        if (save_path_.has_value()) {
            int save_size = 0;
            if (qn_has_battery_ram(core_) && qn_battery_ram_size(core_, &save_size) == nullptr && save_size > 0) {
                if (std::filesystem::exists(*save_path_)) {
                    std::ifstream save_input(*save_path_, std::ios::binary);
                    std::vector<std::uint8_t> save_bytes((std::istreambuf_iterator<char>(save_input)), std::istreambuf_iterator<char>());
                    if (static_cast<int>(save_bytes.size()) == save_size) {
                        qn_battery_ram_load(core_, save_bytes.data(), save_size);
                    }
                } else {
                    qn_battery_ram_clear(core_);
                }
            }
        }
        rebuild_domains();
    });
}

void NesRuntime::set_save_path(std::optional<std::string> save_path) {
    save_path_ = std::move(save_path);
}

void NesRuntime::start() {
    dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
    });
}

void NesRuntime::stop() {
    if (owner_thread_.joinable()) {
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
}

void NesRuntime::flush_save() {
    dispatch([this]() {
        if (core_ == nullptr || !save_path_.has_value() || !qn_has_battery_ram(core_)) {
            return;
        }
        int save_size = 0;
        if (qn_battery_ram_size(core_, &save_size) != nullptr || save_size <= 0) {
            return;
        }
        std::vector<std::uint8_t> save(static_cast<std::size_t>(save_size));
        if (const char* err = qn_battery_ram_save(core_, save.data(), save_size); err != nullptr) {
            throw std::runtime_error(err);
        }
        std::filesystem::create_directories(std::filesystem::path(*save_path_).parent_path());
        std::ofstream output(*save_path_, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("nes_save_open_failed");
        }
        output.write(reinterpret_cast<const char*>(save.data()), static_cast<std::streamsize>(save.size()));
    });
}

std::string NesRuntime::system() const {
    return "NES";
}

std::string NesRuntime::rom_hash() const {
    return const_cast<NesRuntime*>(this)->dispatch([this]() {
        if (rom_data_.empty()) {
            throw std::runtime_error("runtime_not_loaded");
        }
        return compute_crc32_hash();
    });
}

std::uint64_t NesRuntime::frame_count() const {
    return const_cast<NesRuntime*>(this)->dispatch([this]() {
        return frame_count_;
    });
}

std::size_t NesRuntime::memory_size(const std::string& domain) const {
    return const_cast<NesRuntime*>(this)->dispatch([this, domain]() {
        return require_domain(domain).size;
    });
}

std::optional<std::string> NesRuntime::save_path() const {
    return save_path_;
}

std::vector<std::uint32_t> NesRuntime::video_frame() const {
    return const_cast<NesRuntime*>(this)->dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }

        constexpr int kWidth = 256;
        constexpr int kHeight = 240;
        std::vector<std::int32_t> colors(quickerNES::Emu::color_table_size, 0);
        if (const auto* palette = qn_get_default_colors(); palette != nullptr) {
            for (int index = 0; index < quickerNES::Emu::color_table_size; ++index) {
                const auto& rgb = palette[index];
                colors[static_cast<std::size_t>(index)] =
                    static_cast<std::int32_t>(0xFF000000U |
                                              (static_cast<std::uint32_t>(rgb.red) << 16U) |
                                              (static_cast<std::uint32_t>(rgb.green) << 8U) |
                                              static_cast<std::uint32_t>(rgb.blue));
            }
        }

        std::vector<std::int32_t> frame(static_cast<std::size_t>(kWidth) * kHeight, 0);
        qn_blit(core_, frame.data(), colors.data(), 0, 0, 0, 0);

        std::vector<std::uint32_t> converted(frame.size(), 0);
        for (std::size_t index = 0; index < frame.size(); ++index) {
            converted[index] = static_cast<std::uint32_t>(frame[index]);
        }
        return converted;
    });
}

void NesRuntime::run_frame() {
    dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        if (const char* err = qn_emulate_frame(core_, to_quickness_pad(keys_), 0U, 0U, 0U, kQuickNesJoypadController); err != nullptr) {
            throw std::runtime_error(err);
        }
        ++frame_count_;
        rebuild_domains();
    });
}

void NesRuntime::set_keys(const std::uint32_t keys) {
    keys_ = keys;
}

void NesRuntime::reset(const bool hard) {
    dispatch([this, hard]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        qn_reset(core_, hard ? 1 : 0);
        frame_count_ = 0;
        rebuild_domains();
    });
}

std::size_t NesRuntime::state_size() const {
    return const_cast<NesRuntime*>(this)->dispatch([this]() -> std::size_t {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        int size = 0;
        if (const char* err = qn_state_size(core_, &size); err != nullptr) {
            throw std::runtime_error(err);
        }
        return size > 0 ? static_cast<std::size_t>(size) : 0U;
    });
}

std::vector<std::uint8_t> NesRuntime::save_state() const {
    return const_cast<NesRuntime*>(this)->dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        int size = 0;
        if (const char* err = qn_state_size(core_, &size); err != nullptr) {
            throw std::runtime_error(err);
        }
        if (size <= 0) {
            throw std::runtime_error("state_size_zero");
        }
        std::vector<std::uint8_t> state(static_cast<std::size_t>(size));
        if (const char* err = qn_state_save(core_, state.data(), size); err != nullptr) {
            throw std::runtime_error(err);
        }
        return state;
    });
}

void NesRuntime::load_state(const std::vector<std::uint8_t>& state) {
    dispatch([this, &state]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        if (state.empty()) {
            throw std::runtime_error("state_empty");
        }
        if (const char* err = qn_state_load(core_, state.data(), static_cast<int>(state.size())); err != nullptr) {
            throw std::runtime_error(err);
        }
        rebuild_domains();
    });
}

unsigned NesRuntime::audio_sample_rate() const {
    return audio_sample_rate_;
}

unsigned NesRuntime::audio_channels() const {
    return audio_channels_;
}

std::vector<std::int16_t> NesRuntime::read_audio_samples(const std::size_t max_samples) {
    return dispatch([this, max_samples]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        if (max_samples == 0) {
            return std::vector<std::int16_t>{};
        }
        int available_samples = 0;
        int channels = 0;
        qn_get_audio_info(core_, &available_samples, &channels);
        if (available_samples <= 0 || channels <= 0) {
            return std::vector<std::int16_t>{};
        }
        const std::size_t sample_limit = std::min<std::size_t>(
            static_cast<std::size_t>(available_samples) * static_cast<std::size_t>(channels),
            max_samples);
        std::vector<std::int16_t> samples(sample_limit);
        const int read = qn_read_audio(core_, samples.data(), static_cast<int>(sample_limit));
        if (read < 0) {
            throw std::runtime_error("audio_read_failed");
        }
        samples.resize(static_cast<std::size_t>(read));
        return samples;
    });
}

std::vector<std::uint8_t> NesRuntime::read_memory(
    const std::uint32_t address,
    const std::size_t size,
    const std::string& domain) {
    return dispatch([this, address, size, domain]() {
        if (domain == "System Bus") {
            if (address + size > 0x10000U) {
                throw std::runtime_error("read_out_of_range");
            }
            std::vector<std::uint8_t> result(size);
            for (std::size_t i = 0; i < size; ++i) {
                result[i] = qn_peek_prgbus(core_, static_cast<int>(address + i));
            }
            return result;
        }

        const auto& info = require_domain(domain);
        if (address + size > info.size) {
            throw std::runtime_error("read_out_of_range");
        }
        return std::vector<std::uint8_t>(info.pointer + address, info.pointer + address + size);
    });
}

void NesRuntime::write_memory(
    const std::uint32_t address,
    const std::vector<std::uint8_t>& value,
    const std::string& domain) {
    dispatch([this, address, value, domain]() {
        if (domain == "System Bus") {
            if (address + value.size() > 0x10000U) {
                throw std::runtime_error("write_out_of_range");
            }
            for (std::size_t i = 0; i < value.size(); ++i) {
                qn_poke_prgbus(core_, static_cast<int>(address + i), value[i]);
            }
            rebuild_domains();
            return;
        }

        auto& info = require_domain(domain);
        if (!info.writable) {
            throw std::runtime_error("domain_read_only");
        }
        if (address + value.size() > info.size) {
            throw std::runtime_error("write_out_of_range");
        }
        std::memcpy(const_cast<std::uint8_t*>(info.pointer) + address, value.data(), value.size());
    });
}

void NesRuntime::owner_loop() {
    owner_thread_id_ = std::this_thread::get_id();
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return owner_stop_requested_.load() || !queue_.empty();
            });
            if (owner_stop_requested_.load() && queue_.empty()) {
                break;
            }
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        job();
    }
}

void NesRuntime::destroy_core() {
    if (core_ != nullptr) {
        qn_delete(core_);
        core_ = nullptr;
    }
    domains_.clear();
}

void NesRuntime::rebuild_domains() {
    domains_.clear();
    if (core_ == nullptr) {
        return;
    }
    for (int index = 0; index < 8; ++index) {
        const void* data = nullptr;
        int size = 0;
        int writable = 0;
        const char* name = nullptr;
        if (!qn_get_memory_area(core_, index, &data, &size, &writable, &name)) {
            continue;
        }
        if (data == nullptr || size <= 0 || name == nullptr) {
            continue;
        }
        domains_.emplace(std::string{name}, NesDomainInfo{
            .name = name,
            .size = static_cast<std::size_t>(size),
            .pointer = static_cast<const std::uint8_t*>(data),
            .writable = writable != 0,
        });
    }
    domains_.emplace("System Bus", NesDomainInfo{
        .name = "System Bus",
        .size = 0x10000U,
        .pointer = nullptr,
        .writable = true,
    });
}

const NesDomainInfo& NesRuntime::require_domain(const std::string& domain) const {
    const auto it = domains_.find(domain);
    if (it == domains_.end()) {
        throw std::runtime_error("domain_missing");
    }
    return it->second;
}

NesDomainInfo& NesRuntime::require_domain(const std::string& domain) {
    const auto it = domains_.find(domain);
    if (it == domains_.end()) {
        throw std::runtime_error("domain_missing");
    }
    return it->second;
}

std::string NesRuntime::compute_crc32_hash() const {
    return to_upper_hex(crc32_bytes(rom_data_));
}

}  // namespace sekailink
