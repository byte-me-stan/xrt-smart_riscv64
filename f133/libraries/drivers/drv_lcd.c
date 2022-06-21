/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-04-12     RT-Thread    the first version
 */

#include "rtthread.h"
#include <rthw.h>
#include <string.h>

#include "interrupt.h"
#include "mmu.h"
#include "drv_lcd.h"

#include <page.h>
#include <lwp_user_mm.h>

#include <video/sunxi_display2.h>
#include "dev_disp.h"

#define BSP_USING_LCD
#ifdef BSP_USING_LCD

#define DEFAULT_SCREEN          (0)
#ifdef BOARD_allwinnerd1
#define LCD_LAYER_WIDTH         (800)
#define LCD_LAYER_HEIGHT        (1280)
#else /* f133 */
#define LCD_LAYER_WIDTH         (480)
#define LCD_LAYER_HEIGHT        (272)
#endif
static rt_uint32_t memcp_tick;

enum use_buff { FRONT, BACK };
enum state_buff { EMPTY, FULL };

struct buff_info
{
    rt_uint32_t *buff;
    rt_uint32_t *buff_phy;
    enum state_buff status;
};

/* kind of a candidate for the official lcd driver framework */
struct lcd_device
{
    struct rt_device lcd;
    struct rt_device_graphic_info lcd_info;     /* rtdef.h */
    struct rt_semaphore lcd_sem;
    int use_screen;                 /* layer index */
    void *framebuffer;
    void *framebuffer_phy;
    struct buff_info front_buf_info;
    struct buff_info back_buf_info;
    enum use_buff current_buf;     /* either 'front_buf' or 'back_buf' */
};

static struct disp_layer_config layer_cfg;

typedef struct lcd_device *lcd_device_t;

void *lcd_get_framebuffer(void *dev)
{
    return ((struct lcd_device *)dev)->framebuffer;
}

/* Multi-layer is not supported now. */
#ifdef GUIENGINE_USING_MULTI_LAYER
#include <rtgui/rtgui_layer.h>
struct debe_info
{
    int index;
    void *buffer;
    rt_uint32_t bf_size;
};
static struct debe_info _debe_use[3];
#endif

/* pixel format, only 565 (2 bytes) or 666 (4 bytes) are supported */
static inline int _lcd_format_get(rt_uint8_t pixel_format)
{
    switch(pixel_format)
    {
    case RTGRAPHIC_PIXEL_FORMAT_RGB565:
        return DISP_FORMAT_RGB_565;
    case RTGRAPHIC_PIXEL_FORMAT_ARGB888:
        return DISP_FORMAT_ARGB_8888;
    default:
        return -1;
    }
}

/* Add the first layer, then enable the interrupt */
static rt_err_t rt_lcd_init(rt_device_t dev)
{
    lcd_device_t lcd_drv = (lcd_device_t)dev;
    int format;

    RT_ASSERT(lcd_drv != RT_NULL);

    format = _lcd_format_get(lcd_drv->lcd_info.pixel_format);
    if (format < 0)
    {
        rt_kprintf("lcd init faile pixel_format:%d\n", lcd_drv->lcd_info.pixel_format);
        return -RT_ERROR;
    }
    //config layer info
    memset(&layer_cfg, 0, sizeof(layer_cfg));
    layer_cfg.info.b_trd_out = 0;
    layer_cfg.channel = 0;
    layer_cfg.layer_id = 0;
    layer_cfg.info.fb.format = format;
    layer_cfg.info.fb.crop.x = 0;
    layer_cfg.info.fb.crop.y = 0;
    layer_cfg.info.fb.crop.width = LCD_LAYER_WIDTH;
    layer_cfg.info.fb.crop.height = LCD_LAYER_HEIGHT;
    layer_cfg.info.fb.crop.width = layer_cfg.info.fb.crop.width << 32;
    layer_cfg.info.fb.crop.height = layer_cfg.info.fb.crop.height << 32;
    layer_cfg.info.fb.align[0] = 4;
    layer_cfg.info.mode = 0; // LAYER_MODE_BUFFER
    layer_cfg.info.alpha_mode = 1;
    layer_cfg.info.alpha_value = 255;
    layer_cfg.info.zorder = 0;
    layer_cfg.info.screen_win.x = 0;
    layer_cfg.info.screen_win.y = 0;
    layer_cfg.info.screen_win.width = LCD_LAYER_WIDTH;
    layer_cfg.info.screen_win.height = LCD_LAYER_HEIGHT;

    layer_cfg.info.fb.size[0].width = LCD_LAYER_WIDTH;
    layer_cfg.info.fb.size[0].height = LCD_LAYER_HEIGHT;
    layer_cfg.info.fb.size[1].width = LCD_LAYER_WIDTH;
    layer_cfg.info.fb.size[1].height = LCD_LAYER_HEIGHT;
    layer_cfg.info.fb.size[2].width = LCD_LAYER_WIDTH;
    layer_cfg.info.fb.size[2].height = LCD_LAYER_HEIGHT;

    layer_cfg.info.fb.addr[0] = (size_t)lcd_drv->framebuffer_phy;

    /* INTERLEAVED */
    layer_cfg.info.fb.addr[0] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT / 3 * 0);
    layer_cfg.info.fb.addr[1] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT / 3 * 1);
    layer_cfg.info.fb.addr[2] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT / 3 * 2);
    layer_cfg.info.fb.trd_right_addr[0] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * 3 / 2);
    layer_cfg.info.fb.trd_right_addr[1] = (int)(layer_cfg.info.fb.trd_right_addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT);
    layer_cfg.info.fb.trd_right_addr[2] = (int)(layer_cfg.info.fb.trd_right_addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * 3 / 2);

    layer_cfg.enable = 1;

    return 0;
}

extern void rt_hw_cpu_dcache_clean(void *addr, int size);

static rt_err_t rt_lcd_control(rt_device_t dev, int cmd, void *args)
{
    struct lcd_device * lcd_drv = (struct lcd_device *)dev;
    rt_uint32_t memcp_tick_tmp;

    switch (cmd)
    {
    case RTGRAPHIC_CTRL_RECT_UPDATE:
    {
        extern int disp_ioctl(int cmd, void *arg);
        unsigned long arg[6] = {0};
        int ret;

        layer_cfg.info.fb.addr[0] = (size_t)lcd_drv->framebuffer_phy;
        /* INTERLEAVED */
        layer_cfg.info.fb.addr[0] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT / 3 * 0);
        layer_cfg.info.fb.addr[1] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT / 3 * 1);
        layer_cfg.info.fb.addr[2] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT / 3 * 2);
        layer_cfg.info.fb.trd_right_addr[0] = (int)(layer_cfg.info.fb.addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * 3 / 2);
        layer_cfg.info.fb.trd_right_addr[1] = (int)(layer_cfg.info.fb.trd_right_addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT);
        layer_cfg.info.fb.trd_right_addr[2] = (int)(layer_cfg.info.fb.trd_right_addr[0] + LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * 3 / 2);
        arg[0] = DEFAULT_SCREEN;
        arg[1] = (unsigned long)&layer_cfg;
        arg[2] = 1;
        arg[3] = 0;
        ret = disp_ioctl(DISP_LAYER_SET_CONFIG, (void *)arg);
        if (0 != ret)
            rt_kprintf("fail to set layer cfg %d\n",ret);

        break;
    }
#ifdef GUIENGINE_USING_MULTI_LAYER
    case RTGRAPHIC_CTRL_CREATE_LAYER:
    {
        rtgui_layer_t layer = (rtgui_layer_t)args;
        struct layer_info info;
        int format, i, bf_size;
        void *_fb;
        struct debe_info *p_info = RT_NULL;

        if (layer == RT_NULL)
        {
            return -RT_ERROR;
        }
        for(i = 0; i < (sizeof(_debe_use) / sizeof(_debe_use[0])); i++)
        {
            if (_debe_use[i].buffer == RT_NULL)
            {
                p_info = &_debe_use[i];
                break;
            }
        }
        if (p_info == RT_NULL)
        {
            return -RT_ERROR;
        }
        //config layer info
        format = _lcd_format_get(layer->pixel_format);
        if (format < 0)
        {
            rt_kprintf("lcd create layer faile pixel_format:%d\n", layer->pixel_format);
            return -RT_ERROR;
        }
        bf_size = layer->bits_per_pixel * layer->width * layer->height;
        _fb = rt_malloc_align(bf_size, 32);
        if (_fb == RT_NULL)
        {
            rt_kprintf("lcd layer malloc fail\n");
            return -RT_ERROR;
        }
        memset(_fb, 0, layer->bits_per_pixel * layer->width * layer->height);
        memset(&info, 0, sizeof(struct layer_info));
        info.index = i+1;
        info.pipe = PIPE1;
        info.alpha_enable = 1;
        info.alpha_value = layer->alpha_value;
        info.x = layer->x;
        info.y = layer->y;
        info.width = layer->width;
        info.height = layer->height;
        info.format = format;
        info.type = LAYER_TYPE_RGB;
        info.buffer_addr = (rt_uint32_t)_fb;

        layer->user_data = p_info;

        p_info->index = info.index;
        p_info->buffer = _fb;
        p_info->bf_size = bf_size;

        //add layer
        tina_debe_layer_add(DEBE, &info);
        //show this layer
        tina_debe_layer_visible(DEBE, info.index);
        break;
    }
    case RTGRAPHIC_CTRL_UPDATE_LAYER:
    {
        rtgui_layer_t layer = (rtgui_layer_t)args;
        struct debe_info *p_info = RT_NULL;

        p_info = layer->user_data;
        memcpy(p_info->buffer, layer->buffer, p_info->bf_size);
        mmu_clean_dcache((rt_uint32_t)p_info->buffer, p_info->bf_size);
        tina_debe_coordinate_set(DEBE, p_info->index, layer->x, layer->y);
        tina_debe_alpha_set(DEBE, p_info->index, layer->alpha_value);
        break;
    }
    case RTGRAPHIC_CTRL_DELETE_LAYER:
    {
        rtgui_layer_t layer = (rtgui_layer_t)args;
        struct debe_info *p_info = RT_NULL;

        if ((layer == RT_NULL) || (layer->user_data))
        {
            return -RT_ERROR;
        }
        p_info = layer->user_data;
        tina_debe_layer_del(DEBE, p_info->index);
        rt_free_align(p_info->buffer);
        rt_memset(p_info, 0, sizeof(struct debe_info));
        layer->user_data = RT_NULL;
        break;
    }
#endif  /* GUIENGINE_USING_MULTI_LAYER */
    case RTGRAPHIC_CTRL_POWERON:
        break;
    case RTGRAPHIC_CTRL_POWEROFF:
        break;
    case RTGRAPHIC_CTRL_GET_INFO:
        memcpy(args, &lcd_drv->lcd_info, sizeof(struct rt_device_graphic_info));
        break;
    case FBIOGET_FSCREENINFO:
    {
#ifdef RT_USING_USERSPACE
        struct fb_fix_screeninfo *info = (struct fb_fix_screeninfo *)args;
        strncpy(info->id, "lcd", sizeof(info->id));
        info->smem_len    = LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * sizeof(rt_uint32_t);
        info->smem_start  = (size_t)lwp_map_user_phy(lwp_self(), RT_NULL, lcd_drv->framebuffer_phy,
            info->smem_len, 1);
		rt_kprintf("info->smem_start:%x\n",info->smem_start);
        info->line_length = LCD_LAYER_WIDTH * sizeof(rt_uint32_t);
#endif
    }
    case RTGRAPHIC_CTRL_SET_MODE:
        break;
    }
    return RT_EOK;
}

struct lcd_device * g_lcd = RT_NULL;

/* set up the 'lcd_device' and register it */
int rt_hw_lcd_init(void)
{
    static struct lcd_device _lcd_device;
    struct lcd_device * lcd_drv = &_lcd_device;
    rt_uint32_t *framebuffer = RT_NULL;

	g_lcd = lcd_drv;
    /* the content of tcon control registers can be loaded from a xml file ? */
    // _panel = load_config_from_xml();
    memset(lcd_drv, 0, sizeof(struct lcd_device));

    /* allocate the framebuffer, the front buffer and the back buffer */
    /* framebuffer */
    framebuffer = rt_pages_alloc (rt_page_bits(LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * sizeof(rt_uint32_t)));
    if (!framebuffer)
    {
        rt_kprintf("malloc framebuffer fail\n");
        goto out;
    }
    lcd_drv->framebuffer = framebuffer;
    lcd_drv->framebuffer_phy = (void*)((uint32_t)framebuffer - PV_OFFSET);
    memset(framebuffer, 0, sizeof(rt_uint32_t) * LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT);
    rt_hw_cpu_dcache_clean(lcd_drv->framebuffer, LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * sizeof(rt_uint32_t));

    /*
     * The semaphore is used for the synchronization between updating the
     * framebuffer and flushing the screen.
     */
    rt_sem_init(&lcd_drv->lcd_sem, "lcd_sem", 0, RT_IPC_FLAG_FIFO);

    /* the lcd device information defined by RT-Thread */
    lcd_drv->lcd_info.bits_per_pixel = 32;
    lcd_drv->lcd_info.pixel_format = RTGRAPHIC_PIXEL_FORMAT_ARGB888; /* should be coherent to adding layers */
    lcd_drv->lcd_info.framebuffer = (void *)framebuffer;
    lcd_drv->lcd_info.width = LCD_LAYER_WIDTH;
    lcd_drv->lcd_info.height = LCD_LAYER_HEIGHT;

    /* initialize device structure, the type of 'lcd' is 'rt_device' */
    lcd_drv->lcd.type = RT_Device_Class_Graphic;
    lcd_drv->lcd.init = rt_lcd_init;
    lcd_drv->lcd.open = RT_NULL;
    lcd_drv->lcd.close = RT_NULL;
    lcd_drv->lcd.control = rt_lcd_control;
    lcd_drv->lcd.user_data = (void *)&lcd_drv->lcd_info;

    /* register lcd device to RT-Thread */
    rt_device_register(&lcd_drv->lcd, "lcd", RT_DEVICE_FLAG_RDWR);

    /* the number of the lcd ? DEFAULT_SCREEN = 0 */
    lcd_drv->use_screen = DEFAULT_SCREEN;

#ifdef PKG_USING_GUIENGINE
    extern rt_err_t rtgui_graphic_set_device(rt_device_t device);
    rtgui_graphic_set_device((rt_device_t)lcd_drv);
#endif

    rt_lcd_init(&lcd_drv->lcd);
    return RT_EOK;

out:
    if (framebuffer)
        rt_free_align(framebuffer);

    return RT_ERROR;
}
//INIT_COMPONENT_EXPORT(rt_hw_lcd_init);
INIT_APP_EXPORT(rt_hw_lcd_init); //调整初始化顺序，确保文件系统已经启动成功

static int lcd_draw_point(int args,char *argv[])
{
    struct lcd_device *lcd = g_lcd;

    rt_kprintf("lcd_draw_point\n");

    int x = 0;
    int y = 0;
    int i, k;

    x = atoi(argv[1]);
    y = atoi(argv[2]);

    if(x >= LCD_LAYER_WIDTH)   x = LCD_LAYER_WIDTH  - 1;
    if(y >= LCD_LAYER_HEIGHT)  y = LCD_LAYER_HEIGHT - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    rt_kprintf("Darw point is x:%d,y:%d\n",x,y);

    // memset(lcd->framebuffer, 0, sizeof(rt_uint32_t) * LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT);

    for (i = y - 100; i < y + 100; i++)
    {
        if (i < 0) continue;
        if (i >= LCD_LAYER_HEIGHT) break;
        for (k = x - 100; k < x + 100; k++)
        {
            if (k < 0) continue;
            if (k >= LCD_LAYER_WIDTH) break;

            *((uint32_t *)lcd->framebuffer + LCD_LAYER_WIDTH * i + k) = 0xff00ff00;
        }
    }

    *((uint32_t *)lcd->framebuffer + LCD_LAYER_WIDTH * y + x) = 0xffff0000;
    // *((uint32_t *)lcd->framebuffer + LCD_LAYER_WIDTH * y + x + 2) = 0xff00ff00;

    rt_hw_cpu_dcache_clean(lcd->framebuffer, LCD_LAYER_WIDTH * LCD_LAYER_HEIGHT * sizeof(rt_uint32_t));
    rt_lcd_control((rt_device_t)g_lcd, RTGRAPHIC_CTRL_RECT_UPDATE, RT_NULL);

    return 0;
}
MSH_CMD_EXPORT(lcd_draw_point,draw a point on lcd);

#endif
