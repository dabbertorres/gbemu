#pragma once

#include <cstdint>

struct registers
{
    union
    {
        struct
        {
            uint8_t F;
            uint8_t A;
        };

        uint16_t AF;
    };

    union
    {
        struct
        {
            uint8_t C;
            uint8_t B;
        };

        uint16_t BC;
    };

    union
    {
        struct
        {
            uint8_t E;
            uint8_t D;
        };

        uint16_t DE;
    };

    union
    {
        struct
        {
            uint8_t L;
            uint8_t H;
        };

        uint16_t HL;
    };

    uint16_t sp = 0xFFFE;
    uint16_t pc = 0x0100;

    // flags
    [[nodiscard]] bool zero() const noexcept { return (F & 0x80) != 0; }

    void zero(bool set) noexcept
    {
        if (set) set_zero();
        else reset_zero();
    }

    void set_zero() noexcept { F |= 0x80; }
    void reset_zero() noexcept { F &= ~0x80; }

    [[nodiscard]] bool sub() const noexcept { return (F & 0x40) != 0; }
    void               sub(bool b) noexcept
    {
        if (b) set_sub();
        else reset_sub();
    }

    void set_sub() noexcept { F |= 0x40; }
    void reset_sub() noexcept { F &= ~0x40; }

    [[nodiscard]] bool half_carry() const noexcept { return (F & 0x20) != 0; }
    void               half_carry(bool b) noexcept
    {
        if (b) set_half_carry();
        else reset_half_carry();
    }

    void set_half_carry() noexcept { F |= 0x20; }
    void reset_half_carry() noexcept { F &= ~0x20; }

    [[nodiscard]] bool carry() const noexcept { return (F & 0x10) != 0; }
    void               carry(bool b) noexcept
    {
        if (b) set_carry();
        else reset_carry();
    }

    void set_carry() noexcept { F |= 0x10; }
    void reset_carry() noexcept { F &= ~0x10; }
};
