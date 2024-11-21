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

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace axcl {

class event final {
public:
    event() = default;

    bool wait(int32_t timeout_ms = -1);
    void set();
    void reset();

private:
    std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_signal = false;
};

}  // namespace axcl