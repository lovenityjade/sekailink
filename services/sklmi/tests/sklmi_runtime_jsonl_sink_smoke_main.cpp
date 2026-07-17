#include "runtime_jsonl_sink.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::string Slurp(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool Bounded(const std::filesystem::path& path, std::uintmax_t max_bytes) {
    std::error_code ec;
    return !std::filesystem::exists(path, ec) || std::filesystem::file_size(path, ec) <= max_bytes;
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace sekailink::sklmi;

    const auto root = fs::temp_directory_path() / "sekailink-sklmi-jsonl-sink-smoke";
    const auto trace = root / "trace.jsonl";
    fs::remove_all(root);
    fs::create_directories(root);

    {
        std::ofstream oversized(trace, std::ios::binary | std::ios::trunc);
        oversized << std::string(4096, 'x');
    }

    {
        JsonlFileEventSink sink(trace, 1024, 128);
        if (!sink.good()) return EXIT_FAILURE;
        sink.trace({LogLevel::trace, "bridge", "tick", "tick_ok", 1, 16});
        sink.trace({LogLevel::info, "bridge", "start", "ready", 0, 0});
        sink.trace({LogLevel::warning, "bridge", "large", std::string(1024, 'y'), 0, 0});
    }

    const auto initial = Slurp(trace);
    if (initial.find("tick_ok") != std::string::npos ||
        initial.find(std::string(256, 'y')) != std::string::npos ||
        initial.find("record_omitted") == std::string::npos) {
        std::cerr << "runtime_jsonl_sink_filter_failed\n";
        return EXIT_FAILURE;
    }

    {
        JsonlFileEventSink sink(trace, 1024, 128);
        for (int index = 0; index < 80; ++index) {
            sink.emit({EventType::location_checked, "location", "test-value"});
        }
    }

    const auto current = Slurp(trace);
    const auto rotated = Slurp(trace.string() + ".1");
    if (!Bounded(trace, 1024) ||
        !Bounded(trace.string() + ".1", 1024)) {
        std::cerr << "runtime_jsonl_sink_bounds_failed\n";
        return EXIT_FAILURE;
    }

    fs::remove_all(root);
    std::cout << "sklmi_runtime_jsonl_sink_smoke_ok\n";
    return EXIT_SUCCESS;
}
