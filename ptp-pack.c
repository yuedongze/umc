/* ptp-pack.c
 *
 * Copyright (C) 2002-2005 Mariusz Woloszyn <emsi@ipartners.pl>
 *
 *  This file is part of libptp2.
 *
 *  libptp2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libptp2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libptp2; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* curently this file is included into ptp.c */

static inline uint16_t
htod16p (PTPParams *params, uint16_t var)
{
	return ((params->byteorder==PTP_DL_LE)?htole16(var):htobe16(var));
}

static inline uint32_t
htod32p (PTPParams *params, uint32_t var)
{
	return ((params->byteorder==PTP_DL_LE)?htole32(var):htobe32(var));
}

static inline void
htod16ap (PTPParams *params, unsigned char *a, uint16_t val)
{
	if (params->byteorder==PTP_DL_LE)
		htole16a(a,val); else 
		htobe16a(a,val);
}

static inline void
htod32ap (PTPParams *params, unsigned char *a, uint32_t val)
{
	if (params->byteorder==PTP_DL_LE)
		htole32a(a,val); else 
		htobe32a(a,val);
}

static inline uint16_t
dtoh16p (PTPParams *params, uint16_t var)
{
	return ((params->byteorder==PTP_DL_LE)?le16toh(var):be16toh(var));
}

static inline uint32_t
dtoh32p (PTPParams *params, uint32_t var)
{
	return ((params->byteorder==PTP_DL_LE)?le32toh(var):be32toh(var));
}

static inline uint16_t
dtoh16ap (PTPParams *params, unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le16atoh(a):be16atoh(a));
}

static inline uint32_t
dtoh32ap (PTPParams *params, unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le32atoh(a):be32atoh(a));
}

#define htod8a(a,x)	*(uint8_t*)(a) = x
#define htod16a(a,x)	htod16ap(params,(unsigned char *)a,x)
#define htod32a(a,x)	htod32ap(params,(unsigned char *)a,x)
#define htod16(x)	htod16p(params,x)
#define htod32(x)	htod32p(params,x)

#define dtoh8a(x)	(*(uint8_t*)(x))
#define dtoh16a(a)	dtoh16ap(params,(unsigned char *)a)
#define dtoh32a(a)	dtoh32ap(params,(unsigned char *)a)
#define dtoh16(x)	dtoh16p(params,x)
#define dtoh32(x)	dtoh32p(params,x)


static inline char*
ptp_unpack_string(PTPParams *params, char* data, uint16_t offset, uint8_t *len)
{
	int i;
	char *string=NULL;

	*len=dtoh8a(&data[offset]);
	if (*len) {
		string=malloc(*len);
		memset(string, 0, *len);
		for (i=0;i<*len && i< PTP_MAXSTRLEN; i++) {
			string[i]=(char)dtoh16a(&data[offset+i*2+1]);
		}
		/* be paranoid! :( */
		string[*len-1]=0;
	}
	return (string);
}

static inline void
ptp_pack_string(PTPParams *params, char *string, char* data, uint16_t offset, uint8_t *len)
{
	int i;
	*len = (uint8_t)strlen(string);
	
	/* XXX: check strlen! */
	htod8a(&data[offset],*len+1);
	for (i=0;i<*len && i< PTP_MAXSTRLEN; i++) {
		htod16a(&data[offset+i*2+1],(uint16_t)string[i]);
	}
}

static inline uint32_t
ptp_unpack_uint32_t_array(PTPParams *params, char* data, uint16_t offset, uint32_t **array)
{
	uint32_t n, i=0;

	n=dtoh32a(&data[offset]);
	*array = malloc (n*sizeof(uint32_t));
	while (n>i) {
		(*array)[i]=dtoh32a(&data[offset+(sizeof(uint32_t)*(i+1))]);
		i++;
	}
	return n;
}

static inline uint32_t
ptp_unpack_uint16_t_array(PTPParams *params, char* data, uint16_t offset, uint16_t **array)
{
	uint32_t n, i=0;

	n=dtoh32a(&data[offset]);
	*array = malloc (n*sizeof(uint16_t));
	while (n>i) {
		(*array)[i]=dtoh16a(&data[offset+(sizeof(uint16_t)*(i+2))]);
		i++;
	}
	return n;
}

/* DeviceInfo pack/unpack */

#define PTP_di_StandardVersion		 0
#define PTP_di_VendorExtensionID	 2
#define PTP_di_VendorExtensionVersion	 6
#define PTP_di_VendorExtensionDesc	 8
#define PTP_di_FunctionalMode		 8
#define PTP_di_OperationsSupported	10

static inline void
ptp_unpack_DI (PTPParams *params, char* data, PTPDeviceInfo *di)
{
	uint8_t len;
	unsigned int totallen;
	
	di->StaqndardVersion = dtoh16a(&data[PTP_di_StandardVersion]);
	di->VendorExtensionID =
		dtoh32a(&data[PTP_di_VendorExtensionID]);
	di->VendorExtensionVersion =
		dtoh16a(&data[PTP_di_VendorExtensionVersion]);
	di->VendorExtensionDesc = 
		ptp_unpack_string(params, data,
		PTP_di_VendorExtensionDesc, &len); 
	totallen=len*2+1;
	di->FunctionalMode = 
		dtoh16a(&data[PTP_di_FunctionalMode+totallen]);
	di->OperationsSupported_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->OperationsSupported);
	totallen=totallen+di->OperationsSupported_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->EventsSupported_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->EventsSupported);
	totallen=totallen+di->EventsSupported_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->DevicePropertiesSupported_len =
		ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->DevicePropertiesSupported);
	totallen=totallen+di->DevicePropertiesSupported_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->CaptureFormats_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->CaptureFormats);
	totallen=totallen+di->CaptureFormats_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->ImageFormats_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->ImageFormats);
	totallen=totallen+di->ImageFormats_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->Manufacturer = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
	totallen+=len*2+1;
	di->Model = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
	totallen+=len*2+1;
	di->DeviceVersion = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
	totallen+=len*2+1;
	di->SerialNumber = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
}

/* ObjectInfo pack/unpack */

#define PTP_oi_StorageID		 0
#define PTP_oi_ObjectFormat		 4
#define PTP_oi_ProtectionStatus		 6
#define PTP_oi_ObjectCompressedSize	 8
#define PTP_oi_ThumbFormat		12
#define PTP_oi_ThumbCompressedSize	14
#define PTP_oi_ThumbPixWidth		18
#define PTP_oi_ThumbPixHeight		22
#define PTP_oi_ImagePixWidth		26
#define PTP_oi_ImagePixHeight		30
#define PTP_oi_ImageBitDepth		34
#define PTP_oi_ParentObject		38
#define PTP_oi_AssociationType		42
#define PTP_oi_AssociationDesc		44
#define PTP_oi_SequenceNumber		48
#define PTP_oi_filenamelen		52
#define PTP_oi_Filename			53

static inline uint32_t
ptp_pack_OI (PTPParams *params, PTPObjectInfo *oi, char** oidataptr)
{
	char* oidata;
	uint8_t filenamelen;
	uint8_t capturedatelen=0;
	/* let's allocate some memory first; XXX i'm sure it's wrong */
	oidata=malloc(PTP_oi_Filename+(strlen(oi->Filename)+1)*2+4);
	/* the caller should free it after use! */
#if 0
	char *capture_date="20020101T010101"; /* XXX Fake date */
#endif
	memset (oidata, 0, (PTP_oi_Filename+(strlen(oi->Filename)+1)*2+4));
	htod32a(&oidata[PTP_oi_StorageID],oi->StorageID);
	htod16a(&oidata[PTP_oi_ObjectFormat],oi->ObjectFormat);
	htod16a(&oidata[PTP_oi_ProtectionStatus],oi->ProtectionStatus);
	htod32a(&oidata[PTP_oi_ObjectCompressedSize],oi->ObjectCompressedSize);
	htod16a(&oidata[PTP_oi_ThumbFormat],oi->ThumbFormat);
	htod32a(&oidata[PTP_oi_ThumbCompressedSize],oi->ThumbCompressedSize);
	htod32a(&oidata[PTP_oi_ThumbPixWidth],oi->ThumbPixWidth);
	htod32a(&oidata[PTP_oi_ThumbPixHeight],oi->ThumbPixHeight);
	htod32a(&oidata[PTP_oi_ImagePixWidth],oi->ImagePixWidth);
	htod32a(&oidata[PTP_oi_ImagePixHeight],oi->ImagePixHeight);
	htod32a(&oidata[PTP_oi_ImageBitDepth],oi->ImageBitDepth);
	htod32a(&oidata[PTP_oi_ParentObject],oi->ParentObject);
	htod16a(&oidata[PTP_oi_AssociationType],oi->AssociationType);
	htod32a(&oidata[PTP_oi_AssociationDesc],oi->AssociationDesc);
	htod32a(&oidata[PTP_oi_SequenceNumber],oi->SequenceNumber);
	
	ptp_pack_string(params, oi->Filename, oidata, PTP_oi_filenamelen, &filenamelen);
/*
	filenamelen=(uint8_t)strlen(oi->Filename);
	htod8a(&req->data[PTP_oi_filenamelen],filenamelen+1);
	for (i=0;i<filenamelen && i< PTP_MAXSTRLEN; i++) {
		req->data[PTP_oi_Filename+i*2]=oi->Filename[i];
	}
*/
	/*
	 *XXX Fake date.
	 * for example Kodak sets Capture date on the basis of EXIF data.
	 * Spec says that this field is from perspective of Initiator.
	 */
#if 0	/* seems now we don't need any data packed in OI dataset... for now ;)*/
	capturedatelen=strlen(capture_date);
	htod8a(&data[PTP_oi_Filename+(filenamelen+1)*2],
		capturedatelen+1);
	for (i=0;i<capturedatelen && i< PTP_MAXSTRLEN; i++) {
		data[PTP_oi_Filename+(i+filenamelen+1)*2+1]=capture_date[i];
	}
	htod8a(&data[PTP_oi_Filename+(filenamelen+capturedatelen+2)*2+1],
		capturedatelen+1);
	for (i=0;i<capturedatelen && i< PTP_MAXSTRLEN; i++) {
		data[PTP_oi_Filename+(i+filenamelen+capturedatelen+2)*2+2]=
		  capture_date[i];
	}
#endif
	/* XXX this function should return dataset length */
	
	*oidataptr=oidata;
	return (PTP_oi_Filename+(filenamelen+1)*2+(capturedatelen+1)*4);
}

static inline void
ptp_unpack_OI (PTPParams *params, char* data, PTPObjectInfo *oi)
{
	uint8_t filenamelen;
	uint8_t capturedatelen;
	char *capture_date;
	char tmp[16];
	struct tm tm;

	memset(&tm,0,sizeof(tm));

	oi->StorageID=dtoh32a(&data[PTP_oi_StorageID]);
	oi->ObjectFormat=dtoh16a(&data[PTP_oi_ObjectFormat]);
	oi->ProtectionStatus=dtoh16a(&data[PTP_oi_ProtectionStatus]);
	oi->ObjectCompressedSize=dtoh32a(&data[PTP_oi_ObjectCompressedSize]);
	oi->ThumbFormat=dtoh16a(&data[PTP_oi_ThumbFormat]);
	oi->ThumbCompressedSize=dtoh32a(&data[PTP_oi_ThumbCompressedSize]);
	oi->ThumbPixWidth=dtoh32a(&data[PTP_oi_ThumbPixWidth]);
	oi->ThumbPixHeight=dtoh32a(&data[PTP_oi_ThumbPixHeight]);
	oi->ImagePixWidth=dtoh32a(&data[PTP_oi_ImagePixWidth]);
	oi->ImagePixHeight=dtoh32a(&data[PTP_oi_ImagePixHeight]);
	oi->ImageBitDepth=dtoh32a(&data[PTP_oi_ImageBitDepth]);
	oi->ParentObject=dtoh32a(&data[PTP_oi_ParentObject]);
	oi->AssociationType=dtoh16a(&data[PTP_oi_AssociationType]);
	oi->AssociationDesc=dtoh32a(&data[PTP_oi_AssociationDesc]);
	oi->SequenceNumber=dtoh32a(&data[PTP_oi_SequenceNumber]);
	oi->Filename= ptp_unpack_string(params, data, PTP_oi_filenamelen, &filenamelen);

	capture_date = ptp_unpack_string(params, data,
		PTP_oi_filenamelen+filenamelen*2+1, &capturedatelen);
	/* subset of ISO 8601, without '.s' tenths of second and 
	 * time zone
	 */
	if (capturedatelen>15)
	{
		strncpy (tmp, capture_date, 4);
		tmp[4] = 0;
		tm.tm_year=atoi (tmp) - 1900;
		strncpy (tmp, capture_date + 4, 2);
		tmp[2] = 0;
		tm.tm_mon = atoi (tmp) - 1;
		strncpy (tmp, capture_date + 6, 2);
		tmp[2] = 0;
		tm.tm_mday = atoi (tmp);
		strncpy (tmp, capture_date + 9, 2);
		tmp[2] = 0;
		tm.tm_hour = atoi (tmp);
		strncpy (tmp, capture_date + 11, 2);
		tmp[2] = 0;
		tm.tm_min = atoi (tmp);
		strncpy (tmp, capture_date + 13, 2);
		tmp[2] = 0;
		tm.tm_sec = atoi (tmp);
		oi->CaptureDate=mktime (&tm);
	}
	free(capture_date);

	/* now it's modification date ;) */
	capture_date = ptp_unpack_string(params, data,
		PTP_oi_filenamelen+filenamelen*2
		+capturedatelen*2+2,&capturedatelen);
	if (capturedatelen>15)
	{
		strncpy (tmp, capture_date, 4);
		tmp[4] = 0;
		tm.tm_year=atoi (tmp) - 1900;
		strncpy (tmp, capture_date + 4, 2);
		tmp[2] = 0;
		tm.tm_mon = atoi (tmp) - 1;
		strncpy (tmp, capture_date + 6, 2);
		tmp[2] = 0;
		tm.tm_mday = atoi (tmp);
		strncpy (tmp, capture_date + 9, 2);
		tmp[2] = 0;
		tm.tm_hour = atoi (tmp);
		strncpy (tmp, capture_date + 11, 2);
		tmp[2] = 0;
		tm.tm_min = atoi (tmp);
		strncpy (tmp, capture_date + 13, 2);
		tmp[2] = 0;
		tm.tm_sec = atoi (tmp);
		oi->ModificationDate=mktime (&tm);
	}
	free(capture_date);
}
