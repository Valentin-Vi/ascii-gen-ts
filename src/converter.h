#pragma once

#include <stdint.h>

typedef struct {
    char    c;
    uint8_t r, g, b;
} Cell;

/*
 * convert_frame - convert a raw RGBA frame to an ASCII cell grid
 *
 * pixels    - RGBA pixel buffer, frame_w * frame_h * 4 bytes
 * frame_w   - source frame width in pixels
 * frame_h   - source frame height in pixels
 * cols      - output grid width in cells
 * rows      - output grid height in cells
 * ramp      - character ramp ordered dark -> bright, 2 <= ramp_len <= 1024
 * ramp_len  - number of characters in ramp
 * out       - caller-allocated output buffer, cols * rows cells
 *
 * Returns 0 on success, negative on invalid input.
 */
int convert_frame(
    const uint8_t *pixels,
    int frame_w, int frame_h,
    int cols, int rows,
    const char *ramp, int ramp_len,
    Cell *out
);
