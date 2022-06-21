/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-11-16     JasonHu      port to rt-thread
 */

#include <rtthread.h>
#include <rtdbg.h>
#include <drv_avs.h>

#ifdef BSP_USING_AVS

static rt_err_t avs_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_err_t ret = RT_EOK;
    rt_hw_driver_avs_t *pusr;
    struct rt_hw_driver_avs_ops *drv_ops = NULL;

    if (dev == NULL)
    {
        return -RT_EINVAL;
    }

    pusr = (rt_hw_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        drv_ops = (struct rt_hw_driver_avs_ops *)pusr->drv_ops;
    }

    if (drv_ops && drv_ops->initialize)
    {
        ret = drv_ops->initialize(pusr->dev_id);
    }

    return ret;
}

static rt_err_t avs_close(struct rt_device *dev)
{
    rt_err_t ret = RT_EOK;
    rt_hw_driver_avs_t *pusr = NULL;
    struct rt_hw_driver_avs_ops *drv_ops = NULL;

    if (dev == NULL)
    {
        return -RT_EINVAL;
    }

    pusr = (rt_hw_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        drv_ops = (struct rt_hw_driver_avs_ops *)pusr->drv_ops;
    }

    if (drv_ops && drv_ops->uninitialize)
    {
        ret = drv_ops->uninitialize(pusr->dev_id);
    }

    return ret;
}

static rt_err_t avs_init(struct rt_device *dev)
{
    return RT_EOK;
}

static rt_size_t avs_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    return RT_EOK;
}

static rt_size_t avs_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return RT_EOK;
}

static rt_err_t avs_control(struct rt_device *dev, int cmd, void *args)
{
    rt_err_t ret = 0;
    rt_hw_driver_avs_t *pusr = NULL;
    struct rt_hw_driver_avs_ops *drv_ops = NULL;

    if (dev == NULL)
    {
        return -RT_EINVAL;
    }

    pusr = (rt_hw_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        drv_ops = (struct rt_hw_driver_avs_ops *)pusr->drv_ops;
    }

    if (drv_ops && drv_ops->control)
    {
        ret = drv_ops->control(pusr->dev_id, cmd, args);
    }

    return ret;
}

static void init_avs_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->open       = avs_open;
    dev->close      = avs_close;
    dev->init       = avs_init;
    dev->write      = avs_write;
    dev->read       = avs_read;
    dev->control    = avs_control;
    dev->user_data  = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK)
    {
        LOG_E("register avs device failure with ret code %d.", ret);
        return;
    }
}

static const struct rt_hw_driver_avs_ops rt_hw_avs_ops =
{
    .initialize     = hal_avs_init,
    .uninitialize   = hal_avs_uninit,
    .control        = hal_avs_control,
};

int rt_hw_driver_avs_init(void)
{
    struct rt_device *device[AVS_NUM];
    static rt_hw_driver_avs_t avs[AVS_NUM];
    int i;

    for (i = 0; i < AVS_NUM; i++)
    {
        device[i] = &avs[i].base;
        avs[i].dev_id = i;
        avs[i].drv_ops = &rt_hw_avs_ops;
        avs[i].used = 0;
        rt_sprintf(avs[i].name, "avs%d", i);
        init_avs_device(device[i], &avs[i], avs[i].name);
    }
    return 0;
}

INIT_DEVICE_EXPORT(rt_hw_driver_avs_init);

#endif
