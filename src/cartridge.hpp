#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gb
{

struct cartridge
{
    enum class color_support
    {
        monochrome_supported,
        color_only,
    };

    enum class memory_bank_controller : int8_t
    {
        none, // aka "ROM only"
        mbc1,
        mbc2,
        mmm01,
        mbc3,
        mbc5,
        mbc6,
        mbc7,
        pocket_camera,
        bandai_tama5,
        huc3,
        huc1,
    };

    enum class additional_hardware : uint8_t
    {
        ram           = 1 << 0,
        battery       = 1 << 1,
        timer         = 1 << 2,
        rumble        = 1 << 3,
        accelerometer = 1 << 4,
    };

    struct type
    {
        memory_bank_controller controller;
        additional_hardware    hardware;
    };

    [[nodiscard]] bool loaded() const noexcept;

    [[nodiscard]] std::array<uint8_t, 4>    entry_point() const noexcept;
    [[nodiscard]] std::array<uint8_t, 0x30> nintendo_logo() const noexcept;
    [[nodiscard]] bool                      nintendo_logo_valid() const noexcept;
    [[nodiscard]] std::string               title() const noexcept;
    /* [[nodiscard]] std::string               manufacturer_code() const noexcept; */
    [[nodiscard]] color_support color_flag() const noexcept;
    [[nodiscard]] std::string   licensee_code() const noexcept;
    [[nodiscard]] bool          supports_super_functions() const noexcept;
    [[nodiscard]] type          describe_type() const noexcept;
    [[nodiscard]] size_t        num_rom_banks() const noexcept;
    [[nodiscard]] size_t        rom_size() const noexcept;
    [[nodiscard]] size_t        num_ram_banks() const noexcept;
    [[nodiscard]] size_t        ram_size() const noexcept;
    [[nodiscard]] bool          japan_only() const noexcept;
    [[nodiscard]] uint8_t       rom_version() const noexcept;
    [[nodiscard]] bool          header_checksum_valid(uint8_t* actual) const noexcept;
    [[nodiscard]] bool          global_checksum_valid(uint16_t* actual) const noexcept;

    std::vector<uint8_t> data;
};

}

inline bool operator&(gb::cartridge::additional_hardware lhs, gb::cartridge::additional_hardware rhs) noexcept
{
    return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
}
