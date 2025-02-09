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
#include "ax_base_type.h"
#include "make_unique.hpp"
#include <cstring>

typedef enum {
    E_OBS_TARGET_TYPE_VENC = 0,
    E_OBS_TARGET_TYPE_JENC,
    E_OBS_TARGET_TYPE_MJENC,
    E_OBS_TARGET_TYPE_IVPS,
    E_OBS_TARGET_TYPE_VDEC,
    E_OBS_TARGET_TYPE_VIN,
    E_OBS_TARGET_TYPE_COLLECT,
    E_OBS_TARGET_TYPE_MPEG4,
    E_OBS_TARGET_TYPE_DETECT,
    E_OBS_TARGET_TYPE_CAPTURE,
    E_OBS_TARGET_TYPE_REGION,
    E_OBS_TARGET_TYPE_AICARD_TRANSFER,
    E_OBS_TARGET_TYPE_DSP,
    E_OBS_TARGET_TYPE_AIRAW,
    E_OBS_TARGET_TYPE_AENC,
    E_OBS_TARGET_TYPE_APLAY,
    E_OBS_TARGET_TYPE_EVENT,
    E_OBS_TARGET_TYPE_AVS,
    E_OBS_TARGET_TYPE_MAX
} OBS_TARGET_TYPE_E;

typedef enum {
    E_OBS_CHANGE_DATA_TYPE_FRAMERATE = 0,
    E_OBS_CHANGE_DATA_TYPE_RESOLUTION,
    E_OBS_CHANGE_DATA_TYPE_PAYLOADTYPE,
    E_OBS_CHANGE_DATA_TYPE_BITRATE,
    E_OBS_CHANGE_DATA_TYPE_ROTATION,
    E_OBS_CHANGE_DATA_TYPE_SET_MAIN_RESOLUTION,
    E_OBS_CHANGE_DATA_TYPE_MAX
} OBS_CHANGE_DATA_TYPE_E;

typedef struct _OBS_TRANS_ATTR_T {
    AX_S32 nGroup;
    AX_S32 nChannel;
    AX_U32 nWidth;
    AX_U32 nHeight;
    AX_U32 nCfgWidth;
    AX_U32 nCfgHeight;
    AX_F32 fFramerate;
    AX_F32 fSrcFramerate;
    AX_BOOL bLink;
    AX_U32 nPayloadType;
    AX_U32 nBitRate;
    AX_S8 nSnsSrc;
    AX_U8 arrOutFBC[2];
    _OBS_TRANS_ATTR_T() {
        memset(this, 0, sizeof(*this));
    }
} OBS_TRANS_ATTR_T, *OBS_TRANS_ATTR_PTR;

typedef struct _OBS_TRANS_RESOLUTION_T {
    AX_U32 nWidth;
    AX_U32 nHeight;
    _OBS_TRANS_RESOLUTION_T() {
        memset(this, 0, sizeof(*this));
    }
} OBS_TRANS_RESOLUTION_T, *OBS_TRANS_RESOLUTION_PTR;

typedef struct _OBS_TRANS_ROTATE_INFO_T {
    AX_U8   nRotation;  //0:0; 1:90; 2:180; 3:270
    AX_U32  nWidth;
    AX_U32  nHeight;
    AX_BOOL bInFBC;
    _OBS_TRANS_ROTATE_INFO_T() {
        memset(this, 0, sizeof(*this));
    }
} OBS_TRANS_ROTATE_INFO_T, *OBS_TRANS_ROTATE_INFO_PTR;

class IObserver {
public:
    virtual ~IObserver(AX_VOID) = default;

public:
    virtual AX_BOOL OnRecvData(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, AX_VOID* pData) = 0;
    virtual AX_BOOL OnRegisterObserver(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, OBS_TRANS_ATTR_PTR pParams) = 0;
    virtual AX_BOOL OnDataChange(OBS_TARGET_TYPE_E eTarget, AX_U32 nGrp, AX_U32 nChn, OBS_CHANGE_DATA_TYPE_E eDataType, AX_VOID* pData) {
        return AX_TRUE;
    }
};

/**
 * @brief Create IObserver pointer and manage by std::unique_ptr (default: c++11)
 * @note
 *      1. Create unique_ptr by new ctor, if c++14 and above, replace with make_unique
 *      2. Make sure T ctor and dtor won't throw exception
 *      3. Simply to destory observer by default deleter, user defined deleter is not support
 * @usage
 *      IObserverUniquePtr p = CObserverMaker::CreateObserver<ObserverImp>(arg);
 */
using IObserverUniquePtr = std::unique_ptr<IObserver>;
class CObserverMaker {
public:
    template <typename T, typename... Args>
    static IObserverUniquePtr CreateObserver(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    };

private:
    CObserverMaker(AX_VOID) noexcept = default;
    ~CObserverMaker(AX_VOID) = default;
};
