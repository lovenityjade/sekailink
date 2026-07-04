#include "sekailink/nes_runtime.hpp"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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

constexpr std::uint32_t kNesA = 0x01U;
constexpr std::uint32_t kNesB = 0x02U;
constexpr std::uint32_t kNesSelect = 0x04U;
constexpr std::uint32_t kNesStart = 0x08U;
constexpr std::uint32_t kNesUp = 0x10U;
constexpr std::uint32_t kNesDown = 0x20U;
constexpr std::uint32_t kNesLeft = 0x40U;
constexpr std::uint32_t kNesRight = 0x80U;

std::uint32_t key_to_button(const SDL_Keycode key) {
    switch (key) {
    case SDLK_x:
        return kNesA;
    case SDLK_z:
        return kNesB;
    case SDLK_BACKSPACE:
        return kNesSelect;
    case SDLK_RETURN:
        return kNesStart;
    case SDLK_UP:
        return kNesUp;
    case SDLK_DOWN:
        return kNesDown;
    case SDLK_LEFT:
        return kNesLeft;
    case SDLK_RIGHT:
        return kNesRight;
    default:
        return 0U;
    }
}

std::string default_save_path(const std::string& rom_path) {
    std::filesystem::path path(rom_path);
    path.replace_extension(".sav");
    return path.string();
}

std::string default_state_path(const std::string& rom_path, const std::optional<std::string>& save_path) {
    if (save_path.has_value()) {
        return *save_path + ".state";
    }
    return rom_path + ".state";
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    try {
        const auto args = parse_args(std::span<char*>(argv, static_cast<std::size_t>(argc)));
        const std::string save_path = args.save.value_or(default_save_path(args.rom));
        const std::string state_path = default_state_path(args.rom, save_path);

        sekailink::NesRuntime runtime;
        runtime.set_save_path(save_path);
        runtime.load_rom(args.rom);
        runtime.set_keys(0U);
        runtime.start();

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
            throw std::runtime_error(std::string("sdl_init_failed:") + SDL_GetError());
        }

        constexpr int kWidth = 256;
        constexpr int kHeight = 240;
        SDL_Window* window = SDL_CreateWindow(
            "SekaiEmu NES Test",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            kWidth * args.scale,
            kHeight * args.scale,
            SDL_WINDOW_SHOWN);
        if (window == nullptr) {
            throw std::runtime_error(std::string("sdl_window_failed:") + SDL_GetError());
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            throw std::runtime_error(std::string("sdl_renderer_failed:") + SDL_GetError());
        }

        SDL_Texture* texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            kWidth,
            kHeight);
        if (texture == nullptr) {
            throw std::runtime_error(std::string("sdl_texture_failed:") + SDL_GetError());
        }

        SDL_AudioSpec desired{};
        desired.freq = static_cast<int>(runtime.audio_sample_rate());
        desired.format = AUDIO_S16SYS;
        desired.channels = static_cast<Uint8>(runtime.audio_channels());
        desired.samples = 2048;
        SDL_AudioSpec obtained{};
        const SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (audio_device == 0) {
            throw std::runtime_error(std::string("sdl_audio_failed:") + SDL_GetError());
        }

        SDL_AudioStream* audio_stream = SDL_NewAudioStream(
            AUDIO_S16SYS,
            desired.channels,
            desired.freq,
            obtained.format,
            obtained.channels,
            obtained.freq);
        if (audio_stream == nullptr) {
            throw std::runtime_error(std::string("sdl_audiostream_failed:") + SDL_GetError());
        }
        SDL_PauseAudioDevice(audio_device, 0);

        std::uint32_t keys = 0U;
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
                        runtime.reset();
                        continue;
                    }
                    if (event.key.keysym.sym == SDLK_F5) {
                        const auto state = runtime.save_state();
                        std::filesystem::create_directories(std::filesystem::path(state_path).parent_path());
                        std::ofstream output(state_path, std::ios::binary | std::ios::trunc);
                        output.write(reinterpret_cast<const char*>(state.data()), static_cast<std::streamsize>(state.size()));
                        continue;
                    }
                    if (event.key.keysym.sym == SDLK_F8) {
                        std::ifstream input(state_path, std::ios::binary);
                        if (!input) {
                            std::cerr << "[sekaiemu-nes-sdl] load_state_failed\n";
                            continue;
                        }
                        std::vector<std::uint8_t> state(
                            (std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
                        if (state.empty()) {
                            std::cerr << "[sekaiemu-nes-sdl] load_state_empty\n";
                            continue;
                        }
                        runtime.load_state(state);
                        continue;
                    }
                    keys |= key_to_button(event.key.keysym.sym);
                    runtime.set_keys(keys);
                } else if (event.type == SDL_KEYUP) {
                    keys &= ~key_to_button(event.key.keysym.sym);
                    runtime.set_keys(keys);
                }
            }

            if (!paused) {
                runtime.run_frame();
                const auto frame = runtime.video_frame();
                SDL_UpdateTexture(texture, nullptr, frame.data(), kWidth * static_cast<int>(sizeof(std::uint32_t)));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);

                const auto audio = runtime.read_audio_samples(8192);
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

                if (SDL_GetQueuedAudioSize(audio_device) >
                    static_cast<Uint32>(obtained.freq * obtained.channels * sizeof(std::int16_t) / 2)) {
                    SDL_ClearQueuedAudio(audio_device);
                    SDL_AudioStreamClear(audio_stream);
                }
            } else {
                SDL_Delay(10);
            }

            next_frame += frame_duration;
            std::this_thread::sleep_until(next_frame);
        }

        runtime.stop();
        SDL_FreeAudioStream(audio_stream);
        SDL_CloseAudioDevice(audio_device);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[sekaiemu-nes-sdl] " << ex.what() << '\n';
        return 1;
    }
}
