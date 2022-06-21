/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-11-16     JasonHu      first version
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <tick.h>

#include <sunxi_timer.h>

#ifdef RT_USING_HWTIMER

struct hal_hwtimer_dev
{
    rt_hwtimer_t parent;
    hal_timer_id_t timer_id;
    void (*timer_handler)(void *);
};

#ifdef BSP_USING_HWTIMER0
static struct hal_hwtimer_dev hwtimer0;

static void hwtimer0_handler(void *param)
{
    rt_device_hwtimer_isr(&hwtimer0.parent);
}
#endif

#ifdef BSP_USING_HWTIMER1
static struct hal_hwtimer_dev hwtimer1;

static void hwtimer1_handler(void *param)
{
    rt_device_hwtimer_isr(&hwtimer1.parent);
}
#endif

static struct rt_hwtimer_info hal_hwtimer_info =
{
    TIMER_CLK_FREQ, /* maximum count frequency */
    TIMER_CLK_FREQ, /* minimum count frequency */
    0xffffffff, /* counter maximum value */
    HWTIMER_CNTMODE_DW
};

static void hal_hwtimer_init(rt_hwtimer_t *timer, rt_uint32_t state)
{
    struct hal_hwtimer_dev *hwtimer = (struct hal_hwtimer_dev *)timer->parent.user_data;

    RT_ASSERT(hwtimer != RT_NULL);

    if (1 == state)
    {
        sunxi_timer_init(hwtimer->timer_id);
    }
    else
    {
        sunxi_timer_uninit(hwtimer->timer_id);
    }

    hwtimer->parent.freq = TIMER_CLK_FREQ; // TODO: get freq from clk
}

/**
 * in sunxi timer, cnt = us * 25
 * so we let us = cnt / 25 here
 * so that cnt equal to sunxi timer cnt
 */
#define TIMER_CNT_TO_US(cnt) ((cnt) / 25)

static rt_err_t hal_hwtimer_start(rt_hwtimer_t *timer,
                                     rt_uint32_t cnt,
                                     rt_hwtimer_mode_t mode)
{
    struct hal_hwtimer_dev *hwtimer = (struct hal_hwtimer_dev *)timer->parent.user_data;

    RT_ASSERT(hwtimer != RT_NULL);

    /* hardware support oneshot and period mode */
    if (mode == HWTIMER_MODE_PERIOD)
    {
        sunxi_timer_set_periodic(TIMER_CNT_TO_US(cnt), hwtimer->timer_id, hwtimer->timer_handler, NULL);
    }
    else
    {
        sunxi_timer_set_oneshot(TIMER_CNT_TO_US(cnt), hwtimer->timer_id, hwtimer->timer_handler, NULL);
    }
    return RT_EOK;
}

static void hal_hwtimer_stop(rt_hwtimer_t *timer)
{
    struct hal_hwtimer_dev *hwtimer = (struct hal_hwtimer_dev *)timer->parent.user_data;

    RT_ASSERT(hwtimer != RT_NULL);

    sunxi_timer_stop(hwtimer->timer_id);
}

static rt_uint32_t hal_hwtimer_count_get(rt_hwtimer_t *timer)
{
    struct hal_hwtimer_dev *hwtimer = (struct hal_hwtimer_dev *)timer->parent.user_data;
    uint32_t hwtimer_count = 0;

    RT_ASSERT(hwtimer != RT_NULL);

    hwtimer_count = sunxi_timer_get_count(hwtimer->timer_id);
    return hwtimer_count;
}

static rt_err_t hal_hwtimer_control(rt_hwtimer_t *timer,
                                    rt_uint32_t cmd,
                                    void *args)
{
    rt_err_t ret = RT_EOK;
    rt_uint32_t freq = 0;
    struct hal_hwtimer_dev *hwtimer = (struct hal_hwtimer_dev *)timer->parent.user_data;

    RT_ASSERT(hwtimer != RT_NULL);

    switch (cmd)
    {
    case HWTIMER_CTRL_FREQ_SET:
        freq = *(rt_uint32_t *)args;
        if (freq != TIMER_CLK_FREQ)
        {
            ret = -RT_ERROR;
        }
        break;

    case HWTIMER_CTRL_STOP:
        sunxi_timer_stop(hwtimer->timer_id);
        break;

    default:
        ret = RT_EINVAL;
        break;
    }
    return ret;
}

static struct rt_hwtimer_ops hal_hwtimer_ops =
{
    hal_hwtimer_init,
    hal_hwtimer_start,
    hal_hwtimer_stop,
    hal_hwtimer_count_get,
    hal_hwtimer_control
};

int rt_hw_hwtimer_init(void)
{
    rt_err_t ret = RT_EOK;

#ifdef BSP_USING_HWTIMER0
    hwtimer0.timer_id = SUNXI_TMR0;
    hwtimer0.timer_handler = hwtimer0_handler;
    hwtimer0.parent.info = &hal_hwtimer_info;
    hwtimer0.parent.ops = &hal_hwtimer_ops;
    ret = rt_device_hwtimer_register(&hwtimer0.parent, "timer0", &hwtimer0);
#endif

#ifdef BSP_USING_HWTIMER1
    hwtimer1.timer_id = SUNXI_TMR1;
    hwtimer1.timer_handler = hwtimer1_handler;
    hwtimer1.parent.info = &hal_hwtimer_info;
    hwtimer1.parent.ops = &hal_hwtimer_ops;
    ret = rt_device_hwtimer_register(&hwtimer1.parent, "timer1", &hwtimer1);
#endif
    return ret;
}
INIT_DEVICE_EXPORT(rt_hw_hwtimer_init);
#endif
