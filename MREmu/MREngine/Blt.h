#pragma once

struct ColorRGBA { uint8_t r, g, b, a; };

typedef ColorRGBA(*read_color_t)(const void* buf, int idx, int trans_color);
typedef void (*write_color_t)(void* buf, int idx, int trans_color, ColorRGBA c);

