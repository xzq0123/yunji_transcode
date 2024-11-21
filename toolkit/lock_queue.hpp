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

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>

namespace axcl {

template <typename T>
class lock_queue {
public:
    lock_queue() = default;

    void set_capacity(uint32_t capacity) {
        m_capacity = capacity;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lck(m_mtx);
        return m_q.size();
    }

    bool full() const {
        std::lock_guard<std::mutex> lck(m_mtx);
        return (m_q.size() >= m_capacity);
    }

    void wakeup() {
        {
            std::lock_guard<std::mutex> lck(m_mtx);
            m_wakeup = true;
        }

        m_cv.notify_one();
    }

    bool push(const T& m) {
        {
            std::lock_guard<std::mutex> lck(m_mtx);
            if (m_q.size() >= m_capacity) {
                return false;
            }

            m_q.push(m);
        }

        m_cv.notify_one();
        return true;
    }

    std::optional<T> replace_push(const T& m) {
        std::optional<T> old;
        {
            std::lock_guard<std::mutex> lck(m_mtx);

            if (m_q.size() >= m_capacity) {
                old = std::move(m_q.front());
                m_q.pop();
            }

            m_q.push(m);
        }

        m_cv.notify_one();
        return old;
    }

    std::optional<T> pop(int32_t timeout_ms = -1) {
        std::unique_lock<std::mutex> lck(m_mtx);

        if (0 == m_q.size()) {
            if (0 == timeout_ms) {
                return std::nullopt;
            } else {
                if (timeout_ms < 0) {
                    m_cv.wait(lck, [this]() -> bool { return !m_q.empty() || m_wakeup; });
                } else {
                    if (!m_cv.wait_for(lck, std::chrono::milliseconds(timeout_ms), [this]() -> bool { return !m_q.empty() || m_wakeup; })) {
                        return std::nullopt;
                    }
                }

                m_wakeup = false;
            }
        }

        if (!m_q.empty()) {
            std::optional<T> m = std::move(m_q.front());
            m_q.pop();
            return m;
        }

        return std::nullopt;
    }

private:
    lock_queue(const lock_queue&) = delete;
    lock_queue(lock_queue&&) = delete;
    lock_queue& operator=(const lock_queue&) = delete;
    lock_queue& operator=(lock_queue&&) = delete;

private:
    uint32_t m_capacity = (uint32_t)-1;
    std::queue<T> m_q;
    mutable std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_wakeup = false;
};

}  // namespace axcl
