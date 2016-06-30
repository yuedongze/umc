//
//  ptp.h
//  umc
//
//  Created by Steven Yue on 16/6/28.
//  Copyright (c) 2016年 Steven Yue. All rights reserved.
//

#ifndef umc_ptp_h
#define umc_ptp_h

#include <stdarg.h>
#include <time.h>

#endif

/* PTP datalayer byteorder */

#define PTP_DL_BE			0xF0
#define	PTP_DL_LE			0x0F

#define PTP_USB_BULK_HS_MAX_PACKET_LEN	512
#define PTP_USB_BULK_HDR_LEN		(2*sizeof(uint32_t)+2*sizeof(uint16_t))
#define PTP_USB_BULK_PAYLOAD_LEN	(PTP_USB_BULK_HS_MAX_PACKET_LEN-PTP_USB_BULK_HDR_LEN)
#define PTP_USB_BULK_REQ_LEN	(PTP_USB_BULK_HDR_LEN+5*sizeof(uint32_t))

/* libptp2 extended ERROR codes */
#define PTP_ERROR_IO			0x02FF
#define PTP_ERROR_DATA_EXPECTED		0x02FE
#define PTP_ERROR_RESP_EXPECTED		0x02FD
#define PTP_ERROR_BADPARAM		0x02FC

#include <byteswap.h>
#include <arpa/inet.h>

#define swap16(x) NXSwapShort(x)
#define swap32(x) NXSwapLong(x)
#define swap64(x) NXSwapLongLong(x)

/* Define aliases for the standard byte swapping macros */
/* Arguments to these macros must be properly aligned on natural word */
/* boundaries in order to work properly on all architectures */
#define htobe16(x) htons(x)
#define htobe32(x) htonl(x)
#define be16toh(x) ntohs(x)
#define be32toh(x) ntohl(x)

#define HTOBE16(x) (x) = htobe16(x)
#define HTOBE32(x) (x) = htobe32(x)
#define BE32TOH(x) (x) = be32toh(x)
#define BE16TOH(x) (x) = be16toh(x)

/* On little endian machines, these macros are null */
#define htole16(x)      (x)
#define htole32(x)      (x)
#define htole64(x)      (x)
#define le16toh(x)      (x)
#define le32toh(x)      (x)
#define le64toh(x)      (x)

#define HTOLE16(x)      (void) (x)
#define HTOLE32(x)      (void) (x)
#define HTOLE64(x)      (void) (x)
#define LE16TOH(x)      (void) (x)
#define LE32TOH(x)      (void) (x)
#define LE64TOH(x)      (void) (x)

/* These don't have standard aliases */
#define htobe64(x)      swap64(x)
#define be64toh(x)      swap64(x)

#define HTOBE64(x)      (x) = htobe64(x)
#define BE64TOH(x)      (x) = be64toh(x)

#define be16atoh(x)     ((uint16_t)(((x)[0]<<8)|(x)[1]))
#define be32atoh(x)     ((uint32_t)(((x)[0]<<24)|((x)[1]<<16)|((x)[2]<<8)|(x)[3]))
#define be64atoh(x)     ((uint64_t)(((x)[0]<<56)|((x)[1]<<48)|((x)[2]<<40)| \
((x)[3]<<32)|((x)[4]<<24)|((x)[5]<<16)|((x)[6]<<8)|(x)[7]))
#define le16atoh(x)     ((uint16_t)(((x)[1]<<8)|(x)[0]))
#define le32atoh(x)     ((uint32_t)(((x)[3]<<24)|((x)[2]<<16)|((x)[1]<<8)|(x)[0]))
#define le64atoh(x)     ((uint64_t)(((x)[7]<<56)|((x)[6]<<48)|((x)[5]<<40)| \
((x)[4]<<32)|((x)[3]<<24)|((x)[2]<<16)|((x)[1]<<8)|(x)[0]))

#define htobe16a(a,x)   (a)[0]=(uint8_t)((x)>>8), (a)[1]=(uint8_t)(x)
#define htobe32a(a,x)   (a)[0]=(uint8_t)((x)>>24), (a)[1]=(uint8_t)((x)>>16), \
(a)[2]=(uint8_t)((x)>>8), (a)[3]=(uint8_t)(x)
#define htobe64a(a,x)   (a)[0]=(uint8_t)((x)>>56), (a)[1]=(uint8_t)((x)>>48), \
(a)[2]=(uint8_t)((x)>>40), (a)[3]=(uint8_t)((x)>>32), \
(a)[4]=(uint8_t)((x)>>24), (a)[5]=(uint8_t)((x)>>16), \
(a)[6]=(uint8_t)((x)>>8), (a)[7]=(uint8_t)(x)
#define htole16a(a,x)   (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)
#define htole32a(a,x)   (a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), \
(a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)
#define htole64a(a,x)   (a)[7]=(uint8_t)((x)>>56), (a)[6]=(uint8_t)((x)>>48), \
(a)[5]=(uint8_t)((x)>>40), (a)[4]=(uint8_t)((x)>>32), \
(a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), \
(a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)

struct _PTPUSBBulkContainer {
    uint32_t length;
    uint16_t type;
    uint16_t code;
    uint32_t trans_id;
    union {
        struct {
            uint32_t param1;
            uint32_t param2;
            uint32_t param3;
            uint32_t param4;
            uint32_t param5;
        } params;
        unsigned char data[PTP_USB_BULK_PAYLOAD_LEN];
    } payload;
};
typedef struct _PTPUSBBulkContainer PTPUSBBulkContainer;










/* PTP request/response/event general PTP container (transport independent) */

struct _PTPContainer {
    uint16_t Code;
    uint32_t SessionID;
    uint32_t Transaction_ID;
    /* params  may be of any type of size less or equal to uint32_t */
    uint32_t Param1;
    uint32_t Param2;
    uint32_t Param3;
    /* events can only have three parameters */
    uint32_t Param4;
    uint32_t Param5;
    /* the number of meaningfull parameters */
    uint8_t	 Nparam;
};
typedef struct _PTPContainer PTPContainer;


/* PTP device info structure (returned by GetDevInfo) */

struct _PTPDeviceInfo {
    uint16_t StaqndardVersion;
    uint32_t VendorExtensionID;
    uint16_t VendorExtensionVersion;
    char	*VendorExtensionDesc;
    uint16_t FunctionalMode;
    uint32_t OperationsSupported_len;
    uint16_t *OperationsSupported;
    uint32_t EventsSupported_len;
    uint16_t *EventsSupported;
    uint32_t DevicePropertiesSupported_len;
    uint16_t *DevicePropertiesSupported;
    uint32_t CaptureFormats_len;
    uint16_t *CaptureFormats;
    uint32_t ImageFormats_len;
    uint16_t *ImageFormats;
    char	*Manufacturer;
    char	*Model;
    char	*DeviceVersion;
    char	*SerialNumber;
};
typedef struct _PTPDeviceInfo PTPDeviceInfo;



/* PTP objectinfo structure (returned by GetObjectInfo) */

struct _PTPObjectInfo {
    uint32_t StorageID;
    uint16_t ObjectFormat;
    uint16_t ProtectionStatus;
    uint32_t ObjectCompressedSize;
    uint16_t ThumbFormat;
    uint32_t ThumbCompressedSize;
    uint32_t ThumbPixWidth;
    uint32_t ThumbPixHeight;
    uint32_t ImagePixWidth;
    uint32_t ImagePixHeight;
    uint32_t ImageBitDepth;
    uint32_t ParentObject;
    uint16_t AssociationType;
    uint32_t AssociationDesc;
    uint32_t SequenceNumber;
    char 	*Filename;
    time_t	CaptureDate;
    time_t	ModificationDate;
    char	*Keywords;
};
typedef struct _PTPObjectInfo PTPObjectInfo;

/* PTP objecthandles structure (returned by GetObjectHandles) */

struct _PTPObjectHandles {
    uint32_t n;
    uint32_t *Handler;
};
typedef struct _PTPObjectHandles PTPObjectHandles;

typedef struct _PTPParams PTPParams;

/* raw write functions */
typedef short (* PTPIOReadFunc)	(unsigned char *bytes, unsigned int size,
void *data);
typedef short (* PTPIOWriteFunc)(unsigned char *bytes, unsigned int size,
void *data);
/*
 * This functions take PTP oriented arguments and send them over an
 * appropriate data layer doing byteorder conversion accordingly.
 */
typedef uint16_t (* PTPIOSendReq)	(PTPParams* params, PTPContainer* req);
typedef uint16_t (* PTPIOSendData)	(PTPParams* params, PTPContainer* ptp,
                                     unsigned char *data, unsigned int size);
typedef uint16_t (* PTPIOGetResp)	(PTPParams* params, PTPContainer* resp);
typedef uint16_t (* PTPIOGetData)	(PTPParams* params, PTPContainer* ptp,
                                     uint32_t *getlen,
                                     unsigned char **data);

typedef void (* PTPErrorFunc) (void *data, const char *format, va_list args);
typedef void (* PTPDebugFunc) (void *data, const char *format, va_list args);

struct _PTPParams {
    /* data layer byteorder */
    uint8_t	byteorder;
    
    /* Data layer IO functions */
    PTPIOReadFunc	read_func;
    PTPIOWriteFunc	write_func;
    PTPIOReadFunc	check_int_func;
    PTPIOReadFunc	check_int_fast_func;
    
    /* Custom IO functions */
    PTPIOSendReq	sendreq_func;
    PTPIOSendData	senddata_func;
    PTPIOGetResp	getresp_func;
    PTPIOGetData	getdata_func;
    PTPIOGetResp	event_check;
    PTPIOGetResp	event_wait;
    
    /* Custom error and debug function */
    PTPErrorFunc error_func;
    PTPDebugFunc debug_func;
    
    /* Data passed to above functions */
    void *data;
    
    /* ptp transaction ID */
    uint32_t transaction_id;
    /* ptp session ID */
    uint32_t session_id;
    
    /* internal structures used by ptp driver */
    PTPObjectHandles handles;
    PTPObjectInfo * objectinfo;
    PTPDeviceInfo deviceinfo;
};


uint16_t ptp_usb_sendreq	(PTPParams* params, PTPContainer* req);
uint16_t ptp_usb_senddata	(PTPParams* params, PTPContainer* ptp,
                             unsigned char *data, unsigned int size);
uint16_t ptp_usb_getresp	(PTPParams* params, PTPContainer* resp);
uint16_t ptp_usb_getdata	(PTPParams* params, PTPContainer* ptp,
                             unsigned int *getlen, unsigned char **data);
uint16_t ptp_usb_event_check	(PTPParams* params, PTPContainer* event);
uint16_t ptp_usb_event_wait		(PTPParams* params, PTPContainer* event);

uint16_t ptp_getdeviceinfo	(PTPParams* params, PTPDeviceInfo* deviceinfo);

uint16_t ptp_opensession	(PTPParams *params, uint32_t session);
uint16_t ptp_closesession	(PTPParams *params);

uint16_t ptp_sendgenericrequest (PTPParams* params, uint16_t reqcode,
                                 uint32_t* reqparams, char** data,
                                 uint32_t direction, long sendlen);

void ptp_perror			(PTPParams* params, uint16_t error);

uint16_t ptp_getobjectinfo	(PTPParams *params, uint32_t handle,
				PTPObjectInfo* objectinfo);

uint16_t ptp_getobject		(PTPParams *params, uint32_t handle,
				char** object);
