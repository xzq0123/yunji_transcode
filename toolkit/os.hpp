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

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>

#define gettid() static_cast<uint32_t>(syscall(SYS_gettid))
