/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 */

#include <rtthread.h>
#include <rthw.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    void rt_hw_uart_start_rx_thread();
    rt_hw_uart_start_rx_thread();
    printf("Hello RISC-V\n");
    printf("allwinner_d1-h board with rt-smart\n");
    while (1)
    {
	    rt_thread_mdelay(10000);
	    printf("%s : ok\n", __func__);
    }
    return 0;
}
