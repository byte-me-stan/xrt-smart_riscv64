/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-29     JasonHu      first version
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <rtthread.h>

#include <typedef.h>
#include <kapi.h>
#include <init.h>
#include <blkpart.h>
#include <sdmmc/hal_sdhost.h>
#include <sdmmc/card.h>
#include <sdmmc/sys/sys_debug.h>
#include <sdmmc/sdmmc.h>
#include <sdmmc/sd_test.h>
#include <drv_sdmmc.h>

#define DBG_SECTION "drv-sdmmc"
#include <rtdbg.h>

#ifdef CONFIG_SUPPORT_SDMMC_CACHE
#include "sdmmc_cache.h"
#endif

#define DETECT_BY_GPIO

#ifndef CONFIG_SDC_DMA_BUF_SIZE
#define SDC_ALIGN_DMA_BUF_SIZE  (64*1024)
#else
#define SDC_ALIGN_DMA_BUF_SIZE  (CONFIG_SDC_DMA_BUF_SIZE * 1024)
#endif

#define SDXC_MAX_TRANS_LEN      SDC_ALIGN_DMA_BUF_SIZE

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(size, align)      ((size) & ~((align) - 1))
#endif

#ifndef MIN
#define MIN(a, b) (a > b ? b : a)
#endif

static rt_err_t sdmmc_init(rt_device_t dev)
{
    int ret = -1;

    struct dev_sdmmc *dev_priv = (struct dev_sdmmc *)dev->user_data;
    int host_id = dev_priv->host_id;

    dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
    int32_t internal_card = 0x00;

    SDC_InitTypeDef sdc_param = {0};
    sdc_param.debug_mask = (ROM_INF_MASK | \
                            ROM_WRN_MASK | ROM_ERR_MASK | ROM_ANY_MASK);

    esCFG_GetKeyValue("sdcard_global", "internal_card", (int32_t *)&internal_card, 1);

    if(((internal_card >> host_id) & 0x01) == 1)
    {
        sdc_param.cd_mode = CARD_ALWAYS_PRESENT;
        LOG_D("cd_mode CARD_ALWAYS_PRESENT!");
    }
    else
    {
#ifndef DETECT_BY_GPIO
    sdc_param.cd_mode = CARD_ALWAYS_PRESENT;
#else
    sdc_param.cd_mode = CARD_DETECT_BY_GPIO_IRQ;
#endif
    }
    sdc_param.cd_cb = &card_detect;
    sdc_param.dma_use = 1;

    if (mmc_test_init(host_id, &sdc_param, 1))
    {
        dev->flag &= ~RT_DEVICE_FLAG_ACTIVATED;
        LOG_E("init sdmmc failed!");
        return ret;
    }
    LOG_D("host_id =%d!", host_id);

    /* wait timeout to sync with sdmmc init done */
    int mdelay = 500;
    while (!hal_sdc_init_timeout() && mdelay > 0)
    {
        rt_thread_mdelay(50);
        mdelay -= 50;
    }
    return 0;
}

static rt_size_t sdmmc_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    int err = -1;
    ssize_t ret, sz = 0;
    struct rt_device_blk_geometry *geometry;
    unsigned long long total_bytes;
    char *sector_buf = NULL;
    uint8_t *data = buffer;

    struct dev_sdmmc *dev_priv = (struct dev_sdmmc *)dev->user_data;
    struct mmc_card *card = mmc_card_open(dev_priv->host_id);
    if (card == NULL)
    {
        LOG_E("mmc open fail");
        return -EIO;
    }

    if (size == 0)
    {
        return 0;
    }

    geometry = &dev_priv->geometry;

    total_bytes = geometry->bytes_per_sector * geometry->sector_count;
    size *= geometry->bytes_per_sector;  /* sector to size */
    pos *= geometry->bytes_per_sector;

    if (pos >= total_bytes)
    {
        LOG_E("read offset %lu over part size %llu", pos, total_bytes);
        return 0;
    }

    if (pos + size > total_bytes)
    {
        LOG_E("over limit: offset %lu + size %lu over %llu",
               pos, size, total_bytes);
    }

    size = MIN(total_bytes - pos, size);
    LOG_D("off 0x%lx size %lu", pos, size);

    if (pos % geometry->bytes_per_sector || size % geometry->bytes_per_sector)
    {
        sector_buf = rt_malloc(geometry->bytes_per_sector);
        if (!sector_buf)
        {
            return -ENOMEM;
        }
        rt_memset(sector_buf, 0, geometry->bytes_per_sector);
    }

    /**
     * Step 1:
     * read the beginning data that not align to block size
     */
    if (pos % geometry->bytes_per_sector)
    {
        uint32_t addr, poff, len;

        addr = ALIGN_DOWN(pos, geometry->bytes_per_sector);
        poff = pos - addr;
        len = MIN(geometry->bytes_per_sector - poff, size);

        LOG_D("offset %lu not align %u, fix them before align read",
                 pos, geometry->bytes_per_sector);
        LOG_D("step1: read page data from addr 0x%x", addr);
        ret = mmc_block_read(card, sector_buf, addr / 512, 1);
        if (ret != 0)
        {
            goto err;
        }

        LOG_D("step2: copy page data to buf with page offset 0x%x and len %u",
                 poff, len);
        rt_memcpy(data, sector_buf + poff, len);

        pos += len;
        buffer += len;
        sz += len;
        size -= len;
    }

    /**
     * Step 2:
     * read data that align to block size
     */
    while (size >= geometry->bytes_per_sector)
    {
        if (size < SDXC_MAX_TRANS_LEN)
        {
            ret = mmc_block_read(card, data, pos / 512, size / geometry->bytes_per_sector);
            if (ret)
            {
                goto err;
            }
            pos += geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
            data += geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
            sz += geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
            size -= geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
        }
        else
        {
            while (size > SDXC_MAX_TRANS_LEN)
            {
                ret = mmc_block_read(card, data, pos / 512, SDXC_MAX_TRANS_LEN / 512);
                if (ret)
                {
                    goto err;
                }
                size -= SDXC_MAX_TRANS_LEN;
                data += SDXC_MAX_TRANS_LEN;
                pos += SDXC_MAX_TRANS_LEN;
                sz += SDXC_MAX_TRANS_LEN;
            }
        }
    }

    /**
     * Step 3:
     * read the last data that not align to block size
     */
    if (size)
    {
        LOG_D("last size %u not align %u, read them", size, geometry->bytes_per_sector);

        LOG_D("step1: read page data from addr 0x%lx", pos);
        ret = mmc_block_read(card, sector_buf, pos / 512, 1);
        if (ret != 0)
        {
            goto err;
        }

        LOG_D("step2: copy page data to buf with page with len %u", size);
        memcpy(data, sector_buf, size);
        sz += size;
    }

    ret = 0;
    goto out;

err:
    LOG_E("read failed - %d", (int)ret);
out:
    if (sector_buf)
    {
        rt_free(sector_buf);
    }
    mmc_card_close(dev_priv->host_id);

    return ret ? ret / geometry->bytes_per_sector: sz / geometry->bytes_per_sector;
}

static rt_err_t sdmmc_open(rt_device_t dev, rt_uint16_t oflag)
{
    return 0;
}

static rt_err_t sdmmc_close(rt_device_t dev)
{
    return 0;
}

static rt_size_t sdmmc_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    int err = -1;
    ssize_t ret, sz = 0;
    struct rt_device_blk_geometry *geometry;
    unsigned long long total_bytes;
    char *sector_buf = NULL;
    uint8_t *data = (uint8_t *)buffer;

    struct dev_sdmmc *dev_priv = (struct dev_sdmmc *)dev->user_data;
    struct mmc_card *card = mmc_card_open(dev_priv->host_id);
    if (card == NULL)
    {
        LOG_E("mmc open fail");
        return -EIO;
    }

    if (size == 0)
    {
        return 0;
    }

    geometry = &dev_priv->geometry;

    total_bytes = ((uint64_t)geometry->bytes_per_sector) * geometry->sector_count;
    size *= geometry->bytes_per_sector;  /* sector to size */
    pos *= geometry->bytes_per_sector;

    if (pos >= total_bytes)
    {
        LOG_E("read offset %lu over part size %llu", pos, total_bytes);
        return 0;
    }

    if (pos + size > total_bytes)
    {
        LOG_E("over limit: offset %lu + size %lu over %llu",
               pos, size, total_bytes);
    }

    size = MIN(total_bytes - pos, size);
    LOG_D("off 0x%lx size %lu", pos, size);

    if (pos % geometry->bytes_per_sector || size % geometry->bytes_per_sector)
    {
        sector_buf = rt_malloc(geometry->bytes_per_sector);
        if (!sector_buf)
        {
            return -ENOMEM;
        }
        memset(sector_buf, 0, geometry->bytes_per_sector);
    }

    /**
     * Step 1:
     * read the beginning data that not align to block size
     */
    if (pos % geometry->bytes_per_sector)
    {
        uint32_t addr, poff, len;

        addr = ALIGN_DOWN(pos, geometry->bytes_per_sector);
        poff = pos - addr;
        len = MIN(geometry->bytes_per_sector - poff, size);

        LOG_D("offset %lu not align %u, fix them before align read",
                 pos, geometry->bytes_per_sector);
        LOG_D("step1: read page data from addr 0x%x", addr);
        ret = mmc_block_write(card, sector_buf, addr / 512, 1);
        if (ret != 0)
        {
            goto err;
        }

        LOG_D("step2: copy page data to buf with page offset 0x%x and len %u",
                 poff, len);
        rt_memcpy(data, sector_buf + poff, len);

        pos += len;
        buffer += len;
        sz += len;
        size -= len;
    }

    /**
     * Step 2:
     * read data that align to block size
     */
    while (size >= geometry->bytes_per_sector)
    {
        if (size < SDXC_MAX_TRANS_LEN)
        {
            ret = mmc_block_write(card, data, pos / 512, size / geometry->bytes_per_sector);
            if (ret)
            {
                goto err;
            }
            pos += geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
            data += geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
            sz += geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
            size -= geometry->bytes_per_sector * (size / geometry->bytes_per_sector);
        }
        else
        {
            while (size > SDXC_MAX_TRANS_LEN)
            {
                ret = mmc_block_write(card, data, pos / 512, SDXC_MAX_TRANS_LEN / 512);
                if (ret)
                {
                    goto err;
                }
                size -= SDXC_MAX_TRANS_LEN;
                data += SDXC_MAX_TRANS_LEN;
                pos += SDXC_MAX_TRANS_LEN;
                sz += SDXC_MAX_TRANS_LEN;
            }
        }
    }

    /**
     * Step 3:
     * read the last data that not align to block size
     */
    if (size)
    {
        LOG_D("last size %u not align %u, read them", size, geometry->bytes_per_sector);

        LOG_D("step1: read page data from addr 0x%llx", pos);
        ret = mmc_block_write(card, sector_buf, pos / 512, 1);
        if (ret != 0)
        {
            goto err;
        }

        LOG_D("step2: copy page data to buf with page with len %u", size);
        memcpy(data, sector_buf, size);
        sz += size;
    }

    ret = 0;
    goto out;

err:
    LOG_E("read failed - %d", (int)ret);
out:
    if (sector_buf)
    {
        rt_free(sector_buf);
    }
    mmc_card_close(dev_priv->host_id);

    return ret ? ret / geometry->bytes_per_sector: sz / geometry->bytes_per_sector;
}

static rt_err_t sdmmc_control(rt_device_t dev, int cmd, void *args)
{
    int ret = -RT_ERROR;
    struct rt_device_blk_geometry *geometry;
    int flag = 0;
    if (!dev)
    {
        return -EINVAL;
    }

    struct dev_sdmmc *dev_priv = (struct dev_sdmmc *)dev->user_data;
    struct mmc_card *card = mmc_card_open(dev_priv->host_id);
    if (!card)
    {
        return ret;
    }

    switch (cmd)
    {
        case BLOCK_DEVICE_CMD_ERASE_ALL:
            break;
        case BLOCK_DEVICE_CMD_ERASE_SECTOR:
            break;
        case BLOCK_DEVICE_CMD_GET_TOTAL_SIZE:
            *(uint64_t *)args = card->csd.capacity * 1024;
            ret = 0;
            break;
        case BLOCK_DEVICE_CMD_GET_PAGE_SIZE:
            *(uint32_t *)args = 512;
            ret = 0;
            break;
        case BLOCK_DEVICE_CMD_GET_BLOCK_SIZE:
            *(uint32_t *)args = 512;
            ret = 0;
            break;
        case RT_DEVICE_CTRL_BLK_GETGEOME:
            geometry = (struct rt_device_blk_geometry *)args;
            rt_memset(geometry, 0, sizeof(struct rt_device_blk_geometry));
            geometry->block_size = 512;
            geometry->bytes_per_sector = 512;
            geometry->sector_count = (card->csd.capacity * 1024) / geometry->bytes_per_sector;
            LOG_D("[sdmmc] getgeome: bytes_per_sector:%ld, block_size:%ld, sector_count:%ld",
                geometry->bytes_per_sector, geometry->block_size, geometry->sector_count);
            ret = RT_EOK;
            break;

        default:
            break;
    }

    mmc_card_close(dev_priv->host_id);
    return ret;
}

static int init_sdmmc_device(rt_device_t device, void *usr_data, char *dev_name)
{
    int ret = -1;

    device = rt_device_create(RT_Device_Class_Block, 0);
    if (!device)
    {
        return ret;
    }
    device->init = sdmmc_init;
    device->open = sdmmc_open;
    device->close = sdmmc_close;
    device->read = sdmmc_read;
    device->write = sdmmc_write;
    device->control = sdmmc_control;
    device->user_data = usr_data;

    ret = rt_device_register(device, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK)
    {
        rt_device_destroy(device);
        return ret;
    }

    int sdmmc_blkpart_init(const char *name);
    ret = sdmmc_blkpart_init(dev_name);

    return ret;
}

static struct dev_sdmmc dev_sdmmc[SDMMC_CARD_NR];

static int driver_sdmmc_init(void)
{
    int ret = -1;
    int i = 0;
    rt_device_t device0, device1;
    int32_t used_card_no = 0x01;
    char name[12];

    ret = esCFG_GetKeyValue("sdcard_global", "used_card_no", (int32_t *)&used_card_no, 1);
    if (ret)
    {
        used_card_no = 0x00;
        LOG_E("get card no failed, card no: %d", used_card_no);
        return ret;
    }

    for (i = 0; i < SDMMC_CARD_NR; ++i)
    {
        if (((used_card_no >> i) & 0x01) == 0)
        {
            continue;
        }

        rt_sprintf(name, "sdmmc%d", i);
        dev_sdmmc[i].host_id = i;
        ret = init_sdmmc_device(device0, (void *)&dev_sdmmc[i], name);
    }
    return ret;
}

#ifdef RT_USING_COMPONENTS_INIT
INIT_DEVICE_EXPORT(driver_sdmmc_init);
#endif
