//
//  main.c
//  umc
//
//  Created by Steven Yue on 16/6/28.
//  Copyright (c) 2016年 Steven Yue. All rights reserved.
//

#include <stdio.h>
#include <usb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ptp.h"
#include "main.h"
#include "constants.h"
#include <malloc.h>
#include "Python.h"

int ptpcam_usb_timeout = USB_TIMEOUT;

PTPParams* globalparams;
PTPParams sony_params;
PTP_USB sony_ptp_usb;
struct usb_device *sony_dev;
int sony_busn = 0, sony_devn = 0;
char sony_data_file[256];
PTPObjectInfo sony_objinfo;
char * imgbuf;

void
display_hexdump(char *data, size_t size)
{
    uint i=0;
    char buffer[50];
    char charBuf[17];
    
    memset((void*)buffer, 0, 49);
    memset((void*)charBuf, 0, 17);
    for (; i < size; ++i)
    {
        snprintf(&(buffer[(i%16) * 3]) , 4, "%02x ", *(data+i)&0xff);
        if ((data[i] >= ' ' && data[i] <= '^') ||
            (data[i] >= 'a' && data[i] <= '~')) // printable characters
            charBuf[i%16] = data[i];
        else
            charBuf[i%16] = '.';
        if ((i % 16) == 15 || i == (size - 1))
        {
            charBuf[(i%16)+1] = '\0';
            buffer[((i%16) * 3) + 2] = '\0';
            printf("%-48s- %-16s\n", buffer, charBuf);
            memset((void*)buffer, 0, 49);
            memset((void*)charBuf, 0, 17);
        }
    }
}

static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
    int result=-1;
    PTP_USB *ptp_usb=(PTP_USB *)data;
    int toread=0;
    signed long int rbytes=size;
    
    do {
        bytes+=toread;
        if (rbytes>PTPCAM_USB_URB)
        {
            toread = PTPCAM_USB_URB;
        } else {
            toread = rbytes;
        }
        result=USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)bytes, toread,ptpcam_usb_timeout);
        /* sometimes retry might help */
        if (result==0) {
            result=USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)bytes, toread,ptpcam_usb_timeout);
        }
        if (result < 0)
            break;
        rbytes-=PTPCAM_USB_URB;
    } while (rbytes>0);
    
    
    if (result >= 0) {
        
         // printf("Read Data:");
         // for (int i = 0; i < size; ++i)
         // {
         // printf("%x",bytes[i]);
         // }
         // printf("\n");
        
        return (PTP_RC_OK);
    }
    else
    {
        perror("usb_bulk_read");
        printf ("IO Error Caught!\n");
        return PTP_ERROR_IO;
    }
}

static short
ptp_write_func (unsigned char *bytes, unsigned int size, void *data)
{
    int result;
    PTP_USB *ptp_usb=(PTP_USB *)data;
    
    
     // printf("Write Data:");
     // for (int i = 0; i < size; ++i)
     // {
     // printf("%x ",bytes[i]);
     // }
     // printf("\n");
     
    
    result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)bytes,size,ptpcam_usb_timeout);
    if (result >= 0)
        return (PTP_RC_OK);
    else 
    {
        perror("usb_bulk_write");
        return PTP_ERROR_IO;
    }
}

/* XXX this one is suposed to return the number of bytes read!!! */
static short
ptp_check_int (unsigned char *bytes, unsigned int size, void *data)
{
    int result;
    PTP_USB *ptp_usb=(PTP_USB *)data;
    
    result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)bytes,size,ptpcam_usb_timeout);
    if (result==0)
        result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)bytes,size,ptpcam_usb_timeout);
    fprintf (stderr, "USB_BULK_READ returned %i, size=%i\n", result, size);
    
    if (result >= 0) {
        return result;
    } else {
        perror("ptp_check_int");
        return result;
    }
}


void
ptpcam_debug (void *data, const char *format, va_list args);
void
ptpcam_debug (void *data, const char *format, va_list args)
{
    vfprintf (stderr, format, args);
    fprintf (stderr,"\n");
    fflush(stderr);
}

void
ptpcam_error (void *data, const char *format, va_list args);
void
ptpcam_error (void *data, const char *format, va_list args)
{
    vfprintf (stderr, format, args);
    fprintf (stderr,"\n");
    fflush(stderr);
}

void
close_usb(PTP_USB* ptp_usb, struct usb_device* dev)
{
    //clear_stall(ptp_usb);
    usb_release_interface(ptp_usb->handle,
                          dev->config->interface->altsetting->bInterfaceNumber);
    usb_reset(ptp_usb->handle);
    usb_close(ptp_usb->handle);
}

struct usb_bus*
init_usb()
{
    usb_init();
    usb_find_busses();
    usb_find_devices();
    return (usb_get_busses());
}

/*
 find_device() returns the pointer to a usb_device structure matching
 given busn, devicen numbers. If any or both of arguments are 0 then the
 first matching PTP device structure is returned.
 */
struct usb_device*
find_device (int busn, int devicen, short force);
struct usb_device*
find_device (int busn, int devn, short force)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    
    bus=init_usb();
    for (; bus; bus = bus->next)
        for (dev = bus->devices; dev; dev = dev->next)
            if (dev->config)
                if ((dev->config->interface->altsetting->bInterfaceClass==
                     USB_CLASS_PTP)||force)
                    if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB)
                    {
                        int curbusn, curdevn;
                        
                        curbusn=strtol(bus->dirname,NULL,10);
                        curdevn=strtol(dev->filename,NULL,10);
                        
                        if (devn==0) {
                            if (busn==0) return dev;
                            if (curbusn==busn) return dev;
                        } else {
                            if ((busn==0)&&(curdevn==devn)) return dev;
                            if ((curbusn==busn)&&(curdevn==devn)) return dev;
                        }
                    }
    return NULL;
}


void
init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
{
    usb_dev_handle *device_handle;
    
    params->write_func=ptp_write_func;
    params->read_func=ptp_read_func;
    params->check_int_func=ptp_check_int;
    params->check_int_fast_func=ptp_check_int;
    params->error_func=ptpcam_error;
    params->debug_func=ptpcam_debug;
    params->sendreq_func=ptp_usb_sendreq;
    params->senddata_func=ptp_usb_senddata;
    params->getresp_func=ptp_usb_getresp;
    params->getdata_func=ptp_usb_getdata;
    params->data=ptp_usb;
    params->transaction_id=0;
    params->byteorder = PTP_DL_LE;
    
    if ((device_handle=usb_open(dev))){
        if (!device_handle) {
            perror("usb_open()");
            exit(0);
        }
        ptp_usb->handle=device_handle;
        usb_set_configuration(device_handle, dev->config->bConfigurationValue);
        usb_claim_interface(device_handle,
                            dev->config->interface->altsetting->bInterfaceNumber);
    }
    globalparams=params;
}

void
find_endpoints(struct usb_device *dev, int* inep, int* outep, int* intep)
{
    int i,n;
    struct usb_endpoint_descriptor *ep;
    
    ep = dev->config->interface->altsetting->endpoint;
    n=dev->config->interface->altsetting->bNumEndpoints;
    
    for (i=0;i<n;i++) {
        if (ep[i].bmAttributes==USB_ENDPOINT_TYPE_BULK)	{
            if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
                USB_ENDPOINT_DIR_MASK)
            {
                *inep=ep[i].bEndpointAddress;
                fprintf(stderr, "Found inep: 0x%02x\n",*inep);
            }
            if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==0)
            {
                *outep=ep[i].bEndpointAddress;
                fprintf(stderr, "Found outep: 0x%02x\n",*outep);
            }
        } else if ((ep[i].bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT) &&
                   ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
                    USB_ENDPOINT_DIR_MASK))
        {
            *intep=ep[i].bEndpointAddress;
            fprintf(stderr, "Found intep: 0x%02x\n",*intep);
        }
    }
}

int
open_camera (int busn, int devn, short force, PTP_USB *ptp_usb, PTPParams *params, struct usb_device **dev)
{
#ifdef DEBUG
    printf("dev %i\tbus %i\n",devn,busn);
#endif
    
    *dev=find_device(busn,devn,force);
    if (*dev==NULL) {
        fprintf(stderr,"could not find any device matching given "
                "bus/dev numbers\n");
        exit(-1);
    }
    find_endpoints(*dev,&ptp_usb->inep,&ptp_usb->outep,&ptp_usb->intep);
    
    init_ptp_usb(params, ptp_usb, *dev);
    if (ptp_opensession(params,1)!=PTP_RC_OK) {
        fprintf(stderr,"ERROR: Could not open session!\n");
        close_usb(ptp_usb, *dev);
        return -1;
    }
    if (ptp_getdeviceinfo(params,&params->deviceinfo)!=PTP_RC_OK) {
        fprintf(stderr,"ERROR: Could not get device info!\n");
        close_usb(ptp_usb, *dev);
        return -1;
    }
    return 0;
}

void
close_camera (PTP_USB *ptp_usb, PTPParams *params, struct usb_device *dev)
{
    if (ptp_closesession(params)!=PTP_RC_OK)
        fprintf(stderr,"ERROR: Could not close session!\n");
    close_usb(ptp_usb, dev);
}

void
list_devices(short force)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    int found=0;
    
    
    bus=init_usb();
    for (; bus; bus = bus->next)
        for (dev = bus->devices; dev; dev = dev->next) {
            /* if it's a PTP device try to talk to it */
            if (dev->config)
                if ((dev->config->interface->altsetting->bInterfaceClass==
                     USB_CLASS_PTP)||force)
                    if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB)
                    {
                        PTPParams params;
                        PTP_USB ptp_usb;
                        PTPDeviceInfo deviceinfo;
                        
                        if (!found){
                            printf("\nListing devices...\n");
                            printf("bus/dev\tvendorID/prodID\tdevice model\n");
                            found=1;
                        }
                        
                        find_endpoints(dev,&ptp_usb.inep,&ptp_usb.outep,
                                       &ptp_usb.intep);
                        init_ptp_usb(&params, &ptp_usb, dev);
                        
                        CC(ptp_opensession (&params,1),
                           "Could not open session!\n"
                           "Try to reset the camera.\n");
                        CC(ptp_getdeviceinfo (&params, &deviceinfo),
                           "Could not get device info!\n");
                        
                        printf("%s/%s\t0x%04X/0x%04X\t%s\n",
                               bus->dirname, dev->filename,
                               dev->descriptor.idVendor,
                               dev->descriptor.idProduct, deviceinfo.Model);
                        
                        CC(ptp_closesession(&params),
                           "Could not close session!\n");
                        close_usb(&ptp_usb, dev);
                    }
        }
    if (!found) printf("\nFound no PTP devices\n");
    printf("\n");
}

void
list_properties (int busn, int devn, short force)
{
    PTPParams params;
    PTP_USB ptp_usb;
    struct usb_device *dev;
    int i;
    
    printf("\nListing properties...\n");
    
    if (open_camera(busn, devn, force, &ptp_usb, &params, &dev)<0)
        return;
    printf("Camera: %s\n",params.deviceinfo.Model);
    printf("Length of Supported Prop: %d\n", params.deviceinfo.DevicePropertiesSupported_len);
    for (i=0; i<params.deviceinfo.DevicePropertiesSupported_len;i++){
        printf("0x%x\n",params.deviceinfo.DevicePropertiesSupported[i]);
    }
    close_camera(&ptp_usb, &params, dev);
}


void sony_auth(int busn, int devn, PTPParams* params)
{
    char *data = NULL;
    long fsize = 0;
    
    uint16_t reqCode = 0x96FE;
    uint32_t reqParams[5];
    reqParams[0] = 0x01;
    reqParams[1] = 0xDA01;
    reqParams[2] = 0xDA01;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    uint32_t direction = PTP_DP_GETDATA;
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("SDIO_Connect: Phase 1 OK!\n");
    }
    
    reqCode = 0x96FE;
    reqParams[0] = 0x02;
    reqParams[1] = 0xDA01;
    reqParams[2] = 0xDA01;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    direction = PTP_DP_GETDATA;
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("SDIO_Connect: Phase 2 OK!\n");
    }
    
    
    
    reqCode = 0x96FD;
    reqParams[0] = 0x00C8;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    direction = PTP_DP_GETDATA;
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("SDIO_GetExtDeviceInfo: OK!\n");
    }
    
    
    reqCode = 0x96FE;
    reqParams[0] = 0x03;
    reqParams[1] = 0xDA01;
    reqParams[2] = 0xDA01;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    direction = PTP_DP_GETDATA;
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("SDIO_Connect: Phase 3 OK!\n");
    }
    
    printf("Sony Authentication Sequence Complete!\n");
    
    //Auth Sequence Complete
    
}

unsigned char* sony_deviceinfo(int busn, int devn, PTPParams* params)
{
    //get device info
    char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    unsigned char *res = NULL;
    
    uint16_t reqCode = 0x96F6;
    reqParams[0] = 0x0;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    uint32_t direction = PTP_DP_GETDATA;
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            //display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("PTP: response OK\n");
    }

    res = (unsigned char*)malloc(malloc_usable_size((void*)data));

	for (int i = 0; i < malloc_usable_size((void*)data); ++i)
	{
		res[i] = data[i]&0xff;
		//printf("%02x ", res[i]);
	}
	//printf("\n");
    
    printf("Successifully Requested Device Info!\n");
    return res;
}

void sony_mstart(int busn, int devn, PTPParams* params, char *data_file){
	char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    
    uint16_t reqCode = 0x96F8;
    reqParams[0] = 0xD60F;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    uint32_t direction = PTP_DP_SENDDATA;
    
    //process data
    memset((void*)data_file, 0, 256);
    data_file[0] = '0';
    data_file[1] = '2';
    data_file[2] = '0';
    data_file[3] = '0';
    
    
    printf("data to send : '%s'\n", data_file);
    
    
    uint len = strlen(data_file);
    char num[3];
    uint i;
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Recording: Started!\n");
    }
}

void sony_mstop(int busn, int devn, PTPParams* params, char *data_file){
	char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    
    uint16_t reqCode = 0x96F8;
    reqParams[0] = 0xD60F;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    uint32_t direction = PTP_DP_SENDDATA;
    
    //process data
    memset((void*)data_file, 0, 256);
    data_file[0] = '0';
    data_file[1] = '1';
    data_file[2] = '0';
    data_file[3] = '0';
    
    
    printf("data to send : '%s'\n", data_file);
    
    
    uint len = strlen(data_file);
    char num[3];
    uint i;
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Recording: Started!\n");
    }
}

void sony_movie_record(int busn, int devn, PTPParams* params, char *data_file)
{
    char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    
    uint16_t reqCode = 0x96F8;
    reqParams[0] = 0xD60F;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    uint32_t direction = PTP_DP_SENDDATA;
    
    //process data
    memset((void*)data_file, 0, 256);
    data_file[0] = '0';
    data_file[1] = '2';
    data_file[2] = '0';
    data_file[3] = '0';
    
    
    printf("data to send : '%s'\n", data_file);
    
    
    uint len = strlen(data_file);
    char num[3];
    uint i;
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Recording: Started!\n");
    }
    
    data = NULL;
    fsize = 0;
    
    int tmp;
    printf("Entering Movie Mode... Press Type Anything to Stop...");
    scanf("%d", tmp);
    printf("\n");
    
    //sleep(8);
    
    reqCode = 0x96F8;
    reqParams[0] = 0xD60F;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    direction = PTP_DP_SENDDATA;
    
    //process data
    memset((void*)data_file, 0, 256);
    data_file[0] = '0';
    data_file[1] = '1';
    data_file[2] = '0';
    data_file[3] = '0';
    
    
    printf("data to send : '%s'\n", data_file);
    
    
    len = strlen(data_file);
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Recording: Stopped!\n");
    }
    
    printf("Recording Complete!\n");
    
}

void sony_shutdown(int busn, int devn, PTPParams* params, char *data_file){
    char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    
    uint16_t reqCode = 0x96F8;
    reqParams[0] = 0xD637;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    uint32_t direction = PTP_DP_SENDDATA;
    
    //process data
    memset((void*)data_file, 0, 256);
    data_file[0] = '0';
    data_file[1] = '2';
    data_file[2] = '0';
    data_file[3] = '0';
    
    
    printf("data to send : '%s'\n", data_file);
    
    
    uint len = strlen(data_file);
    char num[3];
    uint i;
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Power off: Button Pressed Down!\n");
    }
    
    data = NULL;
    fsize = 0;
    
    sleep(1);
    
    reqCode = 0x96F8;
    reqParams[0] = 0xD637;
    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;
    direction = PTP_DP_SENDDATA;
    
    //process data
    memset((void*)data_file, 0, 256);
    data_file[0] = '0';
    data_file[1] = '1';
    data_file[2] = '0';
    data_file[3] = '0';
    
    
    printf("data to send : '%s'\n", data_file);
    
    
    len = strlen(data_file);
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    result=ptp_sendgenericrequest (params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Power off: Button Released!\n");
    }
    
    printf("Finished Power off Sequence!\n");
    
}

char* sconnect(char* input){
    
    if (open_camera(sony_busn, sony_devn, 0, &sony_ptp_usb, &sony_params, &sony_dev)<0) {
            printf("Camera: %s\n",sony_params.deviceinfo.Model);
    }

    return "finished lol";
}

char* auth(char* input){

	sony_auth(sony_busn, sony_devn, &sony_params);

	return "finished lol";

}

static PyObject * connect_wrapper(PyObject * self, PyObject * args){
    char * input;
    char * result;
    PyObject * ret;
    
    // parse arg
    if (!PyArg_ParseTuple(args, "", &input)) {
    //    return NULL;
    }
    
    // run the func
    result = sconnect(input);
    
    // build the resulting string into pyobj
    ret = PyString_FromString(result);
    //free(result);
    
    return ret;
}

static PyObject * auth_wrapper(PyObject * self, PyObject * args){
    char * input;
    char * result;
    PyObject * ret;
    
    // parse arg
    if (!PyArg_ParseTuple(args, "", &input)) {
    //    return NULL;
    }
    
    // run the func
    result = auth(input);
    
    // build the resulting string into pyobj
    ret = PyString_FromString(result);
    //free(result);
    
    return ret;
}

static PyObject * movie_start_wrapper(PyObject * self, PyObject * args){
    char * input;
    char * result;
    PyObject * ret;
    
    // parse arg
    if (!PyArg_ParseTuple(args, "", &input)) {
    //    return NULL;
    }
    
    // run the func
    sony_mstart(sony_busn, sony_devn, &sony_params, sony_data_file);
    result = "finished lol";
    
    // build the resulting string into pyobj
    ret = PyString_FromString(result);
    //free(result);
    
    return ret;
}

static PyObject * movie_stop_wrapper(PyObject * self, PyObject * args){
    char * input;
    char * result;
    PyObject * ret;
    
    // parse arg
    if (!PyArg_ParseTuple(args, "", &input)) {
    //    return NULL;
    }
    
    // run the func
    sony_mstop(sony_busn, sony_devn, &sony_params, sony_data_file);
    result = "finished lol";
    
    // build the resulting string into pyobj
    ret = PyString_FromString(result);
    //free(result);
    
    return ret;
}

static PyObject * shutdown_wrapper(PyObject * self, PyObject * args){
    char * input;
    char * result;
    PyObject * ret;
    
    // parse arg
    if (!PyArg_ParseTuple(args, "", &input)) {
    //    return NULL;
    }
    
    // run the func
    sony_shutdown(sony_busn, sony_devn, &sony_params, sony_data_file);
    result = "finished lol";
    
    // build the resulting string into pyobj
    ret = PyString_FromString(result);
    //free(result);
    
    return ret;
}

static PyListObject * info_wrapper(PyObject * self, PyObject * args){
    char * input;
    unsigned char* result;
    PyObject * tmp;
    PyListObject * list = PyList_New(0);
    char a[1];
    
    // parse arg
    if (!PyArg_ParseTuple(args, "", &input)) {
    //    return NULL;
    }
    
    // run the func
    result = sony_deviceinfo(sony_busn, sony_devn, &sony_params);
    for (int i = 0; i < malloc_usable_size((void*)result); ++i)
    {
    	a[0] = result[i];
    	tmp = PyString_FromString(a);
    	PyList_Append(list, tmp);
    }
    
    // build the resulting string into pyobj
    
    //free(result);
    
    return list;
}

static PyObject * setprop_wrapper(PyObject * self, PyObject * args){
	PyObject * res;
	char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    uint16_t reqCode = 0x96FA;
    char *data_file = malloc(256);

    memset((void*)data_file, 0, 256);

	if (!PyArg_ParseTuple(args, "Is", &reqParams[0], &data_file)) {
        return NULL;
    }

    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;

    uint32_t direction = PTP_DP_SENDDATA;
    
    //process data
    
    printf("data to send : '%s'\n", data_file);
    
    
    uint len = strlen(data_file);
    char num[3];
    uint i;
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (&sony_params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(&sony_params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Request Sent!\n");
    }

    res = PyString_FromString("Finished");

    return res;
}

static PyObject * getliveinfo(PyObject * self, PyObject * args){
	PyObject * res;
	PTPObjectInfo objinfo;
	
	ptp_getobjectinfo (&sony_params, 0xFFFFC002, &objinfo);
	
	printf("%x, %x.\n", objinfo.ImagePixWidth, objinfo.ImagePixHeight);
	//printf("%x, %x.\n", objinfo.ImagePixWidth, objinfo.ImagePixHeight);
	
	res = PyString_FromString("Finished");
	return res;
}

static PyObject * control_wrapper(PyObject * self, PyObject * args){
    PyObject * res;
    char *data = NULL;
    long fsize = 0;
    uint32_t reqParams[5];
    uint16_t reqCode = 0x96F8;
    char *data_file = malloc(256);

    memset((void*)data_file, 0, 256);

    if (!PyArg_ParseTuple(args, "Is", &reqParams[0], &data_file)) {
        return NULL;
    }

    reqParams[1] = 0x0;
    reqParams[2] = 0x0;
    reqParams[3] = 0x0;
    reqParams[4] = 0x0;

    uint32_t direction = PTP_DP_SENDDATA;
    
    //process data
    
    printf("data to send : '%s'\n", data_file);
    
    
    uint len = strlen(data_file);
    char num[3];
    uint i;
    data = (char*)calloc(1,len / 2);
    
    num[2] = 0;
    for (i = 0; i < len ; i += 2) {
        num[1] = data_file[i];
        if (i < len-1) {
            num[0] = data_file[i];
            num[1] = data_file[i+1];
        }
        else {
            num[0] = data_file[i];
            num[1] = 0;
        }
        data[fsize] = (char)strtol(num, NULL, 16);
        printf("Current: %i\n",data[fsize]);
        ++fsize;
    }
    
    
    printf("length: %i.\n",fsize);
    
    printf("Sending generic request: reqCode=0x%04x, params=[0x%08x,0x%08x,0x%08x,0x%08x,0x%08x]\n",
           (uint)reqCode, reqParams[0], reqParams[1], reqParams[2], reqParams[3], reqParams[4]);
    uint16_t result=ptp_sendgenericrequest (&sony_params, reqCode, reqParams, &data, direction, fsize);
    
    if((result)!=PTP_RC_OK) {
        ptp_perror(&sony_params,result);
        if (result > 0x2000)
            fprintf(stderr,"PTP: ERROR: response 0x%04x\n", result);
    } else {
        if (data != NULL && direction == PTP_DP_GETDATA) {
            display_hexdump(data, malloc_usable_size ((void*)data));
            //free(data);
        }
        printf("Control Code Sent!\n");
    }

    res = PyString_FromString("Finished");

    return res;
}

void
save_object(PTPParams *params, uint32_t handle, char* filename, PTPObjectInfo oi, int overwrite)
{
	int file;
	char *image;
	int ret;
	struct utimbuf timebuf;
	
	
	// obtaining object into buffer first
	if (imgbuf == NULL) imgbuf = malloc(oi.ObjectCompressedSize);
	memset(imgbuf, 0, oi.ObjectCompressedSize);
	
	ret=ptp_getobject(params,handle,&imgbuf);
	uint16_t offset = (uint32_t)*((uint32_t*)imgbuf);
	printf("offset: %x.\n",offset);
	uint16_t isize = (uint32_t)*(((uint32_t*)imgbuf)+1);
	printf("size: %x.\n",isize);
	
	// open file and copy the buf to file
	
	file=open(filename, (overwrite==OVERWRITE_EXISTING?0:O_EXCL)|O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRGRP);
	if (file==-1) {
		if (errno==EEXIST) {
			printf("Skipping file: \"%s\", file exists!\n",filename);
			goto out;
		}
		perror("open");
		goto out;
	}
	lseek(file,isize-1,SEEK_SET);
	ret=write(file,"",1);
	if (ret==-1) {
	    perror("write");
	    goto out;
	}
	image=mmap(0,isize,PROT_READ|PROT_WRITE,MAP_SHARED,
		file,0);
	if (image==MAP_FAILED) {
		perror("mmap");
		close(file);
		goto out;
	}
	printf ("Saving file: \"%s\" ",filename);
	fflush(NULL);
	
	for(uint_t si = 0; si < (isize/sizeof(char)); i++)
	{
		printf("%i.",si);
		image[si] = imgbuf[si+(offset/sizeof(char))];
	}
	

	munmap(image,isize);
	if (close(file)==-1) {
	    perror("close");
	}
	timebuf.actime=oi.ModificationDate;
	timebuf.modtime=oi.CaptureDate;
	utime(filename,&timebuf);
	if (ret!=PTP_RC_OK) {
		printf ("error!\n");
		ptp_perror(params,ret);
		//if (ret==PTP_ERROR_IO) clear_stall((PTP_USB *)(params->data));
	} else {
		printf("is done.\n");
	}
out:
	return;
}

static PyObject * getliveobj(PyObject * self, PyObject * args){
	PyObject * res;
	char* data;
	
	ptp_getobjectinfo (&sony_params, 0xFFFFC002, &sony_objinfo);
	
	save_object(&sony_params, 0xFFFFC002, "object", sony_objinfo, 1);
	
	//display_hexdump(data, malloc_usable_size ((void*)data));
	
	res = PyString_FromString("Finished");
	return res;
}

static PyMethodDef SonyMethods[] = {
    {"connect", connect_wrapper, METH_VARARGS, "Connection" },
    {"auth", auth_wrapper, METH_VARARGS, "Authentication"},
    {"info", info_wrapper, METH_VARARGS, "Get All Device Info"},
    {"mstart", movie_start_wrapper, METH_VARARGS, "Movie Start"},
    {"mstop", movie_stop_wrapper, METH_VARARGS, "Movie Stop"},
    {"shutdown", shutdown_wrapper, METH_VARARGS, "Shutdown"},
    {"setprop", setprop_wrapper, METH_VARARGS, "Set Properties"},
    {"control", control_wrapper, METH_VARARGS, "Control Device"},
	{"getliveinfo", getliveinfo, METH_VARARGS, "Get LiveView Info"},
	{"getliveobj", getliveobj, METH_VARARGS, "Get LiveView Object"},
    {NULL, NULL, 0, NULL}
};

DL_EXPORT(void) initsony(void)
{
    Py_InitModule("sony", SonyMethods);
}