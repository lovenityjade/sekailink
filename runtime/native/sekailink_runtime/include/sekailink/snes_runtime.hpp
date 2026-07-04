#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
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

namespace sekailink {

char* snes_path_request_cb(int slot, const char* hint, bool required);
void snes_video_frame_cb(const std::uint16_t* data, int width, int height, int pitch);
std::int16_t snes_input_poll_cb(int port, int index, int id);
void snes_controller_latch_cb();
void snes_no_lag_cb(bool sgb_poll);
void snes_trace_cb(const char* disassembly, const char* register_info);
void snes_read_hook_cb(std::uint32_t address, std::uint8_t& value);
void snes_write_hook_cb(std::uint32_t address, std::uint8_t& value);
void snes_exec_hook_cb(std::uint32_t address);
std::int64_t snes_time_cb();
void snes_msu_open_cb(std::uint16_t track_id);
void snes_msu_seek_cb(long offset, bool relative);
std::uint8_t snes_msu_read_cb();
bool snes_msu_end_cb();

struct SnesDomainInfo {
    std::string name;
    std::size_t size = 0;
    std::uint8_t* pointer = nullptr;
    bool writable = false;
};

struct SnesFrameSnapshot {
    std::vector<std::uint32_t> video;
    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::int16_t> audio;
};

class SnesRuntime {
public:
    SnesRuntime();
    ~SnesRuntime();

    void load_rom(const std::string& rom_path);
    void set_save_path(std::optional<std::string> save_path);
    void start();
    void stop();
    void flush_save();

    [[nodiscard]] std::string system() const;
    [[nodiscard]] std::string rom_hash() const;
    [[nodiscard]] std::uint64_t frame_count() const;
    [[nodiscard]] std::size_t memory_size(const std::string& domain) const;
    [[nodiscard]] std::vector<SnesDomainInfo> memory_domains() const;
    [[nodiscard]] std::optional<std::string> save_path() const;
    [[nodiscard]] std::vector<std::uint32_t> video_frame() const;
    [[nodiscard]] unsigned video_width() const;
    [[nodiscard]] unsigned video_height() const;
    void run_frame();
    void set_keys(std::uint32_t keys);
    void reset();
    [[nodiscard]] std::size_t state_size() const;
    [[nodiscard]] std::vector<std::uint8_t> save_state() const;
    void load_state(const std::vector<std::uint8_t>& state);
    [[nodiscard]] unsigned audio_sample_rate() const;
    [[nodiscard]] unsigned audio_channels() const;
    [[nodiscard]] std::vector<std::int16_t> read_audio_samples(std::size_t max_samples);
    [[nodiscard]] SnesFrameSnapshot consume_frame_snapshot(std::size_t max_audio_samples);
    [[nodiscard]] SnesFrameSnapshot run_frame_and_consume_snapshot(std::size_t max_audio_samples);

    std::vector<std::uint8_t> read_memory(std::uint32_t address, std::size_t size, const std::string& domain);
    void write_memory(std::uint32_t address, const std::vector<std::uint8_t>& value, const std::string& domain);

private:
    friend char* snes_path_request_cb(int slot, const char* hint, bool required);
    friend void snes_video_frame_cb(const std::uint16_t* data, int width, int height, int pitch);
    friend std::int16_t snes_input_poll_cb(int port, int index, int id);
    friend void snes_controller_latch_cb();
    friend void snes_no_lag_cb(bool sgb_poll);
    friend void snes_trace_cb(const char* disassembly, const char* register_info);
    friend void snes_read_hook_cb(std::uint32_t address, std::uint8_t& value);
    friend void snes_write_hook_cb(std::uint32_t address, std::uint8_t& value);
    friend void snes_exec_hook_cb(std::uint32_t address);
    friend std::int64_t snes_time_cb();
    friend void snes_msu_open_cb(std::uint16_t track_id);
    friend void snes_msu_seek_cb(long offset, bool relative);
    friend std::uint8_t snes_msu_read_cb();
    friend bool snes_msu_end_cb();

    template <typename Fn>
    auto dispatch(Fn&& fn) -> std::invoke_result_t<Fn>;

    void owner_loop();
    void destroy_core();
    void rebuild_domains();
    [[nodiscard]] const SnesDomainInfo& require_domain(const std::string& domain) const;
    [[nodiscard]] SnesDomainInfo& require_domain(const std::string& domain);
    [[nodiscard]] std::string compute_crc32_hash() const;
    void load_sram_if_present();

    std::unordered_map<std::string, SnesDomainInfo> domains_;
    std::vector<std::uint8_t> rom_data_;
    std::string rom_path_;
    std::optional<std::string> save_path_;
    std::vector<std::uint32_t> video_buffer_;
    unsigned video_width_ = 256;
    unsigned video_height_ = 224;
    std::vector<std::int16_t> audio_samples_;
    std::uint32_t keys_ = 0U;
    std::uint64_t frame_count_ = 0U;
    unsigned audio_sample_rate_ = 32040U;
    unsigned audio_channels_ = 2U;
    std::atomic<bool> owner_stop_requested_{false};
    std::thread owner_thread_;
    std::thread::id owner_thread_id_{};
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::function<void()>> queue_;
};

template <typename Fn>
auto SnesRuntime::dispatch(Fn&& fn) -> std::invoke_result_t<Fn> {
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
