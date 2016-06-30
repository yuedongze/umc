//
//  main.h
//  umc
//
//  Created by Steven Yue on 16/6/28.
//  Copyright (c) 2016年 Steven Yue. All rights reserved.
//

#ifndef umc_main_h
#define umc_main_h
#endif

#define USB_BULK_READ usb_bulk_read
#define USB_BULK_WRITE usb_bulk_write

/* OUR APPLICATION USB URB (2MB) ;) */
#define PTPCAM_USB_URB		2097152

#define USB_TIMEOUT		5000
#define USB_CAPTURE_TIMEOUT	20000

/* filename overwrite */
#define OVERWRITE_EXISTING	1
#define	SKIP_IF_EXISTS		0

typedef struct _PTP_USB PTP_USB;
struct _PTP_USB {
    usb_dev_handle* handle;
    int inep;
    int outep;
    int intep;
};

/* Check value and Continue on error */
#define CC(result,error) {						\
if((result)!=PTP_RC_OK) {			\
fprintf(stderr,"ERROR: "error);		\
usb_release_interface(ptp_usb.handle,	\
dev->config->interface->altsetting->bInterfaceNumber);	\
continue;					\
}						\
}