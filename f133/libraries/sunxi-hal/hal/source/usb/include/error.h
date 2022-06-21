/*
********************************************************************************************************************
*                                              usb host driver
*
*                              (c) Copyright 2007-2010, javen.China
*                                       All Rights Reserved
*
* File Name     : error.h
*
* Author        : javen
*
* Version       : 2.0
*
* Date          : 2010.03.02
*
* Description   : ��������ֵ˵����
*
* History       :
*
********************************************************************************************************************
*/
#define  USB_ERR_SUCCESS                    0       /* �ɹ�             */
#define  USB_ERR_UNKOWN_ERROR               -1      /* δ֪����         */

/* ���������� */
#define  USB_ERR_BAD_ARGUMENTS              1       /* ��������         */
#define  USB_ERR_DATA_OVERFLOW              2       /* �������         */

/* Ӳ��������� */
#define  USB_ERR_IO_DEVICE_OFFLINE          500     /* �豸������       */
#define  USB_ERR_IO_DEVICE_DIEAD            501     /* �豸������       */
#define  USB_ERR_IO_DEVICE_BUSY             502     /* �豸������       */
#define  USB_ERR_COMMAND_NEED_RETRY         503     /* ������Ҫ�ط�     */
#define  USB_ERR_COMMAND_SEND_FAILED        504     /* �����ʧ��     */
#define  USB_ERR_COMMAND_EXECUTE_FAILED     505     /* ����ִ��ʧ��     */
#define  USB_ERR_RESET_POERT_FAILED         506     /* reset�˿�ʧ��    */
#define  USB_ERR_UNKOWN_DEVICE              507     /* δ֪�豸         */
#define  USB_ERR_DEVICE_PROBE_FAILED        508     /* �豸��ʼ��ʧ��   */
#define  USB_ERR_DEVICE_REMOVE_FAILED       509     /* �豸�Ƴ�ʧ��     */

#define  USB_ERR_MEDIA_NOT_PRESENT          510     /* ����û��׼����   */
#define  USB_ERR_NOT_SUPPORT_COMMAND        511     /* ��֧�ֵ�����     */


/* ����ϵͳ��� */
#define  USB_ERR_CREATE_SIME_FAILED         1000    /* �ź�������ʧ��   */
#define  USB_ERR_MALLOC_FAILED              1001    /* �ڴ����ʧ��     */
#define  USB_ERR_CREATE_THREAD_FAILED       1002    /* �����߳�ʧ��     */
#define  USB_ERR_REG_BLK_DEV_FAILED         1003    /* ע����豸ʧ��   */
#define  USB_ERR_CREATE_TIMER_FAILED        1004    /* ����timerʧ��    */

/* USB */
#define  USB_ERR_ALLOC_URB_FAILED           2000    /* ����URBʧ��      */


