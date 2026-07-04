#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sekailink {

inline std::string base64_encode(const std::vector<std::uint8_t>& data) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2U) / 3U) * 4U);

    std::size_t index = 0;
    while (index + 2 < data.size()) {
        const std::uint32_t chunk = (static_cast<std::uint32_t>(data[index]) << 16U) |
            (static_cast<std::uint32_t>(data[index + 1]) << 8U) |
            static_cast<std::uint32_t>(data[index + 2]);
        out.push_back(table[(chunk >> 18U) & 0x3FU]);
        out.push_back(table[(chunk >> 12U) & 0x3FU]);
        out.push_back(table[(chunk >> 6U) & 0x3FU]);
        out.push_back(table[chunk & 0x3FU]);
        index += 3;
    }

    if (index < data.size()) {
        std::uint32_t chunk = static_cast<std::uint32_t>(data[index]) << 16U;
        if (index + 1 < data.size()) {
            chunk |= static_cast<std::uint32_t>(data[index + 1]) << 8U;
        }
        out.push_back(table[(chunk >> 18U) & 0x3FU]);
        out.push_back(table[(chunk >> 12U) & 0x3FU]);
        out.push_back(index + 1 < data.size() ? table[(chunk >> 6U) & 0x3FU] : '=');
        out.push_back('=');
    }

    return out;
}

inline std::vector<std::uint8_t> base64_decode(std::string_view input) {
    static constexpr std::uint8_t invalid = 0xFFU;
    static constexpr std::uint8_t table[256] = {
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, 62U, invalid, invalid, invalid, 63U,
        52U, 53U, 54U, 55U, 56U, 57U, 58U, 59U, 60U, 61U, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U,
        15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 25U, invalid, invalid, invalid, invalid, invalid,
        invalid, 26U, 27U, 28U, 29U, 30U, 31U, 32U, 33U, 34U, 35U, 36U, 37U, 38U, 39U, 40U,
        41U, 42U, 43U, 44U, 45U, 46U, 47U, 48U, 49U, 50U, 51U, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid,
        invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid, invalid
    };

    std::vector<std::uint8_t> out;
    out.reserve((input.size() / 4U) * 3U);

    std::uint32_t buffer = 0;
    int bits = 0;
    for (const unsigned char ch : input) {
        if (ch == '=') {
            break;
        }
        const std::uint8_t value = table[ch];
        if (value == invalid) {
            throw std::runtime_error("invalid_base64");
        }
        buffer = (buffer << 6U) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFFU));
        }
    }

    return out;
}

}  // namespace sekailink

