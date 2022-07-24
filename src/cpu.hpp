#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <stack>

#include "models.hpp"
#include "registers.hpp"
#include "util.hpp"

namespace gb
{

struct memory;

enum class interrupt : uint8_t
{
    vblank   = 1U << 0U,
    lcd_stat = 1U << 1U,
    timer    = 1U << 2U,
    serial   = 1U << 3U,
    joypad   = 1U << 4U,

    END = 1U << 5U,
};

struct cpu
{
public:
    explicit cpu(std::unique_ptr<memory>&& mem, model model) noexcept;

    void run() noexcept;
    void stop() noexcept;
    void queue_interrupt(interrupt type) noexcept;

private:
    enum class condition : uint8_t
    {
        NZ, // if Z flag is clear
        Z,  // if Z flag is set
        NC, // if C flag is clear
        C,  // if C flag is set
    };

    enum class action : uint8_t
    {
        execute,
        halt,
        disable_interrupts,
        enable_interrupts,
    };

    uint8_t  fetch() noexcept;
    uint16_t fetch16() noexcept;

    void     process_interrupts() noexcept;
    void     update_lcd() noexcept;
    void     update_timers() noexcept;
    uint32_t execute(uint8_t op) noexcept;

    template<std::unsigned_integral T>
    static constexpr bool check_add_half_carry(T a, T b) noexcept
    {
        constexpr T mask = std::numeric_limits<T>::max() >> 8;
        return ((a & mask) + (b & mask)) > mask;
    }

    template<std::unsigned_integral T>
    static constexpr bool check_add_carry(T a, T b) noexcept
    {
        using N = util::promote_t<T>;
        return static_cast<N>(a) + static_cast<N>(b) > std::numeric_limits<T>::max();
    }

    template<std::unsigned_integral T>
    static constexpr bool check_sub_half_carry(T a, T b) noexcept
    {
        constexpr T mask = std::numeric_limits<T>::max() >> 8;
        return static_cast<int>(a & mask) - static_cast<int>(b & mask) < 0;
    }

    template<std::unsigned_integral T>
    static constexpr bool check_sub_carry(T a, T b) noexcept
    {
        return static_cast<int>(a) - static_cast<int>(b) < 0;
    }

    // all instruction implementations return the number of cycles spent

    // 8-bit loads
    uint32_t op_ld_n(uint8_t& reg) noexcept;
    uint32_t op_ld_n(uint16_t addr) noexcept;
    uint32_t op_ld(uint8_t& dst, uint8_t val) noexcept;
    uint32_t op_ld(uint8_t& dst, uint16_t addr) noexcept;
    uint32_t op_ld(uint16_t addr, uint8_t val) noexcept;
    uint32_t op_ld_from_nn(uint8_t& dst) noexcept;
    uint32_t op_ld_to_nn(uint8_t val) noexcept;
    // the suffix in the following functions is the destination of each ld instruction
    uint32_t op_ldd_A() noexcept;
    uint32_t op_ldd_HL() noexcept;
    uint32_t op_ldi_A() noexcept;
    uint32_t op_ldi_HL() noexcept;
    uint32_t op_ldh_A() noexcept;
    uint32_t op_ldh_n() noexcept;

    // 16-bit loads
    uint32_t op_ld16(uint16_t& reg) noexcept;
    uint32_t op_ld16(uint16_t& reg, uint16_t val) noexcept;
    uint32_t op_ld16_HL() noexcept;
    uint32_t op_ld16_nn() noexcept;

    // stack ops
    uint32_t op_push(uint16_t val) noexcept;
    uint32_t op_pop(uint16_t& reg) noexcept;

    // 8-bit alu
    uint32_t op_add(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_add(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_add_n(uint8_t& reg) noexcept;

    uint32_t op_adc(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_adc(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_adc_n(uint8_t& reg) noexcept;

    uint32_t op_sub(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_sub(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_sub_n(uint8_t& reg) noexcept;

    uint32_t op_sbc(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_sbc(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_sbc_n(uint8_t& reg) noexcept;

    uint32_t op_and(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_and(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_and_n(uint8_t& reg) noexcept;

    uint32_t op_or(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_or(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_or_n(uint8_t& reg) noexcept;

    uint32_t op_xor(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_xor(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_xor_n(uint8_t& reg) noexcept;

    uint32_t op_cp(uint8_t& reg, uint8_t val) noexcept;
    uint32_t op_cp(uint8_t& reg, uint16_t addr) noexcept;
    uint32_t op_cp_n(uint8_t& reg) noexcept;

    uint32_t op_inc(uint8_t& reg) noexcept;
    uint32_t op_inc(uint16_t addr) noexcept;

    uint32_t op_dec(uint8_t& reg) noexcept;
    uint32_t op_dec(uint16_t addr) noexcept;

    // 16-bit alu
    uint32_t op_add(uint16_t& reg, uint16_t val) noexcept;
    uint32_t op_add_sp() noexcept;
    uint32_t op_inc16(uint16_t& reg) noexcept;
    uint32_t op_dec16(uint16_t& reg) noexcept;

    // misc
    uint32_t op_swap(uint8_t& reg) noexcept;
    uint32_t op_swap(uint16_t addr) noexcept;
    uint32_t op_daa() noexcept; // "Decimal Adjust" register A
    uint32_t op_cpl() noexcept; // complement register A
    uint32_t op_ccf() noexcept; // complement carry flag
    uint32_t op_scf() noexcept; // set carry flag
    uint32_t op_nop() noexcept;
    uint32_t op_halt() noexcept;
    uint32_t op_stop() noexcept;
    uint32_t op_di() noexcept; // disable interrupts
    uint32_t op_ei() noexcept; // enable interrupts

    // rotates and shifts
    uint32_t op_rlc(uint8_t& reg) noexcept;  // rotate left into carry
    uint32_t op_rlc(uint16_t addr) noexcept; // rotate left into carry
    uint32_t op_rl(uint8_t& reg) noexcept;   // rotate left
    uint32_t op_rl(uint16_t addr) noexcept;  // rotate left
    uint32_t op_rrc(uint8_t& reg) noexcept;  // rotate right into carry
    uint32_t op_rrc(uint16_t addr) noexcept; // rotate right into carry
    uint32_t op_rr(uint8_t& reg) noexcept;   // rotate right
    uint32_t op_rr(uint16_t addr) noexcept;  // rotate right

    uint32_t op_sla(uint8_t& reg) noexcept;  // shift left
    uint32_t op_sla(uint16_t addr) noexcept; // shift left
    uint32_t op_sra(uint8_t& reg) noexcept;  // shift right
    uint32_t op_sra(uint16_t addr) noexcept; // shift right
    uint32_t op_srl(uint8_t& reg) noexcept;  // shift right
    uint32_t op_srl(uint16_t addr) noexcept; // shift right

    // bit ops
    uint32_t op_bit(uint8_t& reg, uint8_t n) noexcept;
    uint32_t op_bit(uint16_t addr, uint8_t n) noexcept;
    uint32_t op_set(uint8_t& reg, uint8_t n) noexcept;
    uint32_t op_set(uint16_t addr, uint8_t n) noexcept;
    uint32_t op_res(uint8_t& reg, uint8_t n) noexcept;
    uint32_t op_res(uint16_t addr, uint8_t n) noexcept;

    // jumps
    uint32_t op_jp() noexcept;
    uint32_t op_jp(uint16_t addr) noexcept;
    uint32_t op_jp(condition cond) noexcept;
    uint32_t op_jr() noexcept;
    uint32_t op_jr(condition cond) noexcept;
    uint32_t op_call() noexcept;
    uint32_t op_call(condition cond) noexcept;
    uint32_t op_rst(uint8_t base) noexcept;
    uint32_t op_ret() noexcept;
    uint32_t op_ret(condition cond) noexcept;
    uint32_t op_reti() noexcept;

    const std::unique_ptr<memory> mem;

    std::stack<action> pipeline;

    std::atomic_bool running;
    bool             interrupts_enabled;
    uint32_t         cycles;
    bool             debug_mode;

    registers r;
};

}
