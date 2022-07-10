#include "cpu.hpp"

#include "memory.hpp"
#include "util.hpp"

namespace gb
{

constexpr uint32_t clock_rate = 4194304; // clock cycles per second / Hz

constexpr uint16_t vblank_handler   = 0x40;
constexpr uint16_t lcd_stat_handler = 0x48;
constexpr uint16_t timer_handler    = 0x50;
constexpr uint16_t serial_handler   = 0x58;
constexpr uint16_t joypad_handler   = 0x60;

cpu::cpu(std::unique_ptr<memory>&& mem) noexcept
    : mem{std::move(mem)},
      pipeline{},
      running{false},
      interrupts_enabled{false},
      cycles{0},
      pc{0}
{}

void cpu::run() noexcept
{
    pipeline.push(action::execute);
    running = true;

    while (running)
    {
        auto next = pipeline.front();
        pipeline.pop();

        switch (next)
        {
        case action::execute:
            cycles += execute(fetch());
            break;

        case action::halt:
            // stay halted until interrupt occurs
            pc--;
            break;

        case action::disable_interrupts:
            // TODO
            interrupts_enabled = false;
            pipeline.push(action::execute);
            break;

        case action::enable_interrupts:
            // TODO
            interrupts_enabled = true;
            pipeline.push(action::execute);
            break;
        }

        process_interrupts();
        update_lcd();
        update_timers();
    }
}

void cpu::stop() noexcept
{
    running = false;
}

void cpu::queue_interrupt(interrupt i) noexcept
{
    if (!interrupts_enabled) return;

    auto val = mem->read(memory::interrupt_flag);
    val |= static_cast<uint8_t>(i);
    mem->write(memory::interrupt_flag, val);
}

uint8_t cpu::fetch() noexcept
{
    return mem->read(pc++);
}

uint16_t cpu::fetch16() noexcept
{
    auto ret = mem->read16(pc);
    pc += 2;
    return ret;
}

void cpu::process_interrupts() noexcept
{
    if (!interrupts_enabled) return;

    uint16_t jump_addr = 0;

    auto requested = mem->read(memory::interrupt_flag);
    if ((requested & static_cast<uint8_t>(interrupt::vblank)) != 0)
        jump_addr = vblank_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::lcd_stat)) != 0)
        jump_addr = lcd_stat_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::timer)) != 0)
        jump_addr = timer_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::serial)) != 0)
        jump_addr = serial_handler;
    else if ((requested & static_cast<uint8_t>(interrupt::joypad)) != 0)
        jump_addr = joypad_handler;

    if (jump_addr != 0)
    {
        interrupts_enabled = false;
        op_push(pc);
        pc = jump_addr;
    }
}

void cpu::update_lcd() noexcept
{
    constexpr uint8_t enabled            = 1 << 7;
    constexpr uint8_t tile_map_select    = 1 << 6;
    constexpr uint8_t window_enabled     = 1 << 5;
    constexpr uint8_t tile_data_select   = 1 << 4;
    constexpr uint8_t bg_tile_map_select = 1 << 3;
    constexpr uint8_t obj_size_select    = 1 << 2;
    constexpr uint8_t obj_enabled        = 1 << 1;
    constexpr uint8_t bg_display         = 1 << 0;

    auto val = mem->read(memory::lcd_control);
    if ((val & enabled) == 0) return;

    // TODO update to declared state
    queue_interrupt(interrupt::vblank);
}

void cpu::update_timers() noexcept
{
    constexpr uint32_t div_inc_rate   = 16384; // Hz
    constexpr uint32_t div_inc_cycles = clock_rate / div_inc_rate;

    constexpr uint8_t started      = 1 << 2;
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

    //
}

uint32_t cpu::execute(uint8_t op) noexcept
{
    // we'll always be executing again
    pipeline.push(action::execute);

    switch (op)
    {
    case 0x00: return op_nop();
    case 0x01: return op_ld16(BC);
    case 0x02: return op_ld(BC, A);
    case 0x03: return op_inc16(BC);
    case 0x04: return op_inc(B);
    case 0x05: return op_dec(B);
    case 0x06: return op_ld_n(B);
    case 0x07: return op_rlc(A);
    case 0x08: return op_ld16_nn();
    case 0x09: return op_add(HL, BC);
    case 0x0a: return op_ld(A, BC);
    case 0x0b: return op_dec16(BC);
    case 0x0c: return op_inc(C);
    case 0x0d: return op_dec(C);
    case 0x0e: return op_ld_n(C);
    case 0x0f: return op_rrc(A);
    case 0x10:
        switch (fetch())
        {
        case 0x00: return op_stop();
        default:   return op_nop();
        }
    case 0x11: return op_ld16(DE);
    case 0x12: return op_ld(DE, A);
    case 0x13: return op_inc16(DE);
    case 0x14: return op_inc(D);
    case 0x15: return op_dec(D);
    case 0x16: return op_ld_n(D);
    case 0x17: return op_rl(A);
    case 0x18: return op_jr();
    case 0x19: return op_add(HL, DE);
    case 0x1a: return op_ld(A, DE);
    case 0x1b: return op_dec16(DE);
    case 0x1c: return op_inc(E);
    case 0x1d: return op_dec(E);
    case 0x1e: return op_ld_n(E);
    case 0x1f: return op_rr(A);
    case 0x20: return op_jr(condition::NZ);
    case 0x21: return op_ld16(HL);
    case 0x22: return op_ldi_HL();
    case 0x23: return op_inc16(HL);
    case 0x24: return op_inc(H);
    case 0x25: return op_dec(H);
    case 0x26: return op_ld_n(H);
    case 0x27: return op_daa();
    case 0x28: return op_jr(condition::Z);
    case 0x29: return op_add(HL, HL);
    case 0x2a: return op_ldi_A();
    case 0x2b: return op_dec16(HL);
    case 0x2c: return op_inc(L);
    case 0x2d: return op_dec(L);
    case 0x2e: return op_ld_n(L);
    case 0x2f: return op_cpl();
    case 0x30: return op_jr(condition::NC);
    case 0x31: return op_ld16(sp);
    case 0x32: return op_ldd_HL();
    case 0x33: return op_inc16(sp);
    case 0x34: return op_inc(HL);
    case 0x35: return op_dec(HL);
    case 0x36: return op_ld_n(HL);
    case 0x37: return op_scf();
    case 0x38: return op_jr(condition::C);
    case 0x39: return op_add(HL, sp);
    case 0x3a: return op_ldd_A();
    case 0x3b: return op_dec16(sp);
    case 0x3c: return op_inc(A);
    case 0x3d: return op_dec(A);
    case 0x3e: return op_ld_n(A);
    case 0x3f: return op_ccf();
    case 0x40: return op_ld(B, B);
    case 0x41: return op_ld(B, C);
    case 0x42: return op_ld(B, D);
    case 0x43: return op_ld(B, E);
    case 0x44: return op_ld(B, H);
    case 0x45: return op_ld(B, L);
    case 0x46: return op_ld(B, HL);
    case 0x47: return op_ld(B, A);
    case 0x48: return op_ld(C, B);
    case 0x49: return op_ld(C, C);
    case 0x4a: return op_ld(C, D);
    case 0x4b: return op_ld(C, E);
    case 0x4c: return op_ld(C, H);
    case 0x4d: return op_ld(C, L);
    case 0x4e: return op_ld(C, HL);
    case 0x4f: return op_ld(C, A);
    case 0x50: return op_ld(D, B);
    case 0x51: return op_ld(D, C);
    case 0x52: return op_ld(D, D);
    case 0x53: return op_ld(D, E);
    case 0x54: return op_ld(D, H);
    case 0x55: return op_ld(D, L);
    case 0x56: return op_ld(D, HL);
    case 0x57: return op_ld(D, A);
    case 0x58: return op_ld(E, B);
    case 0x59: return op_ld(E, C);
    case 0x5a: return op_ld(E, D);
    case 0x5b: return op_ld(E, E);
    case 0x5c: return op_ld(E, H);
    case 0x5d: return op_ld(E, L);
    case 0x5e: return op_ld(E, HL);
    case 0x5f: return op_ld(E, A);
    case 0x60: return op_ld(H, B);
    case 0x61: return op_ld(H, C);
    case 0x62: return op_ld(H, D);
    case 0x63: return op_ld(H, E);
    case 0x64: return op_ld(H, H);
    case 0x65: return op_ld(H, L);
    case 0x66: return op_ld(H, HL);
    case 0x67: return op_ld(H, A);
    case 0x68: return op_ld(L, B);
    case 0x69: return op_ld(L, C);
    case 0x6a: return op_ld(L, D);
    case 0x6b: return op_ld(L, E);
    case 0x6c: return op_ld(L, H);
    case 0x6d: return op_ld(L, L);
    case 0x6e: return op_ld(L, HL);
    case 0x6f: return op_ld(L, A);
    case 0x70: return op_ld(HL, B);
    case 0x71: return op_ld(HL, B);
    case 0x72: return op_ld(HL, B);
    case 0x73: return op_ld(HL, B);
    case 0x74: return op_ld(HL, B);
    case 0x75: return op_ld(HL, B);
    case 0x76: return op_halt();
    case 0x77: return op_ld(HL, A);
    case 0x78: return op_ld(A, B);
    case 0x79: return op_ld(A, C);
    case 0x7a: return op_ld(A, D);
    case 0x7b: return op_ld(A, E);
    case 0x7c: return op_ld(A, H);
    case 0x7d: return op_ld(A, L);
    case 0x7e: return op_ld(A, HL);
    case 0x7f: return op_ld(A, A);
    case 0x80: return op_add(H, B);
    case 0x81: return op_add(H, C);
    case 0x82: return op_add(H, D);
    case 0x83: return op_add(H, E);
    case 0x84: return op_add(H, H);
    case 0x85: return op_add(A, L);
    case 0x86: return op_add(A, HL);
    case 0x87: return op_add(A, A);
    case 0x88: return op_adc(A, B);
    case 0x89: return op_adc(A, C);
    case 0x8a: return op_adc(A, D);
    case 0x8b: return op_adc(A, E);
    case 0x8c: return op_adc(A, H);
    case 0x8d: return op_adc(A, L);
    case 0x8e: return op_adc(A, HL);
    case 0x8f: return op_adc(A, A);
    case 0x90: return op_sub(A, B);
    case 0x91: return op_sub(A, C);
    case 0x92: return op_sub(A, D);
    case 0x93: return op_sub(A, E);
    case 0x94: return op_sub(A, H);
    case 0x95: return op_sub(A, L);
    case 0x96: return op_sub(A, HL);
    case 0x97: return op_sub(A, A);
    case 0x98: return op_sbc(A, B);
    case 0x99: return op_sbc(A, C);
    case 0x9a: return op_sbc(A, D);
    case 0x9b: return op_sbc(A, E);
    case 0x9c: return op_sbc(A, H);
    case 0x9d: return op_sbc(A, L);
    case 0x9e: return op_sbc(A, HL);
    case 0x9f: return op_sbc(A, A);
    case 0xa0: return op_and(A, B);
    case 0xa1: return op_and(A, C);
    case 0xa2: return op_and(A, D);
    case 0xa3: return op_and(A, E);
    case 0xa4: return op_and(A, H);
    case 0xa5: return op_and(A, L);
    case 0xa6: return op_and(A, HL);
    case 0xa7: return op_and(A, A);
    case 0xa8: return op_xor(A, B);
    case 0xa9: return op_xor(A, C);
    case 0xaa: return op_xor(A, D);
    case 0xab: return op_xor(A, E);
    case 0xac: return op_xor(A, H);
    case 0xad: return op_xor(A, L);
    case 0xae: return op_xor(A, HL);
    case 0xaf: return op_xor(A, A);
    case 0xb0: return op_or(A, B);
    case 0xb1: return op_or(A, C);
    case 0xb2: return op_or(A, D);
    case 0xb3: return op_or(A, E);
    case 0xb4: return op_or(A, H);
    case 0xb5: return op_or(A, L);
    case 0xb6: return op_or(A, HL);
    case 0xb7: return op_or(A, A);
    case 0xb8: return op_cp(A, B);
    case 0xb9: return op_cp(A, C);
    case 0xba: return op_cp(A, D);
    case 0xbb: return op_cp(A, E);
    case 0xbc: return op_cp(A, H);
    case 0xbd: return op_cp(A, L);
    case 0xbe: return op_cp(A, HL);
    case 0xbf: return op_cp(A, A);
    case 0xc0: return op_ret(condition::NZ);
    case 0xc1: return op_pop(BC);
    case 0xc2: return op_jp(condition::NZ);
    case 0xc3: return op_jp();
    case 0xc4: return op_call(condition::NZ);
    case 0xc5: return op_push(BC);
    case 0xc6: return op_add_n(A);
    case 0xc7: return op_rst(0x00);
    case 0xc8: return op_ret(condition::Z);
    case 0xc9: return op_ret();
    case 0xca: return op_jp(condition::Z);

    // extended instructions
    // all take an extra 4 clock cycles
    // because of the extra fetch() for decoding
    case 0xcb:
    {
        auto ext_op = fetch();
        switch (ext_op)
        {
        case 0x00: return 4 + op_rlc(B);
        case 0x01: return 4 + op_rlc(C);
        case 0x02: return 4 + op_rlc(D);
        case 0x03: return 4 + op_rlc(E);
        case 0x04: return 4 + op_rlc(H);
        case 0x05: return 4 + op_rlc(L);
        case 0x06: return 4 + op_rlc(HL);
        case 0x07: return 4 + op_rlc(A);
        case 0x08: return 4 + op_rrc(B);
        case 0x09: return 4 + op_rrc(C);
        case 0x0a: return 4 + op_rrc(D);
        case 0x0b: return 4 + op_rrc(E);
        case 0x0c: return 4 + op_rrc(H);
        case 0x0d: return 4 + op_rrc(L);
        case 0x0e: return 4 + op_rrc(HL);
        case 0x0f: return 4 + op_rrc(A);
        case 0x10: return 4 + op_rl(B);
        case 0x11: return 4 + op_rl(C);
        case 0x12: return 4 + op_rl(D);
        case 0x13: return 4 + op_rl(E);
        case 0x14: return 4 + op_rl(H);
        case 0x15: return 4 + op_rl(L);
        case 0x16: return 4 + op_rl(HL);
        case 0x17: return 4 + op_rl(A);
        case 0x18: return 4 + op_rr(B);
        case 0x19: return 4 + op_rr(C);
        case 0x1a: return 4 + op_rr(D);
        case 0x1b: return 4 + op_rr(E);
        case 0x1c: return 4 + op_rr(H);
        case 0x1d: return 4 + op_rr(L);
        case 0x1e: return 4 + op_rr(HL);
        case 0x1f: return 4 + op_rr(A);
        case 0x20: return 4 + op_sla(B);
        case 0x21: return 4 + op_sla(C);
        case 0x22: return 4 + op_sla(D);
        case 0x23: return 4 + op_sla(E);
        case 0x24: return 4 + op_sla(H);
        case 0x25: return 4 + op_sla(L);
        case 0x26: return 4 + op_sla(HL);
        case 0x27: return 4 + op_sla(A);
        case 0x28: return 4 + op_sra(B);
        case 0x29: return 4 + op_sra(C);
        case 0x2a: return 4 + op_sra(D);
        case 0x2b: return 4 + op_sra(E);
        case 0x2c: return 4 + op_sra(H);
        case 0x2d: return 4 + op_sra(L);
        case 0x2e: return 4 + op_sra(HL);
        case 0x2f: return 4 + op_sra(A);
        case 0x30: return 4 + op_swap(B);
        case 0x31: return 4 + op_swap(C);
        case 0x32: return 4 + op_swap(D);
        case 0x33: return 4 + op_swap(E);
        case 0x34: return 4 + op_swap(H);
        case 0x35: return 4 + op_swap(L);
        case 0x36: return 4 + op_swap(HL);
        case 0x37: return 4 + op_swap(A);
        case 0x38: return 4 + op_srl(B);
        case 0x39: return 4 + op_srl(C);
        case 0x3a: return 4 + op_srl(D);
        case 0x3b: return 4 + op_srl(E);
        case 0x3c: return 4 + op_srl(H);
        case 0x3d: return 4 + op_srl(L);
        case 0x3e: return 4 + op_srl(HL);
        case 0x3f: return 4 + op_srl(A);
        case 0x40: return 4 + op_bit(B, 0);
        case 0x41: return 4 + op_bit(C, 0);
        case 0x42: return 4 + op_bit(D, 0);
        case 0x43: return 4 + op_bit(E, 0);
        case 0x44: return 4 + op_bit(H, 0);
        case 0x45: return 4 + op_bit(L, 0);
        case 0x46: return 4 + op_bit(HL, 0);
        case 0x47: return 4 + op_bit(A, 0);
        case 0x48: return 4 + op_bit(B, 1);
        case 0x49: return 4 + op_bit(C, 1);
        case 0x4a: return 4 + op_bit(D, 1);
        case 0x4b: return 4 + op_bit(E, 1);
        case 0x4c: return 4 + op_bit(H, 1);
        case 0x4d: return 4 + op_bit(L, 1);
        case 0x4e: return 4 + op_bit(HL, 1);
        case 0x4f: return 4 + op_bit(A, 1);
        case 0x50: return 4 + op_bit(B, 2);
        case 0x51: return 4 + op_bit(C, 2);
        case 0x52: return 4 + op_bit(D, 2);
        case 0x53: return 4 + op_bit(E, 2);
        case 0x54: return 4 + op_bit(H, 2);
        case 0x55: return 4 + op_bit(L, 2);
        case 0x56: return 4 + op_bit(HL, 2);
        case 0x57: return 4 + op_bit(A, 2);
        case 0x58: return 4 + op_bit(B, 3);
        case 0x59: return 4 + op_bit(C, 3);
        case 0x5a: return 4 + op_bit(D, 3);
        case 0x5b: return 4 + op_bit(E, 3);
        case 0x5c: return 4 + op_bit(H, 3);
        case 0x5d: return 4 + op_bit(L, 3);
        case 0x5e: return 4 + op_bit(HL, 3);
        case 0x5f: return 4 + op_bit(A, 3);
        case 0x60: return 4 + op_bit(B, 4);
        case 0x61: return 4 + op_bit(C, 4);
        case 0x62: return 4 + op_bit(D, 4);
        case 0x63: return 4 + op_bit(E, 4);
        case 0x64: return 4 + op_bit(H, 4);
        case 0x65: return 4 + op_bit(L, 4);
        case 0x66: return 4 + op_bit(HL, 4);
        case 0x67: return 4 + op_bit(A, 4);
        case 0x68: return 4 + op_bit(B, 5);
        case 0x69: return 4 + op_bit(C, 5);
        case 0x6a: return 4 + op_bit(D, 5);
        case 0x6b: return 4 + op_bit(E, 5);
        case 0x6c: return 4 + op_bit(H, 5);
        case 0x6d: return 4 + op_bit(L, 5);
        case 0x6e: return 4 + op_bit(HL, 5);
        case 0x6f: return 4 + op_bit(A, 5);
        case 0x70: return 4 + op_bit(B, 6);
        case 0x71: return 4 + op_bit(C, 6);
        case 0x72: return 4 + op_bit(D, 6);
        case 0x73: return 4 + op_bit(E, 6);
        case 0x74: return 4 + op_bit(H, 6);
        case 0x75: return 4 + op_bit(L, 6);
        case 0x76: return 4 + op_bit(HL, 6);
        case 0x77: return 4 + op_bit(A, 6);
        case 0x78: return 4 + op_bit(B, 7);
        case 0x79: return 4 + op_bit(C, 7);
        case 0x7a: return 4 + op_bit(D, 7);
        case 0x7b: return 4 + op_bit(E, 7);
        case 0x7c: return 4 + op_bit(H, 7);
        case 0x7d: return 4 + op_bit(L, 7);
        case 0x7e: return 4 + op_bit(HL, 7);
        case 0x7f: return 4 + op_bit(A, 7);
        case 0x80: return 4 + op_res(B, 0);
        case 0x81: return 4 + op_res(C, 0);
        case 0x82: return 4 + op_res(D, 0);
        case 0x83: return 4 + op_res(E, 0);
        case 0x84: return 4 + op_res(H, 0);
        case 0x85: return 4 + op_res(L, 0);
        case 0x86: return 4 + op_res(HL, 0);
        case 0x87: return 4 + op_res(A, 0);
        case 0x88: return 4 + op_res(B, 1);
        case 0x89: return 4 + op_res(C, 1);
        case 0x8a: return 4 + op_res(D, 1);
        case 0x8b: return 4 + op_res(E, 1);
        case 0x8c: return 4 + op_res(H, 1);
        case 0x8d: return 4 + op_res(L, 1);
        case 0x8e: return 4 + op_res(HL, 1);
        case 0x8f: return 4 + op_res(A, 1);
        case 0x90: return 4 + op_res(B, 2);
        case 0x91: return 4 + op_res(C, 2);
        case 0x92: return 4 + op_res(D, 2);
        case 0x93: return 4 + op_res(E, 2);
        case 0x94: return 4 + op_res(H, 2);
        case 0x95: return 4 + op_res(L, 2);
        case 0x96: return 4 + op_res(HL, 2);
        case 0x97: return 4 + op_res(A, 2);
        case 0x98: return 4 + op_res(B, 3);
        case 0x99: return 4 + op_res(C, 3);
        case 0x9a: return 4 + op_res(D, 3);
        case 0x9b: return 4 + op_res(E, 3);
        case 0x9c: return 4 + op_res(H, 3);
        case 0x9d: return 4 + op_res(L, 3);
        case 0x9e: return 4 + op_res(HL, 3);
        case 0x9f: return 4 + op_res(A, 3);
        case 0xa0: return 4 + op_res(B, 4);
        case 0xa1: return 4 + op_res(C, 4);
        case 0xa2: return 4 + op_res(D, 4);
        case 0xa3: return 4 + op_res(E, 4);
        case 0xa4: return 4 + op_res(H, 4);
        case 0xa5: return 4 + op_res(L, 4);
        case 0xa6: return 4 + op_res(HL, 4);
        case 0xa7: return 4 + op_res(A, 4);
        case 0xa8: return 4 + op_res(B, 5);
        case 0xa9: return 4 + op_res(C, 5);
        case 0xaa: return 4 + op_res(D, 5);
        case 0xab: return 4 + op_res(E, 5);
        case 0xac: return 4 + op_res(H, 5);
        case 0xad: return 4 + op_res(L, 5);
        case 0xae: return 4 + op_res(HL, 5);
        case 0xaf: return 4 + op_res(A, 5);
        case 0xb0: return 4 + op_res(B, 6);
        case 0xb1: return 4 + op_res(C, 6);
        case 0xb2: return 4 + op_res(D, 6);
        case 0xb3: return 4 + op_res(E, 6);
        case 0xb4: return 4 + op_res(H, 6);
        case 0xb5: return 4 + op_res(L, 6);
        case 0xb6: return 4 + op_res(HL, 6);
        case 0xb7: return 4 + op_res(A, 6);
        case 0xb8: return 4 + op_res(B, 7);
        case 0xb9: return 4 + op_res(C, 7);
        case 0xba: return 4 + op_res(D, 7);
        case 0xbb: return 4 + op_res(E, 7);
        case 0xbc: return 4 + op_res(H, 7);
        case 0xbd: return 4 + op_res(L, 7);
        case 0xbe: return 4 + op_res(HL, 7);
        case 0xbf: return 4 + op_res(A, 7);
        case 0xc0: return 4 + op_set(B, 0);
        case 0xc1: return 4 + op_set(C, 0);
        case 0xc2: return 4 + op_set(D, 0);
        case 0xc3: return 4 + op_set(E, 0);
        case 0xc4: return 4 + op_set(H, 0);
        case 0xc5: return 4 + op_set(L, 0);
        case 0xc6: return 4 + op_set(HL, 0);
        case 0xc7: return 4 + op_set(A, 0);
        case 0xc8: return 4 + op_set(B, 1);
        case 0xc9: return 4 + op_set(C, 1);
        case 0xca: return 4 + op_set(D, 1);
        case 0xcb: return 4 + op_set(E, 1);
        case 0xcc: return 4 + op_set(H, 1);
        case 0xcd: return 4 + op_set(L, 1);
        case 0xce: return 4 + op_set(HL, 1);
        case 0xcf: return 4 + op_set(A, 1);
        case 0xd0: return 4 + op_set(B, 2);
        case 0xd1: return 4 + op_set(C, 2);
        case 0xd2: return 4 + op_set(D, 2);
        case 0xd3: return 4 + op_set(E, 2);
        case 0xd4: return 4 + op_set(H, 2);
        case 0xd5: return 4 + op_set(L, 2);
        case 0xd6: return 4 + op_set(HL, 2);
        case 0xd7: return 4 + op_set(A, 2);
        case 0xd8: return 4 + op_set(B, 3);
        case 0xd9: return 4 + op_set(C, 3);
        case 0xda: return 4 + op_set(D, 3);
        case 0xdb: return 4 + op_set(E, 3);
        case 0xdc: return 4 + op_set(H, 3);
        case 0xdd: return 4 + op_set(L, 3);
        case 0xde: return 4 + op_set(HL, 3);
        case 0xdf: return 4 + op_set(A, 3);
        case 0xe0: return 4 + op_set(B, 4);
        case 0xe1: return 4 + op_set(C, 4);
        case 0xe2: return 4 + op_set(D, 4);
        case 0xe3: return 4 + op_set(E, 4);
        case 0xe4: return 4 + op_set(H, 4);
        case 0xe5: return 4 + op_set(L, 4);
        case 0xe6: return 4 + op_set(HL, 4);
        case 0xe7: return 4 + op_set(A, 4);
        case 0xe8: return 4 + op_set(B, 5);
        case 0xe9: return 4 + op_set(C, 5);
        case 0xea: return 4 + op_set(D, 5);
        case 0xeb: return 4 + op_set(E, 5);
        case 0xec: return 4 + op_set(H, 5);
        case 0xed: return 4 + op_set(L, 5);
        case 0xee: return 4 + op_set(HL, 5);
        case 0xef: return 4 + op_set(A, 5);
        case 0xf0: return 4 + op_set(B, 6);
        case 0xf1: return 4 + op_set(C, 6);
        case 0xf2: return 4 + op_set(D, 6);
        case 0xf3: return 4 + op_set(E, 6);
        case 0xf4: return 4 + op_set(H, 6);
        case 0xf5: return 4 + op_set(L, 6);
        case 0xf6: return 4 + op_set(HL, 6);
        case 0xf7: return 4 + op_set(A, 6);
        case 0xf8: return 4 + op_set(B, 7);
        case 0xf9: return 4 + op_set(C, 7);
        case 0xfa: return 4 + op_set(D, 7);
        case 0xfb: return 4 + op_set(E, 7);
        case 0xfc: return 4 + op_set(H, 7);
        case 0xfd: return 4 + op_set(L, 7);
        case 0xfe: return 4 + op_set(HL, 7);
        case 0xff: return 4 + op_set(A, 7);
        }
    }

    case 0xcc: return op_call(condition::Z);
    case 0xcd: return op_call();
    case 0xce: return op_adc_n(A);
    case 0xcf: return op_rst(0x08);
    case 0xd0: return op_ret(condition::NC);
    case 0xd1: return op_pop(DE);
    case 0xd2: return op_jp(condition::NC);
    case 0xd3: return op_nop();
    case 0xd4: return op_call(condition::NC);
    case 0xd5: return op_push(DE);
    case 0xd6: return op_sub_n(A);
    case 0xd7: return op_rst(0x10);
    case 0xd8: return op_ret(condition::C);
    case 0xd9: return op_reti();
    case 0xda: return op_jp(condition::C);
    case 0xdb: return op_nop();
    case 0xdc: return op_call(condition::C);
    case 0xdd: return op_nop();
    case 0xde: return op_sbc_n(A);
    case 0xdf: return op_rst(0x18);
    case 0xe0: return op_ldh_n();
    case 0xe1: return op_pop(HL);
    case 0xe2: return op_ld(static_cast<uint16_t>(0xff00 + C), A);
    case 0xe3: return op_nop();
    case 0xe4: return op_nop();
    case 0xe5: return op_push(HL);
    case 0xe6: return op_and_n(A);
    case 0xe7: return op_rst(0x20);
    case 0xe8: return op_add_sp();
    case 0xe9: return op_jp(HL);
    case 0xea: return op_ld_nn(static_cast<uint8_t>(A));
    case 0xeb: return op_nop();
    case 0xec: return op_nop();
    case 0xed: return op_nop();
    case 0xee: return op_xor_n(A);
    case 0xef: return op_rst(0x28);
    case 0xf0: return op_ldh_A();
    case 0xf1: return op_pop(AF);
    case 0xf2: return op_ld(A, static_cast<uint16_t>(0xff00 + C));
    case 0xf3: return op_di();
    case 0xf4: return op_nop();
    case 0xf5: return op_push(AF);
    case 0xf6: return op_or_n(A);
    case 0xf7: return op_rst(0x30);
    case 0xf8: return op_ld16_HL();
    case 0xf9: return op_ld16(sp, HL);
    case 0xfa: return op_ld_n(A);
    case 0xfb: return op_ei();
    case 0xfc: return op_nop();
    case 0xfd: return op_nop();
    case 0xfe: return op_cp_n(A);
    case 0xff: return op_rst(0x38);
    default:   return op_nop();
    }
}

uint32_t cpu::op_ld_n(uint8_t& reg) noexcept
{
    reg = fetch();
    return 8;
}

uint32_t cpu::op_ld_n(uint16_t addr) noexcept
{
    mem->write(addr, fetch());
    return 12;
}

uint32_t cpu::op_ld(uint8_t& dst, uint8_t val) noexcept
{
    dst = val;
    return 4;
}

uint32_t cpu::op_ld(uint8_t& dst, uint16_t addr) noexcept
{
    dst = mem->read(addr);
    return 8;
}

uint32_t cpu::op_ld(uint16_t addr, uint8_t val) noexcept
{
    mem->write(addr, val);
    return 8;
}

uint32_t cpu::op_ld_nn(uint8_t& dst) noexcept
{
    uint16_t addr = fetch16();
    dst = mem->read(addr);
    return 16;
}

uint32_t cpu::op_ld_nn(uint8_t val) noexcept
{
    mem->write(fetch16(), val);
    return 16;
}

uint32_t cpu::op_ldd_A() noexcept
{
    A = mem->read(HL);
    HL--;
    return 8;
}

uint32_t cpu::op_ldd_HL() noexcept
{
    mem->write(HL, A);
    HL--;
    return 8;
}

uint32_t cpu::op_ldi_A() noexcept
{
    A = mem->read(HL);
    HL++;
    return 8;
}

uint32_t cpu::op_ldi_HL() noexcept
{
    mem->write(HL, A);
    HL++;
    return 8;
}

uint32_t cpu::op_ldh_A() noexcept
{
    mem->write(0xff00 + fetch(), A);
    return 12;
}

uint32_t cpu::op_ldh_n() noexcept
{
    A = mem->read(0xff00 + fetch());
    return 12;
}

uint32_t cpu::op_ld16(uint16_t& reg) noexcept
{
    reg = fetch16();
    return 12;
}

uint32_t cpu::op_ld16(uint16_t& reg, uint16_t val) noexcept
{
    reg = val;
    return 8;
}

uint32_t cpu::op_ld16_HL() noexcept
{
    auto immd = static_cast<int8_t>(fetch());
    HL = mem->read(sp + immd);

    reset_zero();
    reset_sub();

    if (immd >= 0)
    {
        half_carry(check_add_half_carry(sp, static_cast<uint16_t>(immd)));
        carry(check_add_carry(sp, static_cast<uint16_t>(immd)));
    }
    else
    {
        immd *= -1;
        half_carry(check_sub_half_carry(sp, static_cast<uint16_t>(immd)));
        carry(check_sub_carry(sp, static_cast<uint16_t>(immd)));
    }

    return 12;
}

uint32_t cpu::op_ld16_nn() noexcept
{
    mem->write(fetch16(), sp);
    return 20;
}

uint32_t cpu::op_push(uint16_t val) noexcept
{
    mem->write(sp, val);
    sp -= 2;
    return 16;
}

uint32_t cpu::op_pop(uint16_t& reg) noexcept
{
    reg = mem->read16(sp);
    sp += 2;
    return 12;
}

uint32_t cpu::op_add(uint8_t& reg, uint8_t val) noexcept
{
    auto res = reg + val;

    zero(res == 0);
    reset_sub();
    half_carry(check_add_half_carry(reg, val));
    carry(check_add_carry(reg, val));

    reg = res;
    return 4;
}

uint32_t cpu::op_add(uint8_t& reg, uint16_t addr) noexcept
{
    op_add(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_add_n(uint8_t& reg) noexcept
{
    op_add(reg, fetch());
    return 8;
}

uint32_t cpu::op_adc(uint8_t& reg, uint8_t val) noexcept
{
    auto res = reg + val;
    if (carry())
        res += 1;

    zero(res == 0);
    reset_sub();
    half_carry(check_add_half_carry(reg, val));
    carry(check_add_carry(reg, val));

    reg = res;
    return 4;
}

uint32_t cpu::op_adc(uint8_t& reg, uint16_t addr) noexcept
{
    op_adc(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_adc_n(uint8_t& reg) noexcept
{
    op_adc(reg, fetch());
    return 8;
}

uint32_t cpu::op_sub(uint8_t& reg, uint8_t val) noexcept
{
    auto res = reg - val;

    zero(res == 0);
    set_sub();
    half_carry(check_add_half_carry(reg, val));
    carry(check_add_carry(reg, val));

    reg = res;
    return 4;
}

uint32_t cpu::op_sub(uint8_t& reg, uint16_t addr) noexcept
{
    op_sub(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_sub_n(uint8_t& reg) noexcept
{
    op_sub(reg, fetch());
    return 8;
}

uint32_t cpu::op_sbc(uint8_t& reg, uint8_t val) noexcept
{
    auto res = reg - val;
    if (carry())
        res += 1;

    zero(res == 0);
    set_sub();
    half_carry(check_add_half_carry(reg, val));
    carry(check_add_carry(reg, val));

    reg = res;
    return 4;
}

uint32_t cpu::op_sbc(uint8_t& reg, uint16_t addr) noexcept
{
    op_sub(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_sbc_n(uint8_t& reg) noexcept
{
    op_sub(reg, fetch());
    return 8;
}

uint32_t cpu::op_and(uint8_t& reg, uint8_t val) noexcept
{
    reg &= val;

    zero(reg == 0);
    reset_sub();
    set_half_carry();
    reset_carry();

    return 4;
}

uint32_t cpu::op_and(uint8_t& reg, uint16_t addr) noexcept
{
    op_and(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_and_n(uint8_t& reg) noexcept
{
    op_and(reg, fetch());
    return 8;
}

uint32_t cpu::op_or(uint8_t& reg, uint8_t val) noexcept
{
    reg |= val;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    reset_carry();

    return 4;
}

uint32_t cpu::op_or(uint8_t& reg, uint16_t addr) noexcept
{
    op_or(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_or_n(uint8_t& reg) noexcept
{
    op_or(reg, fetch());
    return 8;
}

uint32_t cpu::op_xor(uint8_t& reg, uint8_t val) noexcept
{
    reg ^= val;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    reset_carry();

    return 4;
}

uint32_t cpu::op_xor(uint8_t& reg, uint16_t addr) noexcept
{
    op_xor(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_xor_n(uint8_t& reg) noexcept
{
    op_xor(reg, fetch());
    return 8;
}

uint32_t cpu::op_cp(uint8_t& reg, uint8_t val) noexcept
{
    zero(reg - val == 0);
    set_sub();
    half_carry(check_sub_half_carry(reg, val));
    carry(check_sub_carry(reg, val));

    return 4;
}

uint32_t cpu::op_cp(uint8_t& reg, uint16_t addr) noexcept
{
    op_cp(reg, mem->read(addr));
    return 8;
}

uint32_t cpu::op_cp_n(uint8_t& reg) noexcept
{
    op_cp(reg, fetch());
    return 8;
}

uint32_t cpu::op_inc(uint8_t& reg) noexcept
{
    auto res = reg + 1;

    zero(res == 0);
    reset_sub();
    half_carry(check_add_half_carry(reg, 1_u8));
    // carry not affected

    reg = res;
    return 4;
}

uint32_t cpu::op_inc(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_inc(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_dec(uint8_t& reg) noexcept
{
    auto res = reg - 1;

    zero(res == 0);
    set_sub();
    half_carry(check_sub_half_carry(reg, 1_u8));
    // carry not affected

    return 4;
}

uint32_t cpu::op_dec(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_dec(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_add(uint16_t& reg, uint16_t val) noexcept
{
    // zero not affected
    reset_sub();
    half_carry(check_add_half_carry(reg, val));
    carry(check_add_carry(reg, val));

    reg += val;
    return 8;
}

uint32_t cpu::op_add_sp() noexcept
{
    reset_zero();
    reset_sub();

    auto val = static_cast<int8_t>(fetch());

    if (val >= 0)
    {
        half_carry(check_add_half_carry(sp, static_cast<uint16_t>(val)));
        carry(check_add_carry(sp, static_cast<uint16_t>(val)));
        sp += val;
    }
    else
    {
        val *= -1;
        half_carry(check_sub_half_carry(sp, static_cast<uint16_t>(val)));
        carry(check_sub_carry(sp, static_cast<uint16_t>(val)));
        sp -= val;
    }

    return 16;
}

uint32_t cpu::op_inc16(uint16_t& reg) noexcept
{
    reg++;
    return 8;
}

uint32_t cpu::op_dec16(uint16_t& reg) noexcept
{
    reg--;
    return 8;
}

uint32_t cpu::op_swap(uint8_t& reg) noexcept
{
    reg = (reg & 0x0f << 4) | (reg & 0xf0 >> 4);

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    reset_carry();

    return 4;
}

uint32_t cpu::op_swap(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    val = (val & 0x0f) << 4 | (val & 0xf0) >> 4;
    mem->write(addr, val);

    zero(val == 0);
    reset_sub();
    reset_half_carry();
    reset_carry();

    return 12;
}

uint32_t cpu::op_daa() noexcept
{
    // NOTE: this is a complex and poorly documented instruction

    if (sub())
    {
        if (carry())
        {
            A -= 0x60_u8;
            set_carry();
        }

        if (half_carry())
            A -= 0x06_u8;
    }
    else
    {
        if (carry() || A > 0x99)
        {
            A += 0x60_u8;
            set_carry();
        }

        if (half_carry() || (A & 0x0f) > 9)
            A += 0x06_u8;
    }

    zero(A == 0);
    // sub flag not affected
    reset_half_carry();
    // carry flag is set (or unchanged) above

    return 4;
}

uint32_t cpu::op_cpl() noexcept
{
    A = ~A;

    // zero unaffected
    set_sub();
    set_half_carry();
    // carry unaffected

    return 4;
}

uint32_t cpu::op_ccf() noexcept
{
    // zero unaffected
    reset_sub();
    reset_half_carry();
    carry(!carry());
    return 4;
}

uint32_t cpu::op_scf() noexcept
{
    // zero unaffected
    reset_sub();
    reset_half_carry();
    set_carry();
    return 4;
}

uint32_t cpu::op_nop() noexcept
{
    return 4;
}

uint32_t cpu::op_halt() noexcept
{
    pipeline.push(action::halt);
    return 4;
}

uint32_t cpu::op_stop() noexcept
{
    op_halt();
    // TODO halt display
    // TODO keep paused until a button is pressed
    return 4;
}

uint32_t cpu::op_di() noexcept
{
    pipeline.push(action::disable_interrupts);
    return 4;
}

uint32_t cpu::op_ei() noexcept
{
    pipeline.push(action::enable_interrupts);
    return 4;
}

uint32_t cpu::op_rlc(uint8_t& reg) noexcept
{
    auto msb = (reg & 0x80) != 0;
    reg <<= 1;

    if (carry()) reg |= 0x01;
    else         reg &= 0xfe;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(msb);

    return 4;
}

uint32_t cpu::op_rlc(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_rlc(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_rl(uint8_t& reg) noexcept
{
    auto msb = (reg & 0x80) != 0;
    reg <<= 1;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(msb);

    return 4;
}

uint32_t cpu::op_rl(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_rl(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_rrc(uint8_t& reg) noexcept
{
    auto lsb = (reg & 0x01) != 0;
    reg >>= 1;

    if (carry()) reg |= 0x80;
    else         reg &= 0x7f;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(lsb);

    return 4;
}

uint32_t cpu::op_rrc(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_rrc(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_rr(uint8_t& reg) noexcept
{
    auto lsb = (reg & 0x01) != 0;
    reg >>= 1;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(lsb);

    return 4;
}

uint32_t cpu::op_rr(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_rr(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_sla(uint8_t& reg) noexcept
{
    auto msb = (reg & 0x80) != 0;
    reg <<= 1;
    reg &= 0xfe;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(msb);

    return 4;
}

uint32_t cpu::op_sla(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_sla(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_sra(uint8_t& reg) noexcept
{
    auto lsb = (reg & 0x01) != 0;
    auto msb = reg & 0x80;
    reg >>= 1;
    reg |= msb;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(lsb);

    return 4;
}

uint32_t cpu::op_sra(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_sra(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_srl(uint8_t& reg) noexcept
{
    auto lsb = (reg & 0x01) != 0;
    reg >>= 1;
    reg &= 0x7f;

    zero(reg == 0);
    reset_sub();
    reset_half_carry();
    carry(lsb);

    return 4;
}

uint32_t cpu::op_srl(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    op_srl(val);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_bit(uint8_t& reg, uint8_t n) noexcept
{
    zero((reg & 1 << n) == 0);
    reset_sub();
    set_half_carry();
    // carry unaffected
    return 4;
}

uint32_t cpu::op_bit(uint16_t addr, uint8_t n) noexcept
{
    auto val = mem->read(addr);
    op_bit(val, n);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_set(uint8_t& reg, uint8_t n) noexcept
{
    reg |= 1 << n;
    return 4;
}

uint32_t cpu::op_set(uint16_t addr, uint8_t n) noexcept
{
    auto val = mem->read(addr);
    op_bit(val, n);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_res(uint8_t& reg, uint8_t n) noexcept
{
    reg &= ~(1 << n);
    return 4;
}

uint32_t cpu::op_res(uint16_t addr, uint8_t n) noexcept
{
    auto val = mem->read(addr);
    op_bit(val, n);
    mem->write(addr, val);
    return 12;
}

uint32_t cpu::op_jp() noexcept
{
    pc = fetch16();
    return 12;
}

uint32_t cpu::op_jp(uint16_t addr) noexcept
{
    pc = addr;
    return 4;
}

uint32_t cpu::op_jp(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!zero()) op_jp();
        break;
    case condition::Z:
        if (zero()) op_jp();
        break;
    case condition::NC:
        if (!carry()) op_jp();
        break;
    case condition::C:
        if (carry()) op_jp();
        break;
    }
    return 12;
}

uint32_t cpu::op_jr() noexcept
{
    pc += static_cast<int8_t>(fetch());
    return 8;
}

uint32_t cpu::op_jr(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!zero()) op_jr();
        break;
    case condition::Z:
        if (zero()) op_jr();
        break;
    case condition::NC:
        if (!carry()) op_jr();
        break;
    case condition::C:
        if (carry()) op_jr();
        break;
    }
    return 8;
}

uint32_t cpu::op_call() noexcept
{
    auto addr = fetch16();
    op_push(pc);
    pc = addr;
    return 12;
}

uint32_t cpu::op_call(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!zero()) op_call();
        break;
    case condition::Z:
        if (zero()) op_call();
        break;
    case condition::NC:
        if (!carry()) op_call();
        break;
    case condition::C:
        if (carry()) op_call();
        break;
    }
    return 12;
}

uint32_t cpu::op_rst(uint8_t base) noexcept
{
    // TODO wtf is this supposed to do, and why so many cycles?

    op_push(pc);
    pc = mem->read(base);
    return 32;
}

uint32_t cpu::op_ret() noexcept
{
    uint16_t addr;
    op_pop(addr);
    pc = addr;
    return 8;
}

uint32_t cpu::op_ret(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!zero()) op_ret();
        break;
    case condition::Z:
        if (zero()) op_ret();
        break;
    case condition::NC:
        if (!carry()) op_ret();
        break;
    case condition::C:
        if (carry()) op_ret();
        break;
    }
    return 8;
}

uint32_t cpu::op_reti() noexcept
{
    op_ret();
    op_ei();
    return 8;
}

}
