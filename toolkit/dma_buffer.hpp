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

#include <cstddef>
#include <cstdint>
#include <functional>

struct dma_mem {
    uint64_t phy = 0;
    void *vir = nullptr;
    uint32_t size = 0;
    bool cached = false;
    std::function<bool(uint64_t, void *, uint32_t)> flush = nullptr;
    std::function<bool(uint64_t, void *, uint32_t)> invalidate = nullptr;
};

class dma_buffer {
public:
    dma_buffer();
    ~dma_buffer();

    dma_buffer(dma_buffer &&) noexcept;
    dma_buffer &operator=(dma_buffer &&) noexcept;
    dma_buffer(const dma_buffer &) = delete;
    dma_buffer &operator=(const dma_buffer &) = delete;

    [[nodiscard]] bool alloc(size_t size, bool cached = false);
    void free();

    [[nodiscard]] bool flush(uint64_t phy, void *vir, uint32_t size);
    [[nodiscard]] bool invalidate(uint64_t phy, void *vir, uint32_t size);

    const struct dma_mem &get() const {
        return m_mem;
    }

private:
    int32_t m_fd;
    struct dma_mem m_mem;
};
