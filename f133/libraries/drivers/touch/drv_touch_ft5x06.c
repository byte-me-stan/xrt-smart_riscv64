/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-08-08     Yang         the first version
 * 2017-08-08     XY           imxrt1052
 * 2018-10-29     XY
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "drv_touch.h"
// #include "tcon.h"
#include "drv_lcd.h"
// #include "drv_gpio.h"
#include "drv_pin.h"


#define TP_INT_PIN GET_PIN(GPIO_PORT_E, GPIO_PIN_10)       /* GPIO_PORT_E GPIO_PIN_10 */
#define TP_RST_PIN GET_PIN(GPIO_PORT_E, GPIO_PIN_11)       /* GPIO_PORT_E GPIO_PIN_11 */

#define GREE_ZERO_VERSION_TOUCH  49152
#define GREE_ONE_VERSION_TOUCH   15712256

#define FT5x06_TS_ADDR      (0x38)

#define DBG_TAG  "TOUCH.ft5x06"
#define DBG_LVL  DBG_LOG
#include <rtdbg.h>

// extern tcon_panel_t _panel;
static struct touch_driver ft5x06_driver;
int use_vesion;


struct rt_touch_data
{
    rt_uint8_t          event;                 /* The touch event of the data */
    rt_uint8_t          track_id;              /* Track id of point */
    rt_uint8_t          width;                 /* Point of width */
    rt_uint16_t         x_coordinate;          /* Point of x coordinate */
    rt_uint16_t         y_coordinate;          /* Point of y coordinate */
    rt_tick_t           timestamp;             /* The timestamp when the data was received */
};

typedef enum _touch_event
{
    kTouch_Down = 0,    /*!< The state changed to touched. */
    kTouch_Up = 1,      /*!< The state changed to not touched. */
    kTouch_Contact = 2, /*!< There is a continuous touch being detected. */
    kTouch_Reserved = 3 /*!< No touch information available. */
} touch_event_t;

typedef struct _touch_point
{
    touch_event_t TOUCH_EVENT;  /*!< Indicates the state or event of the touch point. */
    uint8_t TOUCH_ID;           /*!< Id of the touch point. This numeric value stays constant between down and up event. */
    uint16_t TOUCH_X;           /*!< X coordinate of the touch point */
    uint16_t TOUCH_Y;           /*!< Y coordinate of the touch point */
} touch_point_t;

typedef struct _ft5406_touch_point
{
    uint8_t XH;
    uint8_t XL;
    uint8_t YH;
    uint8_t YL;
    uint8_t RESERVED[2];
} ft5406_touch_point_t;

typedef struct _ft5406_touch_data
{
    uint8_t DEVIDE_MODE;
    uint8_t GEST_ID;
    uint8_t TD_STATUS;
    ft5406_touch_point_t TOUCH;
} ft5406_touch_data_t;

#define TOUCH_POINT_GET_EVENT(T)    ((touch_event_t)((T).XH >> 6))
#define TOUCH_POINT_GET_ID(T)       ((T).YH >> 4)
#define TOUCH_POINT_GET_X(T)        ((((T).XH & 0x0f) << 8) | (T).XL)
#define TOUCH_POINT_GET_Y(T)        ((((T).YH & 0x0f) << 8) | (T).YL)

static int ft5406_read_touch(touch_point_t *dp)
{
    rt_uint8_t cmd = 0;
    ft5406_touch_data_t touch_data;

    if (rt_touch_read(FT5x06_TS_ADDR, &cmd, 1, &touch_data, sizeof(ft5406_touch_data_t)) != 0)
        return -1;

    dp->TOUCH_X     = TOUCH_POINT_GET_Y(touch_data.TOUCH);
    dp->TOUCH_Y     = TOUCH_POINT_GET_X(touch_data.TOUCH);
    dp->TOUCH_EVENT = TOUCH_POINT_GET_EVENT(touch_data.TOUCH);
    dp->TOUCH_ID    = TOUCH_POINT_GET_ID(touch_data.TOUCH);

    if (dp->TOUCH_EVENT == 3) return -1;

    if (touch_data.TD_STATUS != 0)
        return 0;
    else
        return -1;
}

static void ft5x06_isr_enable(rt_bool_t enable)
{
    if(enable == RT_TRUE)
    {
        rt_pin_irq_enable(TP_INT_PIN, PIN_IRQ_ENABLE);
    }
    else
    {
        rt_pin_irq_enable(TP_INT_PIN, PIN_IRQ_DISABLE);
    }
}

static void ft5x06_touch_isr(void *parameter)
{
    ft5x06_isr_enable(RT_FALSE);
    rt_sem_release(ft5x06_driver.isr_sem);
}

static int get_ft5x06_version(rt_uint16_t version[2])
{
    int ret = 0;
    int temp = ((version[0]<<8) | version[1]);

    if(temp==GREE_ZERO_VERSION_TOUCH)
    {
        ret = 0;
    }
    else if(temp==GREE_ONE_VERSION_TOUCH)
    {
        ret = 1;
    }
    return ret;
}

static rt_err_t ft5x06_read_point(touch_message_t msg)
{
    touch_point_t dp = {0};

    if (ft5406_read_touch(&dp) != 0)
    {
        msg->event = TOUCH_EVENT_UP;
    }
    else
    {
        if (dp.TOUCH_EVENT == kTouch_Up)
        {
            msg->event = TOUCH_EVENT_UP;
        }
        else if (dp.TOUCH_EVENT == kTouch_Down)
        {
            msg->event = TOUCH_EVENT_DOWN;
        }
        else if (dp.TOUCH_EVENT == kTouch_Contact)
        {
            msg->event = TOUCH_EVENT_MOVE;
        }
        else
        {
            msg->event = TOUCH_EVENT_UP;
        }
    }

#if 1
    msg->x = dp.TOUCH_X;
    msg->y = dp.TOUCH_Y;
#else
    switch (_panel->ctp_flag)
    {
    case REVERSE_X:
        msg->x = dp.TOUCH_X;
        msg->x = _panel->width - msg->x;
        msg->y = dp.TOUCH_Y;
        break;
    case REVERSE_Y:
        msg->x = dp.TOUCH_X;
        msg->x = _panel->width - msg->x;
        msg->y = dp.TOUCH_Y;
        msg->y = _panel->height - msg->y;
        break;
    case (REVERSE_X | REVERSE_Y):
        msg->x = dp.TOUCH_X;
        msg->y = dp.TOUCH_Y;
        msg->y = _panel->height - msg->y;
        break;
    case REVERSE_MODE:
        msg->y = dp.TOUCH_X;
        msg->x = dp.TOUCH_Y;
        msg->x = _panel->height - msg->x;
        break;
    case (REVERSE_MODE | REVERSE_X):
        msg->y = dp.TOUCH_X;
        msg->x = dp.TOUCH_Y;
        msg->x = msg->x;
        break;
    case (REVERSE_MODE | REVERSE_Y):
        msg->y = dp.TOUCH_X;
        msg->y = _panel->width - msg->y;
        msg->x = dp.TOUCH_Y;
        msg->x = _panel->height - msg->x;
        break;
    case (REVERSE_MODE | REVERSE_X | REVERSE_Y):
        msg->y = dp.TOUCH_X;
        msg->x = dp.TOUCH_Y;
        msg->y = _panel->width - msg->y;
        break;
    default:
        msg->x = dp.TOUCH_X;
        msg->y = dp.TOUCH_Y;
        break;
    }
#endif
    // if (msg->event != TOUCH_EVENT_NONE)
    // LOG_D("[TP] [%d, %d] %s\n", msg->x, msg->y,
    //        msg->event == TOUCH_EVENT_DOWN ? "DOWN" : (msg->event == TOUCH_EVENT_MOVE ? "MOVE" : (msg->event == TOUCH_EVENT_UP ? "UP" : "NONE")));


    if (msg->event != TOUCH_EVENT_UP)
    {
        rt_sem_release(ft5x06_driver.isr_sem);
    }
    else
    {
        ft5x06_isr_enable(RT_TRUE);
    }

    return RT_EOK;
}

static void ft5x06_init(struct rt_i2c_bus_device *i2c_bus)
{
    ft5x06_driver.isr_sem = rt_sem_create("ft5x06", 0, RT_IPC_FLAG_FIFO);
    RT_ASSERT(ft5x06_driver.isr_sem);

    rt_pin_attach_irq(TP_INT_PIN, PIN_IRQ_MODE_LOW_LEVEL, ft5x06_touch_isr, &ft5x06_driver);
    rt_pin_irq_enable(TP_INT_PIN, PIN_IRQ_ENABLE);
    rt_thread_delay(RT_TICK_PER_SECOND / 5);
}

static void ft5x06_deinit(void)
{
    if (ft5x06_driver.isr_sem)
    {
        rt_sem_delete(ft5x06_driver.isr_sem);
        ft5x06_driver.isr_sem = RT_NULL;
    }
}

struct touch_ops ft5x06_ops =
{
    ft5x06_init,
    ft5x06_deinit,
    ft5x06_read_point,
};

static void ft5406_hw_reset(void)
{
    rt_pin_mode(TP_RST_PIN, PIN_MODE_OUTPUT);

    rt_pin_write(TP_RST_PIN, PIN_HIGH);
    rt_thread_delay(RT_TICK_PER_SECOND / 50);

    rt_pin_write(TP_RST_PIN, PIN_LOW);
    rt_thread_delay(RT_TICK_PER_SECOND / 50);

    rt_pin_write(TP_RST_PIN, PIN_HIGH);
    rt_thread_delay(RT_TICK_PER_SECOND / 50);
}

static rt_bool_t ft5x06_probe(struct rt_i2c_bus_device *i2c_bus)
{
    rt_uint16_t cmd = 0x000c;
    rt_uint16_t tmp[2];

    ft5406_hw_reset();

    if (rt_touch_read(FT5x06_TS_ADDR, &cmd, 2, tmp, 2) != 0)
    {
        return RT_FALSE;
    }
	use_vesion = get_ft5x06_version(tmp);
    LOG_I("FT5X06 Touch Version : %d", (tmp[0]<<8) | tmp[1]);

    return RT_TRUE;
}

int ft5x06_driver_register(void)
{
    ft5x06_driver.probe = ft5x06_probe;
    ft5x06_driver.ops = &ft5x06_ops;
    ft5x06_driver.user_data = RT_NULL;

    rt_touch_drivers_register(&ft5x06_driver);

    return 0;
}
// INIT_ENV_EXPORT(ft5x06_driver_register);
