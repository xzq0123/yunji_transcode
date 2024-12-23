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
#include "detector.hpp"
#include "IObserver.h"

/**
 * @brief
 *
 */
class CDetectObserver final : public IObserver {
public:
    CDetectObserver(CDetector* pSink, AX_U32 nChn) noexcept : m_pSink(pSink){};
    virtual ~CDetectObserver(AX_VOID) = default;

    AX_BOOL OnRecvData(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, AX_VOID* pData) override {
        if (nullptr == pData) {
            return AX_FALSE;
        }

        if (E_OBS_TARGET_TYPE_VDEC == eTarget) {
            CAXFrame* pFrame = (CAXFrame*)pData;
            return m_pSink->SendFrame(*pFrame);
        } else if (E_OBS_TARGET_TYPE_IVPS == eTarget) {
            CAXFrame* pFrame = (CAXFrame*)pData;
            return m_pSink->SendFrame(*pFrame);
        }

        return AX_FALSE;
    }

    AX_BOOL OnRegisterObserver(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, OBS_TRANS_ATTR_PTR pParams) override {
        return AX_TRUE;
    }

private:
    CDetector* m_pSink;
};