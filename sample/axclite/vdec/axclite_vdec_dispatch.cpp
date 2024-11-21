/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "axclite_vdec_dispatch.hpp"
#include <algorithm>
#include <chrono>
#include "axclite_helper.hpp"
#include "axclite_sink.hpp"
#include "log/logger.hpp"

#define TAG "axclite-vdec-dispatch"

namespace axclite {

void vdec_dispatch::dispatch_thread(int32_t device) {
    LOG_MM_D(TAG, "+++");

    context_guard context_holder(device);

    axclError ret;
    AX_VDEC_GRP_SET_INFO_T grp_info;
    constexpr AX_S32 TIMEOUT = -1;
    while (1) {
        wait();

        if (!m_thread.running()) {
            break;
        }

        LOG_MM_D(TAG, "AXCL_VDEC_SelectGrp() +++");
        ret = AXCL_VDEC_SelectGrp(&grp_info, 1000);
        LOG_MM_D(TAG, "AXCL_VDEC_SelectGrp() ---");
        if (AXCL_SUCC != ret) {
            if (AX_ERR_VDEC_FLOW_END == ret) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                if (AX_ERR_VDEC_TIMED_OUT != ret) {
                    LOG_MM_E(TAG, "AXCL_VDEC_SelectGrp() fail, ret = {:#x}", static_cast<uint32_t>(ret));
                } else {
                    LOG_MM_I(TAG, "AXCL_VDEC_SelectGrp() timeout");
                }
            }
            continue;
        }

        if (0 == grp_info.u32GrpCount) {
            LOG_MM_C(TAG, "AXCL_VDEC_SelectGrp() success, but grp count is 0");
            continue;
        }

        for (AX_U32 i = 0; i < grp_info.u32GrpCount; ++i) {
            for (AX_U32 j = 0; j < grp_info.stChnSet[i].u32ChnCount; ++j) {
                const AX_VDEC_GRP grp = grp_info.stChnSet[i].VdGrp;
                const AX_VDEC_CHN chn = grp_info.stChnSet[i].VdChn[j];
                if (!CHECK_VDEC_GRP(grp) || !CHECK_VDEC_CHN(chn)) {
                    LOG_MM_C(TAG, "AXCL_VDEC_SelectGrp() success, but invalid vdGrp {} or vdChn {}", grp, chn);
                    continue;
                }

                AX_VIDEO_FRAME_INFO_T frame;
                ret = AXCL_VDEC_GetChnFrame(grp, chn, &frame, TIMEOUT);
                if (0 != ret) {
                    if (AX_ERR_VDEC_FLOW_END == ret) {
                        LOG_MM_I(TAG, "vdGrp {} vdChn {} received flow end", grp, chn);
                        remove_sinks(grp, chn);
                    } else if (AX_ERR_VDEC_STRM_ERROR == ret) {
                        LOG_MM_W(TAG, "AXCL_VDEC_GetChnFrame(vdGrp {}, vdChn {}): stream is undecodeable", grp, chn);
                    } else if (AX_ERR_VDEC_UNEXIST == ret) {
                        LOG_MM_D(TAG, "vdGrp {} vdChn {} maybe under reseting", grp, chn);
                    } else {
                        LOG_MM_E(TAG, "AXCL_VDEC_GetChnFrame(vdGrp {}, vdChn {}, timeout {}) fail, ret = {:#x}", grp, chn, TIMEOUT,
                                 static_cast<uint32_t>(ret));
                    }
                    continue;
                }

                /* SDK only return 0, needs to release */
                if (AX_INVALID_BLOCKID == frame.stVFrame.u32BlkId[0]) {
                    LOG_MM_C(TAG, "AXCL_VDEC_GetChnFrame(vdGrp {}, vdChn {}) recv invalid frame blk id", grp, chn);
                    continue;
                }

                if (0 == frame.stVFrame.u32Width || 0 == frame.stVFrame.u32Height) {
                    LOG_MM_E(TAG, "AXCL_VDEC_GetChnFrame(vdGrp {}, vdChn {}) recv invalid frame {}x{}, pxl fmt {}, blk {:#x}", grp, chn,
                             frame.stVFrame.u32Width, frame.stVFrame.u32Height, static_cast<int32_t>(frame.stVFrame.enImgFormat),
                             frame.stVFrame.u32BlkId[0]);
                    /* if valid blk id, should release */
                    if (ret = AXCL_VDEC_ReleaseChnFrame(grp, chn, &frame); AXCL_SUCC != ret) {
                        LOG_MM_E(TAG, "AXCL_VDEC_ReleaseChnFrame(vdGrp {}, vdChn {}, blk {:#x}) fail, ret = {:#x}", grp, chn,
                                 frame.stVFrame.u32BlkId[0], static_cast<uint32_t>(ret));
                        return;
                    }

                    continue;
                }

                LOG_MM_I(TAG, "decoded vdGrp {} vdChn {} frame {} pts {} phy {:#x} {}x{} stride {} blkId {:#x}", grp, chn,
                         frame.stVFrame.u64SeqNum, frame.stVFrame.u64PTS, frame.stVFrame.u64PhyAddr[0], frame.stVFrame.u32Width,
                         frame.stVFrame.u32Height, frame.stVFrame.u32PicStride[0], frame.stVFrame.u32BlkId[0]);

                (void)dispatch_frame(grp, chn, frame);

                if (ret = AXCL_VDEC_ReleaseChnFrame(grp, chn, &frame); AXCL_SUCC != ret) {
                    LOG_MM_E(TAG, "AXCL_VDEC_ReleaseChnFrame(vdGrp {}, vdChn {}, blk {:#x}) fail, ret = {:#x}", grp, chn,
                             frame.stVFrame.u32BlkId[0], static_cast<uint32_t>(ret));
                    return;
                }
            }
        }
    } /* end while (1) */

    LOG_MM_D(TAG, "---");
}

bool vdec_dispatch::start(int32_t device) {
    if (m_started) {
        LOG_MM_W(TAG, "vdec dispatch thread is already started");
        return true;
    }

    LOG_MM_D(TAG, "+++");

    m_thread.start("vdispatch", SCHED_FIFO, 99, &vdec_dispatch::dispatch_thread, this, device);
    m_started = true;

    LOG_MM_D(TAG, "---");
    return true;
}

bool vdec_dispatch::stop() {
    if (!m_started) {
        LOG_MM_W(TAG, "vdec dispatch thread is not started yet");
        return true;
    }

    LOG_MM_D(TAG, "+++");

    m_thread.stop();
    wakeup();
    m_thread.join();
    m_started = false;

    LOG_MM_D(TAG, "---");
    return true;
}

void vdec_dispatch::register_sink(AX_VDEC_GRP grp, AX_VDEC_CHN chn, sinker* sink) {
    {
        std::lock_guard<std::mutex> lck(m_mtx);

        auto key = std::make_pair(grp, chn);
        auto range = m_map_sinks.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            if (sink == it->second) {
                LOG_MM_W(TAG, "sink {} is already registed to vdGrp {} vdChn {}", reinterpret_cast<void*>(sink), grp, chn);
                return;
            }
        }

        m_map_sinks.emplace(key, sink);
        LOG_MM_I(TAG, "sink {} is registed to vdGrp {} vdChn {}", reinterpret_cast<void*>(sink), grp, chn);
    }

    wakeup();
}

void vdec_dispatch::unregister_sink(AX_VDEC_GRP grp, AX_VDEC_CHN chn, sinker* sink) {
    std::lock_guard<std::mutex> lck(m_mtx);

    auto range = m_map_sinks.equal_range(std::make_pair(grp, chn));
    for (auto it = range.first; it != range.second; ++it) {
        if (sink == it->second) {
            m_map_sinks.erase(it);
            LOG_MM_I(TAG, "sink {} is unregisted from vdGrp {} vdChn {}", reinterpret_cast<void*>(sink), grp, chn);
            break;
        }
    }
}

void vdec_dispatch::register_sinks(AX_VDEC_GRP grp, AX_VDEC_CHN chn, const std::list<sinker*>& sinks) {
    bool registed = false;

    {
        std::lock_guard<std::mutex> lck(m_mtx);

        auto key = std::make_pair(grp, chn);
        for (auto&& m : sinks) {
            if (!m) {
                continue;
            }

            bool found = false;
            auto range = m_map_sinks.equal_range(key);
            for (auto it = range.first; it != range.second; ++it) {
                if (m == it->second) {
                    found = true;
                    LOG_MM_W(TAG, "sink {} is already registed to vdGrp {} vdChn {}", reinterpret_cast<void*>(m), grp, chn);
                    break;
                }
            }

            if (!found) {
                m_map_sinks.emplace(key, m);
                registed = true;
                LOG_MM_I(TAG, "sink {} is registed to vdGrp {} vdChn {}", reinterpret_cast<void*>(m), grp, chn);
            }
        }
    }

    if (registed) {
        wakeup();
    }
}

void vdec_dispatch::unregister_sinks(AX_VDEC_GRP grp, AX_VDEC_CHN chn, const std::list<sinker*>& sinks) {
    std::lock_guard<std::mutex> lck(m_mtx);

    for (auto&& m : sinks) {
        if (!m) {
            continue;
        }

        auto range = m_map_sinks.equal_range(std::make_pair(grp, chn));
        for (auto it = range.first; it != range.second; ++it) {
            if (m == it->second) {
                m_map_sinks.erase(it);
                LOG_MM_I(TAG, "sink {} is unregisted from vdGrp {} vdChn {}", reinterpret_cast<void*>(m), grp, chn);
                break;
            }
        }
    }
}

void vdec_dispatch::remove_sinks(AX_VDEC_GRP grp, AX_VDEC_CHN chn) {
    std::lock_guard<std::mutex> lck(m_mtx);
    m_map_sinks.erase({grp, chn});
}

void vdec_dispatch::wait() {
    std::unique_lock<std::mutex> lck(m_mtx);
    while (0 == m_map_sinks.size() && m_thread.running()) {
        m_cv.wait(lck);
    }
}

void vdec_dispatch::wakeup() {
    m_cv.notify_one();
}

bool vdec_dispatch::dispatch_frame(AX_VDEC_GRP grp, AX_VDEC_CHN chn, const AX_VIDEO_FRAME_INFO_T& frame) {
    axclite_frame axframe;
    axframe.grp = grp;
    axframe.chn = chn;
    axframe.module = AXCL_LITE_VDEC;
    axframe.frame = frame;

    if (axclError ret = axframe.increase_ref_cnt(); AXCL_SUCC != ret) {
        LOG_MM_E(TAG, "increase vdGrp {} vdChn {} frame {} VB ref count fail, ret = {:#x}", grp, chn, frame.stVFrame.u64SeqNum,
                 static_cast<uint32_t>(ret));
        return false;
    }

    std::lock_guard<std::mutex> lck(m_mtx);
    auto range = m_map_sinks.equal_range(std::make_pair(grp, chn));
    for (auto it = range.first; it != range.second; ++it) {
        (void)it->second->recv_frame(axframe);
    }

    return true;
}

}  // namespace axclite