#pragma once

#include <array>
#include <cstdint>
#include <ranges>
#include <vector>

namespace gb {

  struct rom {
    rom(size_t size) : data{} { data.resize(size); }

    std::vector<uint8_t> data;

    // auto entry_point() const noexcept
    //{
    //     constexpr auto off = 0x0100;
    //     return data | std::views::drop(off) | std::views::take(4);
    // }

    // auto logo() const noexcept
    //{
    //     constexpr auto off = 0x0104;
    //     constexpr auto length = 0x0133 - off + 1;
    //     return data | std::views::drop(off) | std::views::take(length);
    // }

    // auto title() const noexcept
    //{
    //     constexpr auto off = 0x0134;
    //     constexpr auto length = 0x0143 - off + 1;
    //     return data | std::views::drop(off) | std::views::take(length);
    // }
  };

}  // namespace gb
