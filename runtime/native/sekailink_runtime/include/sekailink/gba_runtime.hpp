#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>

struct mCore;
struct mCoreMemoryBlock;

namespace sekailink {

struct DomainInfo {
    std::string name;
    std::uint32_t base = 0;
    std::size_t size = 0;
    void* pointer = nullptr;
    bool writable = false;
};

class GbaRuntime {
public:
    GbaRuntime();
    ~GbaRuntime();

    void load_rom(const std::string& rom_path, const std::optional<std::string>& bios_path, bool use_bios, bool skip_bios);
    void set_save_path(std::optional<std::string> save_path);
    void start();
    void stop();
    void flush_save();

    [[nodiscard]] std::string system() const;
    [[nodiscard]] std::string rom_hash() const;
    [[nodiscard]] std::uint64_t frame_count() const;
    [[nodiscard]] std::size_t memory_size(const std::string& domain) const;
    [[nodiscard]] std::vector<DomainInfo> memory_domains() const;
    [[nodiscard]] std::optional<std::string> save_path() const;
    void run_frame();
    void set_keys(std::uint32_t keys);
    void reset();
    [[nodiscard]] std::size_t state_size() const;
    [[nodiscard]] std::vector<std::uint8_t> save_state() const;
    void load_state(const std::vector<std::uint8_t>& state);
    [[nodiscard]] unsigned audio_sample_rate() const;
    [[nodiscard]] unsigned audio_channels() const;
    [[nodiscard]] std::vector<std::int16_t> read_audio_frames(std::size_t max_frames);
    [[nodiscard]] std::vector<std::uint32_t> video_frame() const;

    std::vector<std::uint8_t> read_memory(std::uint32_t address, std::size_t size, const std::string& domain);
    void write_memory(std::uint32_t address, const std::vector<std::uint8_t>& value, const std::string& domain);

    void set_locked(bool value);
    [[nodiscard]] bool locked() const;

    void display_message(const std::string& message);
    void set_message_interval(double value);

    void with_transaction(const std::function<void()>& fn);

private:
    struct AudioCaptureStream {
        mAVStream stream{};
        GbaRuntime* runtime = nullptr;
    };

    template <typename Fn>
    auto dispatch(Fn&& fn) -> std::invoke_result_t<Fn>;
    void owner_loop();
    void destroy_core();
    void rebuild_domains();
    void rebuild_video_buffer();
    [[nodiscard]] const DomainInfo& require_domain(const std::string& domain) const;
    [[nodiscard]] DomainInfo& require_domain(const std::string& domain);
    [[nodiscard]] std::vector<std::uint8_t> read_bus(std::uint32_t address, std::size_t size);
    void write_bus(std::uint32_t address, const std::vector<std::uint8_t>& value);
    [[nodiscard]] std::vector<std::uint8_t> read_combined_wram(std::uint32_t address, std::size_t size);
    void write_combined_wram(std::uint32_t address, const std::vector<std::uint8_t>& value);
    [[nodiscard]] std::uint32_t system_bus_size() const;
    [[nodiscard]] std::string detect_system() const;
    [[nodiscard]] std::string compute_crc32_hash() const;
    static void on_audio_rate_changed(mAVStream* stream, unsigned rate);
    static void on_post_audio_buffer(mAVStream* stream, mAudioBuffer* buffer);

    mCore* core_ = nullptr;
    std::vector<std::uint32_t> video_buffer_;
    std::unordered_map<std::string, DomainInfo> domains_;
    AudioCaptureStream audio_stream_{};
    mutable std::mutex audio_mutex_;
    mAudioBuffer resampled_audio_{};
    mAudioResampler audio_resampler_{};
    mutable unsigned audio_sample_rate_ = 48000U;
    double audio_source_rate_ = 32768.0;
    std::string rom_path_;
    std::optional<std::string> bios_path_;
    std::atomic<bool> locked_{false};
    std::atomic<bool> owner_stop_requested_{false};
    std::thread owner_thread_;
    std::thread::id owner_thread_id_{};
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::function<void()>> queue_;
    double message_interval_seconds_ = 0.0;
    std::optional<std::string> save_path_;
    std::string system_name_ = "GBA";
};

template <typename Fn>
auto GbaRuntime::dispatch(Fn&& fn) -> std::invoke_result_t<Fn> {
    using Result = std::invoke_result_t<Fn>;

    if (std::this_thread::get_id() == owner_thread_id_) {
        if constexpr (std::is_void_v<Result>) {
            fn();
            return;
        } else {
            return fn();
        }
    }

    auto promise = std::make_shared<std::promise<Result>>();
    auto future = promise->get_future();

    {
        std::scoped_lock lock(queue_mutex_);
        queue_.push_back([promise, func = std::forward<Fn>(fn)]() mutable {
            try {
                if constexpr (std::is_void_v<Result>) {
                    func();
                    promise->set_value();
                } else {
                    promise->set_value(func());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    }
    queue_cv_.notify_one();

    if constexpr (std::is_void_v<Result>) {
        future.get();
    } else {
        return future.get();
    }
}

}  // namespace sekailink
