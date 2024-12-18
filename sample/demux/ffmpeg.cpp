/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "ffmpeg.hpp"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include "axcl_rt.h"
#include "elapser.hpp"
#include "nalu_lock_fifo.hpp"
#include "threadx.hpp"
#include "utils/logger.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}

#define AVERRMSG(err, msg)                  \
    ({                                      \
        av_strerror(err, msg, sizeof(msg)); \
        msg;                                \
    })

#define FFMPEG_CONTEXT(handle)                                                           \
    ({                                                                                   \
        struct ffmpeg_context *ctx = reinterpret_cast<struct ffmpeg_context *>(demuxer); \
        if (!ctx) {                                                                      \
            SAMPLE_LOG_E("invalid ffmpeg demux context");                                \
            return -EINVAL;                                                              \
        }                                                                                \
        ctx;                                                                             \
    })

static constexpr const char *ffmpeg_demuxer_attr_frame_rate_control = "ffmpeg.demux.file.frc";
static constexpr const char *ffmpeg_demuxer_attr_file_loop = "ffmpeg.demux.file.loop";

typedef struct PacketGroup {
    AVPacket packet;           // 当前的数据包
    struct PacketGroup *next;  // 下一个数据包组合
} PacketGroup;                 // 定义数据包组合结构

typedef struct PacketQueue {
    PacketGroup *first_pkt;  // 第一个数据包组合
    PacketGroup *last_pkt;   // 最后一个数据包组合
} PacketQueue;               // 定义数据包队列结构

// 数据包入列
void push_packet(PacketQueue *packet_list, AVPacket packet) {
    PacketGroup *this_pkt = (PacketGroup *)av_malloc(sizeof(PacketGroup));
    this_pkt->packet = packet;
    this_pkt->next = NULL;
    if (packet_list->first_pkt == NULL) {
        packet_list->first_pkt = this_pkt;
    }
    if (packet_list->last_pkt == NULL) {
        PacketGroup *last_pkt = (PacketGroup *)av_malloc(sizeof(PacketGroup));
        packet_list->last_pkt = last_pkt;
    }
    packet_list->last_pkt->next = this_pkt;
    packet_list->last_pkt = this_pkt;
    return;
}

// 数据包出列
AVPacket pop_packet(PacketQueue *packet_list) {
    PacketGroup *first_pkt = packet_list->first_pkt;
    packet_list->first_pkt = packet_list->first_pkt->next;
    return first_pkt->packet;
}

// 判断队列是否为空
int is_empty(PacketQueue packet_list) {
    return packet_list.first_pkt == NULL ? 1 : 0;
}

struct ffmpeg_context {
    std::string url;
    AVFormatContext *avfmt_in_ctx = nullptr;

    std::string mp4_file;
    AVFormatContext *avfmt_mp4_ctx = nullptr;

    std::string rtmp_url;
    AVFormatContext *avfmt_rtmp_ctx = nullptr;

    axcl::threadx demux_thread;
    axcl::threadx dispatch_thread;
    axcl::threadx sync_thread;
    axcl::threadx rtmp_thread;

    int32_t video_track_id = -1;
    int32_t audio_track_id = -1;

    AVCodecID encodec = AV_CODEC_ID_NONE;
    int32_t cookie = -1;
    int32_t device = -1;
    struct stream_info info;
    AVBSFContext *avbsf_ctx = nullptr;
    AVStream *src_video = NULL;
    AVStream *src_audio = NULL;
    AVStream *dest_video = NULL;
    AVStream *dest_audio = NULL;
    axcl::event eof;

    /* sink and sink userdata */
    stream_sink sink;
    uint64_t userdata = 0;

    /* dispatch fifo */
    nalu_lock_fifo *fifo = nullptr;
    nalu_lock_fifo *fifo_v = nullptr;

    PacketQueue packet_audio_list;
    PacketQueue packet_video_list;

    /* attribute */
    bool frame_rate_control = false;
    bool loop = false;

/**
 * just for debug:
 * check dispatch put and pop nalu data
 */
//  #define __SAVE_NALU_DATA__
#if defined(__SAVE_NALU_DATA__)
    FILE *fput = nullptr;
    FILE *fpop = nullptr;
    std::vector<std::vector<uint8_t>> put_data;
#endif
};

static int ffmpeg_init_demuxer(ffmpeg_context *context);
static int ffmpeg_deinit_demuxer(ffmpeg_context *context);

static void ffmpeg_demux_thread(ffmpeg_context *context);

static void ffmpeg_dispatch_thread(ffmpeg_context *context);
static void ffmpeg_sync_thread(ffmpeg_context *context);
static void ffmpeg_rtmp_thread(ffmpeg_context *context);

int ffmpeg_create_demuxer(ffmpeg_demuxer *demuxer, const char *url, const char *rtmp_url, int32_t encodec, int32_t device, stream_sink sink, uint64_t userdata) {
    if (!url || device <= 0 || !demuxer) {
        SAMPLE_LOG_E("invalid parameters");
        return -EINVAL;
    }

    ffmpeg_context *context = new (std::nothrow) ffmpeg_context();
    if (!context) {
        SAMPLE_LOG_E("create ffmpeg demux context fail");
        return -EFAULT;
    }

    context->url = url;
    context->mp4_file = "output.mp4";
    // context->rtmp_url = "rtmp://192.168.0.134:1935/live/test";
    context->rtmp_url = rtmp_url;

    switch (encodec) {
        case PT_H264:
            context->encodec = AV_CODEC_ID_H264;
            break;
        case PT_H265:
            context->encodec = AV_CODEC_ID_HEVC;
            break;
        default:
            context->encodec = AV_CODEC_ID_H264;
            SAMPLE_LOG_E("[%d] unsupport video encodec!", encodec);
            break;
    }

    context->cookie = device;
    context->device = device;
    context->sink = sink;
    context->userdata = userdata;

    if (int ret = ffmpeg_init_demuxer(context); 0 != ret) {
        delete context;
        return ret;
    }

    *demuxer = reinterpret_cast<ffmpeg_demuxer>(context);
    return 0;
}

int ffmpeg_destory_demuxer(ffmpeg_demuxer demuxer) {
    ffmpeg_context *context = FFMPEG_CONTEXT(demuxer);
    if (int ret = ffmpeg_deinit_demuxer(context); 0 != ret) {
        return ret;
    }

    if (context->fifo) {
        delete context->fifo;
    }

    if (context->fifo_v) {
        delete context->fifo_v;
    }

    delete context;
    return 0;
}

const struct stream_info *ffmpeg_get_stream_info(ffmpeg_demuxer demuxer) {
    ffmpeg_context *context = reinterpret_cast<ffmpeg_context *>(demuxer);
    if (!context) {
        SAMPLE_LOG_E("invalid context handle");
        return nullptr;
    }

    return &context->info;
}

int ffmpeg_push_video_packet(ffmpeg_demuxer demuxer, AVPacket packet) {
    ffmpeg_context *context = reinterpret_cast<ffmpeg_context *>(demuxer);
    if (!context) {
        SAMPLE_LOG_E("invalid context handle");
        return -1;
    }

    packet.stream_index = context->video_track_id;
    push_packet(&context->packet_video_list, packet);

    return 0;
}

int ffmpeg_push_video_nalu(ffmpeg_demuxer demuxer, nalu_data *nalu) {
    int ret = 0;
    ffmpeg_context *context = reinterpret_cast<ffmpeg_context *>(demuxer);
    if (!context) {
        SAMPLE_LOG_E("invalid context handle");
        return -1;
    }

    if (ret = context->fifo_v->push(*nalu, -1); 0 != ret) {
        SAMPLE_LOG_E("[%d] push frame len %d to fifo fail, ret = %d", context->cookie, nalu->len, ret);
    }

    return 0;
}

int ffmpeg_start_demuxer(ffmpeg_demuxer demuxer) {
    ffmpeg_context *context = FFMPEG_CONTEXT(demuxer);
    char name[16];
    sprintf(name, "dispatch%d", context->cookie);
    context->dispatch_thread.start(name, ffmpeg_dispatch_thread, context);

    sprintf(name, "demux%d", context->cookie);
    context->demux_thread.start(name, ffmpeg_demux_thread, context);

    sprintf(name, "rtmp%d", context->cookie);
    context->rtmp_thread.start(name, ffmpeg_rtmp_thread, context);
    return 0;
}

static inline void ffmpeg_stop_dispatch(ffmpeg_context *context) {
    context->dispatch_thread.stop();
}

int ffmpeg_stop_demuxer(ffmpeg_demuxer demuxer) {
    ffmpeg_context *context = FFMPEG_CONTEXT(demuxer);
    context->demux_thread.stop();
    context->demux_thread.join();

    context->dispatch_thread.stop();
    context->dispatch_thread.join();

    context->rtmp_thread.stop();
    context->rtmp_thread.join();

    return 0;
}

static void ffmpeg_demux_eof(ffmpeg_context *context) {
    context->eof.set();
}

int ffmpeg_wait_demuxer_eof(ffmpeg_demuxer demuxer, int32_t timeout) {
    ffmpeg_context *context = FFMPEG_CONTEXT(demuxer);
    return context->eof.wait(timeout) ? 0 : -1;
}

int ffmpeg_set_demuxer_sink(ffmpeg_demuxer demuxer, stream_sink sink, uint64_t userdata) {
    ffmpeg_context *context = FFMPEG_CONTEXT(demuxer);
    context->sink = sink;
    context->userdata = userdata;
    return 0;
}

static void ffmpeg_sync_thread(ffmpeg_context *context) {
    SAMPLE_LOG_I("[%d] +++", context->cookie);

    if (avio_open(&context->avfmt_mp4_ctx->pb, context->mp4_file.c_str(), AVIO_FLAG_WRITE) < 0) {
        SAMPLE_LOG_E("Could not open '%s'\n", context->mp4_file.c_str());
        return;
    }

    if (avformat_write_header(context->avfmt_mp4_ctx, NULL) < 0) {
        SAMPLE_LOG_E("Could not write header\n");
        return;
    }

    int ret;
    double audio_time = 0;
    double video_time = 0;
    AVPacket video_packet;
    AVPacket audio_packet;
    int v_flag = 0;
    while (context->sync_thread.running()) {
        if (is_empty(context->packet_video_list)) {
            continue;
        }

        if (v_flag == 0) {
            video_packet = pop_packet(&context->packet_video_list);  // 取出头部的视频包
            if (video_packet.pts != AV_NOPTS_VALUE) {                // 保存视频时钟
                video_time = av_q2d(context->src_video->time_base) * video_packet.pts;
                SAMPLE_LOG_I("Video Seconds PTS = %f PTS = %ld", video_time, video_packet.pts);
            }
            v_flag = 1;
            video_packet.stream_index = 0;

            // 变基
            av_packet_rescale_ts(&video_packet, context->src_video->time_base, context->dest_video->time_base);
            ret = av_write_frame(context->avfmt_mp4_ctx, &video_packet);
            if (ret < 0) {
                SAMPLE_LOG_E("write frame occur error %d.\n", ret);
                break;
            }
            av_packet_unref(&video_packet);
        }

        if (video_time >= audio_time) {
            if (is_empty(context->packet_audio_list)) {
                continue;
            }

            audio_packet = pop_packet(&context->packet_audio_list);  // 取出头部的音频包
            if (audio_packet.pts != AV_NOPTS_VALUE) {                // 保存音频时钟
                audio_time = av_q2d(context->src_audio->time_base) * audio_packet.pts;
                SAMPLE_LOG_I("Audio Seconds PTS = %f PTS = %ld", audio_time, audio_packet.pts);
            }
            audio_packet.stream_index = 1;

            // 变基
            av_packet_rescale_ts(&audio_packet, context->src_audio->time_base, context->dest_audio->time_base);
            ret = av_write_frame(context->avfmt_mp4_ctx, &audio_packet);
            if (ret < 0) {
                SAMPLE_LOG_E("write frame occur error %d.\n", ret);
                break;
            }
            av_packet_unref(&audio_packet);
        } else {
            v_flag = 0;
        }
    }

    av_write_trailer(context->avfmt_mp4_ctx);
}

static void ffmpeg_dispatch_thread(ffmpeg_context *context) {
    SAMPLE_LOG_I("[%d] +++", context->cookie);

    /**
     * As sink callback will invoke axcl API, axcl runtime context should be created for dispatch thread.
     * Remind to destory before thread exit.
     */
    axclrtContext runtime;
    if (axclError ret = axclrtCreateContext(&runtime, context->device); AXCL_SUCC != ret) {
        return;
    }

#ifdef __SAVE_NALU_DATA__
    context->fpop = fopen("./fpop.raw", "wb");
#endif

    int32_t ret;
    uint64_t count = 0;
    while (context->dispatch_thread.running() || context->fifo->size() > 0) {
        nalu_data nalu;
        uint32_t total_len = 0;

        // SAMPLE_LOG_I("peek size %d frame %ld +++", context->fifo->size(), count);
        if (ret = context->fifo->peek(nalu, total_len, -1); 0 != ret) {
            if (-EINTR != ret) {
                SAMPLE_LOG_E("[%d] peek from fifo fail, ret = %d", context->cookie, ret);
            }
            break;
        }
        // SAMPLE_LOG_I("peek size %d frame %ld ---", context->fifo->size(), count);

        struct stream_data stream;
        stream.payload = context->info.video.payload;
        stream.cookie = context->cookie;
        stream.video.pts = nalu.pts;
        if (nalu.len2 > 0) {
            stream.video.size = nalu.len + nalu.len2;
            stream.video.data = reinterpret_cast<uint8_t *>(malloc(stream.video.size));
            memcpy(stream.video.data, nalu.nalu, nalu.len);
            memcpy(stream.video.data + nalu.len, nalu.nalu2, nalu.len2);
        } else {
            stream.video.size = nalu.len;
            stream.video.data = nalu.nalu;
        }

        if (stream.video.size > 0) {
            ++count;

#if defined(__SAVE_NALU_DATA__)
            fwrite((const void *)stream.video.data, 1, stream.video.size, context->fpop);
            if (stream.video.size != context->put_data[count - 1].size()) {
                SAMPLE_LOG_E("frame %ld, size not equal %d != %ld", count, stream.video.size, context->put_data[count - 1].size());
            } else {
                if (0 != memcmp(context->put_data[count - 1].data(), stream.video.data, stream.video.size)) {
                    SAMPLE_LOG_E("frame %ld, size %d, data not equal", count, stream.video.size);
                }
            }
#endif
        }

        context->sink.on_stream_data(&stream, context->userdata);

        /* pop from fifo */
        context->fifo->skip(total_len);

        if (nalu.len2 > 0) {
            free(stream.video.data);
        }
    }

#ifdef __SAVE_NALU_DATA__
    fflush(context->fpop);
    fclose(context->fpop);
#endif

    /* notify eof */
    ffmpeg_demux_eof(context);

    /* destory axcl runtime context */
    axclrtDestroyContext(runtime);
    SAMPLE_LOG_I("[%d] dispatched total %ld frames ---", context->cookie, count);
}

static int32_t ffmpeg_seek_to_begin(ffmpeg_context *context) {
    /**
     * AVSEEK_FLAG_BACKWARD may fail (example: zhuheqiao.mp4), use AVSEEK_FLAG_ANY, but not guarantee seek to I frame
     */
    av_bsf_flush(context->avbsf_ctx);
    int32_t ret = av_seek_frame(context->avfmt_in_ctx, context->video_track_id, 0, AVSEEK_FLAG_ANY /* AVSEEK_FLAG_BACKWARD */);
    if (ret < 0) {
        char msg[64];
        SAMPLE_LOG_W("[%d] seek to begin fail, %s, retry once ...", context->cookie, AVERRMSG(ret, msg));
        ret = avformat_seek_file(context->avfmt_in_ctx, context->video_track_id, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_BYTE);
        if (ret < 0) {
            SAMPLE_LOG_E("[%d] seek to begin fail, %s", context->cookie, AVERRMSG(ret, msg));
            return ret;
        }
    }

    return ret;
}

static void ffmpeg_demux_thread(ffmpeg_context *context) {
    SAMPLE_LOG_I("[%d] +++", context->cookie);

    int ret;
    char msg[64] = {0};
    uint64_t count = 0;
    uint64_t pts = 0;
    uint64_t now = 0;
    uint64_t last = 0;
    const uint64_t interval = 1000000 / context->info.video.fps;

    AVPacket *avpkt = av_packet_alloc();
    if (!avpkt) {
        SAMPLE_LOG_E("[%d] av_packet_alloc() fail!", context->cookie);
        return;
    }

#ifdef __SAVE_NALU_DATA__
    context->fput = fopen("./fput.raw", "wb");
#endif

    context->eof.reset();
    while (context->demux_thread.running()) {
        ret = av_read_frame(context->avfmt_in_ctx, avpkt);
        if (ret < 0) {
            if (AVERROR_EOF == ret) {
                if (context->loop) {
                    if (ret = ffmpeg_seek_to_begin(context); 0 != ret) {
                        break;
                    }

                    continue;
                }

                SAMPLE_LOG_I("[%d] reach eof", context->cookie);
                if (context->sink.on_stream_data) {
                    /* send eof */
                    nalu_data nalu = {};
                    nalu.userdata = context->userdata;
                    if (ret = context->fifo->push(nalu, -1); 0 != ret) {
                        if (-EINTR != ret) {
                            SAMPLE_LOG_E("[%d] push eof to fifo fail, ret = %d", context->cookie, ret);
                        }
                    }
                }

                ffmpeg_stop_dispatch(context);
                break;
            } else {
                SAMPLE_LOG_E("[%d] av_read_frame() fail, %s", context->cookie, AVERRMSG(ret, msg));
                break;
            }

        } else {
            if (avpkt->stream_index == context->video_track_id) {
                ret = av_bsf_send_packet(context->avbsf_ctx, avpkt);
                if (ret < 0) {
                    av_packet_unref(avpkt);
                    SAMPLE_LOG_E("[%d] av_bsf_send_packet() fail, %s", context->cookie, AVERRMSG(ret, msg));
                    break;
                }

                while (ret >= 0) {
                    ret = av_bsf_receive_packet(context->avbsf_ctx, avpkt);
                    if (ret < 0) {
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        }

                        av_packet_unref(avpkt);
                        SAMPLE_LOG_E("[%d] av_bsf_receive_packet() fail, %s", context->cookie, AVERRMSG(ret, msg));

                        ffmpeg_stop_dispatch(context);
                        return;
                    }

                    ++count;

                    pts += interval;

                    nalu_data nalu = {};
                    nalu.userdata = context->userdata;
                    nalu.pts = avpkt->pts;
                    nalu.nalu = avpkt->data;
                    nalu.len = avpkt->size;

#ifdef __SAVE_NALU_DATA__
                    fwrite(nalu.nalu, 1, nalu.len, context->fput);
                    context->put_data.emplace_back(nalu.nalu, nalu.nalu + nalu.len);
#endif

                    if (context->frame_rate_control) {
                        /**
                         * Simulate fps by sleeping.
                         * 1. If the host is AX650: Since the CPU frequency is 100Hz, we use the high-precision delay (AX_SYS_Usleep)
                         * provided by the hardware timer64.
                         * 2. If the host is x64: Ensure that a high-precision delay is implemented.
                         */
                        now = axcl::elapser::ticks();
                        if (count <= 1) {
                            last = now;
                        } else {
                            auto diff = now - last;
                            if (diff < interval) {
                                auto delay = interval - diff;
                                delay -= (delay % 1000); /* truncated to ms */
                                axcl::elapser::ax_usleep(delay);
                                // SAMPLE_LOG_I("[%d] frame %ld: now %ld, last %ld, delay %ld", context->cookie, count, now, last, delay);
                            } else {
                                SAMPLE_LOG_W("[%d] frame %ld: now %ld, last %ld, delay %ld ==> execced fps interval %ld", context->cookie,
                                             count, now, last, diff - interval, interval);
                            }

                            last = axcl::elapser::ticks();
                        }
                    }

                    if (ret = context->fifo->push(nalu, -1); 0 != ret) {
                        SAMPLE_LOG_E("[%d] push frame %ld len %d to fifo fail, ret = %d", context->cookie, count, nalu.len, ret);
                    }
                }
                av_packet_unref(avpkt);
            }

            if (avpkt->stream_index == context->audio_track_id) {
                push_packet(&context->packet_audio_list, *avpkt);  // 把音频包加入队列
            }
        }
    }

#ifdef __SAVE_NALU_DATA__
    fflush(context->fput);
    fclose(context->fput);
#endif

    av_packet_free(&avpkt);
    SAMPLE_LOG_I("[%d] demuxed    total %ld frames ---", context->cookie, count);
}

static void ffmpeg_rtmp_thread(ffmpeg_context *context) {
    SAMPLE_LOG_I("[%d] +++", context->cookie);

    int ret;
    char errbuf[256];
    int64_t start_time = 0;
    int frame_idx = 0;
    int audio_idx = 0;

    if (!(context->avfmt_rtmp_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&context->avfmt_rtmp_ctx->pb, context->rtmp_url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output URL '%s'", context->rtmp_url.c_str());
            return;
        }
    }

    // Write file header
    ret = avformat_write_header(context->avfmt_rtmp_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occured when opening output URL\n");
        return;
    }

    AVPacket *avpkt = av_packet_alloc();
    if (!avpkt) {
        SAMPLE_LOG_E("[%d] av_packet_alloc() fail!", context->cookie);
        return;
    }

    start_time = av_gettime();
    while (context->demux_thread.running()) {
        AVStream *in_stream;
        AVStream *out_stream;

        nalu_data nalu_v;
        uint32_t total_len = 0;

        if (context->fifo_v->size() > 0) {
            if (ret = context->fifo_v->peek(nalu_v, total_len, -1); 0 != ret) {
                SAMPLE_LOG_E("[%d] peek from fifo fail, ret = %d", context->cookie, ret);
                continue;
            }
            avpkt->data = nalu_v.nalu;
            avpkt->size = nalu_v.len;
            // SAMPLE_LOG_I("peek size %d  %ld---", avpkt->size, nalu_v.pts);

            context->fifo_v->skip(total_len);

        } else {
            av_usleep(1000); // 等待一段时间再检查 FIFO
            continue;
        }

        // write pts
        if (avpkt->pts == AV_NOPTS_VALUE) {
            // write pts
            AVRational time_base1 = context->avfmt_in_ctx->streams[context->video_track_id]->time_base;
            // Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(context->avfmt_in_ctx->streams[context->video_track_id]->r_frame_rate);
            // parameters
            // pts是播放时间戳,告诉播放器什么时候播放这一帧视频,PTS通常是按照递增顺序排列的,以保证正确的时间顺序和播放同步
            // dts是解码时间戳,告诉播放器什么时候解码这一帧视频
            avpkt->pts = (double)(frame_idx * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
            avpkt->dts = avpkt->pts;
            avpkt->duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
        }

        // important: delay
        if (avpkt->stream_index == context->video_track_id) {
            AVRational time_base = context->avfmt_in_ctx->streams[context->video_track_id]->time_base;
            AVRational time_base_q = {1, AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(avpkt->dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time) {
                av_usleep(pts_time - now_time);
            }
            // av_usleep(50);
        }

        in_stream = context->avfmt_in_ctx->streams[avpkt->stream_index];
        out_stream = context->avfmt_rtmp_ctx->streams[avpkt->stream_index];

        // copy packet
        // convert PTS/DTS
        avpkt->pts = av_rescale_q_rnd(avpkt->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        avpkt->dts = av_rescale_q_rnd(avpkt->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        avpkt->duration = av_rescale_q(avpkt->duration, in_stream->time_base, out_stream->time_base);
        avpkt->pos = -1;

        // print to screen
        if (avpkt->stream_index == context->video_track_id) {
            // fprintf(stdout, "Send %8d video frames to output URL\n", frame_idx);
            frame_idx++;
        } else if (avpkt->stream_index == context->audio_track_id) {
            fprintf(stdout, "Send %8d audio frames to output URL\n", audio_idx);
            audio_idx++;
        }

        avpkt->stream_index = 0;
        ret = av_interleaved_write_frame(context->avfmt_rtmp_ctx, avpkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet, error code:%d\n", ret);
            break;
        }
        av_packet_unref(avpkt);
    }

    // write file trailer
    av_write_trailer(context->avfmt_rtmp_ctx);

    if (!(context->avfmt_rtmp_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&context->avfmt_rtmp_ctx->pb);
    }

    av_packet_free(&avpkt);

    SAMPLE_LOG_I("[%d] ---", context->cookie);
}

static int ffmpeg_init_demuxer(ffmpeg_context *context) {
    SAMPLE_LOG_I("[%d] url: %s", context->cookie, context->url.c_str());

    avformat_network_init();

    char msg[64] = {0};
    int ret;
    context->avfmt_in_ctx = avformat_alloc_context();
    if (!context->avfmt_in_ctx) {
        SAMPLE_LOG_E("[%d] avformat_alloc_context() fail.", context->cookie);
        return -EFAULT;
    }

    do {
        ret = avformat_open_input(&context->avfmt_in_ctx, context->url.c_str(), NULL, NULL);
        if (ret < 0) {
            SAMPLE_LOG_E("[%d] open %s fail, %s", context->cookie, context->url.c_str(), AVERRMSG(ret, msg));
            break;
        }

        ret = avformat_find_stream_info(context->avfmt_in_ctx, NULL);
        if (ret < 0) {
            SAMPLE_LOG_E("[%d] avformat_find_stream_info() fail, %s", context->cookie, AVERRMSG(ret, msg));
            break;
        }

        av_dump_format(context->avfmt_in_ctx, 0, context->url.c_str(), 0);

        avformat_alloc_output_context2(&context->avfmt_mp4_ctx, NULL, NULL, context->mp4_file.c_str());
        if (!context->avfmt_mp4_ctx) {
            SAMPLE_LOG_E("avformat_alloc_output_context2 failed\n");
            break;
        }

        ret = avformat_alloc_output_context2(&context->avfmt_rtmp_ctx, NULL, "flv", context->rtmp_url.c_str());  // RTMP
        if (ret < 0) {
            fprintf(stderr, "Could not create output context, error code:%d\n", ret);
            break;
        }

        context->video_track_id = av_find_best_stream(context->avfmt_in_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        SAMPLE_LOG_I("%s video_index = %d", context->url.c_str(), context->video_track_id);

        context->audio_track_id = av_find_best_stream(context->avfmt_in_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        SAMPLE_LOG_I("%s audio_index = %d", context->url.c_str(), context->audio_track_id);

        for (unsigned int i = 0; i < context->avfmt_in_ctx->nb_streams; i++) {
            SAMPLE_LOG_I("type of the encoded data: %d\n", context->avfmt_in_ctx->streams[i]->codecpar->codec_id);
            if (AVMEDIA_TYPE_VIDEO == context->avfmt_in_ctx->streams[i]->codecpar->codec_type) {
                context->video_track_id = i;
                context->src_video = context->avfmt_in_ctx->streams[i];  // 保存视频的时间基
                SAMPLE_LOG_I("the video frame pixels: width: %d, height: %d, pixel format: %d\n",
                             context->avfmt_in_ctx->streams[i]->codecpar->width, context->avfmt_in_ctx->streams[i]->codecpar->height,
                             context->avfmt_in_ctx->streams[i]->codecpar->format);

                context->dest_video = avformat_new_stream(context->avfmt_mp4_ctx, NULL);
                if (!context->dest_video) {
                    SAMPLE_LOG_E("avformat_new_stream\n");
                    break;
                }
                if (avcodec_parameters_copy(context->dest_video->codecpar, context->avfmt_in_ctx->streams[i]->codecpar) < 0) {
                    SAMPLE_LOG_E("avcodec_parameters_copy\n");
                    break;
                }
                context->dest_video->codecpar->codec_id = context->encodec;
                context->dest_video->codecpar->codec_tag = 0;
                break;
            } else if (AVMEDIA_TYPE_AUDIO == context->avfmt_in_ctx->streams[i]->codecpar->codec_type) {
                context->audio_track_id = i;
                context->src_audio = context->avfmt_in_ctx->streams[i];  // 保存音频的时间基
                SAMPLE_LOG_I("audio sample format: %d\n", context->avfmt_in_ctx->streams[i]->codecpar->format);

                context->dest_audio = avformat_new_stream(context->avfmt_mp4_ctx, NULL);
                if (!context->dest_audio) {
                    SAMPLE_LOG_E("avformat_new_stream\n");
                    break;
                }
                if (avcodec_parameters_copy(context->dest_audio->codecpar, context->avfmt_in_ctx->streams[i]->codecpar) < 0) {
                    SAMPLE_LOG_E("avcodec_parameters_copy\n");
                    break;
                }
                context->dest_audio->codecpar->codec_tag = 0;
                break;
            }
        }

        for (unsigned int i = 0; i < 1; i++) {
            AVStream *in_stream = context->avfmt_in_ctx->streams[i];
            AVStream *out_stream = avformat_new_stream(context->avfmt_rtmp_ctx, context->avfmt_in_ctx->video_codec);
            if (!out_stream) {
                fprintf(stderr, "Failed to allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                break;
            }

            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) {
                fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
                break;
            }

            out_stream->codecpar->codec_tag = 0;
        }

        if (-1 == context->video_track_id) {
            ret = -EINVAL;
            SAMPLE_LOG_E("[%d] has no video stream", context->cookie);
            break;
        } else {
            AVStream *avs = context->avfmt_in_ctx->streams[context->video_track_id];
            switch (avs->codecpar->codec_id) {
                case AV_CODEC_ID_H264:
                    context->info.video.payload = PT_H264;
                    break;
                case AV_CODEC_ID_HEVC:
                    context->info.video.payload = PT_H265;
                    break;
                default:
                    SAMPLE_LOG_E("[%d] unsupport video codec %d!", context->cookie, avs->codecpar->codec_id);
                    break;
            }

            context->info.video.width = avs->codecpar->width;
            context->info.video.height = avs->codecpar->height;
            context->fifo = new nalu_lock_fifo(context->info.video.width * context->info.video.height * 2);
            context->fifo_v = new nalu_lock_fifo(context->info.video.width * context->info.video.height * 2);

            if (avs->avg_frame_rate.den == 0 || (avs->avg_frame_rate.num == 0 && avs->avg_frame_rate.den == 1)) {
                context->info.video.fps = static_cast<uint32_t>(round(av_q2d(avs->r_frame_rate)));
            } else {
                context->info.video.fps = static_cast<uint32_t>(round(av_q2d(avs->avg_frame_rate)));
            }

            if (0 == context->info.video.fps) {
                context->info.video.fps = 25;
                SAMPLE_LOG_W("[%d] url %s: fps is 0, set to %d fps", context->cookie, context->url.c_str(), context->info.video.fps);
            }

            SAMPLE_LOG_I("[%d] url %s: codec %d, %dx%d, fps %d", context->cookie, context->url.c_str(), context->info.video.payload,
                         context->info.video.width, context->info.video.height, context->info.video.fps);
        }

        if (PT_H264 == context->info.video.payload || PT_H265 == context->info.video.payload) {
            const AVBitStreamFilter *bsf =
                av_bsf_get_by_name((PT_H264 == context->info.video.payload) ? "h264_mp4toannexb" : "hevc_mp4toannexb");
            if (!bsf) {
                ret = -EINVAL;
                SAMPLE_LOG_E("[%d] av_bsf_get_by_name() fail, make sure input is mp4|h264|h265", context->cookie);
                break;
            }

            ret = av_bsf_alloc(bsf, &context->avbsf_ctx);
            if (ret < 0) {
                SAMPLE_LOG_E("[%d] av_bsf_alloc() fail, %s", context->cookie, AVERRMSG(ret, msg));
                break;
            }

            ret = avcodec_parameters_copy(context->avbsf_ctx->par_in, context->avfmt_in_ctx->streams[context->video_track_id]->codecpar);
            if (ret < 0) {
                SAMPLE_LOG_E("[%d] avcodec_parameters_copy() fail, %s", context->cookie, AVERRMSG(ret, msg));
                break;
            }

            context->avbsf_ctx->time_base_in = context->avfmt_in_ctx->streams[context->video_track_id]->time_base;

            ret = av_bsf_init(context->avbsf_ctx);
            if (ret < 0) {
                SAMPLE_LOG_E("[%d] av_bsf_init() fail, %s", context->cookie, AVERRMSG(ret, msg));
                break;
            }
        }

        if (-1 == context->audio_track_id) {
            SAMPLE_LOG_E("no audio stream\n");
        }

        return 0;

    } while (0);

    ffmpeg_deinit_demuxer(context);
    return ret;
}

static int ffmpeg_deinit_demuxer(ffmpeg_context *context) {
    if (context->avfmt_in_ctx) {
        avformat_close_input(&context->avfmt_in_ctx);
        /*  avformat_close_input will free ctx
            http://ffmpeg.org/doxygen/trunk/demux_8c_source.html
        */
        // avformat_free_context(avfmt_in_ctx);
        context->avfmt_in_ctx = nullptr;
    }

    if (context->avfmt_mp4_ctx) {
        if (!(context->avfmt_mp4_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&context->avfmt_mp4_ctx->pb);

        avformat_close_input(&context->avfmt_mp4_ctx);
        context->avfmt_mp4_ctx = nullptr;
    }

    if (context->fifo) {
        delete context->fifo;
        context->fifo = nullptr;
    }

    if (context->fifo_v) {
        delete context->fifo_v;
        context->fifo_v = nullptr;
    }

    return 0;
}

int ffmpeg_set_demuxer_attr(ffmpeg_demuxer demuxer, const char *name, const void *attr) {
    if (!name) {
        SAMPLE_LOG_E("nil attribute name");
        return -EINVAL;
    }

    ffmpeg_context *context = FFMPEG_CONTEXT(demuxer);
    if (0 == strcmp(name, ffmpeg_demuxer_attr_frame_rate_control)) {
        context->frame_rate_control = (1 == *(reinterpret_cast<const int32_t *>(attr))) ? true : false;
        SAMPLE_LOG_I("[%d] set %s to %d", context->cookie, ffmpeg_demuxer_attr_frame_rate_control, context->frame_rate_control);
    } else if (0 == strcmp(name, ffmpeg_demuxer_attr_file_loop)) {
        context->loop = (1 == *(reinterpret_cast<const int32_t *>(attr))) ? true : false;
        SAMPLE_LOG_I("[%d] set %s to %d", context->cookie, ffmpeg_demuxer_attr_file_loop, context->loop);
    } else {
        SAMPLE_LOG_E("[%d] unsupport attribute %s", context->cookie, name);
        return -EINVAL;
    }

    return 0;
}
