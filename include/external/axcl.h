/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __AXCL_H__
#define __AXCL_H__

#include "axcl_base.h"
#include "axcl_native.h"
#include "axcl_rt.h"

#ifdef __cplusplus
extern "C" {
#endif

axclError axclInit(const char *config);
axclError axclFinalize();

/**
 * @brief set axcl log level
 * @param lv log level
 *          0: trace
 *          1: debug
 *          2: info
 *          3: warning
 *          4: error
 *          5: critical
 *          6: off
 * @return 0 success, otherwise failure
*/
axclError axclSetLogLevel(int32_t lv);

#ifdef __cplusplus
}
#endif

#endif /* __AXCL_H__ */