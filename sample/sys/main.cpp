/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "../utils/logger.h"
#include "axcl.h"
#include "cmdline.h"

static int sample_sys_alloc_free();
static int sample_sys_alloc_cache_free();
static int sample_sys_commom_pool();
static int sample_sys_private_pool();
static int sample_sys_link();

int main(int argc, char *argv[]) {
    cmdline::parser a;
    a.add<int32_t>("device", 'd', "device id", false, 0);
    a.add<std::string>("json", '\0', "axcl.json path", false, "./axcl.json");
    a.parse_check(argc, argv);
    int32_t device = a.get<int32_t>("device");
    const std::string json = a.get<std::string>("json");

    axclError ret;
    SAMPLE_LOG_I("json: %s", json.c_str());
    ret = axclInit(json.c_str());
    if (AXCL_SUCC != ret) {
        return 1;
    }

    if (device <= 0) {
        axclrtDeviceList lst;
        if (axclError ret = axclrtGetDeviceList(&lst); AXCL_SUCC != ret || 0 == lst.num) {
            SAMPLE_LOG_E("no device is connected");
            axclFinalize();
            return 1;
        }

        device = lst.devices[0];
        SAMPLE_LOG_I("device id: %d", device);
    }

    ret = axclrtSetDevice(device);
    if (AXCL_SUCC != ret) {
        axclFinalize();
        return 1;
    }

    sample_sys_alloc_free();
    sample_sys_alloc_cache_free();
    sample_sys_commom_pool();
    sample_sys_private_pool();
    sample_sys_link();

    axclrtResetDevice(device);
    axclFinalize();
    return 0;
}

static int sample_sys_alloc_free() {
    AX_S32 ret, i;
    AX_U64 PhyAddr[10]= { 0 };
    AX_VOID *pVirAddr[10] = { 0 };
    AX_U32 BlockSize = 1 * 1024 * 1024;
    AX_U32 align = 0x1000;
    AX_S8 blockname[20];

    SAMPLE_LOG_I("sys_alloc_free begin\n");

    for (i = 0; i < 10; i++) {
        sprintf((char *)blockname, "noncache_block_%d", i);
        ret = AXCL_SYS_MemAlloc(&PhyAddr[i], (AX_VOID **)&pVirAddr[i], BlockSize, align, blockname);
        if (AXCL_SUCC != ret) {
            SAMPLE_LOG_E("AXCL_SYS_MemAlloc failed\n");
            return -1;
        }
        SAMPLE_LOG_I("alloc PhyAddr= 0x%llx,pVirAddr=%p\n", PhyAddr[i], pVirAddr[i]);
    }

    for (i = 0; i < 10; i++) {
        ret = AXCL_SYS_MemFree(PhyAddr[i], (AX_VOID *)pVirAddr[i]);
        if (AXCL_SUCC != ret) {
            SAMPLE_LOG_E("AXCL_SYS_MemFree failed\n");
            return -1;
        }
        SAMPLE_LOG_I("free PhyAddr= 0x%llx,pVirAddr=%p\n", PhyAddr[i], pVirAddr[i]);
    }

    SAMPLE_LOG_I("sys_alloc_free end success\n");
    return ret;
}

static int sample_sys_alloc_cache_free() {
    AX_S32 ret, i;
    AX_U64 PhyAddr[10]= { 0 };
    AX_VOID *pVirAddr[10] = { 0 };
    AX_U32 BlockSize = 1 * 1024 * 1024;
    AX_U32 align = 0x1000;
    AX_S8 blockname[20];

    SAMPLE_LOG_I("sys_alloc_cache_free begin\n");

    for (i = 0; i < 10; i++) {
        sprintf((char *)blockname, "cache_block_%d", i);
        ret = AXCL_SYS_MemAllocCached(&PhyAddr[i], (AX_VOID **)&pVirAddr[i], BlockSize, align, blockname);
        if (AXCL_SUCC != ret) {
            SAMPLE_LOG_E("AXCL_SYS_MemAllocCached failed\n");
            return -1;
        }
        SAMPLE_LOG_I("alloc PhyAddr= 0x%llx,pVirAddr=%p\n", PhyAddr[i], pVirAddr[i]);
    }

    for (i = 0; i < 10; i++) {
        ret = AXCL_SYS_MemFree(PhyAddr[i], (AX_VOID *)pVirAddr[i]);
        if (AXCL_SUCC != ret) {
            SAMPLE_LOG_E("AXCL_SYS_MemFree failed\n");
            return -1;
        }
        SAMPLE_LOG_I("free PhyAddr= 0x%llx,pVirAddr=%p\n", PhyAddr[i], pVirAddr[i]);
    }

    SAMPLE_LOG_I("sys_alloc_cache_free end success\n");
    return ret;
}

static int sample_sys_commom_pool() {
    AX_S32 ret = 0;
    AX_POOL_FLOORPLAN_T PoolFloorPlan = { 0 };
    AX_U64 BlkSize;
    AX_BLK BlkId;
    AX_POOL PoolId;
    AX_U64 PhysAddr, MetaPhysAddr;

    SAMPLE_LOG_I("sys_commom_pool begin\n");

    /* step 1:SYS模块初始化 */
    ret = AXCL_SYS_Init();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Init fail!!ret:0x%x\n", ret);
        return -1;
    } else {
        SAMPLE_LOG_I("AXCL_SYS_Init success!\n");
    }

    /* step 2:去初始化缓存池 */
    ret = AXCL_POOL_Exit();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_Exit fail!!ret:0x%x\n", ret);
        goto error0;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Exit success!\n");
    }

    /* step 3:设置公共缓存池参数。如下设置了三个common pool,均在anonymous分段,每个Pool里面BlockSize不同,metasize都为0x2000字节 */
    memset(&PoolFloorPlan, 0, sizeof(AX_POOL_FLOORPLAN_T));
    PoolFloorPlan.CommPool[0].MetaSize   = 0x2000;
    PoolFloorPlan.CommPool[0].BlkSize   = 1 * 1024 * 1024;
    PoolFloorPlan.CommPool[0].BlkCnt    = 5;
    PoolFloorPlan.CommPool[0].CacheMode = POOL_CACHE_MODE_NONCACHE;
    PoolFloorPlan.CommPool[1].MetaSize   = 0x2000;
    PoolFloorPlan.CommPool[1].BlkSize   = 2 * 1024 * 1024;
    PoolFloorPlan.CommPool[1].BlkCnt    = 5;
    PoolFloorPlan.CommPool[1].CacheMode = POOL_CACHE_MODE_NONCACHE;
    PoolFloorPlan.CommPool[2].MetaSize   = 0x2000;
    PoolFloorPlan.CommPool[2].BlkSize   = 3 * 1024 * 1024;
    PoolFloorPlan.CommPool[2].BlkCnt    = 5;
    PoolFloorPlan.CommPool[2].CacheMode = POOL_CACHE_MODE_NONCACHE;

    /* PartitionName默认是anonymous分段,分段名必须是cmm模块加载时存在的 */
    memset(PoolFloorPlan.CommPool[0].PartitionName, 0, sizeof(PoolFloorPlan.CommPool[0].PartitionName));
    memset(PoolFloorPlan.CommPool[1].PartitionName, 0, sizeof(PoolFloorPlan.CommPool[1].PartitionName));
    memset(PoolFloorPlan.CommPool[2].PartitionName, 0, sizeof(PoolFloorPlan.CommPool[2].PartitionName));
    strcpy((char *)PoolFloorPlan.CommPool[0].PartitionName, "anonymous");
    strcpy((char *)PoolFloorPlan.CommPool[1].PartitionName, "anonymous");
    strcpy((char *)PoolFloorPlan.CommPool[2].PartitionName, "anonymous");

    ret = AXCL_POOL_SetConfig(&PoolFloorPlan);

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_SetConfig fail!ret:0x%x\n", ret);
        goto error0;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_SetConfig success!\n");
    }

    /* step 4:初始化公共缓存池 */
    ret = AXCL_POOL_Init();
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_Init fail!!ret:0x%x\n", ret);
        goto error0;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Init success!\n");
    }

    /*
    * step 5:从公共缓存池获取Block
    * 第一个参数为指定的PoolID, 如果设置为 AX_INVALID_POOLID,则表示从任意公共缓存池获取，也就是SetConfig->Init创建的公共缓存池。
    */

    BlkSize = 3 * 1024 * 1024;//期望获取的buffer大小
    BlkId = AXCL_POOL_GetBlock(AX_INVALID_POOLID, BlkSize, NULL);

    if (BlkId == AX_INVALID_BLOCKID) {
        SAMPLE_LOG_E("AXCL_POOL_GetBlock fail!\n");
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_GetBlock success!BlkId:0x%X\n", BlkId);
    }

    /* step 6:通过Block句柄获取其PoolId */
    PoolId = AXCL_POOL_Handle2PoolId(BlkId);

    if (PoolId == AX_INVALID_POOLID) {
        SAMPLE_LOG_E("AXCL_POOL_Handle2PoolId fail!\n");
        AXCL_POOL_ReleaseBlock(BlkId);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Handle2PoolId success!(Blockid:0x%X --> PoolId=%d)\n", BlkId, PoolId);
    }

    /* step 7:通过Block句柄获取其物理地址 */
    PhysAddr = AXCL_POOL_Handle2PhysAddr(BlkId);

    if (!PhysAddr) {
        SAMPLE_LOG_E("AXCL_POOL_Handle2PhysAddr fail!\n");
        AXCL_POOL_ReleaseBlock(BlkId);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Handle2PhysAddr success!(Blockid:0x%X --> PhyAddr=0x%llx)\n", BlkId, PhysAddr);
    }

    /* step 8:通过Block句柄获取其metadata物理地址 */
    MetaPhysAddr = AXCL_POOL_Handle2MetaPhysAddr(BlkId);

    if (!MetaPhysAddr) {
        SAMPLE_LOG_E("AXCL_POOL_Handle2MetaPhysAddr fail!\n");
        AXCL_POOL_ReleaseBlock(BlkId);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Handle2MetaPhysAddr success!(Blockid:0x%X --> MetaPhyAddr=0x%llx)\n", BlkId, MetaPhysAddr);
    }

    /* step 9:使用完block后，释放Block句柄 */
    ret = AXCL_POOL_ReleaseBlock(BlkId);

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_ReleaseBlock fail!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_ReleaseBlock success!Blockid=0x%x\n", BlkId);
    }

    /* step 10:清除缓存池 */
    ret = AXCL_POOL_Exit();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_Exit fail!!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Exit success!\n");
    }

    /* step 11:SYS模块去初始化 */
    ret = AXCL_SYS_Deinit();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Deinit fail!!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_SYS_Deinit success!\n");
    }

    SAMPLE_LOG_I("sys_commom_pool end success!\n");
    return 0;

error1:
    AXCL_POOL_Exit();

error0:
    AXCL_SYS_Deinit();

    SAMPLE_LOG_I("sys_commom_pool end fail!\n");
    return -1;
}

static int sample_sys_private_pool() {
    AX_S32 ret = 0;
    AX_U64 BlkSize;
    AX_BLK BlkId;
    AX_POOL PoolId;
    AX_U64 PhysAddr, MetaPhysAddr;
    AX_POOL_CONFIG_T PoolConfig = { 0 };
    AX_POOL UserPoolId0 , UserPoolId1;

    SAMPLE_LOG_I("sys_private_pool begin\n");

    /* step 1:SYS模块初始化 */
    ret = AXCL_SYS_Init();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Init fail!!ret:0x%x\n", ret);
        return -1;
    } else {
        SAMPLE_LOG_I("AXCL_SYS_Init success!\n");
    }

    /* step 2:去初始化缓存池 */
    ret = AXCL_POOL_Exit();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_Exit fail!!ret:0x%x\n", ret);
        goto error0;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Exit success!\n");
    }

    /* step 3:用户通过AX_POOL_CreatePool依次创建两个专用缓存池 */

    /* 创建user_pool_0 :blocksize=1*1024*1024,metasize=0x1000,block count =2 ,noncache类型 */
    memset(&PoolConfig, 0, sizeof(AX_POOL_CONFIG_T));
    PoolConfig.MetaSize = 0x1000;
    PoolConfig.BlkSize = 1 * 1024 * 1024;
    PoolConfig.BlkCnt = 2;
    PoolConfig.CacheMode = POOL_CACHE_MODE_NONCACHE;
    memset(PoolConfig.PartitionName,0, sizeof(PoolConfig.PartitionName));
    strcpy((char *)PoolConfig.PartitionName, "anonymous");

    UserPoolId0 = AXCL_POOL_CreatePool(&PoolConfig);

    if (AX_INVALID_POOLID == UserPoolId0) {
        SAMPLE_LOG_E("AXCL_POOL_CreatePool error!!!\n");
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_CreatePool[%d] success\n", UserPoolId0);
    }

    /* 创建user_pool_1:blocksize=2*1024*1024,metasize=0x1000,block count =3 ,noncache类型 */
    memset(&PoolConfig, 0, sizeof(AX_POOL_CONFIG_T));
    PoolConfig.MetaSize = 0x1000;
    PoolConfig.BlkSize = 2 * 1024 * 1024;
    PoolConfig.BlkCnt = 3;
    PoolConfig.CacheMode = POOL_CACHE_MODE_NONCACHE;
    memset(PoolConfig.PartitionName,0, sizeof(PoolConfig.PartitionName));
    strcpy((char *)PoolConfig.PartitionName, "anonymous");

    UserPoolId1 = AXCL_POOL_CreatePool(&PoolConfig);

    if (AX_INVALID_POOLID == UserPoolId1) {
        SAMPLE_LOG_E("AXCL_POOL_CreatePool error!!!\n");
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_CreatePool[%d] success\n", UserPoolId1);
    }

    /* step 4:从专用缓存池获取Block */
    BlkSize = 2 * 1024 * 1024;//期望获取的buffer大小
    BlkId = AXCL_POOL_GetBlock(UserPoolId1, BlkSize, NULL);

    if (BlkId == AX_INVALID_BLOCKID) {
        SAMPLE_LOG_E("AXCL_POOL_GetBlock fail!\n");
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_GetBlock success!BlkId:0x%X\n", BlkId);
    }

    /* step 5:通过Block句柄获取其PoolId */
    PoolId = AXCL_POOL_Handle2PoolId(BlkId);

    if (PoolId == AX_INVALID_POOLID) {
        SAMPLE_LOG_E("AXCL_POOL_Handle2PoolId fail!\n");
        AXCL_POOL_ReleaseBlock(BlkId);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Handle2PoolId success!(Blockid:0x%X --> PoolId=%d)\n", BlkId, PoolId);
    }

    /* step 6:通过Block句柄获取其物理地址 */
    PhysAddr = AXCL_POOL_Handle2PhysAddr(BlkId);

    if (!PhysAddr) {
        SAMPLE_LOG_E("AXCL_POOL_Handle2PhysAddr fail!\n");
        AXCL_POOL_ReleaseBlock(BlkId);
        goto error1;
    } else {
        SAMPLE_LOG_I("AX_POOL_Handle2PhysAddr success!(Blockid:0x%X --> PhyAddr=0x%llx)\n", BlkId, PhysAddr);
    }

    /* step 7:通过Block句柄获取其metadata物理地址 */
    MetaPhysAddr = AXCL_POOL_Handle2MetaPhysAddr(BlkId);

    if (!MetaPhysAddr) {
        SAMPLE_LOG_E("AXCL_POOL_Handle2MetaPhysAddr fail!\n");
        AXCL_POOL_ReleaseBlock(BlkId);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_Handle2MetaPhysAddr success!(Blockid:0x%X --> MetaPhyAddr=0x%llx)\n", BlkId, MetaPhysAddr);
    }

    /* step 8:使用完block后，释放Block句柄 */
    ret = AXCL_POOL_ReleaseBlock(BlkId);

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_ReleaseBlock fail!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_ReleaseBlock success!Blockid=0x%x\n", BlkId);
    }

    /* step 9:销毁AX_POOL_CreatePool创建的缓存池 */
    ret = AXCL_POOL_DestroyPool(UserPoolId1);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_DestroyPool fail!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_DestroyPool[%d] success!\n", UserPoolId1);
    }

    ret = AXCL_POOL_DestroyPool(UserPoolId0);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_POOL_DestroyPool fail!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_POOL_DestroyPool[%d] success!\n", UserPoolId0);
    }

    /* step 10:SYS模块去初始化 */
    ret = AXCL_SYS_Deinit();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Deinit fail!!ret:0x%x\n", ret);
        goto error1;
    } else {
        SAMPLE_LOG_I("AXCL_SYS_Deinit success!\n");
    }

    SAMPLE_LOG_I("sys_private_pool end success!\n");
    return 0;

error1:
    AXCL_POOL_Exit();

error0:
    AXCL_SYS_Deinit();

    SAMPLE_LOG_I("sys_private_pool end fail!\n");
    return -1;
}

static int sample_sys_link() {
    AX_S32 ret = 0;
    AX_MOD_INFO_T srcMod;
    AX_MOD_INFO_T dstMod;
    AX_MOD_INFO_T dstMod2;
    AX_MOD_INFO_T tempMod;
    AX_LINK_DEST_T linkDest;

    SAMPLE_LOG_I("sample_sys_link begin\n");

    memset(&srcMod, 0, sizeof(srcMod));
    memset(&dstMod, 0, sizeof(dstMod));
    memset(&dstMod2, 0, sizeof(dstMod2));
    memset(&tempMod, 0, sizeof(tempMod));
    memset(&linkDest, 0, sizeof(linkDest));

    /* step 1:SYS模块初始化 */
    ret = AXCL_SYS_Init();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Init fail!!ret:0x%x\n", ret);
        return -1;
    } else {
        SAMPLE_LOG_I("AXCL_SYS_Init success!\n");
    }

    /* one src --> two dst */
    srcMod.enModId = AX_ID_IVPS;
    srcMod.s32GrpId = 1;
    srcMod.s32ChnId = 1;

    dstMod.enModId = AX_ID_VENC;
    dstMod.s32GrpId = 0;
    dstMod.s32ChnId = 1;

    dstMod2.enModId = AX_ID_VENC;
    dstMod2.s32GrpId = 0;
    dstMod2.s32ChnId = 2;

    /* step 2:建立绑定关系 */
    ret = AXCL_SYS_Link(&srcMod, &dstMod);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Link failed,ret:0x%x\n", ret);
        goto error;
    }

    ret = AXCL_SYS_Link(&srcMod, &dstMod2);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Link failed,ret:0x%x\n", ret);
        goto error;
    }

    /* step 3:根据dest查询src */
    ret = AXCL_SYS_GetLinkByDest(&dstMod, &tempMod);
    if ((AXCL_SUCC != ret) || (tempMod.enModId != srcMod.enModId) ||
        (tempMod.s32GrpId != srcMod.s32GrpId) ||
        (tempMod.s32ChnId != srcMod.s32ChnId)) {
        SAMPLE_LOG_E("AXCL_SYS_GetLinkByDest failed,ret:0x%x\n", ret);
        goto error;
    }

    /* step 4:根据src查询dest */
    ret = AXCL_SYS_GetLinkBySrc(&srcMod, &linkDest);
    if ((AXCL_SUCC != ret) || (linkDest.u32DestNum != 2) ||
        (linkDest.astDestMod[0].enModId != AX_ID_VENC) ||
        (linkDest.astDestMod[1].enModId != AX_ID_VENC)) {
        SAMPLE_LOG_E("AXCL_SYS_GetLinkBySrc failed,ret:0x%x\n", ret);
        goto error;
    }

    /* step 5:解除绑定关系 */
    ret = AXCL_SYS_UnLink(&srcMod, &dstMod);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_UnLink failed,ret:0x%x\n", ret);
        goto error;
    }

    ret = AXCL_SYS_UnLink(&srcMod, &dstMod2);
    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_UnLink failed,ret:0x%x\n", ret);
        goto error;
    }

    /* step 6:SYS模块去初始化 */
    ret = AXCL_SYS_Deinit();

    if (AXCL_SUCC != ret) {
        SAMPLE_LOG_E("AXCL_SYS_Deinit fail!!ret:0x%x\n", ret);
        goto error;
    } else {
        SAMPLE_LOG_I("AXCL_SYS_Deinit success!\n");
    }

    SAMPLE_LOG_I("sample_sys_link end success!\n");
    return 0;

error:
    AXCL_SYS_Deinit();

    SAMPLE_LOG_I("sample_sys_link end failed!\n");
    return -1;
}