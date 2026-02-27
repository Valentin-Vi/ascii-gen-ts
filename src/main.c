#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "converter.h"
#include "video.h"

#define DEFAULT_RAMP  " .,:;+*?%S#@"
#define DEFAULT_COLS  80
#define DEFAULT_ROWS  40
#define FRAME_W       320
#define FRAME_H       240

/* IAG binary format magic */
#define IAG_MAGIC "IAG\0"

/*
 * Generate a synthetic RGBA gradient frame for testing.
 * R increases left->right, G increases top->bottom, B is constant.
 */
static void make_gradient(uint8_t *buf, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *p = buf + (y * w + x) * 4;
            p[0] = (uint8_t)(x * 255 / (w - 1));   /* R */
            p[1] = (uint8_t)(y * 255 / (h - 1));   /* G */
            p[2] = 128;                              /* B */
            p[3] = 255;                              /* A */
        }
    }
}

static void render_cells(const Cell *cells, int cols, int rows, int color) {
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            const Cell *c = &cells[row * cols + col];
            if (color)
                printf("\033[38;2;%d;%d;%dm%c", c->r, c->g, c->b, c->c);
            else
                putchar(c->c);
        }
        if (color) printf("\033[0m");
        putchar('\n');
    }
}

/*
 * Render the entire frame into `buf` (pre-allocated) and return the number of
 * bytes written.  The caller then flushes it with a single write() syscall to
 * minimise the time the terminal sees a partial frame.
 *
 * Worst-case size per frame (color):
 *   cursor-up:  "\033[{rows}A"  ≤ 8 bytes
 *   per cell:   "\033[38;2;255;255;255mX"  = 20 bytes
 *   per row:    cols*20 + "\033[0m\n"  = cols*20 + 5
 *   total:      rows*(cols*20 + 5) + 8
 */
static size_t frame_buf_size(int cols, int rows) {
    return (size_t)rows * ((size_t)cols * 20 + 5) + 8;
}

static size_t render_cells_to_buf(char *buf, const Cell *cells,
                                   int cols, int rows, int color,
                                   int frame_index) {
    char *p = buf;

    if (frame_index > 0) {
        p += sprintf(p, "\033[%dA", rows);  /* move cursor up */
    }

    for (int row = 0; row < rows; row++) {
        *p++ = '\r';
        for (int col = 0; col < cols; col++) {
            const Cell *c = &cells[row * cols + col];
            if (color) {
                p += sprintf(p, "\033[38;2;%d;%d;%dm%c",
                             c->r, c->g, c->b, c->c);
            } else {
                *p++ = c->c;
            }
        }
        if (color) { *p++ = '\033'; *p++ = '['; *p++ = '0'; *p++ = 'm'; }
        *p++ = '\n';
    }

    return (size_t)(p - buf);
}

/* Write IAG header:
 *   0  4  magic "IAG\0"
 *   4  4  cols      (uint32_t LE)
 *   8  4  rows      (uint32_t LE)
 *  12  4  fps       (float LE)
 *  16  4  frame_count (uint32_t LE)
 */
static int write_iag_header(FILE *f, uint32_t cols, uint32_t rows,
                             float fps, uint32_t frame_count) {
    if (fwrite(IAG_MAGIC, 1, 4, f) != 4) return -1;
    if (fwrite(&cols,        4, 1, f) != 1) return -1;
    if (fwrite(&rows,        4, 1, f) != 1) return -1;
    if (fwrite(&fps,         4, 1, f) != 1) return -1;
    if (fwrite(&frame_count, 4, 1, f) != 1) return -1;
    return 0;
}

static void nsleep(long ns) {
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    int cols          = DEFAULT_COLS;
    int rows          = DEFAULT_ROWS;
    int color         = 1;
    const char *ramp  = DEFAULT_RAMP;
    int ramp_len      = (int)strlen(ramp);
    const char *video_path  = NULL;
    const char *output_path = NULL;
    int stream = 0;

    /* parse args */
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--cols")     && i + 1 < argc) cols        = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rows")     && i + 1 < argc) rows        = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ramp")     && i + 1 < argc) { ramp = argv[++i]; ramp_len = (int)strlen(ramp); }
        else if (!strcmp(argv[i], "--output")   && i + 1 < argc) output_path = argv[++i];
        else if (!strcmp(argv[i], "--no-color"))                  color       = 0;
        else if (!strcmp(argv[i], "--stream"))                    stream      = 1;
        else if (argv[i][0] != '-' && !video_path)               video_path  = argv[i];
    }

    if (ramp_len < 2 || ramp_len > 1024) {
        fprintf(stderr, "error: ramp length must be between 2 and 1024\n");
        return 1;
    }

    /* ---- synthetic gradient mode (no video arg) ---- */
    if (!video_path) {
        uint8_t *frame = malloc(FRAME_W * FRAME_H * 4);
        Cell    *cells = malloc(cols * rows * sizeof(Cell));
        if (!frame || !cells) {
            fprintf(stderr, "error: out of memory\n");
            return 1;
        }

        make_gradient(frame, FRAME_W, FRAME_H);

        int err = convert_frame(frame, FRAME_W, FRAME_H, cols, rows,
                                 ramp, ramp_len, cells);
        if (err) {
            fprintf(stderr, "error: convert_frame returned %d\n", err);
            return 1;
        }

        render_cells(cells, cols, rows, color);

        free(frame);
        free(cells);
        return 0;
    }

    /* ---- video input ---- */
    VideoReader *vr = video_open(video_path);
    if (!vr) {
        fprintf(stderr, "error: cannot open video '%s'\n", video_path);
        return 1;
    }

    double fps = video_fps(vr);
    if (fps <= 0.0) fps = 25.0;

    Cell *cells = malloc(cols * rows * sizeof(Cell));
    if (!cells) {
        fprintf(stderr, "error: out of memory\n");
        video_close(vr);
        return 1;
    }

    /* ---- file output mode ---- */
    if (output_path) {
        FILE *out = fopen(output_path, "wb");
        if (!out) {
            fprintf(stderr, "error: cannot open output file '%s'\n", output_path);
            free(cells);
            video_close(vr);
            return 1;
        }

        /* reserve space for header; we'll patch frame_count at the end */
        uint32_t frame_count = 0;
        float    fps_f       = (float)fps;
        write_iag_header(out, (uint32_t)cols, (uint32_t)rows, fps_f, 0);

        uint8_t *rgba = NULL;
        int fw, fh;

        while (video_next_frame(vr, &rgba, &fw, &fh) == 1) {
            int err = convert_frame(rgba, fw, fh, cols, rows,
                                    ramp, ramp_len, cells);
            if (err) continue;

            fwrite(cells, sizeof(Cell), (size_t)(cols * rows), out);
            frame_count++;
        }

        /* patch frame_count in header at offset 16 */
        fseek(out, 16, SEEK_SET);
        fwrite(&frame_count, 4, 1, out);

        fclose(out);
        fprintf(stderr, "wrote %u frames to '%s'\n", frame_count, output_path);

    /* ---- real-time playback mode ---- */
    } else {
        long frame_ns = (long)(1e9 / fps);
        uint8_t *rgba = NULL;
        int fw, fh;
        int frame_index = 0;

        char *fbuf = NULL;
        if (stream) {
            fbuf = malloc(frame_buf_size(cols, rows));
            if (!fbuf) {
                fprintf(stderr, "error: out of memory\n");
                free(cells);
                video_close(vr);
                return 1;
            }
            printf("\033[?25l");    /* hide cursor */
            fflush(stdout);
        }

        while (video_next_frame(vr, &rgba, &fw, &fh) == 1) {
            int err = convert_frame(rgba, fw, fh, cols, rows,
                                    ramp, ramp_len, cells);
            if (err) continue;

            if (stream) {
                size_t len = render_cells_to_buf(fbuf, cells, cols, rows,
                                                 color, frame_index);
                (void)write(STDOUT_FILENO, "\033[?2026h", 8);      /* begin sync */
                if (write(STDOUT_FILENO, fbuf, len) < 0) break;  /* single syscall */
                (void)write(STDOUT_FILENO, "\033[?2026l", 8);      /* end sync   */
            } else {
                printf("\033[2J\033[H");
                render_cells(cells, cols, rows, color);
                fflush(stdout);
            }
            frame_index++;

            nsleep(frame_ns);
        }

        if (stream) {
            printf("\033[?25h");    /* restore cursor */
            fflush(stdout);
            free(fbuf);
        }
    }

    free(cells);
    video_close(vr);
    return 0;
}
