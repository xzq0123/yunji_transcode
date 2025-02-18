/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __AXCL_RT_MEMORY_H__
#define __AXCL_RT_MEMORY_H__

#include "axcl_rt_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXCL_DEF_MEMORY_ERR(e)              AXCL_DEF_RUNTIME_ERR(AXCL_RUNTIME_MEMORY, (e))

#define AXCL_ERR_MEMORY_NULL_POINTER        AXCL_DEF_MEMORY_ERR(AXCL_ERR_NULL_POINTER)
#define AXCL_ERR_MEMORY_ENCODE              AXCL_DEF_MEMORY_ERR(AXCL_ERR_ENCODE)
#define AXCL_ERR_MEMORY_DECODE              AXCL_DEF_MEMORY_ERR(AXCL_ERR_DECODE)
#define AXCL_ERR_MEMORY_UNEXPECT_RESPONSE   AXCL_DEF_MEMORY_ERR(AXCL_ERR_UNEXPECT_RESPONSE)
#define AXCL_ERR_MEMORY_EXECUTE_FAIL        AXCL_DEF_MEMORY_ERR(AXCL_ERR_EXECUTE_FAIL)

#define AXCL_ERR_MEMORY_NOT_DEVICE_MEMORY   AXCL_DEF_MEMORY_ERR(0x81)

/**
 * @ingroup AxeraCL Runtime
 * @brief Allocate device memory
 *
 * @param devPtr [OUT]       Pointer to allocated device memory
 * @param size [IN]          Size of memory to allocate
 * @param policy [IN]        Memory allocation policy
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMalloc(void **devPtr, size_t size, axclrtMemMallocPolicy policy);

/**
 * @ingroup AxeraCL Runtime
 * @brief Allocate cached device memory
 *
 * @param devPtr [OUT]       Pointer to allocated cached device memory
 * @param size [IN]          Size of memory to allocate
 * @param policy [IN]        Memory allocation policy
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMallocCached(void **devPtr, size_t size, axclrtMemMallocPolicy policy);

/**
 * @ingroup AxeraCL Runtime
 * @brief Free device memory
 *
 * @param devPtr [IN]        Pointer to device memory to free
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtFree(void *devPtr);

/**
 * @ingroup AxeraCL Runtime
 * @brief Flush device memory
 *
 * @param devPtr [IN]        Pointer to device memory to flush
 * @param size [IN]          Size of memory to flush
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMemFlush(void *devPtr, size_t size);

/**
 * @ingroup AxeraCL Runtime
 * @brief Invalidate device memory
 *
 * @param devPtr [IN]        Pointer to device memory to invalidate
 * @param size [IN]          Size of memory to invalidate
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMemInvalidate(void *devPtr, size_t size);

/**
 * @ingroup AxeraCL Runtime
 * @brief Allocate host memory
 *
 * @param hostPtr [OUT]      Pointer to allocated host memory
 * @param size [IN]          Size of memory to allocate
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMallocHost(void **hostPtr, size_t size);

/**
 * @ingroup AxeraCL Runtime
 * @brief Free host memory
 *
 * @param hostPtr [IN]       Pointer to host memory to free
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtFreeHost(void *hostPtr);

/**
 * @ingroup AxeraCL Runtime
 * @brief Set device memory
 *
 * @param devPtr [IN]        Pointer to device memory
 * @param value [IN]         Value to set
 * @param count [IN]         Number of bytes to set
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMemset(void *devPtr, uint8_t value, size_t count);

/**
 * @ingroup AxeraCL Runtime
 * @brief Copy memory
 *
 * @param dstPtr [OUT]       Destination pointer
 * @param srcPtr [IN]        Source pointer
 * @param count [IN]         Number of bytes to copy
 * @param kind [IN]          Memory copy kind
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMemcpy(void *dstPtr, const void *srcPtr, size_t count, axclrtMemcpyKind kind);

/**
 * @ingroup AxeraCL Runtime
 * @brief Compare device memories
 *
 * @param devPtr1 [IN]       First device memory pointer
 * @param devPtr2 [IN]       Second device memory pointer
 * @param count [IN]         Number of bytes to compare
 *
 * @par Restriction
 * Only available for device memory.
 *
 * @retval AXCL_SUCC The function is successfully executed.
 * @retval OtherValues Failure
**/
axclError axclrtMemcmp(const void *devPtr1, const void *devPtr2, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* __AXCL_RT_MEMORY_H__ */
