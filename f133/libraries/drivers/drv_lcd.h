/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-04-12     RT-Thread    the first version
 */

#ifndef __DRV_LCD_H__
#define __DRV_LCD_H__

#define REVERSE_X       (1<<0)
#define REVERSE_Y       (1<<1)
#define REVERSE_MODE    (1<<2)

// enum lcd_orientation
// {
//     LCD_LANDSCAPE_MODE = 0,                 //ctp_flag = 5;            //ctp_flag =6 ;            //ctp_flag = 6; 
//     LCD_LANDSCAPE_FLIPS_Y_MODE = 1,         //ctp_flag = 4 ;           //ctp_flag = 7;            //ctp_flag = 7;
//     LCD_LANDSCAPE_FLIPS_X_MODE = 2,         // ctp_flag = 7;           //ctp_flag = 4;            //ctp_flag = 4;
//     LCD_LANDSCAPE_FLIPS_X_AND_Y_MODE = 3,   //ctp_flag = 6;            //ctp_flag = 5;
//     LCD_PORTRAIT_MODE = 4,                  //ctp_flag = 1;             //ctp_flag = 3;            //ctp_flag = 3; 
//     LCD_PORTRAIT_FLIPS_Y_MODE = 5,          //ctp_flag = 0;             //ctp_flag = 2;
//     LCD_PORTRAIT_FLIPS_X_MODE = 6,          //ctp_flag = 2;              //ctp_flag = 0;
//     LCD_PORTRAIT_FLIPS_X_AND_Y_MODE = 7,    //ctp_flag = 3;             //ctp_flag = 1;
// };
int rt_hw_lcd_init(void);

#define FBIOGET_VSCREENINFO     0x4600
#define FBIOPUT_VSCREENINFO     0x4601
#define FBIOGET_FSCREENINFO     0x4602
#define FBIOGETCMAP             0x4604
#define FBIOPUTCMAP             0x4605
#define FBIOPAN_DISPLAY         0x4606
#define FBIO_CURSOR             0x4608
/* #define FBIOGET_MONITORSPEC  0x460C */
/* #define FBIOPUT_MONITORSPEC  0x460D */
/* #define FBIOSWITCH_MONIBIT   0x460E */
#define FBIOGET_CON2FBMAP       0x460F
#define FBIOPUT_CON2FBMAP       0x4610
#define FBIOBLANK               0x4611          /* arg: 0 or vesa level + 1 */
#define FBIOGET_VBLANK          0x4612
#define FBIO_ALLOC              0x4613
#define FBIO_FREE               0x4614
#define FBIOGET_GLYPH           0x4615
#define FBIOGET_HWCINFO         0x4616
#define FBIOPUT_MODEINFO        0x4617
#define FBIOGET_DISPINFO        0x4618
#define FBIO_WAITFORVSYNC       0x4620

struct fb_bitfield
{
    uint32_t offset;        /* beginning of bitfield */
    uint32_t length;        /* length of bitfield */
    uint32_t msb_right;     /* != 0 : Most significant bit is */ 
    /* right */
};

struct fb_var_screeninfo
{
    uint32_t xres;
    uint32_t yres;

    uint32_t bits_per_pixel;

    struct fb_bitfield red;     /* bitfield in fb mem if true color, */
    struct fb_bitfield green;   /* else only length is significant */
    struct fb_bitfield blue;
    struct fb_bitfield transp;  /* transparency         */
};


struct fb_fix_screeninfo
{
    char id[16];
    unsigned long smem_start;
    uint32_t smem_len;

    uint32_t line_length;
};

struct lcd_info
{
    struct rt_device_graphic_info graphic;
    struct fb_fix_screeninfo screen;
};

#endif
