#include "cartridge.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <string>

namespace gb
{

cartridge::additional_hardware operator|(cartridge::additional_hardware lhs,
                                         cartridge::additional_hardware rhs) noexcept
{
    using additional_hardware = cartridge::additional_hardware;

    return static_cast<additional_hardware>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

bool cartridge::loaded() const noexcept { return data.size() >= 0x150; }

std::array<uint8_t, 4> cartridge::entry_point() const noexcept
{
    return {
        data[0x100],
        data[0x101],
        data[0x102],
        data[0x103],
    };
}

std::array<uint8_t, 0x30> cartridge::nintendo_logo() const noexcept
{
    std::array<uint8_t, 0x30> out{};
    std::copy(data.begin() + 0x104, data.begin() + 0x133, out.begin());
    return out;
}

bool cartridge::nintendo_logo_valid() const noexcept
{
    static constexpr auto expect = std::to_array<uint8_t>({
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
    });

    const auto actual = nintendo_logo();

    return std::equal(actual.begin(), actual.end(), expect.begin());
}

std::string cartridge::title() const noexcept
{
    std::string title(data.begin() + 0x134, data.begin() + 0x143);

    auto idx = title.find_last_not_of(static_cast<char>(0));
    if (idx != std::string::npos) title = title.substr(0, idx + 1);

    return title;
}

/* std::string cartridge::manufacturer_code() const noexcept */
/* { */
/*     std::string title(data.begin() + 0x134, data.begin() + 0x143); */

/*     return title; */
/* } */

cartridge::color_support cartridge::color_flag() const noexcept
{
    const auto flag = data[0x143];

    if (flag == 0xC0) return color_support::color_only;
    return color_support::monochrome_supported;
}

std::string cartridge::licensee_code() const noexcept
{
    const auto old = data[0x14B];
    if (old == 0x33) return {data.begin() + 0x144, data.begin() + 0x145};

    return {static_cast<char>(old)};
}

bool cartridge::supports_super_functions() const noexcept
{
    const auto flag = data[0x146];
    return flag == 0x03;
}

cartridge::type cartridge::describe_type() const noexcept
{
    const auto code = data[0x147];

    using enum memory_bank_controller;
    using enum additional_hardware;

    switch (code)
    {
    case 0x00: return {none};
    case 0x01: return {mbc1};
    case 0x02: return {mbc1, ram};
    case 0x03: return {mbc1, ram | battery};
    case 0x05: return {mbc2};
    case 0x06: return {mbc2, ram | battery};
    case 0x08: return {none, ram};
    case 0x09: return {none, ram | battery};
    case 0x0B: return {mmm01};
    case 0x0C: return {mmm01, ram};
    case 0x0D: return {mmm01, ram | battery};
    case 0x0F: return {mbc3, timer | battery};
    case 0x10: return {mbc3, ram | timer | battery};
    case 0x11: return {mbc3};
    case 0x12: return {mbc3, ram};
    case 0x13: return {mbc3, ram | battery};
    case 0x19: return {mbc5};
    case 0x1A: return {mbc5, ram};
    case 0x1B: return {mbc5, ram | battery};
    case 0x1C: return {mbc5, rumble};
    case 0x1D: return {mbc5, ram | rumble};
    case 0x1E: return {mbc5, ram | battery | rumble};
    case 0x20: return {mbc6, ram | battery};
    case 0x22: return {mbc7, ram | battery | accelerometer};
    case 0xFC: return {pocket_camera};
    case 0xFD: return {bandai_tama5};
    case 0xFE: return {huc3};
    case 0xFF: return {huc1, ram | battery};
    default: return {static_cast<memory_bank_controller>(code)};
    }
}

size_t cartridge::num_rom_banks() const noexcept
{
    const auto shift = data[0x148];
    return 2 << shift;
}

size_t cartridge::rom_size() const noexcept
{
    const auto banks = num_rom_banks();
    return 0x8000 << banks;
}

size_t cartridge::num_ram_banks() const noexcept
{
    const auto code = data[0x149];
    switch (code)
    {
    case 0: return 0;

    case 1:
    case 2: return 1;

    case 3: return 4;
    case 4: return 16;
    case 5: return 8;

    default: return 0;
    }
}

size_t cartridge::ram_size() const noexcept
{
    const auto code = data[0x149];
    switch (code)
    {
    case 0: return 0;
    case 1: return 0x0800UL;
    case 2: return 0x2000UL;
    case 3: return 0x2000UL * 4;
    case 4: return 0x2000UL * 16;
    case 5: return 0x2000UL * 8;
    default: return 0;
    }
}

bool cartridge::japan_only() const noexcept { return data[0x14A] == 0; }

uint8_t cartridge::rom_version() const noexcept { return data[0x14B]; }

bool cartridge::header_checksum_valid(uint8_t* actual) const noexcept
{
    uint8_t sum = 0;

    for (size_t i = 0x0134; i <= 0x014C; ++i)
    {
        sum = sum - data[i] - 1;
    }

    const uint8_t expect = data[0x014D];
    if (actual != nullptr) *actual = sum;

    return sum == expect;
}

bool cartridge::global_checksum_valid(uint16_t* actual) const noexcept
{
    uint16_t sum = 0;
    for (uint8_t v : data)
    {
        sum += v;
    }

    sum -= data[0x014E] + data[0x014F];
    if (actual != nullptr) *actual = sum;

    const uint16_t expect = (static_cast<uint16_t>(data[0x014E]) << 8) | data[0x014F];
    return sum == expect;
}

}
