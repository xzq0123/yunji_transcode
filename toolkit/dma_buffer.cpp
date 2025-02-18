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
#define AX_IOC_PCIe_BASE 'H'
#define AX_IOC_PCIE_DMA_GET_PHY_BASE    _IOW(AX_IOC_PCIe_BASE, 3, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMORY        _IOW(AX_IOC_PCIe_BASE, 4, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMCACHED     _IOW(AX_IOC_PCIe_BASE, 5, unsigned int)
#define AX_IOC_PCIe_FLUSH_CACHED        _IOW(AX_IOC_PCIe_BASE, 6, struct ax_mem_info)
#define AX_IOC_PCIe_INVALID_CACHED      _IOW(AX_IOC_PCIe_BASE, 7, struct ax_mem_info)
#define AX_IOC_PCIe_SCATTERLIST_ALLOC   _IOW(AX_IOC_PCIe_BASE, 8, struct ax_scatterlist)

struct ax_mem_info {
    uint64_t phy;
    uint64_t vir;
    uint64_t size;
};

struct ax_sg_list {
    unsigned long phyaddr;
    unsigned long virtaddr;
    unsigned int size;
};

#define MAX_SG_LIST_ENTRY 64
struct ax_scatterlist {
    int num;
    int size;
    struct ax_sg_list sg_list[MAX_SG_LIST_ENTRY];
};

static void print_scatterlist(const struct ax_scatterlist &sg) {
    LOG_MM_C(TAG, "pid: {} scatter list: size {:#x} num {}", getpid(), sg.size, sg.num);
    for (int i = 0; i < sg.num; ++i) {
        LOG_MM_C(TAG, "  [{}]: phy {:#x}, size {:#x}", i, sg.sg_list[i].phyaddr, sg.sg_list[i].size);
    }
}

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

bool dma_buffer::alloc(size_t size, bool cached /* = false */, bool scattered /* = false */) {
    if (m_fd > 0) {
        LOG_MM_E(TAG, "{} is opened", AX_MM_DEV);
        return false;
    }

    int fd = ::open(AX_MM_DEV, O_RDWR);
    if (fd < 0) {
        LOG_MM_E(TAG, "open {} fail", AX_MM_DEV);
        return false;
    }

#ifndef AXCL_CMA_CACHED
    cached = false;
#endif

    int ret;

    if (scattered) {
        /* drv use kmalloc to alloc sg memory which means always cached */
        cached = true;

        struct ax_scatterlist sg = {};
        sg.size = size;
        if (ret = ::ioctl(fd, AX_IOC_PCIe_SCATTERLIST_ALLOC, &sg); ret < 0) {
            LOG_MM_E(TAG, "allocate sg buffers (size: {}) fail, {}", size, ::strerror(errno));
            ::close(fd);
            return false;
        }

        if (sg.num <= 0) {
            LOG_MM_E(TAG, "0 sg buffer is allocated");
            ::close(fd);
            return false;
        }

        size_t total_size = 0;
        for (int i = 0; i < sg.num; ++i) {
            if (0 == sg.sg_list[i].phyaddr || 0 == sg.sg_list[i].size) {
                LOG_MM_E(TAG, "allocated invalid sglist[{}] phy {:#x} size: {:#x}", i, sg.sg_list[i].phyaddr, sg.sg_list[i].size);
                print_scatterlist(sg);
                ::close(fd);
                return false;
            }

            total_size += sg.sg_list[i].size;
        }

        if (total_size < size) {
            LOG_MM_E(TAG, "expected {}, but allocated {}", total_size, size);
            print_scatterlist(sg);
            ::close(fd);
            return false;
        }

        m_mem.blk_cnt = sg.num;

        void *vir = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (vir == MAP_FAILED) {
            LOG_MM_E(TAG, "mmap sg buffers fail, {}", ::strerror(errno));
            print_scatterlist(sg);
            ::close(fd);
            return false;
        }

        /* make sure pages have been allocated, and flush is mandatory */
        ::memset(vir, 0xCC, size);

        m_fd = fd;
        if (!flush(sg.sg_list[0].phyaddr, vir, size)) {
            print_scatterlist(sg);
            ::close(fd);
            m_fd = -1;
            return false;
        }

        uint8_t *base = reinterpret_cast<uint8_t *>(vir);
        for (int i = 0; i < sg.num; ++i) {
            struct dma_mem_block &blk = m_mem.blks[i];
            blk.phy = sg.sg_list[i].phyaddr;
            blk.vir = reinterpret_cast<void *>(base);
            blk.size = sg.sg_list[i].size;
            base += blk.size;
        }
    } else {
        uint64_t dma_buffer_size = size;
        if (cached) {
            if (ret = ::ioctl(fd, AX_IOC_PCIe_ALLOC_MEMCACHED, &dma_buffer_size); ret < 0) {
                LOG_MM_E(TAG, "allocate pcie cached dma buffer (size: {}) fail, {}", dma_buffer_size, ::strerror(errno));
                ::close(fd);
                return false;
            }
        } else {
            if (ret = ::ioctl(fd, AX_IOC_PCIe_ALLOC_MEMORY, &dma_buffer_size); ret < 0) {
                LOG_MM_E(TAG, "allocate pcie dma buffer (size: {}) fail, {}", dma_buffer_size, ::strerror(errno));
                ::close(fd);
                return false;
            }
        }

        uint64_t phy = 0;
        if (ret = ::ioctl(fd, AX_IOC_PCIE_DMA_GET_PHY_BASE, &phy); ret < 0) {
            LOG_MM_E(TAG, "get pcie dma phy address fail, {}", ::strerror(errno));
            ::close(fd);
            return false;
        }

        if (0 == phy) {
            LOG_MM_E(TAG, "allocated dma phy address is 0");
            ::close(fd);
            return false;
        }

        void *vir = ::mmap(NULL, dma_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (vir == MAP_FAILED) {
            LOG_MM_E(TAG, "mmap pcie dma phy address fail, {}", ::strerror(errno));
            ::close(fd);
            return false;
        }

        if (cached) {
            /* make sure pages have been allocated, and flush is mandatory */
            ::memset(vir, 0xCC, size);

            m_fd = fd;
            if (!flush(phy, vir, size)) {
                ::close(fd);
                m_fd = -1;
                return false;
            }
        }

        m_mem.blk_cnt = 1;
        m_mem.blks[0].phy = phy;
        m_mem.blks[0].vir = vir;
        m_mem.blks[0].size = size;
    }

    m_fd = fd;
    m_mem.scattered = scattered;
    m_mem.cached = cached;
    m_mem.total_size = size;
    if (cached) {
        m_mem.ops.flush = std::bind(&dma_buffer::flush, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        m_mem.ops.invalidate =
            std::bind(&dma_buffer::invalidate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    }

    return true;
}

void dma_buffer::free() {
    if (m_fd > 0) {
        if (m_mem.blks[0].vir) {
            ::munmap(m_mem.blks[0].vir, m_mem.total_size);
        }

        ::close(m_fd);
        m_fd = -1;
    }

    m_mem.blk_cnt = 0;
    m_mem.total_size = 0;
}

bool dma_buffer::flush(uint64_t phy, void *vir, uint32_t size) {
    struct ax_mem_info mem = {phy, reinterpret_cast<uint64_t>(vir), size};
    if (int ret = ::ioctl(m_fd, AX_IOC_PCIe_FLUSH_CACHED, &mem); ret < 0) {
        LOG_MM_E(TAG, "flush dma buffer (phy {} vir {} size {}) fail, {}", phy, vir, size, ::strerror(errno));
        return false;
    }

    return true;
}

bool dma_buffer::invalidate(uint64_t phy, void *vir, uint32_t size) {
    struct ax_mem_info mem = {phy, reinterpret_cast<uint64_t>(vir), size};
    if (int ret = ::ioctl(m_fd, AX_IOC_PCIe_INVALID_CACHED, &mem); ret < 0) {
        LOG_MM_E(TAG, "invalidate dma buffer (phy {} vir {} size {}) fail, {}", phy, vir, size, ::strerror(errno));
        return false;
    }

    return true;
}

unsigned long dma_buffer::get_cma_free_size() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        LOG_MM_E(TAG, "open /proc/meminfo fail, %s", strerror(errno));
        return 0;
    }

    unsigned long cma_free = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "CmaFree: %lu kB", &cma_free) == 1) {
            break;
        }
    }

    fclose(fp);
    return cma_free;
}