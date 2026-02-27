#include "converter.h"

#include <math.h>
#include <stdint.h>

int convert_frame(
    const uint8_t *pixels,
    int frame_w, int frame_h,
    int cols, int rows,
    const char *ramp, int ramp_len,
    Cell *out
) {
    if (!pixels || !ramp || !out)              return -1;
    if (ramp_len < 2 || ramp_len > 1024)      return -2;
    if (cols <= 0 || rows <= 0)               return -3;
    if (frame_w <= 0 || frame_h <= 0)         return -3;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {

            /* map this cell to its pixel region (float precision) */
            float x_start = (float)col       * frame_w / cols;
            float x_end   = (float)(col + 1) * frame_w / cols;
            float y_start = (float)row        * frame_h / rows;
            float y_end   = (float)(row + 1)  * frame_h / rows;

            int px_start = (int)x_start;
            int px_end   = (int)ceilf(x_end);   /* exclusive */
            int py_start = (int)y_start;
            int py_end   = (int)ceilf(y_end);   /* exclusive */

            float acc_r = 0.0f, acc_g = 0.0f, acc_b = 0.0f, acc_w = 0.0f;

            for (int py = py_start; py < py_end; py++) {
                float wy = fminf((float)(py + 1), y_end) - fmaxf((float)py, y_start);
                if (wy <= 0.0f) continue;

                for (int px = px_start; px < px_end; px++) {
                    float wx = fminf((float)(px + 1), x_end) - fmaxf((float)px, x_start);
                    if (wx <= 0.0f) continue;

                    float w = wx * wy;
                    const uint8_t *p = pixels + (py * frame_w + px) * 4;

                    acc_r += p[0] * w;
                    acc_g += p[1] * w;
                    acc_b += p[2] * w;
                    acc_w += w;
                }
            }

            uint8_t r = 0, g = 0, b = 0;
            if (acc_w > 0.0f) {
                r = (uint8_t)(acc_r / acc_w);
                g = (uint8_t)(acc_g / acc_w);
                b = (uint8_t)(acc_b / acc_w);
            }

            /* BT.601 luma */
            float luma = 0.299f * r + 0.587f * g + 0.114f * b;

            int idx = (int)(luma * (ramp_len - 1) / 255.0f);
            if (idx >= ramp_len) idx = ramp_len - 1;

            out[row * cols + col] = (Cell){ ramp[idx], r, g, b };
        }
    }

    return 0;
}
