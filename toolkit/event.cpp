/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "event.hpp"
#include <chrono>

bool axcl::event::wait(int32_t timeout_ms /* = -1 */) {
    std::unique_lock<std::mutex> lck(m_mtx);
    if (m_signal) {
        return true;
    }

    if (0 == timeout_ms) {
        return false;
    } else if (timeout_ms < 0) {
        m_cv.wait(lck, [this]() -> bool { return m_signal; });
        return true;
    } else {
        return m_cv.wait_for(lck, std::chrono::milliseconds(timeout_ms), [this]() -> bool { return m_signal; });
    }
}

void axcl::event::set() {
    std::lock_guard<std::mutex> lck(m_mtx);
    m_signal = true;

    m_cv.notify_one();
}

void axcl::event::reset() {
    std::lock_guard<std::mutex> lck(m_mtx);
    m_signal = false;
}