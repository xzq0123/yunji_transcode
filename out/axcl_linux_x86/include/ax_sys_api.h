/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef _AX_SYS_API_
#define _AX_SYS_API_
#include "ax_global_type.h"
#include "ax_base_type.h"
#include "ax_pool_type.h"

/* PowerManager Notify Callback */
typedef AX_S32 (*NotifyEventCallback)(const AX_NOTIFY_EVENT_E event,AX_VOID * pdata);

/* flags */
#define AX_MEM_CACHED (1 << 1)    /* alloc mem is cached */
#define AX_MEM_NONCACHED (1 << 2) /* alloc mem is not cached */

typedef struct {
    AX_U64 PhysAddr;
    AX_U32 SizeKB;
    AX_S8  Name[AX_MAX_PARTITION_NAME_LEN];
} AX_PARTITION_INFO_T;

typedef struct {
    AX_U32 PartitionCnt;/* range:1~AX_MAX_PARTITION_COUNT */
    AX_PARTITION_INFO_T PartitionInfo[AX_MAX_PARTITION_COUNT];
} AX_CMM_PARTITION_INFO_T;

typedef struct {
    AX_U32 TotalSize;
    AX_U32 RemainSize;
    AX_U32 BlockCnt;
    AX_CMM_PARTITION_INFO_T Partition;
} AX_CMM_STATUS_T;

/* error code define */
#define AX_ERR_CMM_ILLEGAL_PARAM    AX_DEF_ERR(AX_ID_SYS, 0x00, AX_ERR_ILLEGAL_PARAM)  //0x800B000A
#define AX_ERR_CMM_NULL_PTR         AX_DEF_ERR(AX_ID_SYS, 0x00, AX_ERR_NULL_PTR)       //0x800B000B
#define AX_ERR_CMM_NOTREADY         AX_DEF_ERR(AX_ID_SYS, 0x00, AX_ERR_SYS_NOTREADY)   //0x800B0010
#define AX_ERR_CMM_NOMEM            AX_DEF_ERR(AX_ID_SYS, 0x00, AX_ERR_NOMEM)          //0x800B0018
#define AX_ERR_CMM_UNKNOWN          AX_DEF_ERR(AX_ID_SYS, 0x00, AX_ERR_UNKNOWN)        //0x800B0029
#define AX_ERR_CMM_MMAP_FAIL        AX_DEF_ERR(AX_ID_SYS, 0x00, 0x80)                  //0x800B0080
#define AX_ERR_CMM_MUNMAP_FAIL      AX_DEF_ERR(AX_ID_SYS, 0x00, 0x81)                  //0x800B0081
#define AX_ERR_CMM_FREE_FAIL        AX_DEF_ERR(AX_ID_SYS, 0x00, 0x82)                  //0x800B0082

#define AX_ERR_PTS_ILLEGAL_PARAM    AX_DEF_ERR(AX_ID_SYS, 0x02, AX_ERR_ILLEGAL_PARAM)  //0x800B020A
#define AX_ERR_PTS_NULL_PTR         AX_DEF_ERR(AX_ID_SYS, 0x02, AX_ERR_NULL_PTR)       //0x800B020B
#define AX_ERR_PTS_NOTREADY         AX_DEF_ERR(AX_ID_SYS, 0x02, AX_ERR_SYS_NOTREADY)   //0x800B0210
#define AX_ERR_PTS_NOT_PERM         AX_DEF_ERR(AX_ID_SYS, 0x02, AX_ERR_NOT_PERM)       //0x800B0215
#define AX_ERR_PTS_UNKNOWN          AX_DEF_ERR(AX_ID_SYS, 0x02, AX_ERR_UNKNOWN)        //0x800B0229
#define AX_ERR_PTS_COPY_TO_USER     AX_DEF_ERR(AX_ID_SYS, 0x02, 0x82)                  //0x800B0282
#define AX_ERR_PTS_COPY_FROM_USER   AX_DEF_ERR(AX_ID_SYS, 0x02, 0x83)                  //0x800B0283

#define AX_ERR_LINK_ILLEGAL_PARAM   AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_ILLEGAL_PARAM)  //0x800B030A
#define AX_ERR_LINK_NULL_PTR        AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_NULL_PTR)       //0x800B030B
#define AX_ERR_LINK_NOTREADY        AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_SYS_NOTREADY)   //0x800B0310
#define AX_ERR_LINK_NOT_SUPPORT     AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_NOT_SUPPORT)    //0x800B0314
#define AX_ERR_LINK_NOT_PERM        AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_NOT_PERM)       //0x800B0315
#define AX_ERR_LINK_UNEXIST         AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_UNEXIST)        //0x800B0317
#define AX_ERR_LINK_UNKNOWN         AX_DEF_ERR(AX_ID_SYS, 0x03, AX_ERR_UNKNOWN)        //0x800B0329
#define AX_ERR_LINK_TABLE_FULL      AX_DEF_ERR(AX_ID_SYS, 0x03, 0x80)                  //0x800B0380
#define AX_ERR_LINK_TABLE_EMPTY     AX_DEF_ERR(AX_ID_SYS, 0x03, 0x81)                  //0x800B0381
#define AX_ERR_LINK_COPY_TO_USER    AX_DEF_ERR(AX_ID_SYS, 0x03, 0x82)                  //0x800B0382
#define AX_ERR_LINK_COPY_FROM_USER  AX_DEF_ERR(AX_ID_SYS, 0x03, 0x83)                  //0x800B0383

#define AX_ERR_CHIP_TYPE_NOTREADY   AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_SYS_NOTREADY)   //0x800B0410
#define AX_ERR_CHIP_TYPE_NOMEM      AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_NOMEM)          //0x800B0418
#define AX_ERR_PWR_ILLEGAL_PARAM    AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_ILLEGAL_PARAM)  //0x800B040A
#define AX_ERR_PWR_NULL_PTR         AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_NULL_PTR)       //0x800B040B
#define AX_ERR_PWR_NOTREADY         AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_SYS_NOTREADY)   //0x800B0410
#define AX_ERR_PWR_NOT_PERM         AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_NOT_PERM)       //0x800B0415
#define AX_ERR_PWR_UNKNOWN          AX_DEF_ERR(AX_ID_SYS, 0x04, AX_ERR_UNKNOWN)        //0x800B0429

#endif //_AX_SYS_API_
