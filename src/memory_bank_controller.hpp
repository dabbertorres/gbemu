#pragma once

#include <cstdint>

class memory_bank_controller
{
public:
    memory_bank_controller() = default;

    memory_bank_controller(const memory_bank_controller&)            = default;
    memory_bank_controller& operator=(const memory_bank_controller&) = default;

    memory_bank_controller(memory_bank_controller&&) noexcept            = default;
    memory_bank_controller& operator=(memory_bank_controller&&) noexcept = default;

    virtual ~memory_bank_controller() = default;

    virtual uint8_t read(uint16_t addr) noexcept = 0;
    /* virtual uint16_t read16(uint16_t addr) noexcept                = 0; */
    virtual void write(uint16_t addr, uint8_t val) noexcept = 0;
    /* virtual void     write16(uint16_t addr, uint16_t val) noexcept = 0; */
};
