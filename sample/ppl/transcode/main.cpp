/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <string>
#include "axcl_ppl.h"
#include "axcl_ppl_default_venc_rc.h"
#include "cmdline.h"
#include "demux/ffmpeg.hpp"
#include "nalu_lock_fifo.hpp"
#include "ppl_userdata.hpp"
#include "res_guard.hpp"
#include "threadx.hpp"
#include "utils/logger.h"

#include "libavcodec/packet.h"

static int32_t quit = 0;
static void handler(int s) {
    SAMPLE_LOG_W("\n====================== pid %d caught signal: %d ======================\n", getpid(), s);
    quit = 1;
}

/**
 * @brief Retrieves transcode pipeline parameters from stream information.
 * @param info Stream information, including width, height, payload, and fps.
 * @param cb Callback function to handle the transcoded stream processed by VDEC - IVPS - VENC.
 * @param userdata User data for the callback function.
 *                 The entire pipeline will pass the user data to axcl_ppl_encoded_stream_callback_func.
 * @return axcl_ppl_transcode_param
 */

static axcl_ppl_transcode_param get_transcode_ppl_param_from_stream(const struct stream_info *info,
                                                                    axcl_ppl_encoded_stream_callback_func cb, uint64_t userdata);

/**
 * @brief Processes video stream nalu frame data after pulling the stream (e.g., via RTSP ... protocols).
 *        Once the nalu frame is received, the axcl_ppl_send_stream API is called to send the nalu frame to VDEC for decoding.
 *
 *        As for example, the nalu frames are obtained by demuxing a local video file (mp4|.264|.265).
 *        There is no frame rate control, and this example uses delays to simulate frame rate control for stream pulling.
 * @param nalu specifies the nalu frame obtained by demuxing a local video file.
 * @param userdata bypass user data to axcl_ppl_encoded_stream.userdata
 */
static void handle_down_streaming_nalu_frame(const struct stream_data *nalu, uint64_t userdata);

/**
 * @brief The axcl_ppl processes transcoded nalu frame by this callback function.
 *        Note: Avoid any high-latency operations within this callback function.
 * @param ppl ppl handle create by axcl_ppl_create
 * @param stream nalu stream encoded by VENC
 * @param userdata bypass user data from axcl_ppl_transcode_param.userdata
 */
static void handle_encoded_stream_callback(axcl_ppl ppl, const axcl_ppl_encoded_stream *stream, AX_U64 userdata);

int main(int argc, char *argv[]) {
    const int32_t pid = static_cast<int32_t>(getpid());
    SAMPLE_LOG_I("============== %s sample started %s %s pid %d ==============\n", AXCL_BUILD_VERSION, __DATE__, __TIME__, pid);
    signal(SIGINT, handler);

    cmdline::parser a;
    a.add<std::string>("url", 'i', "mp4|.264|.265 file path", true);
    a.add<std::string>("rtmp_url", 'o', "rtmp url", true);
    a.add<int32_t>("device", 'd', "device id", true);
    a.add<uint32_t>("width", 'w', "output width, 0 means same as input", false, 0);
    a.add<uint32_t>("height", 'h', "output height, 0 means same as input", false, 0);
    a.add<std::string>("json", '\0', "axcl.json path", false, "./axcl.json");
    a.add<int32_t>("loop", '\0', "1: loop demux for local file  0: no loop(default)", false, 0, cmdline::oneof(0, 1));
    a.add<int32_t>("dump", '\0', "1: dump file  0: no dump(default)", false, 0, cmdline::oneof(0, 1));
    a.parse_check(argc, argv);
    const std::string url = a.get<std::string>("url");
    const std::string rtmp_url = a.get<std::string>("rtmp_url");
    const int32_t device = a.get<int32_t>("device");
    const uint32_t width = a.get<uint32_t>("width");
    const uint32_t height = a.get<uint32_t>("height");
    const std::string json = a.get<std::string>("json");
    int32_t loop = a.get<int32_t>("loop");
    int32_t dump = a.get<int32_t>("dump");
    if (dump) {
        SAMPLE_LOG_W("if enable dump, disable loop automatically");
        loop = 0;
    }

    axclError ret;
    axcl_ppl ppl;
    std::unique_ptr<ppl_user_data, decltype(&ppl_user_data::destroy)> ppl_data(ppl_user_data::alloc(dump), &ppl_user_data::destroy);
    ppl_user_data *ppl_info = ppl_data.get();
    ppl_info->payload = PT_H264;

    /**
     * @brief Initialize system runtime:
     *     1. axclInit
     *     2. axclrtSetDevice
     *     3. initialize media modules
     * @param AXCL_LITE_DEFAULT means VDEC + IVPS + VENC modules
     */
    axcl_ppl_init_param init_param;
    memset(&init_param, 0, sizeof(init_param));
    init_param.json = json.c_str();
    init_param.device = device;
    init_param.modules = AXCL_LITE_DEFAULT;
    /**
     *  !!!
     *  should >= the number of processes
     */
    init_param.max_vdec_grp = 32;
    init_param.max_venc_thd = 1;
    if (ret = axcl_ppl_init(&init_param); AXCL_SUCC != ret) {
        SAMPLE_LOG_E("axcl_ppl_init(device %d) fail, ret = 0x%x", device, ret);
        return 1;
    }

    /**
     * @brief Create ffmpeg demuxer
     *        Use RAII to auto release resource of demuxer by res_guard template class
     *    1. Probe stream
     *    2. Retrieve stream information such as width, height, payload, fps.
     */
    res_guard<ffmpeg_demuxer> demuxer_holder(
        [&url, &rtmp_url, device, pid]() -> ffmpeg_demuxer {
            ffmpeg_demuxer handle;
            ffmpeg_create_demuxer(&handle, url.c_str(), rtmp_url.c_str(), PT_H264, device, {}, 0);
            return handle;
        },
        [](ffmpeg_demuxer &handle) {
            if (handle) {
                ffmpeg_destory_demuxer(handle);
            }
        });
    ffmpeg_demuxer demuxer = demuxer_holder.get();

    constexpr int32_t active_fps = 1;
    ffmpeg_set_demuxer_attr(demuxer, "ffmpeg.demux.file.frc", (const void *)&active_fps);
    ffmpeg_set_demuxer_attr(demuxer, "ffmpeg.demux.file.loop", (const void *)&loop);
    const struct stream_info *info = ffmpeg_get_stream_info(demuxer);

    /**
     * @brief Config input and output parameters (axcl_ppl_transcode_param) of AXCL_PPL_TRANSCODE ppl,
     *       and then create PPL (VDEC - IVPS - VENC)
     */
    axcl_ppl_transcode_param transcode_param = get_transcode_ppl_param_from_stream(
        ffmpeg_get_stream_info(demuxer), handle_encoded_stream_callback, reinterpret_cast<uint64_t>(ppl_info));
    if (width > 0 && height > 0) {
        transcode_param.venc.width = width;
        transcode_param.venc.height = height;
    }

    axcl_ppl_param ppl_param;
    memset(&ppl_param, 0, sizeof(ppl_param));
    ppl_param.ppl = AXCL_PPL_TRANSCODE;
    ppl_param.param = (void *)&transcode_param;
    if (ret = axcl_ppl_create(&ppl, &ppl_param); AXCL_SUCC != ret) {
        SAMPLE_LOG_E("axcl_ppl_create(device %d) fail, ret = 0x%x", device, ret);
        axcl_ppl_deinit();
        return 1;
    } else {
        /**
         * @brief Regist callback to handle ffmpeg demux thread
         * @param handle_down_streaming_nalu_frame callback to handle nalu frame
         * @param ppl_info callback user data
         */
        ppl_info->ppl = ppl;
        ppl_info->device = device;
        ppl_info->fps = info->video.fps;
        ppl_info->demuxer = demuxer;

        /* below attributes are for debug or print log, not mandatory */
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.vdec.grp", reinterpret_cast<void *>(&ppl_info->vdec));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.ivps.grp", reinterpret_cast<void *>(&ppl_info->ivps));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.venc.chn", reinterpret_cast<void *>(&ppl_info->venc));
        SAMPLE_LOG_I("pid %d: [vdec %02d] - [ivps %02d] - [venc %02d]", pid, ppl_info->vdec, ppl_info->ivps, ppl_info->venc);

        uint32_t vdec_blk_cnt;
        uint32_t vdec_out_depth;
        uint32_t ivps_in_depth;
        uint32_t ivps_out_depth;
        uint32_t ivps_blk_cnt;
        uint32_t ivps_engine;
        uint32_t venc_in_depth;
        uint32_t venc_out_depth;
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.vdec.blk.cnt", reinterpret_cast<void *>(&vdec_blk_cnt));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.vdec.out.depth", reinterpret_cast<void *>(&vdec_out_depth));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.ivps.in.depth", reinterpret_cast<void *>(&ivps_in_depth));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.ivps.out.depth", reinterpret_cast<void *>(&ivps_out_depth));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.ivps.blk.cnt", reinterpret_cast<void *>(&ivps_blk_cnt));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.ivps.engine", reinterpret_cast<void *>(&ivps_engine));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.venc.in.depth", reinterpret_cast<void *>(&venc_in_depth));
        axcl_ppl_get_attr(ppl, "axcl.ppl.transcode.venc.out.depth", reinterpret_cast<void *>(&venc_out_depth));
        SAMPLE_LOG_I("pid %d: VDEC attr ==> blk cnt: %d, fifo depth: out %d", pid, vdec_blk_cnt, vdec_out_depth);
        SAMPLE_LOG_I("pid %d: IVPS attr ==> blk cnt: %d, fifo depth: in %d, out %d, engine %d", pid, ivps_blk_cnt, ivps_in_depth,
                     ivps_out_depth, ivps_engine);
        SAMPLE_LOG_I("pid %d: VENC attr ==> fifo depth: in %d, out %d", pid, venc_in_depth, venc_out_depth);

        ffmpeg_set_demuxer_sink(demuxer, {handle_down_streaming_nalu_frame}, reinterpret_cast<uint64_t>(ppl_info));
    }

    /**
     * @brief Start transcode ppl and then start to demux.
     */
    if (ret = axcl_ppl_start(ppl); AXCL_SUCC != ret) {
        axcl_ppl_destroy(ppl);
        axcl_ppl_deinit();
        return 1;
    }

    ffmpeg_start_demuxer(demuxer);

    /**
     * transcoding ...
     */
    while (!quit) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (0 == ffmpeg_wait_demuxer_eof(demuxer, 0)) {
            /**
             * @brief wait eof of local stream file
             */
            SAMPLE_LOG_I("ffmpeg (pid %d) demux eof", static_cast<uint32_t>(getpid()));
            break;
        }
    }

    /**
     * @brief Stop ppl and ffmpeg demuxer.
     */
    axcl_ppl_stop(ppl);
    ffmpeg_stop_demuxer(demuxer);

    /**
     * @brief Destroy ppl and deinitialize system.
     */
    axcl_ppl_destroy(ppl);
    axcl_ppl_deinit();

    SAMPLE_LOG_I("total transcoded frames: %ld", ppl_info->frame_count);
    SAMPLE_LOG_I("============== %s sample exited %s %s pid %d ==============\n", AXCL_BUILD_VERSION, __DATE__, __TIME__, pid);
    return 0;
}

#if 0
static void sample_down_streaming_thread(axcl_ppl ppl, int32_t device) {
    SAMPLE_LOG_I("+++");

    /* thread runtime context is needed */
    axclError ret;
    axclrtContext context;
    ret = axclrtCreateContext(&context, device);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("axclrtCreateContext(device %d) fail, ret = 0x%x", device, ret);
        return;
    }

    constexpr int32_t timeout = 1000;
    axcl_ppl_input_stream stream = {0};
    while (!quit) {
        /**
         * TODO: fill axcl_ppl_input_stream
         */

        ret = axcl_ppl_send_stream(ppl, &stream, timeout);
        if (AXCL_SUCC != ret) {
            SAMPLE_LOG_E("axcl_ppl_send_stream(device %d) fail, ret = 0x%x", device, ret);
            return;
        }
    }

    axclrtDestroyContext(context);
    SAMPLE_LOG_I("---");
}
#endif

static axcl_ppl_transcode_param get_transcode_ppl_param_from_stream(const struct stream_info *info,
                                                                    axcl_ppl_encoded_stream_callback_func cb, uint64_t userdata) {
    ppl_user_data *ppl_info = reinterpret_cast<ppl_user_data *>(userdata);

    axcl_ppl_transcode_param transcode_param;
    memset(&transcode_param, 0, sizeof(transcode_param));
    transcode_param.vdec.payload = info->video.payload;
    transcode_param.vdec.width = info->video.width;
    transcode_param.vdec.height = info->video.height;
    transcode_param.vdec.output_order = AX_VDEC_OUTPUT_ORDER_DEC;

    /**
     * AX_VDEC_DISPLAY_MODE_PREVIEW :  pull-streaming such as RTSP
     * AX_VDEC_DISPLAY_MODE_PLAYBACK:  local stream file
     */
    transcode_param.vdec.display_mode = AX_VDEC_DISPLAY_MODE_PLAYBACK;

    transcode_param.venc.width = info->video.width;
    transcode_param.venc.height = info->video.height;
    if (ppl_info->payload == PT_H264) {
        // H264
        transcode_param.venc.payload = ppl_info->payload;
        transcode_param.venc.profile = AX_VENC_H264_BASE_PROFILE;
        transcode_param.venc.level = AX_VENC_H264_LEVEL_5_2;
        transcode_param.venc.gop.enGopMode = AX_VENC_GOPMODE_NORMALP;
        transcode_param.venc.rc = axcl_default_rc_h264_cbr_1080p_4096kbps;
        transcode_param.venc.rc.stH264Cbr.u32Gop = info->video.fps * 2;
    } else if (ppl_info->payload == PT_H265) {
        // H265
        transcode_param.venc.payload = ppl_info->payload;
        transcode_param.venc.profile = AX_VENC_HEVC_MAIN_PROFILE;
        transcode_param.venc.level = AX_VENC_HEVC_LEVEL_5_2;
        transcode_param.venc.gop.enGopMode = AX_VENC_GOPMODE_NORMALP;
        transcode_param.venc.rc = axcl_default_rc_h265_cbr_1080p_4096kbps;
        transcode_param.venc.rc.stH265Cbr.u32Gop = info->video.fps * 2;
    }
    transcode_param.venc.rc.stFrameRate.fSrcFrameRate = info->video.fps;
    transcode_param.venc.rc.stFrameRate.fDstFrameRate = info->video.fps;

    transcode_param.cb = cb;
    transcode_param.userdata = userdata;

    return transcode_param;
}


//这个回调就是ffmpeg解码之后的回调。
static void handle_down_streaming_nalu_frame(const struct stream_data *nalu, uint64_t userdata) {
    ppl_user_data *ppl_info = reinterpret_cast<ppl_user_data *>(userdata);

    axcl_ppl_input_stream stream;
    stream.nalu = nalu->video.data;
    stream.nalu_len = nalu->video.size;
    stream.pts = nalu->video.pts;
    stream.userdata = userdata;
    if (axclError ret = axcl_ppl_send_stream(ppl_info->ppl, &stream, 1000); AXCL_SUCC != ret) {
        if (AXCL_ERR_LITE_PPL_NOT_STARTED != ret) {
            SAMPLE_LOG_E("axcl_ppl_send_stream(pid: %d) fail, ret = 0x%x", static_cast<uint32_t>(ppl_info->pid), ret);
        }
    }
    // SAMPLE_LOG_I("axcl_ppl_send_stream(pid: %d)", static_cast<uint32_t>(ppl_info->pid));
}

//这个线程应该是编码之后回调的线程
static void handle_encoded_stream_callback(axcl_ppl ppl, const axcl_ppl_encoded_stream *stream, AX_U64 userdata) {
    ppl_user_data *ppl_info = reinterpret_cast<ppl_user_data *>(userdata);
    /**
     * Avoid any high-latency operations within this callback function.
     */
    // SAMPLE_LOG_I("received encoded nalu size %d", stream->stPack.u32Len);
    if (ppl_info->fp) {
        fwrite(stream->stPack.pu8Addr, 1, stream->stPack.u32Len, ppl_info->fp);
    }

    nalu_data nalu = {};
    nalu.pts = stream->stPack.u64PTS;
    nalu.nalu = stream->stPack.pu8Addr;
    nalu.len = stream->stPack.u32Len;
    ffmpeg_push_video_nalu(ppl_info->demuxer, &nalu);

    ++ppl_info->frame_count;
}
