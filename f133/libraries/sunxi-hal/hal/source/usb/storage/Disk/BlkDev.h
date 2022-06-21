/*
********************************************************************************************************************
*                                              usb host driver
*
*                              (c) Copyright 2007-2010, javen.China
*                                       All Rights Reserved
*
* File Name     : UsbBlkDev.h
*
* Author        : javen
*
* Version       : 2.0
*
* Date          : 2010.03.02
*
* Description   :
*
* History       :
*
********************************************************************************************************************
*/
#ifndef  __BLKDEV_H__
#define  __BLKDEV_H__

#include  "usbh_disk_info.h"

#define  USB_BLK_DEV_MAGIC              0x5a13d099
#define  USB_BULK_DISK_MAX_NAME_LEN     32

typedef struct __UsbBlkDev
{
    unsigned int   last_lun;               //���Ϊ1����ʾ�����һ������
    unsigned int   Magic;                  /* ��ʾ�豸�Ƿ�Ϸ�                 */
    __mscLun_t *Lun;                /* sd������scsi device���е�����    */

    /* Disk information */
    unsigned int used;                     /* ���豸����                     */
    __dev_devop_t DiskOp;           /* �豸��������                     */

    /* Disk manager */
    void *DevParaHdle;      /* openʱ�ľ��                     */
    void *DevRegHdle;       /* regʱ�ľ��                      */

    unsigned int DevNo;                                /* ���豸��, ����host_id, target_id, lun ���   */
    unsigned char ClassName[USB_BULK_DISK_MAX_NAME_LEN]; /* �豸����, ��"disk"               */
    unsigned char DevName[USB_BULK_DISK_MAX_NAME_LEN];   /* ���豸��, ��"SCSI_DISK_000"      */

    unsigned int is_RegDisk;               /* �Ƿ�ע���disk�豸                           */
    unsigned int ErrCmdNr;                 /* test_unit_ready�ڼ�, δ֪����Ĵ���          */

    void *Extern;                   /* ��չ����, ��cd                               */

    usbh_disk_device_info_t device_info;
} __UsbBlkDev_t;

//------------------------------------------
//
//------------------------------------------
__UsbBlkDev_t *UsbBlkDevAllocInit(__mscLun_t *mscLun);
int UsbBlkDevFree(__UsbBlkDev_t *BlkDev);

void GetDiskInfo(__UsbBlkDev_t *BlkDev);
void ShutDown(__UsbBlkDev_t *BlkDev);

int UsbBlkDevReg(__UsbBlkDev_t *BlkDev, unsigned char *ClassName, unsigned int RegDisk);
int UsbBlkDevUnReg(__UsbBlkDev_t *BlkDev);


#endif   //__BLKDEV_H__


