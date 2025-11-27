#pragma once

#include <cstdint>

// ----------------------------------------------------------
// RGB struct
// ----------------------------------------------------------
struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    RGB() : r(0), g(0), b(0) {}
    RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};