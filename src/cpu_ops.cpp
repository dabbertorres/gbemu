#include "cpu.hpp"
#include "memory.hpp"
#include "util.hpp"

namespace gb
{

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

uint32_t cpu::op_ld_from_nn(uint8_t& dst) noexcept
{
    uint16_t addr = fetch16();
    dst           = mem->read(addr);
    return 16;
}

uint32_t cpu::op_ld_to_nn(uint8_t val) noexcept
{
    mem->write(fetch16(), val);
    return 16;
}

uint32_t cpu::op_ldd_A() noexcept
{
    r.A = mem->read(r.HL);
    r.HL--;
    return 8;
}

uint32_t cpu::op_ldd_HL() noexcept
{
    mem->write(r.HL, r.A);
    r.HL--;
    return 8;
}

uint32_t cpu::op_ldi_A() noexcept
{
    r.A = mem->read(r.HL);
    r.HL++;
    return 8;
}

uint32_t cpu::op_ldi_HL() noexcept
{
    mem->write(r.HL, r.A);
    r.HL++;
    return 8;
}

uint32_t cpu::op_ldh_A() noexcept
{
    mem->write(0xff00 + fetch(), r.A);
    return 12;
}

uint32_t cpu::op_ldh_n() noexcept
{
    r.A = mem->read(0xff00 + fetch());
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
    r.HL      = mem->read(r.sp + immd);

    r.reset_zero();
    r.reset_sub();

    if (immd >= 0)
    {
        r.half_carry(check_add_half_carry(r.sp, static_cast<uint16_t>(immd)));
        r.carry(check_add_carry(r.sp, static_cast<uint16_t>(immd)));
    }
    else
    {
        immd *= -1;
        r.half_carry(check_sub_half_carry(r.sp, static_cast<uint16_t>(immd)));
        r.carry(check_sub_carry(r.sp, static_cast<uint16_t>(immd)));
    }

    return 12;
}

uint32_t cpu::op_ld16_nn() noexcept
{
    mem->write16(fetch16(), r.sp);
    return 20;
}

uint32_t cpu::op_push(uint16_t val) noexcept
{
    mem->write16(r.sp, val);
    r.sp -= 2;
    return 16;
}

uint32_t cpu::op_pop(uint16_t& reg) noexcept
{
    reg = mem->read16(r.sp);
    r.sp += 2;
    return 12;
}

uint32_t cpu::op_add(uint8_t& reg, uint8_t val) noexcept
{
    auto res = reg + val;

    r.zero(res == 0);
    r.reset_sub();
    r.half_carry(check_add_half_carry(reg, val));
    r.carry(check_add_carry(reg, val));

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
    if (r.carry()) res += 1;

    r.zero(res == 0);
    r.reset_sub();
    r.half_carry(check_add_half_carry(reg, val));
    r.carry(check_add_carry(reg, val));

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

    r.zero(res == 0);
    r.set_sub();
    r.half_carry(check_add_half_carry(reg, val));
    r.carry(check_add_carry(reg, val));

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
    if (r.carry()) res += 1;

    r.zero(res == 0);
    r.set_sub();
    r.half_carry(check_add_half_carry(reg, val));
    r.carry(check_add_carry(reg, val));

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

    r.zero(reg == 0);
    r.reset_sub();
    r.set_half_carry();
    r.reset_carry();

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.reset_carry();

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.reset_carry();

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
    r.zero(reg - val == 0);
    r.set_sub();
    r.half_carry(check_sub_half_carry(reg, val));
    r.carry(check_sub_carry(reg, val));

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

    r.zero(res == 0);
    r.reset_sub();
    r.half_carry(check_add_half_carry(reg, 1_u8));
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

    r.zero(res == 0);
    r.set_sub();
    r.half_carry(check_sub_half_carry(reg, 1_u8));
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
    r.reset_sub();
    r.half_carry(check_add_half_carry(reg, val));
    r.carry(check_add_carry(reg, val));

    reg += val;
    return 8;
}

uint32_t cpu::op_add_sp() noexcept
{
    r.reset_zero();
    r.reset_sub();

    auto val = static_cast<int8_t>(fetch());

    if (val >= 0)
    {
        r.half_carry(check_add_half_carry(r.sp, static_cast<uint16_t>(val)));
        r.carry(check_add_carry(r.sp, static_cast<uint16_t>(val)));
        r.sp += val;
    }
    else
    {
        val *= -1;
        r.half_carry(check_sub_half_carry(r.sp, static_cast<uint16_t>(val)));
        r.carry(check_sub_carry(r.sp, static_cast<uint16_t>(val)));
        r.sp -= val;
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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.reset_carry();

    return 4;
}

uint32_t cpu::op_swap(uint16_t addr) noexcept
{
    auto val = mem->read(addr);
    val      = (val & 0x0f) << 4 | (val & 0xf0) >> 4;
    mem->write(addr, val);

    r.zero(val == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.reset_carry();

    return 12;
}

uint32_t cpu::op_daa() noexcept
{
    // NOTE: this is a complex and poorly documented instruction

    if (r.sub())
    {
        if (r.carry())
        {
            r.A -= 0x60_u8;
            r.set_carry();
        }

        if (r.half_carry()) r.A -= 0x06_u8;
    }
    else
    {
        if (r.carry() || r.A > 0x99)
        {
            r.A += 0x60_u8;
            r.set_carry();
        }

        if (r.half_carry() || (r.A & 0x0f) > 9) r.A += 0x06_u8;
    }

    r.zero(r.A == 0);
    // sub flag not affected
    r.reset_half_carry();
    // carry flag is set (or unchanged) above

    return 4;
}

uint32_t cpu::op_cpl() noexcept
{
    r.A = ~r.A;

    // zero unaffected
    r.set_sub();
    r.set_half_carry();
    // carry unaffected

    return 4;
}

uint32_t cpu::op_ccf() noexcept
{
    // zero unaffected
    r.reset_sub();
    r.reset_half_carry();
    r.carry(!r.carry());
    return 4;
}

uint32_t cpu::op_scf() noexcept
{
    // zero unaffected
    r.reset_sub();
    r.reset_half_carry();
    r.set_carry();
    return 4;
}

uint32_t cpu::op_nop() noexcept { return 4; }

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
    /* pipeline.push(action::disable_interrupts); */
    interrupts_enabled = false;
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

    if (r.carry()) reg |= 0x01;
    else reg &= 0xfe;

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(msb);

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(msb);

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

    if (r.carry()) reg |= 0x80;
    else reg &= 0x7f;

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(lsb);

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(lsb);

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(msb);

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(lsb);

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

    r.zero(reg == 0);
    r.reset_sub();
    r.reset_half_carry();
    r.carry(lsb);

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
    r.zero((reg & 1 << n) == 0);
    r.reset_sub();
    r.set_half_carry();
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
    r.pc = fetch16();
    return 12;
}

uint32_t cpu::op_jp(uint16_t addr) noexcept
{
    r.pc = addr;
    return 4;
}

uint32_t cpu::op_jp(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!r.zero()) op_jp();
        break;
    case condition::Z:
        if (r.zero()) op_jp();
        break;
    case condition::NC:
        if (!r.carry()) op_jp();
        break;
    case condition::C:
        if (r.carry()) op_jp();
        break;
    }
    return 12;
}

uint32_t cpu::op_jr() noexcept
{
    r.pc += static_cast<int8_t>(fetch());
    return 8;
}

uint32_t cpu::op_jr(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!r.zero()) op_jr();
        break;
    case condition::Z:
        if (r.zero()) op_jr();
        break;
    case condition::NC:
        if (!r.carry()) op_jr();
        break;
    case condition::C:
        if (r.carry()) op_jr();
        break;
    }
    return 8;
}

uint32_t cpu::op_call() noexcept
{
    auto addr = fetch16();
    op_push(r.pc);
    r.pc = addr;
    return 12;
}

uint32_t cpu::op_call(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!r.zero()) op_call();
        break;
    case condition::Z:
        if (r.zero()) op_call();
        break;
    case condition::NC:
        if (!r.carry()) op_call();
        break;
    case condition::C:
        if (r.carry()) op_call();
        break;
    }
    return 12;
}

uint32_t cpu::op_rst(uint8_t base) noexcept
{
    // TODO wtf is this supposed to do, and why so many cycles?

    op_push(r.pc);
    r.pc = mem->read(base);
    return 32;
}

uint32_t cpu::op_ret() noexcept
{
    uint16_t addr = 0;
    op_pop(addr);
    r.pc = addr;
    return 8;
}

uint32_t cpu::op_ret(condition cond) noexcept
{
    switch (cond)
    {
    case condition::NZ:
        if (!r.zero()) op_ret();
        break;
    case condition::Z:
        if (r.zero()) op_ret();
        break;
    case condition::NC:
        if (!r.carry()) op_ret();
        break;
    case condition::C:
        if (r.carry()) op_ret();
        break;
    }
    return 8;
}

uint32_t cpu::op_reti() noexcept
{
    op_ret();
    interrupts_enabled = true;
    return 8;
}

}
