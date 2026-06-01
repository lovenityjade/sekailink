#include "sekailink/ui_sdl.hpp"

#include <SDL.h>

#include <chrono>
#include <cstring>
#include <cstdint>
#include <thread>
#include <stdexcept>

#include <mgba/internal/gba/input.h>

namespace sekailink {

namespace {

double native_frame_rate(const GbaRuntime& runtime) {
    const auto system = runtime.system();
    if (system == "GB" || system == "GBC") {
        return 4194304.0 / 70224.0;
    }
    return 16777216.0 / 280896.0;
}

std::pair<int, int> native_video_size(const GbaRuntime& runtime) {
    const auto system = runtime.system();
    if (system == "GB" || system == "GBC") {
        return {160, 144};
    }
    return {240, 160};
}

std::uint32_t map_key(const SDL_Keycode key) {
    switch (key) {
    case SDLK_x:
        return 1U << GBA_KEY_A;
    case SDLK_z:
        return 1U << GBA_KEY_B;
    case SDLK_BACKSPACE:
        return 1U << GBA_KEY_SELECT;
    case SDLK_RETURN:
        return 1U << GBA_KEY_START;
    case SDLK_RIGHT:
        return 1U << GBA_KEY_RIGHT;
    case SDLK_LEFT:
        return 1U << GBA_KEY_LEFT;
    case SDLK_UP:
        return 1U << GBA_KEY_UP;
    case SDLK_DOWN:
        return 1U << GBA_KEY_DOWN;
    case SDLK_a:
        return 1U << GBA_KEY_L;
    case SDLK_s:
        return 1U << GBA_KEY_R;
    default:
        return 0;
    }
}

struct AudioCallbackContext {
    SDL_AudioStream* stream = nullptr;
};

void audio_callback(void* userdata, Uint8* stream, int len) {
    auto* context = static_cast<AudioCallbackContext*>(userdata);
    if (context == nullptr || context->stream == nullptr || len <= 0) {
        std::memset(stream, 0, static_cast<std::size_t>(std::max(len, 0)));
        return;
    }

    const int bytes_read = SDL_AudioStreamGet(context->stream, stream, len);
    if (bytes_read < 0) {
        std::memset(stream, 0, static_cast<std::size_t>(len));
        return;
    }
    if (bytes_read < len) {
        std::memset(stream + bytes_read, 0, static_cast<std::size_t>(len - bytes_read));
    }
}

}  // namespace

int run_sdl_frontend(GbaRuntime& runtime, const int scale, const std::atomic<bool>& stop_requested) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        throw std::runtime_error(SDL_GetError());
    }

    const unsigned source_channels = std::max<unsigned>(1U, runtime.audio_channels());
    const int source_rate = static_cast<int>(runtime.audio_sample_rate());
    AudioCallbackContext audio_context{};
    SDL_AudioSpec desired_audio{};
    desired_audio.freq = source_rate;
    desired_audio.format = AUDIO_S16SYS;
    desired_audio.channels = static_cast<Uint8>(source_channels);
    desired_audio.samples = 512;
    desired_audio.callback = &audio_callback;
    desired_audio.userdata = &audio_context;

    SDL_AudioSpec obtained_audio{};
    const SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(
        nullptr,
        0,
        &desired_audio,
        &obtained_audio,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device == 0) {
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }

    SDL_AudioStream* audio_stream = SDL_NewAudioStream(
        AUDIO_S16SYS,
        static_cast<Uint8>(source_channels),
        source_rate,
        obtained_audio.format,
        obtained_audio.channels,
        obtained_audio.freq);
    if (audio_stream == nullptr) {
        SDL_CloseAudioDevice(audio_device);
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    audio_context.stream = audio_stream;
    SDL_PauseAudioDevice(audio_device, 0);

    const auto [native_width, native_height] = native_video_size(runtime);
    const std::string window_title = std::string("SekaiEmu ") + runtime.system() + " Test";
    SDL_Window* window = SDL_CreateWindow(
        window_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        native_width * scale,
        native_height * scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        SDL_FreeAudioStream(audio_stream);
        SDL_CloseAudioDevice(audio_device);
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        SDL_FreeAudioStream(audio_stream);
        SDL_CloseAudioDevice(audio_device);
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        native_width,
        native_height);
    if (texture == nullptr) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_FreeAudioStream(audio_stream);
        SDL_CloseAudioDevice(audio_device);
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }

    std::uint32_t keys = 0;
    bool running = true;
    using clock = std::chrono::steady_clock;
    const auto frame_duration = std::chrono::duration<double>(1.0 / native_frame_rate(runtime));
    auto next_frame_time = clock::now();
    while (running && !stop_requested.load()) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                const std::uint32_t mask = map_key(event.key.keysym.sym);
                if (mask != 0) {
                    if (event.type == SDL_KEYDOWN) {
                        keys |= mask;
                    } else {
                        keys &= ~mask;
                    }
                    runtime.set_keys(keys);
                }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                    break;
                }
            }
        }

        if (!runtime.locked()) {
            runtime.run_frame();
            auto audio_frames = runtime.read_audio_frames(4096);
            if (!audio_frames.empty()) {
                SDL_LockAudioDevice(audio_device);
                const int queued_bytes = SDL_AudioStreamAvailable(audio_stream);
                const int max_buffered_bytes = std::max<int>(
                    4096,
                    (obtained_audio.freq * obtained_audio.channels * static_cast<int>(sizeof(std::int16_t))) / 10);
                if (queued_bytes > max_buffered_bytes * 2) {
                    SDL_AudioStreamClear(audio_stream);
                }
                if (SDL_AudioStreamPut(
                        audio_stream,
                        audio_frames.data(),
                        static_cast<int>(audio_frames.size() * sizeof(std::int16_t))) < 0) {
                    SDL_UnlockAudioDevice(audio_device);
                    throw std::runtime_error(SDL_GetError());
                }
                SDL_UnlockAudioDevice(audio_device);
            }
        }
        next_frame_time += std::chrono::duration_cast<clock::duration>(frame_duration);
        const auto frame = runtime.video_frame();
        SDL_UpdateTexture(texture, nullptr, frame.data(), native_width * static_cast<int>(sizeof(std::uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        const auto now = clock::now();
        if (next_frame_time > now) {
            std::this_thread::sleep_until(next_frame_time);
        } else if (now - next_frame_time > frame_duration * 4.0) {
            next_frame_time = now;
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_FreeAudioStream(audio_stream);
    SDL_CloseAudioDevice(audio_device);
    SDL_Quit();
    return 0;
}

}  // namespace sekailink
