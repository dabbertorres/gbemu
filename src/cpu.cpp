#include "cpu.hpp"

#include <SDL2/SDL_log.h>

#include "instructions.hpp"
#include "memory.hpp"
#include "models.hpp"

namespace gb
{

constexpr uint32_t clock_rate = 4194304; // clock cycles per second / Hz

constexpr uint16_t vblank_handler   = 0x40;
constexpr uint16_t lcd_stat_handler = 0x48;
constexpr uint16_t timer_handler    = 0x50;
constexpr uint16_t serial_handler   = 0x58;
constexpr uint16_t joypad_handler   = 0x60;

cpu::cpu(std::unique_ptr<memory>&& mem, model model) noexcept
    : mem{std::move(mem)}
    , running{false}
    , interrupts_enabled{false}
    , cycles{0}
    , r{}
{
    initialize_registers(model, r, false /* TODO */);
    r.sp = 0xFFFE;
    r.pc = 0x100;

    mem->write(gb::memory::joypad_input, 0xCF);
    mem->write(gb::memory::serial_transfer_data, 0x00);
    mem->write(gb::memory::serial_transfer_ctrl, 0x7E);
    mem->write(gb::memory::divider, 0xAB);
    mem->write(gb::memory::timer_counter, 0x00);
    mem->write(gb::memory::timer_modulo, 0x00);
    mem->write(gb::memory::timer_control, 0xF8);
    mem->write(gb::memory::interrupt_flag, 0xE1);

    mem->write(0xFF10, 0x80);
    mem->write(0xFF11, 0xBF);
    mem->write(0xFF12, 0xF3);
    mem->write(0xFF13, 0xFF);
    mem->write(0xFF14, 0xBF);
    mem->write(0xFF16, 0x3F);
    mem->write(0xFF17, 0x00);
    mem->write(0xFF18, 0xFF);
    mem->write(0xFF19, 0xBF);
    mem->write(0xFF1A, 0x7F);
    mem->write(0xFF1B, 0xFF);
    mem->write(0xFF1C, 0x9F);
    mem->write(0xFF1D, 0xFF);
    mem->write(0xFF1E, 0xBF);
    mem->write(0xFF20, 0xFF);
    mem->write(0xFF21, 0x00);
    mem->write(0xFF22, 0x00);
    mem->write(0xFF23, 0xBF);
    mem->write(0xFF24, 0x77);
    mem->write(0xFF25, 0xF3);
    mem->write(0xFF26, 0xF1);

    mem->write(gb::memory::lcd_control, 0x91);
    mem->write(gb::memory::stat, 0x85);
    mem->write(gb::memory::screen_y, 0x00);
    mem->write(gb::memory::screen_x, 0x00);
    mem->write(gb::memory::ly, 0x00);
    mem->write(gb::memory::lyc, 0x00);
    mem->write(gb::memory::dma, 0xFF);
    mem->write(gb::memory::bgp, 0xFC);
    mem->write(gb::memory::object_pallete_0, 0x00);
    mem->write(gb::memory::object_pallete_1, 0x00);
    mem->write(gb::memory::window_y, 0x00);
    mem->write(gb::memory::window_x, 0x00);

    mem->write(gb::memory::key1, 0xFF);
    mem->write(gb::memory::vram_bank_key, 0xFF);

    mem->write(gb::memory::vram_dma_start + 0, 0xFF);
    mem->write(gb::memory::vram_dma_start + 1, 0xFF);
    mem->write(gb::memory::vram_dma_start + 2, 0xFF);
    mem->write(gb::memory::vram_dma_start + 3, 0xFF);
    mem->write(gb::memory::vram_dma_start + 4, 0xFF);

    mem->write(gb::memory::infrared_port, 0xFF);

    mem->write(gb::memory::bg_palette_index, 0xFF);
    mem->write(gb::memory::bg_palette_data, 0xFF);

    mem->write(gb::memory::obj_palette_index, 0xFF);
    mem->write(gb::memory::obj_palette_data, 0xFF);

    mem->write(gb::memory::wram_bank_select, 0xFF);

    mem->write(gb::memory::interrupt_enable, 0x00);
}

void cpu::run() noexcept
{
    pipeline.push(action::execute);
    running = true;

    while (running)
    {
        auto next = pipeline.top();
        pipeline.pop();

        switch (next)
        {
        case action::execute:
            cycles += execute(fetch()); // "Just do it"
            pipeline.push(action::execute);
            break;

        case action::halt:
            // halt mode is exited when a flag in register IF is set,
            // and the corresponding flag in IE is set, regardless of IME.
            // If IME = 1, the CPU will jump to the interrupt vector (and
            // clear the IF flag). If IME = 0, the CPU will simply continue
            // without jumping and clearing the IF flag.
            // TODO https://gbdev.io/pandocs/halt.html#halt-bug
            pipeline.push(action::halt);
            break;

        case action::disable_interrupts:
            // interrupts are disabled immediately (via op DI)
            pipeline.push(action::execute);
            break;

        case action::enable_interrupts:
            // interrupts are enabled AFTER the next execution cycle (with op EI)
            interrupts_enabled = true;
            pipeline.push(action::execute);
            break;
        }

        process_interrupts();
        update_lcd();
        update_timers();
    }
}

void cpu::stop() noexcept { running = false; }

void cpu::queue_interrupt(interrupt type) noexcept
{
    if (!interrupts_enabled) return;

    auto val = mem->read(memory::interrupt_flag);
    val |= static_cast<uint8_t>(type);
    mem->write(memory::interrupt_flag, val);
}

uint8_t cpu::fetch() noexcept
{
    auto ret = mem->read(r.pc);
    ++r.pc;
    return ret;
}

uint16_t cpu::fetch16() noexcept
{
    auto ret = mem->read16(r.pc);
    r.pc += 2;
    return ret;
}

void cpu::process_interrupts() noexcept
{
    if (!interrupts_enabled) return;

    uint16_t jump_addr = 0;

    auto requested = mem->read(memory::interrupt_flag);
    if ((requested & static_cast<uint8_t>(interrupt::vblank)) != 0) jump_addr = vblank_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::lcd_stat)) != 0) jump_addr = lcd_stat_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::timer)) != 0) jump_addr = timer_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::serial)) != 0) jump_addr = serial_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::joypad)) != 0) jump_addr = joypad_handler;

    if (jump_addr != 0)
    {
        interrupts_enabled = false;
        op_push(r.pc);
        r.pc = jump_addr;
    }
}

void cpu::update_lcd() noexcept
{
    constexpr uint8_t enabled            = 1U << 7U;
    constexpr uint8_t tile_map_select    = 1U << 6U;
    constexpr uint8_t window_enabled     = 1U << 5U;
    constexpr uint8_t tile_data_select   = 1U << 4U;
    constexpr uint8_t bg_tile_map_select = 1U << 3U;
    constexpr uint8_t obj_size_select    = 1U << 2U;
    constexpr uint8_t obj_enabled        = 1U << 1U;
    constexpr uint8_t bg_display         = 1U << 0U;

    auto val = mem->read(memory::lcd_control);
    if ((val & enabled) == 0) return;

    // TODO update to declared state
    queue_interrupt(interrupt::vblank);
}

void cpu::update_timers() noexcept
{
    constexpr uint32_t div_inc_rate   = 0x4000; // Hz
    constexpr uint32_t div_inc_cycles = clock_rate / div_inc_rate;

    constexpr uint8_t started      = 1U << 2U;
    constexpr uint8_t clock_select = 0x03;

    constexpr std::array<uint32_t, 4> clocks = {
        4096,
        262144,
        65536,
        16834,
    };

    // DIV clock is always running
    if (cycles >= div_inc_cycles)
    {
        auto curr = mem->read(memory::divider);
        curr++;
        mem->write(memory::divider, curr);
        cycles %= div_inc_cycles;
    }

    auto state = mem->read(memory::timer_control);
    if ((state & started) == 0) return;
}

uint32_t cpu::execute(uint8_t op) noexcept
{
    if (op != 0xCB) log_instruction(op);

    switch (op)
    {
    case 0x00: return op_nop();
    case 0x01: return op_ld16(r.BC);
    case 0x02: return op_ld(r.BC, r.A);
    case 0x03: return op_inc16(r.BC);
    case 0x04: return op_inc(r.B);
    case 0x05: return op_dec(r.B);
    case 0x06: return op_ld_n(r.B);
    case 0x07: return op_rlc(r.A);
    case 0x08: return op_ld16_nn();
    case 0x09: return op_add(r.HL, r.BC);
    case 0x0a: return op_ld(r.A, r.BC);
    case 0x0b: return op_dec16(r.BC);
    case 0x0c: return op_inc(r.C);
    case 0x0d: return op_dec(r.C);
    case 0x0e: return op_ld_n(r.C);
    case 0x0f: return op_rrc(r.A);
    case 0x10:
        switch (fetch())
        {
        case 0x00: return op_stop();
        default: return op_nop(); // TODO this actually would hang the CPU
        }
    case 0x11: return op_ld16(r.DE);
    case 0x12: return op_ld(r.DE, r.A);
    case 0x13: return op_inc16(r.DE);
    case 0x14: return op_inc(r.D);
    case 0x15: return op_dec(r.D);
    case 0x16: return op_ld_n(r.D);
    case 0x17: return op_rl(r.A);
    case 0x18: return op_jr();
    case 0x19: return op_add(r.HL, r.DE);
    case 0x1a: return op_ld(r.A, r.DE);
    case 0x1b: return op_dec16(r.DE);
    case 0x1c: return op_inc(r.E);
    case 0x1d: return op_dec(r.E);
    case 0x1e: return op_ld_n(r.E);
    case 0x1f: return op_rr(r.A);
    case 0x20: return op_jr(condition::NZ);
    case 0x21: return op_ld16(r.HL);
    case 0x22: return op_ldi_HL();
    case 0x23: return op_inc16(r.HL);
    case 0x24: return op_inc(r.H);
    case 0x25: return op_dec(r.H);
    case 0x26: return op_ld_n(r.H);
    case 0x27: return op_daa();
    case 0x28: return op_jr(condition::Z);
    case 0x29: return op_add(r.HL, r.HL);
    case 0x2a: return op_ldi_A();
    case 0x2b: return op_dec16(r.HL);
    case 0x2c: return op_inc(r.L);
    case 0x2d: return op_dec(r.L);
    case 0x2e: return op_ld_n(r.L);
    case 0x2f: return op_cpl();
    case 0x30: return op_jr(condition::NC);
    case 0x31: return op_ld16(r.sp);
    case 0x32: return op_ldd_HL();
    case 0x33: return op_inc16(r.sp);
    case 0x34: return op_inc(r.HL);
    case 0x35: return op_dec(r.HL);
    case 0x36: return op_ld_n(r.HL);
    case 0x37: return op_scf();
    case 0x38: return op_jr(condition::C);
    case 0x39: return op_add(r.HL, r.sp);
    case 0x3a: return op_ldd_A();
    case 0x3b: return op_dec16(r.sp);
    case 0x3c: return op_inc(r.A);
    case 0x3d: return op_dec(r.A);
    case 0x3e: return op_ld_n(r.A);
    case 0x3f: return op_ccf();
    case 0x40: return op_ld(r.B, r.B);
    case 0x41: return op_ld(r.B, r.C);
    case 0x42: return op_ld(r.B, r.D);
    case 0x43: return op_ld(r.B, r.E);
    case 0x44: return op_ld(r.B, r.H);
    case 0x45: return op_ld(r.B, r.L);
    case 0x46: return op_ld(r.B, r.HL);
    case 0x47: return op_ld(r.B, r.A);
    case 0x48: return op_ld(r.C, r.B);
    case 0x49: return op_ld(r.C, r.C);
    case 0x4a: return op_ld(r.C, r.D);
    case 0x4b: return op_ld(r.C, r.E);
    case 0x4c: return op_ld(r.C, r.H);
    case 0x4d: return op_ld(r.C, r.L);
    case 0x4e: return op_ld(r.C, r.HL);
    case 0x4f: return op_ld(r.C, r.A);
    case 0x50: return op_ld(r.D, r.B);
    case 0x51: return op_ld(r.D, r.C);
    case 0x52: return op_ld(r.D, r.D);
    case 0x53: return op_ld(r.D, r.E);
    case 0x54: return op_ld(r.D, r.H);
    case 0x55: return op_ld(r.D, r.L);
    case 0x56: return op_ld(r.D, r.HL);
    case 0x57: return op_ld(r.D, r.A);
    case 0x58: return op_ld(r.E, r.B);
    case 0x59: return op_ld(r.E, r.C);
    case 0x5a: return op_ld(r.E, r.D);
    case 0x5b: return op_ld(r.E, r.E);
    case 0x5c: return op_ld(r.E, r.H);
    case 0x5d: return op_ld(r.E, r.L);
    case 0x5e: return op_ld(r.E, r.HL);
    case 0x5f: return op_ld(r.E, r.A);
    case 0x60: return op_ld(r.H, r.B);
    case 0x61: return op_ld(r.H, r.C);
    case 0x62: return op_ld(r.H, r.D);
    case 0x63: return op_ld(r.H, r.E);
    case 0x64: return op_ld(r.H, r.H);
    case 0x65: return op_ld(r.H, r.L);
    case 0x66: return op_ld(r.H, r.HL);
    case 0x67: return op_ld(r.H, r.A);
    case 0x68: return op_ld(r.L, r.B);
    case 0x69: return op_ld(r.L, r.C);
    case 0x6a: return op_ld(r.L, r.D);
    case 0x6b: return op_ld(r.L, r.E);
    case 0x6c: return op_ld(r.L, r.H);
    case 0x6d: return op_ld(r.L, r.L);
    case 0x6e: return op_ld(r.L, r.HL);
    case 0x6f: return op_ld(r.L, r.A);
    case 0x70: return op_ld(r.HL, r.B);
    case 0x71: return op_ld(r.HL, r.B);
    case 0x72: return op_ld(r.HL, r.B);
    case 0x73: return op_ld(r.HL, r.B);
    case 0x74: return op_ld(r.HL, r.B);
    case 0x75: return op_ld(r.HL, r.B);
    case 0x76: return op_halt();
    case 0x77: return op_ld(r.HL, r.A);
    case 0x78: return op_ld(r.A, r.B);
    case 0x79: return op_ld(r.A, r.C);
    case 0x7a: return op_ld(r.A, r.D);
    case 0x7b: return op_ld(r.A, r.E);
    case 0x7c: return op_ld(r.A, r.H);
    case 0x7d: return op_ld(r.A, r.L);
    case 0x7e: return op_ld(r.A, r.HL);
    case 0x7f: return op_ld(r.A, r.A);
    case 0x80: return op_add(r.H, r.B);
    case 0x81: return op_add(r.H, r.C);
    case 0x82: return op_add(r.H, r.D);
    case 0x83: return op_add(r.H, r.E);
    case 0x84: return op_add(r.H, r.H);
    case 0x85: return op_add(r.A, r.L);
    case 0x86: return op_add(r.A, r.HL);
    case 0x87: return op_add(r.A, r.A);
    case 0x88: return op_adc(r.A, r.B);
    case 0x89: return op_adc(r.A, r.C);
    case 0x8a: return op_adc(r.A, r.D);
    case 0x8b: return op_adc(r.A, r.E);
    case 0x8c: return op_adc(r.A, r.H);
    case 0x8d: return op_adc(r.A, r.L);
    case 0x8e: return op_adc(r.A, r.HL);
    case 0x8f: return op_adc(r.A, r.A);
    case 0x90: return op_sub(r.A, r.B);
    case 0x91: return op_sub(r.A, r.C);
    case 0x92: return op_sub(r.A, r.D);
    case 0x93: return op_sub(r.A, r.E);
    case 0x94: return op_sub(r.A, r.H);
    case 0x95: return op_sub(r.A, r.L);
    case 0x96: return op_sub(r.A, r.HL);
    case 0x97: return op_sub(r.A, r.A);
    case 0x98: return op_sbc(r.A, r.B);
    case 0x99: return op_sbc(r.A, r.C);
    case 0x9a: return op_sbc(r.A, r.D);
    case 0x9b: return op_sbc(r.A, r.E);
    case 0x9c: return op_sbc(r.A, r.H);
    case 0x9d: return op_sbc(r.A, r.L);
    case 0x9e: return op_sbc(r.A, r.HL);
    case 0x9f: return op_sbc(r.A, r.A);
    case 0xa0: return op_and(r.A, r.B);
    case 0xa1: return op_and(r.A, r.C);
    case 0xa2: return op_and(r.A, r.D);
    case 0xa3: return op_and(r.A, r.E);
    case 0xa4: return op_and(r.A, r.H);
    case 0xa5: return op_and(r.A, r.L);
    case 0xa6: return op_and(r.A, r.HL);
    case 0xa7: return op_and(r.A, r.A);
    case 0xa8: return op_xor(r.A, r.B);
    case 0xa9: return op_xor(r.A, r.C);
    case 0xaa: return op_xor(r.A, r.D);
    case 0xab: return op_xor(r.A, r.E);
    case 0xac: return op_xor(r.A, r.H);
    case 0xad: return op_xor(r.A, r.L);
    case 0xae: return op_xor(r.A, r.HL);
    case 0xaf: return op_xor(r.A, r.A);
    case 0xb0: return op_or(r.A, r.B);
    case 0xb1: return op_or(r.A, r.C);
    case 0xb2: return op_or(r.A, r.D);
    case 0xb3: return op_or(r.A, r.E);
    case 0xb4: return op_or(r.A, r.H);
    case 0xb5: return op_or(r.A, r.L);
    case 0xb6: return op_or(r.A, r.HL);
    case 0xb7: return op_or(r.A, r.A);
    case 0xb8: return op_cp(r.A, r.B);
    case 0xb9: return op_cp(r.A, r.C);
    case 0xba: return op_cp(r.A, r.D);
    case 0xbb: return op_cp(r.A, r.E);
    case 0xbc: return op_cp(r.A, r.H);
    case 0xbd: return op_cp(r.A, r.L);
    case 0xbe: return op_cp(r.A, r.HL);
    case 0xbf: return op_cp(r.A, r.A);
    case 0xc0: return op_ret(condition::NZ);
    case 0xc1: return op_pop(r.BC);
    case 0xc2: return op_jp(condition::NZ);
    case 0xc3: return op_jp();
    case 0xc4: return op_call(condition::NZ);
    case 0xc5: return op_push(r.BC);
    case 0xc6: return op_add_n(r.A);
    case 0xc7: return op_rst(0x00);
    case 0xc8: return op_ret(condition::Z);
    case 0xc9: return op_ret();
    case 0xca: return op_jp(condition::Z);

    // extended instructions
    // all take an extra 4 clock cycles because of the extra fetch() for decoding
    case 0xcb:
    {
        auto ext_op = fetch();

        log_ext_instruction(ext_op);

        switch (ext_op)
        {
        case 0x00: return 4 + op_rlc(r.B);
        case 0x01: return 4 + op_rlc(r.C);
        case 0x02: return 4 + op_rlc(r.D);
        case 0x03: return 4 + op_rlc(r.E);
        case 0x04: return 4 + op_rlc(r.H);
        case 0x05: return 4 + op_rlc(r.L);
        case 0x06: return 4 + op_rlc(r.HL);
        case 0x07: return 4 + op_rlc(r.A);
        case 0x08: return 4 + op_rrc(r.B);
        case 0x09: return 4 + op_rrc(r.C);
        case 0x0a: return 4 + op_rrc(r.D);
        case 0x0b: return 4 + op_rrc(r.E);
        case 0x0c: return 4 + op_rrc(r.H);
        case 0x0d: return 4 + op_rrc(r.L);
        case 0x0e: return 4 + op_rrc(r.HL);
        case 0x0f: return 4 + op_rrc(r.A);
        case 0x10: return 4 + op_rl(r.B);
        case 0x11: return 4 + op_rl(r.C);
        case 0x12: return 4 + op_rl(r.D);
        case 0x13: return 4 + op_rl(r.E);
        case 0x14: return 4 + op_rl(r.H);
        case 0x15: return 4 + op_rl(r.L);
        case 0x16: return 4 + op_rl(r.HL);
        case 0x17: return 4 + op_rl(r.A);
        case 0x18: return 4 + op_rr(r.B);
        case 0x19: return 4 + op_rr(r.C);
        case 0x1a: return 4 + op_rr(r.D);
        case 0x1b: return 4 + op_rr(r.E);
        case 0x1c: return 4 + op_rr(r.H);
        case 0x1d: return 4 + op_rr(r.L);
        case 0x1e: return 4 + op_rr(r.HL);
        case 0x1f: return 4 + op_rr(r.A);
        case 0x20: return 4 + op_sla(r.B);
        case 0x21: return 4 + op_sla(r.C);
        case 0x22: return 4 + op_sla(r.D);
        case 0x23: return 4 + op_sla(r.E);
        case 0x24: return 4 + op_sla(r.H);
        case 0x25: return 4 + op_sla(r.L);
        case 0x26: return 4 + op_sla(r.HL);
        case 0x27: return 4 + op_sla(r.A);
        case 0x28: return 4 + op_sra(r.B);
        case 0x29: return 4 + op_sra(r.C);
        case 0x2a: return 4 + op_sra(r.D);
        case 0x2b: return 4 + op_sra(r.E);
        case 0x2c: return 4 + op_sra(r.H);
        case 0x2d: return 4 + op_sra(r.L);
        case 0x2e: return 4 + op_sra(r.HL);
        case 0x2f: return 4 + op_sra(r.A);
        case 0x30: return 4 + op_swap(r.B);
        case 0x31: return 4 + op_swap(r.C);
        case 0x32: return 4 + op_swap(r.D);
        case 0x33: return 4 + op_swap(r.E);
        case 0x34: return 4 + op_swap(r.H);
        case 0x35: return 4 + op_swap(r.L);
        case 0x36: return 4 + op_swap(r.HL);
        case 0x37: return 4 + op_swap(r.A);
        case 0x38: return 4 + op_srl(r.B);
        case 0x39: return 4 + op_srl(r.C);
        case 0x3a: return 4 + op_srl(r.D);
        case 0x3b: return 4 + op_srl(r.E);
        case 0x3c: return 4 + op_srl(r.H);
        case 0x3d: return 4 + op_srl(r.L);
        case 0x3e: return 4 + op_srl(r.HL);
        case 0x3f: return 4 + op_srl(r.A);
        case 0x40: return 4 + op_bit(r.B, 0);
        case 0x41: return 4 + op_bit(r.C, 0);
        case 0x42: return 4 + op_bit(r.D, 0);
        case 0x43: return 4 + op_bit(r.E, 0);
        case 0x44: return 4 + op_bit(r.H, 0);
        case 0x45: return 4 + op_bit(r.L, 0);
        case 0x46: return 4 + op_bit(r.HL, 0);
        case 0x47: return 4 + op_bit(r.A, 0);
        case 0x48: return 4 + op_bit(r.B, 1);
        case 0x49: return 4 + op_bit(r.C, 1);
        case 0x4a: return 4 + op_bit(r.D, 1);
        case 0x4b: return 4 + op_bit(r.E, 1);
        case 0x4c: return 4 + op_bit(r.H, 1);
        case 0x4d: return 4 + op_bit(r.L, 1);
        case 0x4e: return 4 + op_bit(r.HL, 1);
        case 0x4f: return 4 + op_bit(r.A, 1);
        case 0x50: return 4 + op_bit(r.B, 2);
        case 0x51: return 4 + op_bit(r.C, 2);
        case 0x52: return 4 + op_bit(r.D, 2);
        case 0x53: return 4 + op_bit(r.E, 2);
        case 0x54: return 4 + op_bit(r.H, 2);
        case 0x55: return 4 + op_bit(r.L, 2);
        case 0x56: return 4 + op_bit(r.HL, 2);
        case 0x57: return 4 + op_bit(r.A, 2);
        case 0x58: return 4 + op_bit(r.B, 3);
        case 0x59: return 4 + op_bit(r.C, 3);
        case 0x5a: return 4 + op_bit(r.D, 3);
        case 0x5b: return 4 + op_bit(r.E, 3);
        case 0x5c: return 4 + op_bit(r.H, 3);
        case 0x5d: return 4 + op_bit(r.L, 3);
        case 0x5e: return 4 + op_bit(r.HL, 3);
        case 0x5f: return 4 + op_bit(r.A, 3);
        case 0x60: return 4 + op_bit(r.B, 4);
        case 0x61: return 4 + op_bit(r.C, 4);
        case 0x62: return 4 + op_bit(r.D, 4);
        case 0x63: return 4 + op_bit(r.E, 4);
        case 0x64: return 4 + op_bit(r.H, 4);
        case 0x65: return 4 + op_bit(r.L, 4);
        case 0x66: return 4 + op_bit(r.HL, 4);
        case 0x67: return 4 + op_bit(r.A, 4);
        case 0x68: return 4 + op_bit(r.B, 5);
        case 0x69: return 4 + op_bit(r.C, 5);
        case 0x6a: return 4 + op_bit(r.D, 5);
        case 0x6b: return 4 + op_bit(r.E, 5);
        case 0x6c: return 4 + op_bit(r.H, 5);
        case 0x6d: return 4 + op_bit(r.L, 5);
        case 0x6e: return 4 + op_bit(r.HL, 5);
        case 0x6f: return 4 + op_bit(r.A, 5);
        case 0x70: return 4 + op_bit(r.B, 6);
        case 0x71: return 4 + op_bit(r.C, 6);
        case 0x72: return 4 + op_bit(r.D, 6);
        case 0x73: return 4 + op_bit(r.E, 6);
        case 0x74: return 4 + op_bit(r.H, 6);
        case 0x75: return 4 + op_bit(r.L, 6);
        case 0x76: return 4 + op_bit(r.HL, 6);
        case 0x77: return 4 + op_bit(r.A, 6);
        case 0x78: return 4 + op_bit(r.B, 7);
        case 0x79: return 4 + op_bit(r.C, 7);
        case 0x7a: return 4 + op_bit(r.D, 7);
        case 0x7b: return 4 + op_bit(r.E, 7);
        case 0x7c: return 4 + op_bit(r.H, 7);
        case 0x7d: return 4 + op_bit(r.L, 7);
        case 0x7e: return 4 + op_bit(r.HL, 7);
        case 0x7f: return 4 + op_bit(r.A, 7);
        case 0x80: return 4 + op_res(r.B, 0);
        case 0x81: return 4 + op_res(r.C, 0);
        case 0x82: return 4 + op_res(r.D, 0);
        case 0x83: return 4 + op_res(r.E, 0);
        case 0x84: return 4 + op_res(r.H, 0);
        case 0x85: return 4 + op_res(r.L, 0);
        case 0x86: return 4 + op_res(r.HL, 0);
        case 0x87: return 4 + op_res(r.A, 0);
        case 0x88: return 4 + op_res(r.B, 1);
        case 0x89: return 4 + op_res(r.C, 1);
        case 0x8a: return 4 + op_res(r.D, 1);
        case 0x8b: return 4 + op_res(r.E, 1);
        case 0x8c: return 4 + op_res(r.H, 1);
        case 0x8d: return 4 + op_res(r.L, 1);
        case 0x8e: return 4 + op_res(r.HL, 1);
        case 0x8f: return 4 + op_res(r.A, 1);
        case 0x90: return 4 + op_res(r.B, 2);
        case 0x91: return 4 + op_res(r.C, 2);
        case 0x92: return 4 + op_res(r.D, 2);
        case 0x93: return 4 + op_res(r.E, 2);
        case 0x94: return 4 + op_res(r.H, 2);
        case 0x95: return 4 + op_res(r.L, 2);
        case 0x96: return 4 + op_res(r.HL, 2);
        case 0x97: return 4 + op_res(r.A, 2);
        case 0x98: return 4 + op_res(r.B, 3);
        case 0x99: return 4 + op_res(r.C, 3);
        case 0x9a: return 4 + op_res(r.D, 3);
        case 0x9b: return 4 + op_res(r.E, 3);
        case 0x9c: return 4 + op_res(r.H, 3);
        case 0x9d: return 4 + op_res(r.L, 3);
        case 0x9e: return 4 + op_res(r.HL, 3);
        case 0x9f: return 4 + op_res(r.A, 3);
        case 0xa0: return 4 + op_res(r.B, 4);
        case 0xa1: return 4 + op_res(r.C, 4);
        case 0xa2: return 4 + op_res(r.D, 4);
        case 0xa3: return 4 + op_res(r.E, 4);
        case 0xa4: return 4 + op_res(r.H, 4);
        case 0xa5: return 4 + op_res(r.L, 4);
        case 0xa6: return 4 + op_res(r.HL, 4);
        case 0xa7: return 4 + op_res(r.A, 4);
        case 0xa8: return 4 + op_res(r.B, 5);
        case 0xa9: return 4 + op_res(r.C, 5);
        case 0xaa: return 4 + op_res(r.D, 5);
        case 0xab: return 4 + op_res(r.E, 5);
        case 0xac: return 4 + op_res(r.H, 5);
        case 0xad: return 4 + op_res(r.L, 5);
        case 0xae: return 4 + op_res(r.HL, 5);
        case 0xaf: return 4 + op_res(r.A, 5);
        case 0xb0: return 4 + op_res(r.B, 6);
        case 0xb1: return 4 + op_res(r.C, 6);
        case 0xb2: return 4 + op_res(r.D, 6);
        case 0xb3: return 4 + op_res(r.E, 6);
        case 0xb4: return 4 + op_res(r.H, 6);
        case 0xb5: return 4 + op_res(r.L, 6);
        case 0xb6: return 4 + op_res(r.HL, 6);
        case 0xb7: return 4 + op_res(r.A, 6);
        case 0xb8: return 4 + op_res(r.B, 7);
        case 0xb9: return 4 + op_res(r.C, 7);
        case 0xba: return 4 + op_res(r.D, 7);
        case 0xbb: return 4 + op_res(r.E, 7);
        case 0xbc: return 4 + op_res(r.H, 7);
        case 0xbd: return 4 + op_res(r.L, 7);
        case 0xbe: return 4 + op_res(r.HL, 7);
        case 0xbf: return 4 + op_res(r.A, 7);
        case 0xc0: return 4 + op_set(r.B, 0);
        case 0xc1: return 4 + op_set(r.C, 0);
        case 0xc2: return 4 + op_set(r.D, 0);
        case 0xc3: return 4 + op_set(r.E, 0);
        case 0xc4: return 4 + op_set(r.H, 0);
        case 0xc5: return 4 + op_set(r.L, 0);
        case 0xc6: return 4 + op_set(r.HL, 0);
        case 0xc7: return 4 + op_set(r.A, 0);
        case 0xc8: return 4 + op_set(r.B, 1);
        case 0xc9: return 4 + op_set(r.C, 1);
        case 0xca: return 4 + op_set(r.D, 1);
        case 0xcb: return 4 + op_set(r.E, 1);
        case 0xcc: return 4 + op_set(r.H, 1);
        case 0xcd: return 4 + op_set(r.L, 1);
        case 0xce: return 4 + op_set(r.HL, 1);
        case 0xcf: return 4 + op_set(r.A, 1);
        case 0xd0: return 4 + op_set(r.B, 2);
        case 0xd1: return 4 + op_set(r.C, 2);
        case 0xd2: return 4 + op_set(r.D, 2);
        case 0xd3: return 4 + op_set(r.E, 2);
        case 0xd4: return 4 + op_set(r.H, 2);
        case 0xd5: return 4 + op_set(r.L, 2);
        case 0xd6: return 4 + op_set(r.HL, 2);
        case 0xd7: return 4 + op_set(r.A, 2);
        case 0xd8: return 4 + op_set(r.B, 3);
        case 0xd9: return 4 + op_set(r.C, 3);
        case 0xda: return 4 + op_set(r.D, 3);
        case 0xdb: return 4 + op_set(r.E, 3);
        case 0xdc: return 4 + op_set(r.H, 3);
        case 0xdd: return 4 + op_set(r.L, 3);
        case 0xde: return 4 + op_set(r.HL, 3);
        case 0xdf: return 4 + op_set(r.A, 3);
        case 0xe0: return 4 + op_set(r.B, 4);
        case 0xe1: return 4 + op_set(r.C, 4);
        case 0xe2: return 4 + op_set(r.D, 4);
        case 0xe3: return 4 + op_set(r.E, 4);
        case 0xe4: return 4 + op_set(r.H, 4);
        case 0xe5: return 4 + op_set(r.L, 4);
        case 0xe6: return 4 + op_set(r.HL, 4);
        case 0xe7: return 4 + op_set(r.A, 4);
        case 0xe8: return 4 + op_set(r.B, 5);
        case 0xe9: return 4 + op_set(r.C, 5);
        case 0xea: return 4 + op_set(r.D, 5);
        case 0xeb: return 4 + op_set(r.E, 5);
        case 0xec: return 4 + op_set(r.H, 5);
        case 0xed: return 4 + op_set(r.L, 5);
        case 0xee: return 4 + op_set(r.HL, 5);
        case 0xef: return 4 + op_set(r.A, 5);
        case 0xf0: return 4 + op_set(r.B, 6);
        case 0xf1: return 4 + op_set(r.C, 6);
        case 0xf2: return 4 + op_set(r.D, 6);
        case 0xf3: return 4 + op_set(r.E, 6);
        case 0xf4: return 4 + op_set(r.H, 6);
        case 0xf5: return 4 + op_set(r.L, 6);
        case 0xf6: return 4 + op_set(r.HL, 6);
        case 0xf7: return 4 + op_set(r.A, 6);
        case 0xf8: return 4 + op_set(r.B, 7);
        case 0xf9: return 4 + op_set(r.C, 7);
        case 0xfa: return 4 + op_set(r.D, 7);
        case 0xfb: return 4 + op_set(r.E, 7);
        case 0xfc: return 4 + op_set(r.H, 7);
        case 0xfd: return 4 + op_set(r.L, 7);
        case 0xfe: return 4 + op_set(r.HL, 7);
        case 0xff: return 4 + op_set(r.A, 7);
        }
    }

    case 0xcc: return op_call(condition::Z);
    case 0xcd: return op_call();
    case 0xce: return op_adc_n(r.A);
    case 0xcf: return op_rst(0x08);
    case 0xd0: return op_ret(condition::NC);
    case 0xd1: return op_pop(r.DE);
    case 0xd2: return op_jp(condition::NC);
    case 0xd3: return op_nop(); // TODO this actually would hang the CPU
    case 0xd4: return op_call(condition::NC);
    case 0xd5: return op_push(r.DE);
    case 0xd6: return op_sub_n(r.A);
    case 0xd7: return op_rst(0x10);
    case 0xd8: return op_ret(condition::C);
    case 0xd9: return op_reti();
    case 0xda: return op_jp(condition::C);
    case 0xdb: return op_nop(); // TODO this actually would hang the CPU
    case 0xdc: return op_call(condition::C);
    case 0xdd: return op_nop(); // TODO this actually would hang the CPU
    case 0xde: return op_sbc_n(r.A);
    case 0xdf: return op_rst(0x18);
    case 0xe0: return op_ldh_n();
    case 0xe1: return op_pop(r.HL);
    case 0xe2: return op_ld(static_cast<uint16_t>(0xff00 + r.C), r.A);
    case 0xe3: return op_nop(); // TODO this actually would hang the CPU
    case 0xe4: return op_nop(); // TODO this actually would hang the CPU
    case 0xe5: return op_push(r.HL);
    case 0xe6: return op_and_n(r.A);
    case 0xe7: return op_rst(0x20);
    case 0xe8: return op_add_sp();
    case 0xe9: return op_jp(r.HL);
    case 0xea: return op_ld_to_nn(r.A);
    case 0xeb: return op_nop(); // TODO this actually would hang the CPU
    case 0xec: return op_nop(); // TODO this actually would hang the CPU
    case 0xed: return op_nop(); // TODO this actually would hang the CPU
    case 0xee: return op_xor_n(r.A);
    case 0xef: return op_rst(0x28);
    case 0xf0: return op_ldh_A();
    case 0xf1: return op_pop(r.AF);
    case 0xf2: return op_ld(r.A, static_cast<uint16_t>(0xff00 + r.C));
    case 0xf3: return op_di();
    case 0xf4: return op_nop(); // TODO this actually would hang the CPU
    case 0xf5: return op_push(r.AF);
    case 0xf6: return op_or_n(r.A);
    case 0xf7: return op_rst(0x30);
    case 0xf8: return op_ld16_HL();
    case 0xf9: return op_ld16(r.sp, r.HL);
    case 0xfa: return op_ld_to_nn(r.A);
    case 0xfb: return op_ei();
    case 0xfc: return op_nop(); // TODO this actually would hang the CPU
    case 0xfd: return op_nop(); // TODO this actually would hang the CPU
    case 0xfe: return op_cp_n(r.A);
    case 0xff: return op_rst(0x38);

    default:
        // not reachable
        return op_nop();
    }
}

}
