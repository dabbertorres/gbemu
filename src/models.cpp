#include "models.hpp"

namespace gb
{

struct model_registers
{
    uint16_t af;
    uint16_t bc;
    uint16_t de;
    uint16_t hl;
};

static const model_registers original            = {0x01B0, 0x0013, 0x00D8, 0x014D};
static const model_registers pocket              = {0xFFB0, 0x0013, 0x00D8, 0x014D};
static const model_registers super               = {0x0100, 0x0014, 0x0000, 0xC060};
static const model_registers super2              = {0xFF00, 0x0014, 0x0000, 0x0000};
static const model_registers color               = {0x1180, 0x0000, 0x0008, 0x007C};
static const model_registers color_in_color      = {0x1180, 0x0000, 0xFF56, 0x000D};
static const model_registers advance             = {0x1100, 0x0100, 0x0008, 0x007C};
static const model_registers advance_in_color    = {0x1100, 0x0100, 0xFF56, 0x000D};
static const model_registers advance_sp          = {0x1100, 0x0100, 0x0008, 0x007C};
static const model_registers advance_sp_in_color = {0x1100, 0x0100, 0x0008, 0x007C};

void initialize_registers(model m, registers& r, bool color_game) noexcept
{
    const model_registers* values = nullptr;

    switch (m)
    {
    case model::original:
        values = &original; //
        break;

    case model::pocket:
        values = &pocket; //
        break;

    case model::super:
        values = &super; //
        break;

    case model::super2:
        values = &super2; //
        break;

    case model::color:
        if (color_game) values = &color_in_color;
        else values = &color;
        break;

    case model::advance:
        if (color_game) values = &advance_in_color;
        else values = &advance;
        break;

    case model::advance_sp:
        if (color_game) values = &advance_sp_in_color;
        else values = &advance_sp;
        break;
    }

    r.AF = values->af;
    r.BC = values->bc;
    r.DE = values->de;
    r.HL = values->hl;
}

}
