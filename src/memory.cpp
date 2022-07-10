#include "memory.hpp"

namespace gb
{

memory::memory()
{
    std::copy(bootstrap_rom.begin(), bootstrap_rom.end(), space.begin());
}

void memory::load_rom(rom r) noexcept
{
    std::copy(r.data.begin(), r.data.end(), space.begin() + 0x100);
}

uint8_t memory::read(uint16_t addr) noexcept
{
    return space[addr];
}

uint16_t memory::read16(uint16_t addr) noexcept
{
    return (static_cast<uint16_t>(space[addr + 1]) << 8) | static_cast<uint16_t>(space[addr]);
}

void memory::write(uint16_t addr, uint8_t val) noexcept
{
    space[addr] = val;
}

void memory::write(uint16_t addr, uint16_t val) noexcept
{
    space[addr] = val;
    space[addr+1] = val >> 8;
}

}
