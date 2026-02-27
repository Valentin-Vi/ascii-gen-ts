/*
 * test_tear.c — DEC 2026 Synchronized Output tearing test
 *
 * Renders two visually distinct frames alternating at ~30 fps for 3 seconds:
 *   Frame A: all '@' in solid red
 *   Frame B: all '+' in solid blue
 *
 * Modes:
 *   --no-sync  raw write() per frame  (tearing visible on slow terminals)
 *   --sync     wrap each write in \033[?2026h ... \033[?2026l (clean swap)
 *   --measure  print buffer size vs PTY limit estimate, then exit
 *
 * Build:
 *   gcc -O2 -std=c11 -D_POSIX_C_SOURCE=200809L tests/test_tear.c -o test_tear
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define COLS        80
#define ROWS        40
#define TARGET_FPS  30
#define DURATION_S  3

/* Worst-case bytes per cell: "\033[38;2;255;255;255mX" = 20 bytes
 * Per row: COLS*20 + "\033[0m\r\n" = COLS*20 + 6
 * Cursor-up prefix: "\033[{ROWS}A\r" <= 8 bytes
 * Total:
 */
#define CELL_BYTES   20
#define ROW_SUFFIX    6   /* \033[0m\r\n */
#define CURSOR_UP     8

static size_t frame_buf_size(void) {
    return (size_t)ROWS * ((size_t)COLS * CELL_BYTES + ROW_SUFFIX) + CURSOR_UP;
}

/*
 * Build one full frame into buf.
 * ch  — character to fill ('A' or '+')
 * r,g,b — foreground color
 * frame_index — 0 for first frame (no cursor-up), >0 moves cursor back up
 *
 * Returns number of bytes written.
 */
static size_t build_frame(char *buf, char ch,
                           int r, int g, int b,
                           int frame_index)
{
    char *p = buf;

    if (frame_index > 0) {
        p += sprintf(p, "\033[%dA\r", ROWS);
    }

    for (int row = 0; row < ROWS; row++) {
        p += sprintf(p, "\033[38;2;%d;%d;%dm", r, g, b);
        for (int col = 0; col < COLS; col++) {
            *p++ = ch;
        }
        /* reset + newline */
        *p++ = '\033'; *p++ = '['; *p++ = '0'; *p++ = 'm';
        *p++ = '\r';   *p++ = '\n';
    }

    return (size_t)(p - buf);
}

static long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

static void nsleep(long ns) {
    if (ns <= 0) return;
    struct timespec ts = { .tv_sec  = ns / 1000000000L,
                           .tv_nsec = ns % 1000000000L };
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    int mode_sync    = 0;
    int mode_nosync  = 0;
    int mode_measure = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--sync"))    mode_sync    = 1;
        else if (!strcmp(argv[i], "--no-sync")) mode_nosync  = 1;
        else if (!strcmp(argv[i], "--measure")) mode_measure = 1;
    }

    if (!mode_sync && !mode_nosync && !mode_measure) {
        fprintf(stderr,
            "usage: %s --no-sync | --sync | --measure\n"
            "\n"
            "  --no-sync   raw write() per frame (tearing visible)\n"
            "  --sync      DEC 2026 synchronized output (clean swap)\n"
            "  --measure   print buffer size vs PTY limit, then exit\n",
            argv[0]);
        return 1;
    }

    /* ---- measure mode ---- */
    if (mode_measure) {
        size_t fbuf  = frame_buf_size();
        size_t pty   = 4096;   /* typical Linux PTY slave buffer */
        printf("Frame buffer size : %zu bytes (%.1f KB)\n",
               fbuf, fbuf / 1024.0);
        printf("PTY slave buffer  : ~%zu bytes (%zu KB)\n",
               pty, pty / 1024);
        printf("Overflow ratio    : %.1fx  (%s)\n",
               (double)fbuf / (double)pty,
               fbuf > pty ? "TEARING EXPECTED without sync" : "fits in one PTY chunk");
        printf("Grid              : %d cols x %d rows\n", COLS, ROWS);
        return 0;
    }

    /* ---- allocate frame buffer ---- */
    size_t bufsz = frame_buf_size();
    char *fbuf = malloc(bufsz);
    if (!fbuf) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    /* hide cursor, clear screen */
    (void)write(STDOUT_FILENO, "\033[?25l\033[2J\033[H", 13);

    long frame_ns  = (long)(1e9 / TARGET_FPS);
    long end_time  = now_ns() + (long)DURATION_S * 1000000000L;
    int  frame_idx = 0;
    long t_start   = now_ns();

    while (now_ns() < end_time) {
        long t_frame_start = now_ns();

        /* alternate between frame A (red '@') and frame B (blue '+') */
        size_t len;
        if (frame_idx % 2 == 0) {
            len = build_frame(fbuf, '@', 220, 50, 50, frame_idx);   /* red  */
        } else {
            len = build_frame(fbuf, '+', 50, 100, 220, frame_idx);  /* blue */
        }

        if (mode_sync) {
            (void)write(STDOUT_FILENO, "\033[?2026h", 8);  /* begin sync */
        }

        if (write(STDOUT_FILENO, fbuf, len) < 0) break;

        if (mode_sync) {
            (void)write(STDOUT_FILENO, "\033[?2026l", 8);  /* end sync */
        }

        frame_idx++;

        long elapsed = now_ns() - t_frame_start;
        nsleep(frame_ns - elapsed);
    }

    long total_ns = now_ns() - t_start;

    /* restore cursor, move below rendered area */
    (void)write(STDOUT_FILENO, "\033[?25h\r\n", 8);

    /* stats */
    double elapsed_s = total_ns / 1e9;
    double achieved  = frame_idx / elapsed_s;
    fprintf(stderr,
        "\nframes rendered : %d\n"
        "elapsed time    : %.2f s\n"
        "achieved fps    : %.1f\n"
        "mode            : %s\n",
        frame_idx, elapsed_s, achieved,
        mode_sync ? "--sync (DEC 2026)" : "--no-sync (raw write)");

    free(fbuf);
    return 0;
}
