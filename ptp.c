//
//  ptp.c
//  umc
//
//  Created by Steven Yue on 16/6/28.
//  Copyright (c) 2016年 Steven Yue. All rights reserved.
//

#include <stdio.h>
#include "ptp.h"
#include "constants.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

/**
 * ptp_opensession:
 * params:	PTPParams*
 * 		session			- session number
 *
 * Establishes a new session.
 *
 * Return values: Some PTP_RC_* code.
 **/
#define CHECK_PTP_RC(result)	{uint16_t r=(result); if (r!=PTP_RC_OK) return r;}
#define PTP_CNT_INIT(cnt) {memset(&cnt,0,sizeof(cnt));}
#define _(String) (String)
#define N_(String) (String)

static void
ptp_debug (PTPParams *params, const char *format, ...)
{
    va_list args;
    
    va_start (args, format);
    if (params->debug_func!=NULL)
        params->debug_func (params->data, format, args);
    else
    {
        vfprintf (stderr, format, args);
        fprintf (stderr,"\n");
        fflush (stderr);
    }
    va_end (args);
}

/* Pack / unpack functions */

#include "ptp-pack.c"

/* send / receive functions */

uint16_t
ptp_usb_sendreq (PTPParams* params, PTPContainer* req)
{
    static uint16_t ret;
    static PTPUSBBulkContainer usbreq;
    
    PTP_CNT_INIT(usbreq);
    /* build appropriate USB container */
    usbreq.length=htod32(PTP_USB_BULK_REQ_LEN-
                         (sizeof(uint32_t)*(5-req->Nparam)));
    usbreq.type=htod16(PTP_USB_CONTAINER_COMMAND);
    usbreq.code=htod16(req->Code);
    usbreq.trans_id=htod32(req->Transaction_ID);
    usbreq.payload.params.param1=htod32(req->Param1);
    usbreq.payload.params.param2=htod32(req->Param2);
    usbreq.payload.params.param3=htod32(req->Param3);
    usbreq.payload.params.param4=htod32(req->Param4);
    usbreq.payload.params.param5=htod32(req->Param5);
    /* send it to responder */
    ret=params->write_func((unsigned char *)&usbreq,
                           PTP_USB_BULK_REQ_LEN-(sizeof(uint32_t)*(5-req->Nparam)),
                           params->data);
    if (ret!=PTP_RC_OK) {
        printf("error found at loc 7.");
        ret = PTP_ERROR_IO;
        /*		ptp_error (params,
         "PTP: request code 0x%04x sending req error 0x%04x",
         req->Code,ret); */
    }
    return ret;
}

uint16_t
ptp_usb_senddata (PTPParams* params, PTPContainer* ptp,
                  unsigned char *data, unsigned int size)
{
    static uint16_t ret;
    static PTPUSBBulkContainer usbdata;
    int i,j;
    
    /* build appropriate USB container */
    usbdata.length=htod32(PTP_USB_BULK_HDR_LEN+size);
    usbdata.type=htod16(PTP_USB_CONTAINER_DATA);
    usbdata.code=htod16(ptp->Code);
    usbdata.trans_id=htod32(ptp->Transaction_ID);
    memcpy(usbdata.payload.data,data,(size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN);
    
    
    static PTPUSBBulkContainer d;
    d.length=(uint32_t)PTP_USB_BULK_HDR_LEN+size;
    d.type=(uint16_t)PTP_USB_CONTAINER_DATA;
    d.code=(uint16_t)ptp->Code;
    d.trans_id=(uint32_t)ptp->Transaction_ID;
    memcpy((d.payload.data),(data),(size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN);
    
    printf("Sending Data (steven):");
    for (i = 0; i < 20; ++i)
    {
        printf("%x ",((unsigned char *)&d)[i]);
    }
    printf("\n");
    
    printf("Sending Data (ptpcam):");
    for (j = 0; j < 20; ++j)
    {
        printf("%x ",((unsigned char *)&usbdata)[j]);
    }
    printf("\n");
    
    /* send first part of data */
    ret=params->write_func((unsigned char *)&usbdata, PTP_USB_BULK_HDR_LEN+
                           ((size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN),
                           params->data);
    if (ret!=PTP_RC_OK) {
        printf("error found at loc 8.");
        ret = PTP_ERROR_IO;
        /*		ptp_error (params,
         "PTP: request code 0x%04x sending data error 0x%04x",
         ptp->Code,ret);*/
        return ret;
    }
    if (size<=PTP_USB_BULK_PAYLOAD_LEN) return ret;
    /* if everything OK send the rest */
    ret=params->write_func (data+PTP_USB_BULK_PAYLOAD_LEN,
                            size-PTP_USB_BULK_PAYLOAD_LEN, params->data);
    if (ret!=PTP_RC_OK) {
        printf("error found at loc 9.");
        ret = PTP_ERROR_IO;
        /*		ptp_error (params,
         "PTP: request code 0x%04x sending data error 0x%04x",
         ptp->Code,ret); */
    }
    return ret;
}

uint16_t
ptp_usb_getdata (PTPParams* params, PTPContainer* ptp,  unsigned int *getlen,
                 unsigned char **data)
{
    static uint16_t ret;
    static PTPUSBBulkContainer usbdata;
    
    PTP_CNT_INIT(usbdata);
#if 0
    if (*data!=NULL) return PTP_ERROR_BADPARAM;
#endif
    do {
        /* read first(?) part of data */
        ret=params->read_func((unsigned char *)&usbdata,
                              sizeof(usbdata), params->data);
        if (ret!=PTP_RC_OK) {
            printf("error found at loc 1. Error is %x.\n", ret);
            ret = PTP_ERROR_IO;
            break;
        } else
            if (dtoh16(usbdata.type)!=PTP_USB_CONTAINER_DATA
                && dtoh16(usbdata.type)!=PTP_USB_CONTAINER_RESPONSE) {
                ret = PTP_ERROR_DATA_EXPECTED;
                break;
            } else
                if (dtoh16(usbdata.code)!=ptp->Code) {
                    ret = dtoh16(usbdata.code);
                    break;
                }
        /* evaluate data length */
        *getlen=dtoh32(usbdata.length)-PTP_USB_BULK_HDR_LEN;
        /* allocate memory for data if not allocated already */
        if (*data==NULL) *data=calloc(1,*getlen);
        /* copy first part of data to 'data' */
        memcpy(*data,usbdata.payload.data,
               PTP_USB_BULK_PAYLOAD_LEN<*getlen?
               PTP_USB_BULK_PAYLOAD_LEN:*getlen);
        /* is that all of data? */
        if (*getlen+PTP_USB_BULK_HDR_LEN<=sizeof(usbdata)) break;
        /* if not finaly read the rest of it */
        ret=params->read_func(((unsigned char *)(*data))+
                              PTP_USB_BULK_PAYLOAD_LEN,
                              *getlen-PTP_USB_BULK_PAYLOAD_LEN,
                              params->data);
        if (ret!=PTP_RC_OK) {
            printf("error found at loc 2.");
            ret = PTP_ERROR_IO;
            break;
        }
    } while (0);
    /*
     if (ret!=PTP_RC_OK) {
     ptp_error (params,
     "PTP: request code 0x%04x getting data error 0x%04x",
     ptp->Code, ret);
     }*/
    return ret;
}

uint16_t
ptp_usb_getresp (PTPParams* params, PTPContainer* resp)
{
    static uint16_t ret;
    static PTPUSBBulkContainer usbresp;
    
    PTP_CNT_INIT(usbresp);
    /* read response, it should never be longer than sizeof(usbresp) */
    ret=params->read_func((unsigned char *)&usbresp,
                          sizeof(usbresp), params->data);
    
    if (ret!=PTP_RC_OK) {
        printf("error found at loc 3.\n");
        ret = PTP_ERROR_IO;
    } else
        if (dtoh16(usbresp.type)!=PTP_USB_CONTAINER_RESPONSE) {
            ret = PTP_ERROR_RESP_EXPECTED;
        } else
            if (dtoh16(usbresp.code)!=resp->Code) {
                ret = dtoh16(usbresp.code);
            }
    if (ret!=PTP_RC_OK) {
        /*		ptp_error (params,
         "PTP: request code 0x%04x getting resp error 0x%04x",
         resp->Code, ret);*/
        return ret;
    }
    /* build an appropriate PTPContainer */
    resp->Code=dtoh16(usbresp.code);
    resp->SessionID=params->session_id;
    resp->Transaction_ID=dtoh32(usbresp.trans_id);
    resp->Param1=dtoh32(usbresp.payload.params.param1);
    resp->Param2=dtoh32(usbresp.payload.params.param2);
    resp->Param3=dtoh32(usbresp.payload.params.param3);
    resp->Param4=dtoh32(usbresp.payload.params.param4);
    resp->Param5=dtoh32(usbresp.payload.params.param5);
    return ret;
}


/**
 * ptp_transaction:
 * params:	PTPParams*
 * 		PTPContainer* ptp	- general ptp container
 * 		uint16_t flags		- lower 8 bits - data phase description
 * 		unsigned int sendlen	- senddata phase data length
 * 		char** data		- send or receive data buffer pointer
 *
 * Performs PTP transaction. ptp is a PTPContainer with appropriate fields
 * filled in (i.e. operation code and parameters). It's up to caller to do
 * so.
 * The flags decide whether the transaction has a data phase and what is its
 * direction (send or receive).
 * If transaction is sending data the sendlen should contain its length in
 * bytes, otherwise it's ignored.
 * The data should contain an address of a pointer to data going to be sent
 * or is filled with such a pointer address if data are received depending
 * od dataphase direction (send or received) or is beeing ignored (no
 * dataphase).
 * The memory for a pointer should be preserved by the caller, if data are
 * beeing retreived the appropriate amount of memory is beeing allocated
 * (the caller should handle that!).
 *
 * Return values: Some PTP_RC_* code.
 * Upon success PTPContainer* ptp contains PTP Response Phase container with
 * all fields filled in.
 **/
uint16_t
ptp_transaction (PTPParams* params, PTPContainer* ptp,
                 uint16_t flags, unsigned int sendlen, char** data)
{
    if ((params==NULL) || (ptp==NULL))
        return PTP_ERROR_BADPARAM;
    
    ptp->Transaction_ID=params->transaction_id++;
    ptp->SessionID=params->session_id;
    /* send request */
    CHECK_PTP_RC(params->sendreq_func (params, ptp));
    /* is there a dataphase? */
    switch (flags&PTP_DP_DATA_MASK) {
        case PTP_DP_SENDDATA:
            CHECK_PTP_RC(params->senddata_func(params, ptp,
                                               (unsigned char*)*data, sendlen));
            break;
        case PTP_DP_GETDATA:
        {
            unsigned int getlen;
            CHECK_PTP_RC(params->getdata_func(params, ptp,
                                              sendlen?(unsigned int *)(((uint64_t)(&getlen)&0xffffffff00000000)|sendlen):
                                              &getlen,
                                              (unsigned char**)data));
        }
            break;
        case PTP_DP_NODATA:
            break;
        default:
            return PTP_ERROR_BADPARAM;
    }
    /* get response */
    CHECK_PTP_RC(params->getresp_func(params, ptp));
    return PTP_RC_OK;
}

/**
 * ptp_getdeviceinfo:
 * params:	PTPParams*
 *
 * Gets device info dataset and fills deviceinfo structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getdeviceinfo (PTPParams* params, PTPDeviceInfo* deviceinfo)
{
    uint16_t ret;
    PTPContainer ptp;
    char* di=NULL;
    
    ptp_debug(params,"PTP: Obtaining DeviceInfo");
    
    PTP_CNT_INIT(ptp);
    ptp.Code=PTP_OC_GetDeviceInfo;
    ptp.Nparam=0;
    ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &di);
    if (ret == PTP_RC_OK) ptp_unpack_DI(params, di, deviceinfo);
    free(di);
    return ret;
}

uint16_t
ptp_opensession (PTPParams* params, uint32_t session)
{
    uint16_t ret;
    PTPContainer ptp;
    
    ptp_debug(params,"PTP: Opening session 0x%08x", session);
    
    /* SessonID field of the operation dataset should always
     be set to 0 for OpenSession request! */
    params->session_id=0x00000000;
    /* TransactionID should be set to 0 also! */
    params->transaction_id=0x0000000;
    
    PTP_CNT_INIT(ptp);
    ptp.Code=PTP_OC_OpenSession;
    ptp.Param1=session;
    ptp.Nparam=1;
    ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL);
    /* now set the global session id to current session number */
    params->session_id=session;
    return ret;
}

/**
 * ptp_closesession:
 * params:	PTPParams*
 *
 * Closes session.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_closesession (PTPParams* params)
{
    PTPContainer ptp;
    
    ptp_debug(params,"PTP: Closing session");
    
    PTP_CNT_INIT(ptp);
    ptp.Code=PTP_OC_CloseSession;
    ptp.Nparam=0;
    return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL);
}

static void
ptp_error (PTPParams *params, const char *format, ...)
{
    va_list args;
    
    va_start (args, format);
    if (params->error_func!=NULL)
        params->error_func (params->data, format, args);
    else
    {
        vfprintf (stderr, format, args);
        fprintf (stderr,"\n");
        fflush (stderr);
    }
    va_end (args);
}

uint16_t
ptp_sendgenericrequest (PTPParams* params, uint16_t reqcode,
                        uint32_t* reqparams, char** data, uint32_t direction, long sendlen)
{
    PTPContainer ptp;
    uint16_t ret=0;
    char *dpv=NULL;
    
    if (direction == PTP_DP_GETDATA)
        *data = NULL;
    
    ptp_debug(params, "PTP: Sending generic Request 0x%04x", reqcode);
    
    PTP_CNT_INIT(ptp);
    ptp.Code=reqcode;
    ptp.Nparam=0;
    // carfull here, do not only rely on the nullity of a value,
    // because the next parameter might be not null
    if((ptp.Param1=reqparams[0]) != 0)
        ptp.Nparam = 1;
    if((ptp.Param2=reqparams[1]) != 0)
        ptp.Nparam = 2;
    if((ptp.Param3=reqparams[2]) != 0)
        ptp.Nparam = 3;
    if((ptp.Param4=reqparams[3]) != 0)
        ptp.Nparam = 4;
    if((ptp.Param5=reqparams[4]) != 0)
        ptp.Nparam = 5;
    
    ret=ptp_transaction(params, &ptp, direction, sendlen, data);
    return ret;
}

/* report PTP errors */

void
ptp_perror(PTPParams* params, uint16_t error) {
    
    int i;
    /* PTP error descriptions */
    static struct {
        uint16_t error;
        const char *txt;
    } ptp_errors[] = {
        {PTP_RC_Undefined, 		N_("PTP: Undefined Error")},
        {PTP_RC_OK, 			N_("PTP: OK!")},
        {PTP_RC_GeneralError, 		N_("PTP: General Error")},
        {PTP_RC_SessionNotOpen, 	N_("PTP: Session Not Open")},
        {PTP_RC_InvalidTransactionID, 	N_("PTP: Invalid Transaction ID")},
        {PTP_RC_OperationNotSupported, 	N_("PTP: Operation Not Supported")},
        {PTP_RC_ParameterNotSupported, 	N_("PTP: Parameter Not Supported")},
        {PTP_RC_IncompleteTransfer, 	N_("PTP: Incomplete Transfer")},
        {PTP_RC_InvalidStorageId, 	N_("PTP: Invalid Storage ID")},
        {PTP_RC_InvalidObjectHandle, 	N_("PTP: Invalid Object Handle")},
        {PTP_RC_DevicePropNotSupported, N_("PTP: Device Prop Not Supported")},
        {PTP_RC_InvalidObjectFormatCode, N_("PTP: Invalid Object Format Code")},
        {PTP_RC_StoreFull, 		N_("PTP: Store Full")},
        {PTP_RC_ObjectWriteProtected, 	N_("PTP: Object Write Protected")},
        {PTP_RC_StoreReadOnly, 		N_("PTP: Store Read Only")},
        {PTP_RC_AccessDenied,		N_("PTP: Access Denied")},
        {PTP_RC_NoThumbnailPresent, 	N_("PTP: No Thumbnail Present")},
        {PTP_RC_SelfTestFailed, 	N_("PTP: Self Test Failed")},
        {PTP_RC_PartialDeletion, 	N_("PTP: Partial Deletion")},
        {PTP_RC_StoreNotAvailable, 	N_("PTP: Store Not Available")},
        {PTP_RC_SpecificationByFormatUnsupported,
            N_("PTP: Specification By Format Unsupported")},
        {PTP_RC_NoValidObjectInfo, 	N_("PTP: No Valid Object Info")},
        {PTP_RC_InvalidCodeFormat, 	N_("PTP: Invalid Code Format")},
        {PTP_RC_UnknownVendorCode, 	N_("PTP: Unknown Vendor Code")},
        {PTP_RC_CaptureAlreadyTerminated,
            N_("PTP: Capture Already Terminated")},
        {PTP_RC_DeviceBusy, 		N_("PTP: Device Busy")},
        {PTP_RC_InvalidParentObject, 	N_("PTP: Invalid Parent Object")},
        {PTP_RC_InvalidDevicePropFormat, N_("PTP: Invalid Device Prop Format")},
        {PTP_RC_InvalidDevicePropValue, N_("PTP: Invalid Device Prop Value")},
        {PTP_RC_InvalidParameter, 	N_("PTP: Invalid Parameter")},
        {PTP_RC_SessionAlreadyOpened, 	N_("PTP: Session Already Opened")},
        {PTP_RC_TransactionCanceled, 	N_("PTP: Transaction Canceled")},
        {PTP_RC_SpecificationOfDestinationUnsupported,
            N_("PTP: Specification Of Destination Unsupported")},
        
        {PTP_ERROR_IO,		  N_("PTP: I/O error")},
        {PTP_ERROR_BADPARAM,	  N_("PTP: Error: bad parameter")},
        {PTP_ERROR_DATA_EXPECTED, N_("PTP: Protocol error: data expected")},
        {PTP_ERROR_RESP_EXPECTED, N_("PTP: Protocol error: response expected")},
        {0, NULL}
    };
    
    for (i=0; ptp_errors[i].txt!=NULL; i++)
        if (ptp_errors[i].error == error){
            ptp_error(params, ptp_errors[i].txt);
            return;
        }
    
    ptp_error(params, "PTP: Error 0x%04x", error);
    
}


/**
 * ptp_ptp_getobjectinfo:
 * params:	PTPParams*
 *		handle			- object Handler
 *		objectinfo		- pointer to PTPObjectInfo structure
 *
 * Fills objectinfo structure with appropriate data of object given by
 * hander.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobjectinfo (PTPParams* params, uint32_t handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	char* oi=NULL;

	ptp_debug(params,"PTP: Obtaining ObjectInfo for object 0x%08x",
		handle);

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectInfo;
	ptp.Param1=handle;
	ptp.Nparam=1;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &oi);
	if (ret == PTP_RC_OK) ptp_unpack_OI(params, oi, objectinfo);
	free(oi);
	return ret;
}

uint16_t
ptp_getobject (PTPParams* params, uint32_t handle, char** object)
{
	PTPContainer ptp;

	ptp_debug(params,"PTP: Downloading Object 0x%08x",
		handle);

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object);
}