/*
********************************************************************************
*                                USB Hid Driver
*
*                (c) Copyright 2006-2010, All winners Co,Ld. 
*                        All Right Reserved 
*
* FileName		:  Hid_i.h
*
* Author		:  Javen
*
* Date			:  2010/06/02
*
* Description	:  Hid Driver�ж�USB�ӿ��豸������
*
* Others		:  NULL
*
* History:
*		<time> 			<author>	 <version >		<desc>
*	   2010.06.02		Javen			1.0			build this file 
*
********************************************************************************
*/
#ifndef  __HID_I_H__
#define  __HID_I_H__
#include <hal_osal.h>

//---------------------------------------------------------
//  Ԥ����
//---------------------------------------------------------
struct _HidRequest;
struct _HidDev;
struct _usbHidReport;

//----------------------------------------------------------------------
// 
//
//----------------------------------------------------------------------
/* input, output, feature */
#define USB_HID_MAX_FIELDS 		64
typedef struct _usbHidField{
	/* Field��; */
	unsigned int physical;				/* physical usage for this field 					*/
	unsigned int logical;				/* logical usage for this field 					*/
	unsigned int application;			/* application usage for this field 				*/
	usbHidUsage_t *usage;		/* usage table for this function 					*/
	unsigned int maxusage;				/* maximum usage index	 							*/
	unsigned int flags;				/* main-item flags (i.e. volatile,array,constant) 	*/
	unsigned int report_offset;		/* bit offset in the report 						*/
	unsigned int report_size;			/* size of this field in the report 				*/
	unsigned int report_count;			/* number of this field in the report 				*/
	unsigned int report_type;			/* (input,output,feature) 							*/
	unsigned int *value;				/* last known value(s) 								*/
	int logical_minimum;		/* ��С�߼�ֵ 										*/
	int logical_maximum;		/* ����߼�ֵ 										*/
	int physical_minimum;		/* ��С����ֵ 										*/
	int physical_maximum;		/* �������ֵ 										*/
	int unit_exponent;		/* ��λָ�� 										*/
	unsigned int unit;					/* ��λ 											*/

	/* Field���� */
	unsigned int Index;				/* ndex into report->field[] 						*/
	struct _usbHidReport *HidReport; /* field ������ HID report 					*/
}usbHidField_t;

#define  USB_HID_REPORT_TYPES		3  	/* ��������� 		*/
#define  USB_HID_REPORT_MAX_NUM		256	/* ����������� 	*/

/* �豸���涨�壬��input, output, feature��3�� */
typedef struct _usbHidReport{
	unsigned int Id;									/* id of this report 			*/
	unsigned int Type;									/* report type,  				*/

	unsigned int Maxfield;								/* maximum valid field index 	*/
	usbHidField_t *Field[USB_HID_MAX_FIELDS];	/* fields of the report 		*/

	unsigned int Size;									/* size of the report (bits) 	*/
}usbHidReport_t;

/* �豸�����б��� */
typedef struct _usbHidReportEnum{
	unsigned int numbered;   /* reprot�Ƿ���� */

    unsigned int ReportNum;  /* ��Ч��Report�ĸ��� */
	usbHidReport_t *Report[USB_HID_REPORT_MAX_NUM];
}usbHidReportEnum_t;

#define  USB_HID_GLOBAL_STACK_SIZE 			4
#define  USB_HID_COLLECTION_STACK_SIZE 		4
typedef struct _usbHidParser {
	usbHidGlobalItems_t global;
	usbHidGlobalItems_t global_stack[USB_HID_GLOBAL_STACK_SIZE];
	unsigned int  global_stack_ptr;

	usbHidLocalItems_t local;

	unsigned int collection_stack[USB_HID_COLLECTION_STACK_SIZE];
	unsigned int collection_stack_ptr;

	struct _HidDev *HidDev;
}usbHidParser_t;


/* hid�¼���hid DATA���ƫ���� */
typedef struct _usbHidEvnetExcursion{
	unsigned int BitOffset;
	unsigned int BitCount;
}usbHidEvnetExcursion_t;

//---------------------------------------------------------
// 
//---------------------------------------------------------

/* Hid device state */
typedef enum _HidDev_state{
	HID_DEV_OFFLINE= 0,			/* HidDev�Ѿ��γ� 		*/
	HID_DEV_ONLINE,				/* HidDev�Ѿ���� 		*/
	HID_DEV_DIED,				/* HidDev������ 		*/
	HID_DEV_RESET				/* HidDev���ڱ�reset 	*/
}HidDev_State_t;

/* USB Hid device type */
//#define  USB_HID_DEVICE_TYPE_UNKOWN		0x00	/* δ֪�豸 */
//#define  USB_HID_DEVICE_TYPE_KEYBOARD	0x01	/* ���� 	*/
//#define  USB_HID_DEVICE_TYPE_MOUSE		0x02	/* ��� 	*/

typedef int (* Hid_SoftReset)(struct _HidDev *HidDev);
typedef int (* Hid_ResetRecovery)(struct _HidDev *HidDev);
typedef int (* Hid_Transport)(struct _HidDev *HidDev, struct _HidRequest *HidReq);
typedef int (* Hid_StopTransport)(struct _HidDev *HidDev);

typedef int (* HidClientProbe)(struct _HidDev *);
typedef int (* HidClientRemove)(struct _HidDev *);

/* ����USB�ӿڵ���Ϣ */
typedef struct _HidDev{
	struct usb_host_virt_dev *pusb_dev;		/* mscDev ��Ӧ��Public USB Device 	*/
	struct usb_interface	 *pusb_intf;	/* Public usb interface 			*/

	/* device information */
	unsigned char InterfaceNo;						/* �ӿں� 							*/
	unsigned char SubClass; 							/* ���� 							*/
	unsigned char Protocol; 							/* ����Э�� 						*/
	unsigned int DevType;							/* �豸���� 						*/
	unsigned int DevNo; 							/* �豸�� hid �����еı��			*/

	/* device manager */
	HidDev_State_t State;					/* Dev��ǰ����������״̬ 			*/

	unsigned char *ReportDesc;						/* װ����Hid�豸��report������ 		*/
	unsigned int ReportSize;						/* report�������Ĵ�С 				*/

	usbHidCollectionItems_t *collection;		/* List of HID collections 				*/
	unsigned collection_size;					/* Number of allocated hid_collections 	*/
	unsigned maxcollection;						/* Number of parsed collections 		*/
	unsigned maxapplication;					/* Number of applications 				*/
	usbHidReportEnum_t HidReportEnum[USB_HID_REPORT_TYPES];		/* �豸�ı�����Ϣ 		*/

	/* transport */
	unsigned int CtrlIn; 							/* ctrl in  pipe					*/
	unsigned int CtrlOut; 							/* ctrl out pipe					*/
	unsigned int IntIn;							/* interrupt in pipe 				*/
	unsigned char  EpInterval;						/* int ����������ѯ�豸������   	*/
	unsigned int OnceTransferLength;				/* �ж�ep����������С 			*/

	unsigned int busy;								/* �����Ƿ����ڴ������� 			*/
	struct urb *CurrentUrb;					/* USB requests	 					*/
	hal_sem_t UrbWait;			/* wait for Urb done 				*/
	struct usb_ctrlrequest *CtrlReq;		/* control requests	 				*/

    /* USB�ӿڲ��� */
	Hid_SoftReset 	  SoftReset;			/* ��λ��ֻ����� hid device ��״̬ */
	Hid_ResetRecovery ResetRecovery;		/* reset device 					*/
	Hid_Transport 	  Transport;			/* ���� 							*/
	Hid_StopTransport StopTransport;		/* ��ֹ���� 						*/

	/* Hid�豸���� */
	HidClientProbe  ClientProbe;
	HidClientRemove ClientRemove;

	void *Extern;							/* ��Ӧ�����hid�豸, ��mouse, keyboard */
}HidDev_t;

typedef void (* HidClientDone)(struct _HidRequest *);

/* Hid �������� */
typedef struct _HidRequest{
	HidDev_t *HidDev;

	void *buffer;							/* Data buffer 					*/
	unsigned int DataTransferLength;				/* Size of data buffer 			*/
	unsigned int ActualLength;						/* actual transport length		*/

	HidClientDone Done;
	unsigned int Result;							/* ִ�н��						*/

	void *Extern;							/* ��Ӧ�����hid�豸, ��mouse, keyboard */
}HidRequest_t;

//-----------------------------------------------------
//  Hid ������
//-----------------------------------------------------
#define  USB_HID_TRANSPORT_SUCCESS				0x00  /* ����ɹ� 			*/

#define  USB_HID_TRANSPORT_DEVICE_DISCONNECT	0x01  /* �豸�Ͽ� 			*/
#define  USB_HID_TRANSPORT_DEVICE_RESET			0x02  /* �豸��λ 			*/
#define  USB_HID_TRANSPORT_PIPE_HALT			0x03  /* ����ܵ��쳣 		*/
#define  USB_HID_TRANSPORT_CANCEL_CMD			0x04  /* �����ֹ�˴δ��� 	*/

#define  USB_HID_TRANSPORT_UNKOWN_ERR			0xFF  /* δ֪���� 			*/


//-----------------------------------------------------
//
//-----------------------------------------------------
int HidSentRequest(HidRequest_t *HidReq);
int HidGetInputReport(HidDev_t *HidDev, unsigned int Usagepage, unsigned int Usage, unsigned int *BitOffset, unsigned int *BitCount);


#endif   //__HID_I_H__

