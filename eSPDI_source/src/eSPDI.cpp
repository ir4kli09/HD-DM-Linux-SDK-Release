/* eSPDI.cpp
    \brief Main EtronDI SDK API functions
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eSPDI.h"
#include "EtronDI.h"
#include "videodevice.h"
#include "libeysov/fisheye360_def.h"
#include "debug.h"
#include "InterruptHandle.h"
#include "DepthMerge.h"
 #include <errno.h>
#ifndef UAC_NOT_SUPPORTED
#include "uac.h"
#endif

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/utsname.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include "shmringbuffer.h"
#include <cmath>
#include "CComputerFactory.h"
#include "CComputer.h"
#include <memory>

#include <csignal>
void signalHandler(int signum)
{
    printf("received SIGINT\n"); 
    exit(signum);
}

using namespace std;

static int imu_testing = false;
int baseline_index = 1;
static int baseline_m0_video_3 = false;
bool bEX8029 = false;
int kernel_index = 1; //1 <= 4.15, else 2
#define M0_VIDEO_3  2

CVideoDevice *g_pVideoDev[MAX_DEV_COUNT * 2]; /* max default video devices support */

bool g_bIsLogEnabled = false;

int EtronDI_Init(
    void **ppHandleEtronDI,
    bool bIsLogEnabled)
{
    signal(SIGINT, signalHandler);

    g_bIsLogEnabled = bIsLogEnabled;
    if (g_bIsLogEnabled == true)
        LOGI("%s\n", __func__ );

    int i = 0;
    struct utsname unameData;
    int first, midle, tail;

    for(i = 0; i < MAX_DEV_COUNT * 2; i++) g_pVideoDev[i] = NULL;

    LOGI("eSPDI: EtronDI_Init");

    CEtronDI* pEtronDI = new CEtronDI;
    int nRet = pEtronDI->Init(g_bIsLogEnabled);

    if( nRet == ETronDI_OK) {
        if (uname(&unameData) != 0) {
             perror("uname");
             exit(EXIT_FAILURE);
        }


        LOGI("unameData.release = %s\n",unameData.release);
        sscanf(unameData.release, "%d.%d.%d", &first, &midle, &tail);
        LOGI("version %d.%d.%d\n", first, midle, tail);
        if (first > 4) {
            kernel_index = 2;
        } else if (first == 4) {
            if (midle <= 15) {
                kernel_index = 1;
            } else {
                 kernel_index = 2;
            }
        } else {
            kernel_index = 1;
        }

        nRet = pEtronDI->FindDevice();

        if(nRet == ETronDI_OK) {
             LOGI("TotalDevNum: %d \n",pEtronDI->m_Adi.nTotalDevNum);
            int i = 0;
            for(i = 0; i < pEtronDI->m_Adi.nTotalDevNum; i++) {
                g_pVideoDev[2 * i] = new CVideoDevice(pEtronDI->m_Adi.device_list[i], g_bIsLogEnabled);
                if(pEtronDI->m_Adi.device_list2[i] != NULL) {
                    g_pVideoDev[2 * i + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[i], g_bIsLogEnabled);
                }
            }
        }
    }

    if (nRet == ETronDI_OK)
    {
        *ppHandleEtronDI = pEtronDI;
    }
    else
    {
        delete pEtronDI;
    }

    return nRet;
}

void EtronDI_Release(
    void **ppHandleEtronDI)
{

    CEtronDI *pEtronDI;
    //
    if (!ppHandleEtronDI) return;
    if (!*ppHandleEtronDI) return;
    //

    int i = 0;
    for(i=0;i<MAX_DEV_COUNT * 2;i++) {
        if(g_pVideoDev[i] != NULL) {
            delete g_pVideoDev[i];
            g_pVideoDev[i] = NULL;
        }
    }

    pEtronDI = (CEtronDI *)*ppHandleEtronDI;
    delete pEtronDI;
    *ppHandleEtronDI = NULL;
}

static void input_dev(int in, char *interface)
{
    sprintf(interface, "/sys/class/video4linux/video%d/device/interface", in);
}

#define TEST_M1_PATTERN "eTronMultiBaseline_M1"
#define TEST_M0_PATTERN "eTronMultiBaseline_M0"

static void check_baseline_m0(void)
{
    int i = 0;
    FILE *fp;
    char buffer[128];

begin:
    input_dev(i, buffer);
    i++;
    fp = fopen(buffer, "r");
    if (!fp) {
            LOGE("No File: %s\n", buffer);
            goto begin;

    }

    memset(buffer, 0, sizeof(buffer));
    /* Read and display data */
    fread(buffer, 64, 1, fp);
    fclose(fp);

    if (strncmp(buffer, TEST_M1_PATTERN, strlen(TEST_M1_PATTERN)) == 0) {
        baseline_m0_video_3 = true;
    } else {
        baseline_m0_video_3 = false;
    }
}

//(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit = 1)

int EtronDI_DoFusion(unsigned char **pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double *pBaseline, double *pWRNear, double *pWRFar, double *pWRFusion, int nMergeNum, bool bdepth2Byte11bit, int method)
{
    int (*fusionFunc)(unsigned char **, double *, unsigned char *, int, int, double , double *, double *, double *, double *, int, bool);

    switch (method) {
    case 0:
        fusionFunc = depthMergeMBLbase;
        break;
    case 1:
        fusionFunc = depthMergeMBRbaseV0;
        break;
    case 2:
        fusionFunc = depthMergeMBRbaseV1;
        break;
    }


    return fusionFunc(pDepthBufList, pDepthMerge, pDepthMergeFlag, nDWidth, nDHeight, fFocus, pBaseline, pWRNear, pWRFar, pWRFusion, nMergeNum, bdepth2Byte11bit);
}

bool EtronDI_IsMLBaseLine(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    int num;
    FILE *fp;
    char buffer[128];
    CEtronDI *pEtronDI;
    DEVINFORMATION devInfo;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //int nRet;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    EtronDI_GetDeviceInfo(pHandleEtronDI, pDevSelInfo, &devInfo);

    sscanf(devInfo.strDevName, "/dev/video%d", &num);

    input_dev(num, buffer);
    fp = fopen(buffer, "r");
    if (!fp) {
            LOGE("No File: %s\n", buffer);
            return false;
    }

    memset(buffer, 0, sizeof(buffer));
    /* Read and display data */
    fread(buffer, 64, 1, fp);
    fclose(fp);

    if (strncmp(buffer, TEST_M0_PATTERN, strlen(TEST_M0_PATTERN)) == 0) {
        return true;
    } else if (strncmp(buffer, TEST_M1_PATTERN, strlen(TEST_M1_PATTERN)) == 0) {
        return true;
    }


    return  false;
}

int EtronDI_SwitchBaseline(int index)
{
    if (index > 0 && index < 4) {
        if (index == 2)
            index = 1;
        baseline_index = index;
        return ETronDI_OK;
    }
    return ETronDI_NotSupport;
}

int EtronDI_FindDevice(
    void *pHandleEtronDI)
{
    CEtronDI *pEtronDI;
    int nRet = ETronDI_OK;

    if (g_bIsLogEnabled == true)
        LOGI("%s\n", __func__ );

    //
    pEtronDI = (CEtronDI*)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_NullPtr;
    //

    nRet = pEtronDI->FindDevice();

    if(nRet == ETronDI_OK) {

        int i = 0;
        for(i=0;i<pEtronDI->m_Adi.nTotalDevNum;i++) {
            LOGD("new CVideoDevice(%s)\n", pEtronDI->m_Adi.device_list[i]);
            g_pVideoDev[2*i] = new CVideoDevice(pEtronDI->m_Adi.device_list[i], g_bIsLogEnabled);
            if(pEtronDI->m_Adi.device_list2[i] != NULL)
                g_pVideoDev[2*i+1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[i], g_bIsLogEnabled);
        }
    }

    return nRet;
}

int EtronDI_RefreshDevice(
    void *pHandleEtronDI)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_NullPtr;
    //

    return pEtronDI->FindDevice();
}

int EtronDI_GetDeviceNumber(
    void *pHandleEtronDI)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    return pEtronDI->GetDeviceNumber();
}

int EtronDI_GetDeviceInfo(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    DEVINFORMATION* pdevinfo)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //int nRet;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //
    int nRet = pEtronDI->GetDeviceInfo(pDevSelInfo, pdevinfo);
    if(ETronDI_OK == nRet){
        if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
            g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
        nRet = g_pVideoDev[2*pDevSelInfo->index]->GetPidVid(&pdevinfo->wPID, &pdevinfo->wVID);
    }

    return nRet;
}

int EtronDI_GetDeviceInfoMBL_15cm(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    DEVINFORMATION* pdevinfo)
{
    CEtronDI *pEtronDI;
    DEVSELINFO TmpDevSelInfo;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //int nRet;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

#if defined(FIND_DEVICE2)
    TmpDevSelInfo.index = pDevSelInfo->index + 1;
#else
     if (baseline_m0_video_3 == true) {
        TmpDevSelInfo.index = pDevSelInfo->index - 1;
     } else {
        TmpDevSelInfo.index = pDevSelInfo->index + 1;
     }
#endif
     return pEtronDI->GetDeviceInfo(&TmpDevSelInfo, pdevinfo);

}

int EtronDI_SelectDevice(
    void *pHandleEtronDI,
    int dev_index)
{
    return ETronDI_NotSupport;
}

// register APIs +
int EtronDI_GetFWRegister(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short address,
    unsigned short *pValue,
    int flag)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetFWRegister( address, pValue, flag);

    return nRet;
}

int EtronDI_SetFWRegister(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short address,
    unsigned short nValue,
    int flag)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetFWRegister( address, nValue, flag);

    return nRet;
}

int EtronDI_GetHWRegister(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short address,
    unsigned short *pValue,
    int flag)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetHWRegister( address, pValue, flag);

    return nRet;
}

int EtronDI_SetHWRegister(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short address,
    unsigned short nValue,
    int flag)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetHWRegister( address, nValue, flag);

    return nRet;
}

int EtronDI_GetSensorRegister(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    unsigned short address,
    unsigned short *pValue,
    int flag,
    SENSORMODE_INFO SensorMode)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    int nSensorMode = 0;
    if(SensorMode == SENSOR_A) nSensorMode = 0;
    else if(SensorMode == SENSOR_B) nSensorMode = 1;
    else if(SensorMode == SENSOR_C) nSensorMode = 3;
    else if(SensorMode == SENSOR_D) nSensorMode = 4;
    else nSensorMode = 2;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetSensorRegister( nId, address, pValue, flag, nSensorMode);

    return nRet;
}

int EtronDI_SetSensorRegister(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    unsigned short address,
    unsigned short nValue,
    int flag,
    SENSORMODE_INFO SensorMode)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    int nSensorMode = 0;
    if(SensorMode == SENSOR_A) nSensorMode = 0;
    else if(SensorMode == SENSOR_B) nSensorMode = 1;
    else if(SensorMode == SENSOR_C) nSensorMode = 3;
    else if(SensorMode == SENSOR_D) nSensorMode = 4;
    else nSensorMode = 2;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetSensorRegister( nId, address, nValue, flag, nSensorMode);

    return nRet;
}
// register APIs -


// File ID +
int  EtronDI_GetBusInfo(
        void *pHandleEtronDI,
        PDEVSELINFO pDevSelInfo,
        char *pszBusInfo,
        int *pActualLength)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(!pszBusInfo) return ETronDI_NullPtr;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetUSBBusInfo(pszBusInfo);

    if(pActualLength) *pActualLength = strlen(pszBusInfo);

    return nRet;
}

int EtronDI_GetFwVersion(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    char *pszFwVersion,
    int nBufferSize,
    int *pActualLength)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetFwVersion( pszFwVersion, nBufferSize, pActualLength);

    return nRet;
}

int EtronDI_GetPidVid(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pPidBuf,
    unsigned short *pVidBuf)
{
    CEtronDI *pEtronDI;
    int nRet;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetPidVid( pPidBuf, pVidBuf);

    return nRet;
}

int EtronDI_SetPidVid(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pPidBuf,
    unsigned short *pVidBuf)
{
    CEtronDI *pEtronDI;
    int nRet;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetPidVid( pPidBuf, pVidBuf);

    return nRet;
}

int EtronDI_GetSerialNumber(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned char* pData,
    int nbufferSize,
    int *pLen)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetSerialNumber( pData, nbufferSize, pLen);

    return nRet;
}

int EtronDI_SetSerialNumber(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned char* pData,
    int nLen)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetSerialNumber( pData, nLen);

    return nRet;
}

int EtronDI_GetYOffset(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    int index)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetYOffset( buffer, BufferLength, pActualLength, index);

    return nRet;
}

int EtronDI_GetRectifyTable(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    int index)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetRectifyTable( buffer, BufferLength, pActualLength, index);

    return nRet;
}

int EtronDI_GetZDTable(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    PZDTABLEINFO pZDTableInfo)
{
    CEtronDI *pEtronDI;
        DEVINFORMATION devInfo;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(!buffer) return ETronDI_NullPtr;
    if(pZDTableInfo->nIndex > 9) return ETronDI_NotSupport;

    switch(BufferLength){
        case ETronDI_ZD_TABLE_FILE_SIZE_8_BITS:
        case ETronDI_ZD_TABLE_FILE_SIZE_11_BITS:
        break;

        default: return ETronDI_ErrBufLen;
    }

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetZDTable( buffer, BufferLength, pActualLength, pZDTableInfo);

    return nRet;
}

int EtronDI_GetLogData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    int index,
    CALIBRATION_LOG_TYPE type)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetLogData( buffer, BufferLength, pActualLength, index, type);

    return nRet;
}

int EtronDI_GetUserData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    USERDATA_SECTION_INDEX usi)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetUserData( buffer, BufferLength, usi);

    return nRet;
}

int EtronDI_SetYOffset(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    int index)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetYOffset( buffer, BufferLength, pActualLength, index);

    return nRet;
}

int EtronDI_SetRectifyTable(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    int index)
{
    CEtronDI *pEtronDI;
    int nRet;
    //

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetRectifyTable( buffer, BufferLength, pActualLength, index);

    return nRet;
}

int EtronDI_SetZDTable(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    PZDTABLEINFO pZDTableInfo)
{
    CEtronDI *pEtronDI;
    DEVINFORMATION devInfo;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(!buffer) return ETronDI_NullPtr;
    if(pZDTableInfo->nIndex > 9) return ETronDI_NotSupport;

    switch(BufferLength){
          case ETronDI_ZD_TABLE_FILE_SIZE_8_BITS:
               if( pZDTableInfo->nDataType != ETronDI_DEPTH_DATA_8_BITS && pZDTableInfo->nDataType != ETronDI_DEPTH_DATA_8_BITS_x80 )
                   return ETronDI_ErrBufLen;
                break;
          case ETronDI_ZD_TABLE_FILE_SIZE_11_BITS:
               if( pEtronDI->GetDeviceInfo(pDevSelInfo, &devInfo)==ETronDI_OK && devInfo.nDevType == AXES1 )
                   return ETronDI_DEVICE_NOT_SUPPORT;
               else if( pZDTableInfo->nDataType == ETronDI_DEPTH_DATA_8_BITS || pZDTableInfo->nDataType == ETronDI_DEPTH_DATA_8_BITS_x80 )
                   return ETronDI_ErrBufLen;
               break;
          default: return ETronDI_ErrBufLen;
    }

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetZDTable( buffer, BufferLength, pActualLength, pZDTableInfo);

    return nRet;
}

int EtronDI_SetLogData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    int *pActualLength,
    int index)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetLogData( buffer, BufferLength, pActualLength, index, ALL_LOG);

    return nRet;
}

int EtronDI_SetUserData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *buffer,
    int BufferLength,
    USERDATA_SECTION_INDEX usi)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetUserData( buffer, BufferLength, usi);

    return nRet;
}

int EtronDI_ReadFlashData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    FLASH_DATA_TYPE fdt,
    BYTE *pBuffer,
    unsigned long int BufferLength,
    unsigned long int *pActualLength)
{
    CEtronDI *pEtronDI;
    bool bBootloader = false;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //
    // check if bootloader existed in the firmware
        if(pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index] == AXES1)
        bBootloader = true;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->ReadFlashData( pBuffer, BufferLength, pActualLength, fdt, bBootloader);

      LOGI("............ EtronDI_ReadFlashData bBootloader . %d\n", bBootloader);

    return nRet;
}

int EtronDI_WriteFlashData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    FLASH_DATA_TYPE fdt,
    BYTE *pBuffer,
    unsigned long int BufferLength,
    bool bIsDataVerify,
    KEEP_DATA_CTRL kdc)
{
    CEtronDI *pEtronDI;
    bool bBootloader = false;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    // check if bootloader existed in the firmware
    if(pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index] == AXES1)
        bBootloader = true;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->WriteFlashData_FullFunc( pBuffer, BufferLength, fdt, kdc, bIsDataVerify, bBootloader);

    LOGI("............ EtronDI_WriteFlashData bBootloader . %d\n", bBootloader);

    return nRet;
}

int EtronDI_GetDevicePortType(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    USB_PORT_TYPE* pUSB_Port_Type)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    return g_pVideoDev[2*pDevSelInfo->index]->GetDevicePortType(*pUSB_Port_Type);

}

// File ID -

// image +
int EtronDI_GetDeviceResolutionList(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nMaxCount0,
    ETRONDI_STREAM_INFO *pStreamInfo0,
    int nMaxCount1,
    ETRONDI_STREAM_INFO *pStreamInfo1)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->GetDeviceResolutionList(nMaxCount0, pStreamInfo0);

    if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index] != NULL && pStreamInfo1 != NULL) {
        if(g_pVideoDev[2*pDevSelInfo->index + baseline_index] == NULL)
            g_pVideoDev[2*pDevSelInfo->index + baseline_index] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index], g_bIsLogEnabled);
        nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->GetDeviceResolutionList(nMaxCount1, pStreamInfo1);
    }

    return nRet;
}

int EtronDI_SetDepthDataType(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short nValue)
{
  CEtronDI *pEtronDI;
  DEVINFORMATION devInfo;
  int nRlt;

  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;

  pEtronDI = (CEtronDI *)pHandleEtronDI;
  if (!pEtronDI) return ETronDI_Init_Fail;

  if (pDevSelInfo == NULL) return ETronDI_NullPtr;

  if (g_pVideoDev[2*pDevSelInfo->index] == NULL)
    g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

  // Check device type
  if (pEtronDI->GetDeviceInfo(pDevSelInfo, &devInfo)==ETronDI_OK && devInfo.nDevType == AXES1){
      if (ETronDI_DEPTH_DATA_8_BITS_RAW == nValue) nValue = 1;
      else if (ETronDI_DEPTH_DATA_OFF_RECTIFY == nValue) nValue = 1;
  }
  if (g_pVideoDev[2*pDevSelInfo->index] != NULL) {
    nRlt = g_pVideoDev[2*pDevSelInfo->index]->SetDepthDataType(nValue);
  }

  return nRlt;
}

int EtronDI_GetDepthDataType(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pValue)
{
  CEtronDI *pEtronDI;
  DEVINFORMATION devInfo;

  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  pEtronDI = (CEtronDI *)pHandleEtronDI;
  if (!pEtronDI) return ETronDI_Init_Fail;

  if (pDevSelInfo == NULL) return ETronDI_NullPtr;

  if (g_pVideoDev[2*pDevSelInfo->index] == NULL)
    g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

  // Check device type
  if (pEtronDI->GetDeviceInfo(pDevSelInfo, &devInfo)==ETronDI_OK && devInfo.nDevType == PUMA){
      *pValue = ETronDI_DEPTH_DATA_8_BITS;
  }

  if (!pValue) return ETronDI_RET_BAD_PARAM;

  return g_pVideoDev[2*pDevSelInfo->index]->GetDepthDataType(pValue);
}

// IR support
#ifndef DOXYGEN_SHOULD_SKIP_THIS
int _GetDevice(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
  CEtronDI *pEtronDI = (CEtronDI *)pHandleEtronDI;

  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  if (!pEtronDI)
    return ETronDI_Init_Fail;

  if (pDevSelInfo == NULL)
    return ETronDI_NullPtr;

  if (g_pVideoDev[2*pDevSelInfo->index] == NULL)
    g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

  return ETronDI_OK;
}
#endif

int EtronDI_SetCurrentIRValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short nValue)
{

  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;

  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->SetCurrentIRValue(nValue);
}

int EtronDI_GetCurrentIRValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pValue)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->GetCurrentIRValue(pValue);
}

int EtronDI_GetIRMinValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pValue)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->GetIRMinValue(pValue);
}

int EtronDI_SetIRMaxValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short nValue)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->SetIRMaxValue(nValue);
}

int EtronDI_GetIRMaxValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pValue)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->GetIRMaxValue(pValue);
}

int EtronDI_SetIRMode(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short nValue)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->SetIRMode(nValue);
}

int EtronDI_GetIRMode(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short *pValue)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->GetIRMode(pValue);
}
// ~IR support

int EtronDI_Setup_v4l2_requestbuffers(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int cnt)
{
  if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
  int nRet = _GetDevice(pHandleEtronDI, pDevSelInfo);

  if (nRet != ETronDI_OK)
    return nRet;

  return g_pVideoDev[2*pDevSelInfo->index]->Set_v4l2_requestbuffers(cnt);
}

int EtronDI_OpenDevice(
        void *pHandleEtronDI,
        PDEVSELINFO pDevSelInfo,
        int nEP0Width,
        int nEP0Height,
        bool bEP0MJPG,
        int nEP1Width,
        int nEP1Height,
        DEPTH_TRANSFER_CTRL dtc,
        bool bIsOutputRGB24,
        void *phWndNotice,
        int *pFPS,
        CONTROL_MODE cm)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    int nRet = ETronDI_Init_Fail;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //
    LOGD("%s: pDevSelInfo->index = %d\n", __func__, pDevSelInfo->index);
    LOGD("%s: nEP0Width = %d, nEP0Height = %d, bEP0MJPG = %d, pFPS = %d\n", __func__, nEP0Width, nEP0Height, bEP0MJPG, pFPS);

    if( nEP0Width == 0 && nEP0Height == 0) {
        delete g_pVideoDev[2*pDevSelInfo->index];
        g_pVideoDev[2*pDevSelInfo->index] = NULL;
    } else {
        nRet = g_pVideoDev[2*pDevSelInfo->index]->OpenDevice( nEP0Width, nEP0Height, bEP0MJPG, pFPS);
    }

    return nRet;
}

int EtronDI_OpenDevice2(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nEP0Width,
    int nEP0Height,
    bool bEP0MJPG,
    int nEP1Width,
    int nEP1Height,
    DEPTH_TRANSFER_CTRL dtc,
    bool bIsOutputRGB24,
    void *phWndNotice,
    int *pFPS,
    CONTROL_MODE cm)
{
    CEtronDI *pEtronDI;
//    check_baseline_m0();
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    int nRet = ETronDI_Init_Fail;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //
    if (baseline_m0_video_3 == true) {
        LOGD("baseline_m0_video_3 == true\n");
        if (pDevSelInfo->index == 0 || (pDevSelInfo->index == 1 && baseline_index < 3)) {
            if( nEP0Width != 0 && nEP0Height != 0) {
                nRet = g_pVideoDev[2*pDevSelInfo->index]->OpenDevice( nEP0Width, nEP0Height, bEP0MJPG, pFPS);
            }

            if(nEP1Width != 0 && nEP1Height != 0) {
               nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->OpenDevice( nEP1Width, nEP1Height, false, pFPS, dtc);
            }
        } else {
            if( nEP0Width != 0 && nEP0Height != 0) {
                nRet = g_pVideoDev[2*pDevSelInfo->index]->OpenDevice( nEP0Width, nEP0Height, bEP0MJPG, pFPS);
            }

            if(nEP1Width != 0 && nEP1Height != 0) {
                nRet = g_pVideoDev[1]->OpenDevice( nEP1Width, nEP1Height, false, pFPS, dtc);
            }

        }
    } else {
            if( nEP0Width != 0 && nEP0Height != 0) {
                nRet = g_pVideoDev[2*pDevSelInfo->index]->OpenDevice( nEP0Width, nEP0Height, bEP0MJPG, pFPS);
            }

            if(nEP1Width != 0 && nEP1Height != 0) {
                nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->OpenDevice( nEP1Width, nEP1Height, false, pFPS, dtc);
            }
    }

    return nRet;
}

int EtronDI_OpenDeviceMBL(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nEP0Width,
    int nEP0Height,
    bool bEP0MJPG,
    int nEP1Width,
    int nEP1Height,
    DEPTH_TRANSFER_CTRL dtc,
    bool bIsOutputRGB24,
    void *phWndNotice,
    int *pFPS,
    CONTROL_MODE cm)
{
    CEtronDI *pEtronDI;
    int nRet = ETronDI_Init_Fail;

    check_baseline_m0();
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;

    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

 /*
Case 1:
/dev/video0 M0 ==> First
/dev/video1 X
/dev/video2
/dev/video3 M1

pEtronDI->m_Adi.device_list[pDevSelInfo->index = 0] = /dev/video0
pEtronDI->m_Adi.device_list[pDevSelInfo->index + 1 = 1] = /dev/video2
pEtronDI->m_Adi.device_list2[pDevSelInfo->index  = 0] = /dev/video1 X
pEtronDI->m_Adi.device_list2[ppDevSelInfo->index + 1 = 1] = /dev/video3

Case 2:
/dev/video0 M1
/dev/video1 
/dev/video2 M0  ==> First
/dev/video3 x
pEtronDI->m_Adi.device_list[pDevSelInfo->index - 1= 0 ] = /dev/video0 
pEtronDI->m_Adi.device_list[pDevSelInfo->index = 1] = /dev/video2
pEtronDI->m_Adi.device_list2[pDevSelInfo->index -1 = 0] = /dev/video1
pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1 = 2] = /dev/video3 X
m_Adi.device_list[0] = /dev/video0  M1
m_Adi.device_list[1] = /dev/video2  M0
m_Adi.device_list2[0] = /dev/video1 X
m_Adi.device_list2[1] = /dev/video3
g_pVideoDev[2 * i] = new CVideoDevice(pEtronDI->m_Adi.device_list[i], g_bIsLogEnabled);
if(pEtronDI->m_Adi.device_list2[i] != NULL) {
    g_pVideoDev[2 * i + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[i], g_bIsLogEnabled);
}

*/

#if defined(FIND_DEVICE2)
    if (g_bIsLogEnabled == true)
        LOGI("2 * pDevSelInfo->index = %d\n", 2 * pDevSelInfo->index);
    if( nEP0Width == 0 && nEP0Height == 0) {
        delete g_pVideoDev[2 * pDevSelInfo->index];
        g_pVideoDev[2 * pDevSelInfo->index] = NULL;
    } else {
        nRet = g_pVideoDev[2 * pDevSelInfo->index]->OpenDevice(nEP0Width, nEP0Height, bEP0MJPG, pFPS);
    }
    if (g_bIsLogEnabled == true)
        LOGI("2 * pDevSelInfo->index + 2 = %d\n", 2 * pDevSelInfo->index + 1);
    if(nEP1Width == 0 && nEP1Height == 0) {
        delete g_pVideoDev[2 * pDevSelInfo->index + 1];
        g_pVideoDev[2 * pDevSelInfo->index + 1] = NULL;
    } else {
        nRet = g_pVideoDev[2 * pDevSelInfo->index + 1]->OpenDevice(nEP1Width, nEP1Height, false, pFPS, dtc);
    }

    if (g_bIsLogEnabled == true)
        LOGI("pDevSelInfo->index = %d\n", pDevSelInfo->index);
    if(nEP1Width == 0 && nEP1Height == 0) {
        delete g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1];
        g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1] = NULL;
    } else {
        nRet = g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1]->OpenDevice(nEP1Width, nEP1Height, false, pFPS, dtc);
    }
#else
    if (baseline_m0_video_3 == true) {
        if (g_bIsLogEnabled == true)
            LOGI("2 * pDevSelInfo->index = %d\n", 2 * pDevSelInfo->index);
        if( nEP0Width != 0 && nEP0Height != 0) {
            nRet = g_pVideoDev[2 * pDevSelInfo->index]->OpenDevice(nEP0Width, nEP0Height, bEP0MJPG, pFPS);
        }
        if (g_bIsLogEnabled == true)
            LOGI("2 * pDevSelInfo->index + 2 = %d\n", 2 * pDevSelInfo->index + 1);
        if(nEP1Width != 0 && nEP1Height != 0) {
            nRet = g_pVideoDev[2 * pDevSelInfo->index + 1]->OpenDevice(nEP1Width, nEP1Height, false, pFPS, dtc);
        }
        if (g_bIsLogEnabled == true)
            LOGI("pDevSelInfo->index = %d\n", pDevSelInfo->index);
        if(nEP1Width != 0 && nEP1Height != 0) {
            nRet = g_pVideoDev[pDevSelInfo->index]->OpenDevice(nEP1Width, nEP1Height, false, pFPS, dtc);
        }
    } else {
            if( nEP0Width != 0 && nEP0Height != 0) {
                nRet = g_pVideoDev[pDevSelInfo->index]->OpenDevice(nEP0Width, nEP0Height, bEP0MJPG, pFPS);
            }
            if(nEP1Width != 0 && nEP1Height != 0) {
                nRet = g_pVideoDev[pDevSelInfo->index + 1]->OpenDevice(nEP1Width, nEP1Height, false, pFPS, dtc);
            }

            if (nEP1Width != 0 && nEP1Height != 0) {
                nRet = g_pVideoDev[pDevSelInfo->index + 3]->OpenDevice(nEP1Width, nEP1Height, false, pFPS, dtc);
            }
    }
#endif

    return nRet;
}

/*

I/eSPDI_API: eSPDI: EtronDI_InitnRet = 0
nDevCount = 2
Device Name = /dev/video0
PID = 0x0117 ==> M0
VID = 0x1e4e
Chip ID = 0x15
FW Version = EX8038-B01-B0144M-BL60U-011-Beta1

Device Name = /dev/video2
PID = 0x0110
VID = 0x1e4e
Chip ID = 0x15
FW Version = EX8038-B01-B0144M-BL60U-011-Beta1

EtronDI_CloseDeviceMBL
/dev/video0 M0 ==> First
/dev/video1 X
/dev/video2
/dev/video3 M1

pEtronDI->m_Adi.device_list[pDevSelInfo->index = 0] = /dev/video0
pEtronDI->m_Adi.device_list[pDevSelInfo->index + 1 = 1] = /dev/video2
pEtronDI->m_Adi.device_list2[pDevSelInfo->index  = 0] = /dev/video1 X
pEtronDI->m_Adi.device_list2[ppDevSelInfo->index + 1 = 2] = /dev/video3

m_Adi.device_list[0] = /dev/video0 M0
m_Adi.device_list[1] = /dev/video2 M1
m_Adi.device_list2[0] = /dev/video1
m_Adi.device_list2[1] = /dev/video3 X


I/eSPDI_API: eSPDI: EtronDI_InitnRet = 0
nDevCount = 2
Device Name = /dev/video0
PID = 0x0110
VID = 0x1e4e
Chip ID = 0x15
FW Version = EX8038-B01-B0144M-BL60U-011-Beta1

Device Name = /dev/video2
PID = 0x0117  ==> M0
VID = 0x1e4e
Chip ID = 0x15
FW Version = EX8038-B01-B0144M-BL60U-011-Beta1

EtronDI_CloseDeviceMBL
/dev/video0 M1
/dev/video1
/dev/video2 M0  ==> First
/dev/video3 x
pEtronDI->m_Adi.device_list[pDevSelInfo->index - 1= 0 ] = /dev/video0
pEtronDI->m_Adi.device_list[pDevSelInfo->index = 1] = /dev/video2
pEtronDI->m_Adi.device_list2[pDevSelInfo->index -1 = 0] = /dev/video1
pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1 = 2] = /dev/video3 X
m_Adi.device_list[0] = /dev/video0  M1
m_Adi.device_list[1] = /dev/video2  M0
m_Adi.device_list2[0] = /dev/video1 X
m_Adi.device_list2[1] = /dev/video3
g_pVideoDev[2 * i] = new CVideoDevice(pEtronDI->m_Adi.device_list[i], g_bIsLogEnabled);
if(pEtronDI->m_Adi.device_list2[i] != NULL) {
    g_pVideoDev[2 * i + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[i], g_bIsLogEnabled);
}

*/

int EtronDI_CloseDeviceMBL(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
    {
    CEtronDI *pEtronDI;
    int nRet = ETronDI_OK;
    if (g_bIsLogEnabled == true)
        LOGI("%s: pDevSelInfo->index = %d\n", __func__, pDevSelInfo->index);


    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

#if defined(FIND_DEVICE2)
    if(g_pVideoDev[2 * pDevSelInfo->index] != NULL) {
        delete g_pVideoDev[2 * pDevSelInfo->index];
    }
    g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index] != NULL) {
        if(g_pVideoDev[2 * pDevSelInfo->index + 1] != NULL) {
            delete g_pVideoDev[2 * pDevSelInfo->index + 1];
        }
        g_pVideoDev[2 * pDevSelInfo->index + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index], g_bIsLogEnabled);
    }

    if(g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1] != NULL) {
        delete g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1];
    }
    g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1], g_bIsLogEnabled);
    // for Tx2 /dev/video2
#else
    if (baseline_m0_video_3 == true) {
        if(g_pVideoDev[2 * pDevSelInfo->index] != NULL) {
            delete g_pVideoDev[2 * pDevSelInfo->index];
        }
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

        if(pEtronDI->m_Adi.device_list[pDevSelInfo->index] != NULL) {

            if(g_pVideoDev[2 * pDevSelInfo->index + 1] != NULL) {
                delete g_pVideoDev[2 * pDevSelInfo->index + 1];
            }
            g_pVideoDev[2 * pDevSelInfo->index + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index], g_bIsLogEnabled);
        }

        if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index - 1] != NULL) {
            if(g_pVideoDev[pDevSelInfo->index] != NULL) {
                delete g_pVideoDev[pDevSelInfo->index];
            }
            g_pVideoDev[pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index - 1], g_bIsLogEnabled);
            // for Tx2 /dev/video2
        }

    } else {
        if(g_pVideoDev[pDevSelInfo->index] != NULL) {
            delete g_pVideoDev[pDevSelInfo->index];
        }
        g_pVideoDev[ pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
        LOGD("%s:pDevSelInfo->index = %d\n", __func__, pDevSelInfo->index);

        if(pEtronDI->m_Adi.device_list[pDevSelInfo->index + 1] != NULL) { // for Tx2 /dev/video4
            if(g_pVideoDev[pDevSelInfo->index + 1] != NULL) {
                delete g_pVideoDev[pDevSelInfo->index  + 1];
            }
            g_pVideoDev[pDevSelInfo->index + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index], g_bIsLogEnabled);
            // for Tx2 /dev/video4
        }

        if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1] != NULL) { // for Tx2 /dev/video4
            if(g_pVideoDev[pDevSelInfo->index + 3] != NULL) {
                delete g_pVideoDev[pDevSelInfo->index + 3];
            }
            g_pVideoDev[pDevSelInfo->index + 3] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1], g_bIsLogEnabled);
        }


    }
#endif
    return nRet;
}

int EtronDI_CloseDevice(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
    {
    CEtronDI *pEtronDI;
    int nRet = ETronDI_OK;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if (g_pVideoDev[2*pDevSelInfo->index] == NULL) return ETronDI_NullPtr;

    g_pVideoDev[2*pDevSelInfo->index]->CloseDevice();
    if (g_pVideoDev[2 * pDevSelInfo->index + baseline_index] != NULL){
        g_pVideoDev[2 * pDevSelInfo->index + baseline_index]->CloseDevice();
    }

    return nRet;
}

int EtronDI_CloseDeviceEx(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
    {
    CEtronDI *pEtronDI;
    int nRet = ETronDI_OK;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //
    if (pDevSelInfo->index == 1) {
        if(g_pVideoDev[2*pDevSelInfo->index] != NULL) {
            delete g_pVideoDev[2*pDevSelInfo->index];
        }
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

        if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index] != NULL) {

            if(g_pVideoDev[2*pDevSelInfo->index + 1] != NULL) {
                delete g_pVideoDev[2*pDevSelInfo->index + baseline_index];
            }
            g_pVideoDev[2*pDevSelInfo->index + 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index], g_bIsLogEnabled);
        }

        if(pEtronDI->m_Adi.device_list2[0] != NULL) { // for Tx2 /dev/video2
            if(g_pVideoDev[2 * pDevSelInfo->index - 1] != NULL) {
                delete g_pVideoDev[2*pDevSelInfo->index - 1];
            }
            g_pVideoDev[2*pDevSelInfo->index - 1] = new CVideoDevice(pEtronDI->m_Adi.device_list2[0], g_bIsLogEnabled);
            // for Tx2 /dev/video2
        }
    } else {
        if(g_pVideoDev[2*pDevSelInfo->index] != NULL) {
            delete g_pVideoDev[2*pDevSelInfo->index];
        }
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

        if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index] != NULL) { // for Tx2 /dev/video4
            if(g_pVideoDev[2 * pDevSelInfo->index + baseline_index] != NULL) {
                delete g_pVideoDev[2*pDevSelInfo->index + baseline_index];
            }
            g_pVideoDev[2*pDevSelInfo->index + baseline_index] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index], g_bIsLogEnabled);
        }

        if(pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1] != NULL) { // for Tx2 /dev/video4
            if(g_pVideoDev[2 * pDevSelInfo->index + 3] != NULL) {
                delete g_pVideoDev[2*pDevSelInfo->index + 3];
            }
            g_pVideoDev[2*pDevSelInfo->index + 3] = new CVideoDevice(pEtronDI->m_Adi.device_list2[pDevSelInfo->index + 1], g_bIsLogEnabled);
        }
    }

    return nRet;
}

int EtronDI_SetupBlock(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    bool enable)
{
    CEtronDI *pEtronDI;
    int nRet = -1;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if (enable == true) {
        if(g_pVideoDev[2 * pDevSelInfo->index] != NULL) {
            nRet = g_pVideoDev[2*pDevSelInfo->index]->SetupBlock(0, 1000 * 200);
        }

        if(nRet == 0 && g_pVideoDev[2*pDevSelInfo->index + baseline_index] != NULL) {
            nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->SetupBlock(0, 1000 * 200);
        }
    } else {
        if(g_pVideoDev[2 * pDevSelInfo->index] != NULL) {
            nRet = g_pVideoDev[2*pDevSelInfo->index]->SetupNonBlock();
        }

        if(nRet == 0 && g_pVideoDev[2*pDevSelInfo->index + baseline_index] != NULL) {
            nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->SetupNonBlock();
        }
    }

    return nRet;
}

int EtronDI_GetImage(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pBuf,
    unsigned long int *pImageSize,
    int *pSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //

    if(g_pVideoDev[2*pDevSelInfo->index] != NULL) {
        if( g_pVideoDev[2*pDevSelInfo->index]->m_fival.width > 0 && g_pVideoDev[2*pDevSelInfo->index]->m_fival.height > 0 ) {
            g_pVideoDev[2*pDevSelInfo->index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet = g_pVideoDev[2*pDevSelInfo->index]->GetImage(pBuf, pImageSize, pSerial, false, nDepthDataType);
        }
    }

    if(g_pVideoDev[2*pDevSelInfo->index + baseline_index] != NULL) {
        if( g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.width > 0 && g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.height > 0 ) {
            g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->GetImage(pBuf, pImageSize, pSerial, true, nDepthDataType);
        }
    }

    return nRet;
}

int EtronDI_GetColorImage(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pBuf,
    unsigned long int *pImageSize,
    int *pSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet = -1;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //

    if(g_pVideoDev[2*pDevSelInfo->index] != NULL) {
        if( g_pVideoDev[2*pDevSelInfo->index]->m_fival.width > 0 && g_pVideoDev[2*pDevSelInfo->index]->m_fival.height > 0 ) {
            g_pVideoDev[2*pDevSelInfo->index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet = g_pVideoDev[2*pDevSelInfo->index]->GetImage(pBuf, pImageSize, pSerial, false, DEPTH_IMG_NON_TRANSFER);
        }
    }

    return nRet;
}

int EtronDI_GetDepthImage(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pBuf,
    unsigned long int *pImageSize,
    int *pSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet = -1;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index + baseline_index] != NULL) {
        if (g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.width >  0 &&
                    g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.height > 0 ) {
            g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->GetImage(pBuf, pImageSize, pSerial, true, nDepthDataType);
        }
    }

    return nRet;
}

int EtronDI_Get_Color_30_mm_depth(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pDepthImgBuf,
    unsigned long int *pImageSize,
    int *pSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //

    if(g_pVideoDev[2*pDevSelInfo->index] != NULL) {
        if( g_pVideoDev[2*pDevSelInfo->index]->m_fival.width > 0 && g_pVideoDev[2*pDevSelInfo->index]->m_fival.height > 0 ) {
            g_pVideoDev[2*pDevSelInfo->index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet = g_pVideoDev[2*pDevSelInfo->index]->GetImage(pDepthImgBuf, pImageSize, pSerial, false, nDepthDataType);
        }
    }

    return nRet;
}

int EtronDI_Get_60_mm_depth(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pDepthImgBuf,
    unsigned long int *pImageSize,
    int *pSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //

    g_pVideoDev[2*pDevSelInfo->index + 1]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
    nRet = g_pVideoDev[2*pDevSelInfo->index + 1]->GetImage(pDepthImgBuf, pImageSize, pSerial, false, nDepthDataType);

    return nRet;
}

int EtronDI_Get_150_mm_depth(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pDepthImgBuf,
    unsigned long int *pDepthImageSize,
    int *pDepthSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    //

#if defined(FIND_DEVICE2)
    g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index + 1];
    nRet = g_pVideoDev[2 * (pDevSelInfo->index + 1) + 1]->GetImage(pDepthImgBuf, pDepthImageSize, pDepthSerial, true, nDepthDataType);
#else
    if (baseline_m0_video_3 == true) {
        g_pVideoDev[1]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
        nRet = g_pVideoDev[1]->GetImage(pDepthImgBuf, pDepthImageSize, pDepthSerial, true, nDepthDataType);
    } else {
        g_pVideoDev[2*pDevSelInfo->index + 3]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
        nRet = g_pVideoDev[2*pDevSelInfo->index + 3]->GetImage(pDepthImgBuf, pDepthImageSize, pDepthSerial, true, nDepthDataType);
    }
#endif

    return nRet;
}

int EtronDI_Get2Image(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    BYTE *pColorImgBuf,
    BYTE *pDepthImgBuf,
    unsigned long int *pColorImageSize,
    unsigned long int *pDepthImageSize,
    int *pColorSerial,
    int *pDepthSerial,
    int nDepthDataType)
{
    CEtronDI *pEtronDI;
    int nRet;
    int nRet1;

    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if (baseline_m0_video_3 == true) {
        if (pDevSelInfo->index == 0 || (pDevSelInfo->index == 1 && baseline_index < 3)) {
            if( g_pVideoDev[2*pDevSelInfo->index]->m_fival.width >    0 &&
                g_pVideoDev[2*pDevSelInfo->index]->m_fival.height >   0 &&
                g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.width >  0 &&
                g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.height > 0 ) {
                g_pVideoDev[2*pDevSelInfo->index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
                nRet = g_pVideoDev[2*pDevSelInfo->index]->GetImage(pColorImgBuf, pColorImageSize, pColorSerial, false, nDepthDataType);
                g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
                nRet1 = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->GetImage(pDepthImgBuf, pDepthImageSize, pDepthSerial, true, nDepthDataType);
            }
        } else {
            if( g_pVideoDev[2*pDevSelInfo->index]->m_fival.width >    0 &&
                g_pVideoDev[2*pDevSelInfo->index]->m_fival.height >   0 &&
                g_pVideoDev[1]->m_fival.width >  0 &&
                g_pVideoDev[1]->m_fival.height > 0 ) {
                g_pVideoDev[2*pDevSelInfo->index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
                nRet = g_pVideoDev[2*pDevSelInfo->index]->GetImage(pColorImgBuf, pColorImageSize, pColorSerial, false, nDepthDataType);
                g_pVideoDev[1]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
                nRet1 = g_pVideoDev[1]->GetImage(pDepthImgBuf, pDepthImageSize, pDepthSerial, true, nDepthDataType);
            }
        }
    } else {
        if( g_pVideoDev[2*pDevSelInfo->index]->m_fival.width >    0 &&
            g_pVideoDev[2*pDevSelInfo->index]->m_fival.height >   0 &&
            g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.width >  0 &&
            g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_fival.height > 0 ) {
            g_pVideoDev[2*pDevSelInfo->index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet = g_pVideoDev[2*pDevSelInfo->index]->GetImage(pColorImgBuf, pColorImageSize, pColorSerial, false, nDepthDataType);
            g_pVideoDev[2*pDevSelInfo->index + baseline_index]->m_devType = pEtronDI->m_Adi.deviceTypeArray[pDevSelInfo->index];
            nRet1 = g_pVideoDev[2*pDevSelInfo->index + baseline_index]->GetImage(pDepthImgBuf, pDepthImageSize, pDepthSerial, true, nDepthDataType);
        }
    }


    if(nRet  != ETronDI_OK) return nRet;
    if(nRet1 != ETronDI_OK) return nRet1;
    return nRet;
}
// image -

// for AEAWB Control +
int EtronDI_SetSensorTypeName(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    SENSOR_TYPE_NAME stn)
{
    CEtronDI *pEtronDI;
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->SetSensorTypeName(stn);
}

SENSOR_TYPE_NAME GetSensorType(const int pid)
{
    switch ( pid )
    {
    case ETronDI_PID_8037:
    case ETronDI_PID_8029:    return ETRONDI_SENSOR_TYPE_OV9714;
    case ETronDI_PID_8051:
    case ETronDI_PID_8052:
    case ETronDI_PID_8062:
    case ETronDI_PID_8036:    return ETRONDI_SENSOR_TYPE_AR0135;

    case ETronDI_PID_AMBER:
    case ETronDI_PID_8038_M1:
    case ETronDI_PID_8038:    return ETRONDI_SENSOR_TYPE_AR0144;

    case ETronDI_PID_8040S:   return ETRONDI_SENSOR_TYPE_AR0330;
    case ETronDI_PID_8040S_K: return ETRONDI_SENSOR_TYPE_AR1335;
    case ETronDI_PID_8054:    return ETRONDI_SENSOR_TYPE_AR0330;
    case ETronDI_PID_8054_K:  return ETRONDI_SENSOR_TYPE_AR1335;

    case ETronDI_PID_8053:    return ETRONDI_SENSOR_TYPE_H65;
    case ETronDI_PID_8059:    return ETRONDI_SENSOR_TYPE_H65;

    case ETronDI_PID_8060:    return ETRONDI_SENSOR_TYPE_AR0522;
    case ETronDI_PID_8060_K:  return ETRONDI_SENSOR_TYPE_AR0522;
    case ETronDI_PID_8060_T:  return ETRONDI_SENSOR_TYPE_AR0522;

    case ETronDI_PID_SALLY:   return ETRONDI_SENSOR_TYPE_OV9282;
    }
    return ( SENSOR_TYPE_NAME )EOF;
}

int EtronDI_GetExposureTime(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nSensorMode,
    float *pfExpTimeMS)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    g_pVideoDev[2*pDevSelInfo->index]->SetSensorTypeName(GetSensorType(pEtronDI->m_Adi.nPID[pDevSelInfo->index]));
    return g_pVideoDev[2*pDevSelInfo->index]->GetExposureTime(nSensorMode, pfExpTimeMS);
}

int EtronDI_SetExposureTime(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nSensorMode,
    float fExpTimeMS)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    g_pVideoDev[2*pDevSelInfo->index]->SetSensorTypeName(GetSensorType(pEtronDI->m_Adi.nPID[pDevSelInfo->index]));
    return g_pVideoDev[2*pDevSelInfo->index]->SetExposureTime(nSensorMode, fExpTimeMS);
}

int EtronDI_GetGlobalGain(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nSensorMode,
    float *pfGlobalGain)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    g_pVideoDev[2*pDevSelInfo->index]->SetSensorTypeName(GetSensorType(pEtronDI->m_Adi.nPID[pDevSelInfo->index]));
    return g_pVideoDev[2*pDevSelInfo->index]->GetGlobalGain(nSensorMode, pfGlobalGain);
}

int EtronDI_SetGlobalGain(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nSensorMode,
    float fGlobalGain)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    g_pVideoDev[2*pDevSelInfo->index]->SetSensorTypeName(GetSensorType(pEtronDI->m_Adi.nPID[pDevSelInfo->index]));
    return g_pVideoDev[2*pDevSelInfo->index]->SetGlobalGain(nSensorMode, fGlobalGain);
}

int  EtronDI_GetColorGain(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nSensorMode,
    float *pfGainR,
    float *pfGainG,
    float *pfGainB)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetColorGain(nSensorMode, pfGainR, pfGainG, pfGainB);
}

int  EtronDI_SetColorGain(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nSensorMode,
    float fGainR,
    float fGainG,
    float fGainB)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->SetColorGain(nSensorMode, fGainR, fGainG, fGainB);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
int EtronDI_GetAccMeterValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int *pX,
    int *pY,
    int *pZ)
 {
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetAccMeterValue( pX, pY, pZ);
}
#endif

int EtronDI_GetAEStatus(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    PAE_STATUS pAEStatus)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetAEStatus( pAEStatus);
}

int EtronDI_GetAWBStatus(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    PAWB_STATUS pAWBStatus)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetAWBStatus( pAWBStatus);
}

int EtronDI_EnableAE(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
{
    CEtronDI *pEtronDI;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->EnableAE(true);
}

int EtronDI_DisableAE(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->EnableAE(false);
}

int EtronDI_EnableAWB(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->EnableAWB(true);
}

int EtronDI_DisableAWB(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->EnableAWB(false);
}

int EtronDI_GetGPIOValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nGPIOIndex, BYTE *pValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL) g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetGPIOValue( nGPIOIndex, pValue);

}

int EtronDI_SetGPIOValue(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nGPIOIndex,
    BYTE nValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL) g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->SetGPIOValue( nGPIOIndex, nValue);

}

int EtronDI_SetGPIOCtrl(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nGPIOIndex,
    BYTE nValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL) g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->SetGPIOCtrl( nGPIOIndex, nValue);
}

int EtronDI_GetCTPropVal(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    long int *pValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetCTPropVal( nId, pValue);
}

int EtronDI_SetCTPropVal(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    long int nValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->SetCTPropVal( nId, nValue);
}

int EtronDI_GetPUPropVal(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    long int *pValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetPUPropVal( nId, pValue);
}

int EtronDI_SetPUPropVal(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    long int nValue)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->SetPUPropVal( nId, nValue);
}

int EtronDI_GetCTRangeAndStep(
    void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, int nId, int *pMax, int *pMin, int *pStep, int *pDefault, int *pFlags)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetCTRangeAndStep( nId, pMax, pMin, pStep, pDefault, pFlags);
}

int EtronDI_GetPURangeAndStep(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int nId,
    int *pMax,
    int *pMin,
    int *pStep,
    int *pDefault,
    int *pFlags)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetPURangeAndStep( nId, pMax, pMin, pStep, pDefault, pFlags);
}
// for AEAWB Control -

// for Calibration Log +
int EtronDI_GetRectifyLogData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    eSPCtrl_RectLogData *pData, int index)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetRectifyLogFromCalibrationLog( pData, index);
}

int EtronDI_GetRectifyMatLogData(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    eSPCtrl_RectLogData *pData, int index)
{
    CEtronDI *pEtronDI;
    int rc;
    char FWVersion[256];
    int len;
    if (g_bIsLogEnabled == true)
        LOGI("%s,\n", __func__);

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    memset(FWVersion, 0, sizeof(FWVersion));

    rc = EtronDI_GetFwVersion(pHandleEtronDI, pDevSelInfo, FWVersion, sizeof(FWVersion), &len);
    if (rc != 0)
        ETronDI_Init_Fail;
    if (strncmp(FWVersion, "EX8029", strlen("EX8029")) == 0) {
        bEX8029 = true;
        LOGI("EX8029 device...\n");
    } else {
        bEX8029 = false;
    }

    rc = g_pVideoDev[2*pDevSelInfo->index]->GetRecifyMatLogData(pData, index);
    if (rc != ETronDI_OK)
        return rc;

    /*
        Remind Task #4919
    */
    if (pData->ReProjectMat[0] != 0) {
        int NormalizeValue = pData->ReProjectMat[0];
        int array_num = sizeof(pData->ReProjectMat) / sizeof(pData->ReProjectMat[0]);
        for (int i = 0; i < array_num; i++) {
            pData->ReProjectMat[i] = pData->ReProjectMat[i] / NormalizeValue;
        }
    }

    return ETronDI_OK;
}
// for Calibration Log -

// for Post Process +
int EtronDI_EnablePostProcess(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    bool bEnable)
{
    return ETronDI_NotSupport;
}

int EtronDI_PostInitial(
    void *pHandleEtronDI)
{
    return ETronDI_NotSupport;
}

int EtronDI_PostEnd(
    void *pHandleEtronDI)
{
    return ETronDI_NotSupport;
}

int EtronDI_ProcessFrame(
    void *pHandleEtronDI,
    unsigned char *pYUY2Buf,
    unsigned char *pDepthBuf,
    unsigned char *OutputBuf,
    int width,
    int height)
{
    return ETronDI_NotSupport;
}
int EtronDI_PostSetParam(
    void *pHandleEtronDI,
    int Idx,
    int Val)
{
    return ETronDI_NotSupport;
}

int EtronDI_PostGetParam(
    void *pHandleEtronDI,
    int Idx,
    int *pVal)
{
    return ETronDI_NotSupport;
}

int EtronDI_Convert_Depth_Y_To_Buffer(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height, bool color, unsigned short nDepthDataType)
{
        CEtronDI *pEtronDI;
        pEtronDI = (CEtronDI *)pHandleEtronDI;
        if (!pEtronDI) return ETronDI_Init_Fail;
        //

        if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
                g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
        return g_pVideoDev[2*pDevSelInfo->index]->convert_depth_y_to_buffer(depth_y, rgb, width, height, color, nDepthDataType);
}

int EtronDI_Convert_Depth_Y_To_Buffer_offset(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height, bool color, unsigned short nDepthDataType, int offset)
{
        CEtronDI *pEtronDI;
        pEtronDI = (CEtronDI *)pHandleEtronDI;
        if (!pEtronDI) return ETronDI_Init_Fail;
        //

        if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
                g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
        return g_pVideoDev[2*pDevSelInfo->index]->convert_depth_y_to_buffer_offset(depth_y, rgb, width, height, color, nDepthDataType, offset);
}

bool EtronDI_IsInterleaveDevice(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    CEtronDI *pEtronDI;
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    char fwVer[256] = {'\0'};
    unsigned short tmp;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    nRet = EtronDI_GetFWRegister(pHandleEtronDI, pDevSelInfo, 0xED, &tmp, FG_Address_1Byte | FG_Value_1Byte);

    if (nRet == 0) {
        if (tmp == 0xff)
            return false;
    }

    return true;
}

int EtronDI_EnableInterleave(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, bool enable)
{
    CEtronDI *pEtronDI;
    int nRet;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    nRet = g_pVideoDev[2*pDevSelInfo->index]->SetFWRegister(0xED, enable ? 1 : 0, FG_Address_1Byte | FG_Value_1Byte);

    return nRet;
}

int EtronDI_SetControlCounterMode(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char nValue)
{
    CEtronDI *pEtronDI;
    int nRet;
    int flag = 0;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //


    flag |= FG_Address_1Byte;
    flag |= FG_Value_1Byte;


    nRet = EtronDI_SetFWRegister(pHandleEtronDI, pDevSelInfo, 0xE4, nValue, flag);

    return nRet;
}

int EtronDI_GetControlCounterMode(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char *nValue)
{
    CEtronDI *pEtronDI;
    int nRet;
     unsigned char tmp;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    nRet = EtronDI_GetFWRegister(pHandleEtronDI, pDevSelInfo, 0xE4, (unsigned short *)&tmp, FG_Address_1Byte | FG_Value_1Byte);

    if (nRet == 0) {
        *nValue = tmp;
    }

    return nRet;
}

int EtronDI_CreateSwPostProc(int depthBits, void **handle)
{
    return ETronDI_NotSupport;
}

int EtronDI_ReleaseSwPostProc(void** handle)
{
    return ETronDI_NotSupport;
}

int EtronDI_DoSwPostProc(void* handle, unsigned char* colorBuf, bool isColorRgb24, unsigned char* depthBuf, unsigned char* outputBuf, int width, int height)
{
    return ETronDI_NotSupport;
}

int EtronDI_FlyingDepthCancellation_D8(void *pHandleEtronDI,
                                       PDEVSELINFO pDevSelInfo,
                                       unsigned char *pdepthD8,
                                       int width,
                                       int height)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->FlyingDepthCancellation_D8(pdepthD8, width, height);
}

int EtronDI_FlyingDepthCancellation_D11( void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* pdepthD11, int width, int height )
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->FlyingDepthCancellation_D11(pdepthD11, width, height);
}

// for Post Process -

// for sensorif +
int EtronDI_EnableSensorIF(
    void *pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    bool bIsEnable)
{
    CEtronDI *pEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    //
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    //

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->EnableSensorIF(bIsEnable);
}
// for sensorif -
#ifndef TINY_VERSION
int EtronDI_GenerateLutFile(
    void* pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    const char* filename)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    return g_pVideoDev[2 * pDevSelInfo->index]->GenerateLutFile(filename);
}

int EtronDI_SaveLutData(
    void* pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    const char* filename)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    return g_pVideoDev[2 * pDevSelInfo->index]->SaveLutData(filename);
}
int EtronDI_GetLutData(
        void* pHandleEtronDI,
        PDEVSELINFO pDevSelInfo,
        BYTE* buffer, int nSize)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    //return g_pVideoDev[2 * pDevSelInfo->index]->ReadLutData(buffer, nSize);
    LOGD("GetLutData %d\n", nSize);
    if (nSize != sizeof(eys::fisheye360::ParaLUT))
        return ETronDI_ErrBufLen;
    int ret = g_pVideoDev[2 * pDevSelInfo->index]->GetUserData(buffer, nSize, USERDATA_SECTION_0);
    return ret;
}

int EtronDI_EncryptMP4(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo, const char *filename)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    return g_pVideoDev[2 * pDevSelInfo->index]->EncryptMp4(filename);
}

int EtronDI_DecryptMP4(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo, const char* filename)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    return g_pVideoDev[2 * pDevSelInfo->index]->DecryptMp4(filename);
}
#ifndef DOXYGEN_SHOULD_SKIP_THIS
int EtronDI_InjectExtraDataToMp4(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo,
        const char* filename, const char* data, int dataLen)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    return g_pVideoDev[2 * pDevSelInfo->index]->InjectExtraDataToMp4(filename, data, dataLen);
}

int EtronDI_RetrieveExtraDataFromMp4(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo,
        const char* filename, char* data, int* dataLen)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    if (pEtronDI == NULL)
    {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
    {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    return g_pVideoDev[2 * pDevSelInfo->index]->RetrieveExtraDataFromMp4(filename, data, dataLen);
}

int EtronDI_EncryptString(const char* src, char* dst)
{
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    return CVideoDevice::EncryptString(src, dst);
}

int EtronDI_DecryptString(const char* src, char* dst)
{
    return CVideoDevice::DecryptString(src, dst);
}

int EtronDI_EncryptString(const char* src1, const char* src2, char* dst)
{
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    return CVideoDevice::EncryptString(src1, src2, dst);
}

int EtronDI_DecryptString(const char* src, char* dst1, char* dst2)
{
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    return CVideoDevice::DecryptString(src, dst1, dst2);
}
#endif
#endif
int EtronDI_GetAutoExposureMode(
    void* pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short* mode)
{
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    return EtronDI_GetFWRegister(pHandleEtronDI, pDevSelInfo, 0x84, mode, FG_Address_1Byte | FG_Value_1Byte);
}

int EtronDI_SetAutoExposureMode(
    void* pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    unsigned short mode)
{
    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    return EtronDI_SetFWRegister(pHandleEtronDI, pDevSelInfo, 0x84, mode, FG_Address_1Byte | FG_Value_1Byte);
}

#ifndef __WEYE__
//__WEYE__ no support Gyro
// for Gyro +

#ifndef UAC_NOT_SUPPORTED
static int uac_skip_cnt = 10;

int EtronDI_getUACNAME(char *input, char *output)
{
    return  uac_name(input, output);
}

int EtronDI_InitialUAC(char *deviceName)
{
    LOGI("Not Ready");
    return -1;
    uac_skip_cnt = 10;

    return  uac_init(deviceName);
}

int EtronDI_WriteWaveHeader(int fd)
{
    LOGI("Not Ready");
    return -1;
    return begin_wave(fd);
}

int EtronDI_WriteWaveEnd(int fd, size_t length)
{
    return end_wave(fd, length);
}

int EtronDI_GetUACData(unsigned char *buffer, int length)
{
    LOGI("Not Ready");
    return -1;

    return uac_get(buffer, length);
}

int EtronDI_ReleaseUAC(void)
{
    LOGI("Not Ready");
    return -1;
    return uac_release();
}

#endif //UAC_NOT_SUPPORTED

static int hid_fd =  -1;
int EtronDI_InitialHidGyro(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;
    int i = 0;
    char path[256];
    char buf2[256];

    if (pEtronDI == NULL) {
        return ETronDI_Init_Fail;
    }

    LOGI("\n");
    do {
        sprintf(path, "/dev/hidraw%d", i);
        //hid_fd = open(path, O_RDWR|O_NONBLOCK);
        hid_fd = open(path, O_RDWR); // default BLOCK mode
        if(hid_fd <= 0) {
            LOGI("open IMU device error! = %d\n",  hid_fd);
            i++;
            continue;
        }

        LOGI("hid_fd = %d\n", hid_fd);
        int res = ioctl(hid_fd, HIDIOCGRAWNAME(256), buf2);
        if (res < 0) {
            //perror("HIDIOCGRAWNAME");
            i++;
            continue;
        } else {
           LOGD("Raw Name: %s\n", buf2);
        }

        if (strstr(buf2, "EX8040S") > 0) {
            LOGI("Got EX8040 IMU Device hid_fd = %d\n", hid_fd);
            break;
        } else if (strstr(buf2, "YX8060_DIMU ") > 0) {
            LOGI("Got YX8060 IMU Device hid_fd = %d\n", hid_fd);
            break;
        } else if (strstr(buf2, "FrogEye2_IMU") > 0) {
            LOGI("Got YX8060 IMU Device hid_fd = %d\n", hid_fd);
            break;
        } else {
            LOGI("Can't find IMU Device %d\n", hid_fd);
            close(hid_fd);
            hid_fd =  -1;
            i++;
            continue;
        }
    } while(i < MAX_DEV_COUNT);

    if (hid_fd < 0) {
       LOGE("No IMU device\n");
       return ETronDI_NoDevice;
    }

    LOGD("%s: hid_fd = %d\n", __func__, hid_fd);

    return ETronDI_OK;
}

int EtronDI_ReleaseHidGyro(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;

    if (pEtronDI == NULL) {
        return ETronDI_Init_Fail;
    }

    if (hid_fd > 0)
        close(hid_fd);

    return ETronDI_OK;
}

#define READ_STATUS  	0
#define WRITE_STATUS 	1
#define EXCPT_STATUS 	2
/*
 * s    - fd
 * sec  - timeout seconds
 * usec - timeout microseconds
 * x    - select status
 */
int x_select(int s, int sec, int usec, int x)
{
    int st = -1, maxfd = s + 1;
    struct timeval to;
    fd_set fs, fs_e;

    if (sec == 0 && usec == 0) {
        LOGE("sec == 0 && usec == 0 =>Bypass waiting, go to IOCtrl directly\n");
        return 1;
    }

    if (maxfd > (FD_SETSIZE - 1)){
        LOGE("maxfd > FD_SETSIZE-1\n");
        return -1;
    }

    to.tv_sec = sec;
    to.tv_usec = usec;
    if (s < 0) {
        LOGE("s < 0\n");
        return -1;
    }

    FD_ZERO(&fs);
    FD_SET(s, &fs);
    FD_ZERO(&fs_e);
    FD_SET(s, &fs_e);

    switch(x) {
    case READ_STATUS:
        st = select(maxfd, &fs, 0, &fs_e, &to);
            break;

    case WRITE_STATUS:
        st = select(maxfd, 0, &fs, &fs_e, &to);
            break;

    case EXCPT_STATUS:
        st = select(maxfd, 0, 0, &fs, &to);
        break;

    default:
            return st;
    }
    if(st <= 0)
        LOGE("STATUS : %d , return st : %d\n",x,st);

    if (st > 0 && FD_ISSET(s, &fs)==1 && FD_ISSET(s, &fs_e)==0) {
        return 1;
    } else if (st < 0) {
        return -1;
    } else {
        return 0;
    }
}

int EtronDI_GetHidGyro(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo,
                       unsigned char *pBuffer,
                       int length)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    int ret = -1;

    if (pEtronDI == NULL)
        return ETronDI_Init_Fail;

    LOGD("%s: hid_fd = %d\n", __func__, hid_fd);
    if (hid_fd < 0) {
       LOGE("No IMU device\n");
       return ETronDI_NoDevice;
    }

    if (x_select(hid_fd, 0, 166666, READ_STATUS) > 0) { // 16.6 msec
        ret = read(hid_fd, pBuffer, length);
        if (ret <= 0) {  //almost 21 bytes
            return ETronDI_READ_REG_FAIL;
        }
    }

    return ETronDI_OK;
}

int EtronDI_SetupHidGyro(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo,
                       unsigned char *pCmdBuf, int cmdlength)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    int ret = -1, i;
    unsigned char buffer[32];

    if (pEtronDI == NULL)
        return ETronDI_Init_Fail;

    memset(buffer, 0, sizeof(buffer));
    memcpy(&buffer[1], pCmdBuf, cmdlength);
    int res = ioctl(hid_fd, HIDIOCSFEATURE(9), buffer);
    if (res < 0) {
        perror("HIDIOCSFEATURE");
        return ETronDI_Init_Fail;
    }

    for (i = 0; i < res; i++)
        LOGI("%hhx ", buffer[i]);

    puts("\n");

    return ETronDI_OK;
}

int EtronDI_InitSRB(void **ppSrbHandle, int QueueSize, char *srbName)
{
    ShmRingBuffer<shm_packet_s>* pSrbHandle = new ShmRingBuffer<shm_packet_s>(QueueSize, true, srbName);
    *ppSrbHandle = pSrbHandle;

    return 0;
}

int EtronDI_PutSRB(void *pSrbHandle, srb_packet_s *pPacket)
{
    ShmRingBuffer<srb_packet_s> *pSRB = (ShmRingBuffer<srb_packet_s> *)pSrbHandle;
    pSRB->push_back(*pPacket);
    return 0;
}

int EtronDI_GetSRB(void *pSrbHandle, srb_packet_s *pPacket)
{
    int i = 0;
    ShmRingBuffer<shm_packet_s> *pSRB = (ShmRingBuffer<shm_packet_s> *)pSrbHandle;
    srb_packet_s *packet;
    do {
        packet = (srb_packet_s *)pSRB->unparse(i);

        if (packet == NULL) {
             usleep(10 * 1000);
             return -1;
        }

        if (packet->bisReady == true) {
            pPacket->serial = packet->serial;
            pPacket->len = packet->len;
            pPacket->bisRGB = packet->bisRGB;
            pPacket->bisReady = packet->bisReady;
            if (packet->len > 19660800) { //for work around...2560 x 2560 x 3
                return -1;
            }
            if (packet->bisRGB == true) {
                memcpy(pPacket->buffer_RGB , packet->buffer_RGB, packet->len);
            } else {
                memcpy(pPacket->buffer_yuyv , packet->buffer_yuyv, packet->len);
            }

            packet->bisReady = false;
            packet = (srb_packet_s *)pSRB->unparse(i);
            return 0;
        } else {
            LOGD("packet->bisReady = false\n");
            usleep(10 * 1000);
        }
        i++;
    } while (i < QUEUE_LENGTH);

    return -1;
}

/*! \fn int EtronDI_ReleaseSRB(void *pSrbHandle)
    \brief Release the SRB Class
    \param void *pSrbHandle pointer to SRB class
    \return success: EtronDI_OK, others: see eSPDI_def.h
*/

int EtronDI_ReleaseSRB(void *pSrbHandle)
{
    ShmRingBuffer<packet_s> *pSRB = (ShmRingBuffer<packet_s> *)pSrbHandle;
    delete pSRB;

    return 0;
}

int EtronDI_InitialCmdFiFo(const char *pfifoName, int *pFileDescrption, bool bRead)
{
    if (access(pfifoName, F_OK ) != 0) {
       mkfifo(pfifoName, 0777);
    } 
    LOGI("before open path = %s\n", pfifoName);

    if (bRead == true)
        *pFileDescrption = open(pfifoName, O_RDONLY);
    else
        *pFileDescrption = open(pfifoName, O_WRONLY);

    if (*pFileDescrption <= 0) {
        LOGI("Failed, fd = %d, open cmd fifo incorrect\n", *pFileDescrption);
        perror(pfifoName);
        exit(-1);
    }

    LOGI("success open path = %s\n", pfifoName);
    return 0;
}

int EtronDI_CloseCmdFiFo(int FileDescrption)
{

    if (FileDescrption > 0) {
        close(FileDescrption);
    }

    return 0;
}

int EtronDI_WriteCmdFiFo(int FileDescrption, unsigned char *pCmd, int len)
{
    int rdlength = 0, tmp_len = -1;

    if (FileDescrption <= 0) {
        LOGE("Please initial the Cmd Fifo\n");
        return -1;
    }

    tmp_len = len;
    do {
        rdlength = write(FileDescrption, &pCmd[rdlength], tmp_len);
        if (rdlength <= 0) {
            return 0;
        }
        tmp_len -= rdlength;
    } while(tmp_len != 0);

    return 0;
}

int EtronDI_ReadCmdFiFo(int FileDescrption, unsigned char *pBuf, int len)
{
    int rdlength = 0, tmp_len = -1;

    if (FileDescrption <= 0) {
        LOGE("Please initial the Cmd Fifo\n");
        return -1;
    }

    tmp_len = len;
    do {
        rdlength = read(FileDescrption, &pBuf[rdlength], tmp_len);
        if (rdlength <= 0) {
            return 0;
        }
        tmp_len -= rdlength;
    } while(tmp_len != 0);

    return 0;
}

int EtronDI_GetInfoHidGyro(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo,
                       unsigned char *pCmdBuf, int cmdlength,
                       unsigned char *pResponseBuf, int *resplength)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    int ret = -1;
    unsigned char buffer[256];

    if (pEtronDI == NULL)
        return ETronDI_Init_Fail;

    memset(buffer, 0, sizeof(buffer));
    memcpy(&buffer[1], pCmdBuf, cmdlength);

    int res = ioctl(hid_fd, HIDIOCSFEATURE(9), buffer);
    if (res < 0) {
        perror("HIDIOCGFEATURE");
        return ETronDI_Init_Fail;
    }

    res = ioctl(hid_fd, HIDIOCGFEATURE(9), buffer);

    if (res < 0) {
        LOGE("Get Info Fail\n");
        return ETronDI_Init_Fail;
    }

    memcpy(pResponseBuf, buffer, res);
    *resplength = res;

    return ETronDI_OK;
}


static int fd_int_event;
int EtronDI_InitialFlexibleGyro(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;
    int rc;
    char eventPath[64];

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    else imu_testing = true;

    if (pEtronDI == NULL) {
        return ETronDI_Init_Fail;
    }

    if (getIntEventPath(0x1e4e, eventPath)) {
        return ETronDI_Init_Fail;
    }

    fd_int_event = open(eventPath, O_RDONLY);
    if (fd_int_event <= 0) {
        return ETronDI_Init_Fail;
    }
    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL) {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }
    rc = g_pVideoDev[2 * pDevSelInfo->index]->GetMutex();

    return rc;
}

int EtronDI_ReleaseFlexibleGyro(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    int rc;

    if (imu_testing == true)
        imu_testing = false;
    else
        return ETronDI_Init_Fail;

    if (fd_int_event > 0)
        close(fd_int_event);

    rc = g_pVideoDev[2 * pDevSelInfo->index]->ReleaseMutex();

    return rc;
}

int EtronDI_GetFlexibleGyroData(
    void* pHandleEtronDI,
    PDEVSELINFO pDevSelInfo,
    int length,
    unsigned char *pGyroData)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;
    int rc;

    if (imu_testing == false) return ETronDI_Init_Fail;

    if (length < 128) {
                return ETronDI_ErrBufLen;
        }

    if (pEtronDI == NULL) {
        return ETronDI_Init_Fail;
    }

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL) {
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    }

    rc = g_pVideoDev[2 * pDevSelInfo->index]->ReadFlexibleGyroData(pGyroData, length);

    return rc;
}

int EtronDI_GetImageInterrupt(void)
{
    return getInterrupt(fd_int_event);
}


int EtronDI_GetFlexibleGyroLength(void* pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned short* GyroLen)
{
    CEtronDI* pEtronDI = (CEtronDI*)pHandleEtronDI;
    int nRet = -1;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;

    if (pEtronDI == NULL) {
        return ETronDI_Init_Fail;
    }

    unsigned short BytePerSample = 0, SamplePerFrame = 0;

    nRet = EtronDI_GetFWRegister(pEtronDI, pDevSelInfo, 0x84, &BytePerSample, FG_Address_1Byte | FG_Value_1Byte);
    if (nRet != ETronDI_OK)
        return nRet;

    nRet = EtronDI_GetFWRegister(pEtronDI, pDevSelInfo, 0x86, &SamplePerFrame, FG_Address_1Byte | FG_Value_1Byte);
    if (nRet != ETronDI_OK)
        return nRet;

    *GyroLen = (BytePerSample -2)  * SamplePerFrame + 4;

    return ETronDI_OK;
}


static int getbytesPerPixel(EtronDIImageType::Value imgType)
{
    switch(imgType) {
    case EtronDIImageType::DEPTH_8BITS: return 1;
    case EtronDIImageType::COLOR_YUY2:
    case EtronDIImageType::DEPTH_8BITS_0x80:
    case EtronDIImageType::DEPTH_11BITS:
    case EtronDIImageType::DEPTH_14BITS: return 2;
    case EtronDIImageType::COLOR_RGB24:  return 3;
    default:
        break;
    }
    LOGE("%s, imgType = %d\n", __func__, imgType);
    return -1;
}

int EtronDI_ResizeImgToHalf(EtronDIImageType::Value imgType, int width, int height,
                            unsigned char *src, unsigned char *dst, int len)
{
    int bytesPerPixel = getbytesPerPixel(imgType);
    unsigned int imgSize = 0;
    int dstSize = 0, i, j;
    int resize_height, resize_width;

    if (bytesPerPixel < 0) {
        LOGE("%s,bytesPerPixel incorrect\n", __func__);
        return ETronDI_NotSupport;
    }

    if (bytesPerPixel == 1) {
        resize_width =  width / 2;
        resize_height = height /  2;
    } else {
        resize_width =  width / 2;
        resize_height = height /  2;
    }

    imgSize = resize_width * resize_height * bytesPerPixel;
    if ((int)imgSize > len) {
        LOGE("%s, Image size incorrect = %d, but len = %d\n", __func__, imgSize, len);
        return ETronDI_ErrBufLen;
    }


    if (imgType == EtronDIImageType::COLOR_YUY2) {
       /*
           http://www.fourcc.org/pixel-format/yuv-yuy2/
            Get 4 pixels every times, but actually YUYV just need 2 pixels .
            The 2 Pixels of the half size image is equal to the 4 Pixels of the full size image.
            One  pixel is equal to two bytes.
        */
        for (i = 0; i < height; i += 2) {
            for (j = 0; j < (width * 2); j += 8){
                memcpy(&dst[dstSize], &(src[i * width * 2 + j]), bytesPerPixel * 2);
                dstSize += bytesPerPixel * 2;
            }
        }
    } else if (bytesPerPixel == 3) {
        /*
            "COLOR_RGB24", bytesPerPixel = 3;
            For the image type RGB24, the one pixel is the three bytes.
            Get 2 pixel every times, but actually RGB24 just need 1 pixels .
            Because The one  Pixel of the half size image is equal to the two Pixels of the full size image.
        */
        for (i = 0; i < height; i += 2) {
            for (j = 0; j < width; j += 2) {
                memcpy(dst + dstSize, &(src[(i * width + j) * bytesPerPixel]), bytesPerPixel);
                dstSize += bytesPerPixel;
            }
        }
    } else if (bytesPerPixel == 2) {
        /*
            "DEPTH_8BITS", bytesPerPixel = 1;`
            "COLOR_YUY2 \ DEPTH_8BITS_0x80 \ DEPTH_11BITS \ DEPTH_14BITS", bytesPerPixel == 2;
        */
        for (i = 0; i < height; i += 2) {
            for (j = 0; j < (width * 2); j += 4){
                memcpy(&dst[dstSize], &(src[i * width * 2 + j]), bytesPerPixel);
                dstSize += bytesPerPixel;
            }
        }
    } else if (bytesPerPixel == 1) {
        /*
            "DEPTH_8BITS", bytesPerPixel = 1;
        */
//        for (i = 0; i < height; i += 2) {
//            for (j = 0; j < width * 2; j += 2) {
//                memcpy(dst + dstSize, &(src[(i * width + j) * bytesPerPixel]), bytesPerPixel);
//                dstSize += bytesPerPixel;
//            }
//        }
        for (i = 0; i < height; i += 2) {
            for (j = 0; j < (width * 2); j += 2){
                memcpy(&dst[dstSize], &(src[i * width * 2 + j]), bytesPerPixel);
                dstSize += bytesPerPixel;
            }
        }
    }


    return ETronDI_OK;
}

#include "opencv2/core.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/opencv.hpp"

static int GetImgType(int bytesPerPixel)
{
    switch (bytesPerPixel) {
    case 1: return CV_8UC1;
    case 2: return CV_8UC2;
    case 3: return CV_8UC3;
    case 4: return CV_8UC4;
    default: return -1;
    }

    return ETronDI_NotSupport;
}


static int Img_Flip_CV(
        const int img_cols,
        const int img_rows,
        const int bytesPerPixel,
        unsigned char *img_src,
        int flip_code,
        unsigned char *img_dst
    )
{
    cv::Mat cv_img_src = cv::Mat(img_rows, img_cols, GetImgType(bytesPerPixel));
    cv::Mat cv_img_dst;
    cv_img_src.data = img_src;

    if (!cv_img_src.data) {
        return -1;
    }

    flip(cv_img_src, cv_img_dst, flip_code);

    if (!cv_img_dst.data) {
        return -2;
    }

    memcpy(img_dst, cv_img_dst.data,
           cv_img_dst.rows * cv_img_dst.cols * bytesPerPixel);
    return 0;

    return ETronDI_NotSupport;
}

int EtronDI_ImgMirro(EtronDIImageType::Value imgType, int width, int height,
                        unsigned char *src, unsigned char *dst)
{
    int bytesPerPixel = getbytesPerPixel(imgType);
    int i;
    int ret = -1;
    unsigned char data;

    if (bytesPerPixel < 0) {
        LOGE("%s,bytesPerPixel incorrect\n", __func__);
        return ETronDI_NotSupport;
    }

    if (bytesPerPixel == 1) {
        width *= 2;
    }

    ret = Img_Flip_CV(width, height, bytesPerPixel, src, 1, dst);
    if (ret < 0) {
        LOGE("%s, Img_Flip_CV incorrect\n", __func__);
        return ETronDI_NotSupport;
    }

    if (bytesPerPixel == 2) {
        for (i = 0; i < width * height * bytesPerPixel; i++) {
            if (((i + 1) % 4) == 0) {
                data = dst[i];
                dst[i] = dst[i - 2];
                dst[i - 2] = data;
            }
        }
    }

    return ETronDI_OK;
}


static int Img_Rot90_CV(
    const int img_cols,
    const int img_rows,
    const int bytesPerPixel,
    unsigned char *img_src,
    int flip_code,
    unsigned char *img_dst
)
{
    return ETronDI_NotSupport;
}

static int Img_Rot180_CV(
    const int img_cols,
    const int img_rows,
    const int bytesPerPixel,
    unsigned char *img_src,
    unsigned char *img_dst
)
{
    return ETronDI_NotSupport;
}

int EtronDI_RotateImg90(EtronDIImageType::Value imgType, int width, int height,
                        unsigned char *src, unsigned char *dst, int len, bool clockwise)
{
    int bytesPerPixel = getbytesPerPixel(imgType);
    unsigned int imgSize = 0;
    int dstSize = 0, i, j;

    if (bytesPerPixel < 0) {
        LOGE("%s,bytesPerPixel incorrect\n", __func__);
        return ETronDI_NotSupport;
    }

    if (clockwise == false) {
        LOGE("%s, clockwise incorrect\n", __func__);
        return ETronDI_NotSupport;
    }

    imgSize = width * height * bytesPerPixel;
    if ((int)imgSize > len) {
        LOGE("%s, Image size incorrect = %d, but len = %d\n", __func__, imgSize, len);
        return ETronDI_ErrBufLen;
    }


#if 1
    static std::unique_ptr<CComputer> pComputer(CComputerFactory::GetInstance()->GenerateComputer());
    pComputer->ImageRotate90(imgType, 
                             width, height,
                             src, dst, len, clockwise);
#else
    if (imgType == EtronDIImageType::COLOR_YUY2) {
        int jBase[2] = { 0, 0 };
        int dstY[2][2] = { { 0, 0 }, { 0, 0 } };
        for (int j = 0; j < height; j += 2) {
            jBase[0] = j * width * 2;
            jBase[1] = jBase[0] + width * 2;
            for (int i = 0; i < width; i += 2) {
                dstY[0][0] = (i * height + (height - 1 - j)) * 2;
                dst[dstY[0][0]] = src[jBase[0] + i * 2];
                dst[dstY[0][0] + 1] = src[jBase[0] + i * 2 + 3];

                dstY[0][1] = ((i + 1) * height + (height - 1 - j)) * 2;
                dst[dstY[0][1]] = src[jBase[0] + i * 2 + 2];
                dst[dstY[0][1] + 1] = src[jBase[0] + i * 2 + 3];

                dstY[1][0] = (i * height + (height - 1 - (j + 1))) * 2;
                dst[dstY[1][0]] = src[jBase[1] + i * 2];
                dst[dstY[1][0] + 1] = src[jBase[1] + i * 2 + 1];

                dstY[1][1] = ((i + 1) * height + (height - 1 - (j + 1))) * 2;
                dst[dstY[1][1]] = src[jBase[1] + i * 2 + 2];
                dst[dstY[1][1] + 1] = src[jBase[1] + i * 2 + 1];
            }
        }
    } else if (bytesPerPixel == 3) {
        return Img_Rot90_CV(width, height, bytesPerPixel, src, clockwise, dst);
    } else if (bytesPerPixel == 2) {
        /*
            "DEPTH_8BITS", bytesPerPixel = 1;`
            "COLOR_YUY2 \ DEPTH_8BITS_0x80 \ DEPTH_11BITS \ DEPTH_14BITS", bytesPerPixel == 2;
        */
        for (j = 0; j < (width * 2); j += 2) {
            for (i = 0;  i < height; i++) {
                memcpy(&dst[dstSize], &(src[(height - i - 1) * width * 2 + j]), 2);
                dstSize += 2;
            }
        }

    } else if (bytesPerPixel == 1) {
        /*
            "DEPTH_8BITS", bytesPerPixel = 1;
        */
        for (j = 0; j < (width); j ++) {
            for (i = 0;  i < height; i++) {
                memcpy(&dst[dstSize], &(src[(height - i - 1) * width + j]), 1);
                dstSize ++;
            }
        }
    }
#endif


    return ETronDI_OK;
}

int EtronDI_RotateImg180(EtronDIImageType::Value imgType, int width, int height,
                        unsigned char *src, unsigned char *dst, int len)
{
    int bytesPerPixel = getbytesPerPixel(imgType);
    unsigned int imgSize = 0;
    int dstSize = 0;

    if (bytesPerPixel < 0) {
        LOGE("%s,bytesPerPixel incorrect\n", __func__);
        return ETronDI_NotSupport;
    }

    imgSize = width * height * bytesPerPixel;
    if ((int)imgSize > len) {
        LOGE("%s, Image size incorrect = %d, but len = %d\n", __func__, imgSize, len);
        return ETronDI_ErrBufLen;
    }

    if (imgType == EtronDIImageType::COLOR_YUY2) {

        int x = 0;
        for (int n = width * height * 2 - 1; n >= 3; n -= 4)
        {
            memcpy(&dst[x], &(src[n - 1]), 1);        // Y1->Y0
            memcpy(&dst[x + 1], &(src[n - 2]), 1);    // U
            memcpy(&dst[x + 2], &(src[n - 3]), 1);    // Y0->Y1
            memcpy(&dst[x + 3], &(src[n]), 1);        // V
            x += 4;
        }
    } else if (bytesPerPixel == 3) {
        return Img_Rot180_CV(width, height, bytesPerPixel, src, dst);
    } else if (bytesPerPixel == 2 || bytesPerPixel == 1) {
        int wh = width * height * bytesPerPixel;
        for (int i = wh; i >= bytesPerPixel; i -= bytesPerPixel) {
            memcpy(&dst[dstSize], &(src[i-bytesPerPixel]), bytesPerPixel);
            dstSize += bytesPerPixel;
        }
    } else {
        return ETronDI_NotSupport;
    }

    return ETronDI_OK;
}

int EtronDI_DepthMerge( void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char** pDepthBufList, float *pDepthMergeOut,
    unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, float fFocus, float * pBaseline, float * pWRNear, float * pWRFar, float * pWRFusion, int nMergeNum )
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->DepthMerge(pDepthBufList, pDepthMergeOut,
                                                         pDepthMergeFlag, nDWidth, nDHeight,
                                                         fFocus, pBaseline,
                                                         pWRNear, pWRFar,
                                                         pWRFusion, nMergeNum);
}

int EtronDI_GetPointCloud( void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* ImgColor, int CW, int CH,
                           unsigned char* ImgDepth, int DW, int DH,
                           PointCloudInfo* pPointCloudInfo,
                           unsigned char* pPointCloudRGB, float* pPointCloudXYZ, float Near, float Far )
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    unsigned short pid = pEtronDI->m_Adi.nPID[pDevSelInfo->index];

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->GetPointCloud(ImgColor, CW, CH,
                                                            ImgDepth, DW, DH,
                                                            pPointCloudInfo,
                                                            pPointCloudRGB, pPointCloudXYZ, Near, Far, pid);
}

int EtronDI_ColorFormat_to_RGB24( void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* ImgDst, unsigned char* ImgSrc, int SrcSize, int width, int height, EtronDIImageType::Value type )
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->ColorFormat_to_RGB24(ImgDst, ImgSrc,
                                                                   SrcSize,
                                                                   width, height, type);
}

int EtronDI_RotateImg90(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst, int len, bool clockwise)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->RotateImg90(imgType,
                                                          width, height,
                                                          src, dst, len,
                                                          clockwise);
}

int EtronDI_ImgMirro(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    return g_pVideoDev[2*pDevSelInfo->index]->ImgMirro(imgType,
                                                       width, height,
                                                       src, dst);
}

// __attribute__((packed)) on non-Intel arch may cause some unexpected error, plz be informed.

typedef struct tagBITMAPFILEHEADER {
    unsigned short    bfType; // 2  /* Magic identifier */
    unsigned int   bfSize; // 4  /* File size in bytes */
    unsigned short    bfReserved1; // 2
    unsigned short    bfReserved2; // 2
    unsigned int   bfOffBits; // 4 /* Offset to image data, bytes */
} __attribute__((packed)) BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    unsigned int    biSize;     // 4 /* Header size in bytes */
    int             biWidth;    // 4 /* Width of image */
    int             biHeight;   // 4 /* Height of image */
    unsigned short  biPlanes;   // 2 /* Number of colour planes */
    unsigned short  biBitCount; // 2 /* Bits per pixel */
    unsigned int    biCompress; // 4 /* Compression type */
    unsigned int    biSizeImage; // 4 /* Image size in bytes */
    int             biXPelsPerMeter; // 4
    int             biYPelsPerMeter; // 4 /* Pixels per meter */
    unsigned int    biClrUsed; // 4 /* Number of colours */
    unsigned int    biClrImportant; // 4 /* Important colours */
} __attribute__((packed)) BITMAPINFOHEADER;

/*
typedef struct tagRGBQUAD
{
    unsigned char    rgbBlue;
    unsigned char    rgbGreen;
    unsigned char    rgbRed;
    unsigned char    rgbReserved;
} RGBQUAD;
* for biBitCount is 16/24/32, it may be useless
*/

typedef struct
{
        unsigned char    b;
        unsigned char    g;
        unsigned char    r;
} RGB_data; // RGB TYPE, plz also make sure the order

int bmp_generator(char *filename, int width, int height, unsigned char *data)
{
    BITMAPFILEHEADER bmp_head;
    BITMAPINFOHEADER bmp_info;
    int size = width * height * 3;

    bmp_head.bfType = 0x4D42; // 'BM'
    bmp_head.bfSize= size + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); // 24 + head + info no quad
    bmp_head.bfReserved1 = bmp_head.bfReserved2 = 0;
    bmp_head.bfOffBits = bmp_head.bfSize - size;
    // finish the initial of head

    bmp_info.biSize = sizeof(BITMAPINFOHEADER);
    bmp_info.biWidth = width;
    bmp_info.biHeight = height;
    bmp_info.biPlanes = 1;
    bmp_info.biBitCount = 24; // bit(s) per pixel, 24 is true color
    bmp_info.biCompress = 0;
    bmp_info.biSizeImage = size;
    bmp_info.biXPelsPerMeter = 0;
    bmp_info.biYPelsPerMeter = 0;
    bmp_info.biClrUsed = 0 ;
    bmp_info.biClrImportant = 0;
    // finish the initial of infohead;

    // copy the data
    FILE *fp;
    if (!(fp = fopen(filename,"wb"))) return 0;

    fwrite(&bmp_head, 1, sizeof(BITMAPFILEHEADER), fp);
    fwrite(&bmp_info, 1, sizeof(BITMAPINFOHEADER), fp);
    for (int h = 0 ; h < height ; h++)
    {
        for (int w = 0 ; w < width ; ++w)
        {
            fwrite(data + ((width * 3) * (height - (h + 1))) + (w * 3) + 2, 1, 1, fp);
            fwrite(data + ((width * 3) * (height - (h + 1))) + (w * 3) + 1, 1, 1, fp);
            fwrite(data + ((width * 3) * (height - (h + 1))) + (w * 3), 1, 1, fp);
        }
    }
    fflush(fp);
    fclose(fp);
    return 0;
}

int EtronDI_RGB2BMP(char *filename, int width, int height, unsigned char *data)
{
    return bmp_generator(filename, width, height, data);
}

int EtronDI_HoleFilled(unsigned short *pDImgIn, unsigned short *pDImgOut, int width, int height, int holeFilldiff)
{
    int HOLEFILL_DIR_MASK = 1;
        int HOLEFILL_DIFF = holeFilldiff;

        unsigned short *dptr = pDImgIn;
        unsigned short *dout = pDImgOut;

        unsigned short *forward = new unsigned short [width + 10];
        unsigned short *backward = new unsigned short [width + 10];
        unsigned short *result = new unsigned short [width];

        int mask0 = HOLEFILL_DIR_MASK & 1;
        int mask1 = (HOLEFILL_DIR_MASK >> 1) & 1;
        int mask2 = (HOLEFILL_DIR_MASK >> 2) & 1;
        int mode = 0;//pREG->HOLEFILL_MODE;

#define d_top dptr[(y-1)*width + x  ]
#define d_t_l dptr[(y-1)*width + x-1]
#define d_t_r dptr[(y-1)*width + x+1]
#define d_rgt backward[x+1]
#define d_lft forward[x-1]

        for (int y = 0; y < height; y++) {
            // backward process

            for (int x = width-1; x >= 0; x-- ) {
                int center = x + y * width; // address of the center pixel
                int new_d;

                if (!dptr[center]) { //it means disp = 0
                    if (mode) {

                    } else {
                        if(y == 0 && x == width-1)
                            new_d = 0;
                        else if(y == 0 || mask0)
                            new_d = x == width-1 ? 0 : d_rgt;
                        else if(x == width-1 || mask1)
                            new_d = y == 0 ? 0 : d_top;
                        else
                            new_d = min(d_top, d_rgt);
                    }

                    backward[x] = new_d;

                } else {
                    backward[x] = dptr[center];
                }
            }

            // forward process
            for (int x = 0; x < width; x++ ) {
                int center = x + y * width; // address of the center pixel

                int new_d;

                if(!dptr[center]) { //it means disp = 0
                    if(mode) {

                    } else {
                        if(y == 0 && x == 0)
                            new_d = 0;
                        else if(y == 0 || mask0)
                            new_d = x == 0 ? 0 : d_lft;
                        else if(x == 0 || mask1)
                            new_d = y == 0 ? 0 : d_top;
                        else
                            new_d = min(d_top, d_lft);

                    }

                    forward[x] = new_d;
                } else {
                    forward[x] = dptr[center];
                }

                result[x] = (abs((forward[x] - backward[x])>>3) > HOLEFILL_DIFF ? 0 : min(forward[x], backward[x]));
                dout[center] = result[x];// ? result[x] : (pREG->HOLEFILL_NO_ZERO ? 1 : 0);
            }

            memcpy(dptr + y*width, result, width*2);
        }

        delete [] forward;
        delete [] backward;
        delete [] result;

        return 0;
}
#endif //none __WEYE__
// for Gyro -

int EtronDI_SubSample(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char** SubSample, unsigned char* depthBuf, int bytesPerPixel,
                                  int width, int height, int& new_width, int& new_height, int mode, int factor )
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    *SubSample = g_pVideoDev[2*pDevSelInfo->index]->SubSample(depthBuf, bytesPerPixel, width, height, new_width, new_height, mode, factor );

    return ETronDI_OK;
}

int EtronDI_HoleFill(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* depthBuf, int bytesPerPixel, int kernel_size,
                                 int width, int height, int level, bool horizontal)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    g_pVideoDev[2*pDevSelInfo->index]->HoleFill( depthBuf, bytesPerPixel, kernel_size, width, height, level, horizontal );

    return ETronDI_OK;
}

int EtronDI_TemporalFilter(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    g_pVideoDev[2*pDevSelInfo->index]->TemporalFilter( depthBuf, bytesPerPixel, width, height, alpha, history );

    return ETronDI_OK;
}

int EtronDI_EdgePreServingFilter(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* depthBuf, int type,
                                             int width, int height, int level, float sigma, float lumda)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    g_pVideoDev[2*pDevSelInfo->index]->EdgePreServingFilter( depthBuf, type, width, height, level, sigma, lumda );

    return ETronDI_OK;
}

int EtronDI_ApplyFilters(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, unsigned char* depthBuf, unsigned char* subDisparity, int bytesPerPixel,
                                     int width, int height, int sub_w, int sub_h, int threshold)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    g_pVideoDev[2*pDevSelInfo->index]->ApplyFilters( depthBuf, subDisparity, bytesPerPixel, width, height, sub_w, sub_h, threshold );

    return ETronDI_OK;
}

int EtronDI_ResetFilters(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);
    g_pVideoDev[2*pDevSelInfo->index]->ResetFilters();

    return ETronDI_OK;
}

int EtronDI_EnableGPUAcceleration(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, bool enable)
{
    CEtronDI *pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI)
        return ETronDI_Init_Fail;

    if (g_pVideoDev[2 * pDevSelInfo->index] == NULL)
        g_pVideoDev[2 * pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    g_pVideoDev[2 * pDevSelInfo->index]->EnableGPUAcceleration(enable);

    return ETronDI_OK;
}

int EtronDI_TableToData(void *pHandleEtronDI, PDEVSELINFO pDevSelInfo, int width, int height, int TableSize, unsigned short* Table, unsigned short* Src, unsigned short* Dst)
{
    CEtronDI* pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;

    if(g_pVideoDev[2*pDevSelInfo->index] == NULL)
        g_pVideoDev[2*pDevSelInfo->index] = new CVideoDevice(pEtronDI->m_Adi.device_list[pDevSelInfo->index], g_bIsLogEnabled);

    return g_pVideoDev[2*pDevSelInfo->index]->TableToData( width, height, TableSize, Table, Src, Dst );
}

//+[Thermal device]
bool EtronDI_GetThermalFD(
    void *pHandleEtronDI,
    int *p_FD
   )
{
    CEtronDI *pEtronDI;
    bool nRet = false;

    if (imu_testing == true) return  ETronDI_DEVICE_BUSY;
    
    pEtronDI = (CEtronDI *)pHandleEtronDI;
    if (!pEtronDI) return ETronDI_Init_Fail;
    
    for(int i = 0; i < pEtronDI->m_Adi.nTotalDevNum; i++) {
        if(pEtronDI->m_Adi.grape_device[i] == true){
                 *p_FD = g_pVideoDev[2 * i]->m_fd;
                return true;
            }
     }
    return nRet;
}
//-[Thermal device]
