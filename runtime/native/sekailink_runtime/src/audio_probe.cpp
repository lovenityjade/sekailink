#include <algorithm>
#include <array>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/interface.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/vfs.h>

namespace {

struct Args {
    std::string rom;
    int frames = 600;
    bool skip_bios = true;
};

Args parse_args(const std::span<char*> argv) {
    Args args;
    for (std::size_t i = 1; i < argv.size(); ++i) {
        const std::string_view arg = argv[i];
        auto take = [&](const char* name) {
            if (i + 1 >= argv.size()) {
                throw std::runtime_error(std::string("missing_value_for_") + name);
            }
            return std::string(argv[++i]);
        };
        if (arg == "--rom") {
            args.rom = take("rom");
        } else if (arg == "--frames") {
            args.frames = std::stoi(take("frames"));
        } else if (arg == "--no-skip-bios") {
            args.skip_bios = false;
        } else {
            throw std::runtime_error("unknown_argument");
        }
    }
    if (args.rom.empty()) {
        throw std::runtime_error("missing_required_argument:--rom");
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto args = parse_args(std::span<char*>(argv, static_cast<std::size_t>(argc)));
        mCore* core = mCoreFind(args.rom.c_str());
        if (core == nullptr || !core->init(core)) {
            throw std::runtime_error("core_init_failed");
        }
        mCoreInitConfig(core, nullptr);
        mCoreConfigSetOverrideIntValue(&core->config, "useBios", 0);
        mCoreConfigSetOverrideIntValue(&core->config, "skipBios", args.skip_bios ? 1 : 0);
        mCoreConfigSetOverrideIntValue(&core->config, "volume", 0x100);
        mCoreConfigSetOverrideIntValue(&core->config, "mute", 0);
        mCoreConfigSetOverrideIntValue(&core->config, "audioSync", 0);
        mCoreConfigSetOverrideIntValue(&core->config, "sampleRate", 48000);
        mCoreConfigSetOverrideIntValue(&core->config, "audioBuffers", 2048);
        mCoreLoadConfig(core);
        core->setAudioBufferSize(core, 2048);

        unsigned width = 0;
        unsigned height = 0;
        core->baseVideoSize(core, &width, &height);
        std::vector<std::uint32_t> frame(static_cast<std::size_t>(width) * height, 0U);
        core->setVideoBuffer(core, frame.data(), width);

        struct ProbeStream {
            mAVStream d{};
            std::size_t count = 0;
            std::size_t non_zero = 0;
            std::int16_t peak = 0;
        } stream;
        stream.d.postAudioFrame = [](mAVStream* av, int16_t left, int16_t right) {
            auto* self = reinterpret_cast<ProbeStream*>(av);
            self->count += 2;
            if (left != 0) {
                ++self->non_zero;
                self->peak = std::max<std::int16_t>(self->peak, static_cast<std::int16_t>(std::abs(left)));
            }
            if (right != 0) {
                ++self->non_zero;
                self->peak = std::max<std::int16_t>(self->peak, static_cast<std::int16_t>(std::abs(right)));
            }
        };
        core->setAVStream(core, &stream.d);

        VFile* rom = VFileOpen(args.rom.c_str(), O_RDONLY);
        if (rom == nullptr || !core->loadROM(core, rom)) {
            if (rom) {
                rom->close(rom);
            }
            core->deinit(core);
            throw std::runtime_error("gba_rom_load_failed");
        }
        core->reset(core);
        for (int i = 0; i < args.frames; ++i) {
            core->runFrame(core);
        }
        auto* buffer = core->getAudioBuffer(core);
        const std::size_t available_frames = buffer ? mAudioBufferAvailable(buffer) : 0U;
        std::vector<std::int16_t> samples(available_frames * (buffer ? buffer->channels : 2U));
        if (buffer && !samples.empty()) {
            const auto frames_read = mAudioBufferRead(buffer, samples.data(), available_frames);
            samples.resize(frames_read * buffer->channels);
        }
        mCoreConfigDeinit(&core->config);
        core->deinit(core);

        std::size_t non_zero = 0;
        std::int16_t peak = 0;
        for (const auto sample : samples) {
            if (sample != 0) {
                ++non_zero;
                peak = std::max<std::int16_t>(peak, static_cast<std::int16_t>(std::abs(sample)));
            }
        }

        std::uint32_t video_non_zero = 0;
        for (const auto pixel : frame) {
            if (pixel != 0U) {
                ++video_non_zero;
            }
        }

        std::cout
            << "{"
            << "\"sampleCount\":" << samples.size() << ","
            << "\"nonZero\":" << non_zero << ","
            << "\"peak\":" << peak << ","
            << "\"videoNonZero\":" << video_non_zero << ","
            << "\"streamSampleCount\":" << stream.count << ","
            << "\"streamNonZero\":" << stream.non_zero << ","
            << "\"streamPeak\":" << stream.peak
            << "}\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
