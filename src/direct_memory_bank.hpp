#pragma once

#include "cartridge.hpp"
#include "memory_bank_controller.hpp"

namespace gb
{

class direct_memory_bank : public memory_bank_controller
{
public:
    explicit direct_memory_bank(cartridge& cart);

    uint8_t read(uint16_t addr) noexcept override { return cart.data[addr]; }
    /* uint16_t read16(uint16_t addr) noexcept override; */
    void write(uint16_t addr, uint8_t val) noexcept override { cart.data[addr] = val; }
    /* void     write16(uint16_t addr, uint16_t val) noexcept override; */

private:
    cartridge& cart;
};

}
