#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "PlyWriter.h"
#include "eSPDI.h"

#include "jpeglib.h"
#include <turbojpeg.h>
#include "ColorPaletteGenerator.h"
#include "RegisterSettings.h"


#define CT_DEBUG_PRINTF(format, ...) \
    printf("[CT][%s][%d]" format, __func__, __LINE__, ##__VA_ARGS__)

#define CT_DEBUG_ENABLE 1
#ifdef CT_DEBUG_ENABLE
#define CT_DEBUG CT_DEBUG_PRINTF
#else
#define CT_DEBUG(fmt, args...) do {} while (0)
#endif


#define SAVE_FILE_DIR "./out_img/"
#define MAX_FILE_PATH_LEN 1024

#define COLOR_DEPTH_STR "COLOR_DEPTH"

#define ONLY_PRINT_OVER_DIFF 1

static void* EYSD = NULL;
static DEVSELINFO g_DevSelInfo;
static DEVINFORMATION *g_pDevInfo = NULL;

#define CAMERA_PID_SANDRA 0x0167
#define CAMERA_PID_NORA  0x0168

#define DEFAULT_VIDEO_MODE_SELECTED_INDEX 0
struct video_mode {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t depth_type;
    bool is_interleave_mode;
    uint32_t pixelcode;
    bool is_scale_down;
};
struct video_mode v1_video_modes []  ={
    {
        .width = 2560,
        .height = 720,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2560,
        .height = 720,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        
        .width = 1280,
        .height = 360,
        .fps = 60,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
        
    },
    {
        .width = 2560,
        .height = 720,
        .fps = 12,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2560,
        .height = 720,
        .fps = 12,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 1280,
        .height = 360,
        .fps = 24,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = true
    }
    
};


struct video_mode v2_video_modes []  ={
    {
        .width = 2048,
        .height = 768,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 15,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 15,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 10,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 10,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 20,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 2048,
        .height = 768,
        .fps = 20,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = false
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 15,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 15,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 10,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 10,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 30,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 20,
        .depth_type = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = true,
        .pixelcode = 0,
        .is_scale_down = true
    },
    {
        .width = 1024,
        .height = 384,
        .fps = 20,
        .depth_type = APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY,
        .is_interleave_mode = false,
        .pixelcode = 0,
        .is_scale_down = true
    },
    
};

static int gColorFormat = 0; // 0:YUYV (only support YUYV) 
static int gColorWidth = 1280;
static int gColorHeight = 720;
static int gDepthWidth = 1280; // Depth is only YUYV format
static int gDepthHeight = 720;
static int gActualFps = 30;
static uint32_t gDepthType = APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY; //APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY
static bool gIsScaleDown = false;
static bool gIsInterleaveMode = false;

static DEPTH_TRANSFER_CTRL gDepth_Transfer_ctrl = DEPTH_IMG_NON_TRANSFER;

static  uint8_t g_pzdTable[APC_ZD_TABLE_FILE_SIZE_11_BITS] = {0x0};
static  uint16_t g_maxFar = 0;
static  uint16_t g_maxNear = 0;
static int g_zdTableInfo_index = 0;

static uint8_t *gColorImgBuf = NULL;
static uint8_t *gDepthImgBuf = NULL;
static uint8_t *gColorRGBImgBuf = NULL;
static unsigned long int gColorImgSize = 0;
static unsigned long int gDepthImgSize = 0;
static bool bSnapshot = true;

int GetDateTime(char *psDateTime);

static int init_device(void);
static void release_device(void);
static int open_device(uint32_t depthtype);
static int close_device(void);


static int getZDtable(void);

static int getIRValue(uint16_t *min, uint16_t *max);
static int setIRValue(uint16_t IRvalue);

static int getPointCloud(uint8_t *ImgColor,
                         int CW, int CH,
                         uint8_t *ImgDepth,
                         int DW, int DH, int depthDataType,
                         uint8_t *pPointCloudRGB,
                         float *pPointCloudXYZ,
                         float fNear, float fFar);


static int demo(void *(*func)(void *), void *arg);
static void *get_color_depth_image_func(void *arg);
static void *point_cloud_func(void *arg);
static void *property_bar_test_func(void *arg);

int main(void)
{
    int input = 255;
    do {
        printf("\n-----------------------------------------\n");
        printf("Software version : %s\n", APC_VERSION);
        printf("Please choose fllowing steps:\n");       
        printf("0. Get Color and Depth Image\n");
        printf("1. Point cloud\n");
        printf("2. Proptery bar test (AE/AWB)\n");
        printf("255. exit)\n");
        scanf("%d", &input);
        switch(input) {
        case 0:
            demo(get_color_depth_image_func, NULL);
            break;
        case 1:
            demo(point_cloud_func, NULL);
            break;
        case 2:
            demo(property_bar_test_func, NULL);
            break;
        case 255:
            return 0;
            break; 
        default:
            continue;
        }
    } while(1);

	return 0;
}


static int demo(void *(*func)(void *), void *arg)
{
    int ret = 0;
    pthread_t thread_id;
    pthread_attr_t thread_attr;
    struct sched_param thread_param; 
    
    ret = init_device();
    if (ret == APC_OK) {
        ret = open_device(gDepthType);
        if (ret == APC_OK) { 
            pthread_attr_init(&thread_attr);
            pthread_attr_getschedparam (&thread_attr, &thread_param);
            thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
            pthread_attr_setschedparam(&thread_attr, &thread_param);
            pthread_create(&thread_id, &thread_attr, func, arg);
            CT_DEBUG("Wait the finish of func !!\n");
            pthread_join(thread_id, NULL);
            ret = close_device();
            if (ret != APC_OK) {
                CT_DEBUG("Failed to call close_device()!!\n");
            }
        }
    }

    release_device();
    
    return 0;
}

int GetDateTime(char *psDateTime)
{
    time_t timep; 
    struct tm *p; 
    
    time(&timep); 
    p=localtime(&timep); 

    sprintf(psDateTime,"%04d%02d%02d_%02d%02d%02d", (1900+p->tm_year), (1+p->tm_mon), p->tm_mday,                                                                                                                                                                                                     
            p->tm_hour, p->tm_min, p->tm_sec);
    return 0;
}

static int init_device(void)
{
    int ret = 0;
    char FWVersion[256] = {0x0};
    int nActualLength = 0;
    struct video_mode *mode = NULL;
    
    ret = APC_Init(&EYSD, true);
    if (ret == APC_OK) {
        CT_DEBUG("APC_Init() success! (EYSD : %p)\n", EYSD);
    } else {
        CT_DEBUG("APC_Init() fail.. (ret : %d, EYSD : %p)\n", ret, EYSD);
    }

    if( APC_OK == APC_GetFwVersion(EYSD, &g_DevSelInfo, FWVersion, 256, &nActualLength)) {
        CT_DEBUG("FW Version = [%s]\n", FWVersion);
    }

    
    g_DevSelInfo.index = 0;
    g_pDevInfo = (DEVINFORMATION*)malloc(sizeof(DEVINFORMATION));
    CT_DEBUG("select index = %d\n", g_DevSelInfo.index);
    
    ret = APC_GetDeviceInfo(EYSD, &g_DevSelInfo ,g_pDevInfo);
    if (ret == APC_OK) {
        CT_DEBUG("Device Name = %s\n", g_pDevInfo->strDevName);
        CT_DEBUG("PID = 0x%04x\n", g_pDevInfo->wPID);
        CT_DEBUG("VID = 0x%04x\n", g_pDevInfo->wVID);
        CT_DEBUG("Chip ID = 0x%x\n", g_pDevInfo->nChipID);
        CT_DEBUG("device type = %d\n", g_pDevInfo->nDevType);
        switch (g_pDevInfo->wPID) {
           case CAMERA_PID_SANDRA:
                mode = &v1_video_modes[DEFAULT_VIDEO_MODE_SELECTED_INDEX];
                break;
           case CAMERA_PID_NORA:
                mode = &v2_video_modes[DEFAULT_VIDEO_MODE_SELECTED_INDEX];
           default:
               ret = APC_NoDevice;
               CT_DEBUG("Unkown PID (0x%04x) !!\n", g_pDevInfo->wPID);
               goto exit;
       }
       
       gColorFormat = mode->pixelcode;
       gColorWidth = mode->width / 2;
       gColorHeight =  mode->height;
       gDepthWidth = mode->width / 2;
       gDepthHeight = mode->height;
       gActualFps = mode->fps;
       gDepthType = mode->depth_type;
       gIsScaleDown = mode->is_scale_down;
       gIsInterleaveMode = mode->is_interleave_mode;
       
       CT_DEBUG("vido mode:[%u, %u, %u, %u, %d, %d]\n", mode->width, mode->height, mode->fps, mode->depth_type, mode->is_scale_down, mode->is_interleave_mode);

    } else {
        CT_DEBUG("Failed to call APC_GetDeviceInfo()!!\n");
    }
exit:
    return ret;    
}

static void release_device(void)
{
    CT_DEBUG("Release (EYSD : %p) !!\n", EYSD);
    APC_Release(&EYSD);
    if (g_pDevInfo) {
        free(g_pDevInfo);
        g_pDevInfo = NULL;
    }

}

static int open_device(uint32_t depthtype)
{
    int ret = 0;
    uint16_t ir_max = 0;
    uint16_t ir_min = 0;
    uint16_t ir_set = 0;
    bool intleave_mode = false;
    uint16_t depth_type_val = 0;

    if (!EYSD) {
        init_device();
    }
    
    ret = APC_SetDepthDataType(EYSD, &g_DevSelInfo, depthtype);
    if (ret != APC_OK) {
        CT_DEBUG("APC_SetDepthDataType() fail.. (ret=%d)\n", ret);
        goto exit;
    } else {
        ret = APC_GetDepthDataType(EYSD, &g_DevSelInfo, &depth_type_val);
        if (ret != APC_OK) {
            CT_DEBUG("APC_GetDepthDataType() fail.. (ret=%d)\n", ret);
            goto exit;
        }
        CT_DEBUG("DepthType: (0x%04x)(0x%04x)\n",depthtype, depth_type_val);
    }
    
    
    ret = APC_SetInterleaveMode(EYSD, &g_DevSelInfo, gIsInterleaveMode);
    if (ret != APC_OK) {
        CT_DEBUG("APC_SetInterleaveMode() fail.. (ret=%d)\n", ret);
        goto exit;
    } else {
        ret = APC_GetInterleaveMode(EYSD, &g_DevSelInfo, &intleave_mode);
        if (ret != APC_OK) {
            CT_DEBUG("APC_SetInterleaveMode() fail.. (ret=%d)\n", ret);
            goto exit;
        }
        CT_DEBUG("InterleaveMode: (%d)(%d)\n",gIsInterleaveMode, intleave_mode);
    }
    
    
    if (APCImageType::DepthDataTypeToDepthImageType(gDepthType) == APCImageType::DEPTH_11BITS) {
        ret = getZDtable();
        if (ret != APC_OK) {
            CT_DEBUG("Failed to get ZD table!!\n");
            goto exit;
        }
    }

    ret= APC_OpenDevice(EYSD, &g_DevSelInfo,
                          gColorWidth, gColorHeight, (bool)gColorFormat,
                          gDepthWidth, gDepthHeight,
                          gDepth_Transfer_ctrl,
                          false, NULL,
                          &gActualFps, IMAGE_SN_SYNC/*IMAGE_SN_NONSYNC*/);
    if (ret == APC_OK) {
            CT_DEBUG("APC_OpenDevice() success! (FPS=%d)\n", gActualFps);
    } else {
            CT_DEBUG("APC_OpenDevice() fail.. (ret=%d)\n", ret);
    }

    if (gIsInterleaveMode == true) {
        setIRValue(0x00ff);
        if (getIRValue(&ir_min, &ir_max) == 0) {
            CT_DEBUG("(ir_min, ir_max) = [0x%04x, 0x%04x]\n", ir_min, ir_max);
            ir_set = (ir_max + ir_min)/2;
            CT_DEBUG("Set ir as [0x%4x]\n", ir_set);
            setIRValue(ir_set);
        }
    }

 exit:   
    return ret;
}



static int close_device(void)
{
    int ret;
    
    ret = APC_CloseDevice(EYSD, &g_DevSelInfo);
    if(ret == APC_OK) {
        CT_DEBUG("APC_CloseDevice() success!\n");
    } else {
        CT_DEBUG("APC_CloseDevice() fail.. (ret=%d)\n", ret);
    }
    
    return ret;
}

static int getZDtable(void)
{
    ZDTABLEINFO zdTableInfo = {0x0};
    int bufSize = 0;
    int nRet = APC_OK;
    uint16_t nZValue;
    int actualLength = 0;
    
    zdTableInfo.nDataType = APC_DEPTH_DATA_11_BITS;
    
    memset(g_pzdTable, 0, sizeof(g_pzdTable));
   
    bufSize = APC_ZD_TABLE_FILE_SIZE_11_BITS;


    zdTableInfo.nIndex = 0; //The resolution is original or scale down resolution of sensor. 


    nRet = APC_GetZDTable(EYSD, &g_DevSelInfo, g_pzdTable, bufSize, &actualLength, &zdTableInfo);
    if (nRet != APC_OK) {
        CT_DEBUG("Failed to call APC_GetZDTable()(%d)\n", nRet);
        return nRet;
    }
    
    g_maxNear = 0xfff;
    g_maxFar = 0;

    CT_DEBUG("[%s][%d]Enter to calac mxaFar and maxNear...\n", __func__, __LINE__);
    for (int i = 0 ; i < APC_ZD_TABLE_FILE_SIZE_11_BITS ; ++i) {
        if ((i * 2) == APC_ZD_TABLE_FILE_SIZE_11_BITS)
            break;
        nZValue = (((uint16_t)g_pzdTable[i * 2]) << 8) +g_pzdTable[i * 2 + 1];
        if (nZValue) {
            g_maxNear = std::min<uint16_t>(g_maxNear, nZValue);
            g_maxFar = std::max<uint16_t>(g_maxFar, nZValue);
        }
    }
    CT_DEBUG("[%s][%d]Leave to calac mxaFar and maxNear...\n", __func__, __LINE__);

    if (g_maxNear > g_maxFar)
        g_maxNear = g_maxFar;

    if (g_maxFar > 1000)
        g_maxFar = 1000;

    
    g_zdTableInfo_index = zdTableInfo.nIndex;
    
    CT_DEBUG("Get ZD Table actualLength : %d, g_maxNear : %d, g_maxFar : %d\n", actualLength, g_maxNear ,g_maxFar);
    return nRet;
}

static int getIRValue(uint16_t *min, uint16_t *max)
{
    int ret = APC_OK;
    uint16_t m_nIRMax = 0; 
    uint16_t m_nIRMin = 0;
    
    ret = APC_GetFWRegister(EYSD, &g_DevSelInfo,
                                0xE2, &m_nIRMax,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret) {
        CT_DEBUG("get IR Max value failed\n");
        return ret;
     }

    ret = APC_GetFWRegister(EYSD, &g_DevSelInfo,
                                0xE1, &m_nIRMin,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret) {
        CT_DEBUG("get IR Min value failed\n");
        return ret;
     }
    CT_DEBUG("IR range: %d ~ %d\n",m_nIRMin, m_nIRMax);      
    
    *min = m_nIRMin;
    *max = m_nIRMax;
    
    return ret;
}


static int setIRValue(uint16_t IRvalue)
{
    int ret = APC_OK;
    uint16_t m_nIRMax, m_nIRMin, m_nIRValue;
    
    ret = APC_GetFWRegister(EYSD, &g_DevSelInfo,
                                0xE2, &m_nIRMax,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret) return ret;

    ret = APC_GetFWRegister(EYSD, &g_DevSelInfo,
                                0xE1, &m_nIRMin,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret) return ret;

    if (IRvalue > m_nIRMax || m_nIRMax < m_nIRMin) {
        m_nIRValue = (m_nIRMax - m_nIRMin) / 2;
    } else {
        m_nIRValue = IRvalue;
    }
    CT_DEBUG("IR range, IR Min : %d, IR Max : %d, set IR Value : %d\n", m_nIRMin, m_nIRMax, m_nIRValue);

    if (m_nIRValue != 0) {
        ret = APC_SetIRMode(EYSD, &g_DevSelInfo, 0x63); // 6 bits on for opening both 6 ir
        if (APC_OK != ret) return ret;
        CT_DEBUG("enable IR and set IR Value : %d\n",m_nIRValue);
        ret = APC_SetCurrentIRValue(EYSD, &g_DevSelInfo, m_nIRValue);
        if (APC_OK != ret) return ret;
        ret = APC_GetCurrentIRValue(EYSD, &g_DevSelInfo, &m_nIRValue);
        if (APC_OK != ret) return ret;
        CT_DEBUG("get IR Value : %d\n",m_nIRValue);
    } else {
        ret = APC_SetCurrentIRValue(EYSD, &g_DevSelInfo, m_nIRValue);
        if (APC_OK != ret) return ret;
        ret = APC_SetIRMode(EYSD,&g_DevSelInfo, 0x00); // turn off ir
        if (APC_OK != ret) return ret;
        CT_DEBUG("disable IR\n");
    }
    return APC_OK;
}


static int getPointCloudInfo(void *pHandleEYSD, DEVSELINFO *pDevSelInfo, PointCloudInfo *pointCloudInfo, int depthDataType, int depthHeight)
{
    float ratio_Mat = 0.0f;
    float baseline  = 0.0f;
    float diff      = 0.0f;
    eSPCtrl_RectLogData rectifyLogData;
    eSPCtrl_RectLogData *pRectifyLogData = NULL;
    int nIndex = g_zdTableInfo_index;
    int ret = APC_NoDevice;
    
    if (!pointCloudInfo){
        return -EINVAL;
    }

    memset(&rectifyLogData, 0x0, sizeof(eSPCtrl_RectLogData));
    ret = APC_GetRectifyMatLogData(pHandleEYSD, pDevSelInfo, &rectifyLogData, nIndex);
    if (ret == APC_OK) {
        pRectifyLogData = &rectifyLogData;
        ratio_Mat = (float)depthHeight / pRectifyLogData->OutImgHeight;
        baseline  = 1.0f / pRectifyLogData->ReProjectMat[14];
        diff      = pRectifyLogData->ReProjectMat[15] * ratio_Mat;
        
        memset(pointCloudInfo, 0, sizeof(PointCloudInfo));
        pointCloudInfo->wDepthType = depthDataType;
        
        pointCloudInfo->centerX = -1.0f * pRectifyLogData->ReProjectMat[3] * ratio_Mat;
        pointCloudInfo->centerY = -1.0f * pRectifyLogData->ReProjectMat[7] * ratio_Mat;
        pointCloudInfo->focalLength = pRectifyLogData->ReProjectMat[11] * ratio_Mat;

        switch (APCImageType::DepthDataTypeToDepthImageType(depthDataType)){
            case APCImageType::DEPTH_14BITS: pointCloudInfo->disparity_len = 0; break;
            case APCImageType::DEPTH_11BITS:
            {
                pointCloudInfo->disparity_len = 2048;
                for(int i = 0 ; i < pointCloudInfo->disparity_len ; ++i){
                    pointCloudInfo->disparityToW[i] = ( i * ratio_Mat / 8.0f ) / baseline + diff;
                }
                break;
            }
            default:
                pointCloudInfo->disparity_len = 256;
                for(int i = 0 ; i < pointCloudInfo->disparity_len ; ++i){
                pointCloudInfo->disparityToW[i] = (i * ratio_Mat) / baseline + diff;
                }
                break;
        }
    }
    return APC_OK;
}


static int getPointCloud(uint8_t *ImgColor, int CW, int CH,
                  uint8_t *ImgDepth, int DW, int DH, int depthDataType,
                  uint8_t *pPointCloudRGB,
                  float *pPointCloudXYZ,
                  float fNear,
                  float fFar)
{
    int ret = 0;
    PointCloudInfo pointCloudInfo;
    std::vector<CloudPoint> cloud;
    char DateTime[32] = {0};
    static uint32_t yuv_index = 0;
    char fname[256] = {0};
    int i = 0;
    CloudPoint cloudpoint = {0};
    void *pHandleEYSD =  EYSD;
    DEVSELINFO *pDevSelInfo = &g_DevSelInfo;
    
    if (!pHandleEYSD) {
        return -EINVAL;
    }

    memset(DateTime, 0, sizeof(DateTime));
    GetDateTime(DateTime);
    
    ret = getPointCloudInfo(pHandleEYSD, pDevSelInfo, &pointCloudInfo, depthDataType, DH);
    if (ret == APC_OK) {
        ret = APC_GetPointCloud(pHandleEYSD, pDevSelInfo, ImgColor, CW, CH, ImgDepth, DW, DH,
                                &pointCloudInfo, pPointCloudRGB, pPointCloudXYZ, fNear, fFar);
        if (ret == APC_OK) {
            snprintf(fname, sizeof(fname), SAVE_FILE_DIR"cloud_%d_%s.ply", yuv_index++, DateTime);
           
            while(i < (DW * DH * 3)) {
                
                if (isnan(pPointCloudXYZ[i]) || isnan(pPointCloudXYZ[i+1]) || isnan(pPointCloudXYZ[i+2])) {
                    //Do nothing!!
                } else {
                    cloudpoint.r = pPointCloudRGB[i];
                    cloudpoint.g = pPointCloudRGB[i + 1];
                    cloudpoint.b = pPointCloudRGB[i + 2];
                    cloudpoint.x = pPointCloudXYZ[i];
                    cloudpoint.y = pPointCloudXYZ[i + 1];
                    cloudpoint.z = pPointCloudXYZ[i+2];
                    cloud.push_back(cloudpoint);
                }
                i+=3;
                if (i == (DW * DH * 3))
                    break;
            }
            PlyWriter::writePly(cloud, fname);
        }
    }
    
    return ret;
}

int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    uint32_t pixel32 = 0;
    uint8_t *pixel = (uint8_t *)&pixel32;
    int r, g, b;

    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));

    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;

    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;

    pixel[0] = r * 220 / 256;
    pixel[1] = g * 220 / 256;
    pixel[2] = b * 220 / 256;

    return pixel32;
}

int convert_yuv_to_rgb_buffer(uint8_t *yuv, uint8_t *rgb, uint32_t width, uint32_t height)
{
    uint32_t in, out = 0;
    uint32_t pixel_16;
    uint8_t pixel_24[3];
    uint32_t pixel32;
    int y0, u, y1, v;
    for(in = 0; in < width * height * 2; in += 4) {
        pixel_16 =
        yuv[in + 3] << 24 |
        yuv[in + 2] << 16 |
        yuv[in + 1] <<  8 |
        yuv[in + 0];

        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;

        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);

        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];

        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);

        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }

    return 0;
}

static void *get_color_depth_image_func(void *arg)
{
    int ret = APC_OK;
    int64_t cur_tv_sec = 0;
    int64_t cur_tv_usec = 0;
    int64_t first_tv_sec = 0;
    int64_t first_tv_usec = 0;
    int64_t prv_tv_sec = -1;
    int64_t prv_tv_usec = -1;
    int cur_serial_num = -1;
    int pre_serial_num = -1;
    int cur_depth_serial_num = -1;
    uint64_t frame_rate_count = 0;
    uint32_t max_calc_frame_count = 10;
    int mCount = 0;
    bool bFirstReceived = true;
    const char *pre_str = COLOR_DEPTH_STR;
    int64_t diff = 0;
    int s_diff = 0;
    int serial_number = 0;
    int i = 0;
    uint64_t index = 0;
    uint32_t BytesPerPixel = 2; //Only support YUYV
    char *color_file_name = NULL;
    char *depth_file_name = NULL;
    FILE *color_fp = NULL;
    FILE *depth_fp = NULL;
    
    color_file_name = (char *)calloc(MAX_FILE_PATH_LEN, sizeof(char));
    depth_file_name = (char *)calloc(MAX_FILE_PATH_LEN, sizeof(char));
    
    // snprintf(fname, sizeof(fname), SAVE_FILE_PATH"cloud_%d_%s.ply", yuv_index++, DateTime);
    
    CT_DEBUG("color image: [%d x %d @ %d]\n", gColorWidth, gColorHeight, gActualFps);
    CT_DEBUG("depth image: [%d x %d @ %d]\n", gDepthWidth, gDepthHeight, gActualFps);

    if(gColorImgBuf == NULL) {
        gColorImgBuf = (uint8_t*)calloc(gColorWidth * gColorHeight * BytesPerPixel, sizeof(uint8_t));
    }
    if(gColorImgBuf == NULL) {
        CT_DEBUG("alloc ColorImgBuf fail..\n");
        return NULL;
    }
    
    if(gDepthImgBuf == NULL) {
        gDepthImgBuf = (uint8_t*)calloc(gDepthWidth * gDepthHeight * BytesPerPixel, sizeof(uint8_t));
    }
    
    if(gDepthImgBuf == NULL) {
        CT_DEBUG("alloc DepthImgBuf fail..\n");
        return NULL;
    }
    
#if defined(ONLY_PRINT_OVER_DIFF)
    max_calc_frame_count = max_calc_frame_count * 2;
#endif
    
    if (bSnapshot) {
        if (color_file_name && gColorImgBuf) {
            snprintf(color_file_name, MAX_FILE_PATH_LEN, SAVE_FILE_DIR"color_%lux%lu.yuv", gColorWidth, gColorHeight);
            color_fp = fopen(color_file_name, "wb");
            fseek(color_fp, 0, SEEK_SET);
        }
        if (depth_file_name && gDepthImgBuf) {
            snprintf(depth_file_name, MAX_FILE_PATH_LEN, SAVE_FILE_DIR"depth_%lux%lu.yuv", gDepthWidth, gDepthHeight);
            depth_fp = fopen(depth_file_name, "wb");
            fseek(depth_fp, 0, SEEK_SET);
        }
    }
    
    
    while (mCount < 1) {

        ret = APC_Get2ImageWithTimestamp(EYSD, &g_DevSelInfo, (uint8_t*)gColorImgBuf,
                                         (uint8_t *)gDepthImgBuf, &gColorImgSize, &gDepthImgSize,
                                         &cur_serial_num, &cur_depth_serial_num, gDepthType, &cur_tv_sec, &cur_tv_usec);
        if (ret == APC_OK) {
            serial_number = 0;
            if (gColorFormat == 0) {   
                //V4L2_PIX_FMT_YUYV
                for ( i = 0; i < 16; i++ ) {
                    serial_number |= ( *(((uint8_t*)gColorImgBuf)+i)&1) << i;
                }
            } else {
                //V4L2_PIX_FMT_MJPEG
                  serial_number  = *(((uint8_t*)gColorImgBuf)+6)*256 + *(((uint8_t*)gColorImgBuf)+7);
            }
            if (bFirstReceived) {
                bFirstReceived = false;
                RegisterSettings::DM_Quality_Register_Setting(EYSD, &g_DevSelInfo, g_pDevInfo->wPID);
                CT_DEBUG("[%s]SN: [%03d/%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu]\n", pre_str,
                       (int)cur_serial_num, serial_number, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
            }
            if (frame_rate_count == 0) {
                first_tv_sec  = cur_tv_sec;
                first_tv_usec = cur_tv_usec;
            } else {
                diff = ((cur_tv_sec - prv_tv_sec)*1000000+cur_tv_usec)-prv_tv_usec;
                s_diff = cur_serial_num - pre_serial_num;

#if defined(ONLY_PRINT_OVER_DIFF)
                if (gActualFps == 60) {                 
                    if (diff > (16666)) {
                       // CT_DEBUG("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                         //   (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                    }

                } else  if (gActualFps == 30) {
                    if (diff > (33333)) {
                        //CT_DEBUG("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                          //  (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                    }
                }

                if (s_diff > 1) {
                    CT_DEBUG("[%s][%03lu]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n",
                            pre_str, frame_rate_count,
                            (int)cur_serial_num, s_diff,
                           (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                }
#else

                CT_DEBUG("[%s]SN: [%03d/%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu]\n", pre_str,
                       (int)cur_serial_num, serial_number, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
#endif
            }

            if (frame_rate_count == (max_calc_frame_count -1)) {              
                float fltotal_time = 0.0;
                fltotal_time = ((cur_tv_sec - first_tv_sec)*1000000+cur_tv_usec)-first_tv_usec;
#if defined(ONLY_PRINT_OVER_DIFF)
                CT_DEBUG("[%s] %lu usec per %ufs (fps = %6f)\n", pre_str,
                       (unsigned long)fltotal_time, max_calc_frame_count, (1000000 * max_calc_frame_count)/fltotal_time);
#endif
                frame_rate_count = 0;
                mCount ++;
            } else {
                frame_rate_count++;
            }
            prv_tv_sec = cur_tv_sec;
            prv_tv_usec = cur_tv_usec;
            pre_serial_num = cur_serial_num;
            
            if (bSnapshot) {
                if (color_fp && gColorImgBuf)
                    fwrite(gColorImgBuf, sizeof(uint8_t), gColorImgSize, color_fp);
                if (depth_fp && gDepthImgBuf)
                    fwrite(gDepthImgBuf, sizeof(uint8_t), gDepthImgSize, depth_fp);
                
            }
            index++;
            
        }
    }
    
    if(gColorImgBuf){
        CT_DEBUG("free gColorImgBuf : %p\n",gColorImgBuf);
        free(gColorImgBuf);
        gColorImgBuf = NULL;
    }
    
    if(gDepthImgBuf){
        CT_DEBUG("free gDepthImgBuf : %p\n",gDepthImgBuf);
        free(gDepthImgBuf);
        gDepthImgBuf = NULL;
    }
    
    if (color_file_name) {
        free(color_file_name);
        color_file_name = NULL;
    }
    
    if (depth_file_name) {
        free(depth_file_name);
        depth_file_name = NULL;
    }
    
    if (depth_fp) {
        fclose(depth_fp);
        depth_fp = NULL;
    }
    
    if (color_fp) {
        fclose(color_fp);
        color_fp = NULL;
    }
    
    if (bSnapshot) {
        sync();
    }
    
    return NULL;
}

static void *point_cloud_func(void *arg) 
{
    int ret = APC_OK;
    int64_t cur_tv_sec = 0;
    int64_t cur_tv_usec = 0;
    int cur_serial_num = -1;
    int cur_depth_serial_num = -1;
    
    unsigned int count = 0;
    unsigned int max_count = 10;
    
    static unsigned int m_BufferSize = 0;
    
    bool bFirstReceived = true;

    int i = 0;
    uint8_t *pPointCloudRGB = NULL;
    float *pPointCloudXYZ = NULL;

    (void)arg;

    CT_DEBUG("depth image: [%d x %d @ %d]\n", gDepthWidth, gDepthHeight, gActualFps);
    
    pPointCloudRGB = (uint8_t *)malloc(gDepthWidth * gDepthHeight * 3 * sizeof(uint8_t));
    pPointCloudXYZ = (float *)malloc(gDepthWidth * gDepthHeight * 3 * sizeof(float));
    if((pPointCloudRGB == NULL) || (pPointCloudXYZ == NULL)) {
        CT_DEBUG("alloc for pPointCloudRGB or  pPointCloudXYZ fail..\n");
        goto exit;
    }
    
    if(gDepthImgBuf == NULL) {
        m_BufferSize = gDepthWidth * gDepthHeight * 2;
        gDepthImgBuf = (uint8_t*)calloc(m_BufferSize, sizeof(uint8_t));
    }
    if(gDepthImgBuf == NULL) {
        CT_DEBUG("alloc for gDepthImageBuf fail..\n");
        goto exit;
    }

    CT_DEBUG("color image: [%d x %d @ %d]\n", gColorWidth, gColorHeight, gActualFps);
    if(gColorImgBuf == NULL) {
        gColorImgBuf = (uint8_t*)calloc(2 * gColorWidth * gColorHeight , sizeof(uint8_t));
    }
    if(gColorImgBuf == NULL) {
        CT_DEBUG("alloc ColorImgBuf fail..\n");
        goto exit;
    }
    
    if(gColorRGBImgBuf == NULL) {
        gColorRGBImgBuf = (uint8_t*)calloc(3 * gColorWidth * gColorHeight, sizeof(uint8_t));
    }
    if(gColorRGBImgBuf == NULL) {
        CT_DEBUG("alloc gColorRGBImgBuf fail..\n");
        goto exit;
    }

    while (count < max_count) {
        ret = APC_Get2ImageWithTimestamp(EYSD, &g_DevSelInfo, (uint8_t*)gColorImgBuf,
                                         (uint8_t *)gDepthImgBuf, &gColorImgSize, &gDepthImgSize,
                                         &cur_serial_num, &cur_depth_serial_num, gDepthType, &cur_tv_sec, &cur_tv_usec);
        if (ret == APC_OK) {
            if (bFirstReceived){
                bFirstReceived = false;
                RegisterSettings::DM_Quality_Register_Setting(EYSD, &g_DevSelInfo, g_pDevInfo->wPID);
            }
            
            convert_yuv_to_rgb_buffer(gColorImgBuf, gColorRGBImgBuf, gColorWidth, gColorHeight);
            ret = getPointCloud(gColorRGBImgBuf, gColorWidth, gColorHeight,
                                gDepthImgBuf, gDepthWidth, gDepthHeight, gDepthType,
                                pPointCloudRGB, pPointCloudXYZ, g_maxNear, g_maxFar);
        }
        count++;
    }
    
exit:
    if(gDepthImgBuf != NULL){
        CT_DEBUG("free gDepthImgBuf : %p\n",gDepthImgBuf);
        free(gDepthImgBuf);
        gDepthImgBuf = NULL;
     }

     
    if (pPointCloudRGB != NULL) {
        free(pPointCloudRGB);
        pPointCloudRGB = NULL;
    }
    
    if (pPointCloudXYZ != NULL) {
        free(pPointCloudXYZ);
        pPointCloudXYZ = NULL;
    }
    
    if (gColorRGBImgBuf != NULL) {
        free(gColorRGBImgBuf);
        gColorRGBImgBuf = NULL;
    }
     
    return NULL;
    
}

static void *property_bar_test_func(void *arg)
{
    int ret = 0;

    int id = 0;
    int max = 0;
    int min = 0;
    int step = 0;
    int def = 0;
    int flag = 0;
    long int cur_val = 0;
    long int set_val = 0;

    (void)arg;

    id = CT_PROPERTY_ID_AUTO_EXPOSURE_MODE_CTRL;
    ret = APC_GetCTPropVal(EYSD, &g_DevSelInfo, id, &cur_val);
    if (ret == APC_OK) {
        CT_DEBUG("AE_MODE[cur_val] = [%ld]\n", cur_val);
        //NOTE: The 0x01 means the 'EXPOSURE_MANUAL' 
        if (cur_val != 0x01) {
            set_val = 0x01;
            ret = APC_SetCTPropVal(EYSD, &g_DevSelInfo, id, set_val);
            if (ret != APC_OK) {
                CT_DEBUG("Failed to call APC_SetCTPropVal() for (%d) !! (%d)\n", id, ret);
            } else {
                ret = APC_GetCTPropVal(EYSD, &g_DevSelInfo, id, &cur_val);
                if (ret == APC_OK) {
                    CT_DEBUG("AE_MODE[cur_val] = [%ld] (after set %ld)\n", cur_val, set_val);
                } else {
                    CT_DEBUG("Failed to call APC_GetCTPropVal() for (%d) !! (%d)\n", id, ret);
                }
            }
            ret = APC_GetCTRangeAndStep(EYSD, &g_DevSelInfo, id, &max, &min, &step, &def, &flag);
            if (ret == APC_OK) {
                CT_DEBUG("AE_MODE[max, min, setp, def, flag] = [%d, %d, %d, %d, %d]\n", max, min, step, def, flag);
            } else {
                CT_DEBUG("Failed to call APC_GetCTRangeAndStep() for (%d) !! (%d)\n", id, ret);
            }
        }
    } else {
        CT_DEBUG("Failed to call APC_GetCTPropVal() for (%d) !! (%d)\n", id, ret);
    }

    id = PU_PROPERTY_ID_BRIGHTNESS_CTRL;
    ret = APC_GetPURangeAndStep(EYSD, &g_DevSelInfo, id, &max, &min, &step, &def, &flag);
    if (ret == APC_OK) {
        printf ("BR[max, min, setp, def, flag] = [%d, %d, %d, %d, %d]\n", max, min, step, def, flag);
    } else {
        CT_DEBUG("Failed to call APC_GetPURangeAndStep() for (%d) !! (%d)\n", id, ret);
    }

    ret = APC_GetPUPropVal(EYSD, &g_DevSelInfo, id, &cur_val);
    if (ret == APC_OK) {
        CT_DEBUG("BR[cur_val] = [%ld]\n", cur_val);
    } else {
        CT_DEBUG("Failed to call APC_GetPUPropVal() for (%d) !! (%d)\n", id, ret);
    }

    set_val = (max + min)/2;
    ret = APC_SetPUPropVal(EYSD, &g_DevSelInfo, id, set_val);
    if (ret == APC_OK) {
        ret = APC_GetPUPropVal(EYSD, &g_DevSelInfo, id, &cur_val);
        if (ret == APC_OK) {
            CT_DEBUG("BR[cur_val] = [%ld] (after set %ld)\n", cur_val, set_val);
        } else {
            CT_DEBUG("Failed to call APC_GetPUPropVal() for (%d) !! (%d)\n", id, ret);
        }
    } else {
        CT_DEBUG("Failed to call APC_SetPUPropVal() for (%d) !! (%d)\n", id, ret);
    }

    
    return NULL;
}

