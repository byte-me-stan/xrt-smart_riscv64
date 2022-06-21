/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-10-20     JasonHu       first version
 */

#include <rtconfig.h>

#ifdef BSP_USING_DIRECT_UART
#include <sbi.h>

/**
* This function is used by rt_kprintf to display a string on console.
*
* @param str the displayed string
*/
void rt_hw_console_output(const char *str)
{
    while (*str) {
        sbi_console_putchar(*str++);
    }
}

#endif  /* BSP_USING_DIRECT_UART */
