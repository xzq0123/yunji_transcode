/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#pragma once
#include <exception>
#include "AXFrame.hpp"
#include "IObserver.h"
#include "Vo.hpp"

#include "axcl_rt_memory.h"

/**
 * @brief
 *
 */
class CVoObserver final : public IObserver {
public:
    CVoObserver(CVo* pSink, AX_U32 nChn) noexcept : m_pSink(pSink), m_nChn(nChn){};
    virtual ~CVoObserver(AX_VOID) = default;

    AX_BOOL OnRecvData(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, AX_VOID* pData) override {
        if (nullptr == pData) {
            return AX_FALSE;
        }

        AX_U64 u64PhysAddr;
        AX_U32 u32FrameSize = ALIGN_UP(m_pSink->GetRectWidth(), 16) * m_pSink->GetRectHeight() * 3 / 2;

        AX_BLK BlkId = AX_POOL_GetBlock(m_pSink->GetPriPoolID(nGrp), u32FrameSize, NULL);
        if (AX_INVALID_BLOCKID != BlkId) {
            u64PhysAddr = AX_POOL_Handle2PhysAddr(BlkId);
            if (!u64PhysAddr) {
                LOG_M_E("DISP", "[%d] AX_POOL_Handle2PhysAddr failed,BlkId=0x%x", nGrp, BlkId);
                return AX_FALSE;
            }

            CAXFrame* pAxFrame = (CAXFrame*)pData;
            axclrtMemcpy((AX_VOID *)u64PhysAddr, (AX_VOID *)pAxFrame->stFrame.stVFrame.stVFrame.u64PhyAddr[0],
                         pAxFrame->stFrame.stVFrame.stVFrame.u32FrameSize, AXCL_MEMCPY_DEVICE_TO_HOST_PHY);

            AX_VIDEO_FRAME_T voFrame = pAxFrame->stFrame.stVFrame.stVFrame;

            memset(voFrame.u32BlkId, 0, sizeof(voFrame.u32BlkId));
            memset(voFrame.u64VirAddr, 0, sizeof(voFrame.u64VirAddr));
            memset(voFrame.u64PhyAddr, 0, sizeof(voFrame.u64PhyAddr));

            voFrame.u32BlkId[0] = BlkId;
            voFrame.u64PhyAddr[0] = u64PhysAddr;
            voFrame.u32FrameFlag |= AX_FRM_FLG_FR_CTRL;

            CAXFrame fAxFrame;
            fAxFrame.stFrame.stVFrame.stVFrame = voFrame;

            m_pSink->SendFrame(m_pSink->GetVideoChn(nGrp), fAxFrame, -1);

            AX_POOL_ReleaseBlock(BlkId);
            return AX_TRUE;
        } else {
            LOG_M_E("DISP", "[%d] AX_POOL_GetBlock failed.", nGrp);
        }

        return AX_FALSE;
    }

    AX_BOOL OnRegisterObserver(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, OBS_TRANS_ATTR_PTR pParams) override {
        return AX_TRUE;
    }

private:
    CVo* m_pSink;
    AX_U32 m_nChn;
};
