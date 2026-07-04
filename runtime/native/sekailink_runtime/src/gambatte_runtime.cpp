#include "sekailink/gambatte_runtime.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <vector>

#include <gambatte.h>

namespace sekailink {

namespace {

constexpr std::size_t kFrameSamples = 35112;
constexpr std::size_t kAudioOverrun = 2064;
constexpr int kVideoWidth = 160;
constexpr int kVideoHeight = 144;
constexpr unsigned kAudioSampleRate = 2097152U;
constexpr unsigned kAudioChannels = 2U;
constexpr unsigned long kDmgPalette[12] = {
    0xFFFFFFUL, 0xAAAAAAUL, 0x555555UL, 0x000000UL,
    0xFFFFFFUL, 0xAAAAAAUL, 0x555555UL, 0x000000UL,
    0xFFFFFFUL, 0xAAAAAAUL, 0x555555UL, 0x000000UL,
};
constexpr int kSameBoyCgbCurve[32] = {
    0, 6, 12, 20, 28, 36, 45, 56,
    66, 76, 88, 100, 113, 125, 137, 149,
    161, 172, 182, 192, 202, 210, 218, 225,
    232, 238, 243, 247, 250, 252, 254, 255
};

std::vector<char> read_file_bytes(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("file_open_failed:" + path);
    }
    return std::vector<char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool has_extension(const std::string& path, const std::string& extension) {
    return std::filesystem::path(path).extension() == extension;
}

void apply_default_dmg_palette(gambatte::GB& core) {
    for (int palette = 0; palette < 3; ++palette) {
        for (int color = 0; color < 4; ++color) {
            core.setDmgPaletteColor(palette, color, kDmgPalette[palette * 4 + color]);
        }
    }
}

void apply_default_cgb_palette(gambatte::GB& core) {
    std::vector<unsigned> lut(32768);
    int index = 0;
    for (int b = 0; b < 32; ++b) {
        for (int g = 0; g < 32; ++g) {
            for (int r = 0; r < 32; ++r) {
                int red = kSameBoyCgbCurve[r];
                int green = (kSameBoyCgbCurve[g] * 3 + kSameBoyCgbCurve[b]) / 4;
                int blue = kSameBoyCgbCurve[b];
                lut[index++] = 0xFF000000U
                    | (static_cast<unsigned>(red) << 16U)
                    | (static_cast<unsigned>(green) << 8U)
                    | static_cast<unsigned>(blue);
            }
        }
    }
    core.setCgbPalette(lut.data());
}

}  // namespace

GambatteRuntime::GambatteRuntime()
    : frame_buffer_(static_cast<std::size_t>(kVideoWidth) * kVideoHeight, 0U),
      raw_audio_buffer_(kFrameSamples + kAudioOverrun, 0U),
      core_(new gambatte::GB()) {
    if (core_ == nullptr) {
        throw std::runtime_error("gambatte_create_failed");
    }
}

GambatteRuntime::~GambatteRuntime() {
    stop();
    delete core_;
    core_ = nullptr;
}

void GambatteRuntime::load_rom(const std::string& rom_path, const std::optional<std::string>& bios_path) {
    rom_path_ = rom_path;
    bios_path_ = bios_path;
    frame_count_ = 0;

    if (core_ == nullptr) {
        throw std::runtime_error("gambatte_not_initialized");
    }

    if (bios_path_.has_value()) {
        const auto bios = read_file_bytes(*bios_path_);
        if (core_->loadBios(bios.data(), bios.size()) != 0) {
            throw std::runtime_error("gambatte_bios_load_failed");
        }
    }

    if (core_->load(rom_path_, load_flags()) != 0) {
        throw std::runtime_error("gambatte_rom_load_failed");
    }

    is_cgb_ = core_->isCgb();
    if (is_cgb_) {
        apply_default_cgb_palette(*core_);
    } else {
        apply_default_dmg_palette(*core_);
    }
    load_savedata();
}

void GambatteRuntime::set_save_path(std::optional<std::string> save_path) {
    save_path_ = std::move(save_path);
}

std::optional<std::string> GambatteRuntime::save_path() const {
    return save_path_;
}

void GambatteRuntime::start() {
    running_ = true;
}

void GambatteRuntime::stop() {
    if (!running_) {
        return;
    }
    flush_save();
    running_ = false;
}

void GambatteRuntime::run_frame() {
    if (!running_) {
        throw std::runtime_error("runtime_not_running");
    }

    audio_frames_.clear();

    while (true) {
        std::size_t samples = kFrameSamples;
        const auto frame_result = core_->runFor(frame_buffer_.data(), kVideoWidth, raw_audio_buffer_.data(), samples);

        const auto previous_size = audio_frames_.size();
        audio_frames_.resize(previous_size + samples * 2U);
        for (std::size_t index = 0; index < samples; ++index) {
            const auto packed = raw_audio_buffer_[index];
            audio_frames_[previous_size + index * 2U] = static_cast<std::int16_t>(packed & 0xFFFFU);
            audio_frames_[previous_size + index * 2U + 1U] = static_cast<std::int16_t>((packed >> 16) & 0xFFFFU);
        }

        if (frame_result >= 0) {
            break;
        }
    }
    ++frame_count_;
}

void GambatteRuntime::set_keys(const std::uint32_t keys) {
    keys_ = keys;
    if (core_ != nullptr) {
        core_->setInputGetter(&GambatteRuntime::input_getter, this);
    }
}

void GambatteRuntime::reset() {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }
    core_->reset(0);
    frame_count_ = 0;
    load_savedata();
}

std::string GambatteRuntime::rom_hash() const {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }
    return compute_crc32_hash();
}

std::uint64_t GambatteRuntime::frame_count() const {
    return frame_count_;
}

std::size_t GambatteRuntime::memory_size(const std::string& domain) const {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }
    if (domain == "System Bus") {
        return 0x10000U;
    }

    int which = -1;
    if (domain == "VRAM") {
        which = 0;
    } else if (domain == "ROM") {
        which = 1;
    } else if (domain == "WRAM") {
        which = 2;
    } else if (domain == "CartRAM" || domain == "SRAM") {
        which = 3;
    } else if (domain == "OAM") {
        which = 4;
    } else if (domain == "HRAM") {
        which = 5;
    } else {
        throw std::runtime_error("unknown_memory_domain:" + domain);
    }

    unsigned char* data = nullptr;
    int length = 0;
    if (!core_->getMemoryArea(which, &data, &length) || data == nullptr || length < 0) {
        return 0U;
    }
    return static_cast<std::size_t>(length);
}

std::string GambatteRuntime::system() const {
    return is_cgb_ ? "GBC" : "GB";
}

std::vector<std::uint32_t> GambatteRuntime::video_frame() const {
    return frame_buffer_;
}

std::vector<std::int16_t> GambatteRuntime::read_audio_frames() {
    auto out = audio_frames_;
    audio_frames_.clear();
    return out;
}

unsigned GambatteRuntime::audio_sample_rate() const {
    return kAudioSampleRate;
}

unsigned GambatteRuntime::audio_channels() const {
    return kAudioChannels;
}

std::size_t GambatteRuntime::state_size() const {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }
    return core_->saveState(frame_buffer_.data(), kVideoWidth, static_cast<char*>(nullptr));
}

std::vector<std::uint8_t> GambatteRuntime::save_state() const {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }
    const auto size = state_size();
    if (size == 0) {
        throw std::runtime_error("state_size_zero");
    }
    std::vector<std::uint8_t> state(size);
    const auto actual = core_->saveState(frame_buffer_.data(), kVideoWidth, reinterpret_cast<char*>(state.data()));
    if (actual != size) {
        throw std::runtime_error("state_save_size_mismatch");
    }
    return state;
}

void GambatteRuntime::load_state(const std::vector<std::uint8_t>& state) {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }
    if (state.empty()) {
        throw std::runtime_error("state_empty");
    }
    if (!core_->loadState(reinterpret_cast<const char*>(state.data()), state.size())) {
        throw std::runtime_error("state_load_failed");
    }
}

bool GambatteRuntime::save_state(const std::string& path) {
    if (core_ == nullptr) {
        return false;
    }
    return core_->saveState(frame_buffer_.data(), kVideoWidth, path);
}

bool GambatteRuntime::load_state(const std::string& path) {
    if (core_ == nullptr) {
        return false;
    }
    return core_->loadState(path);
}

bool GambatteRuntime::flush_save() {
    if (core_ == nullptr || !save_path_.has_value()) {
        return false;
    }

    const auto save_length = core_->getSavedataLength();
    if (save_length == 0) {
        return false;
    }

    std::vector<char> save_data(save_length, 0);
    core_->saveSavedata(save_data.data());

    std::ofstream stream(*save_path_, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(save_data.data(), static_cast<std::streamsize>(save_data.size()));
    return stream.good();
}

std::vector<std::uint8_t> GambatteRuntime::read_memory(
    const std::uint32_t address,
    const std::size_t size,
    const std::string& domain) {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }

    if (domain == "System Bus") {
        std::vector<std::uint8_t> out(size, 0);
        for (std::size_t i = 0; i < size; ++i) {
            out[i] = core_->externalRead(static_cast<unsigned short>((address + i) & 0xFFFFU));
        }
        return out;
    }

    int which = -1;
    if (domain == "VRAM") {
        which = 0;
    } else if (domain == "ROM") {
        which = 1;
    } else if (domain == "WRAM") {
        which = 2;
    } else if (domain == "CartRAM" || domain == "SRAM") {
        which = 3;
    } else if (domain == "OAM") {
        which = 4;
    } else if (domain == "HRAM") {
        which = 5;
    } else {
        throw std::runtime_error("unknown_memory_domain:" + domain);
    }

    unsigned char* data = nullptr;
    int length = 0;
    if (!core_->getMemoryArea(which, &data, &length) || data == nullptr || length <= 0) {
        return {};
    }
    const auto start = std::min<std::size_t>(address, static_cast<std::size_t>(length));
    const auto end = std::min<std::size_t>(start + size, static_cast<std::size_t>(length));
    return std::vector<std::uint8_t>(data + start, data + end);
}

void GambatteRuntime::write_memory(
    const std::uint32_t address,
    const std::vector<std::uint8_t>& value,
    const std::string& domain) {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }

    if (domain == "System Bus") {
        for (std::size_t i = 0; i < value.size(); ++i) {
            core_->externalWrite(static_cast<unsigned short>((address + i) & 0xFFFFU), value[i]);
        }
        return;
    }

    int which = -1;
    if (domain == "VRAM") {
        which = 0;
    } else if (domain == "ROM") {
        throw std::runtime_error("memory_domain_not_writable:ROM");
    } else if (domain == "WRAM") {
        which = 2;
    } else if (domain == "CartRAM" || domain == "SRAM") {
        which = 3;
    } else if (domain == "OAM") {
        which = 4;
    } else if (domain == "HRAM") {
        which = 5;
    } else {
        throw std::runtime_error("unknown_memory_domain:" + domain);
    }

    unsigned char* data = nullptr;
    int length = 0;
    if (!core_->getMemoryArea(which, &data, &length) || data == nullptr || length <= 0) {
        throw std::runtime_error("memory_domain_unavailable:" + domain);
    }
    const auto start = std::min<std::size_t>(address, static_cast<std::size_t>(length));
    const auto copy_size = std::min<std::size_t>(value.size(), static_cast<std::size_t>(length) - start);
    std::copy_n(value.begin(), copy_size, data + start);
}

unsigned GambatteRuntime::input_getter(void* context) {
    return static_cast<GambatteRuntime*>(context)->keys_;
}

unsigned GambatteRuntime::load_flags() const {
    unsigned flags = gambatte::GB::READONLY_SAV | gambatte::GB::NO_BIOS;
    if (has_extension(rom_path_, ".gbc")) {
        flags |= gambatte::GB::CGB_MODE;
    }
    return flags;
}

void GambatteRuntime::load_savedata() {
    if (core_ == nullptr || !save_path_.has_value()) {
        return;
    }

    const auto expected_size = core_->getSavedataLength();
    if (expected_size == 0 || !std::filesystem::exists(*save_path_)) {
        return;
    }

    auto save_data = read_file_bytes(*save_path_);
    if (save_data.size() != expected_size) {
        save_data.resize(expected_size, static_cast<char>(0xFF));
    }
    core_->loadSavedata(save_data.data());
}

std::string GambatteRuntime::compute_crc32_hash() const {
    const auto rom_data = read_file_bytes(rom_path_);
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : rom_data) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return std::format("{:08X}", ~crc);
}

}  // namespace sekailink
