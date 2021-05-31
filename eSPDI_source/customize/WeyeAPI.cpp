#include "stdafx.h"
#include "WeyeAPI.h"
#include "EtronDI.h"
#include <string>
#include <stdlib.h>
#include <unistd.h>
#ifdef __linux__
#include "eSPDI.h"
#else
#include "EtronDI_O.h"
#endif
#include "En-Decrypt.h"
#include "Mp4FileUtility.h"
#include "libeysov/fisheye360_def.h"

#include "../debug.h"
#include "../eSPDI_version.h"
const static char sdk_version[] = ETRONDI_VERSION;

static char g_szSerialNumber[32];
static BYTE* g_pLutBuffer = NULL;

int EtronDI_Weye_GetSerialNumber2(void* pHandleEtronDI, char* serialNumber, int* snLength);


bool IsWeyeSupportedDevice(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{

ALOGV("IsWeyeSupportedDevice"); 
    char fwVer[256] = {'\0'};
    int actualSize = 0;
    int ret = EtronDI_GetFwVersion(pHandleEtronDI, pDevSelInfo, fwVer, 255, &actualSize);

    std::string fwVersion(fwVer);

    return fwVersion.substr(0, fwVersion.find('-')) == std::string("EX8039");
}

int EtronDI_Weye_Activate(void** ppHandleEtronDI, bool bIsLogEnabled)
{
ALOGV("EtronDI_Weye_Activate version %s", sdk_version);
    void* pHandle = NULL;

    int ret = EtronDI_Init(&pHandle, bIsLogEnabled);
    if (ret != ETronDI_OK)
    {
    	return ret;
    }

    DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
    if (IsWeyeSupportedDevice(pHandle, &devSelInfo))
    {

	int deviceCount = EtronDI_GetDeviceNumber(pHandle);
        if (deviceCount == 1)
        {
            ret = EtronDI_SetHWRegister(pHandle, &devSelInfo, 0xF1F9, 0x07, FG_Address_2Byte | FG_Value_1Byte);
ALOGD("EtronDI_SetHWRegister 0xF1F9, 0x07, return %d", ret);
            if (ret == ETronDI_OK)
            {
                *ppHandleEtronDI = pHandle;
				//Get serial number 
				memset(g_szSerialNumber, 0, sizeof(g_szSerialNumber));
				int len = sizeof(g_szSerialNumber);
				ret = EtronDI_Weye_GetSerialNumber2(pHandle, g_szSerialNumber, &len);
				if (ret != ETronDI_OK) {
					ALOGE("EtronDI_Weye_GetSerialNumber2 failed!\n");
				}
				//get LUT
				if (!g_pLutBuffer) 
					g_pLutBuffer = (BYTE*) malloc(sizeof(eys::fisheye360::ParaLUT));

				if(ETronDI_OK != EtronDI_GetLutData(pHandle, &devSelInfo, g_pLutBuffer, sizeof(eys::fisheye360::ParaLUT))) {
					memset(g_pLutBuffer, 0, sizeof(eys::fisheye360::ParaLUT));
					ALOGE("EtronDI_GetLutData failed \n");
				}
				
                return ETronDI_OK;
            }
        }
        else
        {
            if (deviceCount == 0)
            {
                ret = ETronDI_NoDevice;
            }
            else if (deviceCount > 1)
            {
                ret = ETronDI_TOO_MANY_DEVICE;
            }
        }
    }
    else
    {
        ret = ETronDI_DEVICE_NOT_SUPPORT;
    }
ALOGD("EtronDI_Release");
    EtronDI_Release(&pHandle);

    return ret;
}

void EtronDI_Weye_Deactivate(void** ppHandleEtronDI)
{
ALOGV("EtronDI_Weye_Deactivate");
    if (ppHandleEtronDI == NULL || *ppHandleEtronDI == NULL)
    {
        return;
    }

    DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
    EtronDI_SetHWRegister(*ppHandleEtronDI, &devSelInfo, 0xF1F9, 0x00, FG_Address_2Byte | FG_Value_1Byte);
//
	if (g_pLutBuffer) {
		free(g_pLutBuffer);
		g_pLutBuffer = NULL;
	}				

    EtronDI_Release(ppHandleEtronDI);
}

int EtronDI_Weye_SaveLutData(void* pHandleEtronDI, const char* filename)
{
    DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
    ALOGV("EtronDI_Weye_SaveLutData\n");
    return EtronDI_SaveLutData(pHandleEtronDI, &devSelInfo, filename);

}
int EtronDI_Weye_GenerateLutFile(void* pHandleEtronDI, const char* filename)
{
#if 0 
    DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
ALOGV("EtronDI_Weye_GenerateLutFile  %s", filename);
    //for Weye project, force to use SaveLutData 
    return EtronDI_SaveLutData(pHandleEtronDI, &devSelInfo, filename);
#endif
	//preload LUT data to ensure LUT file can be generated successfully
	ALOGV("EtronDI_Weye_GenerateLutFile  %s", filename);

	FILE* fp = NULL;
	
	fp = fopen(filename, "wb");
	if (fp == NULL)
		return ETronDI_OPEN_DEVICE_FAIL;
	int err = fwrite(g_pLutBuffer, sizeof(eys::fisheye360::ParaLUT), 1, fp);
	if( err != 1) 
		ALOGE("Write LUT file %s failed!\n", filename);

	fclose(fp);
	return (err ==1)?ETronDI_OK : ETronDI_WRITE_REG_FAIL; 
}

int EtronDI_Weye_EncryptMP4(void* pHandleEtronDI, const char* filename)
{
ALOGV("EtronDI_Weye_EncryptMP4 %s", filename);

	return En_DecryptionMP4(filename, true);
}

int EtronDI_Weye_EncryptMP4Ex(void* pHandleEtronDI, const char* filename, const char* machineId, const char* serialNumber)
{
ALOGV("EtronDI_Weye_EncryptMP4Ex %s", filename);
#if 0 
	DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
	
	if (machineId != NULL && serialNumber != NULL)
	{
		// char0~char7: last 8 chars of serial number
		// char8~char15: machine id. fill the remainder chars with '\0'
		char injectData[16] = {0};
		memcpy(&injectData[0], &serialNumber[strlen((char*)serialNumber) - 8], 8);
		memcpy(&injectData[8], machineId, strlen(machineId));
		int ret = EtronDI_InjectExtraDataToMp4(pHandleEtronDI, &devSelInfo, filename, injectData, 16);
		if (ret != ETronDI_OK)
		{
			return ret;
		}
	}

    return EtronDI_EncryptMP4(pHandleEtronDI, &devSelInfo, filename);
#endif 
	if (machineId != NULL && serialNumber != NULL)
	{
	
		char injectData[16] = {0};
		memcpy(&injectData[0], &serialNumber[strlen((char*)serialNumber) - 8], 8);
		memcpy(&injectData[8], machineId, strlen(machineId));
		
		std::vector<char> injectData2(16, 0);
		memcpy(&injectData2[0], injectData, 16);
		bool ret = AccessMp4FileExtraData(filename, injectData2, true);
		if ( !ret)
		{
			return ETronDI_ACCESS_MP4_EXTRA_DATA_FAIL;
		}

	}
	return En_DecryptionMP4(filename, true);
}


int EtronDI_Weye_GetSerialNumber(void* pHandleEtronDI, char* serialNumber, int* snLength)
{
	int nLength =*snLength;
	if(nLength > sizeof(g_szSerialNumber)) nLength =  sizeof(g_szSerialNumber);
	memcpy(serialNumber,  g_szSerialNumber,  nLength);
	return ETronDI_OK;
}

int EtronDI_Weye_GetSerialNumber2(void* pHandleEtronDI, char* serialNumber, int* snLength)
{
	DEVSELINFO devSelInfo;
    devSelInfo.index = 0;

	unsigned char sn[512] = {0};
	int len = 512;
    int ret = EtronDI_GetSerialNumber(pHandleEtronDI, &devSelInfo, sn, len, &len);
	if (ret != ETronDI_OK)
	{
		return ret;
	}
	
	if (*snLength < len / 2)
	{
		*snLength = len / 2;
		return ETronDI_ErrBufLen;
	}
	
	// trans wchar to char
	for (int i = 0; i < len; i += 2)
	{
		serialNumber[i / 2] = sn[i];
	}
	serialNumber[len / 2] = '\0';
	ALOGV("EtronDI_Weye_GetSerialNumber %s\n", serialNumber);
	
	return ETronDI_OK;
}


int EtronDI_Weye_SetAutoExposureMode(void* pHandleEtronDI, unsigned short mode)
{
    DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
ALOGV("EtronDI_Weye_SetAutoExposureMode mode=%d", mode);
    return EtronDI_SetAutoExposureMode(pHandleEtronDI, &devSelInfo, mode);
}

#ifdef __linux__
// for windows, need to find definition of the id that will be set to Property.Id of the structure KSPROPERTY_VIDEOPROCAMP_S
int EtronDI_Weye_SetCTPropVal(void* pHandleEtronDI, int id, int value)
{
    DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
ALOGV(" EtronDI_Weye_SetCTPropVal  id=%d value = %d", id, value);
    switch (id)
    {
    case CT_PROPERTY_ID_AUTO_EXPOSURE_MODE_CTRL:
        {
			// 0: for auto
			// 1: for manual
        
 #if 0 //this is not work by SetCTPropVal, use EnableAE instead.       
            if (value != 0 && value != 1)
            {
                return ETronDI_SET_CT_PROP_VAL_FAIL;
            }
			break;
#endif
			if (value == 0)
				return EtronDI_EnableAE(pHandleEtronDI, &devSelInfo);
			return EtronDI_DisableAE(pHandleEtronDI, &devSelInfo);
        }
    case CT_PROPERTY_ID_EXPOSURE_TIME_ABSOLUTE_CTRL:
        {
            if (value > 9 || value < 0)
            {
                return ETronDI_SET_CT_PROP_VAL_FAIL;   
            }
            value = -13 + (int)((double)(value) * 16.0 / 9.0);//mapping from [0~9] to [-13~3]
            break;
        }
    default:
        break;
    }

    return EtronDI_SetCTPropVal(pHandleEtronDI, &devSelInfo, id, value);
}

int EtronDI_Weye_SetPUPropVal(void* pHandleEtronDI, int id, int value)
{
	DEVSELINFO devSelInfo;
    devSelInfo.index = 0;
ALOGV("EtronDI_Weye_SetPUPropVal id=%d v=%d enter", id, value);
	
	// id: PU_PROPERTY_ID_POWER_LINE_FREQUENCY_CTRL
	// value: 1 for 50HZ
	// value: 2 for 60HZ
	
	return EtronDI_SetPUPropVal(pHandleEtronDI, &devSelInfo, id, value);
}
#endif

int EtronDI_Weye_EncryptString(const char* src, char* dst)
{
	ALOGV("EtronDI_Weye_EncryptString");

	return EtronDI_EncryptString(src, dst);
}

int EtronDI_Weye_DecryptString(const char* src, char* dst)
{
	ALOGV("EtronDI_Weye_DecryptString");

	return EtronDI_DecryptString(src, dst);
}

int ETRONDI_WEYE_API EtronDI_Weye_EncryptTraceCodeAndPassword(
	const char* traceCode, const char* password, char* result)
{
	ALOGV("EtronDI_Weye_EncryptTraceCodeAndPassword");

	return EtronDI_EncryptString(traceCode, password, result);
}

int ETRONDI_WEYE_API EtronDI_Weye_DecryptTraceCodeAndPassword(
	const char* encryptedString, char* traceCode, char* password)
{
		ALOGV("EtronDI_Weye_DecryptTraceCodeAndPassword");

	return EtronDI_DecryptString(encryptedString, traceCode, password);
}	
