/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <stdio.h>
#include "../utils/logger.h"
#include "axcl.h"
#include "cmdline.h"

int main(int argc, char *argv[]) {
    SAMPLE_LOG_I("============== APP(%s) Started %s %s ==============\n", AXCL_BUILD_VERSION, __DATE__, __TIME__);

    cmdline::parser a;
    a.add<int32_t>("device", 'd', "device id", false, 0);
    a.add<std::string>("json", '\0', "axcl.json path", false, "./axcl.json");
    a.parse_check(argc, argv);
    auto device = a.get<int32_t>("device");
    const std::string json = a.get<std::string>("json");

    SAMPLE_LOG_I("json: %s", json.c_str());
    if (axclError ret = axclInit(json.c_str()); AXCL_SUCC != ret) {
        return ret;
    }

    if (device <= 0) {
        axclrtDeviceList lst;
        if (axclError ret = axclrtGetDeviceList(&lst); AXCL_SUCC != ret || 0 == lst.num) {
            SAMPLE_LOG_E("no device is connected");
            axclFinalize();
            return ret;
        }

        device = lst.devices[0];
        SAMPLE_LOG_I("device id: %d", device);
    }

    if (axclError ret = axclrtSetDevice(device); AXCL_SUCC != ret) {
        axclFinalize();
        return ret;
    }

    axclrtResetDevice(device);
    axclFinalize();

    SAMPLE_LOG_I("============== APP(%s) Exited %s %s ==============\n", AXCL_BUILD_VERSION, __DATE__, __TIME__);
    return 0;
}