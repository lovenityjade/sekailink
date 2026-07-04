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

namespace quickerNES {
class Emu;
}

namespace sekailink {

struct NesDomainInfo {
    std::string name;
    std::size_t size = 0;
    const std::uint8_t* pointer = nullptr;
    bool writable = false;
};

class NesRuntime {
public:
    NesRuntime();
    ~NesRuntime();

    void load_rom(const std::string& rom_path);
    void set_save_path(std::optional<std::string> save_path);
    void start();
    void stop();
    void flush_save();

    [[nodiscard]] std::string system() const;
    [[nodiscard]] std::string rom_hash() const;
    [[nodiscard]] std::uint64_t frame_count() const;
    [[nodiscard]] std::size_t memory_size(const std::string& domain) const;
    [[nodiscard]] std::optional<std::string> save_path() const;
    [[nodiscard]] std::vector<std::uint32_t> video_frame() const;
    void run_frame();
    void set_keys(std::uint32_t keys);
    void reset(bool hard = true);
    [[nodiscard]] std::size_t state_size() const;
    [[nodiscard]] std::vector<std::uint8_t> save_state() const;
    void load_state(const std::vector<std::uint8_t>& state);
    [[nodiscard]] unsigned audio_sample_rate() const;
    [[nodiscard]] unsigned audio_channels() const;
    [[nodiscard]] std::vector<std::int16_t> read_audio_samples(std::size_t max_samples);

    std::vector<std::uint8_t> read_memory(std::uint32_t address, std::size_t size, const std::string& domain);
    void write_memory(std::uint32_t address, const std::vector<std::uint8_t>& value, const std::string& domain);

private:
    template <typename Fn>
    auto dispatch(Fn&& fn) -> std::invoke_result_t<Fn>;

    void owner_loop();
    void destroy_core();
    void rebuild_domains();
    [[nodiscard]] const NesDomainInfo& require_domain(const std::string& domain) const;
    [[nodiscard]] NesDomainInfo& require_domain(const std::string& domain);
    [[nodiscard]] std::string compute_crc32_hash() const;

    quickerNES::Emu* core_ = nullptr;
    std::unordered_map<std::string, NesDomainInfo> domains_;
    std::vector<std::uint8_t> rom_data_;
    std::string rom_path_;
    std::optional<std::string> save_path_;
    std::uint32_t keys_ = 0U;
    std::uint64_t frame_count_ = 0U;
    unsigned audio_sample_rate_ = 44100U;
    unsigned audio_channels_ = 1U;
    std::atomic<bool> owner_stop_requested_{false};
    std::thread owner_thread_;
    std::thread::id owner_thread_id_{};
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::function<void()>> queue_;
};

template <typename Fn>
auto NesRuntime::dispatch(Fn&& fn) -> std::invoke_result_t<Fn> {
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
