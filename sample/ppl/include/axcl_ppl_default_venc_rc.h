/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __AXCL_PPL_DEFALT_RC_H__
#define __AXCL_PPL_DEFALT_RC_H__

#include "axcl_ppl_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * default is refer to << 44 - AX 码率控制使用方法.docx >>
 */
const AX_VENC_RC_ATTR_T axcl_default_rc_h264_cbr_1080p_4096kbps = {
    .enRcMode = AX_VENC_RC_MODE_H264CBR,
    .s32FirstFrameStartQp = -1,
    .stFrameRate = {
        .fSrcFrameRate = 30.0F,
        .fDstFrameRate = 30.0F,
    },
    .stH264Cbr =
        {
            .u32Gop = 60,
            .u32BitRate = 4096,
            .u32MaxQp = 51,
            .u32MinQp = 10,
            .u32MaxIQp = 51,
            .u32MinIQp = 10,
            .u32MaxIprop = 40,
            .u32MinIprop = 10,
            .s32IntraQpDelta = -2,
            .u32IdrQpDeltaRange = 0,
        },
};

const AX_VENC_RC_ATTR_T axcl_default_rc_h264_vbr_1080p_4096kbps = {
    .enRcMode = AX_VENC_RC_MODE_H264VBR,
    .s32FirstFrameStartQp = -1,
    .stFrameRate = {
        .fSrcFrameRate = 30.0F,
        .fDstFrameRate = 30.0F,
    },
    .stH264Vbr =
        {
            .u32Gop = 60,
            .u32MaxBitRate = 4096,
            .u32MaxQp = 44,
            .u32MinQp = 10,
            .u32MaxIQp = 51,
            .u32MinIQp = 24,
            .s32IntraQpDelta = 0,
            .u32IdrQpDeltaRange = 0,
        },
};

const AX_VENC_RC_ATTR_T axcl_default_rc_h264_avbr_1080p_4096kbps = {
    .enRcMode = AX_VENC_RC_MODE_H264AVBR,
    .s32FirstFrameStartQp = -1,
    .stFrameRate = {
        .fSrcFrameRate = 30.0F,
        .fDstFrameRate = 30.0F,
    },
    .stH264AVbr =
        {
            .u32Gop = 60,
            .u32MaxBitRate = 4096,
            .u32MaxQp = 42,
            .u32MinQp = 10,
            .u32MaxIQp = 51,
            .u32MinIQp = 24,
            .s32IntraQpDelta = 0,
            .u32IdrQpDeltaRange = 0,
        },
};

const AX_VENC_RC_ATTR_T axcl_default_rc_h265_cbr_1080p_4096kbps = {
    .enRcMode = AX_VENC_RC_MODE_H265CBR,
    .s32FirstFrameStartQp = -1,
    .stFrameRate = {
        .fSrcFrameRate = 30.0F,
        .fDstFrameRate = 30.0F,
    },
    .stH265Cbr =
        {
            .u32Gop = 60,
            .u32BitRate = 4096,
            .u32MaxQp = 51,
            .u32MinQp = 10,
            .u32MaxIQp = 51,
            .u32MinIQp = 10,
            .u32MaxIprop = 40,
            .u32MinIprop = 10,
            .s32IntraQpDelta = -2,
            .u32IdrQpDeltaRange = 0,
        },
};

const AX_VENC_RC_ATTR_T axcl_default_rc_h265_vbr_1080p_4096kbps = {
    .enRcMode = AX_VENC_RC_MODE_H265VBR,
    .s32FirstFrameStartQp = -1,
    .stFrameRate = {
        .fSrcFrameRate = 30.0F,
        .fDstFrameRate = 30.0F,
    },
    .stH265Vbr =
        {
            .u32Gop = 60,
            .u32MaxBitRate = 4096,
            .u32MaxQp = 42,
            .u32MinQp = 10,
            .u32MaxIQp = 51,
            .u32MinIQp = 24,
            .s32IntraQpDelta = 0,
            .u32IdrQpDeltaRange = 0,
        },
};

const AX_VENC_RC_ATTR_T axcl_default_rc_h265_avbr_1080p_4096kbps = {
    .enRcMode = AX_VENC_RC_MODE_H265AVBR,
    .s32FirstFrameStartQp = -1,
    .stFrameRate = {
        .fSrcFrameRate = 30.0F,
        .fDstFrameRate = 30.0F,
    },
    .stH265AVbr =
        {
            .u32Gop = 60,
            .u32MaxBitRate = 4096,
            .u32MaxQp = 44,
            .u32MinQp = 10,
            .u32MaxIQp = 51,
            .u32MinIQp = 24,
            .s32IntraQpDelta = 0,
            .u32IdrQpDeltaRange = 0,
        },
};

#ifdef __cplusplus
}
#endif

#endif /* __AXCL_PPL_DEFALT_RC_H__ */