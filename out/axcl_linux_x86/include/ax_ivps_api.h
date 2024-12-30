/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef _AX_IVPS_API_H_
#define _AX_IVPS_API_H_
#include "ax_base_type.h"
#include "ax_global_type.h"
#include "ax_sys_api.h"
#include "ax_ivps_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /***************************************************************************************************************/
    /*                                                   PIPELINE                                                  */
    /***************************************************************************************************************/
    typedef struct
    {
        AX_IVPS_ASPECT_RATIO_E eMode;
        /* Typical RGB color:
     1. WHITE : 0xFFFFFF
     2. BLACK : 0x000000
     3. RED   : 0xFF0000
     4. BLUE  : 0x00FF00
     5. YELLOW: 0xFF00FF
     31       23      15      7       0
     |--------|---R---|---G---|---B---|
     */
        AX_U32 nBgColor;
        AX_IVPS_ASPECT_RATIO_ALIGN_E eAligns[2]; /* IVPS_ASPECT_RATIO_ALIGN: [0]: HORIZONTAL [1]: VERTICAL */
        AX_IVPS_RECT_T tRect;                    /* should be set in ASPECT_RATIO_MANUAL mode */
    } AX_IVPS_ASPECT_RATIO_T;

    typedef enum
    {
        AX_IVPS_ENGINE_SUBSIDIARY = 0, /* do not create owner workqueue, subsidiary of the previous filter */
        AX_IVPS_ENGINE_TDP,
        AX_IVPS_ENGINE_GDC,
        AX_IVPS_ENGINE_VPP,
        AX_IVPS_ENGINE_VGP,
        AX_IVPS_ENGINE_IVE,
        AX_IVPS_ENGINE_VO,
        AX_IVPS_ENGINE_DSP,
        /* AX_IVPS_ENGINE_DPU, */ /* depth processor unit */
        AX_IVPS_ENGINE_BUTT
    } AX_IVPS_ENGINE_E;

    /*
     * IVPS_PIPELINE_X is the mainstream pipeline we currently support. The mode is as follows:
     *                            => Pipe1Filter0 Pipe1Filter1 (chn 0)
     * Pipe0Filter0 Pipe0Filter1  => Pipe2Filter0 Pipe2Filter1 (chn 1)
     *                            => Pipe3Filter0 Pipe3Filter1 (chn 2)
     * Note: Filter[n]:
     * n: channel filter type(0: RFC/CROP/SCALE/ROTATE; 1:REGION)
     */
    typedef enum
    {
        AX_IVPS_PIPELINE_DEFAULT = 0,
        AX_IVPS_PIPELINE_BUTT
    } AX_IVPS_PIPELINE_E;

    /*
     * AX_IVPS_TDP_CFG_T
     * This configuration is specific to TDP engine.
     * This engine can support many functions,
     * such as mirror, flip, rotation, scale, mosaic, osd and so on.
     * If in eCompressMode, stride and width should be 64 pixels aligned.
     */
    typedef struct
    {
        AX_IVPS_ROTATION_E eRotation;
        AX_BOOL bMirror;
        AX_BOOL bFlip;
    } AX_IVPS_TDP_CFG_T;

    typedef enum
    {
        AX_IVPS_SCALE_NORMAL = 0,
        AX_IVPS_SCALE_UP,
        AX_IVPS_SCALE_DOWN,
        AX_IVPS_SCALE_BUTT
    } AX_IVPS_SCALE_MODE_E;

    /*
     * AX_IVPS_FILTER_T
     * A pipeline consists of one filter or several filters.
     * Each filter can complete specific functions. You can choose the engine to do this.
     * bInplace means drawing OSD/line/polygon/mosaic on the original frame directly.
     * Only TDP,VPP,VGP can support bInplace.
     * bInplace cannot coexist with rotation, scale, flip or mirror.
     */
    typedef struct
    {
        AX_BOOL bEngage;
        AX_IVPS_ENGINE_E eEngine;

        AX_FRAME_RATE_CTRL_T tFRC;
        AX_BOOL bCrop;
        AX_IVPS_RECT_T tCropRect;
        AX_U32 nDstPicWidth;
        AX_U32 nDstPicHeight;
        AX_U32 nDstPicStride;
        AX_IMG_FORMAT_E eDstPicFormat;
        AX_FRAME_COMPRESS_INFO_T tCompressInfo;
        AX_BOOL bInplace;
        AX_IVPS_ASPECT_RATIO_T tAspectRatio;
        union /* engine specific config data */
        {
            AX_IVPS_TDP_CFG_T tTdpCfg;
            AX_IVPS_GDC_CFG_T tGdcCfg;
        };
        AX_U32 nFRC; /* Reserved */
        AX_IVPS_SCALE_MODE_E eScaleMode;
    } AX_IVPS_FILTER_T;

    /*
     * AX_IVPS_PIPELINE_ATTR_T
     * Dynamic attribute for the group.
     * @nOutChnNum: [1~AX_IVPS_MAX_OUTCHN_NUM] set according to output channel number.
     * @nInDebugFifoDepth: [0~1024] used by ax_ivps_dump.bin.
     * @nOutFifoDepth: [0~4] If user want to get frame from channel, set it to [1~4].
     *  Otherwise, set it to 0.
     */
    typedef struct
    {
        AX_U8 nOutChnNum;
        AX_U16 nInDebugFifoDepth;
        AX_U8 nOutFifoDepth[AX_IVPS_MAX_OUTCHN_NUM];
        AX_IVPS_FILTER_T tFilter[AX_IVPS_MAX_OUTCHN_NUM + 1][AX_IVPS_MAX_FILTER_NUM_PER_OUTCHN];
    } AX_IVPS_PIPELINE_ATTR_T;

    /*
     * AX_IVPS_GRP_ATTR_T
     * Static attribute for the group.
     * @nInFifoDepth: default 2, if the memory is tight, it's better to set it to 1.
     */
    typedef struct
    {
        AX_U8 nInFifoDepth;
        AX_IVPS_PIPELINE_E ePipeline;
    } AX_IVPS_GRP_ATTR_T;

    typedef struct
    {
        AX_FRAME_RATE_CTRL_T tFRC;
        AX_U32 nDstPicWidth;
        AX_U32 nDstPicHeight;
        AX_U32 nDstPicStride;
        AX_IMG_FORMAT_E eDstPicFormat;
        AX_IVPS_ASPECT_RATIO_T tAspectRatio;
        AX_U8 nOutFifoDepth;
        AX_U32 nFRC; /* Reserved */
    } AX_IVPS_CHN_ATTR_T;

    typedef struct
    {
        /* Range: [0, 100] */
        AX_U64 TDP0_duty_cycle;
        AX_U64 TDP1_duty_cycle;
        AX_U64 GDC_duty_cycle;
        AX_U64 VPP_duty_cycle;
        AX_U64 VGP_duty_cycle;
        AX_U64 VDSP0_duty_cycle;
        AX_U64 VDSP1_duty_cycle;
    } AX_IVPS_DUTY_CYCLE_ATTR_T;

    /***************************************************************************************************************/
    /*                                               REGION                                                        */
    /***************************************************************************************************************/
    typedef AX_S32 IVPS_RGN_HANDLE;

#define AX_IVPS_MAX_RGN_HANDLE_NUM (384)
#define AX_IVPS_INVALID_REGION_HANDLE (IVPS_RGN_HANDLE)(-1)
#define AX_IVPS_REGION_MAX_DISP_NUM (32)

    typedef enum
    {
        AX_IVPS_RGN_TYPE_LINE = 0,
        AX_IVPS_RGN_TYPE_RECT = 1,
        AX_IVPS_RGN_TYPE_POLYGON = 2, /* convex quadrilateral */
        AX_IVPS_RGN_TYPE_MOSAIC = 3,
        AX_IVPS_RGN_TYPE_OSD = 4,
        AX_IVPS_RGN_TYPE_BUTT
    } AX_IVPS_RGN_TYPE_E;

    typedef struct
    {
        AX_S32 nZindex;           /* RW; if drawing OSD, for the same filter, different region handle owns different nZindex */
        AX_BOOL bSingleCanvas;    /* RW; AX_TURE: single canvas; AX_FALSE: double canvas */
        AX_U16 nAlpha;            /*RW; range: (0, 255]; 0: transparent, 255: opaque */
        AX_IMG_FORMAT_E eFormat;
        AX_BITCOLOR_T nBitColor;  /*RW; only for bitmap */
        AX_COLORKEY_T nColorKey;
    } AX_IVPS_RGN_CHN_ATTR_T;

    typedef struct
    {
        AX_POINT_T tPTs[2];       /* RW; fixed two point coordinates, starting and ending points of the line */
        AX_U32 nLineWidth;        /* RW; range: [1, 16]  */
        IVPS_RGB nColor;          /* RGB Color: 0xRRGGBB, eg: red: 0xFF0000 */
        AX_U8 nAlpha;             /* RW; range: (0, 255]; 0: transparent, 255: opaque */
    } AX_IVPS_RGN_LINE_T;

    typedef struct
    {
        union
        {
            AX_IVPS_RECT_T tRect;     /* rectangle info */
            AX_IVPS_POINT_T tPTs[AX_IVPS_MAX_POLYGON_POINT_NUM];  /* RW; polygon fixed-point coordinates */
        };
        AX_U8 nPointNum;              /* RW; range: [4, 10] [AX_IVPS_MIN_POLYGON_POINT_NUM, AX_IVPS_MAX_POLYGON_POINT_NUM]*/
        AX_U32 nLineWidth;            /* RW; range: [1, 16] */
        IVPS_RGB nColor;              /* RW; range: [0, 0xffffff]; color RGB888; 0xRRGGBB */
        AX_U8 nAlpha;                 /* RW; range: (0, 255]; 0: transparent, 255: opaque*/
        AX_BOOL bSolid;               /* if AX_TRUE, fill the rect with the nColor */

        AX_IVPS_CORNER_RECT_ATTR_T tCornerRect;
    } AX_IVPS_RGN_POLYGON_T;

    typedef struct
    {
        AX_IVPS_RECT_T tRect;
        AX_IVPS_MOSAIC_BLK_SIZE_E eBklSize;
    } AX_IVPS_RGN_MOSAIC_T;

    typedef AX_OSD_BMP_ATTR_T AX_IVPS_RGN_OSD_T;

    typedef union
    {
        AX_IVPS_RGN_LINE_T tLine;
        AX_IVPS_RGN_POLYGON_T tPolygon;
        AX_IVPS_RGN_MOSAIC_T tMosaic;
        AX_IVPS_RGN_OSD_T tOSD;
    } AX_IVPS_RGN_DISP_U;

    typedef struct
    {
        AX_BOOL bShow;
        AX_IVPS_RGN_TYPE_E eType;
        AX_IVPS_RGN_DISP_U uDisp;
    } AX_IVPS_RGN_DISP_T;

    typedef struct
    {
        AX_U32 nNum;
        AX_IVPS_RGN_CHN_ATTR_T tChnAttr;
        AX_IVPS_RGN_DISP_T arrDisp[AX_IVPS_REGION_MAX_DISP_NUM];
    } AX_IVPS_RGN_DISP_GROUP_T;

    typedef struct
    {
        AX_U16 nThick;
        AX_U16 nAlpha;
        AX_U32 nColor;
        AX_BOOL bSolid;  /* if AX_TRUE, fill the rect with the nColor */
        AX_BOOL bAbsCoo; /* is Absolute Coordinate */
        AX_IVPS_CORNER_RECT_ATTR_T tCornerRect;
    } AX_IVPS_GDI_ATTR_T;

    typedef struct
    {
        AX_U64 nPhyAddr;
        AX_VOID *pVirAddr;
        AX_U32 nUVOffset; /* Pixels of Y and UV offset. If YUV420 format, nUVOffset = u64PhyAddr[1] - u64PhyAddr[0] */
        AX_U32 nStride;
        AX_U16 nW;
        AX_U16 nH;
        AX_IMG_FORMAT_E eFormat;
    } AX_IVPS_RGN_CANVAS_INFO_T;

    /*
    Create region
    @return : return the region handle created
    */
#ifdef __cplusplus
}
#endif
#endif /* _AX_IVPS_API_H_ */
