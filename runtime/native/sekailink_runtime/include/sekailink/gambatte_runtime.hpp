#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gambatte {
class GB;
}

namespace sekailink {

class GambatteRuntime {
public:
    GambatteRuntime();
    ~GambatteRuntime();

    void load_rom(const std::string& rom_path, const std::optional<std::string>& bios_path);
    void set_save_path(std::optional<std::string> save_path);
    [[nodiscard]] std::optional<std::string> save_path() const;
    void start();
    void stop();

    void run_frame();
    void set_keys(std::uint32_t keys);
    void reset();
    [[nodiscard]] std::string rom_hash() const;
    [[nodiscard]] std::uint64_t frame_count() const;
    [[nodiscard]] std::size_t memory_size(const std::string& domain) const;

    [[nodiscard]] std::string system() const;
    [[nodiscard]] std::vector<std::uint32_t> video_frame() const;
    [[nodiscard]] std::vector<std::int16_t> read_audio_frames();
    [[nodiscard]] unsigned audio_sample_rate() const;
    [[nodiscard]] unsigned audio_channels() const;

    [[nodiscard]] std::size_t state_size() const;
    [[nodiscard]] std::vector<std::uint8_t> save_state() const;
    void load_state(const std::vector<std::uint8_t>& state);
    bool save_state(const std::string& path);
    bool load_state(const std::string& path);
    bool flush_save();

    std::vector<std::uint8_t> read_memory(std::uint32_t address, std::size_t size, const std::string& domain);
    void write_memory(std::uint32_t address, const std::vector<std::uint8_t>& value, const std::string& domain);

private:
    static unsigned input_getter(void* context);
    [[nodiscard]] unsigned load_flags() const;
    void load_savedata();
    [[nodiscard]] std::string compute_crc32_hash() const;

    std::string rom_path_;
    std::optional<std::string> bios_path_;
    std::optional<std::string> save_path_;
    std::uint32_t keys_ = 0;
    bool running_ = false;
    bool is_cgb_ = false;
    std::uint64_t frame_count_ = 0;
    std::vector<std::uint32_t> frame_buffer_;
    std::vector<std::uint32_t> raw_audio_buffer_;
    std::vector<std::int16_t> audio_frames_;
    gambatte::GB* core_ = nullptr;
};

}  // namespace sekailink
