#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>

#include "cartridge.hpp"
#include "memory_bank_controller.hpp"

namespace gb
{

struct memory
{
public:
    // memory mapped registers
    static constexpr uint16_t joypad_input         = 0xFF00;
    static constexpr uint16_t serial_transfer_data = 0xFF01;
    static constexpr uint16_t serial_transfer_ctrl = 0xFF02;

    static constexpr uint16_t divider       = 0xFF04; // aka DIV
    static constexpr uint16_t timer_counter = 0xFF05; // aka TIMA
    static constexpr uint16_t timer_modulo  = 0xFF06; // aka TMA
    static constexpr uint16_t timer_control = 0xFF07; // aka TAC

    static constexpr uint16_t interrupt_flag = 0xFF0F;

    static constexpr uint16_t sound_start = 0xFF10;
    static constexpr uint16_t sound_end   = 0xFF26;

    static constexpr uint16_t wave_pattern_start = 0xFF30;
    static constexpr uint16_t wave_pattern_end   = 0xFF3F;

    static constexpr uint16_t lcd_control      = 0xFF40; // aka LCDC
    static constexpr uint16_t stat             = 0xFF41; // aka STAT
    static constexpr uint16_t screen_y         = 0xFF42; // aka SY
    static constexpr uint16_t screen_x         = 0xFF43; // aka SX
    static constexpr uint16_t ly               = 0xFF44; // aka LY
    static constexpr uint16_t lyc              = 0xFF45; // aka LYC
    static constexpr uint16_t dma              = 0xFF46; // aka DMA
    static constexpr uint16_t bgp              = 0xFF47; // aka BGP
    static constexpr uint16_t object_pallete_0 = 0xFF48; // aka OBP0
    static constexpr uint16_t object_pallete_1 = 0xFF49; // aka OBP1
    static constexpr uint16_t window_y         = 0xFF4A; // aka WY
    static constexpr uint16_t window_x         = 0xFF4B; // aka WX

    static constexpr uint16_t key1 = 0xFF4D;

    static constexpr uint16_t vram_bank_key = 0xFF4F; // aka VBK

    static constexpr uint16_t disable_boot_rom = 0xFF50;

    static constexpr uint16_t vram_dma_start = 0xFF51; // aka HDMA1
    static constexpr uint16_t vram_dma_end   = 0xFF55; // aka HDMA5

    static constexpr uint16_t infrared_port = 0xFF56; // aka RP

    static constexpr uint16_t bg_palette_index = 0xFF68; // aka BCPS
    static constexpr uint16_t bg_palette_data  = 0xFF69; // aka BCPD

    static constexpr uint16_t obj_palette_index = 0xFF6A; // aka OCPS
    static constexpr uint16_t obj_palette_data  = 0xFF6B; // aka OCPD

    static constexpr uint16_t wram_bank_select = 0xFF70; // aka SVBK

    static constexpr uint16_t interrupt_enable = 0xFFFF; // aka IE

    memory(std::unique_ptr<memory_bank_controller> controller, cartridge& cart);

    uint8_t  read(uint16_t addr) noexcept;
    uint16_t read16(uint16_t addr) noexcept;
    void     write(uint16_t addr, uint8_t val) noexcept;
    void     write16(uint16_t addr, uint16_t val) noexcept;

private:
    // 0000 - 3FFF: 16 KiB ROM bank 00: from cartridge, usually a fixed bank
    // 4000 - 7FFF: 16 KiB ROM bank 01-NN: from cartridge, switch bank via mapper (if any)
    // 8000 - 9FFF: 8 KiB Video RAM (VRAM): in CGB mode, switchable bank 0/1
    // A000 - BFFF: 8 KiB External RAM: From cartridge, switchable bank 1-7
    // C000 - CFFF: 4 KiB Work RAM (WRAM)
    // D000 - DFFF: 4 KiB Work RAM (WRAM): In CGB mode, switchable bank 1-7
    // E000 - FDFF: Mirror of 0xC000-0xDFFF: Nintendo says use is prohibited
    // FE00 - FE9F: Sprite Attribute Table aka OAM (Object Attribute Memory)
    // FEA0 - FEFF: Not Usable: Nintendo says use is prohibited
    // FF00 - FF7F: I/O Registers
    // FF80 - FFFE: Stack
    // FFFF: Interrupt Enable Register (IE)

    // Jump Vectors in first ROM bank (can be used for other uses though):
    // RST: 0000, 0008, 0010, 0018, 0020, 0028, 0030, 0038
    // Interrupts: 0040, 0048, 0050, 0058, 0060

    static constexpr uint16_t boot_rom_end     = 0x0100;
    static constexpr uint16_t rom_bank_0_end   = 0x4000;
    static constexpr uint16_t rom_bank_n_end   = 0x8000;
    static constexpr uint16_t vram_end         = 0xA000;
    static constexpr uint16_t ext_ram_end      = 0xC000;
    static constexpr uint16_t wram_0_end       = 0xD000;
    static constexpr uint16_t wram_n_end       = 0xE000;
    static constexpr uint16_t mirror_0_end     = 0xF000;
    static constexpr uint16_t mirror_n_end     = 0xFE00;
    static constexpr uint16_t oam_end          = 0xFEA0;
    static constexpr uint16_t oam_invalid_end  = 0xFF00;
    static constexpr uint16_t io_registers_end = 0xFF80;
    static constexpr uint16_t stack_end        = 0xFFFF;

    std::unique_ptr<memory_bank_controller> controller;
    cartridge&                              cart;
    std::array<uint8_t, 0x2000>             vram; // TODO: switchable in color
    std::array<uint8_t, 0x1000>             wram_bank_0;
    std::array<uint8_t, 0x1000>             wram_bank_n; // TODO: switchable in color
    // TODO Sprite Attribute Table                       //
    // TODO "Invalid" Sprite Attribute Table
    std::array<uint8_t, 0x80> io_registers;
    std::array<uint8_t, 0x7F> stack;
    uint8_t                   interrupt_enable_register;

    // clang-format off
    static constexpr std::array<uint8_t, 0x100> bootstrap_rom = {
        0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32,
        0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
        0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3,
        0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
        0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a,
        0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
        0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06,
        0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9,
        0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99,
        0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,
        0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64,
        0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
        0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90,
        0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
        0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62,
        0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,
        0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42,
        0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
        0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04,
        0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
        0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9,
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d,
        0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
        0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99,
        0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
        0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
        0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
        0x21, 0x04, 0x01, 0x11, 0xa8, 0x00, 0x1a, 0x13,
        0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
        0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20,
        0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x01, 0xe0, 0x50,
    };
    // clang-format on
};

}
