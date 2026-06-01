#include "sekailink/gba_runtime.hpp"

#include <fcntl.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/internal/gb/gb.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/vfs.h>

namespace sekailink {

namespace {

constexpr std::uint32_t kBaseBios = 0x00000000U;
constexpr std::uint32_t kBaseEwram = 0x02000000U;
constexpr std::uint32_t kBaseIwram = 0x03000000U;
constexpr std::uint32_t kBasePalRam = 0x05000000U;
constexpr std::uint32_t kBaseVram = 0x06000000U;
constexpr std::uint32_t kBaseOam = 0x07000000U;
constexpr std::uint32_t kBaseRom = 0x08000000U;
constexpr std::uint32_t kBaseSram = 0x0E000000U;
constexpr std::size_t kCombinedWramSize = (256U + 32U) * 1024U;
constexpr std::uint32_t kGbaSystemBusSize = 0x10000000U;
constexpr std::uint32_t kGbSystemBusSize = 0x00010000U;
constexpr std::uint32_t kGbColorFlagAddress = 0x0143U;

bool has_color_support_flag(const std::vector<std::uint8_t>& header) {
    if (header.size() <= kGbColorFlagAddress) {
        return false;
    }
    const auto value = header[kGbColorFlagAddress];
    return value == 0x80U || value == 0xC0U;
}

bool is_writable(const std::uint32_t flags) {
    return (flags & mCORE_MEMORY_WRITE) != 0;
}

std::string normalize_long_name(const char* value, const std::string& fallback) {
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

void silent_log(struct mLogger*, int, enum mLogLevel, const char*, va_list) {
}

struct SilentLogger {
    mLogFilter filter{};
    mLogger logger{};

    SilentLogger() {
        mLogFilterInit(&filter);
        filter.defaultLevels = 0;
        logger.log = &silent_log;
        logger.filter = &filter;
    }

    ~SilentLogger() {
        mLogFilterDeinit(&filter);
    }
};

SilentLogger& global_silent_logger() {
    static SilentLogger logger;
    return logger;
}

std::string to_upper_hex(const std::uint32_t value) {
    return std::format("{:08X}", value);
}

}  // namespace

GbaRuntime::GbaRuntime() {
    mLogSetDefaultLogger(&global_silent_logger().logger);
    mAudioBufferInit(&resampled_audio_, 8192, 2);
    mAudioResamplerInit(&audio_resampler_, mINTERPOLATOR_SINC);
    mAudioResamplerSetDestination(&audio_resampler_, &resampled_audio_, audio_sample_rate_);
    audio_stream_.runtime = this;
    audio_stream_.stream.audioRateChanged = &GbaRuntime::on_audio_rate_changed;
    audio_stream_.stream.postAudioBuffer = &GbaRuntime::on_post_audio_buffer;
    owner_thread_ = std::thread(&GbaRuntime::owner_loop, this);
}

GbaRuntime::~GbaRuntime() {
    stop();
    mAudioResamplerDeinit(&audio_resampler_);
    mAudioBufferDeinit(&resampled_audio_);
}

void GbaRuntime::load_rom(
    const std::string& rom_path,
    const std::optional<std::string>& bios_path,
    const bool use_bios,
    const bool skip_bios) {
    rom_path_ = rom_path;
    bios_path_ = bios_path;
    dispatch([this, use_bios, skip_bios]() {
        destroy_core();

        system_name_ = "GBA";
        video_buffer_.clear();
        domains_.clear();

        core_ = mCoreFind(rom_path_.c_str());
        if (core_ == nullptr) {
            throw std::runtime_error("core_create_failed");
        }
        if (!core_->init(core_)) {
            core_->deinit(core_);
            core_ = nullptr;
            throw std::runtime_error("core_init_failed");
        }

        mCoreInitConfig(core_, nullptr);
        mCoreConfigSetOverrideIntValue(&core_->config, "useBios", use_bios ? 1 : 0);
        mCoreConfigSetOverrideIntValue(&core_->config, "skipBios", skip_bios ? 1 : 0);
        mCoreConfigSetOverrideIntValue(&core_->config, "logLevel", mLOG_FATAL | mLOG_ERROR);
        mCoreConfigSetOverrideIntValue(&core_->config, "volume", 0x100);
        mCoreConfigSetOverrideIntValue(&core_->config, "mute", 0);
        mCoreConfigSetOverrideIntValue(&core_->config, "audioSync", 0);
        mCoreConfigSetOverrideIntValue(&core_->config, "sampleRate", 48000);
        mCoreConfigSetOverrideIntValue(&core_->config, "audioBuffers", 2048);
        if (bios_path_.has_value()) {
            mCoreConfigSetValue(&core_->config, "gba.bios", bios_path_->c_str());
        }
        mCoreLoadConfig(core_);
        core_->setAudioBufferSize(core_, 2048);
        core_->setAVStream(core_, &audio_stream_.stream);
        {
            std::scoped_lock lock(audio_mutex_);
            mAudioBufferClear(&resampled_audio_);
            audio_source_rate_ = core_->audioSampleRate ? static_cast<double>(core_->audioSampleRate(core_)) : 32768.0;
            if (audio_source_rate_ <= 0.0) {
                audio_source_rate_ = 32768.0;
            }
            mAudioResamplerSetSource(&audio_resampler_, core_->getAudioBuffer(core_), audio_source_rate_, true);
        }
        rebuild_video_buffer();
        if (bios_path_.has_value() && core_->platform(core_) == mPLATFORM_GBA) {
            if (VFile* bios = VFileOpen(bios_path_->c_str(), O_RDONLY)) {
                if (!core_->loadBIOS(core_, bios, 0)) {
                    bios->close(bios);
                    throw std::runtime_error("gba_bios_load_failed");
                }
            } else {
                throw std::runtime_error("gba_bios_open_failed");
            }
        }

        VFile* rom = VFileOpen(rom_path_.c_str(), O_RDONLY);
        if (rom == nullptr) {
            throw std::runtime_error("gba_rom_open_failed");
        }
        if (!core_->loadROM(core_, rom)) {
            rom->close(rom);
            throw std::runtime_error("gba_rom_load_failed");
        }

        if (save_path_.has_value()) {
            if (VFile* save = VFileOpen(save_path_->c_str(), O_CREAT | O_RDWR)) {
                if (!core_->loadSave(core_, save)) {
                    save->close(save);
                    throw std::runtime_error("save_load_failed");
                }
            }
        }
        core_->reset(core_);
        rebuild_video_buffer();
        system_name_ = detect_system();
        rebuild_domains();
    });
}

void GbaRuntime::set_save_path(std::optional<std::string> save_path) {
    save_path_ = std::move(save_path);
}

void GbaRuntime::start() {
    dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
    });
}

void GbaRuntime::stop() {
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

void GbaRuntime::flush_save() {
    dispatch([this]() {
        if (core_ == nullptr || !save_path_.has_value()) {
            return;
        }
        // When a save VFile is attached via loadSave(), mGBA owns persistence and
        // flushes/unmaps it during ROM unload. Rewriting the file from a clone here
        // can overwrite valid mapped save contents with stale data.
    });
}

std::string GbaRuntime::system() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() {
        if (core_ == nullptr) {
            return system_name_;
        }
        return detect_system();
    });
}

std::string GbaRuntime::rom_hash() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        return compute_crc32_hash();
    });
}

std::uint64_t GbaRuntime::frame_count() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() -> std::uint64_t {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        return core_->frameCounter(core_);
    });
}

std::size_t GbaRuntime::memory_size(const std::string& domain) const {
    return const_cast<GbaRuntime*>(this)->dispatch([this, domain]() {
        return require_domain(domain).size;
    });
}

std::vector<DomainInfo> GbaRuntime::memory_domains() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() {
        std::vector<DomainInfo> out;
        out.reserve(domains_.size());
        for (const auto& [_, info] : domains_) {
            out.push_back(info);
        }
        return out;
    });
}

std::optional<std::string> GbaRuntime::save_path() const {
    return save_path_;
}

void GbaRuntime::run_frame() {
    dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        core_->runFrame(core_);
        unsigned width = 0;
        unsigned height = 0;
        core_->currentVideoSize(core_, &width, &height);
        if (width != 0 && height != 0 && video_buffer_.size() != static_cast<std::size_t>(width) * height) {
            rebuild_video_buffer();
        }
    });
}

void GbaRuntime::set_keys(const std::uint32_t keys) {
    dispatch([this, keys]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        core_->setKeys(core_, keys);
    });
}

void GbaRuntime::reset() {
    dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        core_->reset(core_);
        rebuild_domains();
    });
}

std::size_t GbaRuntime::state_size() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() -> std::size_t {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        return core_->stateSize(core_);
    });
}

std::vector<std::uint8_t> GbaRuntime::save_state() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        const std::size_t size = core_->stateSize(core_);
        if (size == 0) {
            throw std::runtime_error("state_size_zero");
        }
        std::vector<std::uint8_t> state(size);
        if (!core_->saveState(core_, state.data())) {
            throw std::runtime_error("save_state_failed");
        }
        return state;
    });
}

void GbaRuntime::load_state(const std::vector<std::uint8_t>& state) {
    dispatch([this, &state]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        const std::size_t expected = core_->stateSize(core_);
        if (state.empty() || state.size() != expected) {
            throw std::runtime_error("state_size_mismatch");
        }
        if (!core_->loadState(core_, state.data())) {
            throw std::runtime_error("load_state_failed");
        }
        rebuild_domains();
    });
}

unsigned GbaRuntime::audio_sample_rate() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() -> unsigned {
        return audio_sample_rate_;
    });
}

unsigned GbaRuntime::audio_channels() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() -> unsigned {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        auto* buffer = core_->getAudioBuffer(core_);
        return buffer != nullptr && buffer->channels != 0 ? buffer->channels : 2U;
    });
}

std::vector<std::int16_t> GbaRuntime::read_audio_frames(const std::size_t max_frames) {
    if (max_frames == 0) {
        return {};
    }

    std::scoped_lock lock(audio_mutex_);
    const std::size_t available_frames = std::min<std::size_t>(mAudioBufferAvailable(&resampled_audio_), max_frames);
    if (available_frames == 0) {
        return {};
    }
    const std::size_t channels = std::max<unsigned>(1U, resampled_audio_.channels);
    std::vector<std::int16_t> samples(available_frames * channels);
    const std::size_t frames_read = mAudioBufferRead(&resampled_audio_, samples.data(), available_frames);
    samples.resize(frames_read * channels);
    return samples;
}

std::vector<std::uint32_t> GbaRuntime::video_frame() const {
    return const_cast<GbaRuntime*>(this)->dispatch([this]() {
        if (core_ != nullptr && core_->platform(core_) == mPLATFORM_GB && core_->getPixels != nullptr) {
            size_t stride = 0;
            const void* pixels = nullptr;
            core_->getPixels(core_, &pixels, &stride);
            unsigned width = 0;
            unsigned height = 0;
            core_->currentVideoSize(core_, &width, &height);
            if (pixels != nullptr && width != 0 && height != 0) {
                const auto* rows = static_cast<const std::uint32_t*>(pixels);
                std::vector<std::uint32_t> frame(static_cast<std::size_t>(width) * height);
                for (unsigned y = 0; y < height; ++y) {
                    std::memcpy(
                        frame.data() + static_cast<std::size_t>(y) * width,
                        rows + static_cast<std::size_t>(y) * stride,
                        static_cast<std::size_t>(width) * sizeof(std::uint32_t));
                }
                return frame;
            }
        }
        return video_buffer_;
    });
}

std::vector<std::uint8_t> GbaRuntime::read_memory(
    const std::uint32_t address,
    const std::size_t size,
    const std::string& domain) {
    return dispatch([this, address, size, domain]() {
        if (domain == "System Bus") {
            return read_bus(address, size);
        }
        if (domain == "Combined WRAM") {
            return read_combined_wram(address, size);
        }
        const DomainInfo& info = require_domain(domain);
        if (address > info.size || size > info.size || address + size > info.size) {
            throw std::runtime_error("read_out_of_range");
        }
        const auto* bytes = static_cast<const std::uint8_t*>(info.pointer);
        return std::vector<std::uint8_t>(bytes + address, bytes + address + size);
    });
}

void GbaRuntime::write_memory(
    const std::uint32_t address,
    const std::vector<std::uint8_t>& value,
    const std::string& domain) {
    dispatch([this, address, value, domain]() {
        if (domain == "System Bus") {
            write_bus(address, value);
            return;
        }
        if (domain == "Combined WRAM") {
            write_combined_wram(address, value);
            return;
        }
        DomainInfo& info = require_domain(domain);
        if (!info.writable) {
            throw std::runtime_error("domain_read_only");
        }
        if (address > info.size || value.size() > info.size || address + value.size() > info.size) {
            throw std::runtime_error("write_out_of_range");
        }
        auto* bytes = static_cast<std::uint8_t*>(info.pointer);
        std::memcpy(bytes + address, value.data(), value.size());
    });
}

void GbaRuntime::set_locked(const bool value) {
    locked_.store(value);
}

bool GbaRuntime::locked() const {
    return locked_.load();
}

void GbaRuntime::display_message(const std::string& message) {
    std::cerr << "[sekailink_runtime_gba] message: " << message << '\n';
}

void GbaRuntime::set_message_interval(const double value) {
    message_interval_seconds_ = value;
}

void GbaRuntime::with_transaction(const std::function<void()>& fn) {
    dispatch([this, &fn]() {
        if (core_ == nullptr) {
            throw std::runtime_error("runtime_not_loaded");
        }
        fn();
    });
}

void GbaRuntime::owner_loop() {
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

void GbaRuntime::destroy_core() {
    domains_.clear();
    if (core_ != nullptr) {
        mCoreConfigDeinit(&core_->config);
        core_->deinit(core_);
        core_ = nullptr;
    }
    std::scoped_lock lock(audio_mutex_);
    mAudioBufferClear(&resampled_audio_);
}

void GbaRuntime::rebuild_domains() {
    if (core_ == nullptr) {
        throw std::runtime_error("runtime_not_loaded");
    }

    const struct mCoreMemoryBlock* blocks = nullptr;
    const auto block_count = core_->listMemoryBlocks(core_, &blocks);

    auto add_domain = [this](const std::string& name, const std::uint32_t base, std::size_t size, void* pointer, const bool writable) {
        if (pointer == nullptr || size == 0) {
            return;
        }
        domains_[name] = DomainInfo{name, base, size, pointer, writable};
    };

    for (std::size_t index = 0; index < block_count; ++index) {
        const auto& block = blocks[index];
        if ((block.flags & mCORE_MEMORY_MAPPED) == 0 || block.size == 0) {
            continue;
        }
        std::size_t size = 0;
        void* pointer = core_->getMemoryBlock(core_, block.id, &size);
        if (pointer == nullptr || size == 0) {
            continue;
        }
        const auto writable = is_writable(block.flags);
        const auto label = normalize_long_name(block.longName, block.shortName != nullptr ? block.shortName : block.internalName);
        add_domain(label, block.start, size, pointer, writable);

        const std::string internal_name = block.internalName != nullptr ? block.internalName : "";
        if (internal_name == "bios") {
            add_domain("BIOS", block.start, size, pointer, writable);
        } else if (internal_name == "wram") {
            if (core_->platform(core_) == mPLATFORM_GBA) {
                add_domain("EWRAM", block.start, size, pointer, writable);
            }
            add_domain("WRAM", block.start, size, pointer, writable);
            add_domain("RAM", block.start, size, pointer, writable);
        } else if (internal_name == "iwram") {
            add_domain("IWRAM", block.start, size, pointer, writable);
        } else if (internal_name == "palette") {
            add_domain("PALRAM", block.start, size, pointer, writable);
        } else if (internal_name == "vram") {
            add_domain("VRAM", block.start, size, pointer, writable);
        } else if (internal_name == "oam") {
            add_domain("OAM", block.start, size, pointer, writable);
        } else if (internal_name == "cart0") {
            add_domain("ROM", block.start, size, pointer, writable);
        } else if (internal_name == "sram") {
            add_domain("SRAM", block.start, size, pointer, writable);
            add_domain("CARTRAM", block.start, size, pointer, writable);
        } else if (internal_name == "eeprom") {
            add_domain("SRAM", block.start, size, pointer, writable);
        } else if (internal_name == "io") {
            add_domain("IO", block.start, size, pointer, writable);
            add_domain("MMIO", block.start, size, pointer, writable);
        } else if (internal_name == "hram") {
            add_domain("HRAM", block.start, size, pointer, writable);
        }
    }

    if (core_->platform(core_) == mPLATFORM_GBA && domains_.contains("EWRAM") && domains_.contains("IWRAM")) {
        domains_["Combined WRAM"] = DomainInfo{"Combined WRAM", 0U, kCombinedWramSize, nullptr, true};
    }
    domains_["System Bus"] = DomainInfo{"System Bus", 0U, system_bus_size(), nullptr, true};
}

void GbaRuntime::rebuild_video_buffer() {
    if (core_ == nullptr) {
        return;
    }
    unsigned width = 0;
    unsigned height = 0;
    core_->baseVideoSize(core_, &width, &height);
    if (width == 0 || height == 0) {
        throw std::runtime_error("video_size_invalid");
    }
    video_buffer_.assign(static_cast<std::size_t>(width) * height, 0U);
    core_->setVideoBuffer(core_, video_buffer_.data(), width);
}

void GbaRuntime::on_audio_rate_changed(mAVStream* stream, const unsigned rate) {
    auto* capture = reinterpret_cast<AudioCaptureStream*>(stream);
    if (capture == nullptr || capture->runtime == nullptr) {
        return;
    }
    auto* runtime = capture->runtime;
    std::scoped_lock lock(runtime->audio_mutex_);
    runtime->audio_source_rate_ = rate != 0 ? static_cast<double>(rate) : 32768.0;
    if (runtime->core_ != nullptr && runtime->audio_resampler_.source != nullptr) {
        mAudioResamplerProcess(&runtime->audio_resampler_);
        mAudioResamplerSetSource(
            &runtime->audio_resampler_,
            runtime->core_->getAudioBuffer(runtime->core_),
            runtime->audio_source_rate_,
            true);
    }
}

void GbaRuntime::on_post_audio_buffer(mAVStream* stream, mAudioBuffer* buffer) {
    auto* capture = reinterpret_cast<AudioCaptureStream*>(stream);
    if (capture == nullptr || capture->runtime == nullptr || buffer == nullptr) {
        return;
    }
    auto& runtime = *capture->runtime;
    std::scoped_lock lock(runtime.audio_mutex_);
    if (runtime.audio_resampler_.source != buffer) {
        mAudioResamplerSetSource(&runtime.audio_resampler_, buffer, runtime.audio_source_rate_, true);
    }
    mAudioResamplerProcess(&runtime.audio_resampler_);
    const std::size_t max_latency_frames = std::max<std::size_t>(2048U, runtime.audio_sample_rate_ / 8U);
    if (mAudioBufferAvailable(&runtime.resampled_audio_) > max_latency_frames * 2U) {
        mAudioBufferClear(&runtime.resampled_audio_);
    }
}

const DomainInfo& GbaRuntime::require_domain(const std::string& domain) const {
    const auto it = domains_.find(domain);
    if (it == domains_.end()) {
        throw std::runtime_error("unknown_domain:" + domain);
    }
    return it->second;
}

DomainInfo& GbaRuntime::require_domain(const std::string& domain) {
    const auto it = domains_.find(domain);
    if (it == domains_.end()) {
        throw std::runtime_error("unknown_domain:" + domain);
    }
    return it->second;
}

std::vector<std::uint8_t> GbaRuntime::read_bus(const std::uint32_t address, const std::size_t size) {
    if (address + size > system_bus_size()) {
        throw std::runtime_error("read_out_of_range");
    }
    std::vector<std::uint8_t> out;
    out.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
        out.push_back(static_cast<std::uint8_t>(core_->busRead8(core_, address + static_cast<std::uint32_t>(index))));
    }
    return out;
}

void GbaRuntime::write_bus(const std::uint32_t address, const std::vector<std::uint8_t>& value) {
    if (address + value.size() > system_bus_size()) {
        throw std::runtime_error("write_out_of_range");
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        core_->busWrite8(core_, address + static_cast<std::uint32_t>(index), value[index]);
    }
}

std::vector<std::uint8_t> GbaRuntime::read_combined_wram(const std::uint32_t address, const std::size_t size) {
    if (!domains_.contains("EWRAM") || !domains_.contains("IWRAM")) {
        throw std::runtime_error("combined_wram_unavailable");
    }
    if (address + size > kCombinedWramSize) {
        throw std::runtime_error("read_out_of_range");
    }
    const DomainInfo& ewram = require_domain("EWRAM");
    const DomainInfo& iwram = require_domain("IWRAM");
    const auto* ewram_bytes = static_cast<const std::uint8_t*>(ewram.pointer);
    const auto* iwram_bytes = static_cast<const std::uint8_t*>(iwram.pointer);

    std::vector<std::uint8_t> out;
    out.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
        const std::size_t absolute = address + index;
        if (absolute < ewram.size) {
            out.push_back(ewram_bytes[absolute]);
        } else {
            out.push_back(iwram_bytes[absolute - ewram.size]);
        }
    }
    return out;
}

void GbaRuntime::write_combined_wram(const std::uint32_t address, const std::vector<std::uint8_t>& value) {
    if (!domains_.contains("EWRAM") || !domains_.contains("IWRAM")) {
        throw std::runtime_error("combined_wram_unavailable");
    }
    if (address + value.size() > kCombinedWramSize) {
        throw std::runtime_error("write_out_of_range");
    }
    DomainInfo& ewram = require_domain("EWRAM");
    DomainInfo& iwram = require_domain("IWRAM");
    auto* ewram_bytes = static_cast<std::uint8_t*>(ewram.pointer);
    auto* iwram_bytes = static_cast<std::uint8_t*>(iwram.pointer);

    for (std::size_t index = 0; index < value.size(); ++index) {
        const std::size_t absolute = address + index;
        if (absolute < ewram.size) {
            ewram_bytes[absolute] = value[index];
        } else {
            iwram_bytes[absolute - ewram.size] = value[index];
        }
    }
}

std::string GbaRuntime::compute_crc32_hash() const {
    std::uint32_t crc32 = 0;
    core_->checksum(core_, &crc32, mCHECKSUM_CRC32);
    return to_upper_hex(crc32);
}

std::uint32_t GbaRuntime::system_bus_size() const {
    if (core_ != nullptr && core_->platform(core_) == mPLATFORM_GB) {
        return kGbSystemBusSize;
    }
    return kGbaSystemBusSize;
}

std::string GbaRuntime::detect_system() const {
    if (core_ == nullptr) {
        return system_name_;
    }
    if (core_->platform(core_) == mPLATFORM_GBA) {
        return "GBA";
    }
    if (core_->platform(core_) == mPLATFORM_GB) {
        const struct mCoreMemoryBlock* blocks = nullptr;
        const auto block_count = core_->listMemoryBlocks(core_, &blocks);
        for (std::size_t index = 0; index < block_count; ++index) {
            const auto& block = blocks[index];
            const std::string internal_name = block.internalName != nullptr ? block.internalName : "";
            if (internal_name != "cart0") {
                continue;
            }
            std::size_t rom_size = 0;
            void* rom_ptr = core_->getMemoryBlock(core_, block.id, &rom_size);
            if (rom_ptr == nullptr || rom_size <= kGbColorFlagAddress) {
                break;
            }
            const auto* bytes = static_cast<const std::uint8_t*>(rom_ptr);
            const std::vector<std::uint8_t> header(bytes, bytes + std::min<std::size_t>(rom_size, 0x150U));
            return has_color_support_flag(header) ? "GBC" : "GB";
        }
        return "GB";
    }
    return system_name_;
}

}  // namespace sekailink
