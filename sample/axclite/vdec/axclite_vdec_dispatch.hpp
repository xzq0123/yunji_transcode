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

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>
#include "axclite.h"
#include "singleton.hpp"
#include "threadx.hpp"

namespace axclite {
class sinker;
class vdec_dispatch : public axcl::singleton<vdec_dispatch> {
    friend class axcl::singleton<vdec_dispatch>;
    friend class vdec;

    struct pair_hash {
        template <typename T1, typename T2>
        std::size_t operator()(const std::pair<T1, T2>& p) const {
            return std::hash<T1>{}(p.first) ^ std::hash<T2>{}(p.second);
        }
    };

    struct pair_equal {
        template <typename T1, typename T2>
        bool operator()(const std::pair<T1, T2>& a, const std::pair<T1, T2>& b) const {
            return a.first == b.first && a.second == b.second;
        }
    };

public:
    bool start(int32_t device);
    bool stop();

protected:
    void register_sink(AX_VDEC_GRP grp, AX_VDEC_CHN chn, sinker* sink);
    void unregister_sink(AX_VDEC_GRP grp, AX_VDEC_CHN chn, sinker* sink);

    void register_sinks(AX_VDEC_GRP grp, AX_VDEC_CHN chn, const std::list<sinker*>& sinks);
    void unregister_sinks(AX_VDEC_GRP grp, AX_VDEC_CHN chn, const std::list<sinker*>& sinks);

    void dispatch_thread(int32_t device);
    void wait();
    void wakeup();

    bool dispatch_frame(AX_VDEC_GRP grp, AX_VDEC_CHN chn, const AX_VIDEO_FRAME_INFO_T& frame);
    void remove_sinks(AX_VDEC_GRP grp, AX_VDEC_CHN chn);

protected:
    axcl::threadx m_thread;
    std::atomic<bool> m_started = {false};
    std::condition_variable m_cv;
    std::mutex m_mtx;
    std::unordered_multimap<std::pair<AX_VDEC_GRP, AX_VDEC_CHN>, sinker*, pair_hash, pair_equal> m_map_sinks;
};

}  // namespace axclite