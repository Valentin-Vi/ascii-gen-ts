#pragma once

#include <stdint.h>

typedef struct VideoReader VideoReader;

VideoReader *video_open(const char *path);

/* Returns next RGBA frame (caller must NOT free, valid until next call).
 * Sets *w, *h to frame dimensions.
 * Returns 1 on success, 0 on EOF, -1 on error. */
int video_next_frame(VideoReader *vr, uint8_t **rgba_out, int *w, int *h);

double video_fps(const VideoReader *vr);

void video_close(VideoReader *vr);
