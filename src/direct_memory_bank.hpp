#pragma once

#include <array>

#include "memory_bank_controller.hpp"

class direct_memory_bank : public memory_bank_controller
{
public:
    explicit direct_memory_bank(/* TODO data source */);

    uint8_t read(uint16_t addr) noexcept override { return rom[addr]; }
    /* uint16_t read16(uint16_t addr) noexcept override; */
    void write(uint16_t addr, uint8_t val) noexcept override { rom[addr] = val; }
    /* void     write16(uint16_t addr, uint16_t val) noexcept override; */

private:
    std::array<uint8_t, 0x8000> rom;
};
