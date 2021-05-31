#pragma once

#ifdef __linux__
#define ETRONDI_WEYE_API
#else
#ifdef __WEYE__
#ifdef ETRONDI_EXPORTS
#define ETRONDI_WEYE_API __declspec(dllexport)
#else
#define ETRONDI_WEYE_API __declspec(dllimport)
#endif
#ifdef  __cplusplus
extern "C" {
#endif
#else
#define ETRONDI_WEYE_API
#endif
#endif


#ifdef __linux__
#include "eSPDI_def.h"
#else
#include "eSPDI_ErrCode.h"
#endif

int ETRONDI_WEYE_API EtronDI_Weye_Activate(void** ppHandleEtronDI, bool bIsLogEnabled);
void ETRONDI_WEYE_API EtronDI_Weye_Deactivate(void** ppHandleEtronDI);
int ETRONDI_WEYE_API EtronDI_Weye_GenerateLutFile(void* pHandleEtronDI, const char* filename);
int ETRONDI_WEYE_API EtronDI_Weye_SaveLutData(void* pHandleEtronDI, const char* filename);
int ETRONDI_WEYE_API EtronDI_Weye_EncryptMP4(void* pHandleEtronDI, const char *filename);
int ETRONDI_WEYE_API EtronDI_Weye_EncryptMP4Ex(void* pHandleEtronDI, 
		const char* filename, const char* machineId, const char* serialNumber);
int ETRONDI_WEYE_API EtronDI_Weye_GetSerialNumber(void* pHandleEtronDI, char* serialNumber, int* snLength);
int ETRONDI_WEYE_API EtronDI_Weye_SetAutoExposureMode(void* pHandleEtronDI, unsigned short mode);
#ifdef __linux__
int ETRONDI_WEYE_API EtronDI_Weye_SetCTPropVal(void* pHandleEtronDI, int id, int value);
int ETRONDI_WEYE_API EtronDI_Weye_SetPUPropVal(void* pHandleEtronDI, int id, int value);
#endif

int ETRONDI_WEYE_API EtronDI_Weye_EncryptString(const char* src, char* dst);// length of dst > 2 * length of src
int ETRONDI_WEYE_API EtronDI_Weye_DecryptString(const char* src, char* dst);
int ETRONDI_WEYE_API EtronDI_Weye_EncryptTraceCodeAndPassword(
	const char* traceCode, const char* password, char* result);// buffer length of result > 2 * (traceCode + password) + 10
int ETRONDI_WEYE_API EtronDI_Weye_DecryptTraceCodeAndPassword(
	const char* encryptedString, char* traceCode, char* password);

#ifndef __linux__
#ifdef __WEYE__
#ifdef __cplusplus
}
#endif
#endif
#endif
