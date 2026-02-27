#include "video.h"

#include <stdlib.h>
#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

struct VideoReader {
    AVFormatContext *fmt_ctx;
    AVCodecContext  *codec_ctx;
    struct SwsContext *sws_ctx;
    AVFrame  *frame;
    AVPacket *packet;
    int       stream_idx;
    double    fps;

    /* internal RGBA buffer, allocated once on first frame */
    uint8_t  *rgba_buf;
    int       rgba_w;
    int       rgba_h;
};

VideoReader *video_open(const char *path) {
    VideoReader *vr = calloc(1, sizeof(*vr));
    if (!vr) return NULL;

    if (avformat_open_input(&vr->fmt_ctx, path, NULL, NULL) < 0) {
        fprintf(stderr, "video_open: cannot open '%s'\n", path);
        goto fail;
    }

    if (avformat_find_stream_info(vr->fmt_ctx, NULL) < 0) {
        fprintf(stderr, "video_open: cannot find stream info\n");
        goto fail;
    }

    const AVCodec *codec = NULL;
    vr->stream_idx = av_find_best_stream(vr->fmt_ctx, AVMEDIA_TYPE_VIDEO,
                                          -1, -1, &codec, 0);
    if (vr->stream_idx < 0 || !codec) {
        fprintf(stderr, "video_open: no video stream found\n");
        goto fail;
    }

    AVStream *stream = vr->fmt_ctx->streams[vr->stream_idx];
    AVRational tb = stream->avg_frame_rate;
    vr->fps = (tb.den > 0) ? (double)tb.num / tb.den : 25.0;

    vr->codec_ctx = avcodec_alloc_context3(codec);
    if (!vr->codec_ctx) goto fail;

    if (avcodec_parameters_to_context(vr->codec_ctx,
                                       stream->codecpar) < 0) goto fail;

    if (avcodec_open2(vr->codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "video_open: cannot open codec\n");
        goto fail;
    }

    vr->frame  = av_frame_alloc();
    vr->packet = av_packet_alloc();
    if (!vr->frame || !vr->packet) goto fail;

    return vr;

fail:
    video_close(vr);
    return NULL;
}

int video_next_frame(VideoReader *vr, uint8_t **rgba_out, int *w, int *h) {
    if (!vr) return -1;

    for (;;) {
        /* try to receive a decoded frame first */
        int ret = avcodec_receive_frame(vr->codec_ctx, vr->frame);
        if (ret == 0) {
            /* got a frame — set up sws and RGBA buffer if needed */
            int fw = vr->frame->width;
            int fh = vr->frame->height;

            if (!vr->rgba_buf || vr->rgba_w != fw || vr->rgba_h != fh) {
                free(vr->rgba_buf);
                vr->rgba_buf = malloc((size_t)fw * fh * 4);
                if (!vr->rgba_buf) return -1;
                vr->rgba_w = fw;
                vr->rgba_h = fh;

                if (vr->sws_ctx) sws_freeContext(vr->sws_ctx);
                vr->sws_ctx = sws_getContext(
                    fw, fh, vr->frame->format,
                    fw, fh, AV_PIX_FMT_RGBA,
                    SWS_BILINEAR, NULL, NULL, NULL);
                if (!vr->sws_ctx) return -1;
            }

            uint8_t *dst[1]     = { vr->rgba_buf };
            int      dst_stride[1] = { fw * 4 };
            sws_scale(vr->sws_ctx,
                      (const uint8_t * const *)vr->frame->data,
                      vr->frame->linesize,
                      0, fh,
                      dst, dst_stride);

            *rgba_out = vr->rgba_buf;
            *w = fw;
            *h = fh;
            av_frame_unref(vr->frame);
            return 1;
        }

        if (ret != AVERROR(EAGAIN)) {
            /* AVERROR_EOF or real error — flush done */
            return (ret == AVERROR_EOF) ? 0 : -1;
        }

        /* need more packets */
        for (;;) {
            int r = av_read_frame(vr->fmt_ctx, vr->packet);
            if (r == AVERROR_EOF) {
                /* flush decoder */
                avcodec_send_packet(vr->codec_ctx, NULL);
                break;
            }
            if (r < 0) return -1;

            if (vr->packet->stream_index == vr->stream_idx) {
                avcodec_send_packet(vr->codec_ctx, vr->packet);
                av_packet_unref(vr->packet);
                break;
            }
            av_packet_unref(vr->packet);
        }
    }
}

double video_fps(const VideoReader *vr) {
    return vr ? vr->fps : 0.0;
}

void video_close(VideoReader *vr) {
    if (!vr) return;
    free(vr->rgba_buf);
    if (vr->sws_ctx)   sws_freeContext(vr->sws_ctx);
    if (vr->frame)     av_frame_free(&vr->frame);
    if (vr->packet)    av_packet_free(&vr->packet);
    if (vr->codec_ctx) avcodec_free_context(&vr->codec_ctx);
    if (vr->fmt_ctx)   avformat_close_input(&vr->fmt_ctx);
    free(vr);
}
