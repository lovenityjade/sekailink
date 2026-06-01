#include "sekailink/gba_runtime.hpp"
#include "sekailink/ui_sdl.hpp"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sekailink {
namespace {

std::atomic<bool> g_stop_requested{false};

void on_signal(const int) {
    g_stop_requested.store(true);
}

struct Args {
    std::string rom;
    std::optional<std::string> save;
    std::optional<std::string> bios;
    bool use_bios = false;
    bool skip_bios = true;
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
        } else if (arg == "--bios") {
            args.bios = take_value("bios");
        } else if (arg == "--use-bios") {
            args.use_bios = true;
            args.skip_bios = false;
        } else if (arg == "--skip-bios") {
            args.skip_bios = true;
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

}  // namespace
}  // namespace sekailink

int main(int argc, char** argv) {
    using namespace sekailink;

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    try {
        const auto args = parse_args(std::span<char*>(argv, static_cast<std::size_t>(argc)));
        GbaRuntime runtime;
        runtime.set_save_path(args.save);
        runtime.load_rom(args.rom, args.bios, args.use_bios, args.skip_bios);
        runtime.start();
        const int result = run_sdl_frontend(runtime, args.scale, g_stop_requested);
        runtime.stop();
        return result;
    } catch (const std::exception& ex) {
        std::cerr << "[sekaiemu-gba-sdl] " << ex.what() << '\n';
        return 1;
    }
}
