/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-02-08     Zhangyihong  the first version
 * 2018-10-29     XY
 */
#include "drv_touch.h"
#include <lwp_user_mm.h>

#ifndef TOUCH_I2C_NAME
#define TOUCH_I2C_NAME  "i2c2"
#endif

#ifndef TOUCH_SAMPLE_HZ
#define TOUCH_SAMPLE_HZ (50)
#endif

#ifndef TOUCH_I2C_NAME
#error "Please define touch i2c name!"
#endif

#define DBG_TAG  "TOUCH"
#define DBG_LVL  DBG_LOG
#include <rtdbg.h>
#include <touch.h>
#include "lwp.h"
#include "lwp_shm.h"

#define CMD_MOUSE_SET_NOTIFY 0  /* arg is shmid, in the shm, a sem point is given */

struct drv_mouse_device
{
    struct rt_device parent;
    struct rt_touch_data touchdata;
    int channel;
};
struct drv_mouse_device _mouse;

enum _rtgui_event_type
{
    /* applications event */
    RTGUI_EVENT_APP_CREATE,            /* create an application */
    RTGUI_EVENT_APP_DESTROY,           /* destroy an application */
    RTGUI_EVENT_APP_ACTIVATE,          /* activate an application */
    RTGUI_EVENT_APP_DESTROY_DEAD,      /* delete all dead objects */

    /* window event */
    RTGUI_EVENT_WIN_CREATE,            /* create a window       */
    RTGUI_EVENT_WIN_DESTROY,           /* destroy a window      */
    RTGUI_EVENT_WIN_SHOW,              /* show a window         */
    RTGUI_EVENT_WIN_HIDE,              /* hide a window         */
    RTGUI_EVENT_WIN_ACTIVATE,          /* activate a window     */
    RTGUI_EVENT_WIN_DEACTIVATE,        /* deactivate a window   */
    RTGUI_EVENT_WIN_CLOSE,             /* close a window        */
    RTGUI_EVENT_WIN_MOVE,              /* move a window         */
    RTGUI_EVENT_WIN_RESIZE,            /* resize a window       */
    RTGUI_EVENT_WIN_UPDATE_END,        /* update done for window */
    RTGUI_EVENT_WIN_MODAL_ENTER,       /* the window is entering modal mode.
                                          This event should be sent after the
                                          window got setup and before the
                                          application got setup. */

    /* WM event */
    RTGUI_EVENT_SET_WM,                /* set window manager    */

    RTGUI_EVENT_UPDATE_BEGIN,          /* update a rect         */
    RTGUI_EVENT_UPDATE_END,            /* update a rect         */
    RTGUI_EVENT_MONITOR_ADD,           /* add a monitor rect    */
    RTGUI_EVENT_MONITOR_REMOVE,        /* remove a monitor rect */
    RTGUI_EVENT_SHOW,                  /* the widget is going to be shown */
    RTGUI_EVENT_HIDE,                  /* the widget is going to be hidden */
    RTGUI_EVENT_PAINT,                 /* paint on screen       */
    RTGUI_EVENT_TIMER,                 /* timer                 */
    RTGUI_EVENT_UPDATE_TOPLVL,         /* update the toplevel   */

    /* virtual paint event */
    RTGUI_EVENT_VPAINT_REQ,            /* virtual paint request (server -> client) */

    /* clip rect information */
    RTGUI_EVENT_CLIP_INFO,             /* clip rect info        */

    /* mouse and keyboard event */
    RTGUI_EVENT_MOUSE_MOTION,          /* mouse motion          */
    RTGUI_EVENT_MOUSE_BUTTON,          /* mouse button info     */
    RTGUI_EVENT_KBD,                   /* keyboard info         */
    RTGUI_EVENT_TOUCH,                 /* touch event to server */
    RTGUI_EVENT_GESTURE,               /* gesture event         */

    /* widget event */
    RTGUI_EVENT_FOCUSED,               /* widget focused        */
    RTGUI_EVENT_SCROLLED,              /* scroll bar scrolled   */
    RTGUI_EVENT_RESIZE,                /* widget resize         */
    RTGUI_EVENT_SELECTED,              /* widget selected       */
    RTGUI_EVENT_UNSELECTED,            /* widget un-selected    */
    RTGUI_EVENT_MV_MODEL,              /* data of a model has been changed */

    /* lcd event */
    RTGUI_EVENT_TE,                    /* LCD TE sync signal    */

    WBUS_NOTIFY_EVENT,

    /* user command event. It should always be the last command type. */
    RTGUI_EVENT_COMMAND = 0x0100,      /* user command          */
};

#define RTGUI_MOUSE_BUTTON_LEFT         0x01
#define RTGUI_MOUSE_BUTTON_RIGHT        0x02
#define RTGUI_MOUSE_BUTTON_MIDDLE       0x03
#define RTGUI_MOUSE_BUTTON_WHEELUP      0x04
#define RTGUI_MOUSE_BUTTON_WHEELDOWN    0x08

#define RTGUI_MOUSE_BUTTON_DOWN         0x10
#define RTGUI_MOUSE_BUTTON_UP           0x20

static rt_slist_t _driver_list;
static struct rt_i2c_bus_device *i2c_bus = RT_NULL;

static void post_event(rt_uint16_t x, rt_uint16_t y, rt_uint8_t event)
{
    struct rt_touch_data *minfo = &_mouse.touchdata;
    struct rt_channel_msg ch_msg;

    LOG_D("event:%d, x:%d, y:%d", event, x, y);

    minfo->x_coordinate = x;
    minfo->y_coordinate = y;
    minfo->event = event;
    ch_msg.type = RT_CHANNEL_RAW;
    ch_msg.u.d = (void *)(size_t)event;
    rt_channel_send(_mouse.channel, &ch_msg);
}

static void touch_run(void* parameter)
{
    struct touch_message msg;
    rt_slist_t *driver_list = NULL;
    struct touch_driver *current_driver = RT_NULL;

    i2c_bus = rt_i2c_bus_device_find(TOUCH_I2C_NAME);
    RT_ASSERT(i2c_bus);

    if(rt_device_open(&i2c_bus->parent, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        return;
    }

    rt_slist_for_each(driver_list, &_driver_list)
    {
        current_driver = (struct touch_driver *)driver_list;
        if(current_driver->probe(i2c_bus) == RT_TRUE)
        {
            break;
        }
        current_driver = RT_NULL;
    }

    if(current_driver == RT_NULL)
    {
        LOG_E("[TP] No touch pad or driver.");
        rt_device_close((rt_device_t)i2c_bus);

        return;
    }

    current_driver->ops->init(i2c_bus);

    while (1)
    {
        if (rt_sem_take(current_driver->isr_sem, RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }

        if (current_driver->ops->read_point(&msg) != RT_EOK)
        {
            continue;
        }

        switch (msg.event)
        {
        case TOUCH_EVENT_MOVE:
            post_event(msg.x, msg.y, RT_TOUCH_EVENT_MOVE);
            break;

        case TOUCH_EVENT_DOWN:
            post_event(msg.x, msg.y, RT_TOUCH_EVENT_DOWN);
            break;

        case TOUCH_EVENT_UP:
            post_event(msg.x, msg.y, RT_TOUCH_EVENT_UP);
            break;

        default:
            break;
        }

        rt_thread_delay(RT_TICK_PER_SECOND / TOUCH_SAMPLE_HZ);
    }
}

rt_err_t rt_touch_drivers_register(touch_driver_t drv)
{
    RT_ASSERT(drv != RT_NULL);
    RT_ASSERT(drv->ops   != RT_NULL);
    RT_ASSERT(drv->probe != RT_NULL);

    rt_slist_append(&_driver_list, &drv->list);

    return RT_EOK;
}

static int rt_touch_list_init(void)
{
    rt_slist_init(&_driver_list);

    return RT_EOK;
}
INIT_BOARD_EXPORT(rt_touch_list_init);

static rt_err_t drv_mouse_init(struct rt_device *device)
{
    struct drv_mouse_device *mouse = (struct drv_mouse_device*)device;

    mouse = mouse;
    return RT_EOK;
}

static rt_err_t drv_mouse_control(struct rt_device *device, int cmd, void *args)
{
    switch (cmd)
    {
    case CMD_MOUSE_SET_NOTIFY:
#ifdef RT_USING_USERSPACE
        *(size_t*)args  = (size_t)lwp_map_user_phy(lwp_self(), RT_NULL, (void*)((size_t)&_mouse.touchdata + PV_OFFSET),
            sizeof(struct rt_touch_data), 1);
#endif
        break;
    default:
        break;
    }
    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops _mouse_ops =
{
    drv_mouse_init,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    drv_mouse_control
};
#endif

static int rt_touch_init(void)
{
    rt_thread_t thread = RT_NULL;
    {
        struct rt_device *device = &_mouse.parent;

#ifdef RT_USING_DEVICE_OPS
        device->ops     = &_mouse_ops;
#else
        device->init        = drv_mouse_init;
        device->open        = RT_NULL;
        device->close       = RT_NULL;
        device->read        = RT_NULL;
        device->write       = RT_NULL;
        device->control     = drv_mouse_control;
#endif
        rt_device_register(device, "mouse", RT_DEVICE_FLAG_RDWR);
    }

    /* create the IPC channel for 'mouse' */
    _mouse.channel = rt_channel_open("mouse", O_CREAT);

    thread = rt_thread_create("touch", touch_run, RT_NULL, 2048, 2, 20);
    if(thread)
    {
        return rt_thread_startup(thread);
    }

    return RT_ERROR;
}
INIT_APP_EXPORT(rt_touch_init);

int rt_touch_read(rt_uint16_t addr, void *cmd_buf, size_t cmd_len, void *data_buf, size_t data_len)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = cmd_buf;
    msgs[0].len   = cmd_len;

    msgs[1].addr  = addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = data_buf;
    msgs[1].len   = data_len;

    if (rt_i2c_transfer(i2c_bus, msgs, 2) == 2)
        return 0;
    else
        return -1;
}

int rt_touch_write(rt_uint16_t addr, void *data_buf, size_t data_len)
{
    struct rt_i2c_msg msgs[1];

    msgs[0].addr  = addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = data_buf;
    msgs[0].len   = data_len;

    if (rt_i2c_transfer(i2c_bus, msgs, 1) == 1)
        return 0;
    else
        return -1;
}
