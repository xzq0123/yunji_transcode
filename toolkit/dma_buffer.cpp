/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "dma_buffer.hpp"
#include <error.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include "ax_base_type.h"
#include "log/logger.hpp"

#define AX_MM_DEV "/dev/ax_mmb_dev"
#define TAG "pcie"

// ioctl cmd
#define PCIE_BASE 'H'
#define PCIE_DMA_GET_PHY_BASE _IOW(PCIE_BASE, 3, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMORY _IOW(PCIE_BASE, 4, unsigned int)

dma_buffer::dma_buffer() : m_fd(-1) {
}

dma_buffer::~dma_buffer() {
    free();
}

dma_buffer::dma_buffer(dma_buffer &&rhs) noexcept {
    m_fd = std::exchange(rhs.m_fd, -1);
    m_mem = std::exchange(rhs.m_mem, dma_mem{});
}

dma_buffer &dma_buffer::operator=(dma_buffer &&rhs) noexcept {
    m_fd = std::exchange(rhs.m_fd, -1);
    m_mem = std::exchange(rhs.m_mem, dma_mem{});

    return *this;
}

bool dma_buffer::alloc(size_t size) {
    if (m_fd > 0) {
        LOG_MM_E(TAG, "{} is opened", AX_MM_DEV);
        return false;
    }

    int fd = ::open(AX_MM_DEV, O_RDWR);
    if (fd < 0) {
        LOG_MM_E(TAG, "open {} fail", AX_MM_DEV);
        return false;
    }

    int ret;
    uint64_t dma_buffer_size = size;
    if (ret = ::ioctl(fd, AX_IOC_PCIe_ALLOC_MEMORY, &dma_buffer_size); ret < 0) {
        LOG_MM_E(TAG, "allocate pcie dma buffer (size: {}) fail, {}", dma_buffer_size, ::strerror(errno));
        ::close(fd);
        return false;
    }

    uint64_t phy;
    if (ret = ::ioctl(fd, PCIE_DMA_GET_PHY_BASE, &phy); ret < 0) {
        LOG_MM_E(TAG, "get pcie dma phy address fail, {}", ::strerror(errno));
        ::close(fd);
        return false;
    }

    void *vir;
    if (vir = ::mmap(NULL, dma_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); !vir) {
        LOG_MM_E(TAG, "mmap pcie dma phy address fail, {}", ::strerror(errno));
        ::close(fd);
        return false;
    }

    m_fd = fd;
    m_mem.phy = phy;
    m_mem.vir = vir;
    m_mem.size = dma_buffer_size;
    return true;
}

void dma_buffer::free() {
    if (m_fd > 0) {
        if (m_mem.vir) {
            ::munmap(m_mem.vir, m_mem.size);
            m_mem.vir = nullptr;
        }

        ::close(m_fd);
        m_fd = -1;
    }

    m_mem.phy = 0;
    m_mem.size = 0;
}
