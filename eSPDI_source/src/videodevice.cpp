/*videodevice.cpp
  	\brief video render to get camera images by V4L2 and camera hardware access
  	Copyright:
	This file copyright (C) 2017 by

	eYs3D an Etron company

	An unpublished work.  All rights reserved.

	This file is proprietary information, and may not be disclosed or
	copied without the prior permission of eYs3D an Etron company.

	V4L2 reference link:
	https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec-single/v4l2.html
 */

#include "videodevice.h"

#include <sys/stat.h>

#include <algorithm>
#include <climits>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <locale>
#include <map>
#include <mutex>

#include "CComputerFactory.h"
#include "DataEncrypt.h"
#include "DepthmapFilter.h"
#include "En-Decrypt.h"
#include "Mp4FileUtility.h"
#include "debug.h"
#include "libeysov/fisheye360_api_cc.h"
#include "libeysov/fisheye360_def.h"
#include "math.h"
#include "turbojpeg.h"

using namespace std;

#if !defined(NDEBUG)
#define IOCTL_ERROR_LOG() \
    do{ \
    switch(errno){ \
        case EBADF: \
            LOGE("LINE[%d] FUNC[%s] MESSAGE[EBADF:%s]\n", __LINE__, __func__, strerror(errno)); break; \
        case EFAULT: \
            LOGE("LINE[%d] FUNC[%s] MESSAGE[EFAULT:%s]\n", __LINE__, __func__, strerror(errno)); break; \
        case EINVAL: \
            LOGE("LINE[%d] FUNC[%s] MESSAGE[EINVAL:%s]\n", __LINE__, __func__, strerror(errno)); break; \
        case ENOTTY: \
            LOGE("LINE[%d] FUNC[%s] MESSAGE[ENOTTY:%s]\n", __LINE__, __func__, strerror(errno)); break; \
        default: \
            LOGE("LINE[%d] FUNC[%s], ERRNO[%d], MESSAGE[%s]\n", __LINE__, __func__, errno, strerror(errno)); break; \
    } \
    }while (0) \

#else
#define IOCTL_ERROR_LOG()
#endif

#ifdef __i386
#define MY_INT long
#else
#define MY_INT int
#endif

extern bool g_bIsLogEnabled;
#define READ_STATUS  	0
#define WRITE_STATUS 	1
#define EXCPT_STATUS 	2
/*
 * s    - fd
 * sec  - timeout seconds
 * usec - timeout microseconds
 * x    - select status
 */
extern int x_select(int s, int sec, int usec, int x);

// data definition
//---------------------------------------------0--------------5-------------10-------------15-------------20-------------25
static short TAB_ElementCount[TAB_MAX_NUM] 	= {0, 5, 3, 2, 9, 8, 9, 8, 9, 3, 9, 9,12,12,16, 4, 4, 4, 4, 6, 4, 2, 4, 1, 9, 9,16,16, 1};
static char  TAB_DataSize[TAB_MAX_NUM]     	= {2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 4, 4, 2, 4, 4, 4, 4, 2};
static char  TAB_FractionalBit[TAB_MAX_NUM]	= {0, 0, 0, 0,18,18,18,18,23,23,23,23,13,13,18, 0, 0, 0, 0, 0, 0,23,23, 0,23,23,12, 3, 0};
//static char  TAB_Attri[TAB_MAX_NUM]		  	= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int v4l2_requestbuffers_cnt = 4;
extern bool bEX8029;

const unsigned char encryptToken = 0x39;

CFWMutexLocker::CFWMutexLocker(pthread_mutex_t *pMutex, FWFS_DATA *pFWData, CVideoDevice *pVideoDevice):
m_pMutex(pMutex),
m_pFWData(pFWData),
m_pVideoDevice(pVideoDevice),
m_bThreadLock(false),
m_bHasFWMutex(false)
{

    if (m_pMutex){
        pthread_mutex_lock(m_pMutex);
        m_bThreadLock = true;
    }

    if (m_pFWData){
        memset(m_pFWData, 0, sizeof(FWFS_DATA));
        m_pFWData->nType = FWFS_TYPE_GET_MUTEX;
        m_pVideoDevice->FWFS_Command(m_pFWData);
        m_bHasFWMutex = m_pFWData->nMutex > 0;
        m_nFWMutex = m_pFWData->nMutex;
    }
}

CFWMutexLocker::~CFWMutexLocker()
{
    Release();
}

void CFWMutexLocker::Release()
{
    if (m_bHasFWMutex){
        m_pFWData->nType = FWFS_TYPE_RELEASE_MUTEX;
        m_pFWData->nMutex = m_nFWMutex;
        m_pVideoDevice->FWFS_Command(m_pFWData);

        m_bHasFWMutex = false;
    }

    if (m_bThreadLock){
        pthread_mutex_unlock(m_pMutex);
        m_bThreadLock = false;
    }
}

CVideoDevice::CVideoDevice(char* szDevName, bool bIsLogEnabled)
:
m_depthFilter(new DepthmapFilter()),
m_pTjHandle(tjInitDecompress())
{
	cs_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex for FW access
	m_bIsLogEnabled = bIsLogEnabled;
	
    strcpy(m_szDevName, szDevName);
    if (g_bIsLogEnabled == true)
        LOGI("%s: %s\n", __func__, szDevName);
    
    m_waitingTime_sec = 0;
    m_waitingTime_usec = 0;
    m_b_blocking = false;
    buffers = NULL;
    n_buffers = 0;
	m_bHasStartCapture = false;

    m_RessolutionIndex = 0;
    m_nFrameCount = 0;
    m_index = -1;
    
    m_MinSecPerStep = 6;
    
    m_stn = ETRONDI_SENSOR_TYPE_H22;
    
    m_Dtc = DEPTH_IMG_NON_TRANSFER;

    m_nColorPaletteR_D8 = (unsigned char*)malloc(256);
    m_nColorPaletteG_D8 = (unsigned char*)malloc(256);
    m_nColorPaletteB_D8 = (unsigned char*)malloc(256);
    m_nColorPaletteR_D11 = (unsigned char*)malloc(2048);
    m_nColorPaletteG_D11 = (unsigned char*)malloc(2048);
    m_nColorPaletteB_D11 = (unsigned char*)malloc(2048);
    m_nColorPaletteR_Z14 = (unsigned char*)malloc(16384);
    m_nColorPaletteG_Z14 = (unsigned char*)malloc(16384);
    m_nColorPaletteB_Z14 = (unsigned char*)malloc(16384);
    SetBaseColorPalette(0);
    SetBaseColorPalette(2);
    SetBaseColorPalette(4);

    m_nGrayPaletteR_D8 = (unsigned char*)malloc(256);
    m_nGrayPaletteG_D8 = (unsigned char*)malloc(256);
    m_nGrayPaletteB_D8 = (unsigned char*)malloc(256);
    m_nGrayPaletteR_D11 = (unsigned char*)malloc(2048);
    m_nGrayPaletteG_D11 = (unsigned char*)malloc(2048);
    m_nGrayPaletteB_D11 = (unsigned char*)malloc(2048);
    m_nGrayPaletteR_Z14 = (unsigned char*)malloc(16384);
    m_nGrayPaletteG_Z14 = (unsigned char*)malloc(16384);
    m_nGrayPaletteB_Z14 = (unsigned char*)malloc(16384);
    SetBaseGrayPalette(0);
    SetBaseGrayPalette(2);
    SetBaseGrayPalette(4);

	m_pComputer.reset(CComputerFactory::GetInstance()->GenerateComputer());
	if (CComputer::OPENCL != m_pComputer->GetType()){
		m_depthFilter->EnableGPUAcceleration(false);
	}

	m_fd = open(m_szDevName, O_RDWR | O_NONBLOCK, 0);

    LOGD("Open device path : %s, result : %d\n", m_szDevName, m_fd);
}

CVideoDevice::~CVideoDevice() {

    CloseDevice();

    LOGD("Close device path : %s, result : %d\n", m_szDevName, m_fd);

    free(m_nColorPaletteR_D8);
    free(m_nColorPaletteG_D8);
    free(m_nColorPaletteB_D8);
    free(m_nColorPaletteR_D11);
    free(m_nColorPaletteG_D11);
    free(m_nColorPaletteB_D11);
    free(m_nColorPaletteR_Z14);
    free(m_nColorPaletteG_Z14);
    free(m_nColorPaletteB_Z14);

    free(m_nGrayPaletteR_D8);
    free(m_nGrayPaletteG_D8);
    free(m_nGrayPaletteB_D8);
    free(m_nGrayPaletteR_D11);
    free(m_nGrayPaletteG_D11);
    free(m_nGrayPaletteB_D11);
    free(m_nGrayPaletteR_Z14);
    free(m_nGrayPaletteG_Z14);
    free(m_nGrayPaletteB_Z14);

	close(m_fd);

	if (m_pTjHandle) tjDestroy(m_pTjHandle);
}

//Sometimes GetMute failed because not release properly. This will affect other ioctrl call.
//Call this function before calling other ioctrl function if necessary
//return the tried cycles.
int CVideoDevice:: WaitFwMuteReleased()
{
		pthread_mutex_lock(&cs_mutex);
		// Get Mutex
		memset(&m_FWFSData, 0, sizeof(m_FWFSData));
		m_FWFSData.nType = FWFS_TYPE_GET_MUTEX;
		int retry = 10;
		while(retry>0) {
			FWFS_Command(&m_FWFSData);
			if (m_FWFSData.nMutex > 0)
				break;
			retry --;
			usleep(10000);
		}
		if (retry < 10) {
//                        ALOGD("getMutex faile %d times!\n", 10-retry);
		}
		// Release Mutex
		if (m_FWFSData.nMutex != 0) {  
			m_FWFSData.nType = FWFS_TYPE_RELEASE_MUTEX;
			FWFS_Command(&m_FWFSData);
	
		}
		pthread_mutex_unlock(&cs_mutex);
		return (10-retry);		
}

void CVideoDevice:: SetBaseColorPalette(int nDepthDataType) {

    double R,G,B;
    int i;
    unsigned char *p0_R, *p0_G, *p0_B;

    if( nDepthDataType == ETronDI_DEPTH_DATA_DEFAULT )
    {
        p0_R= m_nColorPaletteR_D8;
        p0_G= m_nColorPaletteG_D8;
        p0_B= m_nColorPaletteB_D8;
            for (i=0; i<256; i++) {
                HSV_to_RGB(255.0-i,1.0,1.0,R,G,B);
                    *(p0_R + i) = (BYTE)R;
                    *(p0_G + i) = (BYTE)G;
                    *(p0_B + i) = (BYTE)B;
            }
    }

    if( nDepthDataType == ETronDI_DEPTH_DATA_11_BITS ) //11bits
    {
        p0_R= m_nColorPaletteR_D11;
        p0_G= m_nColorPaletteG_D11;
        p0_B= m_nColorPaletteB_D11;
        for (i=1; i<2047; i++) {
            HSV_to_RGB((2047.0-i)/8,1.0,1.0,R,G,B);
            *(p0_R + i) = (BYTE)R;
            *(p0_G + i) = (BYTE)G;
            *(p0_B + i) = (BYTE)B;
        }
        i = 0;
        *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)0;
        i = 2047;
        *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)255;

    }

    if( nDepthDataType == ETronDI_DEPTH_DATA_14_BITS ) //14bits
    {
        double fCV = 180;
        int nCenter=1500;
        double r1=0.35;
        double r2=0.55;
        double fx,fy;
        p0_R= m_nColorPaletteR_Z14;
        p0_G= m_nColorPaletteG_Z14;
        p0_B= m_nColorPaletteB_Z14;

        for (i=1; i<16384; i++) {
            if (i==nCenter) {
                fy = fCV;
            } else if (i<nCenter) {
                fx = (double)(nCenter-i)/nCenter;
                fy = fCV - pow(fx, r1)*fCV;
            } else {
                fx = (double)(i-nCenter)/(16384-nCenter);
                fy = fCV + pow(fx, r2)*(256-fCV);
            }
            HSV_to_RGB(fy,1.0,1.0,R,G,B);

            *(p0_R + i) = (BYTE)R; //*(p0 + i*3) = (BYTE)R;
            *(p0_G + i) = (BYTE)G; //*(p0 + i*3 +1) = (BYTE)G;
            *(p0_B + i) = (BYTE)B; //*(p0 + i*3 + 2) = (BYTE)B;
        }
            i = 0;
            *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)0;
            i = 16383;
            *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)255;
    }
}

void CVideoDevice:: SetBaseGrayPalette(int nDepthDataType) {

    double R,G,B;
    int i;
    unsigned char *p0_R, *p0_G, *p0_B;

    if( nDepthDataType == ETronDI_DEPTH_DATA_DEFAULT )
    {
        p0_R= m_nGrayPaletteR_D8;
        p0_G= m_nGrayPaletteG_D8;
        p0_B= m_nGrayPaletteB_D8;
            for (i=0; i<256; i++) {
                HSV_to_RGB(255.0-i,1.0,1.0,R,G,B);
                    *(p0_R + i) = (BYTE)B;
                    *(p0_G + i) = (BYTE)B;
                    *(p0_B + i) = (BYTE)B;
            }
    }

    if( nDepthDataType == ETronDI_DEPTH_DATA_11_BITS ) //11bits
    {
        p0_R= m_nGrayPaletteR_D11;
        p0_G= m_nGrayPaletteG_D11;
        p0_B= m_nGrayPaletteB_D11;
        for (i=1; i<2047; i++) {
            HSV_to_RGB((2047.0-i)/8,1.0,1.0,R,G,B);
            *(p0_R + i) = (BYTE)B;
            *(p0_G + i) = (BYTE)B;
            *(p0_B + i) = (BYTE)B;
        }
        i = 0;
        *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)0;
        i = 2047;
        *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)255;

    }

    if( nDepthDataType == ETronDI_DEPTH_DATA_14_BITS ) //14bits
    {
        double fCV = 180;
        int nCenter=1500;
        double r1=0.35;
        double r2=0.55;
        double fx,fy;
        p0_R= m_nGrayPaletteR_Z14;
        p0_G= m_nGrayPaletteG_Z14;
        p0_B= m_nGrayPaletteB_Z14;

        for (i=1; i<16384; i++) {
            if (i==nCenter) {
                fy = fCV;
            } else if (i<nCenter) {
                fx = (double)(nCenter-i)/nCenter;
                fy = fCV - pow(fx, r1)*fCV;
            } else {
                fx = (double)(i-nCenter)/(16384-nCenter);
                fy = fCV + pow(fx, r2)*(256-fCV);
            }
            HSV_to_RGB(fy,1.0,1.0,R,G,B);

            *(p0_R + i) = (BYTE)B; //*(p0 + i*3) = (BYTE)R;
            *(p0_G + i) = (BYTE)B; //*(p0 + i*3 +1) = (BYTE)G;
            *(p0_B + i) = (BYTE)B; //*(p0 + i*3 + 2) = (BYTE)B;
        }
            i = 0;
            *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)0;
            i = 16383;
            *(p0_R + i) = *(p0_G + i) = *(p0_B + i) = (BYTE)255;
    }
}

int CVideoDevice::convert_depth_y_to_buffer(unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height, bool color, unsigned short nDepthDataType) {

    int cx, cy, v, x, y;
    unsigned char *p0,*p1;
    unsigned char *pR,*pG, *pB;

    //
    if((m_devType == AXES1 && nDepthDataType < 2) && color == false) cx = width;
    else cx = width*2;

    cy = height;
    p0 = depth_y;
    p1 = rgb;

    if( nDepthDataType == ETronDI_DEPTH_DATA_11_BITS ||
            nDepthDataType == ETronDI_DEPTH_DATA_11_BITS_RAW){
        if(color){
            pR = m_nColorPaletteR_D11;
            pG = m_nColorPaletteG_D11;
            pB = m_nColorPaletteB_D11;
        }
        else{
            pR = m_nGrayPaletteR_D11;
            pG = m_nGrayPaletteG_D11;
            pB = m_nGrayPaletteB_D11;
          }
    }
    if( nDepthDataType == ETronDI_DEPTH_DATA_14_BITS ){
        if(color){
            pR = m_nColorPaletteR_Z14;
            pG = m_nColorPaletteG_Z14;
            pB = m_nColorPaletteB_Z14;
        }
        else{
            pR = m_nGrayPaletteR_Z14;
            pG = m_nGrayPaletteG_Z14;
            pB = m_nGrayPaletteB_Z14;
       }
    }

    if( nDepthDataType == ETronDI_DEPTH_DATA_11_BITS ||
            nDepthDataType == ETronDI_DEPTH_DATA_11_BITS_RAW ||
            nDepthDataType == ETronDI_DEPTH_DATA_14_BITS) //11bits&14bits
    {
        cx = width;
        for (y=0; y<cy; y++) {
            for (x=0; x<cx; x++) {
                v = *(p0+1) << 8 | *p0;
                //
                p1[0] = *(pR + v);
                p1[1] = *(pG + v);
                p1[2] = *(pB + v);
                //
                p0 += 2;
                p1 += 3;
            }
        }
    }
    else {
        for (y=0; y<cy; y++) {
            for (x=0; x<cx; x++) {
                v = *p0;
                //
                if(color){
                    p1[0] = *(m_nColorPaletteR_D8 + v);
                    p1[1] = *(m_nColorPaletteG_D8 + v);
                    p1[2] = *(m_nColorPaletteB_D8 + v);
                }
                else{
                    if (m_devType != AXES1){
                        p1[0] = *(m_nGrayPaletteR_D8 + v);
                        p1[1] = *(m_nGrayPaletteR_D8 + v);
                        p1[2] = *(m_nGrayPaletteR_D8 + v);
                    }
                    else{
                         p1[0] = v;
                         p1[1] = v;
                         p1[2] = v;
                    }
                }

                //
                p0 ++;
                p1 += 3;
            }
        }
    }

    //
    return ETronDI_OK;
}


int CVideoDevice::convert_depth_y_to_buffer_offset(unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height, bool color, unsigned short nDepthDataType, int offset) {

    int cx, cy, v, x, y;
    unsigned char *p0,*p1;
    unsigned char *pR,*pG, *pB;

    //
    if((m_devType == AXES1 || nDepthDataType == ETronDI_DEPTH_DATA_8_BITS)
            && color == false) cx = width;
    else cx = width*2;

    cy = height;
    p0 = depth_y;
    p1 = rgb;

    if( nDepthDataType == ETronDI_DEPTH_DATA_11_BITS ){
        if(color){
            pR = m_nColorPaletteR_D11;
            pG = m_nColorPaletteG_D11;
            pB = m_nColorPaletteB_D11;
        }
        else{
            pR = m_nGrayPaletteR_D11;
            pG = m_nGrayPaletteG_D11;
            pB = m_nGrayPaletteB_D11;
          }
    }
    if( nDepthDataType == ETronDI_DEPTH_DATA_14_BITS ){
        if(color){
            pR = m_nColorPaletteR_Z14;
            pG = m_nColorPaletteG_Z14;
            pB = m_nColorPaletteB_Z14;
        }
        else{
            pR = m_nGrayPaletteR_Z14;
            pG = m_nGrayPaletteG_Z14;
            pB = m_nGrayPaletteB_Z14;
       }
    }

    if( nDepthDataType == ETronDI_DEPTH_DATA_11_BITS || nDepthDataType == ETronDI_DEPTH_DATA_14_BITS) //11bits&14bits
    {
        cx = width;
        for (y=0; y<cy; y++) {
            for (x=0; x<cx; x++) {
                v = *(p0 + offset + 1) << 8 | *(p0 + offset);
                //
                p1[0] = *(pR + v);
                p1[1] = *(pG + v);
                p1[2] = *(pB + v);
                //
                p0 += 2;
                p1 += 3;
            }
            p0 += offset;
        }
    } else {
        for (y=0; y<cy; y++) {
            for (x=0; x<cx; x++) {
                v = *(p0 + offset);
                //
                if(color){
                    p1[0] = *(m_nColorPaletteR_D8 + v);
                    p1[1] = *(m_nColorPaletteG_D8 + v);
                    p1[2] = *(m_nColorPaletteB_D8 + v);
                }
                else{
                    if (m_devType != AXES1){
                        p1[0] = *(m_nGrayPaletteR_D8 + v);
                        p1[1] = *(m_nGrayPaletteR_D8 + v);
                        p1[2] = *(m_nGrayPaletteR_D8 + v);
                    }
                    else{
                         p1[0] = v;
                         p1[1] = v;
                         p1[2] = v;
                    }
                }

                //
                p0 += 1;
                p1 += 3;
            }
            p0 += offset;
        }
    }

    //
    return ETronDI_OK;
}


#if 0
int CVideoDevice::convert_depth_y_to_gray_rgb_buffer(unsigned char *depth_y, unsigned char *gray_rgb, unsigned int width, unsigned int height) {

    unsigned int i = 0;
    for(i=0;i<width*height;i++) {
        gray_rgb[i*3+0] = depth_y[i];
        gray_rgb[i*3+1] = depth_y[i];
        gray_rgb[i*3+2] = depth_y[i];
    }

    return ETronDI_OK;
}
#endif
int CVideoDevice::GetFWRegister( unsigned short address, unsigned short* pValue, int flag) {
	
	return GetPropertyValue( address, pValue, T_FIRMWARE, flag);
}

int CVideoDevice::SetFWRegister( unsigned short address, unsigned short nValue, int flag) {
	
	return SetPropertyValue( address, nValue, T_FIRMWARE, flag);
}

int CVideoDevice::GetHWRegister( unsigned short address, unsigned short* pValue, int flag) {
	
	return GetPropertyValue( address, pValue, T_ASIC, flag);
}

int CVideoDevice::SetHWRegister( unsigned short address, unsigned short nValue, int flag) {
	
	return SetPropertyValue( address, nValue, T_ASIC, flag);
}

int CVideoDevice::GetSensorRegister( int nId, unsigned short address, unsigned short* pValue, int flag, int nSensorMode) {
	
	return GetPropertyValue_I2C( nId, address, pValue, T_I2C, flag, nSensorMode);
}

int CVideoDevice::SetSensorRegister( int nId, unsigned short address, unsigned short nValue, int flag, int nSensorMode) {
	
	return SetPropertyValue_I2C( nId, address, nValue, T_I2C, flag, nSensorMode);
}

// File ID +

int CVideoDevice::GetFwVersion( char *pszFwVersion, int nBufferSize, int *pActualLength) {
	
	// get new FW Version +
	
	char szBuf[32] = {0};
	char szBuf2[128] = {0};
	
	bool bIsNewVersionExist = false;
	
	int nOldVersionLength = 0;
	int nNewversionLength = 0;

	unsigned short nValue1 = 0;
	unsigned short nValue2 = 0;
	unsigned short nValue3 = 0;
	
	GetFWRegister( 0x0, &nValue1, FG_Address_1Byte | FG_Value_1Byte);
	GetFWRegister( 0x1, &nValue2, FG_Address_1Byte | FG_Value_1Byte);
	GetFWRegister( 0x3, &nValue3, FG_Address_1Byte | FG_Value_1Byte);
	
	sprintf(szBuf, "%d.%d.%d", nValue1, nValue2, nValue3);
	
	nOldVersionLength = strlen(szBuf);
	
	if( nBufferSize < (nOldVersionLength+1) ) {
        if(m_bIsLogEnabled) LOGE("CEtronDI::GetFwVersion() : Error Buffer Length.\n");
		return ETronDI_ErrBufLen; 
	}
	
	unsigned char buffer[1024] = {0};
	int nStartAddrOfStrPrjVer = 59;
	int i = 0;
	
	if(FWV3ReadData(buffer, 1, 1024) == 0) {		
		
		while(i<6) {
			nStartAddrOfStrPrjVer += buffer[nStartAddrOfStrPrjVer];
			i++;
		}
		
		nNewversionLength = (buffer[nStartAddrOfStrPrjVer]-2)/2;		
        LOGD("nStrlen = %d\n", nNewversionLength);
		
		if(nNewversionLength > 0) {
			
			*pActualLength = nOldVersionLength+nNewversionLength+2;
		
			if( nBufferSize < *pActualLength ) {
				
                if(m_bIsLogEnabled) LOGE("CEtronDI::GetFwVersion() : Error Buffer Length.\n");
				return ETronDI_ErrBufLen;
			}
			else {
				
				wchar_t *pStrPrjVer = (wchar_t*)malloc(nNewversionLength*sizeof(wchar_t)+1);
			
				for(i=0;i<nNewversionLength;i++) {
					pStrPrjVer[i] = buffer[nStartAddrOfStrPrjVer+2+2*i]+buffer[nStartAddrOfStrPrjVer+2+2*i+1]*256;
				}
				pStrPrjVer[nNewversionLength] = L'\0';
                LOGD("New Version = %S\n", pStrPrjVer);
				
				bIsNewVersionExist = (buffer[54] > 5);
				
				// workaround. calling wcstombs may failed on some arm system, 
				// and chars in fw are all in ascii.
				for (int j = 0; j < nNewversionLength; ++j)
				{
					szBuf2[j] = (((char*)&pStrPrjVer[j])[0] != '\0' 
						? ((char*)&pStrPrjVer[j])[0] : ((char*)&pStrPrjVer[j])[1]);
				}
				szBuf2[nNewversionLength] = '\0';
				
				free(pStrPrjVer);
				pStrPrjVer = NULL;
			}
		}
	}
	
	if(bIsNewVersionExist) {
		//sprintf(pszFwVersion, "%s(%s)", szBuf2, szBuf);
		sprintf(pszFwVersion, "%s", szBuf2);
		*pActualLength = nNewversionLength;
        LOGD("New Version = %s\n", pszFwVersion);
	} 
	else {
		memcpy( pszFwVersion, szBuf, (nOldVersionLength+1)*sizeof(char));
		*pActualLength = nOldVersionLength;
	}
	
	// get new FW Version - 
	
	return ETronDI_OK;
}

int CVideoDevice::GetPidVid( unsigned short *pPidBuf, unsigned short *pVidBuf ) {
    std::string name = GetDeviceName();
	std::string modalias;
	unsigned short mi;
    if (!(std::ifstream("/sys/class/video4linux/" + name +
                        "/device/modalias") >>
          modalias))
        return ETronDI_READ_REG_FAIL;
    if (modalias.size() < 14 || modalias.substr(0, 5) != "usb:v" ||
        modalias[9] != 'p')
        return ETronDI_READ_REG_FAIL;
    if (!(std::istringstream(modalias.substr(5, 4)) >> std::hex >> *pVidBuf))
        return ETronDI_READ_REG_FAIL;
    if (!(std::istringstream(modalias.substr(10, 4)) >> std::hex >> *pPidBuf))
        return ETronDI_READ_REG_FAIL;
    if (!(std::ifstream("/sys/class/video4linux/" + name +
                        "/device/bInterfaceNumber") >>
          std::hex >> mi))
        LOGD("Failed to read interface number");
	return ETronDI_OK;
}

int CVideoDevice::SetPidVid( unsigned short *pPidBuf, unsigned short *pVidBuf) {
	
	unsigned char buffer[1024];
	
	bool bIsNewVersionPlugIn = true; 
	if(!IsPlugInVersionNew(&bIsNewVersionPlugIn)) return ETronDI_NotSupport;
	
	if(bIsNewVersionPlugIn) {
		if( FWV3ReadData( buffer, 1, 1024) != ETronDI_OK) {
            if(m_bIsLogEnabled) LOGE("CEtronDI::SetPidVid() : Read flash failed. (1)\n");
			return ETronDI_READFLASHFAIL; 
		}
		
		buffer[1] = (unsigned short)(*pVidBuf/256);
		buffer[0] = (unsigned short)(*pVidBuf%256);
		buffer[3] = (unsigned short)(*pPidBuf/256);
		buffer[2] = (unsigned short)(*pPidBuf%256);
		
		if( FWV3WriteData( buffer, 1, 1024) != ETronDI_OK) {
            if(m_bIsLogEnabled) LOGE("CEtronDI::SetPidVid() : Write flash failed. (1)\n");
			return ETronDI_WRITEFLASHFAIL; 
		}
	}
	else {
		if( FWV3ReadData( buffer, 0, 1024) != ETronDI_OK) {
            if(m_bIsLogEnabled) LOGE("CEtronDI::SetPidVid() : Read flash failed. (2)\n");
			return ETronDI_READFLASHFAIL; 
		}
		
		buffer[4] = (unsigned short)(*pVidBuf/256);
		buffer[3] = (unsigned short)(*pVidBuf%256);
		buffer[6] = (unsigned short)(*pPidBuf/256);
		buffer[5] = (unsigned short)(*pPidBuf%256);
		
		if( FWV3WriteData( buffer, 0, 1024) != ETronDI_OK) {
            if(m_bIsLogEnabled) LOGE("CEtronDI::SetPidVid() : Write flash failed. (1)\n");
			return ETronDI_WRITEFLASHFAIL; 
		}
	}
	
	return ETronDI_OK;
}

int CVideoDevice::GetSerialNumber(unsigned char* pData, int nBufferSize, int *pLen) {   
	
	std::string usb_port_path = GetDeviceUSBInfoPath();
	if (!usb_port_path.empty()) {
		std::string serial;
		if ((std::ifstream(usb_port_path + "/serial") >> serial)){
			std::u16string serial_utf16 = std::wstring_convert< std::codecvt_utf8_utf16<char16_t>, char16_t >{}.from_bytes(serial.c_str());
			
			int size = serial_utf16.size() * sizeof(std::u16string::value_type);
			size = min(size, nBufferSize - 1);

			*pLen = size;

			memcpy(pData, serial_utf16.c_str(), size);
			pData[size] = 0;

			return ETronDI_OK;
		}
	}
	
	unsigned char buffer[1024] = {0};
    int nStartIndex = 0;
    int temp1 = 0;
    int temp2 = 0;
	int i = 0;
	
	if(pData==NULL || pLen==NULL) return ETronDI_NullPtr;
	
	bool bIsNewVersionPlugIn = true; 
	if(!IsPlugInVersionNew(&bIsNewVersionPlugIn)) return ETronDI_NotSupport;
	
    if(bIsNewVersionPlugIn) {  		
		if(FWV3ReadData(buffer, 1, 1024) == ETronDI_OK) {
			temp1 = buffer[VARIABLE_DATA_START_ADDR];
			temp2 = buffer[VARIABLE_DATA_START_ADDR+temp1];
			*pLen  = buffer[VARIABLE_DATA_START_ADDR+temp1+temp2]-2;
			
			//if(nBufferSize < *pLen) return ETronDI_ErrBufLen;
			//if buffersize too small, copy only buffersize-1
			if (nBufferSize <= *pLen+1) {
				*pLen = nBufferSize -1;
				pData[nBufferSize-1] = 0;			
			}
			nStartIndex = VARIABLE_DATA_START_ADDR+temp1+temp2+2;
			
			for(i=0;i<*pLen;i++) {
				pData[i] = buffer[i+nStartIndex];
			}
		}else{
			return ETronDI_READ_REG_FAIL;
		}
	}
    else {	
		if(FWV3ReadData(buffer, 0, 512) == ETronDI_OK) {
			*pLen = (buffer[SERIAL_NUMBER_START_OFFSET]-2);
			
			//if(nBufferSize < *pLen) return ETronDI_ErrBufLen;
			//if buffersize too small, copy only buffersize-1
			if (nBufferSize <= *pLen+1) {
				*pLen = nBufferSize -1;
				pData[nBufferSize-1] = 0;			
			}
			
			for(i=0;i<*pLen;i++) {
			    pData[i] = buffer[SERIAL_NUMBER_START_OFFSET+2+i];
			}
	    }else{
			return ETronDI_READ_REG_FAIL;
		}
	}
    return ETronDI_OK;
}

int CVideoDevice::SetSerialNumber(unsigned char* pData, int nLen) {	
	
	unsigned char  buffer[1024] = {0};
	unsigned char* pbuffer2 = NULL;
    int nStartIndex = 0;
    int temp1 = 0;
    int temp2 = 0;
	int i = 0;
	
	int nDataSizeAfterSN = 0;
	int nOriginalLength = 0;
	int nNewLength = 0;
	
	if(pData==NULL) return ETronDI_NullPtr;
	
	bool bIsNewVersionPlugIn = true; 
	if(!IsPlugInVersionNew(&bIsNewVersionPlugIn)) return ETronDI_NotSupport;
	
	if(bIsNewVersionPlugIn)
	{
		if(nLen > 252) return ETronDI_ErrBufLen;
		
		nNewLength = nLen+2;
		  
		if(FWV3ReadData(buffer,1,1024) == 0) {
			temp1 = buffer[VARIABLE_DATA_START_ADDR];
			temp2 = buffer[VARIABLE_DATA_START_ADDR+temp1];

			nStartIndex = VARIABLE_DATA_START_ADDR+temp1+temp2;
			nOriginalLength = buffer[nStartIndex];
			i = nStartIndex + nOriginalLength;
			
			// Get the number of data after serial number
			while(buffer[i]!=0) {
				i += buffer[i];
			}
			
			nDataSizeAfterSN = i - (nStartIndex + nOriginalLength);
			
			if( nStartIndex+nNewLength+nDataSizeAfterSN > 1024) return ETronDI_ErrBufLen;
			
			pbuffer2 = (unsigned char*)malloc(nDataSizeAfterSN);
			memcpy(pbuffer2, buffer+nStartIndex+nOriginalLength, nDataSizeAfterSN);
			
			// Clear to 0 after serial number
			for(i=nStartIndex;i<1024;i++) {
				buffer[i] = 0;
			}
			
			// Write Serial number 
			buffer[nStartIndex] = nNewLength;
			buffer[nStartIndex+1] = 0x03;
			
			for(i=0;i<nLen;i++) {
				buffer[nStartIndex+2+i] = pData[i];
			}
			memcpy(buffer+nStartIndex+nNewLength,pbuffer2,nDataSizeAfterSN);
			
			free(pbuffer2);
			FWV3WriteData(buffer,1,1024); 
		}
	}
	else
	{
		if(nLen > 32) return ETronDI_ErrBufLen;
		
		if(FWV3ReadData(buffer,0,512) == 0) {
			buffer[SERIAL_NUMBER_START_OFFSET] = nLen+2;
		    buffer[SERIAL_NUMBER_START_OFFSET+1] = 0x03;
		    
			for(i=0;i<nLen;i++) {
				buffer[SERIAL_NUMBER_START_OFFSET+2+i] = pData[i];
			}
			FWV3WriteData(buffer,0,512);
		}
	}
    
	return ETronDI_OK;
}

int CVideoDevice::GetYOffset( BYTE *buffer, int BufferLength, int *pActualLength, int index) {
	
	if(!buffer) return ETronDI_NullPtr;
	if(index!=0) return ETronDI_NotSupport;

	*pActualLength = 256;

	if( BufferLength != 256 ) {
		
		return ETronDI_ErrBufLen;
	}
	
	if( FWV3ReadData( buffer, 30+index, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::GetYOffset() : Read flash failed.\n");
		return ETronDI_READFLASHFAIL; 
	}
	
	return ETronDI_OK;
}

int CVideoDevice::SetYOffset( BYTE *buffer, int BufferLength, int *pActualLength, int index) {
	
	if(!buffer) return ETronDI_NullPtr;
	if(index!=0) return ETronDI_NotSupport;

	*pActualLength = 256;

	if( BufferLength != 256 ) {
		
		return ETronDI_ErrBufLen;
	}
	
	if( FWV3WriteData( buffer, 30+index, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::SetYOffset() : Write flash failed.\n");
		return ETronDI_WRITEFLASHFAIL; 
	}
	
	return ETronDI_OK;
}

int CVideoDevice::GetRectifyTable( BYTE *buffer, int BufferLength, int *pActualLength, int index) {
	
	if(!buffer) return ETronDI_NullPtr;
	if( (index < 0) || ( index > 9) ) return ETronDI_NotSupport;
	
	*pActualLength = 1024;

	if( BufferLength != 1024 ) {

		return ETronDI_ErrBufLen;
	}
	
	if( FWV3ReadData( buffer, 40+index, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::GetRectifyTable() : Read flash failed.\n");
		return ETronDI_READFLASHFAIL; 
	}
	
	return ETronDI_OK;
}

int CVideoDevice::SetRectifyTable( BYTE *buffer, int BufferLength, int *pActualLength, int index) {
	
	if(!buffer) return ETronDI_NullPtr;
	if( (index < 0) || ( index > 9) ) return ETronDI_NotSupport;

	*pActualLength = 1024;

	if( BufferLength != 1024 ) {
				
		return ETronDI_ErrBufLen;
	}
	
	if( FWV3WriteData( buffer, 40+index, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::SetRectifyTable() : Write flash failed.\n");
		return ETronDI_WRITEFLASHFAIL; 
	}
	
	return ETronDI_OK;
}

int CVideoDevice::GetZDTable( BYTE *buffer, int BufferLength, int *pActualLength, PZDTABLEINFO pZDTableInfo) {
	
     if(!buffer) return ETronDI_NullPtr;
	
    if( FWV3ReadData( buffer, ETronDI_ZD_TABLE_FILE_ID_0+pZDTableInfo->nIndex, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::GetZDTable() : Read flash failed.\n");
		return ETronDI_READFLASHFAIL; 
	}

    *pActualLength = BufferLength;
	
	return ETronDI_OK;
}

int CVideoDevice::SetZDTable( BYTE *buffer, int BufferLength, int *pActualLength, PZDTABLEINFO pZDTableInfo) {
	
	if(!buffer) return ETronDI_NullPtr;

    if( FWV3WriteData( buffer, 50+pZDTableInfo->nIndex, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::SetZDTable() : Write flash failed.\n");
		return ETronDI_WRITEFLASHFAIL; 
	}
    *pActualLength = BufferLength;
	
	return ETronDI_OK;
}

int CVideoDevice::GetLogData( BYTE *buffer, int BufferLength, int *pActualLength, int index, CALIBRATION_LOG_TYPE type) {
	
	if(!buffer) return ETronDI_NullPtr;
	if( (index < 0) || ( index > 9) ) return ETronDI_NotSupport;

	*pActualLength = 4096;
	
	unsigned char *pOutputBuffer = (unsigned char*)malloc(*pActualLength*sizeof(unsigned char));

	if( BufferLength != 4096 ) {

		return ETronDI_ErrBufLen;
	}
	
	if( FWV3ReadData( buffer, 240+index, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::GetLogData() : Read flash failed.\n");
		return ETronDI_READFLASHFAIL; 
	}
	
	if(type == SERIAL_NUMBER) {
		
		if(GetLogBufferByTag(SN_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == PRJFILE_LOG) {
		
		if(GetLogBufferByTag(PRJ_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == STAGE_TIME_RESULT_LOG) {
		
		if(GetLogBufferByTag(STAGE_TIME_RESULT_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == SENSOR_OFFSET) {
		
		if(GetLogBufferByTag(YOFFSET_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == AUTO_ADJUST_LOG) {
		
		if(GetLogBufferByTag(AUTOADJUSTMENT_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == RECTIFY_LOG) {
		
		if(GetLogBufferByTag(SMARTK_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == ZD_LOG) {
		
		if(GetLogBufferByTag(SMARTZD_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	if(type == DEPTHMAP_KOG) {
		
		if(GetLogBufferByTag(DQ_ERROR_ID, buffer, pOutputBuffer, pActualLength)) {
			
			memset(buffer, 0, sizeof(unsigned char));
			memcpy(buffer, pOutputBuffer, *pActualLength);
		}
		else return ETronDI_GET_CALIBRATIONLOG_FAIL;
	}
	
	free(pOutputBuffer);
	pOutputBuffer = NULL;
	
	return ETronDI_OK;
}

//Mathmatic Method
int CVideoDevice::GetRecifyMatLogData(eSPCtrl_RectLogData *pData, int index) {

    unsigned char nData[4096 + 0x2c1];

    int nLen;
    short tempBufferShort[8];
    int  tempBufferLong[16];
    short idx = 0;
    char *DumpBuffer;


    if(!pData) return ETronDI_NullPtr;
    if( (index < 0) || ( index > 9) ) return ETronDI_NotSupport;

    nLen = 4096;
    if (FWV3ReadData(nData, 240+index, nLen) != ETronDI_OK) {

        if (m_bIsLogEnabled) LOGE("CEtronDI::GetLogData() : Read flash failed.\n");
        return ETronDI_READFLASHFAIL;
    }

    if (bEX8029 == true) {
        DumpBuffer = (char *)&nData[0x2c1];
    } else {
        DumpBuffer = (char *)&nData[0];
        DumpBuffer += 2;
        DumpBuffer += (TableHeaderSize + TableInfoSize * TAB_MAX_NUM);
    }


    idx++;
    //Table ID 0: Write Log Szie
    memcpy(tempBufferShort, DumpBuffer, sizeof(short) * 1);
    //Table ID 1: Write "InOutDim" Entries
    memcpy(tempBufferShort, DumpBuffer, sizeof(short)*TAB_ElementCount[idx]);
    pData->InImgWidth = (int)tempBufferShort[0];		// 2 bytes
    pData->InImgHeight = (int)tempBufferShort[1];		// 2 bytes
    pData->OutImgWidth = (int)tempBufferShort[2];		// 2 bytes
    pData->OutImgHeight = (int)tempBufferShort[3];		// 2 bytes
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 2: Write "RECT_Flags" Entries
    memcpy(tempBufferShort, DumpBuffer, sizeof(short)*TAB_ElementCount[idx]);
    pData->RECT_ScaleEnable = (bool)tempBufferShort[1];	// 2 bytes
    pData->RECT_CropEnable = (bool)tempBufferShort[2];	// 2 bytes
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 3: Write "RECT_ScaleDimension" Entries
    memcpy(tempBufferShort, DumpBuffer, sizeof(short)*TAB_ElementCount[idx]);
    pData->RECT_ScaleWidth = (int)tempBufferShort[0];		// 2 bytes
    pData->RECT_ScaleHeight = (int)tempBufferShort[1];		// 2 bytes
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 4: Write "CamMat1" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->CamMat1[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 5: Write "CamDist1" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->CamDist1[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 6: Write "CamMat2" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->CamMat2[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 7: Write "CamDist2" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->CamDist2[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 8: Write "RotaMat" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->RotaMat[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 9: Write "TranMat" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->TranMat[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 10: Write "LRotaMat" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->LRotaMat[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 11: Write "RRotaMat" Entries				// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->RRotaMat[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 12: Write "NewCamMat1" Entries			// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->NewCamMat1[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 13: Write "NewCamMat2" Entries			// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->NewCamMat2[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 14: Write "ReprojMat" Entries			// 4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    for (int i = 0; i<TAB_ElementCount[idx]; i++)
        pData->ReProjectMat[i] = (float)((double)(tempBufferLong[i])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 15: Write "ROI1" Entries
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 16: Write "ROI2" Entries
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 17: Write "ComROI" Entries
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 18: Write "ScaleROI" Entries
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 19: Write "RECT_Crop" Entries
    memcpy(tempBufferShort, DumpBuffer, sizeof(short)*TAB_ElementCount[idx]);
    pData->RECT_Crop_Row_BG = (int)tempBufferShort[0];		// 2 bytes
    pData->RECT_Crop_Row_ED = (int)tempBufferShort[1];		// 2 bytes
    pData->RECT_Crop_Col_BG_L = (int)tempBufferShort[2];		// 2 bytes
    pData->RECT_Crop_Col_ED_L = (int)tempBufferShort[3];		// 2 bytes
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 20: Write "RECT_Scale_MN" Entries
    memcpy(tempBufferShort, DumpBuffer, sizeof(short)*TAB_ElementCount[idx]);
    pData->RECT_Scale_Col_M = (int)tempBufferShort[0];		// 2 bytes
    pData->RECT_Scale_Col_N = (int)tempBufferShort[1];		// 2 bytes
    pData->RECT_Scale_Row_M = (int)tempBufferShort[2];		// 2 bytes
    pData->RECT_Scale_Row_N = (int)tempBufferShort[3];		// 2 bytes
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 21: Write "RECT_Scale_C" Entries
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 22: Write "RECT_ErrMeasure" Entries		4 bytes
    memcpy(tempBufferLong, DumpBuffer, sizeof(int)*TAB_ElementCount[idx]);
    pData->RECT_AvgErr = (float)((double)(tempBufferLong[0])) / (1 << TAB_FractionalBit[idx]);
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    //Table ID 23: Write "LineBufer" Entries
    memcpy(tempBufferShort, DumpBuffer, sizeof(short)*TAB_ElementCount[idx]);
    pData->nLineBuffers = (int)tempBufferShort[0];		// 2 bytes
    DumpBuffer += (TAB_DataSize[idx] * TAB_ElementCount[idx]);
    idx++;

    return ETronDI_OK;
}


int CVideoDevice::SetLogData( BYTE *buffer, int BufferLength, int *pActualLength, int index, CALIBRATION_LOG_TYPE type) {
	
	if(!buffer) return ETronDI_NullPtr;
	if( (index < 0) || ( index > 9) ) return ETronDI_NotSupport;

	*pActualLength = 4096;

	if( BufferLength != 4096 ) {
		
		return ETronDI_ErrBufLen;
	}
	
	if( FWV3WriteData( buffer, 240+index, BufferLength) != ETronDI_OK) {
		
        if(m_bIsLogEnabled) LOGE("CEtronDI::SetLogData() : Write flash failed.\n");
		return ETronDI_WRITEFLASHFAIL; 
	}
	
	return ETronDI_OK;
}

int CVideoDevice::GetUserData( BYTE *buffer, int BufferLength, USERDATA_SECTION_INDEX usi) {

	if(!buffer) return ETronDI_NullPtr;

	if( usi == USERDATA_SECTION_0) {
		
		if( BufferLength<1 || BufferLength>512 ) return ETronDI_ErrBufLen;
		
		BYTE buf[512];		
		if( FWV3ReadData( buf, 200+usi, 512) != ETronDI_OK) {
			
            if(m_bIsLogEnabled) LOGE("CEtronDI::GetUserData() : Read flash failed 0.\n");
			return ETronDI_READFLASHFAIL;
		} 
		
		memcpy(buffer, buf, BufferLength); 
	}
	else if( usi == USERDATA_SECTION_1) {
		
		if( BufferLength<1 || BufferLength>4096 ) return ETronDI_ErrBufLen;
		
		BYTE buf[4096];
		if( FWV3ReadData( buf, 200+usi, 4096) != ETronDI_OK) {
			
            if(m_bIsLogEnabled) LOGE("CEtronDI::GetUserData() : Read flash failed 1.\n");
			return ETronDI_READFLASHFAIL;  
		}
		
		memcpy(buffer, buf, BufferLength);
	}
	else return ETronDI_RET_BAD_PARAM;

	return ETronDI_OK;
}

int CVideoDevice::SetUserData( BYTE *buffer, int BufferLength, USERDATA_SECTION_INDEX usi) {

	if(!buffer) return ETronDI_NullPtr;

	if( usi == USERDATA_SECTION_0) {
		
		if( BufferLength<1 || BufferLength>512 ) return ETronDI_ErrBufLen;
		
		BYTE buf[512] = {0};
		memcpy(buf, buffer, BufferLength);
		if( FWV3WriteData( buf, 200+usi, 512) != ETronDI_OK) {
			
            if(m_bIsLogEnabled) LOGE("CEtronDI::SetUserData() : Write flash failed.\n");
			return ETronDI_WRITEFLASHFAIL;
		}  
	}
	else if( usi == USERDATA_SECTION_1) {
		
		if( BufferLength<1 || BufferLength>4096 ) return ETronDI_ErrBufLen;
		
		BYTE buf[4096] = {0};
		memcpy(buf, buffer, BufferLength);
		if( FWV3WriteData( buf, 200+usi, 4096) != ETronDI_OK) {
            if(m_bIsLogEnabled) LOGE("CEtronDI::SetUserData() : Write flash failed.\n");
			return ETronDI_WRITEFLASHFAIL;  
		}
	}
	else return ETronDI_RET_BAD_PARAM;

	return ETronDI_OK;
}

// for calibration log +

int CVideoDevice::GetSerialNumberFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

int CVideoDevice::GetProjectFileFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

int CVideoDevice::GetSTRLogFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

int CVideoDevice::GetSensorOffsetFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

int CVideoDevice::GetAutoAdjustLogFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

int CVideoDevice::GetRectifyLogFromCalibrationLog(eSPCtrl_RectLogData *pData, int index) {

	std::vector<unsigned char> vecDumpBuffer(4096, 0);
	unsigned char *DumpBuffer = vecDumpBuffer.data();
	int BufferLength = 4096;
	int nActualLength = 0;
	int nRet = ETronDI_OK;
	
    nRet = GetLogData(DumpBuffer, BufferLength, &nActualLength, index, RECTIFY_LOG);
	

	if( nRet != ETronDI_OK ) return nRet;
	else {
			
		double M1[9];
		double D1[8];
		double M2[9];
		double D2[8];
		double R[9];
		double T[3];
		double R1[9];
		double R2[9];
		double P1[12];
		double P2[12];
		//double Q[16];
		//double E[9];
		//double F[9];
		
		//float mConfidenceValue[32];
		//float mConfidenceTH[32];
		//short mConfidenceLevel;

		/*double InvR1[3][3];
		double InvR2[3][3];*/

		//bool EnableFixedROI;		//下一版更新

		short  tempBufferShort[8];
		MY_INT tempBufferLong[16];
		//TCHAR strtest[256];

		short idx = 0;

		//Table ID 0: Write Log Szie
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*1);
		//int Log_Size  = (int)tempBufferShort[0];		// 2 bytes
		DumpBuffer += 2;
		idx++;

		DumpBuffer += (TableHeaderSize + TableInfoSize * TAB_MAX_NUM);

		//wsprintf(strtest,L" DumpBuffer = 0x%x   *DumpBuffer = %d\n",DumpBuffer,*DumpBuffer);
		//OutputDebugString(strtest);

		//Table ID 1: Write "InOutDim" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		int in_width   = (int)tempBufferShort[0];		// 2 bytes
		int in_height  = (int)tempBufferShort[1];		// 2 bytes
		int out_width  = (int)tempBufferShort[2];		// 2 bytes
		int out_height = (int)tempBufferShort[3];		// 2 bytes
		//int color_type = (int)tempBufferShort[4];		// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->InImgWidth   = (unsigned short)in_width;
		pData->InImgHeight  = (unsigned short)in_height;
		pData->OutImgWidth  = (unsigned short)out_width;
		pData->OutImgHeight = (unsigned short)out_height;
		//wsprintf(strtest,_T("  width = %d height = %d  width = %d height = %d\n"),in_width,in_height,out_width,  out_height);
		//OutputDebugString(strtest);


		//Table ID 2: Write "RECT_Flags" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		//bool EnableModule = (bool)tempBufferShort[0];	// 2 bytes
		bool EnableScale  = (bool)tempBufferShort[1];	// 2 bytes
		bool EnableCrop   = (bool)tempBufferShort[2];	// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->RECT_ScaleEnable = (int)EnableScale;
		pData->RECT_CropEnable = (int)EnableCrop;

		//Table ID 3: Write "RECT_ScaleDimension" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		int ScaleWidth  = (int)tempBufferShort[0];		// 2 bytes
		int ScaleHeight = (int)tempBufferShort[1];		// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->RECT_ScaleWidth  = (unsigned short)ScaleWidth;
		pData->RECT_ScaleHeight = (unsigned short)ScaleHeight;	

		//Table ID 4: Write "CamMat1" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			M1[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		//Table ID 5: Write "CamDist1" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			D1[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		//Table ID 6: Write "CamMat2" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			M2[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		//Table ID 7: Write "CamDist2" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			D2[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 8: Write "RotaMat" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			R[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 9: Write "TranMat" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			T[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 10: Write "LRotaMat" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			R1[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 11: Write "RRotaMat" Entries				// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			R2[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 12: Write "NewCamMat1" Entries			// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			P1[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 13: Write "NewCamMat2" Entries			// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		for(int i=0;i<TAB_ElementCount[idx];i++)
			P2[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 14: Write "ReprojMat" Entries			// 4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		//for(int i=0;i<TAB_ElementCount[idx];i++)
			//Q[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
			
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		for(int i=0; i<9 ;i++) {
			pData->CamMat1[i] = M1[i];
			pData->CamMat2[i] = M2[i];
			pData->RotaMat[i] = R[i];
			pData->LRotaMat[i] = R1[i];
			pData->RRotaMat[i] = R2[i];
		}
		
		for(int i=0; i<8 ;i++){
			pData->CamDist1[i] = D1[i];
			pData->CamDist2[i] = D2[i];
		}
		
		for(int i=0; i<3 ;i++) {
			pData->TranMat[i] = T[i];
		}
		
		for(int i=0; i<12 ;i++) {
			pData->NewCamMat1[i] = P1[i];
			pData->NewCamMat2[i] = P2[i];
		}
		//Table ID 15: Write "ROI1" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		/*RoiL.x =		(int)tempBufferShort[0];		// 2 bytes
		RoiL.y =		(int)tempBufferShort[1];		// 2 bytes
		RoiL.width =	(int)tempBufferShort[2];		// 2 bytes
		RoiL.height =	(int)tempBufferShort[3];		// 2 bytes*/
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 16: Write "ROI2" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		/*RoiR.x =		(int)tempBufferShort[0];		// 2 bytes
		RoiR.y =		(int)tempBufferShort[1];		// 2 bytes
		RoiR.width =	(int)tempBufferShort[2];		// 2 bytes
		RoiR.height =	(int)tempBufferShort[3];		// 2 bytes*/
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 17: Write "ComROI" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		/*CommonROI.x =		(int)tempBufferShort[0];		// 2 bytes
		CommonROI.y =		(int)tempBufferShort[1];		// 2 bytes
		CommonROI.width =	(int)tempBufferShort[2];		// 2 bytes
		CommonROI.height =	(int)tempBufferShort[3];		// 2 bytes*/
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 18: Write "ScaleROI" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		/*SCommonROI.x =		(int)tempBufferShort[0];		// 2 bytes
		SCommonROI.y =		(int)tempBufferShort[1];		// 2 bytes
		SCommonROI.width =	(int)tempBufferShort[2];		// 2 bytes
		SCommonROI.height =	(int)tempBufferShort[3];		// 2 bytes*/
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 19: Write "RECT_Crop" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		int	mCrop_Row_BG   = (int)tempBufferShort[0];		// 2 bytes
		int	mCrop_Row_ED   = (int)tempBufferShort[1];		// 2 bytes
		int	mCrop_Col_BG_L = (int)tempBufferShort[2];		// 2 bytes
		int	mCrop_Col_ED_L = (int)tempBufferShort[3];		// 2 bytes
		//int	mCrop_Col_BG_R = (int)tempBufferShort[4];		// 2 bytes
		//int	mCrop_Col_ED_R = (int)tempBufferShort[5];		// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->RECT_Crop_Row_BG = (unsigned short)mCrop_Row_BG;
		pData->RECT_Crop_Row_ED = (unsigned short)mCrop_Row_ED;
		pData->RECT_Crop_Col_BG_L = (unsigned short)mCrop_Col_BG_L;
		pData->RECT_Crop_Col_ED_L = (unsigned short)mCrop_Col_ED_L;

		//Table ID 20: Write "RECT_Scale_MN" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		int	mScale_Col_M = (int)tempBufferShort[0];		// 2 bytes
		int	mScale_Col_N = (int)tempBufferShort[1];		// 2 bytes
		int	mScale_Row_M = (int)tempBufferShort[2];		// 2 bytes
		int	mScale_Row_N = (int)tempBufferShort[3];		// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->RECT_Scale_Col_M = (unsigned char)mScale_Col_M;
		pData->RECT_Scale_Col_N = (unsigned char)mScale_Col_N;
		pData->RECT_Scale_Row_M = (unsigned char)mScale_Row_M;
		pData->RECT_Scale_Row_N = (unsigned char)mScale_Row_N;
		
		//Table ID 21: Write "RECT_Scale_C" Entries
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		//double mScale_Col_C = ((double)(tempBufferLong[0]))/(1<<TAB_FractionalBit[idx]);
		//double mScale_Row_C = ((double)(tempBufferLong[1]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 22: Write "RECT_ErrMeasure" Entries		4 bytes
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		double	avgReprojectError = ((double)(tempBufferLong[0]))/(1<<TAB_FractionalBit[idx]);
		//double	minYShiftError	  = ((double)(tempBufferLong[1]))/(1<<TAB_FractionalBit[idx]);
		//double	maxYShiftError	  = ((double)(tempBufferLong[2]))/(1<<TAB_FractionalBit[idx]);
		//double	avgYShiftError	  = ((double)(tempBufferLong[3]))/(1<<TAB_FractionalBit[idx]);
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->RECT_AvgErr = (float)avgReprojectError;

		//Table ID 23: Write "LineBufer" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		int	lineBuffer = (int)tempBufferShort[0];		// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;
		
		pData->nLineBuffers = (unsigned short)lineBuffer;

		//Table ID 24: Write "EssentialMat" Entries				// 4 bytes;
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		//for(int i=0;i<TAB_ElementCount[idx];i++)
			//E[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
			
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 25: Write "fundamentalMat" Entries			// 4 bytes;
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		//for(int i=0;i<TAB_ElementCount[idx];i++)
			//F[i] = ((double)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
			
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 26: Write "ConfidenceVal" Entries			// 4 bytes;
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		//for(int i=0;i<TAB_ElementCount[idx];i++)
			//mConfidenceValue[i] =  ((float)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
			
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 27: Write "ConfidenceTH" Entries
		memcpy(tempBufferLong,DumpBuffer,sizeof(MY_INT)*TAB_ElementCount[idx]);
		//for(int i=0;i<TAB_ElementCount[idx];i++)
			//mConfidenceTH[i] = ((float)(tempBufferLong[i]))/(1<<TAB_FractionalBit[idx]);
			
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		idx++;

		//Table ID 28: Write "ConfidenceLevel" Entries
		memcpy(tempBufferShort,DumpBuffer,sizeof(short)*TAB_ElementCount[idx]);
		//mConfidenceLevel = (int)tempBufferShort[0];	// 2 bytes
		DumpBuffer += (TAB_DataSize[idx]*TAB_ElementCount[idx]);
		
		return ETronDI_OK;	
	}
}

int CVideoDevice::GetZDLogFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

int CVideoDevice::GetDepthQualityLogFromCalibrationLog() {
	
	return ETronDI_NotSupport;
}

// for calibration log -

int CVideoDevice::ReadFlashData ( BYTE *pBuffer, unsigned long int BufferLength, unsigned long int *pActualLength,  FLASH_DATA_TYPE fdt, bool bBootloader) {
	
    unsigned long int nStartAddr = 0;
    unsigned long int nOffset    = 0;
    unsigned long int ReadLen    = 0;
    unsigned long int nLength    = 0;
    int limit = 128 * 1024;

    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

    if(fdt == Total) {
       if( pBuffer == NULL ) return ETronDI_NullPtr;
       if( BufferLength < 1024 * 128 ) return ETronDI_ErrBufLen;
        nStartAddr = 0;
        ReadLen = BufferLength;
        limit = ReadLen;
    }

    if(fdt == FW_PLUGIN) {
        if( pBuffer == NULL ) return ETronDI_NullPtr;
        // check if bootloader existed in the firmware
        if(bBootloader){
            if( BufferLength < 1024*104 ) return ETronDI_ErrBufLen;
            nStartAddr = 1024*24;
            ReadLen = 1024*104;
        }else{
            if( BufferLength < 1024*128 ) return ETronDI_ErrBufLen;
             nStartAddr = 0;
             ReadLen = 1024*128;
        }
    }

    if(fdt == BOOTLOADER_ONLY) {
        if( bBootloader == false ) return ETronDI_DEVICE_NOT_SUPPORT;
        if( pBuffer == NULL ) return ETronDI_NullPtr;
        if( BufferLength < 1024*24 ) return ETronDI_ErrBufLen;
        nStartAddr = 0;
        ReadLen = 1024*24;
    }

    if(fdt == FW_ONLY) {
        if( pBuffer == NULL ) return ETronDI_NullPtr;
        if( BufferLength < 1024*40 ) return ETronDI_ErrBufLen;
        if( bBootloader == true ) nStartAddr = 1024*24;
        else nStartAddr = 0;
        ReadLen = 1024*40;
    }

    if(fdt == PLUGIN_ONLY) {
        if( pBuffer == NULL ) return ETronDI_NullPtr;
        if( BufferLength < 1024*56 ) return ETronDI_ErrBufLen;
        nStartAddr = 1024*64;
        ReadLen = 1024*56;
    }

    int nRet = ETronDI_OK;
    *pActualLength = ReadLen;
    nOffset = nStartAddr;


    while ( nOffset < (nStartAddr+ReadLen) && nOffset < limit) {

       LOGI("nOffset : %d\n", nOffset);
       nLength = (nStartAddr + ReadLen) - nOffset;
       if (nLength > LENGTH_LIMIT) nLength = LENGTH_LIMIT;
       //
       m_FWFSData.nType = FWFS_TYPE_READ_FLASH;
       m_FWFSData.nFsID = 0;
       m_FWFSData.nOffset = nOffset;
       m_FWFSData.nLength = nLength;
       if (FWFS_Command(&m_FWFSData) < 0) {
           if(m_bIsLogEnabled) LOGE("ReadFlashData::Read Data Error!\n");
           nRet = ETronDI_READFLASHFAIL;
           break;
       }
       //
       memcpy(pBuffer+nOffset-nStartAddr, m_FWFSData.nData, nLength);
       nOffset += nLength;
    }

    return nRet;
}

int CVideoDevice::WriteFlashData_FullFunc( BYTE *pBuffer, unsigned long int nBufferLength, 
                                           FLASH_DATA_TYPE fdt, KEEP_DATA_CTRL kdc, bool bIsDataVerify,  bool bBootloader) {
	
	unsigned long int nStartAddr        = 0;
	unsigned long int nWriteLen         = 0;
	
	unsigned long int nPlugInStartAddr  = 0;
	unsigned long int nFileSysStartAddr = 0;
	
	int  nLengthOfFileID  = 0;
	BYTE *pBufferOfFileID = NULL; 
	

	if(fdt == Total) {
		if( pBuffer == NULL ) return ETronDI_NullPtr;
		if( nBufferLength < 1024*120 ) return ETronDI_ErrBufLen;
		nStartAddr = 0;
        nWriteLen = nBufferLength;

        nPlugInStartAddr  = 1024*64;
	}
	
	if(fdt == FW_PLUGIN) {
		if( pBuffer == NULL ) return ETronDI_NullPtr;
        // check if bootloader existed in the firmware
        if(bBootloader){
            if( nBufferLength < 1024*40 ) return ETronDI_ErrBufLen;
            nStartAddr = 1024*24;
            nWriteLen = 1024*104;
            nPlugInStartAddr = 1024*40;
        }else{
            nStartAddr = 0;
            nWriteLen = 1024*128;
            nPlugInStartAddr = 1024*64;
         }
	}
	
	if(fdt == BOOTLOADER_ONLY) {
        if( bBootloader == false ) return ETronDI_DEVICE_NOT_SUPPORT;
		if( pBuffer == NULL ) return ETronDI_NullPtr;
		if( nBufferLength < 1024*24 ) return ETronDI_ErrBufLen;
		nStartAddr = 0;
		nWriteLen = 1024*24;
	}
	
	if(fdt == FW_ONLY) {
		if( pBuffer == NULL ) return ETronDI_NullPtr;
		if( nBufferLength < 1024*40 ) return ETronDI_ErrBufLen;
        if(bBootloader) nStartAddr = 1024*24;
        else nStartAddr = 0;
		nWriteLen = 1024*40;
	}
	
	if(fdt == PLUGIN_ONLY) {
		if( pBuffer == NULL ) return ETronDI_NullPtr;
		if( nBufferLength < 0 ) return ETronDI_ErrBufLen;
		nStartAddr = 1024*64;
		nWriteLen = 1024*56;
		
		nPlugInStartAddr = 0;
	}
		
	if(fdt == Total || fdt == FW_PLUGIN || fdt == PLUGIN_ONLY) {

		nFileSysStartAddr = (unsigned long int)(pBuffer[nPlugInStartAddr+20]*256) + 
							(unsigned long int)pBuffer[nPlugInStartAddr+21] + 
							nPlugInStartAddr; 
        int nActualLength = 0;								
		
		if(kdc.bIsSerialNumberKeep) {
			
            LOGI("keep serial number\n");
			
			BYTE SN_Buf[1024];
			int nACtualSNLenByByte = 0;		
			GetSerialNumber(SN_Buf, 512, &nACtualSNLenByByte);
			
			if( pBuffer[nPlugInStartAddr+8]*256 + pBuffer[nPlugInStartAddr+9] >=4 ) { // ap 2.x
				
				if(!GetDataLengthFromFileID( 1, pBuffer, &nLengthOfFileID, nFileSysStartAddr)) return ETronDI_KEEP_DATA_FAIL;
				
				pBufferOfFileID = (BYTE*)malloc(nLengthOfFileID);
				if(!GetDataBufferFromFileID( 1, pBuffer, pBufferOfFileID, nLengthOfFileID, nFileSysStartAddr)) return ETronDI_KEEP_DATA_FAIL; 
				
				int nStartIndex = 0;
				int nDataSizeAfterSN = 0;
				int nOriginalLength = 0;
				int nNewLength = nACtualSNLenByByte+2;
				char *pBuf = NULL;
				int i = 0;

				int temp1 = pBufferOfFileID[VARIABLE_DATA_START_ADDR];
				int temp2 = pBufferOfFileID[VARIABLE_DATA_START_ADDR+temp1];
				nStartIndex = VARIABLE_DATA_START_ADDR+temp1+temp2; 
				nOriginalLength = pBufferOfFileID[nStartIndex];
				i = nStartIndex + nOriginalLength;

				// Get the numner of data after SN
				while(pBufferOfFileID[i]!=0) 
					i += pBufferOfFileID[i];

				nDataSizeAfterSN = i -(nStartIndex + nOriginalLength);

				if( nStartIndex + nNewLength + nDataSizeAfterSN > 1024) 
                    return ETronDI_KEEP_DATA_FAIL;

				pBuf = (char*)malloc(nDataSizeAfterSN);
				memcpy(pBuf, pBufferOfFileID+nStartIndex+nOriginalLength, nDataSizeAfterSN); 

				for(i=nStartIndex;i<1024;i++) 
					pBufferOfFileID[i] = 0x0;

				pBufferOfFileID[nStartIndex] = nNewLength;
				pBufferOfFileID[nStartIndex+1] = 0x03;

				memcpy(pBufferOfFileID+nStartIndex+2, SN_Buf, nACtualSNLenByByte);
				memcpy(pBufferOfFileID+nStartIndex+nNewLength, pBuf, nDataSizeAfterSN);

				free(pBuf);

				if(!SetDataBufferToFileID( 1, pBuffer, pBufferOfFileID, nLengthOfFileID, nFileSysStartAddr)) 
					return ETronDI_KEEP_DATA_FAIL;

				free(pBufferOfFileID);
			}
			
			else { // ap1.x
				if(nACtualSNLenByByte>30) return ETronDI_KEEP_DATA_FAIL;
				if(!GetDataLengthFromFileID( 0, pBuffer, &nLengthOfFileID, nFileSysStartAddr)) return ETronDI_KEEP_DATA_FAIL;

				pBufferOfFileID = (BYTE*)malloc(nLengthOfFileID); 
				if(!GetDataBufferFromFileID( 0, pBuffer, pBufferOfFileID, nLengthOfFileID, nFileSysStartAddr)) return ETronDI_KEEP_DATA_FAIL;

				pBufferOfFileID[SERIAL_NUMBER_START_OFFSET] = nACtualSNLenByByte+2;
				memcpy(pBufferOfFileID+SERIAL_NUMBER_START_OFFSET+2,SN_Buf,nACtualSNLenByByte);

				if(!SetDataBufferToFileID( 0, pBuffer, pBufferOfFileID, nLengthOfFileID, nFileSysStartAddr)) return ETronDI_KEEP_DATA_FAIL;

				free(pBufferOfFileID);
			}
		}
		
		if(kdc.bIsSensorPositionKeep) {
			
			//if( !GetDataLengthFromFileID( 30, pBuffer, &nLengthOfFileID, nFileSysStartAddr)) return ETronDI_KEEP_DATA_FAIL;
			nLengthOfFileID = 256;
			BYTE *pYOffset_Buf = (BYTE*)malloc(nLengthOfFileID);
			
            LOGI("nLengthOfFileID of pYOffset_Buf = %d\n", nLengthOfFileID);
			
			if( GetYOffset( pYOffset_Buf, nLengthOfFileID, &nActualLength, 0) != ETronDI_OK) { 
				
                LOGE("GetYOffset failed ...\n");
				free(pYOffset_Buf);
				return ETronDI_KEEP_DATA_FAIL;  
			}
			else {
				if( !SetDataBufferToFileID( 30, pBuffer, pYOffset_Buf, nLengthOfFileID, nFileSysStartAddr)) { 
					free(pYOffset_Buf);
					return ETronDI_KEEP_DATA_FAIL;
				}
			} 
			free(pYOffset_Buf); 
		}
		
		if(kdc.bIsRectificationTableKeep) {
			
			nLengthOfFileID = 1024;
			BYTE *pRectify_Buf = (BYTE*)malloc(nLengthOfFileID);
			
            LOGI("nLengthOfFileID of pRectify_Buf = %d\n", nLengthOfFileID);
			
			if( GetRectifyTable( pRectify_Buf, nLengthOfFileID, &nActualLength, 0) != ETronDI_OK) { 
				
                LOGE("GetRectifyTable failed ...\n");
				free(pRectify_Buf);
				return ETronDI_KEEP_DATA_FAIL;  
			}
			else {
				if( !SetDataBufferToFileID( 40, pBuffer, pRectify_Buf, nLengthOfFileID, nFileSysStartAddr)) { 
					free(pRectify_Buf);
					return ETronDI_KEEP_DATA_FAIL;
				}
			} 
			free(pRectify_Buf); 
		}
		
		if(kdc.bIsZDTableKeep) {

            ZDTABLEINFO zdTableInfo;
            BYTE *pZD_Buf = NULL;
            int i;

            for(i=0; i<2; i++){
                zdTableInfo.nIndex = i;
                if(i==0){
                    // 8 bits ZD
                    zdTableInfo.nDataType = ETronDI_DEPTH_DATA_8_BITS;
                    nLengthOfFileID = ETronDI_ZD_TABLE_FILE_SIZE_8_BITS;
                }
                else if(i==1 && bBootloader==false){
                    // 11 bits ZD
                    zdTableInfo.nDataType = ETronDI_DEPTH_DATA_11_BITS;
                    nLengthOfFileID = ETronDI_ZD_TABLE_FILE_SIZE_11_BITS;
                }
                else break;

                pZD_Buf = (BYTE*)malloc(nLengthOfFileID);

                if( GetZDTable( pZD_Buf, nLengthOfFileID, &nActualLength, &zdTableInfo) != ETronDI_OK) {

                    LOGE("GetZDTable failed ...\n");
                    free(pZD_Buf);
                    return ETronDI_KEEP_DATA_FAIL;
                }
                else {
                    if( !SetDataBufferToFileID( 50, pBuffer, pZD_Buf, nLengthOfFileID, nFileSysStartAddr)) {
                        free(pZD_Buf);
                        return ETronDI_KEEP_DATA_FAIL;
                    }
                }
                free(pZD_Buf);
            }
        }

		if(kdc.bIsCalibrationLogKeep) {
			
			nLengthOfFileID = 4096;
			BYTE *pLOG_Buf = (BYTE*)malloc(nLengthOfFileID);
			
			if( GetLogData( pLOG_Buf, nLengthOfFileID, &nActualLength, 0, ALL_LOG) != ETronDI_OK) {
				
                LOGE("GetLogData failed ...\n");
				free(pLOG_Buf);
				return ETronDI_KEEP_DATA_FAIL;  
			}
			else {
				if( !SetDataBufferToFileID( 240, pBuffer, pLOG_Buf, nLengthOfFileID, nFileSysStartAddr)) { 
					free(pLOG_Buf);
					return ETronDI_KEEP_DATA_FAIL;
				}
			} 
			free(pLOG_Buf); 
		}
	}	

	return WriteFlashData(pBuffer, nBufferLength, nStartAddr, nWriteLen, fdt); 
}

int CVideoDevice::WriteFlashData ( BYTE *pBuffer, unsigned long int nBufferLength, unsigned long int nStartAddr, 
								   unsigned long int nWriteLen, FLASH_DATA_TYPE fdt ) {
						 
	unsigned long int nOffset    = 0;
	unsigned long int nLength    = 0;	
    bool b_256M = false;
    int limit;
    int count_retry = 10;

    if (nWriteLen == (1024 * 256)) {
        b_256M = true;
    }

    if (b_256M == true)
        limit = 256 * 1024;
    else limit = 128 * 1024;

    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	int nRet = ETronDI_OK;

    BYTE *lpSrcBuf = (BYTE *)malloc(nWriteLen * sizeof(BYTE));
	memcpy(lpSrcBuf, pBuffer, nBufferLength);
    memset(pBuffer + 1024 * 120, 0xff, 1024 * 8);
	
	nOffset = nStartAddr;
    while ( nOffset < (nStartAddr+nWriteLen) && nOffset < limit) {
        LOGI("nOffset : %d\n", nOffset);
  		nLength = (nStartAddr + nWriteLen) - nOffset;
  		if (nLength > LENGTH_LIMIT) nLength = LENGTH_LIMIT;
  		//
  		m_FWFSData.nType = FWFS_TYPE_WRITE_FLASH;
  		m_FWFSData.nFsID = 0;
  		m_FWFSData.nOffset = nOffset;
  		m_FWFSData.nLength = nLength;

  		memcpy(m_FWFSData.nData, lpSrcBuf+nOffset-nStartAddr, nLength);
 read_do:
  		nRet = FWFS_Command(&m_FWFSData);
  		if (nRet < 0) {
          LOGE("WriteFlashData::Write Data Error!\n");
          if (count_retry > 0) {
              count_retry--;
              usleep(1000 * 2);
              goto read_do;
          } else {
              LOGE("write fail count: 10...\n");
              nRet = ETronDI_WRITEFLASHFAIL;
              break;
          }
        } else {
            count_retry = 10;
        }
  		//
        nOffset += nLength;
    }
	//
  	// Write Sync
  	m_FWFSData.nType = FWFS_TYPE_WRITE_SYNC;
  	if(-1 == FWFS_Command(&m_FWFSData)) {
		
        if(m_bIsLogEnabled) LOGE("writeSPIFlash::Write Sync Error!\n");
		nRet = ETronDI_WRITEFLASHFAIL;
	}
	
	free(lpSrcBuf);
	return nRet;

}
int CVideoDevice::GetDevicePortType(USB_PORT_TYPE &eUSB_Port_Type)
{
    static const std::map<std::string, USB_PORT_TYPE> usb_spec_map = {
        {"Undefined", USB_PORT_TYPE_UNKNOW}, {"1.0", USB_PORT_TYPE_UNKNOW},
        {"1.1", USB_PORT_TYPE_UNKNOW},       {"2.0", USB_PORT_TYPE_2_0},
        {"2.01", USB_PORT_TYPE_2_0},         {"2.1", USB_PORT_TYPE_2_0},
        {"3.0", USB_PORT_TYPE_3_0},          {"3.1", USB_PORT_TYPE_3_0},
        {"3.2", USB_PORT_TYPE_3_0},
    };

    std::string path = GetDeviceUSBInfoPath();
    if (path.empty()) return ETronDI_READ_REG_FAIL;

    std::string val;
    if (!(std::ifstream(path + "/version") >> val))
        return ETronDI_READ_REG_FAIL;

    auto usb_spec = std::find_if(
        usb_spec_map.begin(), usb_spec_map.end(),
        [&val](const std::pair<std::string, USB_PORT_TYPE> &usb_spec) {
            return (std::string::npos != val.find(usb_spec.first));
        });

    if (usb_spec == std::end(usb_spec_map)) return ETronDI_NotSupport;

    return usb_spec->second;
}

// File ID -

int CVideoDevice::GetV4l2BusInfo ( char *pszBusInfo) {
	
	v4l2_capability cap;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	
    if( ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {
		
		if(EINVAL == errno) {
			
            if(m_bIsLogEnabled) LOGE("GetV4l2BusInfo failed[EINVAL]\n");
		}
        else {
			
            if(m_bIsLogEnabled) LOGE("GetV4l2BusInfo failed[%d]\n", errno);
        }

        IOCTL_ERROR_LOG();
        return ETronDI_GET_RES_LIST_FAIL;
    }
    
    LOGD("bus info = %s\n", cap.bus_info);
    strcpy(pszBusInfo, (const char*)cap.bus_info);
    
    return ETronDI_OK;
}

int CVideoDevice::GetDeviceCapability(unsigned int *pDeviceCapability)
{
    if(pDeviceCapability == nullptr) return ETronDI_NullPtr;

    v4l2_capability cap;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

    if( ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {

        if(EINVAL == errno) {

            if(m_bIsLogEnabled) LOGE("GetDeviceCapability failed[EINVAL]\n");
        }
        else {

            if(m_bIsLogEnabled) LOGE("GetDeviceCapability failed[%d]\n", errno);
        }

        IOCTL_ERROR_LOG();

        return ETronDI_GET_RES_LIST_FAIL;
    }

    *pDeviceCapability = cap.device_caps;

    return ETronDI_OK;
}

int CVideoDevice::GetDeviceResolutionList(int nMaxCount, ETRONDI_STREAM_INFO *pStreamInfo) {
	
	enum_frame_formats(pStreamInfo);	
	return ETronDI_OK; 
}

int CVideoDevice::OpenDevice( int nResWidth, int nResHeight, bool bIsMJPEG, int *pFPS, DEPTH_TRANSFER_CTRL dtc) {
						
    CLEAR(m_fival);	

    LOGD("CVideoDevice::OpenDevice %dx%d fps=%d\n", nResWidth, nResHeight, *pFPS);

    m_fival.width = nResWidth;
    m_fival.height = nResHeight;
    if(bIsMJPEG) m_fival.pixel_format = V4L2_PIX_FMT_MJPEG;
    else m_fival.pixel_format = V4L2_PIX_FMT_YUYV;
    
    if( ETronDI_OK != init_device() ) {
        if(m_bIsLogEnabled) LOGE("init_device() failed .\n");
		return ETronDI_OPEN_DEVICE_FAIL;
	}
    else {
		
		if(pFPS != NULL) {
			enum_frame_intervals( m_fival.pixel_format, m_fival.width, m_fival.height);
			set_framerate(pFPS);
		}
		
		if( ETronDI_OK != start_capturing() ) {
			stop_capturing();
			uninit_device();
            LOGE("start_capturing() failed .\n");
			return ETronDI_OPEN_DEVICE_FAIL;
		}
	}
	
	m_Dtc = dtc;

    return ETronDI_OK;						
}	

int CVideoDevice::CloseDevice()	{
    LOGD(" CVideoDevice::CloseDevice() enter\n");

	stop_capturing();
	uninit_device();
	RelaseComputer();

	return ETronDI_OK;
}

// this function check if the 2 bit serial number count is supported for MJPEG
bool CVideoDevice::Is2BitSerial(){

    unsigned short nValue = 0;

    GetHWRegister( SERIAL_2BIT_ADDR, &nValue, FG_Address_2Byte | FG_Value_1Byte );

    if(BIT_CHECK(nValue, 2))
        return false;

    return true;
}

int CVideoDevice::GetImage(BYTE *pBuf, unsigned long int *pImageSize, int *pSerial, bool bIsDepthmap, unsigned short nDepthDataType) {
	
	int nRet = ETronDI_OK;
	size_t len = 0; 
	int i = 0;

	nRet = get_frame((void **)&m_p, &len);
	if( nRet == ETronDI_OK ) {
		if(!bIsDepthmap) {
		
			*pImageSize = len;
			memcpy( pBuf, m_p, len);
			unget_frame();
					
        } else {
			if(m_Dtc == DEPTH_IMG_NON_TRANSFER) {
				
				*pImageSize = len;
				memcpy( pBuf, m_p, len);
				unget_frame();
			}
			
			if(m_Dtc == DEPTH_IMG_GRAY_TRANSFER) {
				
				*pImageSize = len*3;
                convert_depth_y_to_buffer( (unsigned char*)m_p, pBuf, m_fival.width, m_fival.height, false, nDepthDataType);
                unget_frame();
			}
			
			if(m_Dtc == DEPTH_IMG_COLORFUL_TRANSFER) {
				*pImageSize = len*3;
                convert_depth_y_to_buffer( (unsigned char*)m_p, pBuf, m_fival.width, m_fival.height, true, nDepthDataType);
				unget_frame();
			}
            m_nFrameCount++;
            if (m_nFrameCount>10000)
                m_nFrameCount=0;
		}

		if(pSerial != NULL) {
            *pSerial = 0;

            if( m_devType == AXES1 ) {
                // calculate serial number
                if (m_fival.pixel_format != V4L2_PIX_FMT_YUYV) {
                    *pSerial = 0;
                } else {
                    for ( i = 0; i < 8; i++ ) {
                        *pSerial |= ( *(((unsigned char*)m_p)+i)&1) << i;
                    }
                }
            } else {
                if(m_fival.pixel_format != V4L2_PIX_FMT_YUYV)
                    *pSerial = *(((unsigned char*)m_p)+6)*256 + *(((unsigned char*)m_p)+7);
                 else {
                     // calculate serial number
                     for ( i = 0; i < 16; i++ ) {
                        *pSerial |= ( *(((unsigned char*)m_p)+i)&1) << i;
                     }
                 }
            }
        }
	}
	else {
		if(pSerial!=NULL) *pSerial = -1;
	}
	
	return nRet;
}

int CVideoDevice::EnableFrameCount() {

	unsigned short nValue = 0;
	//sleep(1);
	while(nValue == 0) {
		GetHWRegister( 0xf067, &nValue, FG_Address_2Byte | FG_Value_1Byte );
	}
	
	if(!BIT_CHECK(nValue, 7)) {
		BIT_SET(nValue, 7);
		SetHWRegister( 0xf067, nValue, FG_Address_2Byte | FG_Value_1Byte );
	}
	
	return ETronDI_OK;
}

int CVideoDevice::SetDepthDataType(unsigned short nValue) {
    int flag = 0;
    flag |= FG_Address_1Byte;
    flag |= FG_Value_1Byte;

    if(SetFWRegister( 0xF0, nValue , flag) != ETronDI_OK )
       return ETronDI_WRITE_REG_FAIL;
    return ETronDI_OK;
}

int CVideoDevice::GetDepthDataType(unsigned short *pValue) {

    if ( GetFWRegister( 0xF0, pValue, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
         return ETronDI_READ_REG_FAIL;

    return ETronDI_OK;
}

int CVideoDevice::SetCurrentIRValue(unsigned short nValue)
{
  if (SetFWRegister(0xE0, nValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
    return ETronDI_WRITE_REG_FAIL;

  return ETronDI_OK;
}

int CVideoDevice::GetCurrentIRValue(unsigned short *pValue)
{
  if (GetFWRegister(0xE0, pValue, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
  	return ETronDI_READ_REG_FAIL;

  return ETronDI_OK;
}

int CVideoDevice::GetIRMinValue(unsigned short *pValue)
{
  if (GetFWRegister(0xE1, pValue, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
  	return ETronDI_READ_REG_FAIL;

  return ETronDI_OK;
}

int CVideoDevice::SetIRMaxValue(unsigned short nValue)
{
    if(nValue > 15) {
        nValue = 15;
    }
    return SetFWRegister(0xE2, nValue, FG_Address_1Byte | FG_Value_1Byte);
}

int CVideoDevice::GetIRMaxValue(unsigned short *pValue)
{
  if (GetFWRegister(0xE2, pValue, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
  	return ETronDI_READ_REG_FAIL;

  return ETronDI_OK;
}

int CVideoDevice::SetIRMode(unsigned short nValue)
{
  if (SetFWRegister(0xE3, nValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
    return ETronDI_WRITE_REG_FAIL;

  return ETronDI_OK;
}

int CVideoDevice::GetIRMode(unsigned short *pValue)
{
  if (GetFWRegister(0xE3, pValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
    return ETronDI_WRITE_REG_FAIL;

  return ETronDI_OK;
}

typedef struct AE_Map {
    int pow_index;
    int value;
} AE_MAP_t;

const AE_MAP_t AE_MAP[] = {{-13, 1},   {-12, 2},   {-12, 3},    {-11, 4},   {-11, 5},
                           {-10, 9},   {-10, 10},  {-9, 19},    {-9, 20},
                           {-8, 39},   {-7, 78},   {-6, 156},   {-5, 312}, {-5, 313},
                           {-4, 624}, {-4, 625},    {-3, 1250}, {-2, 2500},
                           {-1, 5000}, {0, 10000}, {1, 20000},
                           {2, 40000}, {3, 80000}};


int getAE_index(int value)
{
    int i;
    int length = sizeof(AE_MAP) / sizeof(AE_MAP_t);

    for (i = 0; i < length; i++) {
        if (AE_MAP[i].value == value) {
            return AE_MAP[i].pow_index;
        }
    }

    return AE_MAP[0].pow_index;
}


int CVideoDevice::GetCTPropVal( int nId, long int *pValue) {
    LOGD("CVideoDevice::GetCTPropVal id=%d\n", nId);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

	struct v4l2_control ctrl;
	
	switch(nId) {
		
		case CT_PROPERTY_ID_AUTO_EXPOSURE_MODE_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE_AUTO;
			break;
			
		case CT_PROPERTY_ID_AUTO_EXPOSURE_PRIORITY_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
			break;
			
		case CT_PROPERTY_ID_EXPOSURE_TIME_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_EXPOSURE_TIME_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_FOCUS_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_AUTO_CTRL:
			ctrl.id = V4L2_CID_FOCUS_AUTO;
			break;
			
		case CT_PROPERTY_ID_IRIS_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_IRIS_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_IRIS_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_IRIS_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_ZOOM_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_ZOOM_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_ZOOM_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_ZOOM_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_PAN_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_PAN_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_PAN_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_PAN_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_TILT_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_TILT_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_TILT_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_TILT_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_PRIVACY_CTRL:
			ctrl.id = V4L2_CID_PRIVACY;
			break;
	}
	

    if( ioctl( m_fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        IOCTL_ERROR_LOG();
        return ETronDI_GET_CT_PROP_VAL_FAIL;
    }

    if (nId == CT_PROPERTY_ID_EXPOSURE_TIME_ABSOLUTE_CTRL)
        *pValue = getAE_index(ctrl.value);
    else
        *pValue = ctrl.value;
    return ETronDI_OK;
}


int CVideoDevice::SetCTPropVal( int nId, long int nValue) {
    LOGD("CVideoDevice::SetCTPropVal id=%d, value%d", nId, nValue);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	struct v4l2_control ctrl;
	
	switch(nId) {
		
		case CT_PROPERTY_ID_AUTO_EXPOSURE_MODE_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE_AUTO;
			break;
			
		case CT_PROPERTY_ID_AUTO_EXPOSURE_PRIORITY_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
			break;
			
		case CT_PROPERTY_ID_EXPOSURE_TIME_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
			// input nValue [-13..3] means 2^nValue seconds, 
			// trans to the spec defined value which 1 units is for 100 micro second
            nValue = (int)(pow(2.0, (int)nValue) * 10000);
			break;
			
		case CT_PROPERTY_ID_EXPOSURE_TIME_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_EXPOSURE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_FOCUS_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_AUTO_CTRL:
			ctrl.id = V4L2_CID_FOCUS_AUTO;
			break;
			
		case CT_PROPERTY_ID_IRIS_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_IRIS_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_IRIS_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_IRIS_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_ZOOM_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_ZOOM_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_ZOOM_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_ZOOM_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_PAN_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_PAN_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_PAN_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_PAN_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_TILT_ABSOLUTE_CTRL:
			ctrl.id = V4L2_CID_TILT_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_TILT_RELATIVE_CTRL:
			ctrl.id = V4L2_CID_TILT_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_PRIVACY_CTRL:
			ctrl.id = V4L2_CID_PRIVACY;
			break;
	}
	

	ctrl.value = nValue;
	
    if(m_bIsLogEnabled) LOGI("ctrl.value = %d \n", ctrl.value);
	
    if( ioctl( m_fd, VIDIOC_S_CTRL, &ctrl) < 0 ) {
        IOCTL_ERROR_LOG();
        return ETronDI_SET_CT_PROP_VAL_FAIL;
	}

    return ETronDI_OK;
}

int CVideoDevice::GetPUPropVal( int nId, long int *pValue) {
    LOGD("CVideoDevice::GetPUPropVal id=%d\n", nId);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	struct v4l2_control ctrl;
	
	switch(nId) {
		
		case PU_PROPERTY_ID_BACKLIGHT_COMPENSATION_CTRL:
			ctrl.id = V4L2_CID_BACKLIGHT_COMPENSATION;
			break;
			
		case PU_PROPERTY_ID_BRIGHTNESS_CTRL:
			ctrl.id = V4L2_CID_BRIGHTNESS;
			break;
		
		case PU_PROPERTY_ID_CONTRAST_CTRL:
			ctrl.id = V4L2_CID_CONTRAST;
			break;
			
		case PU_PROPERTY_ID_GAIN_CTRL:
			ctrl.id = V4L2_CID_GAIN;
			break;
		
		case PU_PROPERTY_ID_POWER_LINE_FREQUENCY_CTRL:
			ctrl.id = V4L2_CID_POWER_LINE_FREQUENCY;
			break;
			
		case PU_PROPERTY_ID_HUE_CTRL:
			ctrl.id = V4L2_CID_HUE;
			break;
		
		case PU_PROPERTY_ID_HUE_AUTO_CTRL:
			ctrl.id = V4L2_CID_HUE_AUTO;
			break;
			
		case PU_PROPERTY_ID_SATURATION_CTRL:
			ctrl.id = V4L2_CID_SATURATION;
			break;
		
		case PU_PROPERTY_ID_SHARPNESS_CTRL:
			ctrl.id = V4L2_CID_SHARPNESS;
			break;
			
		case PU_PROPERTY_ID_GAMMA_CTRL:
			ctrl.id = V4L2_CID_GAMMA;
			break;
		
		case PU_PROPERTY_ID_WHITE_BALANCE_CTRL:
			ctrl.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
			break;
			
		case PU_PROPERTY_ID_WHITE_BALANCE_AUTO_CTRL:
			ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
			break;
	}

	
    if( ioctl( m_fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        IOCTL_ERROR_LOG();
        return ETronDI_GET_PU_PROP_VAL_FAIL;
	}

    *pValue = ctrl.value;
    return ETronDI_OK;
}

int CVideoDevice::SetPUPropVal( int nId, long int nValue) {
    LOGD("CVideoDevice::SetPUPropVal id=%d, value%d\n", nId, nValue);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

	struct v4l2_control ctrl;
	
	switch(nId) {
		
		case PU_PROPERTY_ID_BACKLIGHT_COMPENSATION_CTRL:
			ctrl.id = V4L2_CID_BACKLIGHT_COMPENSATION;
			break;
			
		case PU_PROPERTY_ID_BRIGHTNESS_CTRL:
			ctrl.id = V4L2_CID_BRIGHTNESS;
			break;
		
		case PU_PROPERTY_ID_CONTRAST_CTRL:
			ctrl.id = V4L2_CID_CONTRAST;
			break;
			
		case PU_PROPERTY_ID_GAIN_CTRL:
			ctrl.id = V4L2_CID_GAIN;
			break;
		
		case PU_PROPERTY_ID_POWER_LINE_FREQUENCY_CTRL:
			ctrl.id = V4L2_CID_POWER_LINE_FREQUENCY;
			break;
			
		case PU_PROPERTY_ID_HUE_CTRL:
			ctrl.id = V4L2_CID_HUE;
			break;
		
		case PU_PROPERTY_ID_HUE_AUTO_CTRL:
			ctrl.id = V4L2_CID_HUE_AUTO;
			break;
			
		case PU_PROPERTY_ID_SATURATION_CTRL:
			ctrl.id = V4L2_CID_SATURATION;
			break;
		
		case PU_PROPERTY_ID_SHARPNESS_CTRL:
			ctrl.id = V4L2_CID_SHARPNESS;
			break;
			
		case PU_PROPERTY_ID_GAMMA_CTRL:
			ctrl.id = V4L2_CID_GAMMA;
			break;
		
		case PU_PROPERTY_ID_WHITE_BALANCE_CTRL:
			ctrl.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
			break;
			
		case PU_PROPERTY_ID_WHITE_BALANCE_AUTO_CTRL:
			ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
			break;
	}

	ctrl.value = nValue;
	
    if( ioctl( m_fd, VIDIOC_S_CTRL, &ctrl) < 0){
        IOCTL_ERROR_LOG();
        return ETronDI_SET_PU_PROP_VAL_FAIL;
    }

    return ETronDI_OK;
}

int CVideoDevice::GetCTRangeAndStep( int nId, int *pMax, int *pMin, int *pStep, int *pDefault, int *pFlags) {
		
	struct v4l2_queryctrl  Setting;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	switch(nId) {
		
		case CT_PROPERTY_ID_AUTO_EXPOSURE_MODE_CTRL:
			Setting.id = V4L2_CID_EXPOSURE_AUTO;
			break;
			
		case CT_PROPERTY_ID_AUTO_EXPOSURE_PRIORITY_CTRL:
			Setting.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
			break;
			
		case CT_PROPERTY_ID_EXPOSURE_TIME_ABSOLUTE_CTRL:
			Setting.id = V4L2_CID_EXPOSURE_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_EXPOSURE_TIME_RELATIVE_CTRL:
			Setting.id = V4L2_CID_EXPOSURE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_ABSOLUTE_CTRL:
			Setting.id = V4L2_CID_FOCUS_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_RELATIVE_CTRL:
			Setting.id = V4L2_CID_FOCUS_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_FOCUS_AUTO_CTRL:
			Setting.id = V4L2_CID_FOCUS_AUTO;
			break;
			
		case CT_PROPERTY_ID_IRIS_ABSOLUTE_CTRL:
			Setting.id = V4L2_CID_IRIS_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_IRIS_RELATIVE_CTRL:
			Setting.id = V4L2_CID_IRIS_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_ZOOM_ABSOLUTE_CTRL:
			Setting.id = V4L2_CID_ZOOM_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_ZOOM_RELATIVE_CTRL:
			Setting.id = V4L2_CID_ZOOM_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_PAN_ABSOLUTE_CTRL:
			Setting.id = V4L2_CID_PAN_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_PAN_RELATIVE_CTRL:
			Setting.id = V4L2_CID_PAN_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_TILT_ABSOLUTE_CTRL:
			Setting.id = V4L2_CID_TILT_ABSOLUTE;
			break;
			
		case CT_PROPERTY_ID_TILT_RELATIVE_CTRL:
			Setting.id = V4L2_CID_TILT_RELATIVE;
			break;
			
		case CT_PROPERTY_ID_PRIVACY_CTRL:
			Setting.id = V4L2_CID_PRIVACY;
			break;
	}
	
    if( ioctl( m_fd, VIDIOC_QUERYCTRL, &Setting) < 0 ) {
        IOCTL_ERROR_LOG();
        return ETronDI_GET_CT_PROP_RANGE_STEP_FAIL;
	}

    *pMax     = Setting.maximum;
    if (CT_PROPERTY_ID_EXPOSURE_TIME_ABSOLUTE_CTRL == nId){
        *pMax = min(80000, *pMax); // limit exposure time maximum to 80000 micro second;
    }
    *pMin     = Setting.minimum;
    *pStep    = Setting.step;
    *pDefault = Setting.default_value;
    *pFlags   = Setting.flags;

    return ETronDI_OK;
}

int CVideoDevice::GetPURangeAndStep( int nId, int *pMax, int *pMin, int *pStep, int *pDefault, int *pFlags) {
		
	struct v4l2_queryctrl  Setting;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	switch(nId) {
		
		case PU_PROPERTY_ID_BACKLIGHT_COMPENSATION_CTRL:
			Setting.id = V4L2_CID_BACKLIGHT_COMPENSATION;
			break;
			
		case PU_PROPERTY_ID_BRIGHTNESS_CTRL:
			Setting.id = V4L2_CID_BRIGHTNESS;
			break;
		
		case PU_PROPERTY_ID_CONTRAST_CTRL:
			Setting.id = V4L2_CID_CONTRAST;
			break;
			
		case PU_PROPERTY_ID_GAIN_CTRL:
			Setting.id = V4L2_CID_GAIN;
			break;
		
		case PU_PROPERTY_ID_POWER_LINE_FREQUENCY_CTRL:
			Setting.id = V4L2_CID_POWER_LINE_FREQUENCY;
			break;
			
		case PU_PROPERTY_ID_HUE_CTRL:
			Setting.id = V4L2_CID_HUE;
			break;
		
		case PU_PROPERTY_ID_HUE_AUTO_CTRL:
			Setting.id = V4L2_CID_HUE_AUTO;
			break;
			
		case PU_PROPERTY_ID_SATURATION_CTRL:
			Setting.id = V4L2_CID_SATURATION;
			break;
		
		case PU_PROPERTY_ID_SHARPNESS_CTRL:
			Setting.id = V4L2_CID_SHARPNESS;
			break;
			
		case PU_PROPERTY_ID_GAMMA_CTRL:
			Setting.id = V4L2_CID_GAMMA;
			break;
		
		case PU_PROPERTY_ID_WHITE_BALANCE_CTRL:
			Setting.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
			break;
			
		case PU_PROPERTY_ID_WHITE_BALANCE_AUTO_CTRL:
			Setting.id = V4L2_CID_AUTO_WHITE_BALANCE;
			break;
	}
	
    if( ioctl( m_fd, VIDIOC_QUERYCTRL, &Setting) < 0 ) {
        IOCTL_ERROR_LOG();
        return ETronDI_GET_PU_PROP_RANGE_STEP_FAIL;
    }

    *pMax     = Setting.maximum;
    *pMin     = Setting.minimum;
    *pStep    = Setting.step;
    *pDefault = Setting.default_value;
    *pFlags   = Setting.flags;

    return ETronDI_OK;
}

// for AEAWB Control +

int CVideoDevice::SetSensorTypeName(SENSOR_TYPE_NAME stn) {
	
	m_stn = stn; 
	return ETronDI_OK;
}

void CVideoDevice::ByteExchange(unsigned char& byte1, unsigned char& byte2)
{
    if (&byte1 != &byte2)
    {
        byte1 ^= byte2 ^= byte1 ^= byte2;
    }
}

class PlumAR0330Holder{
public:
    PlumAR0330Holder(CVideoDevice *pDeivce){
        m_pDeivce = pDeivce;
        if (m_pDeivce) m_pDeivce->SetPlumAR0330(true);
    }

    ~PlumAR0330Holder(){
        if (m_pDeivce) m_pDeivce->SetPlumAR0330(false);
    }

private:
    CVideoDevice *m_pDeivce;
};
#include <memory>
int CVideoDevice::GetExposureTime(int nSensorMode, float *pfExpTimeMS) {
		
	unsigned long ulPCLK;
	unsigned short  usExpLine, usPixelPerLine;
	unsigned short  usPixel = 0;
    int sensorId;
	//
	unsigned short usValue;

    std::shared_ptr<PlumAR0330Holder> pPlumAR0330Holder(nullptr);
    if (ETRONDI_SENSOR_TYPE_AR0330 == m_stn){
        pPlumAR0330Holder = std::make_shared<PlumAR0330Holder>(this);
    }

    LOGD(" CVideoDevice::GetExposureTime( mode=%d enter", nSensorMode);
	//
	// PCLK
	//
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if( SetFWRegister( 0xA1, 0x3B , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	//

	if( GetFWRegister( 0xA2, &usValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	ulPCLK = usValue & 0xFF;

	if( GetFWRegister( 0xA3, &usValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	ulPCLK = (ulPCLK << 8) | (usValue & 0xFF);

	if( GetFWRegister( 0xA4, &usValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	ulPCLK = (ulPCLK << 8) | (usValue & 0xFF);

	if( GetFWRegister( 0xA5, &usValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	ulPCLK = (ulPCLK << 8) | (usValue & 0xFF);
	if (ulPCLK == 0) return ETronDI_RET_BAD_PARAM;

	//
	// PixelPerLine
	// 
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if( SetFWRegister( 0xA1, 0xE0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	//

	if( GetFWRegister( 0xA2, &usValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	usPixelPerLine = usValue & 0xFF;

	if( GetFWRegister( 0xA3, &usValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	usPixelPerLine = (usPixelPerLine << 8) | (usValue & 0xFF);

	//
	// usExpLine
	//
	switch (m_stn)
	{
		case ETRONDI_SENSOR_TYPE_H22:
			if( GetSensorRegister( H22_SLAVE_ADDR, 0x0002, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;
				        
			usExpLine = usValue & 0xFF;      
			
			if( GetSensorRegister( H22_SLAVE_ADDR, 0x0001, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;   
				     
			usExpLine = (usExpLine << 8) | (usValue & 0xFF);
		break;

        case ETRONDI_SENSOR_TYPE_H65:
        {
            unsigned short nExposureLine;

            if (GetSensorRegister(H65_SLAVE_ADDR, 0x01, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            nExposureLine = (usValue & 0xff);

            if (GetSensorRegister(H65_SLAVE_ADDR, 0x02, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            nExposureLine |= (usValue & 0xff) << 8;

            unsigned short nFrameWidth;
            if (GetSensorRegister(H65_SLAVE_ADDR, 0x20, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            nFrameWidth = (usValue & 0xff);

            if (GetSensorRegister(H65_SLAVE_ADDR, 0x21, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            nFrameWidth |= (usValue & 0xff) << 8;

            *pfExpTimeMS = nExposureLine * ((1.0 / ulPCLK) * nFrameWidth) * 1000.0f;
            return ETronDI_OK;
        }
		case ETRONDI_SENSOR_TYPE_OV7740:
			if( GetSensorRegister( OV4770_SLAVE_ADDR, 0x000F, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;  
				      
			usExpLine = usValue & 0xFF;
			
			if( GetSensorRegister( OV4770_SLAVE_ADDR, 0x0010, &usValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;
				
			usExpLine = (usExpLine << 8) | (usValue & 0xFF);
		break;

        case ETRONDI_SENSOR_TYPE_AR0135:
        case ETRONDI_SENSOR_TYPE_AR0144:
        case ETRONDI_SENSOR_TYPE_AR0522:
        case ETRONDI_SENSOR_TYPE_AR0330:
        case ETRONDI_SENSOR_TYPE_AR1335:
        {
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0135) ? AR0135_SLAVE_ADDR : AR0144_SLAVE_ADDR;
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0522) ? AR0522_SLAVE_ADDR : sensorId;
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR1335) ? AR1335_SLAVE_ADDR : sensorId;
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0330) ? AR0330_SLAVE_ADDR : sensorId;

            unsigned short nExposureLine;

            if (GetSensorRegister(sensorId, 0x3012, &usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((unsigned char *)(&usValue))[0], ((unsigned char *)(&usValue))[1]);

            nExposureLine = usValue;

            unsigned short nFrameWidth;
            if (GetSensorRegister(sensorId, 0x300C, &usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            ByteExchange(((unsigned char *)(&usValue))[0], ((unsigned char *)(&usValue))[1]);

            nFrameWidth = usValue;

            *pfExpTimeMS = nExposureLine * ((1.0 / ulPCLK) * nFrameWidth) * 1000.0f;
            return ETronDI_OK;
        }
		case ETRONDI_SENSOR_TYPE_AR0134:
            if( GetSensorRegister(AR0134_SLAVE_ADDR, 0x3012, &usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((unsigned char *)(&usValue))[0], ((unsigned char *)(&usValue))[1]);
            usExpLine = usValue;

            if (GetSensorRegister( AR0134_SLAVE_ADDR, 0x3014, &usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;

            ByteExchange(((unsigned char *)(&usValue))[0], ((unsigned char *)(&usValue))[1]);
            usPixel = usValue;
        break;
        case ETRONDI_SENSOR_TYPE_OV9714:
			{
				unsigned short nExposureLine;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x3500, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_GETSENSORREG_FAIL;

				nExposureLine = (usValue & 0xf) << 12;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x3501, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_GETSENSORREG_FAIL;

                nExposureLine |= (usValue & 0xff) << 4;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x3502, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_GETSENSORREG_FAIL;

                nExposureLine |= (usValue & 0xf0) >> 4;

				unsigned short nFrameWidth;
				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x0343, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				nFrameWidth = usValue & 0xff;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x0342, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				nFrameWidth |= ((usValue & 0xff) << 8);

                *pfExpTimeMS = nExposureLine * ((1.0 / ulPCLK) * nFrameWidth) * 1000.0f;
				return ETronDI_OK;
			}
        case ETRONDI_SENSOR_TYPE_OV9282:
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_OV9714) ? OV9714_SLAVE_ADDR : OV9282_SLAVE_ADDR;
            if (GetSensorRegister(sensorId, 0x3501, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            usExpLine = usValue & 0xFF;
            if (GetSensorRegister(sensorId, 0x3502, &usValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            usExpLine = (usExpLine << 4) | (usValue >> 4);
            break;

		default:
			return ETronDI_NotSupport;
	}

	//
	// ExpTime
	//
	*pfExpTimeMS = (float)(1000.0 * (usExpLine * usPixelPerLine + usPixel) / ulPCLK);
	//
	return ETronDI_OK;
}

int CVideoDevice::SetExposureTime( int nSensorMode, float fExpTimeMS) {
    LOGD("int CVideoDevice::SetExposureTime ente mode=%d ", nSensorMode);

    auto GetCurrentFPS = [&]() -> int {
        struct v4l2_streamparm Stream_Parm;
        memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
        Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // get current fps
        if(ioctl(m_fd, VIDIOC_G_PARM, &Stream_Parm) < 0) {
            IOCTL_ERROR_LOG();
            return 0;
        }
        return Stream_Parm.parm.capture.timeperframe.denominator;
    };

    const unsigned short nPreservedExposureLine = 5;
    int nFPS = GetCurrentFPS();
    if (0 == nFPS) return ETronDI_WRITE_REG_FAIL;

	unsigned long dwPCLK;
	unsigned short  wExpLine, wPixelPerLine;
    unsigned short wPixel = 0;
	unsigned short wHeight, wBlankV_Extra, wBlankV_Min, wBlankV, wDummyLine;
    int sensorId;
	//
	unsigned short wValue;

    std::shared_ptr<PlumAR0330Holder> pPlumAR0330Holder(nullptr);
    if (ETRONDI_SENSOR_TYPE_AR0330 == m_stn){
        pPlumAR0330Holder = std::make_shared<PlumAR0330Holder>(this);
    }

	//
	// dwPCLK: 0x3B,0x3C,0x3D,0x3E
	//
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	if( SetFWRegister( 0xA1, 0x3B , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	//
	if( GetFWRegister( 0xA2, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	dwPCLK = wValue & 0xFF;

	if( GetFWRegister( 0xA3, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	dwPCLK = (dwPCLK << 8) | (wValue & 0xFF);

	if( GetFWRegister( 0xA4, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	dwPCLK = (dwPCLK << 8) | (wValue & 0xFF);

	if( GetFWRegister( 0xA5, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	dwPCLK = (dwPCLK << 8) | (wValue & 0xFF);
	if (dwPCLK == 0) return ETronDI_RET_BAD_PARAM;

	//
	// wPixelPerLine: 0xE0,0xE1
	// 
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if( SetFWRegister( 0xA1, 0xE0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	//

	if( GetFWRegister( 0xA2, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	wPixelPerLine = wValue & 0xFF;

	if( GetFWRegister( 0xA3, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	wPixelPerLine = (wPixelPerLine << 8) | (wValue & 0xFF);

	//
	// wHeight; 0x33,0x34
	// 
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	if( SetFWRegister( 0xA1, 0x33 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	//
	if( GetFWRegister( 0xA2, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	wHeight = wValue & 0xFF;
	if( GetFWRegister( 0xA3, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	wHeight = (wHeight << 8) | (wValue & 0xFF);
	//
	// wBlankV_Extra; 0x37,0x38
	// wBlankV_Min; 0x39,0x3A
	// 
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	if( SetFWRegister( 0xA1, 0x37 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	//
	if( GetFWRegister( 0xA2, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	wBlankV_Extra = wValue & 0xFF;
	if( GetFWRegister( 0xA3, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	wBlankV_Extra = (wBlankV_Extra << 8) | (wValue & 0xFF);
	//
	//
	if( GetFWRegister( 0xA4, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	wBlankV_Min = wValue & 0xFF;
	if( GetFWRegister( 0xA5, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	wBlankV_Min = (wBlankV_Min << 8) | (wValue & 0xFF);
	//
	// wExpLine
	// 
	if ((0) && (m_stn == ETRONDI_SENSOR_TYPE_AR0134)) {
		//unsigned long dwPCLK2;
		//dwPCLK2 = dwPCLK / 1000;
		wExpLine = (unsigned short)(dwPCLK * (fExpTimeMS / 1000.0f) / wPixelPerLine);
		//wPixel = (unsigned short)(dwPCLK * (fExpTimeMS / 1000.0f) - (float)wPixelPerLine * wExpLine);
    } else if (m_stn == ETRONDI_SENSOR_TYPE_OV9714) {
          wExpLine = (unsigned short)(dwPCLK * (fExpTimeMS / 1000.0f) / wPixelPerLine);
          wPixel = (unsigned short)(dwPCLK * (fExpTimeMS / 1000.0f) - (float)wPixelPerLine * wExpLine);
	} else {
		wExpLine = (unsigned short)(dwPCLK * (fExpTimeMS / 1000.0f) / wPixelPerLine + 0.5f);
        wPixel = 0;
	}
	//
	// wBlankV, wDummyLine
	//
	if (wExpLine < wHeight) {
		wBlankV = wBlankV_Min;
		wDummyLine = 0;
	} else {
		wBlankV = wExpLine - wHeight;
		if (wBlankV < wBlankV_Min) {
			wBlankV = wBlankV_Min;
			wDummyLine = 0;
		} else {
			wBlankV += wBlankV_Extra;
			wDummyLine = wBlankV - wBlankV_Min;
		}
	}
	//
    switch (m_stn) {
		case ETRONDI_SENSOR_TYPE_H22:
			wValue = wExpLine >> 8;
			if( SetSensorRegister( H22_SLAVE_ADDR, 0x0002, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL; 
			wValue = wExpLine & 0xFF;
			if( SetSensorRegister( H22_SLAVE_ADDR, 0x0001, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL; 
			//
			wValue = (wBlankV+wHeight) >> 8;
			if( SetSensorRegister( H22_SLAVE_ADDR, 0x0023, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
			wValue = (wBlankV+wHeight) & 0xFF;
			if( SetSensorRegister( H22_SLAVE_ADDR, 0x0022, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
		break;

        case ETRONDI_SENSOR_TYPE_H65:
        {
            unsigned short nFrameWidth;
            if (GetSensorRegister(H65_SLAVE_ADDR, 0x20, &wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;

            nFrameWidth = (wValue & 0xff);

            if (GetSensorRegister(H65_SLAVE_ADDR, 0x21, &wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;

            nFrameWidth |= (wValue & 0xff) << 8;

            double dblOneLineExposureTime = (1.0 / dwPCLK) * nFrameWidth;
            unsigned short nLimitExposureLine = (((1000.0 / nFPS) / 1000.0) / dblOneLineExposureTime) - nPreservedExposureLine;
            unsigned long ulExposure = ((fExpTimeMS / 1000.0) / dblOneLineExposureTime) + 0.5;
            if (ulExposure > nLimitExposureLine) ulExposure = nLimitExposureLine;
            unsigned short nExposureLine = ulExposure;

            wValue = (nExposureLine & 0xff);
            if (SetSensorRegister(H65_SLAVE_ADDR, 0x01, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;

            wValue = (nExposureLine & 0xff00) >> 8;
            if (SetSensorRegister(H65_SLAVE_ADDR, 0x02, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        }
		case ETRONDI_SENSOR_TYPE_OV7740:
			wValue = wExpLine >> 8;
			if( SetSensorRegister( OV4770_SLAVE_ADDR, 0x000F, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
			wValue = wExpLine & 0xFF;
			if( SetSensorRegister( OV4770_SLAVE_ADDR, 0x0010, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
			//
			wValue = wDummyLine >> 8;
			if( SetSensorRegister( OV4770_SLAVE_ADDR, 0x002E, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
			wValue = wDummyLine & 0xFF;
			if( SetSensorRegister( OV4770_SLAVE_ADDR, 0x002D, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
		break;

        case ETRONDI_SENSOR_TYPE_AR0135:
        case ETRONDI_SENSOR_TYPE_AR0144:
        case ETRONDI_SENSOR_TYPE_AR0522:
        case ETRONDI_SENSOR_TYPE_AR0330:
        case ETRONDI_SENSOR_TYPE_AR1335:
        {
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0135) ? AR0135_SLAVE_ADDR : AR0144_SLAVE_ADDR;
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0522) ? AR0522_SLAVE_ADDR : sensorId;
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR1335) ? AR1335_SLAVE_ADDR : sensorId;
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0330) ? AR0330_SLAVE_ADDR : sensorId;

            unsigned short nFrameWidth;
            if (GetSensorRegister(sensorId, 0x300C, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            ByteExchange(((unsigned char *)(&wValue))[0], ((unsigned char *)(&wValue))[1]);

            nFrameWidth = wValue;

            double dblOneLineExposureTime = (1.0 / dwPCLK) * nFrameWidth;
            unsigned short nLimitExposureLine = (((1000.0 / nFPS) / 1000.0) / dblOneLineExposureTime) - nPreservedExposureLine;
            unsigned long ulExposure = ((fExpTimeMS / 1000.0) / dblOneLineExposureTime) + 0.5;
            if (ulExposure > nLimitExposureLine) ulExposure = nLimitExposureLine;
            unsigned short nExposureLine = ulExposure;

            wValue = nExposureLine;
            if (SetSensorRegister(sensorId, 0x3012, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        }
        break;
        case ETRONDI_SENSOR_TYPE_AR0134:
            wValue = wExpLine;
            if (SetSensorRegister(AR0134_SLAVE_ADDR, 0x3012, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            wValue = wPixel;
            if (SetSensorRegister(AR0134_SLAVE_ADDR, 0x3014, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        break;

        case ETRONDI_SENSOR_TYPE_OV9714:
			{
				unsigned short nFrameWidth;
				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x0343, &wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				nFrameWidth = wValue & 0xff;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x0342, &wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				nFrameWidth |= ((wValue & 0xff) << 8);

                double dblOneLineExposureTime = (1.0 / dwPCLK) * nFrameWidth;
                unsigned short nLimitExposureLine = (((1000.0 / nFPS) / 1000.0) / dblOneLineExposureTime) - nPreservedExposureLine;
                unsigned long ulExposure = ((fExpTimeMS / 1000.0) / dblOneLineExposureTime) + 0.5;
                if (ulExposure > nLimitExposureLine) ulExposure = nLimitExposureLine;
                unsigned short nExposureLine = ulExposure;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x3502, &wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				wValue = ((nExposureLine & 0xf) << 4) | (wValue & 0xf);
				if (SetSensorRegister(OV9714_SLAVE_ADDR, 0x3502, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				wValue = (nExposureLine & 0xff0) >> 4;
				if (SetSensorRegister(OV9714_SLAVE_ADDR, 0x3501, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x3500, &wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;

				wValue = ((nExposureLine & 0xf000) >> 12) | (wValue & 0xf0);

				if (SetSensorRegister(OV9714_SLAVE_ADDR, 0x3500, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
					return ETronDI_SETSENSORREG_FAIL;
			}

			break;
        case ETRONDI_SENSOR_TYPE_OV9282:
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_OV9714) ? OV9714_SLAVE_ADDR : OV9282_SLAVE_ADDR;
            wValue = wExpLine >> 4;
            if (SetSensorRegister(sensorId, 0x3501, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
               return ETronDI_SETSENSORREG_FAIL;
            wValue = (wExpLine & 0x0F) << 4;
            wValue += (wPixel << 4) / wPixelPerLine;
            if (SetSensorRegister(sensorId, 0x3502, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
               return ETronDI_SETSENSORREG_FAIL;
            break;

		default:
			return ETronDI_NotSupport;
	}
	//
	return ETronDI_OK;
}

int CVideoDevice::GetGlobalGain( int nSensorMode, float *pfGlobalGain) {
	
	unsigned short wValue;
    int sensorId;
	float fGain;

    std::shared_ptr<PlumAR0330Holder> pPlumAR0330Holder(nullptr);
    if (ETRONDI_SENSOR_TYPE_AR0330 == m_stn){
        pPlumAR0330Holder = std::make_shared<PlumAR0330Holder>(this);
    }

	//
	switch (m_stn)
	{
		case ETRONDI_SENSOR_TYPE_H22:
        case ETRONDI_SENSOR_TYPE_H65:
			if( GetSensorRegister( H22_SLAVE_ADDR, 0x0000, &wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;
			//
            fGain = (1.0f + (wValue & 0x0F)/16.0f) * pow(2, (wValue & 0x70) >> 4);
			*pfGlobalGain = fGain;	
		break;

		case ETRONDI_SENSOR_TYPE_OV7740:
			if( GetSensorRegister( OV4770_SLAVE_ADDR, 0x0000, &wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;
			//
			fGain = 1.0f + (wValue & 0x0F)/16.0f;
			if (wValue & 0x10) fGain *= 2;
			if (wValue & 0x20) fGain *= 2;
			if (wValue & 0x40) fGain *= 2;
			if (wValue & 0x80) fGain *= 2;
			*pfGlobalGain = fGain;	
		break;

		case ETRONDI_SENSOR_TYPE_AR0134:    
        case ETRONDI_SENSOR_TYPE_AR0135:
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR0134) ? AR0134_SLAVE_ADDR : AR0135_SLAVE_ADDR;
            if( GetSensorRegister(sensorId, 0x305E, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((unsigned char*)(&wValue))[0], ((unsigned char*)(&wValue))[1]);
			//
			fGain = wValue / 32.0f;
			//
            if( GetSensorRegister( sensorId, 0x30B0, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
			//
            ByteExchange(((unsigned char*)(&wValue))[0], ((unsigned char*)(&wValue))[1]);

            switch (wValue & 0x30) {
				case 0x00:	fGain *= 1.0f;	break;
				case 0x10:	fGain *= 2.0f;	break;
				case 0x20:	fGain *= 4.0f;	break;
				case 0x30:	fGain *= 8.0f;	break;
			}
			*pfGlobalGain = fGain;	
		break;
        case ETRONDI_SENSOR_TYPE_AR0144:
            if( GetSensorRegister(AR0144_SLAVE_ADDR, 0x305E, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((unsigned char*)(&wValue))[0], ((unsigned char*)(&wValue))[1]);
            //
            fGain = (wValue & 0x7ff) / 128.0f;
            //
            if( GetSensorRegister( AR0144_SLAVE_ADDR, 0x3060, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            //
            ByteExchange(((unsigned char*)(&wValue))[0], ((unsigned char*)(&wValue))[1]);

            switch (wValue & 0x70) {
                case 0x00:	fGain *= 1.0f;	break;
                case 0x10:	fGain *= 2.0f;	break;
                case 0x20:	fGain *= 4.0f;	break;
                case 0x30:	fGain *= 8.0f;	break;
                case 0x40:	fGain *= 16.0f;	break;
            }
            *pfGlobalGain = fGain;
        break;
        case ETRONDI_SENSOR_TYPE_AR0330:
            if( GetSensorRegister(AR0330_SLAVE_ADDR, 0x305E, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((unsigned char*)(&wValue))[0], ((unsigned char*)(&wValue))[1]);
            //
            fGain = ( wValue & 0x03FF ) / 128.0f;

            if( GetSensorRegister( AR0330_SLAVE_ADDR, 0x3060, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
              return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((BYTE*)&wValue)[0], ((BYTE*)&wValue)[1]);

            switch (wValue & 0x0030)
            {
              case 0x00:	fGain *= 1.0f;	break;
              case 0x10:	fGain *= 2.0f;	break;
              case 0x20:	fGain *= 4.0f;	break;
              case 0x30:	fGain *= 8.0f;	break;
            }
            *pfGlobalGain = fGain;
        break;

        case ETRONDI_SENSOR_TYPE_AR1335:
        case ETRONDI_SENSOR_TYPE_AR0522:
            sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR1335) ? AR1335_SLAVE_ADDR : AR0522_SLAVE_ADDR;
            if( GetSensorRegister(sensorId, 0x305E, &wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            ByteExchange(((unsigned char*)(&wValue))[0], ((unsigned char*)(&wValue))[1]);
            //
            fGain = ( ( wValue & 0xFF80 ) >> 7 ) / 64.0f;
            //
            switch (wValue & 0x0030)
            {
            case 0x0010: fGain *= 1.0f;	break;
            case 0x0020: fGain *= 2.0f;	break;
            case 0x0030: fGain *= 4.0f;	break;
            }
            *pfGlobalGain = fGain * ( 16 + ( wValue & 0x000F ) ) / 16;
            break;

        case ETRONDI_SENSOR_TYPE_OV9714:
            if (GetSensorRegister(OV9714_SLAVE_ADDR, 0x350B, &wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            //
            fGain = 1.0f + (wValue & 0x0F) / 16.0f;
            if (wValue & 0x10) fGain *= 2;
            if (wValue & 0x20) fGain *= 2;
            if (wValue & 0x40) fGain *= 2;
            if (wValue & 0x80) fGain *= 2;
            *pfGlobalGain = fGain;
        break;
        case ETRONDI_SENSOR_TYPE_OV9282:
            if (GetSensorRegister(OV9282_SLAVE_ADDR, 0x3509, &wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_GETSENSORREG_FAIL;
            //
            fGain = (wValue & 0xF0) >> 4;
            fGain += (wValue & 0x0F) / 16.0f;
            *pfGlobalGain = fGain;
        break;

		default:
			return ETronDI_NotSupport;
	}
	//
	return ETronDI_OK;
}

int CVideoDevice::SetGlobalGain( int nSensorMode, float fGlobalGain) {
	
	unsigned short wValue, wValue2;
	float fGain;
    int sensorId;

    std::shared_ptr<PlumAR0330Holder> pPlumAR0330Holder(nullptr);
    if (ETRONDI_SENSOR_TYPE_AR0330 == m_stn){
        pPlumAR0330Holder = std::make_shared<PlumAR0330Holder>(this);
    }

	//
	if (fGlobalGain < 1.0f) fGlobalGain = 1.0f;
	//
	switch (m_stn)
	{
		case ETRONDI_SENSOR_TYPE_H22:
        case ETRONDI_SENSOR_TYPE_H65:
        {
            wValue = 0x00;
			fGain = fGlobalGain;
            int nPowOf2 = 0;

            while (fGain >= 2.0f)
			{ 
				fGain /= 2;
                ++nPowOf2;
			}
            fGain = fGlobalGain - (1 << nPowOf2);
            wValue2 = (unsigned short)((fGain / (( 1 << nPowOf2) / 16.0f)) + 0.5f);
            if (16 == wValue2){
                wValue2 = 0;
                ++nPowOf2;
            }

            if (nPowOf2 > USHRT_MAX){
                wValue = 0xff;
            }else{
                wValue = (nPowOf2 << 4) | wValue2;
            }

			//
            if( SetSensorRegister( H22_SLAVE_ADDR, 0x0000, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        }
		break;

        case ETRONDI_SENSOR_TYPE_OV7740:
            wValue = 0x00;
            fGain = fGlobalGain;
            while (fGain > 2.0f)
            {
                wValue = (wValue << 1) | 0x10;
                fGain /= 2;
            }
            wValue2 = (unsigned short)((fGain-1.0f) * 16 + 0.5f);
            if (wValue2 > 15) wValue2 = 15;
            wValue |= wValue2;
            //
            if( SetSensorRegister( OV4770_SLAVE_ADDR, 0x0000, wValue, FG_Address_1Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        break;


        case ETRONDI_SENSOR_TYPE_AR0135:
        {
            wValue = 0x00;
            fGain = fGlobalGain;
            int nPowOf2 = 0;

            while (fGain >= 8.0f && nPowOf2 < 3)
            {
                fGain /= 2;
                ++nPowOf2;
            }

            wValue = (int)((fGain *  32.0f) + 0.5f);

            if (wValue > 0xff){
                wValue = 0xff;
            }

            //
            unsigned short usValue;
            if(GetSensorRegister(AR0135_SLAVE_ADDR, 0x30B0, &usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            ByteExchange(((unsigned char*)(&usValue))[0], ((unsigned char*)(&usValue))[1]);

            usValue = (usValue & 0xffc0) | (nPowOf2 << 4) | (usValue & 0x0f);
            //
            if (SetSensorRegister(AR0135_SLAVE_ADDR, 0x30B0, usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;

            usValue = wValue;
            if (SetSensorRegister(AR0135_SLAVE_ADDR, 0x305E, usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        }
        break;
		case ETRONDI_SENSOR_TYPE_AR0134:        
			if (fGlobalGain >= 8.0f) 
			{
				fGain = fGlobalGain / 8;
                wValue = (0x80 | 0x30);
			} 
			else if (fGlobalGain >= 4.0f) 
			{
				fGain = fGlobalGain / 4;
                wValue = (0x80 | 0x20);
			} 
			else if (fGlobalGain >= 2.0f) 
			{
				fGain = fGlobalGain / 2;
                wValue = (0x80 | 0x10);
			} 
			else 
			{
				fGain = fGlobalGain / 1;
                wValue = (0x80 | 0x00);
			}

            if( SetSensorRegister(AR0134_SLAVE_ADDR, 0x30B0, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;
			//
			wValue = (int)(fGain * 32 + 0.5f);
			//
            if( SetSensorRegister(AR0134_SLAVE_ADDR, 0x305E, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
				return ETronDI_SETSENSORREG_FAIL;      
		break;
        case ETRONDI_SENSOR_TYPE_AR0144:
        {
            wValue = 0x00;
            fGain = fGlobalGain;
            int nPowOf2 = 0;

            while (fGain >= 16.0f && nPowOf2 < 4)
            {
                fGain /= 2;
                ++nPowOf2;
            }

            wValue = (int)((fGain *  128.0f) + 0.5f);

            if (wValue > 0x7ff){
                wValue = 0x7ff;
            }

            //
            unsigned short usValue;
            if(GetSensorRegister(AR0144_SLAVE_ADDR, 0x3060, &usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
            ByteExchange(((unsigned char*)(&usValue))[0], ((unsigned char*)(&usValue))[1]);

            usValue = (usValue & 0xff80) | (nPowOf2 << 4) | (usValue & 0x0f);
            //
            if (SetSensorRegister(AR0144_SLAVE_ADDR, 0x3060, usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;

            usValue = wValue;
            if (SetSensorRegister(AR0144_SLAVE_ADDR, 0x305E, usValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        }
        break;
        case ETRONDI_SENSOR_TYPE_AR0330:
            if (fGlobalGain >= 8.0f)      { fGain = fGlobalGain / 8; wValue = 0x30; }
            else if (fGlobalGain >= 4.0f) { fGain = fGlobalGain / 4; wValue = 0x20; }
            else if (fGlobalGain >= 2.0f) { fGain = fGlobalGain / 2; wValue = 0x10; }
            else                          { fGain = fGlobalGain / 1; wValue = 0x00; }

            if( SetSensorRegister(AR0330_SLAVE_ADDR, 0x3060, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;

            wValue = (int)(fGain * 128.0f + 0.5f);

            if( SetSensorRegister(AR0330_SLAVE_ADDR, 0x305E, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        break;
        case ETRONDI_SENSOR_TYPE_AR1335:
        case ETRONDI_SENSOR_TYPE_AR0522:
        {
            wValue = 0x0010;
                float digiGain = 1.0f;
                sensorId = (m_stn == ETRONDI_SENSOR_TYPE_AR1335) ? AR1335_SLAVE_ADDR : AR0522_SLAVE_ADDR;
                if (fGlobalGain >= 7.75f)
                {
                    digiGain = fGlobalGain / 7.75f;
                    fGain    = 1.9375f;
                    wValue   = 0x0030;
                }
                else if (fGlobalGain >= 4.0f) { fGain = fGlobalGain / 4; wValue = 0x0030; }
                else if (fGlobalGain >= 2.0f) { fGain = fGlobalGain / 2; wValue = 0x0020; }
                else                          { fGain = fGlobalGain / 1; wValue = 0x0010; }

                wValue |= ( WORD( digiGain * 64 ) << 7 );
                wValue |= ( ( ( BYTE )( fGain * 16 - 16 ) ) & 0x0F );

                if( SetSensorRegister(sensorId, 0x305E, wValue, FG_Address_2Byte | FG_Value_2Byte, nSensorMode) != ETronDI_OK)
                    return ETronDI_SETSENSORREG_FAIL;
            break;
        }
        case ETRONDI_SENSOR_TYPE_OV9714:
            {
                wValue = 0x00;
                fGain = fGlobalGain;
                int nPowOf2 = 0;

                while (fGain >= 2.0f)
                {
                    fGain /= 2;
                    ++nPowOf2;
                }

                fGain = fGlobalGain - (1 << nPowOf2);
                wValue2 = (unsigned short)((fGain / (( 1 << nPowOf2) / 16.0f)) + 0.5f);
                if (16 == wValue2){
                    wValue2 = 0;
                    ++nPowOf2;
                }

                if (nPowOf2 >= 5){
                    wValue = 0xff;
                }else{
                    while (nPowOf2-- > 0) wValue = (wValue << 1) | 0x1;
                    wValue = (wValue << 4) | wValue2;
                }
            }
            //
            if (SetSensorRegister(OV9714_SLAVE_ADDR, 0x350B, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
                return ETronDI_SETSENSORREG_FAIL;
        break;
        case ETRONDI_SENSOR_TYPE_OV9282:

            fGain = fGlobalGain;
            wValue = (int)(fGain * 16);
            //
            if (SetSensorRegister(OV9282_SLAVE_ADDR, 0x3509, wValue, FG_Address_2Byte | FG_Value_1Byte, nSensorMode) != ETronDI_OK)
               return ETronDI_SETSENSORREG_FAIL;

        break;

		default:
			return ETronDI_NotSupport;
	}
	//
	return ETronDI_OK;
}

int CVideoDevice::GetColorGain( int nSensorMode, float *pfGainR, float *pfGainG, float *pfGainB) {
	
    switch (m_devType)
    {
    case AXES1:
        {
            // nSensorMode == 1 is right sensor
            unsigned short wRegR = (nSensorMode != 1) ? 0xF1CC : 0xF28C;
            unsigned short wRegG = (nSensorMode != 1) ? 0xF1CD : 0xF28D;
            unsigned short wRegB = (nSensorMode != 1) ? 0xF1CF : 0xF28F;

            unsigned short wValue = 0;
            if (GetHWRegister(wRegR, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainR = wValue / 64.0f;

            if (GetHWRegister(wRegG, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainG = wValue / 64.0f;

            if (GetHWRegister(wRegB, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainB = wValue / 64.0f;
        }
        break;
    case PUMA:
        {
            // nSensorMode == 1 is right sensor
            unsigned short wRegR_b7b0 = (nSensorMode != 1) ? 0xF1C8 : 0xF288;
            unsigned short wRegR_b9b8 = (nSensorMode != 1) ? 0xF1C9 : 0xF289;
            unsigned short wRegG_b7b0 = (nSensorMode != 1) ? 0xF1CA : 0xF28A;
            unsigned short wRegG_b9b8 = (nSensorMode != 1) ? 0xF1CB : 0xF28B;
            unsigned short wRegB_b7b0 = (nSensorMode != 1) ? 0xF1CC : 0xF28C;
            unsigned short wRegB_b9b8 = (nSensorMode != 1) ? 0xF1CD : 0xF28D;

            unsigned short wValue = 0;
            //R Gain
            if (GetHWRegister(wRegR_b9b8, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainR = (float) wValue * 256;
            if (GetHWRegister(wRegR_b7b0, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainR = *pfGainR + wValue;
            *pfGainR = *pfGainR / 256.0f;

            //G Gain
            if (GetHWRegister(wRegG_b9b8, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainG = (float) wValue * 256;
            if (GetHWRegister(wRegG_b7b0, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainG = *pfGainG + wValue;
            *pfGainG = *pfGainG / 256.0f;

            //B Gain
            if (GetHWRegister(wRegB_b9b8, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainB = (float) wValue * 256;
            if (GetHWRegister(wRegB_b7b0, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            *pfGainB = *pfGainB + wValue;
            *pfGainB = *pfGainB / 256.0f;
        }
        break;
    default:
        return ETronDI_READ_REG_FAIL;
    }

    return ETronDI_OK;
}

int CVideoDevice::SetColorGain( int nSensorMode, float fGainR, float fGainG, float fGainB) {
	
    switch (m_devType)
    {
    case AXES1:
        {
            // nSensorMode == 1 is right sensor
            unsigned short wRegR = (nSensorMode != 1) ? 0xF1CC : 0xF28C;
            unsigned short wRegG = (nSensorMode != 1) ? 0xF1CD : 0xF28D;
            unsigned short wRegG2 = (nSensorMode != 1) ? 0xF1CE : 0xF28E;
            unsigned short wRegB = (nSensorMode != 1) ? 0xF1CF : 0xF28F;

            unsigned short wValue = (unsigned short)(fGainR * 64 + 0.5f);
            if (SetHWRegister(wRegR, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
            //
            wValue = (unsigned short)(fGainG * 64 + 0.5f);
            if (SetHWRegister(wRegG, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
            if (SetHWRegister(wRegG2, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
            //
            wValue = (unsigned short)(fGainB * 64 + 0.5f);
            if (SetHWRegister(wRegB, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
        }
        break;
    case PUMA:
        {
            // nSensorMode == 1 is right sensor
            unsigned short wRegR_b7b0 = (nSensorMode != 1) ? 0xF1C8 : 0xF288;
            unsigned short wRegR_b9b8 = (nSensorMode != 1) ? 0xF1C9 : 0xF289;
            unsigned short wRegG_b7b0 = (nSensorMode != 1) ? 0xF1CA : 0xF28A;
            unsigned short wRegG_b9b8 = (nSensorMode != 1) ? 0xF1CB : 0xF28B;
            unsigned short wRegB_b7b0 = (nSensorMode != 1) ? 0xF1CC : 0xF28C;
            unsigned short wRegB_b9b8 = (nSensorMode != 1) ? 0xF1CD : 0xF28D;

            //R Gain
            unsigned short wValue = (unsigned short)(fGainR * 256 + 0.5f) - (unsigned short)fGainR * 256;
            if (SetHWRegister(wRegR_b7b0, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
            wValue = ((unsigned short)fGainR > 3) ? 3 : (unsigned short)fGainR;
            if (SetHWRegister(wRegR_b9b8, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;

            //G Gain
            wValue = (unsigned short)(fGainG * 256 + 0.5f) - (unsigned short)fGainG * 256;
            if (SetHWRegister(wRegG_b7b0, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
            wValue = ((unsigned short)fGainG > 3) ? 3 : (unsigned short)fGainG;
            if (SetHWRegister(wRegG_b9b8, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;

            //B Gain
            wValue = (unsigned short)(fGainB * 256 + 0.5f) - (unsigned short)fGainB * 256;
            if (SetHWRegister(wRegB_b7b0, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
            wValue = ((unsigned short)fGainB > 3) ? 3 : (unsigned short)fGainB;
            if (SetHWRegister(wRegB_b9b8, wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;

            // Set 0xF103 bit0, bit1 to 1 for sync
            if (GetHWRegister(0xF103, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_READ_REG_FAIL;
            if (SetHWRegister(0xF103, wValue | 0x03, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
        }
        break;
    default:
        return ETronDI_WRITE_REG_FAIL;
    }

    return ETronDI_OK;
}

int CVideoDevice::GetAccMeterValue( int *pX, int *pY, int *pZ) {
	
	unsigned short wValue;
	int v;
	//
	// X Pos
	//
	if (pX) 
	{
		if( GetFWRegister( 0x80, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
		
		v = (wValue&0xFF)<<2;
		
		//
		if( GetFWRegister( 0x81, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
			
		v |= (wValue&0xC0) >> 6;
		
		//
		if (v & 0x200) {
			v -= 1024;
		} 
		//
		*pX = v;
	}
	//
	// Y Pos
	//
	if (pY) 
	{
		if( GetFWRegister( 0x82, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
		v = (wValue&0xFF)<<2;
		//
		if( GetFWRegister( 0x83, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
		v |= (wValue&0xC0) >> 6;
		//
		if (v & 0x200) {
			v -= 1024;
		} 
		//
		*pY = v;
	}
	//
	// Z Pos
	//
	if (pZ) {
		if( GetFWRegister( 0x84, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
		v = (wValue&0xFF)<<2;
		//
		if( GetFWRegister( 0x85, &wValue , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
		v |= (wValue&0xC0) >> 6;
		//
		if (v & 0x200) {
			v -= 1024;
		} 
		//
		*pZ = v;
	}
	//
	return ETronDI_OK;
}

int CVideoDevice::GetAEStatus (PAE_STATUS pAEStatus) {
	
	unsigned short nValue = 0;
	
	if( SetFWRegister( 0xA0, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if( SetFWRegister( 0xA1, 0x23, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;  

	if( GetFWRegister( 0xA2, &nValue, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
		
	if(BIT_CHECK(nValue,0)) *pAEStatus = AE_ENABLE;
	else *pAEStatus = AE_DISABLE;
	
	return ETronDI_OK;
}

int CVideoDevice::GetAWBStatus (PAWB_STATUS pAWBStatus) {
	
	unsigned short nValue = 0;
	
	if( SetFWRegister( 0xA8, 0x0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if( SetFWRegister( 0xA9, 0x0C, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;  

	if( GetFWRegister( 0xAA, &nValue, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
		
	if(BIT_CHECK(nValue,0)) *pAWBStatus = AWB_ENABLE;
	else *pAWBStatus = AWB_DISABLE;
	
	return ETronDI_OK;
}

int CVideoDevice::EnableAE(bool enable)
{
    if (SetFWRegister(0xA0, 0x0, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_WRITE_REG_FAIL;

    if (SetFWRegister(0xA1, 0x23, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_WRITE_REG_FAIL;

    if (SetFWRegister(0xA2, (enable ? 0x1 : 0x0), FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_WRITE_REG_FAIL;

    return ETronDI_OK;
}


int CVideoDevice::EnableAWB(bool enable)
{
    unsigned short wValue = 0;
    if (GetHWRegister(0xF101, &wValue, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_READ_REG_FAIL;

    if (SetFWRegister(0xA8, 0x0, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_WRITE_REG_FAIL;

    if (SetFWRegister(0xA9, 0x0C, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_WRITE_REG_FAIL;

    if (SetFWRegister(0xAA, (enable ? 0x1 : 0x0), FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
        return ETronDI_WRITE_REG_FAIL;

    if (m_devType == PUMA)
    {
        if (enable) // Set 0xF101 bit7 = 1 to enable AWB gain
        {
            if (SetHWRegister(0xF101, wValue | 0x80, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
        }
        else // Set 0xF101 bit7 = 0 to disable AWB gain
        {
            if (SetHWRegister(0xF101, wValue & 0x7F, FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
                return ETronDI_WRITE_REG_FAIL;
        }
    }

    return ETronDI_OK;
}


int CVideoDevice::GetGPIOValue(int nGPIOIndex, BYTE *pValue) {
	
	unsigned short wAddr, wValue;
	//
	switch (nGPIOIndex) {
		
		case 1:		wAddr = 0xF017;		break;
		case 2:		wAddr = 0xF019;		break;
		case 3:
		case 4:
		{
	  		if (m_devType == PUMA)
	  		{
	  			if (nGPIOIndex == 3)
					wAddr = 0xF020;
				else	
					wAddr = 0xF022;
	  		} else
	  			return ETronDI_NotSupport;
    		}
		break;
		default:	return ETronDI_NotSupport;
	}
	
	if( GetHWRegister( wAddr, &wValue , FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
		
	*pValue = (BYTE)(wValue & 0xFF);
	
	return ETronDI_OK; 
}

int CVideoDevice::SetGPIOValue(int nGPIOIndex, BYTE nValue) {
	
	unsigned short wAddr, wValue;
	//
	switch (nGPIOIndex) {
		
		case 1:		wAddr = 0xF017;		break;
		case 2:		wAddr = 0xF019;		break;
		case 3:
		case 4:
		{
        		if (m_devType == PUMA)
	  		{
	  			if (nGPIOIndex == 3)
					wAddr = 0xF020;
				else	
					wAddr = 0xF022;
	  		} else
	  			return ETronDI_NotSupport;
    		}
		break;
		default:	return ETronDI_NotSupport;
	}
	
	wValue = nValue;
	
	if( SetHWRegister( wAddr, wValue , FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
	
	return ETronDI_OK; 
}

int CVideoDevice::SetGPIOCtrl(int nGPIOIndex, BYTE nValue) {
	if (m_devType != PUMA)
		return ETronDI_NotSupport;

	unsigned short wAddr, wValue;
    	switch(nGPIOIndex)
    	{
      	case 1: wAddr = 0xF018; break;
      	case 2: wAddr = 0xF01A; break;
      	case 3: wAddr = 0xF021; break;
      	case 4: wAddr = 0xF023; break;
      	default: return ETronDI_NotSupport;
    	}

	wValue = nValue;
	if (SetHWRegister(wAddr, wValue , FG_Address_2Byte | FG_Value_1Byte) != ETronDI_OK)
	      return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
}

// for AEAWB Control -

// for sensorif +
int CVideoDevice::EnableSensorIF(bool bIsEnable) {
	
	unsigned short wValue;
	
	GetHWRegister( SENIF_EN_REG, &wValue , FG_Address_2Byte | FG_Value_1Byte);

	if(bIsEnable) {
		if( wValue != 0x55 ) SetHWRegister( SENIF_EN_REG, 0x55 , FG_Address_2Byte | FG_Value_1Byte);
	}
	else {    
		if( wValue != 0x0 ) SetHWRegister( SENIF_EN_REG, 0x44 , FG_Address_2Byte | FG_Value_1Byte);
	}
	
	return ETronDI_OK; 
}
// for sensorif -

// for 3D Motor Control +
int CVideoDevice::MotorInit( unsigned short nMinSpeedPerStep) {

	unsigned short value = 0;

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
	return ETronDI_READ_REG_FAIL;

	if(value < 3) {

		if( GetFWRegister( 0x03, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

		if(value<119) m_MinSecPerStep = 20;
		else {
			
			m_MinSecPerStep = nMinSpeedPerStep;
			if( SetFWRegister( 0x9d, m_MinSecPerStep, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
			return ETronDI_WRITE_REG_FAIL;
		}
	}

	else {
		m_MinSecPerStep = nMinSpeedPerStep;
		if( SetFWRegister( 0x9d, m_MinSecPerStep, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK)
		return ETronDI_WRITE_REG_FAIL;
	}

	return ETronDI_OK;
} 

int CVideoDevice::MotorStart ( int motor_id, bool direction, double angle, long step, int timeperstep, float *steppersecond, bool bIsStep, bool bIsTime, bool bIsInfinity) {
  
	if( IsMotorReturnHomeRunning() != ETronDI_OK) return ETronDI_RETURNHOME_RUNNING;
	unsigned short value = 0;
	int i = 0;
	long TotalStep = 0;

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
	return ETronDI_READ_REG_FAIL;
  
	if( value >= 3 ) return ETronDI_ILLEGAL_FIRMWARE_VERSION;
  
	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
    
	if( BIT_CHECK(value,4)) return ETronDI_MOTOR_RUNNING;
  
	// Set motor_id
	if(motor_id==0) BIT_CLEAR(value,5);
	else BIT_SET(value,5);
  
	// Set direction
	if(direction) BIT_CLEAR(value,6);
	else BIT_SET(value,6);

	if( SetFWRegister( 0x82, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
  
	if(bIsStep) {
		
		// Set step
		// Is step legal?
		if( step<0 || step>65535 ) return ETronDI_ILLEGAL_STEP;

		TotalStep = step;
	}
	else {
		
		// Set angle
		// Is angle legal?
		if(!IsAngleLegal(angle)) return ETronDI_ILLEGAL_ANGLE;
		  
		// Angle to Step
		TotalStep = (long)(angle*STEPAMOUNT_PER_ROUND/360);
	}
     
	// Setting 0x9e
	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
  
	for(i=8;i<=15;i++) {
		
		if(BIT_CHECK(TotalStep, i)) BIT_SET(value,i-8);
		else BIT_CLEAR(value,i-8);
	}
  
	if( SetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
  
	// Setting 0x81
	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
  
	for(i=0;i<=7;i++) {
		
		if(BIT_CHECK(TotalStep, i)) BIT_SET(value,i);
		else BIT_CLEAR(value,i);
	}

	if( SetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if(bIsTime) {
		
		// Set time
		// Is time legal?
		timeperstep/=5;
		//if( (timeperstep-m_MinSecPerStep)/2 >= 255 || (timeperstep-m_MinSecPerStep)/2 <0 ) return ETronDI_ILLEGAL_TIMEPERSTEP;
		if( (timeperstep/2-m_MinSecPerStep) >= 255 || (timeperstep/2-m_MinSecPerStep) <0 ) return ETronDI_ILLEGAL_TIMEPERSTEP;
	}
	else {
		
		timeperstep = (int)(1000/(*steppersecond)/5);
		if( (timeperstep/2-m_MinSecPerStep) >= 255 || (timeperstep/2-m_MinSecPerStep) <0 ) return ETronDI_ILLEGAL_STEPPERTIME;
		//*steppersecond = 1000.0f/timeperstep/5.0f;
	}
  
	// Setting 0x83
	if( GetFWRegister( 0x83, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
  
	for(i=0;i<=7;i++)
	{
		if(BIT_CHECK((timeperstep/2-m_MinSecPerStep), i)) BIT_SET(value,i);
		else BIT_CLEAR(value,i);
	}

	if( SetFWRegister( 0x83, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;
    
	// Is Infinity?
	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
    
	if(bIsInfinity) BIT_SET(value,3);
	else BIT_CLEAR(value,3);
    
	if( SetFWRegister( 0x82, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	// Motor On
	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	BIT_SET(value,7);

	if( SetFWRegister( 0x82, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
} 
 
int CVideoDevice::MotorStart_IndexCheckOption( int motor_id, 
											   bool direction, 
											   double angle, 
											   long step, 
											   int timeperstep, 
											   float *steppersecond, 
											   bool bIsStep, 
											   bool bIsTime, 
											   bool bIsInfinity, 
											   bool bIsIgnoreHomeIndexTouched) {
		
	unsigned short value = 0;
	int i = 0;
	long TotalStep = 0;

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( value < 3 ) return ETronDI_ILLEGAL_FIRMWARE_VERSION;

	// check home index is touched +
	if( GetFWRegister( RETURN_HOME_REG, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(bIsIgnoreHomeIndexTouched) {
		BIT_CLEAR(value,2);  
		if( SetFWRegister( RETURN_HOME_REG, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_WRITE_REG_FAIL;
	}
	else {
		BIT_SET(value,2);
		if( SetFWRegister( RETURN_HOME_REG, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_WRITE_REG_FAIL;
		if(BIT_CHECK(value,0)) return ETronDI_MOTOTSTOP_BY_HOME_INDEX;
	}
	// check home index is touched -

	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( BIT_CHECK(value,4) ) 
		return ETronDI_MOTOR_RUNNING;

	// Set motor_id
	if(motor_id==0) BIT_CLEAR(value,5);
	else BIT_SET(value,5);

	// Set direction
	if(direction) BIT_CLEAR(value,6);
	else BIT_SET(value,6);

	if( SetFWRegister( 0x82, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if(bIsStep) {
		
		// Set step
		// Is step legal?
		if( step<0 || step>65535 ) return ETronDI_ILLEGAL_STEP;
		TotalStep = step;
	}
	else {
		// Set angle
		// Is angle legal?
		if(!IsAngleLegal(angle)) return ETronDI_ILLEGAL_ANGLE;
		  
		// Angle to Step
		TotalStep = (long)(angle*STEPAMOUNT_PER_ROUND/360);
	}
	 
	// Setting 0x9e
	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	for(i=8;i<=15;i++)
	{
		if(BIT_CHECK(TotalStep, i)) BIT_SET(value,i-8);
		else BIT_CLEAR(value,i-8);
	}

	if( SetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	// Setting 0x81
	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
	return ETronDI_READ_REG_FAIL;

	for(i=0;i<=7;i++)
	{
		if(BIT_CHECK(TotalStep, i)) BIT_SET(value,i);
		else BIT_CLEAR(value,i);
	}

	if( SetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if(bIsTime) {
		// Set time
		// Is time legal?
		timeperstep/=4;
		//if( (timeperstep-m_MinSecPerStep)/2 >= 255 || (timeperstep-m_MinSecPerStep)/2 <0 ) return ETronDI_ILLEGAL_TIMEPERSTEP;
		if( (timeperstep/2-m_MinSecPerStep) >= 255 || (timeperstep/2-m_MinSecPerStep) <0 ) return ETronDI_ILLEGAL_TIMEPERSTEP;
	}
	else {
		timeperstep = (int)(1000/(*steppersecond)/4);
		if( (timeperstep/2-m_MinSecPerStep) >= 255 || (timeperstep/2-m_MinSecPerStep) <0 ) return ETronDI_ILLEGAL_STEPPERTIME;
		//*steppersecond = 1000.0f/timeperstep/5.0f;
	}

	// Setting 0x83
	if( GetFWRegister( 0x83, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	for(i=0;i<=7;i++)
	{
		if(BIT_CHECK((timeperstep/2-m_MinSecPerStep), i)) BIT_SET(value,i);
		else BIT_CLEAR(value,i);
	}

	if( SetFWRegister( 0x83, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	// Is Infinity?
	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(bIsInfinity) BIT_SET(value,3);
	else BIT_CLEAR(value,3);

	if( SetFWRegister( 0x82, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	// Motor On
	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	BIT_SET(value,7);

	if( SetFWRegister( 0x82, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::MotorStop() {
		
	unsigned short value = 0; 

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( value < 3 ) {
	if( IsMotorReturnHomeRunning() != ETronDI_OK) return ETronDI_RETURNHOME_RUNNING;
	}

	if( SetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, 0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	if( SetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, 0 , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::GetMotorCurrentState( bool* bIsRunning) {
		
	unsigned short value = 0;

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( value >= 3 ) return ETronDI_ILLEGAL_FIRMWARE_VERSION;

	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 4)) *bIsRunning = true;  
	else *bIsRunning = false;

	return ETronDI_OK;
} 

int CVideoDevice::GetMotorCurrentState_IndexCheckOption( bool* bIsRunning, long* nRemainStepNum, int *nHomeIndexNum) {
		
	unsigned short value = 0;

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( value < 3 ) return ETronDI_ILLEGAL_FIRMWARE_VERSION;

	if( GetFWRegister( 0x82, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 4)) {
		
		*bIsRunning = true;
		*nHomeIndexNum = -1;
	}          
	else {
		
		*nHomeIndexNum = 0;
		*bIsRunning = false;
		if( GetFWRegister( RETURN_HOME_REG, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
			return ETronDI_READ_REG_FAIL;
		
		if(BIT_CHECK(value, 0)) {
			
			GetMotorStep(nRemainStepNum);
			return ETronDI_MOTOTSTOP_BY_HOME_INDEX;
		}
		if(BIT_CHECK(value, 1)) {
			
			GetMotorStep(nRemainStepNum);
			return ETronDI_MOTOTSTOP_BY_PROTECT_SCHEME;
		} 
		else return ETronDI_MOTOTSTOP_BY_NORMAL;
	}    

	return ETronDI_OK;
} 

int CVideoDevice::GetMotorAngle( double *angle) {
		
	unsigned short value = 0;
	long nTotalStep = 0;

	// Motor total step
	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 0)) nTotalStep+=256;
	if(BIT_CHECK(value, 1)) nTotalStep+=512; 
	if(BIT_CHECK(value, 2)) nTotalStep+=1024;
	if(BIT_CHECK(value, 3)) nTotalStep+=2048;
	if(BIT_CHECK(value, 4)) nTotalStep+=4096; 
	if(BIT_CHECK(value, 5)) nTotalStep+=8192;
	if(BIT_CHECK(value, 6)) nTotalStep+=16384;
	if(BIT_CHECK(value, 7)) nTotalStep+=32768; 

	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 0)) nTotalStep+=1;
	if(BIT_CHECK(value, 1)) nTotalStep+=2;
	if(BIT_CHECK(value, 2)) nTotalStep+=4;
	if(BIT_CHECK(value, 3)) nTotalStep+=8;
	if(BIT_CHECK(value, 4)) nTotalStep+=16;
	if(BIT_CHECK(value, 5)) nTotalStep+=32;
	if(BIT_CHECK(value, 6)) nTotalStep+=64;
	if(BIT_CHECK(value, 7)) nTotalStep+=128;

	*angle = ((double)nTotalStep)*0.703125f;

	return ETronDI_OK;
} 

int CVideoDevice::GetMotorStep( long *step) {
		
	unsigned short value = 0;
	long nTotalStep = 0;

	// Motor total step
	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_HIGHBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 0)) nTotalStep+=256;
	if(BIT_CHECK(value, 1)) nTotalStep+=512; 
	if(BIT_CHECK(value, 2)) nTotalStep+=1024;
	if(BIT_CHECK(value, 3)) nTotalStep+=2048;
	if(BIT_CHECK(value, 4)) nTotalStep+=4096; 
	if(BIT_CHECK(value, 5)) nTotalStep+=8192;
	if(BIT_CHECK(value, 6)) nTotalStep+=16384;
	if(BIT_CHECK(value, 7)) nTotalStep+=32768; 

	if( GetFWRegister( MOTOR_STEP_NUMBER_REG_LOWBYTE, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 0)) nTotalStep+=1;
	if(BIT_CHECK(value, 1)) nTotalStep+=2;
	if(BIT_CHECK(value, 2)) nTotalStep+=4;
	if(BIT_CHECK(value, 3)) nTotalStep+=8;
	if(BIT_CHECK(value, 4)) nTotalStep+=16;
	if(BIT_CHECK(value, 5)) nTotalStep+=32;
	if(BIT_CHECK(value, 6)) nTotalStep+=64;
	if(BIT_CHECK(value, 7)) nTotalStep+=128;

	*step = nTotalStep;

	return ETronDI_OK;
} 
   
int CVideoDevice::GetMotorTimePerStep( unsigned short *nTimePerStep) {
		
	// Rotation speed
	if( GetFWRegister( 0x83, nTimePerStep , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	//*nTimePerStep *= 2;
	//*nTimePerStep += m_MinSecPerStep; 
	*nTimePerStep += m_MinSecPerStep; 
	*nTimePerStep *= 2;

	unsigned short value = 0;
	
	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK ) 
		return ETronDI_READ_REG_FAIL;

	if(value < 3) {
		*nTimePerStep *= 5;
	}  
	else {
		*nTimePerStep *= 4;
	}       

	return ETronDI_OK;
} 

int CVideoDevice::GetMotorStepPerSecond( float *fpStepPerSecond) {
		
	unsigned short value;
	
	if( GetFWRegister( 0x83, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;
	//value *= 2;
	//value += m_MinSecPerStep;
	value += m_MinSecPerStep; 
	value *= 2; 

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK ) 
		return ETronDI_READ_REG_FAIL; 
		
	if(value < 3) {
		value *= 5;
	}  
	else {
		value *= 4;
	} 

	*fpStepPerSecond = 1000.0f/value;    
	return ETronDI_OK;
} 

int CVideoDevice::GetMotorDirection( bool *direction) {
		
	unsigned short value = 0;
		
	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 6)) *direction = false;   
	else *direction = true;

	return ETronDI_OK;
} 

int CVideoDevice::GetCurrentMotor( int *motor_id) {
		
	unsigned short value = 0;

	if( GetFWRegister( 0x82, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value, 5)) *motor_id = 1;  
	else *motor_id = 0;

	return ETronDI_OK;
} 

// for Gyro +

int CVideoDevice::GyroInit( SENSITIVITY_LEVEL_L3G sll3g, 
			  bool bPowerMode, 
			  bool bIsZEnable, 
			  bool bIsYEnable, 
			  bool bIsXEnable) {
		
	unsigned short value = 0;

	// Set sensitivity
	if( GetSensorRegister( L3G_SLAVE_ADDR, L3G_CTRL_REG4, &value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_GETSENSORREG_FAIL;

	sprintf(m_szDbgStr, "value = %d\r\n", value);   
	//OutputDebugString(g_szDbgStr);

	if(sll3g==DPS_245)
	{
		// 245 dps
		BIT_CLEAR( value,5);
		BIT_CLEAR( value,4);
	}
	else if(sll3g==DPS_500)
	{
		// 500 dps
		BIT_CLEAR( value,5);
		BIT_SET( value,4);
	}
	else if(sll3g==DPS_2000)
	{
		// 2000 dps
		BIT_SET( value,5);
		BIT_CLEAR( value,4);
	}
	else return ETronDI_NotSupport;

	if( SetSensorRegister( L3G_SLAVE_ADDR, L3G_CTRL_REG4, value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_SETSENSORREG_FAIL;

	// Set power mode, x-axis, y-axis, z-axis enable
	if( GetSensorRegister( L3G_SLAVE_ADDR, L3G_CTRL_REG1, &value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_GETSENSORREG_FAIL;

	if(bPowerMode) BIT_SET( value,3);
	else BIT_CLEAR( value,3);

	if(bIsZEnable) BIT_SET( value,2);
	else BIT_CLEAR( value,2);

	if(bIsYEnable) BIT_SET( value,0);
	else BIT_CLEAR( value,0);

	if(bIsXEnable) BIT_SET( value,1);
	else BIT_CLEAR( value,1);

	if( SetSensorRegister( L3G_SLAVE_ADDR, L3G_CTRL_REG1, value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_SETSENSORREG_FAIL;

	return ETronDI_OK;
} 
				   
int CVideoDevice::ReadGyro( GYRO_ANGULAR_RATE_DATA *gard) {
		
	unsigned short nHighByte = 0;
	unsigned short nLowByte = 0; 

	if( GetFWRegister( GYRO_X_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_X_AXIS_FAIL;

	if( GetFWRegister( GYRO_X_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_X_AXIS_FAIL;      

	gard->x = nHighByte*256 + nLowByte;

	if( GetFWRegister( GYRO_Y_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Y_AXIS_FAIL;

	if( GetFWRegister( GYRO_Y_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Y_AXIS_FAIL;  

	gard->y = nHighByte*256 + nLowByte;

	if( GetFWRegister( GYRO_Z_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Z_AXIS_FAIL;

	if( GetFWRegister( GYRO_Z_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Z_AXIS_FAIL;

	gard->z = nHighByte*256 + nLowByte;

	return ETronDI_OK;
} 

int CVideoDevice::ReadGyroData(unsigned short* xValue, unsigned short* yValue, unsigned short* zValue, unsigned short* frameCount)
{
	static BYTE data[8] = { 0 };
	bool bRet = ReadGlobalVariable(0x40, data, 8);
	/* if read failed, keep last value*/
	*xValue = *((unsigned short*)&data[0]);
	*yValue = *((unsigned short*)&data[2]);
	*zValue = *((unsigned short*)&data[4]);
	*frameCount = *((unsigned short*)&data[6]);
        
	return (bRet)? ETronDI_OK: ETronDI_READ_REG_FAIL;
}

int CVideoDevice::ReadFlexibleGyroData(unsigned char *data, int len)
{
	int bRet = ReadSRAM(data, len);

	if (bRet != ETronDI_OK)
		bRet = ETronDI_READ_REG_FAIL;

	return bRet;
}



int CVideoDevice::ReadGlobalVariable(unsigned short addr, BYTE* data, unsigned short dataLen)
{
	int nRet = ETronDI_OK;
    LOGD("CVideoDevice::ReadGlobalVariable enter");
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	const int maxLength = 64;
	unsigned short offset = 0;
	while (offset < dataLen)
	{
		m_FWFSData.nType = FWFS_TYPE_READ_GV;
		m_FWFSData.nOffset = (int)(addr + offset);
		m_FWFSData.nLength = (BYTE)(dataLen - offset > maxLength ? maxLength : dataLen - offset);
  		if (FWFS_Command(&m_FWFSData) < 0) {
            if(m_bIsLogEnabled) LOGE("ReadGlobalVariable::Read Data Error!\n");
  			nRet = ETronDI_READFLASHFAIL;
  			break;	
  		}
		memcpy(data + offset, m_FWFSData.nData, m_FWFSData.nLength);
		
		offset += maxLength;
	}

	return nRet;	
}
//Warning!! GetMutex and ReleaseMutex should not be called separately. They should be called within the same critical section, so that the FW mutex id could not be interrupt.
int CVideoDevice::GetMutex(void)
{
	int nRet = ETronDI_OK;
	CLEAR(m_FWFSData);
	m_FWFSData.nType = FWFS_TYPE_GET_MUTEX;
	FWFS_Command(&m_FWFSData);
	if (m_FWFSData.nMutex == 0) {
        if(m_bIsLogEnabled) LOGE("FWV3ReadData::Get Mutex Error!\n");
		nRet =  ETronDI_READFLASHFAIL;
	}
	return nRet;
}

int CVideoDevice::ReleaseMutex(void)
{
	int nRet = ETronDI_OK;

	m_FWFSData.nType = FWFS_TYPE_RELEASE_MUTEX;
	if(FWFS_Command(&m_FWFSData) < 0) {
        if(m_bIsLogEnabled) LOGE("FWV3ReadData::Release Mutex Error!");
		nRet = ETronDI_READFLASHFAIL;
	}
	return nRet;
}

int CVideoDevice::ReadSRAM(BYTE* data, unsigned short dataLen)
{
	int nRet = ETronDI_OK, count, i, value;

    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	// Get Mutex

	const unsigned long int maxLength = LENGTH_LIMIT;  // Michael Wu: We need fix length
	unsigned short offset = 0;
	for (i = 0; i < (dataLen / maxLength); i++) {
		m_FWFSData.nType = FWFS_TYPE_READ_SRAM;
		m_FWFSData.nOffset = (int)(offset);
		m_FWFSData.nLength = maxLength;
  		if (FWFS_Command(&m_FWFSData) < 0) {
            if(m_bIsLogEnabled) LOGE("ReadSRAM::Read Data Error!\n");
  				nRet = ETronDI_READFLASHFAIL;
  			goto END;
  		}
		//memcpy(data + offset, m_FWFSData.nData, m_FWFSData.nLength);
  		memcpy(data + offset, m_FWFSData.nData, maxLength);
		offset += maxLength;
	}

	m_FWFSData.nOffset = (int)(offset);

END:
	return nRet;
}

// for Gyro -

// for accelerometer and magnetometer +

int CVideoDevice::AccelInit( SENSITIVITY_LEVEL_LSM sllsm) {
		
	unsigned short value = 0;

	if( GetSensorRegister( LSM_SLAVE_ADDR, LSM_CTRL_REG2, &value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_GETSENSORREG_FAIL;
    
	sprintf(m_szDbgStr, "value = %d\r\n", value); 
	//OutputDebugString(g_szDbgStr);

	if(sllsm==_2G)
	{
		// -2~2
		BIT_CLEAR( value,3);
		BIT_CLEAR( value,4);
		BIT_CLEAR( value,5);
	}
	else if(sllsm==_4G)
	{
		// -4~4
		BIT_SET( value,3);
		BIT_CLEAR( value,4);
		BIT_CLEAR( value,5);
	}
	else if(sllsm==_6G)
	{
		// -6~6
		BIT_CLEAR( value,3);
		BIT_SET( value,4);
		BIT_CLEAR( value,5);
	}
	else if(sllsm==_8G)
	{
		// -8~8
		BIT_SET( value,3);
		BIT_SET( value,4);
		BIT_CLEAR( value,5);
	}
	else if(sllsm==_16G)
	{
		// -16~16
		BIT_CLEAR( value,3);
		BIT_CLEAR( value,4);
		BIT_SET( value,5);
	}
	else return ETronDI_NotSupport;

	if( SetSensorRegister( LSM_SLAVE_ADDR, LSM_CTRL_REG2, value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_GETSENSORREG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::ReadAccel( ACCELERATION_DATA *ad) {
		
	unsigned short nHighByte = 0;
	unsigned short nLowByte = 0;

	if( GetFWRegister( ACCELE_X_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_X_AXIS_FAIL;

	if( GetFWRegister( ACCELE_X_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_X_AXIS_FAIL;

	ad->x = nHighByte*256 + nLowByte;

	if( GetFWRegister( ACCELE_Y_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Y_AXIS_FAIL;

	if( GetFWRegister( ACCELE_Y_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Y_AXIS_FAIL;

	ad->y = nHighByte*256 + nLowByte;

	if( GetFWRegister( ACCELE_Z_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Z_AXIS_FAIL;

	if( GetFWRegister( ACCELE_Z_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Z_AXIS_FAIL;

	ad->z = nHighByte*256 + nLowByte;

	return ETronDI_OK;
} 

int CVideoDevice::ReadCompass( COMPASS_DATA *cd) {
		
	unsigned short nHighByte = 0;
	unsigned short nLowByte = 0;

	if( GetFWRegister( MAGNE_X_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_X_AXIS_FAIL;

	if( GetFWRegister( MAGNE_X_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_X_AXIS_FAIL;  

	cd->x = nHighByte*256 + nLowByte;

	if( GetFWRegister( MAGNE_Y_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Y_AXIS_FAIL;

	if( GetFWRegister( MAGNE_Y_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Y_AXIS_FAIL;

	cd->y = nHighByte*256 + nLowByte;

	if( GetFWRegister( MAGNE_Z_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Z_AXIS_FAIL;

	if( GetFWRegister( MAGNE_Z_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_Z_AXIS_FAIL;

	cd->z = nHighByte*256 + nLowByte;

	return ETronDI_OK;
} 

// for accelerometer and magnetometer -

// for pressure +

int CVideoDevice::PsInit( OUTPUT_DATA_RATE odr) {
		
	unsigned short value = 0;

	if( GetSensorRegister( LPS_SLAVE_ADDR, LPS_CTRL_REG1, &value, FG_Address_1Byte | FG_Value_1Byte, 0) != ETronDI_OK)
		return ETronDI_GETSENSORREG_FAIL;

	switch(odr) {
		
		case One_Shot:
			BIT_CLEAR(value,6);
			BIT_CLEAR(value,5);
			BIT_CLEAR(value,4);
		break;
		
		case _1_HZ_1_HZ:
			BIT_CLEAR(value,6);
			BIT_CLEAR(value,5);
			BIT_SET(value,4);
		break;
		
		case _7_HZ_1_HZ:
			BIT_CLEAR(value,6);
			BIT_SET(value,5);
			BIT_CLEAR(value,4);
		break;
		
		case _12_5_HZ_1HZ:
			BIT_CLEAR(value,6);
			BIT_SET(value,5);
			BIT_SET(value,4);
		break;
		
		case _25_HZ_1_HZ:
			BIT_SET(value,6);
			BIT_CLEAR(value,5);
			BIT_CLEAR(value,4);
		break;
		
		case _7_HZ_7_HZ:
			BIT_SET(value,6);
			BIT_CLEAR(value,5);
			BIT_SET(value,4);
		break;
		
		case _12_5_HZ_12_5_HZ:
			BIT_SET(value,6);
			BIT_SET(value,5);
			BIT_CLEAR(value,4);
		break;
		
		case _25_HZ_25_HZ:
			BIT_SET(value,6);
			BIT_SET(value,5);
			BIT_SET(value,4);
		break;
		
		default: 
			return ETronDI_NotSupport;
	}

	if( SetSensorRegister( LPS_SLAVE_ADDR, LPS_CTRL_REG1, value, FG_Address_1Byte | FG_Value_1Byte, 2) != ETronDI_OK)
		return ETronDI_SETSENSORREG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::ReadPressure( int *nPressure) {
		
	unsigned short nHighByte = 0;
	unsigned short nMidByte = 0;
	unsigned short nLowByte = 0;

	if( GetFWRegister( PRESS_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_PRESS_DATA_FAIL;

	if( GetFWRegister( PRESS_DATA_MIDD, &nMidByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_PRESS_DATA_FAIL;

	if( GetFWRegister( PRESS_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_PRESS_DATA_FAIL;

	*nPressure = nHighByte*65536 + nMidByte*256 + nLowByte;

	return ETronDI_OK;    
} 

int CVideoDevice::ReadTemperature( short *nTemperature) {
		
	unsigned short nHighByte = 0;
	unsigned short nLowByte = 0;

	if( GetFWRegister( TEMP_DATA_LOW, &nLowByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_TEMPERATURE_FAIL;

	if( GetFWRegister( TEMP_DATA_HIGH, &nHighByte , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_TEMPERATURE_FAIL;

	*nTemperature = nHighByte*256 + nLowByte;
	
	return ETronDI_OK; 
} 

// for pressure -

// for LED and Laser +

int CVideoDevice::SetLaserPowerState( POWER_STATE ps) {
		
	unsigned short value; 

	if( GetFWRegister( LASER_LED_POWER_CTRL, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( ps == POWER_ON ) BIT_SET( value,0); 
	else BIT_CLEAR( value,0);  

	if( SetFWRegister( LASER_LED_POWER_CTRL, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::SetDesktopLEDPowerState( POWER_STATE ps) {
		
	unsigned short value; 

	if( GetFWRegister( LASER_LED_POWER_CTRL, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( ps == POWER_ON ) BIT_CLEAR( value,1); 
	else BIT_SET( value,1);  

	if( SetFWRegister( LASER_LED_POWER_CTRL, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::GetLaserPowerState( POWER_STATE *ps) {
		
	unsigned short value; 

	if( GetFWRegister( LASER_LED_POWER_CTRL, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value,0)) *ps = POWER_ON;
	else *ps = POWER_OFF;   

	return ETronDI_OK;
} 

int CVideoDevice::GetDesktopLEDPowerState( POWER_STATE *ps) {
		
	unsigned short value; 

	if( GetFWRegister( LASER_LED_POWER_CTRL, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if(BIT_CHECK(value,1)) *ps = POWER_OFF;
	else *ps = POWER_ON;   

	return ETronDI_OK;
} 

int CVideoDevice::SetMobileLEDBrightnessLevel( BRIGHTNESS_LEVEL bl) {
		
	unsigned short value = (unsigned short)bl;

	if( SetFWRegister( LED_BRIGHTNESS_CTRL, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
} 

int CVideoDevice::GetMobileLEDBrightnessLevel( BRIGHTNESS_LEVEL *pbl) {
		
	unsigned short value = 0;

	if( GetFWRegister( LED_BRIGHTNESS_CTRL, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;  

	*pbl = (BRIGHTNESS_LEVEL)value;

	return ETronDI_OK;
} 

// for LED and Laser -

// Return home +
int CVideoDevice::MotorStartReturnHome() {
		
	unsigned short value = 0; 

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	if( value >= 3 ) return ETronDI_ILLEGAL_FIRMWARE_VERSION;

	if( GetFWRegister( RETURN_HOME_REG, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_READ_REG_FAIL;

	BIT_SET( value,0);  

	if( SetFWRegister( RETURN_HOME_REG, value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
		return ETronDI_WRITE_REG_FAIL;

	return ETronDI_OK;
}

int CVideoDevice::IsMotorReturnHomeRunning() {
	
	unsigned short value = 0; 

	if( GetFWRegister( 0x01, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
	return ETronDI_READ_REG_FAIL;

	if( value >= 3 ) return ETronDI_ILLEGAL_FIRMWARE_VERSION;

	if( GetFWRegister( 0x03, &value, FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
	return ETronDI_READ_REG_FAIL;

	if( value < 125 ) return ETronDI_OK;

	if( GetFWRegister( RETURN_HOME_REG, &value , FG_Address_1Byte | FG_Value_1Byte) != ETronDI_OK )
	return ETronDI_READ_REG_FAIL;

	if( value == 0xff ) return ETronDI_OK;

	if(BIT_CHECK(value,1)) return ETronDI_RETURNHOME_RUNNING;

	return ETronDI_OK;
}
// Return home -

// for 3D Motor Control -

int AppendToFileFront(const char* szFilePath, BYTE* pbuffer, int nBufferLen) 
{
    if (pbuffer == NULL)
    {
        return -1;
    }

    std::fstream file(szFilePath, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
    if (file.good())
    {
        size_t fileSize = file.tellg();
        char* buf = new char[nBufferLen + fileSize];
        
        memcpy(buf, pbuffer, nBufferLen);
        file.seekg(0, std::ios::beg);
        file.read(buf + nBufferLen, fileSize);
        
        file.seekp(0, std::ios::beg);
        file.write(buf, nBufferLen + fileSize);

        delete [] buf;

        file.close();
        return 0;
    }

    return -1;
}
#ifndef TINY_VERSION
int CVideoDevice::GenerateLutFile(const char* filename)
{
    LOGV("CVideoDevice::GenerateLutFile %s", filename);
	eys::fisheye360::ParaLUT paraLut;
	int ret = GetUserData((BYTE*)(&paraLut), sizeof(eys::fisheye360::ParaLUT), USERDATA_SECTION_0);
	if (ret != ETronDI_OK)
	{
		return ret;
	}
	//Check LUT file. If LUT file is existed and belong to the device, use old LUT file.
	bool bIsCreateLutTable = false;

	if (paraLut.file_ID_header != 2230  || paraLut.file_ID_version < 4) {
		fprintf(stderr, "Error : invalid File ID \n");
		return ETronDI_MAP_LUT_FAIL; //EYS_ParaLUT_ERROR;
	}
	FILE *fp = fopen(filename, "rb");
	if(fp){
		BYTE *pBuffer2 = (BYTE*)calloc(sizeof(eys::fisheye360::ParaLUT), sizeof(BYTE));
		fread(pBuffer2, sizeof(BYTE),sizeof(eys::fisheye360::ParaLUT), fp);
		if(memcmp(&paraLut, pBuffer2, sizeof(eys::fisheye360::ParaLUT)) != 0) {
			fprintf(stderr,"File exist but content is different ..");
			bIsCreateLutTable = true;
        }
        free(pBuffer2);
        fclose(fp);
    } else {
       	fprintf(stderr,"LUT File %s doesn't exist ..\n", filename);
       	bIsCreateLutTable = true;
	}
	if(!bIsCreateLutTable) {
        fprintf(stderr, "LUT existed.\n");
        return ETronDI_OK;
	}


	if (eys::fisheye360::Map_LUT_CC(paraLut, filename, nullptr, nullptr, 
			eys::LUT_LXLYRXRY_16_3, true) != 1) 
	{
        	return ETronDI_MAP_LUT_FAIL;
	}

	if(AppendToFileFront(filename, (BYTE*)(&paraLut), sizeof(eys::fisheye360::ParaLUT)) != 0) 
	{
        	return ETronDI_APPEND_TO_FILE_FRONT_FAIL;
	}

	return ETronDI_OK;
}
int CVideoDevice::SaveLutData(const char* filename)
{
	eys::fisheye360::ParaLUT paraLut;
    LOGD("SaveLutData %s\n", filename);

	int ret = GetUserData((BYTE*)(&paraLut), sizeof(eys::fisheye360::ParaLUT), USERDATA_SECTION_0);
	if (ret != ETronDI_OK)
	{
		return ret;
	}
	//Check LUT file. If LUT file is existed and belong to the device, use old LUT file.
	bool bIsCreateLutTable = false;

	if (paraLut.file_ID_header != 2230  || paraLut.file_ID_version < 4) {
		fprintf(stderr, "Error : invalid File ID \n");
		return ETronDI_MAP_LUT_FAIL; //EYS_ParaLUT_ERROR;
	}
	FILE *fp = fopen(filename, "rb");
	if(fp){
		BYTE *pBuffer2 = (BYTE*)calloc(sizeof(eys::fisheye360::ParaLUT), sizeof(BYTE));
		fread(pBuffer2, sizeof(BYTE),sizeof(eys::fisheye360::ParaLUT), fp);
		if(memcmp(&paraLut, pBuffer2, sizeof(eys::fisheye360::ParaLUT)) != 0) {
			fprintf(stderr,"File exist but content is different ..");
			bIsCreateLutTable = true;
        }
        free(pBuffer2);
        fclose(fp);
    } else {
       	fprintf(stderr,"LUT File %s doesn't exist ..\n", filename);
       	bIsCreateLutTable = true;
	}
	if(!bIsCreateLutTable) {
        fprintf(stderr, "LUT existed.\n");
        return ETronDI_OK;
	}
	fp = fopen(filename, "wb");
	int nRet = fwrite(&paraLut, sizeof(BYTE),sizeof(eys::fisheye360::ParaLUT), fp);
	fclose(fp);
	return (nRet == sizeof(eys::fisheye360::ParaLUT))? ETronDI_OK: ETronDI_MAP_LUT_FAIL;
}
int CVideoDevice::EncryptMp4(const char *filename)
{
	return En_DecryptionMP4(filename, true);
}

int CVideoDevice::DecryptMp4(const char *filename)
{
	return En_DecryptionMP4(filename, false);
}

int CVideoDevice::InjectExtraDataToMp4(const char* filename, const char* data, int dataLen)
{
	std::vector<char> injectData(dataLen, 0);
	memcpy(&injectData[0], data, dataLen);
	if (!AccessMp4FileExtraData(filename, injectData, true))
	{
		return ETronDI_ACCESS_MP4_EXTRA_DATA_FAIL;
	}
	
	return ETronDI_OK;
}

int CVideoDevice::RetrieveExtraDataFromMp4(const char* filename, char* data, int* dataLen)
{
	std::vector<char> injectData;
	if (!AccessMp4FileExtraData(filename, injectData, false))
	{
		return ETronDI_ACCESS_MP4_EXTRA_DATA_FAIL;
	}
	
	if (*dataLen < (int)injectData.size())
	{
		*dataLen = (int)injectData.size();
		return ETronDI_ErrBufLen;
	}
	
	*dataLen = (int)injectData.size();
	memcpy(data, &injectData[0], injectData.size());
	
	return ETronDI_OK;
}


int CVideoDevice::EncryptString(const char* src, char* dst)
{
	CDataEncrypt dataEncrypt(encryptToken);
	std::string from(src);
	std::string to;
	if (dataEncrypt.Encrypt(from, to))
	{
		strcpy(dst, to.c_str());
		return ETronDI_OK;
	}
	
	return ETronDI_RET_BAD_PARAM;
}

int CVideoDevice::DecryptString(const char* src, char* dst)
{
	CDataEncrypt dataEncrypt(encryptToken);
	std::string from(src);
	std::string to;
	if (dataEncrypt.Decrypt(from, to))
	{
		strcpy(dst, to.c_str());
		return ETronDI_OK;
	}
	
	return ETronDI_RET_BAD_PARAM;
}

int CVideoDevice::EncryptString(const char* src1, const char* src2, char* dst)
{
	CDataEncrypt dataEncrypt(encryptToken);
	std::string from1(src1), from2(src2), to;
	if (dataEncrypt.Encrypt(from1, from2, to))
	{
		strcpy(dst, to.c_str());
		return ETronDI_OK;
	}
	
	return ETronDI_RET_BAD_PARAM;
}

int CVideoDevice::DecryptString(const char* src, char* dst1, char* dst2)
{
	CDataEncrypt dataEncrypt(encryptToken);
	std::string from(src);
	std::string to1, to2;
	if (dataEncrypt.Decrypt(from, to1, to2))
	{
		strcpy(dst1, to1.c_str());
		strcpy(dst2, to2.c_str());
		return ETronDI_OK;
	}
	
	return ETronDI_RET_BAD_PARAM;
}
#endif
// private +

bool CVideoDevice::IsPlugInVersionNew(bool *bIsNewVersionPlugIn) {
	
	unsigned short value = 0;

	if( GetFWRegister( 0x04, &value, FG_Value_1Byte | FG_Value_1Byte) != ETronDI_OK ) return false; 
	 
	if(value>=4) *bIsNewVersionPlugIn = true;
	else *bIsNewVersionPlugIn = false;

	return true;
}


int CVideoDevice::FWV3ReadData(unsigned char* Data, int nID, unsigned long int ReadLen) {
	
	unsigned long int nOffset = 0;
    unsigned long int nLength = 0;
    int nRet = ETronDI_OK;
    
    if(Data == NULL) return ETronDI_NullPtr;
    
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	
	if(m_FWFSData.nMutex == 0)
	{	
          if(m_bIsLogEnabled) LOGE("FWV3ReadData::Get Mutex Error!\n");
		  return ETronDI_READFLASHFAIL;
	}
	
	int nMutex = m_FWFSData.nMutex;
	// Read FileSystem Data, once 64 bytes until all readed
	//
	nOffset = 0;
  	while (nOffset < ReadLen) {
  		nLength = ReadLen - nOffset;
  		if (nLength > LENGTH_LIMIT) nLength = LENGTH_LIMIT;
  		//
  		m_FWFSData.nType = FWFS_TYPE_READ_FS;
  		m_FWFSData.nFsID = (unsigned char)nID;
  		m_FWFSData.nOffset = nOffset;
  		m_FWFSData.nLength = nLength;
  		if (FWFS_Command(&m_FWFSData) < 0) {
			
            if(m_bIsLogEnabled) LOGE("FWV3ReadData::Read Data Error!\n");
            LOGW(" read error, offset is %d", nOffset);
  			nRet = ETronDI_READFLASHFAIL;
  			break;
  		}
  		//
  		memcpy(Data+nOffset, m_FWFSData.nData, nLength);
  		nOffset += nLength;
  	}
	
	return nRet;
}

int CVideoDevice::FWV3WriteData(unsigned char* Data, int nID, unsigned long int WriteLen) {
	
	unsigned long int nOffset = 0;
    unsigned long int nLength = 0;
    int nRet = ETronDI_OK;
    
    if(Data == NULL) return ETronDI_NullPtr;
    
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
 	//
	// Write FileSystem Data, once 64 byte until all readed
	nOffset = 0;
  	while (nOffset < WriteLen) 
    {
  		nLength = WriteLen - nOffset;
  		if (nLength > LENGTH_LIMIT) nLength = LENGTH_LIMIT;
  		//
  		m_FWFSData.nType = FWFS_TYPE_WRITE_FS;
  		m_FWFSData.nFsID = (unsigned char)nID;
  		m_FWFSData.nOffset = nOffset;
  		m_FWFSData.nLength = nLength;
  		memcpy(m_FWFSData.nData, Data+nOffset, nLength);
  		if (FWFS_Command(&m_FWFSData) < 0) {
			
          if(m_bIsLogEnabled) LOGE("FWV3WriteData::Write Data Error!\n");
          nRet = ETronDI_WRITEFLASHFAIL;
          break;
  		}
  		//
  		nOffset += nLength;
  	}
	//
  	// Write Sync
  	m_FWFSData.nType = FWFS_TYPE_WRITE_SYNC;
  	FWFS_Command(&m_FWFSData);
	
	return nRet;
}

int CVideoDevice::GetPropertyValue( unsigned short address, unsigned short* pValue, int control, int flag) {
	
	struct uvc_xu_control_query xqry;
	int nRet = ETronDI_OK;
	unsigned char SetupPacket[64] = {0};

    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

	if(control == T_ASIC) {
        *(SetupPacket) = 0x00;
		if(flag & 0x02) { // Check the address is 2 bytes
			*(SetupPacket) |= 0x02;
            *(SetupPacket+1) = (address >> 8) ;
			*(SetupPacket+2) = (address & 0xff);

		}
		else {
            *(SetupPacket+1) = (address & 0xff);
		}
	}

	if(control==T_FIRMWARE) {
        *(SetupPacket) = 0x20;
        *(SetupPacket+1) = (address & 0xff);
		control = 3;
	}

	if(control == T_I2C) {
		*(SetupPacket) = 0x80;
		*(SetupPacket+1) = 0x42;
		*(SetupPacket+2) = address;
	}

    *(SetupPacket) |= 0x80;

	xqry.unit = ExtenID;
	xqry.selector = control;
	xqry.size = 4;  //register size =3
	xqry.data = (__u8*)malloc(64);
	memcpy(xqry.data, SetupPacket, 59);

	xqry.query = UVC_SET_CUR;

    if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
        IOCTL_ERROR_LOG();
        nRet = ETronDI_READ_REG_FAIL;
	}
	else {
		xqry.query = UVC_GET_CUR;
		//memset(xqry.data, 0x00, 5);

        if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
            IOCTL_ERROR_LOG();
            nRet = ETronDI_READ_REG_FAIL;
		}
		else {
			*pValue = (int)xqry.data[1];
		}
	}
	
	if(xqry.data != NULL) {
		
		free(xqry.data);
		xqry.data = NULL;
	}
	
	return nRet;	
}

int CVideoDevice::SetPropertyValue( unsigned short address, unsigned short nValue, int control, int flag) {
	
    struct uvc_xu_control_query xqry;
	int nRet = ETronDI_OK;
	unsigned char SetupPacket[64] = {0};
    LOGD("SetPropertyValue control=%d value=%d\n", control, nValue);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	
    if(control == T_ASIC)
    {
        *(SetupPacket) = 0x00;
        if(flag & 0x02)  { // Check the address is 2 bytes		
			*(SetupPacket) |= 0x02;
            *(SetupPacket+1) = (address >> 8);
			*(SetupPacket+2) = (address & 0xff);			
            *(SetupPacket+3) = (nValue & 0xff);
		}
		else {
            *(SetupPacket+1) = (address & 0xff);
            *(SetupPacket+2) = (nValue & 0xff);
		}
    }
    else if(control==T_FIRMWARE) {
        *(SetupPacket) = 0x20;
        *(SetupPacket+1) = (address & 0xff);
        *(SetupPacket+2) = (nValue & 0xff);
        control = 3;
    }

	xqry.unit = ExtenID;
	xqry.selector = control;
	xqry.size = 4;  //register size =3
	xqry.data = (__u8*)malloc(64);
	memcpy(xqry.data, SetupPacket, 59);

	xqry.query = UVC_SET_CUR;

    if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
        IOCTL_ERROR_LOG();
        nRet = ETronDI_WRITE_REG_FAIL;
	}
	else {
		xqry.query = UVC_GET_CUR;
		//memset(xqry.data, 0x00, 5);

        if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
            IOCTL_ERROR_LOG();
            nRet = ETronDI_WRITE_REG_FAIL;
		}
		//else {
			//nValue = (int)xqry.data[1];
		//}
	}
	
	if(xqry.data != NULL) {
		
		free(xqry.data);
		xqry.data = NULL;
	}
	
	return nRet;
}

int CVideoDevice::GetPropertyValue_I2C( int senaddr, unsigned short address, unsigned short* pValue, int control, int flag, int nSensorMode) {
	
    struct uvc_xu_control_query xqry;
	int nRet = ETronDI_OK;
	unsigned char SetupPacket[64] = {0};
    
    //Initial value of the byte 0
    *(SetupPacket) = 0x01;
    *(SetupPacket+1)=senaddr;

    if(flag & 0x02)  // Check the address is 2 bytes
    {
        *(SetupPacket) |= 0x02;
        *(SetupPacket+2) = (address >> 8);
        *(SetupPacket+3) = (address & 0xff);        
    }
    else
    {
        *(SetupPacket+2) = (address & 0xff);
    }

    if(flag & 0x20) // Check the value is 2 bytes
    {
        *(SetupPacket) |= 0x04;
    }
    
    if(nSensorMode == 0) *(SetupPacket) |= 0x00;
	else if(nSensorMode == 1) *(SetupPacket) |= 0x08;
    else if(nSensorMode == 3) *(SetupPacket) |= 0x20;
    else if(nSensorMode == 4) *(SetupPacket) |= 0x28;
	else *(SetupPacket) |= 0x18;

    *(SetupPacket) |= 0x80;

    xqry.unit = ExtenID;
	xqry.selector = control;
    xqry.size = 6;  //register size =3
	xqry.data = (__u8*)malloc(64);
	memcpy(xqry.data, SetupPacket, 59);

	xqry.query = UVC_SET_CUR;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

	if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
        IOCTL_ERROR_LOG();
        nRet = ETronDI_READ_REG_FAIL;
	}
	else {
		xqry.query = UVC_GET_CUR;
		//memset(xqry.data, 0x00, 5);

		if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
            IOCTL_ERROR_LOG();
            nRet = ETronDI_READ_REG_FAIL;
		}
        else
        {
			if(flag & 0x20) { // Check the value is 2 bytes			
				*pValue = int(xqry.data[2]);
				*pValue |= (xqry.data[1] << 8);
			}
			else {
                *pValue = int(xqry.data[1]);
            }

//            int check = xqry.data[0];
//            if(check & 0x08) {
//                nRet = ETronDI_READ_REG_FAIL;
//            }
		}
    }
    
    if(xqry.data != NULL) {
		
		free(xqry.data);
		xqry.data = NULL;
	}
    
    return nRet;
}

int CVideoDevice::SetPropertyValue_I2C( int senaddr, unsigned short address, unsigned short nValue,  int control, int flag, int nSensorMode) {
	
    struct uvc_xu_control_query xqry;
    int nRet = ETronDI_OK;
	unsigned char SetupPacket[64]={0};
    int idx;

	//Initial value of the byte 0
    *(SetupPacket) = 0x01;
	*(SetupPacket+1) = senaddr;

    if(flag & 0x02) { // Check the address is 2 bytes    
        *(SetupPacket) |= 0x02;
        *(SetupPacket+2) = (address >> 8);
        *(SetupPacket+3) = (address & 0xff);
        idx = 4;
    }
    else {
        *(SetupPacket+2) = (address & 0xff);
        idx = 3;
    }

    if(flag & 0x20) { // Check the value is 2 bytes
        *(SetupPacket) |= 0x04;
        *(SetupPacket+idx) = (nValue >> 8) ;
        *(SetupPacket+idx+1) = (nValue & 0xff);
    }
    else {
        *(SetupPacket+idx) = (nValue & 0xff);
    }
    
    if(nSensorMode==0) *(SetupPacket) |= 0x00;
	else if(nSensorMode == 1) *(SetupPacket) |= 0x08;
	else if(nSensorMode == 3) *(SetupPacket) |= 0x20;
	else if(nSensorMode == 4) *(SetupPacket) |= 0x28;
	else *(SetupPacket) |= 0x18;

    xqry.unit = ExtenID;
	xqry.selector = control;
    xqry.size = 6;
	xqry.data = (__u8*)malloc(64);
	memcpy(xqry.data, SetupPacket, 59);

	xqry.query = UVC_SET_CUR;

    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

    if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
        IOCTL_ERROR_LOG();
        nRet = ETronDI_WRITE_REG_FAIL;
	}
	else {

		xqry.query = UVC_GET_CUR;
		//memset(xqry.data, 0x00, 5);

        if ( ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0) {
            IOCTL_ERROR_LOG();
            nRet = ETronDI_WRITE_REG_FAIL;
		}
//		else {
//			int check = xqry.data[0];
//			if(check & 0x08) {
//				nRet = ETronDI_WRITE_REG_FAIL;;
//			}
//		}
	}
    
    if(xqry.data != NULL) {
		
		free(xqry.data);
		xqry.data = NULL;
	}
    
    return nRet;
}

int CVideoDevice::FWFS_Command( FWFS_DATA *pFWFSData) {	
	
	struct uvc_xu_control_query xqry;
    int err;
    const unsigned short DATA_SIZE = 256;
    __u8 data[DATA_SIZE] = {0};
    xqry.data = data;
    
    switch(pFWFSData->nType) {
		
		case FWFS_TYPE_GET_MUTEX:
		    xqry.query = UVC_GET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0a;
            xqry.size = 1;
		
            if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                IOCTL_ERROR_LOG();
                err = -1;
			}
			else {
				pFWFSData->nMutex = xqry.data[0];
			}
		break;
		
		case FWFS_TYPE_RELEASE_MUTEX:
		    xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0a;
            xqry.size = 1;
			
            xqry.data[0] = pFWFSData->nMutex;
			
            if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                LOGE("ioctl set_cur control error\n");
                IOCTL_ERROR_LOG();
				err = -1;
			}
			pFWFSData->nMutex = 0;
		break;

		case FWFS_TYPE_READ_GV:
			xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
			xqry.size = 16;

			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x40 | 0x01;	//b[7:6]=1 Read, b[5:0]=1 Memory I/O Command
			xqry.data[2] = 0x0A;	//FW Global Variable
			xqry.data[3] = 0;
			xqry.data[4] = 0;
			xqry.data[5] = 0;
			xqry.data[6] = (unsigned char)((pFWFSData->nOffset >> 8) & 0xFF);	//Offset-H
			xqry.data[7] = (unsigned char)(pFWFSData->nOffset & 0xFF);		//Offset-L
			xqry.data[8] = pFWFSData->nLength/256%256;
			xqry.data[9] = pFWFSData->nLength%256;
            if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                if(m_bIsLogEnabled) LOGE("ioctl set_cur control error\n");
                IOCTL_ERROR_LOG();
				err = -1;
			} else {
				xqry.query = UVC_GET_CUR;
				xqry.unit = ExtenID;
                xqry.selector = 0x0c;
                xqry.size = 32;
                memset(xqry.data, 0, DATA_SIZE);
                if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                    if(m_bIsLogEnabled) LOGE("ioctl get_cur control error\n");
                    IOCTL_ERROR_LOG();
					err = -1;
				} else {
					memcpy(pFWFSData->nData, xqry.data, pFWFSData->nLength);
				}
			}
			break;

		case FWFS_TYPE_READ_SRAM:
			xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
			xqry.size = 16;

			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x40 | 0x01;     //b[7:6]=1 Read, b[5:0]=1 Memory I/O Command
			xqry.data[2] = 0x01;    //SRAM
			xqry.data[3] = 0x01;
			xqry.data[4] = 0;
			xqry.data[5] = 0;
			xqry.data[6] = 0x5E;       //Offset-H
			xqry.data[7] = (unsigned char)(pFWFSData->nOffset & 0xFF);              //Offset-L
			xqry.data[8] = 0x00;
			xqry.data[9] = LENGTH_LIMIT;
            if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                    if(m_bIsLogEnabled) LOGE("ioctl set_cur control error\n");
                    IOCTL_ERROR_LOG();
					err = -1;
			} else {
					xqry.query = UVC_GET_CUR;
					xqry.unit = ExtenID;
					xqry.selector = 0x0c;
					xqry.size = pFWFSData->nLength;
                    memset(xqry.data, 0, DATA_SIZE);
					if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                            if(m_bIsLogEnabled) LOGE("ioctl get_cur control error\n");
							err = -1;
					} else {
							memcpy(pFWFSData->nData, xqry.data, pFWFSData->nLength);
					}
			}
			break;
		case FWFS_TYPE_WRITE_GV:
            LOGD("TODO: not implememt\n");
			err = -1;
			break;

		case FWFS_TYPE_WRITE_SYNC:
		    xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
            xqry.size = 16;
			
			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x00 | 0x02;	  //b[7:6]=0 No Data, b[5:0]=2 Write-Sync
			xqry.data[2] = 0x01;		  //SyncValue=1
			xqry.data[3] = 0;			
			xqry.data[4] = 0;
			xqry.data[5] = 0;
			xqry.data[6] = 0;
			xqry.data[7] = 0;
			
			// ignore this err
			/*
			if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
				cout << "ioctl set_cur control error" << endl;
				err = ET_FAIL;
			}
			*/
			
            if (ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry) < 0){
                IOCTL_ERROR_LOG();
            }
			err = 0;			
		break;
		
		case FWFS_TYPE_WRITE_FS:
		    xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
            xqry.size = 16;
			
			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x80 | 0x01;	  //b[7:6]=2 Write, b[5:0]=1 Memory I/O Command
			xqry.data[2] = 0x05;		  //FileSystem Table
            xqry.data[3] = 1;
			xqry.data[4] = 0;
			xqry.data[5] = pFWFSData->nFsID;	//FileSystem ID
			xqry.data[6] = (unsigned char)((pFWFSData->nOffset >> 8) & 0xFF);	//Offset-H
			xqry.data[7] = (unsigned char)(pFWFSData->nOffset & 0xFF);		//Offset-L
			xqry.data[8] = pFWFSData->nLength/256%256;
			xqry.data[9] = pFWFSData->nLength%256;
			
			if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                if(m_bIsLogEnabled) LOGE("ioctl set_cur control error\n");
                IOCTL_ERROR_LOG();
				err = -1;
			}
			else {
				xqry.query = UVC_SET_CUR;
				xqry.unit = ExtenID;
				xqry.selector = 0x0c;
                xqry.size = 32;
                memset(xqry.data, 0, DATA_SIZE);
				memcpy(xqry.data, pFWFSData->nData, pFWFSData->nLength);
				
				if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                    if(m_bIsLogEnabled) LOGE("ioctl set_cur control error\n");
                    IOCTL_ERROR_LOG();
					err = -1;
				}
			}
		break;
		case FWFS_TYPE_READ_FS:
		    xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
            xqry.size = 16;
			
			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x40 | 0x01;	//b[7:6]=1 Read, b[5:0]=1 Memory I/O Command
			xqry.data[2] = 0x05;			//FileSystem Table
            xqry.data[3] = 1;
			xqry.data[4] = 0;
			xqry.data[5] = pFWFSData->nFsID;	//FileSystem ID
			xqry.data[6] = (unsigned char)((pFWFSData->nOffset >> 8) & 0xFF);	//Offset-H
			xqry.data[7] = (unsigned char)(pFWFSData->nOffset & 0xFF);		//Offset-L
			xqry.data[8] = pFWFSData->nLength/256%256;
			xqry.data[9] = pFWFSData->nLength%256;
			
			if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                if(m_bIsLogEnabled) LOGE("ioctl set_cur control error\n");
                IOCTL_ERROR_LOG();
				err = -1;
			} else {
				xqry.query = UVC_GET_CUR;
				xqry.unit = ExtenID;
				xqry.selector = 0x0c;
                xqry.size = pFWFSData->nLength;
                memset(xqry.data, 0, DATA_SIZE);
				
				if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                    if(m_bIsLogEnabled) LOGE("ioctl get_cur control error\n");
                    IOCTL_ERROR_LOG();
					err = -1;
				} else {
					memcpy(pFWFSData->nData, xqry.data, pFWFSData->nLength);
				}
			}
		break;
		
		case FWFS_TYPE_GET_FS_LENGTH:
			break;
		
		case FWFS_TYPE_WRITE_FLASH:
		    xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
            xqry.size = 16;
			
			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x80 | 0x01;	//b[7:6]=2 Write, b[5:0]=1 Memory I/O Command
			xqry.data[2] = 0x03;			////SPI-Flash
			xqry.data[3] = 1;
			xqry.data[4] = pFWFSData->nOffset/256/256/256;
			xqry.data[5] = pFWFSData->nOffset/256/256%256;
			xqry.data[6] = pFWFSData->nOffset/256%256;
			xqry.data[7] = pFWFSData->nOffset%256;
			xqry.data[8] = pFWFSData->nLength/256%256;
			xqry.data[9] = pFWFSData->nLength%256;
			
			if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                perror("ioctl set_cur control error ---1");
                IOCTL_ERROR_LOG();
				err = -1;
			} else {
				xqry.query = UVC_SET_CUR;
				xqry.unit = ExtenID;
				xqry.selector = 0x0c;
                xqry.size = 32;
                memset(xqry.data, 0, DATA_SIZE);
				memcpy(xqry.data, pFWFSData->nData, pFWFSData->nLength);
				
				if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                    perror("ioctl set_cur control error ---2");
                    IOCTL_ERROR_LOG();
					err = -1;
                } else {
                    LOGI("write success ....\n");
                }
			}
		break;
		case FWFS_TYPE_READ_FLASH:
		    xqry.query = UVC_SET_CUR;
			xqry.unit = ExtenID;
			xqry.selector = 0x0b;
            xqry.size = 16;
			
			xqry.data[0] = pFWFSData->nMutex;
			xqry.data[1] = 0x40 | 0x01;	//b[7:6]=2 Write, b[5:0]=1 Memory I/O Command
			xqry.data[2] = 0x03;			//SPI-Flash
			xqry.data[3] = 1;
			xqry.data[4] = pFWFSData->nOffset/256/256/256;
			xqry.data[5] = pFWFSData->nOffset/256/256%256;
			xqry.data[6] = pFWFSData->nOffset/256%256;
			xqry.data[7] = pFWFSData->nOffset%256;
			xqry.data[8] = pFWFSData->nLength/256%256;
			xqry.data[9] = pFWFSData->nLength%256;
			
			if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                if(m_bIsLogEnabled) LOGE("ioctl set_cur control error\n");
                IOCTL_ERROR_LOG();
				err = -1;
			}
			else {
				xqry.query = UVC_GET_CUR;
				xqry.unit = ExtenID;
				xqry.selector = 0x0c;
                xqry.size = 32;
                memset(xqry.data, 0, DATA_SIZE);
				
				if ((err = ioctl(m_fd, UVCIOC_CTRL_QUERY, &xqry)) < 0) {
                    if(m_bIsLogEnabled) LOGE("ioctl get_cur control error\n");
                    IOCTL_ERROR_LOG();
					err = -1;
				} else {
					memcpy(pFWFSData->nData, xqry.data, pFWFSData->nLength);
				}
			}
		break;
    }
    
    return err;
}

int CVideoDevice::enum_frame_intervals(__u32 pixfmt, __u32 width, __u32 height) {
	
    int ret;
    struct v4l2_frmivalenum fival;

    memset(&fival, 0, sizeof(fival));
    fival.index = 0;
    fival.pixel_format = pixfmt;
    fival.width = width;
    fival.height = height;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
    
    while ((ret = ioctl( m_fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
		
        if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {	
            if(m_bIsLogEnabled) LOGI("fival.discrete.numerator = %d , fival.discrete.denominator = %d\n"
                                     , fival.discrete.numerator, fival.discrete.denominator);
            if(m_bIsLogEnabled) LOGI("frame rate = %d\n", fival.discrete.denominator/fival.discrete.numerator);
        }
         
        else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
			
            if(m_bIsLogEnabled) LOGI("V4L2_FRMIVAL_TYPE_CONTINUOUS");
                LOGD("{min { %u/%u } .. max { %u/%u } }, \n",
                    fival.stepwise.min.numerator, fival.stepwise.min.numerator,
                    fival.stepwise.max.denominator, fival.stepwise.max.denominator);
            break;
        } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
			
            if(m_bIsLogEnabled) LOGI("V4L2_FRMIVAL_TYPE_STEPWISE\n");
                LOGD("{min { %u/%u } .. max { %u/%u } stepsize { %u/%u } }, \n",
                    fival.stepwise.min.numerator, fival.stepwise.min.denominator,
                    fival.stepwise.max.numerator, fival.stepwise.max.denominator,
                    fival.stepwise.step.numerator, fival.stepwise.step.denominator);
            break;
        }
        
        fival.index++;
    }
    
    if (ret < 0 && errno != EINVAL) {
		
        if(m_bIsLogEnabled) LOGE("ERROR enumerating frame intervals: %s\n", strerror(errno));
        IOCTL_ERROR_LOG();
        return errno;
    }

    return 0;
}

int CVideoDevice::enum_frame_sizes(__u32 pixfmt, ETRONDI_STREAM_INFO *pStreamInfo) {
	
    int ret;
    struct v4l2_frmsizeenum fsize;
    CLEAR(fsize);
    fsize.index = 0;
    fsize.pixel_format = pixfmt;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
    
    while ((ret = ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
        if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			
			if (fsize.pixel_format == V4L2_PIX_FMT_YUYV) {
				(pStreamInfo+m_RessolutionIndex)->bFormatMJPG = false;
			}
			else {
				(pStreamInfo+m_RessolutionIndex)->bFormatMJPG = true;
			}
			
			(pStreamInfo+m_RessolutionIndex)->nWidth  = fsize.discrete.width;
			(pStreamInfo+m_RessolutionIndex)->nHeight = fsize.discrete.height;
			
            LOGD("(pStreamInfo+%d)->nWidth = %d\n", m_RessolutionIndex, (pStreamInfo+m_RessolutionIndex)->nWidth);
            LOGD("(pStreamInfo+%d)->nHeight = %d\n", m_RessolutionIndex, (pStreamInfo+m_RessolutionIndex)->nHeight);
        } else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
        }
        fsize.index++;
        m_RessolutionIndex++;
    }
	
    if (ret < 0 && errno != EINVAL) {
		
        if(m_bIsLogEnabled) LOGE("ERROR enumerating frame sizes: %s\n", strerror(errno));
        IOCTL_ERROR_LOG();
        return errno;
    }

    return 0;
}

int CVideoDevice::enum_frame_formats(ETRONDI_STREAM_INFO *pStreamInfo) {
	
	struct v4l2_fmtdesc fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    m_RessolutionIndex = 0;
    
    while ( 0 == ioctl(m_fd, VIDIOC_ENUM_FMT, &fmt) ) {
        
        fmt.index++;
        if ( 0 != enum_frame_sizes( fmt.pixelformat, pStreamInfo)) LOGE("  Unable to enumerate frame sizes.\n");
    }
    if (errno != EINVAL) {
		
        if(m_bIsLogEnabled) LOGE("ERROR enumerating frame formats:%s", strerror(errno));
        IOCTL_ERROR_LOG();
        return errno;
    }

    return 0;
}

bool CVideoDevice::GetDataLengthFromFileID( int nFileId, BYTE *pBuffer, int *pLength, long nFileSysStartAddr ) {
	
  int nStartIndex = nFileSysStartAddr+1;
  int nTotalFileNumber = pBuffer[nFileSysStartAddr];
  int i = 0 ;
  bool bRet = false;
  
  if(m_bIsLogEnabled) LOGI("CEtronDI::GetDataLengthFromFileID enter.\n");
  // Is existed?
  while(i<nTotalFileNumber) {
	  
    if( nFileId == pBuffer[nStartIndex] ) 
    {
      *pLength = pBuffer[nStartIndex+1]*256 + pBuffer[nStartIndex+2] ;
      bRet = true;      
      break;
    }
    else nStartIndex += pBuffer[nStartIndex+1]*256 + pBuffer[nStartIndex+2] + 3; 
    i++; 
  }
  if(m_bIsLogEnabled) LOGI("CEtronDI::GetDataLengthFromFileID exit.\n");
  return bRet;
}

bool CVideoDevice::GetDataBufferFromFileID( int nFileId, BYTE *pBuffer, BYTE *pFileIDBuffer, int nLength ,long nFileSysStartAddr ) {
	
  int nStartIndex = nFileSysStartAddr+1;
  int nTotalFileNumber = pBuffer[nFileSysStartAddr];
  int i = 0 ;
  bool bRet = false;
  
  if(m_bIsLogEnabled) LOGI("CEtronDI::GetDataBufferFromFileID enter.\n");
  
  if( pFileIDBuffer == NULL ) {
	  
    if(m_bIsLogEnabled) LOGE("CEtronDI::GetDataBufferFromFileID() : pFileIDBuffer is NULL.\n");
    return bRet;
  }
  
  // Is existed?
  while(i<nTotalFileNumber)
  {
    if( nFileId == pBuffer[nStartIndex] ) 
    {
      //*pLength = pBuffer[nStartIndex+1]*256 + pBuffer[nStartIndex+2] ;
      memcpy( pFileIDBuffer, pBuffer+nStartIndex+3, nLength);
      bRet = true;      
      break;
    }
    else nStartIndex += pBuffer[nStartIndex+1]*256 + pBuffer[nStartIndex+2] + 3; 
    i++; 
  }
  
  if(m_bIsLogEnabled) LOGI("CEtronDI::GetDataBufferFromFileID exit.\n");
  return bRet;
}

bool CVideoDevice::SetDataBufferToFileID ( int nFileId, BYTE *pBuffer, BYTE *pFileIDBuffer, int nLength ,long nFileSysStartAddr ) {
	
  int nStartIndex = nFileSysStartAddr+1;
  int nTotalFileNumber = pBuffer[nFileSysStartAddr];
  int i = 0 ;
  bool bRet = false;
  
  if(m_bIsLogEnabled) LOGI("CEtronDI::SetDataBufferToFileID enter.\n");
  
  if( pFileIDBuffer == NULL ) {
	  
    if(m_bIsLogEnabled) LOGE("CEtronDI::GetDataBufferFromFileID() : pFileIDBuffer is NULL.\n");
    return bRet;
  }
  
  // Is existed?
  while(i<nTotalFileNumber)
  {
    if( nFileId == pBuffer[nStartIndex] ) 
    {
      //*pLength = pBuffer[nStartIndex+1]*256 + pBuffer[nStartIndex+2] ;
      memcpy( pBuffer+nStartIndex+3, pFileIDBuffer, nLength);
      bRet = true;      
      break;
    }
    else nStartIndex += pBuffer[nStartIndex+1]*256 + pBuffer[nStartIndex+2] + 3; 
    i++; 
  }
  
  if(m_bIsLogEnabled) LOGI("CEtronDI::SetDataBufferToFileID exit.\n");
  
  return bRet;
}

int CVideoDevice::init_device() {
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	
    v4l2_capability cap;
    //v4l2_cropcap cropcap;
    //v4l2_crop crop;
    v4l2_format fmt;

    if(ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        if(EINVAL == errno) {
			
            if(m_bIsLogEnabled) LOGE("%s is no V4l2 device .\n", m_szDevName);
		}
        else {
			
            if(m_bIsLogEnabled) LOGE("VIDIOC_QUERYCAP: %s", strerror(errno));
		}

        IOCTL_ERROR_LOG();
		
        return ETronDI_OPEN_DEVICE_FAIL;
    }

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		
        if(m_bIsLogEnabled) LOGE("%s is no video capture device .\n", m_szDevName);
        return ETronDI_OPEN_DEVICE_FAIL;
    }

    if(!(cap.capabilities & V4L2_CAP_STREAMING)) {
		
        if(m_bIsLogEnabled) LOGE("%s does not support streaming i/o .\n", m_szDevName);
        return ETronDI_OPEN_DEVICE_FAIL;
    }

	/*
    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(0 == ioctl(m_fd, VIDIOC_CROPCAP, &cropcap)) {
		
        CLEAR(crop);
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;

        if(-1 == ioctl(m_fd, VIDIOC_S_CROP, &crop)) {
            if(EINVAL == errno) cout << "VIDIOC_S_CROP not supported ." << endl;
            else cout << "VIDIOC_S_CROP: " << strerror(errno) << endl;
        }
    }
    else {
        cout << "VIDIOC_CROPCAP: " << strerror(errno) << endl;
        return ETronDI_OPEN_DEVICE_FAIL;
    }
	*/
	
    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = m_fival.width;
    fmt.fmt.pix.height = m_fival.height;
    fmt.fmt.pix.pixelformat = m_fival.pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if(ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        LOGE("VIDIOC_S_FMT: %s\n", strerror(errno));
        IOCTL_ERROR_LOG();
        return ETronDI_OPEN_DEVICE_FAIL;
    }

    locker.Release();
    return init_mmap();
}

int CVideoDevice::set_framerate(int *pFPS) {
	
	int nRet = ETronDI_OK;
	
    if(m_bIsLogEnabled) LOGI("set_framerate +, %d", *pFPS);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
	
	struct v4l2_streamparm Stream_Parm;
	memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
	Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 

	Stream_Parm.parm.capture.timeperframe.denominator = *pFPS;;
	Stream_Parm.parm.capture.timeperframe.numerator   = 1;
	
    if(m_bIsLogEnabled) LOGI("Stream_Parm.parm.capture.timeperframe.denominator = %d\n", Stream_Parm.parm.capture.timeperframe.denominator);
    if(m_bIsLogEnabled) LOGI("Stream_Parm.parm.capture.timeperframe.numerator = %d\n", Stream_Parm.parm.capture.timeperframe.numerator);
	
    if(ioctl(m_fd, VIDIOC_S_PARM, &Stream_Parm) < 0) {
        IOCTL_ERROR_LOG();
        return ETronDI_SET_FPS_FAIL;
    }

    // get current fps
    if (ioctl(m_fd, VIDIOC_G_PARM, &Stream_Parm) < 0){
        IOCTL_ERROR_LOG();
        return ETronDI_SET_FPS_FAIL;
    }

    if(m_bIsLogEnabled) LOGI("Stream_Parm.parm.capture.timeperframe.denominator = %d\n", Stream_Parm.parm.capture.timeperframe.denominator);
    if(m_bIsLogEnabled) LOGI("Stream_Parm.parm.capture.timeperframe.numerator = %d\n", Stream_Parm.parm.capture.timeperframe.numerator);

    *pFPS = Stream_Parm.parm.capture.timeperframe.denominator;
	
    if(m_bIsLogEnabled) LOGI("set_framerate -");
	
	return nRet;
}

int CVideoDevice::Set_v4l2_requestbuffers(int cnt)
{
    if (cnt < 1) {
        LOGE("Error:Sould be > 0");
        return -1;
    }
    v4l2_requestbuffers_cnt = cnt;

    return 0;
}


int CVideoDevice::init_mmap() {
	
    v4l2_requestbuffers req;
    CLEAR(req);
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

    req.count = v4l2_requestbuffers_cnt;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if(ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
		
        if(EINVAL == errno) {
            LOGE("%s does not support memory mapping .\n", m_szDevName);
            return ETronDI_OPEN_DEVICE_FAIL;
        }
        else {
            LOGE("VIDIOC_REQBUFS: %s", strerror(errno));
            return ETronDI_OPEN_DEVICE_FAIL;
        }

        IOCTL_ERROR_LOG();
    }

    //if(req.count < 2) {
    //	pthread_mutex_unlock(&cs_mutex);
		
    //    if(m_bIsLogEnabled) cout << "Insufficient buffer memory on ." << m_szDevName << endl;
    //    return ETronDI_OPEN_DEVICE_FAIL;
    //}

    buffers = (buffer*)calloc(req.count, sizeof(*buffers));

    if(!buffers) {
		
        if(m_bIsLogEnabled) LOGE("Out of memory .\n");
        return ETronDI_OPEN_DEVICE_FAIL;
    }

    for(n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if(ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            if(m_bIsLogEnabled) LOGE("VIDIOC_QUERYBUF: %s\n", strerror(errno));
            IOCTL_ERROR_LOG();
            return ETronDI_OPEN_DEVICE_FAIL;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, // start anywhere
                                   buf.length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED,
                                   m_fd, buf.m.offset);

        if(MAP_FAILED == buffers[n_buffers].start) {			
            if(m_bIsLogEnabled) LOGE("mmap : %s\n", strerror(errno));
            return ETronDI_OPEN_DEVICE_FAIL;
        }
    } 
    return ETronDI_OK;
}

int CVideoDevice::start_capturing() {
	
    unsigned int i;
	
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
    for(i = 0; i < n_buffers; ++i) {
        v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory =V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = buffers[i].length;

        if(ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            if(m_bIsLogEnabled) LOGE("VIDIOC_QBUF: %s\n", strerror(errno));
            IOCTL_ERROR_LOG();
            return ETronDI_OPEN_DEVICE_FAIL;
        }
    }

    v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if( ioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        if(m_bIsLogEnabled) LOGE("VIDIOC_STREAMON: %s\n", strerror(errno));
        IOCTL_ERROR_LOG();
        return ETronDI_OPEN_DEVICE_FAIL;
    }
	m_bHasStartCapture = true;
    return ETronDI_OK;
}

int CVideoDevice::stop_capturing() {
	
    v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

	if (m_bHasStartCapture)
	{
        if( ioctl(m_fd, VIDIOC_STREAMOFF, &type)) {
            if(m_bIsLogEnabled) LOGE("VIDIOC_STREAMOFF: %s\n", strerror(errno));
            IOCTL_ERROR_LOG();
			return ETronDI_CLOSE_DEVICE_FAIL;
		}

		m_bHasStartCapture = false;
	}
    
    return ETronDI_OK;
}

int CVideoDevice::uninit_device() {
	
    unsigned int i;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);

    if (buffers){
        for(i = 0; i < n_buffers; ++i) {
            if(-1 == munmap( buffers[i].start, buffers[i].length)) {
                if(m_bIsLogEnabled) LOGE("munmap: %s\n", strerror(errno));
                return ETronDI_CLOSE_DEVICE_FAIL;
            }
        }
        free(buffers);
        buffers = NULL;
    }

    v4l2_requestbuffers req;
    CLEAR(req);

    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if( ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {

        IOCTL_ERROR_LOG();

        if(EINVAL == errno) {
            LOGE("%s does not support memory mapping .\n", m_szDevName);
            return ETronDI_OPEN_DEVICE_FAIL;
        }
        else {
            LOGE("VIDIOC_REQBUFS: %s\n", strerror(errno));
            return ETronDI_OPEN_DEVICE_FAIL;
        }
    }

    return ETronDI_OK;
}


int CVideoDevice::SetupNonBlock() {
    int nRec = 0;
    int flags = 0;

    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
    if((flags = fcntl(m_fd, F_GETFL,0)) == -1)        {
             perror("fcntl F_GETFL fail:");
             nRec = -1;
             goto END;
     }

     flags |= O_NONBLOCK;
     if(fcntl(m_fd, F_SETFL,flags) == -1)        {
             perror("fcntl F_SETFL fail:");
             nRec = -1;
             goto END;
     }
     m_b_blocking = false;

END:
     return nRec;
}

int CVideoDevice::SetupBlock(int waitingTime_sec, int waitingTime_usec) {
    int nRec = 0;
    int flags = 0;
    CFWMutexLocker locker(&cs_mutex, &m_FWFSData, this);
    if((flags = fcntl(m_fd, F_GETFL,0))==-1)        {
             perror("fcntl F_GETFL fail:");
             nRec = -1;
             goto END;
     }

     flags &= ~O_NONBLOCK;
     if(fcntl(m_fd, F_SETFL,flags) == -1)        {
             perror("fcntl F_SETFL fail:");
             nRec = -1;
             goto END;
     }
     m_waitingTime_sec = waitingTime_sec;
     m_waitingTime_usec = waitingTime_usec;
     m_b_blocking = true;
     LOGI("====================>Setup Blocking Mode = true\n");
END:
     return nRec;
}

int CVideoDevice::get_frame(void **frame_buf, size_t* len) {
	
    v4l2_buffer queue_buf;
    int nRet = ETronDI_OK;
    CLEAR(queue_buf);
    CFWMutexLocker locker(&cs_mutex, nullptr, this);

    queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queue_buf.memory = V4L2_MEMORY_MMAP;

    if (m_b_blocking == true) {
        nRet = x_select(m_fd, m_waitingTime_sec, m_waitingTime_usec, READ_STATUS);
        if (nRet == 1) {
            nRet = ETronDI_OK;
        } else {
            if (nRet == 0) {
                LOGE("blocking mode : select time out (0)\n");
                nRet = ETronDI_DEVICE_TIMEOUT;
            } else {
                LOGE("blocking mode : select error (-1)\n");
                nRet = ETronDI_IO_SELECT_EINTR;
            }
            goto END;
        }
    }

    if( ioctl(m_fd, VIDIOC_DQBUF, &queue_buf) < 0) {
        if(m_bIsLogEnabled && errno != 11) LOGE("VIDIOC_DQBUF(%s) :  ETronDI_GET_IMAGE_FAIL, errno:%d\n", strerror(errno), errno);
        nRet = ETronDI_GET_IMAGE_FAIL;
        IOCTL_ERROR_LOG();
        goto END;
    }

    *frame_buf = buffers[queue_buf.index].start;
    *len = queue_buf.bytesused;
    m_index = queue_buf.index;
END:
    return nRet;

}

int CVideoDevice::unget_frame() {
	
    if(m_index != -1) {
        v4l2_buffer queue_buf;
        CLEAR(queue_buf);
        CFWMutexLocker locker(&cs_mutex, nullptr, this);

        queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        queue_buf.memory = V4L2_MEMORY_MMAP;
        queue_buf.index = m_index;

        if( ioctl(m_fd, VIDIOC_QBUF, &queue_buf) < 0) {
            if(m_bIsLogEnabled) LOGE("VIDIOC_QBUF: %s\n", strerror(errno));
            IOCTL_ERROR_LOG();
            return ETronDI_GET_IMAGE_FAIL;
        }
        
        return ETronDI_OK;
    }
    return ETronDI_GET_IMAGE_FAIL;
}

void CVideoDevice::HSV_to_RGB(double H, double S, double V, double &R, double &G, double &B) {

        double nMax,nMin;
        double fDet;
        //
        while (H<0.0) H+=360.0;
        while (H>=360.0) H-=360.0;
        H /= 60.0;
        if (V<0.0) V = 0.0;
        if (V>1.0) V = 1.0;
        V *= 255.0;
        if (S<0.0) S = 0.0;
        if (S>1.0) S = 1.0;
        //
        if (V == 0.0) {
            R = G = B = 0;
        } else {
            fDet = S*V;
            nMax = (V);
            nMin = (V-fDet);
            if (H<=1.0) { //R>=G>=B, H=(G-B)/fDet
                R = nMax;
                B = nMin;
                G = (H*fDet+B);
            } else if (H<=2.0) { //G>=R>=B, H=2+(B-R)/fDet
                G = nMax;
                B = nMin;
                R = ((2.0-H)*fDet+B);
            } else if (H<=3.0) { //G>=B>=R, H=2+(B-R)/fDet
                G = nMax;
                R = nMin;
                B = ((H-2.0)*fDet+R);
            } else if (H<=4.0) { //B>=G>=R, H=4+(R-G)/fDet
                B = nMax;
                R = nMin;
                G = ((4.0-H)*fDet+R);
            } else if (H<=5.0) { //B>=R>=G, H=4+(R-G)/fDet
                B = nMax;
                G = nMin;
                R = ((H-4.0)*fDet+G);
            } else { // if(H<6.0) //R>=B>=G, H=(G-B)/fDet+6
                R = nMax;
                G = nMin;
                B = ((6.0-H)*fDet+G);
            }
        }
}

bool CVideoDevice::GetLogBufferByTag( unsigned char nID, unsigned char *pTotalBuffer, unsigned char *pOutputBuffer, int *pLength) {

	int nIndex = 0;
	bool bIsFind = false;
	
	short temp1, temp2;
	
	for(int i=0;i<4096;i++) {
		if( *(pTotalBuffer+i) != 0 ) bIsFind = true;
	}
	
	if(bIsFind != true) return bIsFind;

	
	while( nID != *(pTotalBuffer+nIndex) ) {
		
		if( *(pTotalBuffer+nIndex) == 0x0f ) {
			
			bIsFind = false;
			break;
		}
		
		if( *(pTotalBuffer+nIndex) == AUTOADJUSTMENT_ERROR_ID ||
			*(pTotalBuffer+nIndex) == SMARTK_ERROR_ID ||
			*(pTotalBuffer+nIndex) == SMARTZD_ERROR_ID )
			nIndex += *(pTotalBuffer+nIndex+2)*256 + *(pTotalBuffer+nIndex+1) + 1;
		else
			nIndex += *(pTotalBuffer+nIndex+1)*256 + *(pTotalBuffer+nIndex+2) + 1;
	}
			
	if(bIsFind) {
		
		if( *(pTotalBuffer+nIndex) == AUTOADJUSTMENT_ERROR_ID ||
			*(pTotalBuffer+nIndex) == SMARTK_ERROR_ID ||
			*(pTotalBuffer+nIndex) == SMARTZD_ERROR_ID ) {
				
			temp1 = *(pTotalBuffer+nIndex+2);
			temp2 = *(pTotalBuffer+nIndex+1) & 0xff;
			*pLength = temp1*256+temp2;
		}
		else {
			
			temp1 = *(pTotalBuffer+nIndex+1);
			temp2 = *(pTotalBuffer+nIndex+2) & 0xff;
			*pLength = temp1*256+temp2;
		}

		memcpy( pOutputBuffer, pTotalBuffer+nIndex+1, *pLength);
	}
	
	return bIsFind;
}

bool CVideoDevice::IsAngleLegal(double angle)
{
	if((int(angle*STEPAMOUNT_PER_ROUND)%360)!=0) return false;
	else {
		if(angle*STEPAMOUNT_PER_ROUND/360>=2048) return false;
	}
	return true;
}

double CVideoDevice::MotorStepToRotationAngle(long nTotalStep)
{
  return (double)nTotalStep*360.0f/512.0f;
}

// private -

void CVideoDevice::RelaseComputer()
{
	m_pComputer->Reset();
}

int CVideoDevice::FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height)
{
	return m_pComputer->FlyingDepthCancellation_D8(pdepthD8, width, height);
}

int CVideoDevice::FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height)
{
	return m_pComputer->FlyingDepthCancellation_D11(pdepthD11, width, height);
}

int CVideoDevice::ColorFormat_to_RGB24( unsigned char* ImgDst, unsigned char* ImgSrc, int SrcSize, int width, int height, EtronDIImageType::Value type )
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> locker(mutex);

    switch (type) { 
		case EtronDIImageType::COLOR_YUY2 :
    return m_pComputer->ColorFormat_to_RGB24(ImgDst, ImgSrc, SrcSize, width, height, type);
		case EtronDIImageType::COLOR_MJPG :
                    return tjDecompress2(m_pTjHandle, ImgSrc, SrcSize, ImgDst,
                                         width, 0, height, TJPF_RGB,
                                         TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE)
                               ? ETronDI_VERIFY_DATA_FAIL
                               : ETronDI_OK;
                default:
					return ETronDI_NotSupport;
        }


        if (EtronDIImageType::COLOR_YUY2 != type) return ETronDI_NotSupport;

    
}

int CVideoDevice::DepthMerge( unsigned char** pDepthBufList, float *pDepthMergeOut, 
							  unsigned char *pDepthMergeFlag,
							  int nDWidth, int nDHeight, 
							  float fFocus, float * pBaseline, 
							  float *pWRNear, float *pWRFar, 
							  float *pWRFusion, int nMergeNum )
{
	return m_pComputer->DepthMerge(pDepthBufList, pDepthMergeOut,
								   pDepthMergeFlag,
								   nDWidth, nDHeight,
								   fFocus, pBaseline,
								   pWRNear, pWRFar,
								   pWRFusion, nMergeNum);
}

int CVideoDevice::RotateImg90(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst, int len, bool clockwise)
{
	return m_pComputer->ImageRotate90(imgType, width, height, src, dst, len, clockwise);
}

int CVideoDevice::ImgMirro(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst)
{
	return m_pComputer->ImageMirro(imgType, width, height, src, dst);
}

void CVideoDevice::Resample( const BYTE* ImgSrc, const int SrcW, const int SrcH, BYTE *ImgDst, const int DstW, const int DstH, int BytePerPixel )
{
	m_pComputer->Resample(ImgSrc, SrcW, SrcH, ImgDst, DstW, DstH, BytePerPixel);
}

int CVideoDevice::GetPointCloud( unsigned char* ImgColor, int CW, int CH,
                                 unsigned char* ImgDepth, int DW, int DH,
                                 PointCloudInfo* pPointCloudInfo,
                                 unsigned char* pPointCloudRGB, float* pPointCloudXYZ, float Near, float Far, unsigned short pid )
{
	bool bDepthOnly = !ImgColor || !CW || !CH;

    BYTE *pDepth = ImgDepth;
    BYTE *pColor = ImgColor;
    int nDstW = DW;
    int nDstH = DH;

    EtronDIImageType::Value imageType = EtronDIImageType::DepthDataTypeToDepthImageType(pPointCloudInfo->wDepthType);

    if (DW != nDstW || DH != nDstH)
    {
        int bytePerPixel = (imageType == EtronDIImageType::DEPTH_8BITS) ? 1 : 2;

        if (nDstW * nDstH * bytePerPixel != m_vecDepthResample.size())
            m_vecDepthResample.resize(nDstW * nDstH * bytePerPixel, 0);

        Resample(ImgDepth, DW, DH, &m_vecDepthResample[0], nDstW, nDstH, bytePerPixel);

        pDepth = &m_vecDepthResample[0];
    }

    if (!bDepthOnly && (CW != nDstW || CH != nDstH)) 
    {
        if (nDstW * nDstH * 3 != m_vecColorResample.size())
            m_vecColorResample.resize(nDstW * nDstH * 3, 0);

        Resample(ImgColor, CW, CH, &m_vecColorResample[0], nDstW, nDstH, 3);

        pColor = &m_vecColorResample[0];
    }
	
	return m_pComputer->GetPointCloud(pColor, nDstW, nDstH,
								      pDepth, nDstW, nDstH,
									  pPointCloudInfo,
									  pPointCloudRGB, pPointCloudXYZ,
									  Near, Far, pid);
}


// Depth filter +
BYTE* CVideoDevice::SubSample(BYTE* depthBuf, int bytesPerPixel, int width, int height, int& new_width, int& new_height, int mode, int factor)
{
    return m_depthFilter->SubSample( depthBuf, bytesPerPixel, width, height, new_width, new_height, mode, factor );
}

void CVideoDevice::HoleFill(BYTE* depthBuf, int bytesPerPixel, int kernel_size, int width, int height, int level, bool horizontal)
{
    m_depthFilter->HoleFill( depthBuf, bytesPerPixel, kernel_size, width, height, level, horizontal );
}

void CVideoDevice::TemporalFilter(BYTE* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history)
{
	if (m_depthFilterParameters.nTemporalFilterWidth != width ||
		m_depthFilterParameters.nTemporalFilterHeight != height ||
		m_depthFilterParameters.nTemporalFilterBytePerPixel != bytesPerPixel)
	{

		m_depthFilterParameters.nTemporalFilterWidth = width;
		m_depthFilterParameters.nTemporalFilterHeight = height;
		m_depthFilterParameters.nTemporalFilterBytePerPixel = bytesPerPixel;

		m_depthFilter->FreeTemporalFilter();
		m_depthFilter->InitTemporalFilter(width, height, bytesPerPixel);
	}

	m_depthFilter->TemporalFilter( depthBuf, bytesPerPixel, width, height, alpha, history );
}

void CVideoDevice::EdgePreServingFilter(BYTE* depthBuf, int type, int width, int height, int level, float sigma, float lumda)
{
	if (m_depthFilterParameters.nEdgePreServingWidth != width ||
	    m_depthFilterParameters.nEdgePreServingHeight != height){
		
		m_depthFilterParameters.nEdgePreServingWidth = width;
		m_depthFilterParameters.nEdgePreServingHeight = height;

		m_depthFilter->FreeEdgePreServingFilter();
		m_depthFilter->InitEdgePreServingFilter(width, height);
	}

	m_depthFilter->EdgePreServingFilter(depthBuf, type, width, height, level, sigma, lumda);
}

void CVideoDevice::ApplyFilters(BYTE* depthBuf, BYTE* subDisparity, int bytesPerPixel, int width, int height, int sub_w, int sub_h, int threshold)
{
    m_depthFilter->ApplyFilters( depthBuf, subDisparity, bytesPerPixel, width, height, sub_w, sub_h, threshold );
}

void CVideoDevice::ResetFilters()
{
    m_depthFilter->Reset();
}

void CVideoDevice::EnableGPUAcceleration(bool enable)
{
	if (CComputer::OPENCL == m_pComputer->GetType())
		m_depthFilter->EnableGPUAcceleration(enable);
	else
		m_depthFilter->EnableGPUAcceleration(false);
}

int CVideoDevice::TableToData(int width, int height, int TableSize, unsigned short* Table, unsigned short* Src, unsigned short* Dst)
{
	return m_pComputer->TableToData(width, height, TableSize, Table, Src, Dst);
}
// Depth filter -

int CVideoDevice::SetPlumAR0330(bool bEnable)
{
    unsigned short nValue = 0;
    if(ETronDI_OK != GetFWRegister(0xF3, &nValue, FG_Address_1Byte | FG_Value_1Byte)){
        return ETronDI_WRITE_REG_FAIL;
    }

    nValue |= 0x11;
    nValue &= bEnable ? 0x10 : 0x01;
    if(ETronDI_OK != SetFWRegister(0xF3, nValue, FG_Address_1Byte | FG_Value_1Byte)){
        return ETronDI_WRITE_REG_FAIL;
    }

    return ETronDI_OK;
}

std::string CVideoDevice::GetDeviceName()
{
    std::string str = m_szDevName;
    auto pos = str.find_last_of("/");
    if (pos == std::string::npos) {
        LOGE("Fail to get vid and pid\n");
        return "";
    }
    return str.substr(pos + 1);
}

std::string CVideoDevice::GetDeviceRealPath()
{
    std::string path = "/sys/class/video4linux/" + GetDeviceName();
    std::string real_path;
    char buff[PATH_MAX] = {0};
    if (realpath(path.c_str(), buff) != nullptr) {
        real_path = std::string(buff);
        if (real_path.find("virtual") != std::string::npos)
            return std::string();
    }

    return real_path;
}

std::string CVideoDevice::GetDeviceUSBInfoPath()
{
	std::string real_path = GetDeviceRealPath();
	if (real_path.empty()) return std::string();

	std::string usb_info_path = real_path + "/../../../";
    char usb_actual_path[PATH_MAX] = {0};
    if (nullptr == realpath(usb_info_path.c_str(), usb_actual_path))
        return std::string();

    return std::string(usb_actual_path);
}

int CVideoDevice::GetDeviceIndex()
{
    std::string path =  "/sys/class/video4linux/" + GetDeviceName() + "/index";

    int index;

    if (!(std::ifstream(path) >> std::dec >> index)) 
        return ETronDI_READ_REG_FAIL;

    return index;
}

std::string CVideoDevice::GetDeviceStatString()
{
    std::string busnum, devnum, devpath;

    auto dev_name = "/dev/" + GetDeviceName();

    struct stat st = {};
    if (stat(dev_name.c_str(), &st) < 0) {
        return std::string();
    }
    if (!S_ISCHR(st.st_mode)) return std::string();

    // Search directory and up to three parent directories to find busnum/devnum
    std::ostringstream ss;
    ss << "/sys/dev/char/" << major(st.st_rdev) << ":" << minor(st.st_rdev)
       << "/device/";
    auto path = ss.str();
    auto valid_path = false;
    for (auto i = 0U; i < 10; ++i) {
        if (std::ifstream(path + "busnum") >> busnum) {
            if (std::ifstream(path + "devnum") >> devnum) {
                if (std::ifstream(path + "devpath") >> devpath) {
                    valid_path = true;
                    break;
                }
            }
        }
        path += "../";
    }

    if (!valid_path) return std::string();

    return busnum + "-" + devnum + "-" + devpath;
}

int CVideoDevice::GetUSBBusInfo(char *pszBusInfo) 
{
    std::string real_path = GetDeviceRealPath();
    if (real_path.empty()) return ETronDI_NullPtr;
    int end_pos  = real_path.find("/video4linux");
    if (end_pos == std::string::npos) {
        return ETronDI_NullPtr;
    }

    int begin_pos = real_path.substr(0, end_pos).find_last_of("/");
    if (begin_pos == std::string::npos) {
        return ETronDI_NullPtr;
    }

	++begin_pos;

    std::string usb_bus = real_path.substr(begin_pos, end_pos - begin_pos);

    strcpy(pszBusInfo, usb_bus.c_str());
	
	return ETronDI_OK;
}