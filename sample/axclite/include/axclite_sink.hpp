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

#include "axclite_frame.hpp"

namespace axclite {

class sinker {
public:
    virtual ~sinker() = default;
    virtual int32_t recv_frame(const axclite_frame& frame) = 0;
};

}  // namespace axclite