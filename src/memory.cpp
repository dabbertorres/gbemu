#include "memory.hpp"

#include <cstdio>
#include <ios>

namespace gb
{

memory::memory(std::unique_ptr<memory_bank_controller> controller, cartridge& cart)
    : controller{std::move(controller)}
    , cart{cart}
    , vram{}
    , wram_bank_0{}
    , wram_bank_n{}
    , io_registers{}
    , stack{}
    , interrupt_enable_register{}
{}

uint8_t memory::read(uint16_t addr) noexcept
{
    if (addr < rom_bank_0_end)
    {
        // 0x50 == disable_boot_rom register
        if (addr < boot_rom_end && io_registers[0x50] == 0) return bootstrap_rom[addr];

        return cart.data[addr];
    }

    if (addr < rom_bank_n_end) return controller->read(addr);
    if (addr < vram_end) return vram[addr - rom_bank_n_end];
    if (addr < ext_ram_end) return controller->read(addr);
    if (addr < wram_0_end) return wram_bank_0[addr - ext_ram_end];
    if (addr < wram_n_end) return wram_bank_n[addr - wram_0_end];
    if (addr < mirror_0_end) return wram_bank_0[addr - wram_n_end];
    if (addr < mirror_n_end) return wram_bank_n[addr - mirror_0_end];
    if (addr < oam_end) return 0;         // TODO: if NOT color, return 0, otherwise... TODO
    if (addr < oam_invalid_end) return 0; // TODO
    if (addr < io_registers_end) return io_registers[addr - oam_invalid_end];
    if (addr < stack_end) return stack[addr - io_registers_end];

    return interrupt_enable_register;
}

uint16_t memory::read16(uint16_t addr) noexcept
{
    return (static_cast<uint16_t>(read(addr + 1)) << 8) | static_cast<uint16_t>(read(addr));
}

void memory::write(uint16_t addr, uint8_t val) noexcept
{
    if (addr < rom_bank_n_end)
    {
        controller->write(addr, val);
        return;
    }

    if (addr < vram_end)
    {
        vram[addr - rom_bank_n_end] = val;
        return;
    }

    if (addr < ext_ram_end)
    {
        controller->write(addr, val);
        return;
    }

    if (addr < wram_0_end)
    {
        wram_bank_0[addr - ext_ram_end] = val;
        return;
    }

    if (addr < wram_n_end)
    {
        wram_bank_n[addr - wram_0_end] = val;
        return;
    }

    if (addr < mirror_0_end)
    {
        wram_bank_0[addr - wram_n_end] = val;
        return;
    }

    if (addr < mirror_n_end)
    {
        wram_bank_n[addr - mirror_0_end] = val;
        return;
    }

    if (addr < oam_end)
    {
        // TODO: if not color, ignore write.
        return;
    }

    if (addr < oam_invalid_end)
    {
        // TODO
        return;
    }

    if (addr < io_registers_end)
    {
        io_registers[addr - oam_invalid_end] = val;
        return;
    }

    if (addr < stack_end)
    {
        stack[addr - io_registers_end] = val;
        return;
    }

    interrupt_enable_register = val;
}

void memory::write16(uint16_t addr, uint16_t val) noexcept
{
    write(addr, (val & 0x00ff) >> 0);
    write(addr, (val & 0xff00) >> 8);
}

}
