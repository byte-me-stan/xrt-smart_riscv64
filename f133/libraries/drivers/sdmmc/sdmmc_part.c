/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-11-11     JasonHu      first version
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <rtthread.h>
#include <dfs_file.h>

#include <drv_sdmmc.h>
#include <blkpart.h>
#include <mbr.h>
#include <sdmmc/hal_sdhost.h>
#include <sdmmc/card.h>
#include <sdmmc/sdmmc.h>

#define DBG_SECTION "drv-sdmmc"
#include <rtdbg.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))
#endif

#define ALIGN_DOWN(x, a) __ALIGN_KERNEL((x) - ((a) - 1), (a))
#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

static struct blkpart sdmmcblk;

struct syspart
{
    char name[MAX_BLKNAME_LEN];
    u32 offset;
    u32 bytes;
};
static const struct syspart syspart[] =
{
};

static struct part all_part[16];

static void show_mbr_part(void *mbr_buf)
{
    int i = 0;

    sunxi_mbr_t *pMBR = (sunxi_mbr_t *)mbr_buf;

    LOG_I("MBR: parts:%d", pMBR->PartCount);
    LOG_I("classname            name     size   offset");
    for (i = 0; i < pMBR->PartCount; i++)
    {
        unsigned int offset      = pMBR->array[i].addrlo;
        unsigned int partsize    = pMBR->array[i].lenlo;
        unsigned int secsize     = 512;

        LOG_I("%8s %16s %8x %8x",
              (const char *)pMBR->array[i].classname, (const char *)pMBR->array[i].name,
              partsize, offset);
    }
}

int mbr_part_cnt(void *mbr_buf)
{
    sunxi_mbr_t *pMBR = (sunxi_mbr_t *)mbr_buf;
    return pMBR->PartCount;
}

static int sdmmc_get_mbr(rt_device_t dev, char *buf, int len)
{
    int ret = 0;
    if (len < MBR_SIZE)
    {
        LOG_E("buf too small for mbr");
        return -EINVAL;
    }

    LOG_D("read mbr from 0x%x", MBR_START_ADDRESS);
    ret = dev->read(dev, MBR_START_ADDRESS / 512, buf, MBR_SIZE / 512);
    return ret < 0 ? -EIO : 0;
}

static int register_part(rt_device_t dev, struct part *part)
{
    int ret = -1;
    struct rt_device *device;

    device = rt_device_create(RT_Device_Class_Block, 0);
    if (!device)
    {
        return ret;
    }

    device->read = part_read;
    device->write = part_erase_without_write;
    device->control = part_control;
    device->user_data = part;

    ret = rt_device_register(device, part->name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        rt_free(device);
        return ret;
    }
    return 0;
}

static int unregister_part(rt_device_t dev)
{
    int ret = -1;
    ret = rt_device_unregister(dev);
    if (ret != 0)
    {
        return ret;
    }
    rt_device_destroy(dev);
    return 0;
}

static int register_blk_device(rt_device_t dev)
{
    int ret = -1, index = 0;
    char *mbr_buf;
    struct mbr_part *mbr_part;
    struct part *part;
    unsigned int offset = 0;
    int i = 0;

    mbr_buf = rt_malloc(MBR_SIZE);
    if (!mbr_buf)
    {
        ret = -ENOMEM;
        goto err;
    }
    rt_memset(mbr_buf, 0, MBR_SIZE);

    dev->control(dev, BLOCK_DEVICE_CMD_GET_PAGE_SIZE, &sdmmcblk.page_bytes);
    dev->control(dev, BLOCK_DEVICE_CMD_GET_TOTAL_SIZE, &sdmmcblk.total_bytes);
    dev->control(dev, BLOCK_DEVICE_CMD_GET_BLOCK_SIZE, &sdmmcblk.blk_bytes);
    sdmmcblk.dev = dev;

    LOG_D("total_bytes = %ld", sdmmcblk.total_bytes);
    LOG_D("blk_bytes = %d", sdmmcblk.blk_bytes);
    LOG_D("page_bytes = %d", sdmmcblk.page_bytes);

    show_mbr_part(mbr_buf);
    ret = mbr_part_cnt(mbr_buf);
    if (ret < 0)
    {
        LOG_E("get part count from mbr failed");
        goto err;
    }

    LOG_I("part cnt = %d", ret);
    sdmmcblk.n_parts = ret + ARRAY_SIZE(syspart);
    LOG_I("parts size = %x", sizeof(struct part) * sdmmcblk.n_parts);
    sdmmcblk.parts = rt_malloc(sizeof(struct part) * sdmmcblk.n_parts);
    if (sdmmcblk.parts == NULL)
    {
        LOG_E("allocate part array failed.");
        goto err;
    }
    rt_memset(sdmmcblk.parts, 0, sizeof(struct part) * sdmmcblk.n_parts);

    for (index = 0; index < ARRAY_SIZE(syspart); index++)
    {
        part = &sdmmcblk.parts[index];
        part->blk = &sdmmcblk;
        part->bytes = syspart[index].bytes;
        part->off = syspart[index].offset;
        rt_snprintf(part->name, MAX_BLKNAME_LEN, "%s", syspart[index].name);
        offset += part->bytes;
    }

    MBR *pMBR = (MBR *)mbr_buf;

    for (i = 0; i < pMBR->PartCount; i++)
    {
        part = &sdmmcblk.parts[index++];
        part->blk = &sdmmcblk;
        part->bytes = pMBR->array[i].lenlo;
        part->off = pMBR->array[i].addrlo;
        rt_snprintf(part->name, MAX_BLKNAME_LEN, "%s", pMBR->array[i].name);
        offset += part->bytes;
    }

    sdmmcblk.parts[--index].bytes = sdmmcblk.total_bytes - offset;

    for (index = 0; index < sdmmcblk.n_parts; index++)
    {
        part = &sdmmcblk.parts[index];
        if (part->bytes % sdmmcblk.blk_bytes)
        {
            LOG_E("part %s with bytes %u should align to block size %u",
                  part->name, part->bytes, sdmmcblk.blk_bytes);
        }
        else
        {
            register_part(dev, part);
        }
    }
    blkpart_add_list(&sdmmcblk);
err:
    rt_free(mbr_buf);
    return ret;
}

static int unregister_blk_device(rt_device_t dev)
{
    int ret = -1;

    rt_device_t part_dev = rt_device_find(SDMMC_DEV_NAME_DEF);
    if (!part_dev)
    {
        LOG_E("device not found");
        return ret;
    }

    ret = unregister_part(part_dev);
    if (ret != 0)
    {
        return ret;
    }
    blkpart_del_list(&sdmmcblk);
    rt_free(sdmmcblk.parts);
    return 0;
}

int sdmmc_blkpart_init(const char *sdmmc_device_name)
{
    int ret = -1;
    rt_device_t dev = rt_device_find(sdmmc_device_name);
    if (!dev)
    {
        LOG_E("the sdmmc device %s is not exist!", sdmmc_device_name);
        return ret;
    }

    if (dev->init)
    {
        ret = dev->init(dev);
    }
    return ret;
}

static int register_blk_bootcard_device(rt_device_t dev)
{
    int ret = -1, index = 0;
    char *mbr_buf;
    struct mbr_part *mbr_part;
    struct part *part;
    unsigned int offset = 0;
    int i = 0;
    /* define the bootcard's two part in front, do not use boot0card.bin which start from 16th sector */
    static const struct syspart syspartcard[] =
    {
        {"none",    0,              20*1024*1024},
        {"mbr",     20*1024*1024,   SUNXI_MBR_SIZE} /* 16K * 4 ,but only read one copie */
    };

    /*read the mbr*/
    mbr_buf = rt_malloc(syspartcard[1].bytes);
    if (!mbr_buf)
    {
        ret = -ENOMEM;
        goto err;
    }
    rt_memset(mbr_buf, 0, syspartcard[1].bytes);

    /* NOTICE: get block geometry fisrt time here, then you can read/write sdmmc. */
    struct dev_sdmmc *dev_sdmmc = (struct dev_sdmmc *)dev->user_data;
    if (dev->control(dev, RT_DEVICE_CTRL_BLK_GETGEOME, &dev_sdmmc->geometry) != RT_EOK)
    {
        LOG_E("device get geometry failed!");
        ret = -ENOSYS;
        goto err;
    }

    dev->control(dev, BLOCK_DEVICE_CMD_GET_PAGE_SIZE, &sdmmcblk.page_bytes);
    dev->control(dev, BLOCK_DEVICE_CMD_GET_TOTAL_SIZE, &sdmmcblk.total_bytes);
    dev->control(dev, BLOCK_DEVICE_CMD_GET_BLOCK_SIZE, &sdmmcblk.blk_bytes);

    sdmmcblk.dev = dev;/*sdmmc device*/

    /*read mbr*/
    ret = dev->read(dev, syspartcard[1].offset / 512, mbr_buf, syspartcard[1].bytes / 512);

    LOG_I("total_bytes = %lx", sdmmcblk.total_bytes);
    LOG_I("blk_bytes = %d", sdmmcblk.blk_bytes);
    LOG_I("page_bytes = %d", sdmmcblk.page_bytes);

    show_mbr_part(mbr_buf);
    ret = mbr_part_cnt(mbr_buf);
    if (ret < 0)
    {
        LOG_E("get part count from mbr failed");
        goto err;
    }

    LOG_I("part cnt = %d", ret);
    sdmmcblk.n_parts = ret + ARRAY_SIZE(syspartcard);
    sdmmcblk.parts = rt_malloc(sizeof(struct part) * sdmmcblk.n_parts);
    if (sdmmcblk.parts == NULL)
    {
        LOG_E("allocate part array failed.");
        goto err;
    }
    rt_memset(sdmmcblk.parts, 0, sizeof(struct part) * sdmmcblk.n_parts);

    for (index = 0; index < ARRAY_SIZE(syspartcard); index++)
    {
        part = &sdmmcblk.parts[index];
        part->blk = &sdmmcblk;
        part->bytes = syspartcard[index].bytes;
        part->off = syspartcard[index].offset;
        rt_snprintf(part->name, MAX_BLKNAME_LEN, "%s", syspartcard[index].name);
        offset += part->bytes;
    }

    sunxi_mbr_t *pMBR = (sunxi_mbr_t *)mbr_buf;

    for (i = 0; i < pMBR->PartCount; i++)
    {
        part = &sdmmcblk.parts[index++];
        part->blk = &sdmmcblk;
        part->bytes = pMBR->array[i].lenlo * sdmmcblk.page_bytes/*512*/;
        part->off = pMBR->array[i].addrlo * sdmmcblk.page_bytes/*512*/ + syspartcard[1].offset;
        rt_snprintf(part->name, MAX_BLKNAME_LEN, "%s", pMBR->array[i].name);
        offset += part->bytes;
    }
    /* modify final part bytes */
    sdmmcblk.parts[--index].bytes = sdmmcblk.total_bytes - offset;

    /* print all parts info */
    LOG_I("==== all parts info head ====");
    for (index = 0; index < sdmmcblk.n_parts; index++)
    {
        part = &sdmmcblk.parts[index];
#ifdef BSP_SDMMC_FIX_PART
        if (!rt_strcmp(part->name, "boot")) /* skip mbr */
        {
            part->off += SUNXI_MBR_SIZE;
            part->bytes = 50 * (1024 * 1024);   /* 50MB boot */
        }
        else if (!rt_strcmp(part->name, "UDISK")) /* update offset */
        {
            part->off = (part - 1)->off + (part - 1)->bytes;
            part->bytes = 100 * (1024 * 1024);  /* 100 MB udisk */
        }
#endif /* BSP_SDMMC_FIX_PART */
        LOG_I("part[%d]%20s: offset:%8lx bytes:%8lx",
            index,part->name, part->off, part->bytes);
    }
    LOG_I("==== all parts info tail ====");

    for (index = 0; index < sdmmcblk.n_parts; index++)
    {
        part = &sdmmcblk.parts[index];
        if (part->bytes % sdmmcblk.blk_bytes)
        {
            LOG_E("part %s with bytes %u should align to block size %u",
                  part->name, part->bytes, sdmmcblk.blk_bytes);
        }
        else
        {
            register_part(dev, part);
        }
    }
    blkpart_add_list(&sdmmcblk);

err:
    rt_free(mbr_buf);
    return ret;
}

void mount_sdmmc_filesystem(int card_id)
{
    int ret = -1, value = 0;
    int need_check = 0;
    int card_product_used = -1;

    ret = esCFG_GetKeyValue("target", "storage_type", (int32_t *)&value, 1);
    if (ret != 0)
    {
        return;
    }
    if (value == STORAGE_SDC0 && card_id == 0) {
        need_check = 1;
    } else if (value == STORAGE_SDC1 && card_id == 1){
        need_check = 1;
    } else {
        need_check = 0;
    }
    if (need_check)
    {
        rt_device_t dev = rt_device_find(SDMMC_DEV_NAME_DEF);
        register_blk_bootcard_device(dev);
    }
    return;
}

void unmount_sdmmc_filesystem(int card_id)
{
    int ret = -1, value = 0;
    int need_check = 0;
    int index;

    ret = esCFG_GetKeyValue("target", "storage_type", (int32_t *)&value, 1);
    if (value == STORAGE_SDC0 && card_id == 0) {
        need_check = 1;
    } else if (value == STORAGE_SDC1 && card_id == 1){
        need_check = 1;
    } else {
        need_check = 0;
    }

    /* umount default part */
    rt_device_t dev = rt_device_find(SDMMC_DEV_NAME_DEF);
    if (!dev)
    {
        LOG_E("the sdmmc device [sdmmc] is not exist!");
        return;
    }

    /* unregister_part for each part on this device */
    for (index = 0; index < sdmmcblk.n_parts; index++)
    {
        struct part *part = &sdmmcblk.parts[index];
        rt_device_t part_dev = rt_device_find(part->name);
        if (part_dev != RT_NULL)
        {
            unregister_part(part_dev);
        }
    }

    /* unregister blk dev */
    unregister_blk_device(dev);
    return;
}
