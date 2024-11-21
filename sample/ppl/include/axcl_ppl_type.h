/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __AXCL_PPL_TYPE_H__
#define __AXCL_PPL_TYPE_H__

#include "axclite.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXCL_DEF_LITE_PPL_ERR(e)           AXCL_DEF_LITE_ERR(0, (e))
#define AXCL_ERR_LITE_PPL_NULL_POINTER     AXCL_DEF_LITE_PPL_ERR(AXCL_ERR_NULL_POINTER)
#define AXCL_ERR_LITE_PPL_ILLEGAL_PARAM    AXCL_DEF_LITE_PPL_ERR(AXCL_ERR_ILLEGAL_PARAM)
#define AXCL_ERR_LITE_PPL_UNSUPPORT        AXCL_DEF_LITE_PPL_ERR(AXCL_ERR_UNSUPPORT)
#define AXCL_ERR_LITE_PPL_NOT_STARTED      AXCL_DEF_LITE_PPL_ERR(0x81)
#define AXCL_ERR_LITE_PPL_CREATE           AXCL_DEF_LITE_PPL_ERR(0x82)
#define AXCL_ERR_LITE_PPL_INVALID_PPL      AXCL_DEF_LITE_PPL_ERR(0x83)

typedef void *axcl_ppl;
#define AXCL_INVALID_PPL (0)

typedef struct {
    const char *json; /* axcl.json path */
    AX_S32 device;
    AX_U32 modules;
    AX_U32 max_vdec_grp;
    AX_U32 max_venc_thd;
} axcl_ppl_init_param;

typedef enum {
    AXCL_PPL_TRANSCODE = 0, /* VDEC -> IVPS ->VENC */
    AXCL_PPL_BUTT
} axcl_ppl_type;

typedef struct {
    axcl_ppl_type ppl;
    void *param;
} axcl_ppl_param;

typedef struct {
    AX_U8 *nalu;
    AX_U32 nalu_len;
    AX_U64 pts;
    AX_U64 userdata;
} axcl_ppl_input_stream;

typedef AX_VENC_STREAM_T axcl_ppl_encoded_stream;
typedef void (*axcl_ppl_encoded_stream_callback_func)(axcl_ppl ppl, const axcl_ppl_encoded_stream *stream, AX_U64 userdata);

/* =================================== PPL param =================================== */
/* PPL: AXCL_PPL_TRANSCODE */
typedef struct {
    AX_PAYLOAD_TYPE_E payload;
    AX_U32 width;
    AX_U32 height;
    AX_VDEC_OUTPUT_ORDER_E output_order;
    AX_VDEC_DISPLAY_MODE_E display_mode;
} axcl_ppl_transcode_vdec_attr;

typedef struct {
    AX_PAYLOAD_TYPE_E payload;
    AX_U32 width;
    AX_U32 height;
    AX_VENC_PROFILE_E profile;
    AX_VENC_LEVEL_E level;
    AX_VENC_TIER_E tile;
    AX_VENC_RC_ATTR_T rc;
    AX_VENC_GOP_ATTR_T gop;
} axcl_ppl_transcode_venc_attr;

typedef struct {
    axcl_ppl_transcode_vdec_attr vdec;
    axcl_ppl_transcode_venc_attr venc;
    axcl_ppl_encoded_stream_callback_func cb;
    AX_U64 userdata;
} axcl_ppl_transcode_param;

#ifdef __cplusplus
}
#endif

#endif /* __AXCL_PPL_TYPE_H__ */