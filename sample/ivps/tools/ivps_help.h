/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef _IVPS_HELP_H_
#define _IVPS_HELP_H_

#include "ivps_util.h"
enum
{
    SAMPLE_HELP_MIN = -1,
    SAMPLE_HELP_USERMODE = 0,
    SAMPLE_HELP_LINKMODE = 1,
    SAMPLE_HELP_REGION = 2,
    SAMPLE_HELP_MAX
};

#define SAMPLE_NAME "opt/bin/axcl_sample_ivps"

AX_VOID ShowUsage(AX_S32 nHelpIdx);

#endif /* _IVPS_HELP_H_ */
