/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-11-16     JasonHu      port to rt-thread
 */

#ifndef DRV_AVS_H__
#define DRV_AVS_H__

#include <stdint.h>
#include <rtthread.h>
#include <sunxi_hal_avs.h>

#define MAX_AVS_NAME_LEN    10

struct rt_hw_driver_avs
{
    struct rt_device base;
    int32_t dev_id;
    bool used;
    const void *drv_ops;
    char name[MAX_AVS_NAME_LEN];
};
typedef struct rt_hw_driver_avs rt_hw_driver_avs_t;

struct rt_hw_driver_avs_ops
{
    int (*initialize)(hal_avs_id_t port);
    int (*uninitialize)(hal_avs_id_t port);
    int (*control)(hal_avs_id_t port, hal_avs_cmd_t cmd, void *args);
};

#endif
