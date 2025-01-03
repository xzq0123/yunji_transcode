/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef _AX_DMADIM_API_H_
#define _AX_DMADIM_API_H_

#include "ax_base_type.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    AX_DMADIM_ENDIAN_DEF = 0,
    AX_DMADIM_ENDIAN_32,
    AX_DMADIM_ENDIAN_16,
    AX_DMADIM_ENDIAN_MAX = AX_DMADIM_ENDIAN_16
} AX_DMADIM_ENDIAN_E;

typedef enum {
    AX_DMADIM_1D = 0,
    AX_DMADIM_2D,
    AX_DMADIM_3D,
    AX_DMADIM_4D,
    AX_DMADIM_MEMORY_INIT,
    AX_DMADIM_CHECKSUM,
} AX_DMADIM_XFER_MODE_E;

typedef struct {
    AX_S32 s32Id;
    AX_U32 u32CheckSum;
    AX_U32 u32Stat;
} AX_DMADIM_XFER_STAT_T;

typedef struct {
    AX_U64 u64PhyAddr;
    AX_U32 u32Imgw;
    AX_U32 u32Stride[3];
} AX_DMADIM_INFO_T;

/** @brief transmit information for AX_DMADIM_2D/AX_DMADIM_3D/AX_DMADIM_4D */
typedef struct {
    AX_U16 u16Ntiles[3];
    AX_DMADIM_INFO_T tSrcInfo;
    AX_DMADIM_INFO_T tDstInfo;
} AX_DMADIM_DESC_XD_T;

/** @brief transmit information for AX_DMADIM_MEMORY_INIT/AX_DMADIM_CHECKSUM/AX_DMADIM_1D */
typedef struct {
    AX_U64 u64PhySrc;
    AX_U64 u64PhyDst;
    AX_U32 u32Size;
} AX_DMADIM_DESC_T;


/**
 * @brief DIM transfer msg struct
 *
 * u32DescNum: The number of dma transmission nodes
 * eEndian: The endian for AX_DMADIM_1D/AX_DMADIM_2D/AX_DMADIM_3D/AX_DMADIM_4D
 * pDescBuf: dma transmission nodes, type is related to eDmaMode, and assigned
 *           (AX_DMADIM_DESC_T *) or (AX_DMADIM_DESC_XD_T *)
 * pfnCallBack: a call back func for dma transfer done,
 *             The first parameter save the dma transfer result
 *             The second parameter id pCbArg, customized by user
 * pCbArg: The second parameter of pfnCallBack
 * eDmaMode: dma transfer mode(AX_DMADIM_XFER_MODE_E)
 */
typedef struct {
    AX_U32 u32DescNum;
    AX_DMADIM_ENDIAN_E eEndian;
    AX_VOID *pDescBuf;
    AX_VOID(*pfnCallBack)(AX_DMADIM_XFER_STAT_T *, AX_VOID *);
    AX_VOID *pCbArg;
    AX_DMADIM_XFER_MODE_E eDmaMode;
} AX_DMADIM_MSG_T;


enum {
    AX_DMADIM_ERRNO_DEFAULT = 0x80,
    AX_DMADIM_ERRNO_ENOENT = 0x82,
    AX_DMADIM_ERRNO_EIO = 0x85,
    AX_DMADIM_ERRNO_EFAULT = 0x8E,
    AX_DMADIM_ERRNO_EINVAL = 0x96,
};

#define AX_ID_DMADIM                       0x1
#define AX_ERR_DMADIM_NOMEM                AX_DEF_ERR(AX_ID_DMA, AX_ID_DMADIM, AX_ERR_NOMEM)
#define AX_ERR_DMADIM_TIMEOUT              AX_DEF_ERR(AX_ID_DMA, AX_ID_DMADIM, AX_ERR_TIMED_OUT)

#define AX_ERR_DMADIM_EIO                  AX_DEF_ERR(AX_ID_DMA, AX_ID_DMADIM, AX_DMADIM_ERRNO_EIO)
#define AX_ERR_DMADIM_ENOENT               AX_DEF_ERR(AX_ID_DMA, AX_ID_DMADIM, AX_DMADIM_ERRNO_ENOENT)
#define AX_ERR_DMADIM_EFAULT               AX_DEF_ERR(AX_ID_DMA, AX_ID_DMADIM, AX_DMADIM_ERRNO_EFAULT)
#define AX_ERR_DMADIM_EINVAL               AX_DEF_ERR(AX_ID_DMA, AX_ID_DMADIM, AX_DMADIM_ERRNO_EINVAL)

#define DMADIM_XFER_SUCCESS     1

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
