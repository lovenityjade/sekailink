#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <target-libsnes/libsnes.hpp>
}

namespace {

std::atomic<bool> g_stop_requested{false};

void on_signal(const int) {
    g_stop_requested.store(true);
}

struct Args {
    std::string rom;
    std::optional<std::string> save;
    int scale = 4;
};

struct SharedState {
    std::vector<std::uint32_t> frame;
    unsigned width = 256;
    unsigned height = 224;
    std::mutex video_mutex;

    std::vector<std::int16_t> audio_samples;
    std::mutex audio_mutex;

    std::atomic<std::uint16_t> keys{0};
};

SharedState* g_state = nullptr;

Args parse_args(const std::span<char*> argv) {
    Args args;
    for (std::size_t index = 1; index < argv.size(); ++index) {
        const std::string_view arg = argv[index];
        auto take_value = [&](const std::string_view name) -> std::string {
            if (index + 1 >= argv.size()) {
                throw std::runtime_error("missing_value_for_" + std::string(name));
            }
            ++index;
            return argv[index];
        };

        if (arg == "--rom") {
            args.rom = take_value("rom");
        } else if (arg == "--save") {
            args.save = take_value("save");
        } else if (arg == "--scale") {
            args.scale = std::max(1, std::stoi(take_value("scale")));
        } else {
            throw std::runtime_error("unknown_argument:" + std::string(arg));
        }
    }

    if (args.rom.empty()) {
        throw std::runtime_error("missing_required_argument:--rom");
    }

    return args;
}

std::string default_save_path(const std::string& rom_path) {
    std::filesystem::path path(rom_path);
    path.replace_extension(".srm");
    return path.string();
}

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

void snes_video_refresh_cb(const std::uint32_t* data, unsigned width, unsigned height) {
    if (g_state == nullptr || data == nullptr) {
        return;
    }
    unsigned video_width = width;
    unsigned video_height = height;
    bool line_double = false;
    bool dot_double = false;
    int yskip = 1;
    int xskip = 1;
    int src_pitch = 1024;
    int src_start = 0;

    const bool interlaced = height == 478U || height == 448U;
    if (width == 512U) {
        video_height *= 2U;
        yskip = 2;
        line_double = true;
    }
    if (interlaced) {
        line_double = false;
        src_pitch = 512;
        yskip = 1;
        video_height = height;
    }
    if (dot_double) {
        video_width *= 2U;
        xskip = 2;
    }

    std::vector<std::uint32_t> packed(video_width * video_height, 0xff000000U);
    for (int j = 0; j < 2; ++j) {
        if (j == 1 && !dot_double) {
            break;
        }
        const int xbonus = j;
        for (int i = 0; i < 2; ++i) {
            if (i == 1 && !line_double) {
                break;
            }
            const int bonus = (i * static_cast<int>(video_width)) + xbonus;
            for (unsigned y = 0; y < height; ++y) {
                for (unsigned x = 0; x < width; ++x) {
                    const int si = (static_cast<int>(y) * src_pitch) + static_cast<int>(x) + src_start;
                    const int di =
                        static_cast<int>(y) * static_cast<int>(video_width) * yskip +
                        static_cast<int>(x) * xskip +
                        bonus;
                    if (di >= 0 && static_cast<std::size_t>(di) < packed.size()) {
                        packed[static_cast<std::size_t>(di)] = data[si];
                    }
                }
            }
        }
    }

    std::scoped_lock lock(g_state->video_mutex);
    g_state->width = video_width;
    g_state->height = video_height;
    g_state->frame = std::move(packed);
}

void snes_audio_sample_cb(std::uint16_t left, std::uint16_t right) {
    if (g_state == nullptr) {
        return;
    }
    std::scoped_lock lock(g_state->audio_mutex);
    g_state->audio_samples.push_back(static_cast<std::int16_t>(left));
    g_state->audio_samples.push_back(static_cast<std::int16_t>(right));
}

void snes_input_poll_cb() {
}

std::int16_t snes_input_state_cb(unsigned port, unsigned device, unsigned, unsigned id) {
    if (g_state == nullptr || port != 0 || device != SNES_DEVICE_JOYPAD) {
        return 0;
    }
    const auto keys = g_state->keys.load();
    return (keys & (1U << id)) != 0 ? 1 : 0;
}

void* snes_alloc_shared_cb(const char*, size_t amount) {
    void* pointer = std::calloc(1, amount);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}

void snes_free_shared_cb(void* pointer) {
    std::free(pointer);
}

std::vector<std::uint32_t> build_bizhawk_palette_lut() {
    std::vector<std::uint32_t> lut(16U * 32768U, 0xff000000U);
    for (int l = 0; l < 16; ++l) {
        for (int r = 0; r < 32; ++r) {
            for (int g = 0; g < 32; ++g) {
                for (int b = 0; b < 32; ++b) {
                    const int rr = (r * l * 17 + 15) / 31;
                    const int gg = (g * l * 17 + 15) / 31;
                    const int bb = (b * l * 17 + 15) / 31;
                    const std::uint32_t color =
                        0xff000000U |
                        (static_cast<std::uint32_t>(rr) << 16U) |
                        (static_cast<std::uint32_t>(gg) << 8U) |
                        static_cast<std::uint32_t>(bb);
                    const std::size_t index =
                        (static_cast<std::size_t>(l) << 15U) |
                        (static_cast<std::size_t>(b) << 10U) |
                        (static_cast<std::size_t>(g) << 5U) |
                        static_cast<std::size_t>(r);
                    lut[index] = color;
                }
            }
        }
    }
    return lut;
}

std::uint16_t key_to_button(const SDL_Keycode key) {
    switch (key) {
    case SDLK_z:
        return 1U << SNES_DEVICE_ID_JOYPAD_B;
    case SDLK_x:
        return 1U << SNES_DEVICE_ID_JOYPAD_A;
    case SDLK_a:
        return 1U << SNES_DEVICE_ID_JOYPAD_Y;
    case SDLK_s:
        return 1U << SNES_DEVICE_ID_JOYPAD_X;
    case SDLK_q:
        return 1U << SNES_DEVICE_ID_JOYPAD_L;
    case SDLK_w:
        return 1U << SNES_DEVICE_ID_JOYPAD_R;
    case SDLK_BACKSPACE:
        return 1U << SNES_DEVICE_ID_JOYPAD_SELECT;
    case SDLK_RETURN:
        return 1U << SNES_DEVICE_ID_JOYPAD_START;
    case SDLK_UP:
        return 1U << SNES_DEVICE_ID_JOYPAD_UP;
    case SDLK_DOWN:
        return 1U << SNES_DEVICE_ID_JOYPAD_DOWN;
    case SDLK_LEFT:
        return 1U << SNES_DEVICE_ID_JOYPAD_LEFT;
    case SDLK_RIGHT:
        return 1U << SNES_DEVICE_ID_JOYPAD_RIGHT;
    default:
        return 0U;
    }
}

void load_sram_if_present(const std::string& save_path) {
    auto* memory = snes_get_memory_data(SNES_MEMORY_CARTRIDGE_RAM);
    const auto size = snes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM);
    if (memory == nullptr || size == 0) {
        return;
    }
    const auto save = read_optional_binary_file(save_path);
    if (save.empty()) {
        return;
    }
    std::memcpy(memory, save.data(), std::min<std::size_t>(save.size(), size));
}

void flush_sram(const std::string& save_path) {
    auto* memory = snes_get_memory_data(SNES_MEMORY_CARTRIDGE_RAM);
    const auto size = snes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM);
    if (memory == nullptr || size == 0) {
        return;
    }
    std::filesystem::create_directories(std::filesystem::path(save_path).parent_path());
    std::ofstream output(save_path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(memory), static_cast<std::streamsize>(size));
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    try {
        const auto args = parse_args(std::span<char*>(argv, static_cast<std::size_t>(argc)));
        const std::string save_path = args.save.value_or(default_save_path(args.rom));

        auto rom_data = read_binary_file(args.rom);
        strip_smc_header(rom_data);

        SharedState state;
        g_state = &state;

        snes_init();
        snes_set_video_refresh(&snes_video_refresh_cb);
        snes_set_audio_sample(&snes_audio_sample_cb);
        snes_set_input_poll(&snes_input_poll_cb);
        snes_set_input_state(&snes_input_state_cb);
        snes_set_allocSharedMemory(&snes_alloc_shared_cb);
        snes_set_freeSharedMemory(&snes_free_shared_cb);
        snes_set_controller_port_device(false, SNES_DEVICE_JOYPAD);
        snes_set_controller_port_device(true, SNES_DEVICE_NONE);

        static std::vector<std::uint32_t> palette_lut = build_bizhawk_palette_lut();
        snes_set_color_lut(palette_lut.data());

        if (!snes_load_cartridge_normal(nullptr, rom_data.data(), static_cast<unsigned>(rom_data.size()))) {
            throw std::runtime_error("snes_load_cartridge_normal_failed");
        }
        load_sram_if_present(save_path);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(std::string("sdl_init_failed:") + SDL_GetError());
        }

        SDL_Window* window = SDL_CreateWindow(
            "SekaiEmu SNES Test",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            256 * args.scale,
            224 * args.scale,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
            throw std::runtime_error(std::string("sdl_window_failed:") + SDL_GetError());
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            throw std::runtime_error(std::string("sdl_renderer_failed:") + SDL_GetError());
        }

        unsigned texture_width = 256;
        unsigned texture_height = 224;
        SDL_Texture* texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            static_cast<int>(texture_width),
            static_cast<int>(texture_height));
        if (texture == nullptr) {
            throw std::runtime_error(std::string("sdl_texture_failed:") + SDL_GetError());
        }

        SDL_AudioSpec desired{};
        desired.freq = 48000;
        desired.format = AUDIO_S16SYS;
        desired.channels = 2;
        desired.samples = 2048;
        SDL_AudioSpec obtained{};
        const SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (audio_device == 0) {
            throw std::runtime_error(std::string("sdl_audio_failed:") + SDL_GetError());
        }

        SDL_AudioStream* audio_stream = SDL_NewAudioStream(
            AUDIO_S16SYS,
            2,
            32040,
            obtained.format,
            obtained.channels,
            obtained.freq);
        if (audio_stream == nullptr) {
            throw std::runtime_error(std::string("sdl_audiostream_failed:") + SDL_GetError());
        }
        SDL_PauseAudioDevice(audio_device, 0);

        bool paused = false;
        auto next_frame = std::chrono::steady_clock::now();
        constexpr auto frame_duration = std::chrono::microseconds(16667);

        while (!g_stop_requested.load()) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    g_stop_requested.store(true);
                    break;
                }
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        g_stop_requested.store(true);
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_p) {
                        paused = !paused;
                        continue;
                    }
                    if (event.key.keysym.sym == SDLK_F1) {
                        snes_reset();
                        continue;
                    }
                    state.keys.fetch_or(key_to_button(event.key.keysym.sym));
                } else if (event.type == SDL_KEYUP) {
                    state.keys.fetch_and(static_cast<std::uint16_t>(~key_to_button(event.key.keysym.sym)));
                }
            }

            if (!paused) {
                snes_run();

                std::vector<std::uint32_t> frame;
                unsigned width = 256;
                unsigned height = 224;
                {
                    std::scoped_lock lock(state.video_mutex);
                    frame = state.frame;
                    width = state.width;
                    height = state.height;
                }

                if (!frame.empty()) {
                    if (width != texture_width || height != texture_height) {
                        SDL_DestroyTexture(texture);
                        texture = SDL_CreateTexture(
                            renderer,
                            SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING,
                            static_cast<int>(width),
                            static_cast<int>(height));
                        if (texture == nullptr) {
                            throw std::runtime_error(std::string("sdl_texture_recreate_failed:") + SDL_GetError());
                        }
                        texture_width = width;
                        texture_height = height;
                    }

                    SDL_UpdateTexture(texture, nullptr, frame.data(), static_cast<int>(width * sizeof(std::uint32_t)));
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_RenderPresent(renderer);
                }

                std::vector<std::int16_t> audio;
                {
                    std::scoped_lock lock(state.audio_mutex);
                    audio.swap(state.audio_samples);
                }
                if (!audio.empty()) {
                    SDL_AudioStreamPut(audio_stream, audio.data(), static_cast<int>(audio.size() * sizeof(std::int16_t)));
                }

                std::vector<std::int16_t> device_buffer(8192);
                while (SDL_AudioStreamAvailable(audio_stream) >= static_cast<int>(device_buffer.size() * sizeof(std::int16_t))) {
                    const int read = SDL_AudioStreamGet(
                        audio_stream,
                        device_buffer.data(),
                        static_cast<int>(device_buffer.size() * sizeof(std::int16_t)));
                    if (read <= 0) {
                        break;
                    }
                    SDL_QueueAudio(audio_device, device_buffer.data(), static_cast<Uint32>(read));
                }
                if (SDL_GetQueuedAudioSize(audio_device) > static_cast<Uint32>(obtained.freq * obtained.channels * sizeof(std::int16_t))) {
                    SDL_ClearQueuedAudio(audio_device);
                }
            }

            next_frame += frame_duration;
            std::this_thread::sleep_until(next_frame);
        }

        flush_sram(save_path);

        SDL_ClearQueuedAudio(audio_device);
        SDL_CloseAudioDevice(audio_device);
        SDL_FreeAudioStream(audio_stream);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        snes_unload_cartridge();
        snes_term();
        g_state = nullptr;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[sekaiemu-snes-sdl] " << ex.what() << '\n';
        return 1;
    }
}
