#pragma once

#include <cstdint>

#include "registers.hpp"

namespace gb
{

enum class model
{
    original, // aka, "dmg" = "dot matrix game"
    pocket,   // aka "light"
    super,
    super2,
    color,
    advance,
    advance_sp,
};

void initialize_registers(model m, registers& r, bool color_game) noexcept;

}
