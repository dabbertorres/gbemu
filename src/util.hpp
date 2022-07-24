#pragma once

#include <cstdint>

namespace gb::util
{

// promote is a helper type for selecting the next larger type integer type
template<typename T>
struct promote;

template<>
struct promote<uint8_t>
{
    using type = uint16_t;
};

template<>
struct promote<uint16_t>
{
    using type = uint32_t;
};

template<>
struct promote<uint32_t>
{
    using type = uint64_t;
};

template<typename T>
using promote_t = typename promote<T>::type;

}

constexpr uint8_t  operator"" _u8(unsigned long long v) { return static_cast<uint8_t>(v); }
constexpr uint16_t operator"" _u16(unsigned long long v) { return static_cast<uint16_t>(v); }
constexpr uint32_t operator"" _u32(unsigned long long v) { return static_cast<uint32_t>(v); }
constexpr uint64_t operator"" _u64(unsigned long long v) { return static_cast<uint64_t>(v); }
