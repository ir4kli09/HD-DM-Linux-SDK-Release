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

#include "PlyWriter.h"
#include "eSPDI.h"

#include "jpeglib.h"
#include <turbojpeg.h>
#include "ColorPaletteGenerator.h"
#include "RegisterSettings.h"

#define USE_PTHREAD_TO_GET_COLOR_AND_DEPTH_IMAGE 1

#define DEFAULT_DEVICE_INDEX		        (0)
#define DEFAULT_COLOR_IMG_FORMAT	PIX_FMT_MJPEG
#define DEFAULT_COLOR_IMG_WIDTH		(1280)
#define DEFAULT_COLOR_IMG_HEIGHT	(720)
#define DEFAULT_DEPTH_IMG_WIDTH		(640)
#define DEFAULT_DEPTH_IMG_HEIGHT	(720)
#define UNUSED(x) (void)(x)

#define APC_DEPTH_DATA_OFF_RAW			        0 /* raw (depth off, only raw color) */
#define APC_DEPTH_DATA_DEFAULT			        0 /* raw (depth off, only raw color) */
#define APC_DEPTH_DATA_8_BITS				1 /* rectify, 1 byte per pixel */
#define APC_DEPTH_DATA_14_BITS				2 /* rectify, 2 byte per pixel */
#define APC_DEPTH_DATA_8_BITS_x80		3 /* rectify, 2 byte per pixel but using 1 byte only */
#define APC_DEPTH_DATA_11_BITS				4 /* rectify, 2 byte per pixel but using 11 bit only */
#define APC_DEPTH_DATA_OFF_RECTIFY		5 /* rectify (depth off, only rectify color) */
#define APC_DEPTH_DATA_8_BITS_RAW		6 /* raw */
#define APC_DEPTH_DATA_14_BITS_RAW		7 /* raw */
#define APC_DEPTH_DATA_8_BITS_x80_RAW	8 /* raw */
#define APC_DEPTH_DATA_11_BITS_RAW		9 /* raw */
#define APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY     13// multi-baseline

#define TEST_RUN_NUMBER  10

#define SAVE_FILE_PATH "./out_img/"

//s:[eys3D] 20200615 implement ZD table
#define COLOR_PALETTE_MAX_COUNT 16384
#define ONLY_PRINT_OVER_DIFF 1
#define COLOR_STR "COLOR"
#define DEPTH_STR "DEPTH"
static int g_v4l2_buffer_quque_size =  32;

bool DEBUG_LOG = false;

DEPTH_TRANSFER_CTRL g_depth_output =  DEPTH_IMG_COLORFUL_TRANSFER;
BYTE g_pzdTable[APC_ZD_TABLE_FILE_SIZE_11_BITS];
unsigned short g_distance_table[APC_ZD_TABLE_FILE_SIZE_11_BITS /  2];

RGBQUAD     *g_ColorPaletteZ14 = nullptr;
RGBQUAD     *g_GrayPaletteZ14 = nullptr;

unsigned short g_maxFar;
unsigned short g_maxNear;
int g_zdTableInfo_index = 0;

DEVINFORMATION *g_pDevInfo =nullptr;
DEVSELINFO g_DevSelInfo;
//e:[eys3D] 20200615 implement ZD table

void* EYSD = NULL;
//pthread_mutex_t save_file_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t gcolor_thread_id = (pthread_t)-1;
static pthread_t gdepth_thread_id =(pthread_t) -1;

static int gColorFormat = 1; // 0: YUYV, 1: MJPEG
static int gColorWidth = 1280;
static int gColorHeight = 720;

static bool snapShot_color = true;
static bool snapShot_depth = true;
static bool bTesting_color = true;
static bool bTesting_depth = true;
static bool bTestEnd_color = false;
static bool bTestEnd_depth = false;

typedef enum {
    ERROR_NONE				                        = 0,       /**< Successful */
    ERROR_NO_SUCH_DEVICE		        = -1,	  /**< No such device or address */
    ERROR_NOT_SUPPORTED		        = -2,	  /**< Not supported in this device */
    ERROR_NOT_PERMITTED		        = -3,	  /**< Operation not permitted */
    ERROR_PERMISSION_DENIED	= -4,	  /**< Permission denied */
    ERROR_RESOURCE_BUSY		        = -5,	  /**< Device or resource busy */
    ERROR_ALREADY_IN_PROGRESS	= -6,	  /**< Operation already in progress */
    ERROR_OUT_OF_MEMORY		        = -7,	  /**< Out of memory */
    ERROR_INVALID_PARAMETER	= -8,	  /**< Invalid parameter */
    ERROR_INVALID_OPERATION	= -9,	  /**< Invalid Operation */
    ERROR_IO_ERROR			                = -10,/**< IO ERROR */
    ERROR_TIMED_OUT			                = -11,/**< Time out */
    ERROR_UNKNOWN				                = -12,/**< Unknown */
} error_e;

DEPTH_TRANSFER_CTRL gDepth_Transfer_ctrl = DEPTH_IMG_NON_TRANSFER;

static int gDepthWidth = 640; // Depth is only YUYV format
static int gDepthHeight = 720;
static DEVSELINFO gsDevSelInfo;
PAPC_STREAM_INFO gpsStreamColorInfo = NULL; 
PAPC_STREAM_INFO gpsStreamDepthInfo = NULL;

static int gActualFps = 30;
static unsigned char *gColorImgBuf = NULL;
static unsigned char *gDepthImgBuf = NULL;

//s:[eys3D] 20200610 implement to save raw data to RGB format
static unsigned char *gTempImgBuf = NULL;
static unsigned char *gColorRGBImgBuf = NULL;
static unsigned char *gDepthRGBImgBuf = NULL;
//e:[eys3D] 20200610 implement to save raw data to RGB format
static unsigned long int gColorImgSize = 0;
static unsigned long int gDepthImgSize = 0;	
static int gDepthDataType = APC_DEPTH_DATA_8_BITS;
static int gColorSerial = 0;
static int gDepthSerial = 0;

//s:[eys3D] 20200610 definition functions
int convert_yuv_to_rgb_pixel(int y, int u, int v);
int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height);
int save_file(unsigned char *buf, int size, int width, int height,int type, bool saveRawData);
int get_product_name(char *path, char *out);
void print_APC_error(int error);
static int error_msg(int error);
static void setupDepth(void);
static void *pfunc_thread_close(void *arg);
static long long calcByGetTimeOfDay() ;

int tjpeg2yuv(unsigned char* jpeg_buffer, int jpeg_size, unsigned char** yuv_buffer, int* yuv_size, int* yuv_type);
int tyuv2rgb(unsigned char* yuv_buffer, int yuv_size, int width, int height, int subsample, unsigned char** rgb_buffer, int* rgb_size);
int getZDtable(DEVINFORMATION *pDevInfo, DEVSELINFO DevSelInfo, int depthHeight,int colorHeight);


int getPointCloud(void *pHandleEYSD, DEVSELINFO *pDevSelInfo, unsigned char *ImgColor, int CW, int CH,
                  unsigned char *ImgDepth, int DW, int DH, int depthDataType,
                  unsigned char *pPointCloudRGB,
                  float *pPointCloudXYZ,
                  float fNear,
                  float fFar);

int saveDepth2rgb(unsigned char *m_pDepthImgBuf, unsigned char *m_pRGBBuf, unsigned int m_nImageWidth, unsigned int m_nImageHeight);
void setHypatiaVideoMode(int mode);
int setupIR(unsigned short value);
void UpdateD8bitsDisplayImage_DIB24(RGBQUAD *pColorPalette, unsigned char *pDepth, unsigned char *pRGB, int width, int height);
void UpdateD11DisplayImage_DIB24(const RGBQUAD* pColorPalette, const unsigned char *pDepth, unsigned char *pRGB, int width, int height);
void UpdateZ14DisplayImage_DIB24(RGBQUAD *pColorPaletteZ14, unsigned char *pDepthZ14, unsigned char *pDepthDIB24, int cx, int cy);
char* PidToModuleName(unsigned short pid);

static void init_device(void);
static void *test_color_time_stamp(void *arg);
static void *test_depth_time_stamp(void *arg);
static void *test_color_depth_time_stamp(void *arg);
static void open_device_default(bool two_open, int fps, WORD videoMode);
static void open_device(void);
static void get_color_image(void);
static void get_depth_image(void);
static void get_point_cloud(void);
static void close_device(void);
static void release_device(void);
static void SetSnapShotFlag();
static void setupFWRegister_EX8038();
static void setupFWRegister();
static void setupFWRegister();
static void readFWRegister();
static void SetCounterMode();
static void GetCounterMode();
static void setV4L2buffer();
static void setIRValue();
//e:[eys3D] 20200610 definition functions
static void test_file_saving(APCImageType::Value type);
static int TransformDepthDataType(int *nDepthDataType, bool bRectifyData);
static unsigned int gCameraPID = 0xffff;

#define _ENABLE_INTERACTIVE_UI_ 1
//#define _ENABLE_PROFILE_UI_ 1
//#define _ENABLE_FILESAVING_DEMO_UI_ 1
//#define _ENABLE_FILESAVING_DEMO_POINT_CLOUD_UI_ 1
#define DEFAULT_SAVING_FRAME_COUNT 150

int main(void)
{
    int input = 0;
    do {
        printf("\n-----------------------------------------\n");
        printf("Software version : %s\n",APC_VERSION);
        printf("Please choose fllowing steps:\n");
#if defined(_ENABLE_INTERACTIVE_UI_)        
        printf("0. Init device\n");
        printf("1. Open device\n");
        printf("2. Get Color Image\n");
        printf("3. Get Depth Image\n");
        printf("4. Get Color and Depth Image\n");
        printf("5. Get Point Cloud\n");
        printf("6. Close Device\n");
        printf("7. Release Device\n");
        printf("8. SnapShot\n");
        printf("9. FW Reg Write\n");
        printf("10. FW Reg Read\n");
        printf("11. set IR value\n");
#endif
#if defined(_ENABLE_PROFILE_UI_)
        printf("12. test color+depth (1280X720@60, APC_DEPTH_DATA_11_BITS)\n");
        printf("13. test color       (1280X720@60, APC_DEPTH_DATA_11_BITS)\n");
        printf("14. test color+depth (1280X720@60, APC_DEPTH_DATA_11_BITS)(thread)\n");
        printf("15. test depth       (1280X720@60, APC_DEPTH_DATA_11_BITS)\n");
        printf("16. test color+depth (1280X720@30, APC_DEPTH_DATA_11_BITS)\n");
        printf("17. test color       (1280X720@30, APC_DEPTH_DATA_11_BITS)\n");
        printf("18. test color+depth (1280X720@30, APC_DEPTH_DATA_11_BITS)(thread)\n");
        printf("19. test depth       (1280X720@30, APC_DEPTH_DATA_11_BITS)\n");
#endif
#if defined(_ENABLE_FILESAVING_DEMO_UI_)
        printf("20. save file demo   (1280X720@30, APC_DEPTH_DATA_14_BITS)\n");
#endif
#if defined(_ENABLE_FILESAVING_DEMO_POINT_CLOUD_UI_)
        printf("21. point cloud demo   (1280X720@30, APC_DEPTH_DATA_14_BITS)\n");
#endif
        printf("255. exit)\n");
        scanf("%d", &input);
        switch(input) {
#if defined(_ENABLE_INTERACTIVE_UI_)
        case 0:
            init_device();
            break;
        case 1:
            open_device();
            break;
        case 2:
            get_color_image();
            if (gcolor_thread_id != (pthread_t)-1)
                pthread_join(gcolor_thread_id, NULL);
            break;
        case 3:
            get_depth_image();
            if (gdepth_thread_id != (pthread_t)-1)
                pthread_join(gdepth_thread_id, NULL);
            break;
        case 4:
            get_color_image();
            get_depth_image();
            if (gdepth_thread_id != (pthread_t)-1)
                pthread_join(gdepth_thread_id, NULL);
            if (gcolor_thread_id != (pthread_t)-1)
                pthread_join(gcolor_thread_id, NULL);
            break;
        case 5:
            get_point_cloud();
            break;
        case 6:
            close_device();
            break;
        case 7:
            release_device();
            break;
        case 8:
           SetSnapShotFlag();
           break;
        case 9:
            setupFWRegister();
            break;
        case 10:
            readFWRegister();
            break;
        case 11:
            setIRValue();
            break;
#endif
#if defined(_ENABLE_PROFILE_UI_)
        case 12:
            init_device();
            open_device_default(true, 60, APC_DEPTH_DATA_11_BITS);
            test_color_time_stamp(NULL);
            test_depth_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 13:
            init_device();
            open_device_default(true, 60, APC_DEPTH_DATA_11_BITS);
            test_color_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 14:
            init_device();
            open_device_default(true, 60, APC_DEPTH_DATA_11_BITS);
            test_color_depth_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 15:
            init_device();
            open_device_default(true, 60, APC_DEPTH_DATA_11_BITS);
            test_depth_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 16:
            init_device();
            open_device_default(true, 30, APC_DEPTH_DATA_11_BITS);
            test_color_time_stamp(NULL);
            test_depth_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 17:
            init_device();
            open_device_default(true, 30, APC_DEPTH_DATA_11_BITS);
            test_color_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 18:
            init_device();
            open_device_default(true, 30, APC_DEPTH_DATA_11_BITS);
            test_color_depth_time_stamp(NULL);
            close_device();
            release_device();
            break;
        case 19:
            init_device();
            open_device_default(true, 30, APC_DEPTH_DATA_11_BITS);
            test_depth_time_stamp(NULL);
            close_device();
            release_device();
            break;
#endif
#if defined(_ENABLE_FILESAVING_DEMO_UI_)
        case 20:
            init_device();
            open_device_default(true, 30, APC_DEPTH_DATA_14_BITS /* Distance mm*/);
            test_file_saving(APCImageType::Value::COLOR_YUY2);
            close_device();
            release_device();
            printf("Sync the filesytem !! Please wait a minute !!\n");
            sync();
            break;
#endif
#if defined(_ENABLE_FILESAVING_DEMO_POINT_CLOUD_UI_)
        case 21:
            init_device();
            open_device_default(true, 30, APC_DEPTH_DATA_14_BITS /* Distance mm*/);
            get_point_cloud();
            close_device();
            release_device();
            break;
#endif
        case 255:
            //release_device();
            return 0;
            break; 
        default:
            continue;
        }
    } while(1);

	return 0;
}


int GetDateTime(char * psDateTime){
    time_t timep; 
    struct tm *p; 
    
    time(&timep); 
    p=localtime(&timep); 

    sprintf(psDateTime,"%04d%02d%02d_%02d%02d%02d", (1900+p->tm_year), (1+p->tm_mon), p->tm_mday,                                                                                                                                                                                                     
            p->tm_hour, p->tm_min, p->tm_sec);
    return 0;
}

static void *test_color_depth_time_stamp(void *arg)
{
    pthread_t color_thread_id;
    pthread_attr_t color_thread_attr;
    struct sched_param color_thread_param;
    
    pthread_t depth_thread_id;
    pthread_attr_t depth_thread_attr;
    struct sched_param depth_thread_param;
    
    pthread_attr_init(&color_thread_attr);
    pthread_attr_setschedpolicy(&color_thread_attr, SCHED_FIFO);
    pthread_attr_getschedparam (&color_thread_attr, &color_thread_param);
    color_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&color_thread_attr, &color_thread_param);

    pthread_attr_init(&depth_thread_attr);
    pthread_attr_setschedpolicy(&depth_thread_attr, SCHED_FIFO);
    pthread_attr_getschedparam (&depth_thread_attr, &depth_thread_param);
    depth_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&depth_thread_attr, &depth_thread_param);
    
    pthread_create(&color_thread_id, &color_thread_attr, test_color_time_stamp, NULL);
    pthread_create(&depth_thread_id, &depth_thread_attr, test_depth_time_stamp, NULL);
    
    printf("Wait for finish of depth thread..\n");
    pthread_join(depth_thread_id, NULL);
    printf("Wait for finish of color thread..\n");
   pthread_join(color_thread_id, NULL);
    
    return NULL;
}


static void *test_color_time_stamp(void *arg)
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
    uint64_t frame_rate_count = 0;
    unsigned int max_calc_frame_count = 150;
    int mCount = 0;
    bool bFirstReceived = true;
    const char *pre_str = COLOR_STR;
    int64_t diff = 0;
    int s_diff = 0;
    int serial_number = 0;
    int i = 0;
    
    printf("\ncolor image: [%d x %d @ %d]\n", gColorWidth, gColorHeight, gActualFps);
    
    if(gColorImgBuf == NULL) {
        gColorImgBuf = (unsigned char*)calloc(2 * gColorWidth * gColorHeight , sizeof(unsigned char));
    }
    if(gColorImgBuf == NULL) {
        printf("alloc ColorImgBuf fail..\n");
        return NULL;
    }

#if defined(ONLY_PRINT_OVER_DIFF)
    max_calc_frame_count = 1000;
#endif
    
    while (mCount < 1)
    {
  
        ret = APC_GetColorImageWithTimestamp(EYSD, &gsDevSelInfo,
                                                (BYTE*)gColorImgBuf, &gColorImgSize, &cur_serial_num, 0, &cur_tv_sec, &cur_tv_usec);
        if ((ret == APC_OK) /*&& (gColorSerial > 0)*/) {
            serial_number = 0;
            /*TODO: 
             * The following serail_number is for !AXES1 type models (ex: KIWI, PUMA)
             * The other type models should be implmented in the feature.
             */
            if (gColorFormat == 0) {   
                //V4L2_PIX_FMT_YUYV
                for ( i = 0; i < 16; i++ ) {
                    serial_number |= ( *(((unsigned char*)gColorImgBuf)+i)&1) << i;
                }
            } else {
                //V4L2_PIX_FMT_MJPEG
                  serial_number  = *(((unsigned char*)gColorImgBuf)+6)*256 + *(((unsigned char*)gColorImgBuf)+7);
            }
            if (bFirstReceived) {
                bFirstReceived = false;
                printf("[%s]SN: [%03d/%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu]\n", pre_str,
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
                       // printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                         //   (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                    }

                } else  if (gActualFps == 30) {
                    if (diff > (33333)) {
                        //printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                          //  (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                    }
                }

                if (s_diff > 1) {
                    printf("[%s][%03lu]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n",
                            pre_str, frame_rate_count,
                            (int)cur_serial_num, s_diff,
                           (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                }
#else

                printf("[%s]SN: [%03d/%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu]\n", pre_str,
                       (int)cur_serial_num, serial_number, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
#endif
            }

            if (frame_rate_count == (max_calc_frame_count -1)) {              
                float fltotal_time = 0.0;
                fltotal_time = ((cur_tv_sec - first_tv_sec)*1000000+cur_tv_usec)-first_tv_usec;
#if defined(ONLY_PRINT_OVER_DIFF)
                printf("[%s] %lu usec per %ufs (fps = %6f)\n", pre_str,
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
        }
    }
    if(gColorImgBuf != NULL){
        if(DEBUG_LOG) {
            printf("free gColorImgBuf : %p\n",gColorImgBuf);
        }
        free(gColorImgBuf);
        gColorImgBuf = NULL;
    }
    return NULL;
}

static void *test_depth_time_stamp(void *arg)
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
    uint64_t frame_rate_count = 0;
    unsigned int max_calc_frame_count = 150;
    int mCount = 0;
    static unsigned int m_BufferSize = 0;
    bool bFirstReceived = true;
    bool bAfterQCFG = false;
    bool bSecondReceived = false;
    const char *pre_str = DEPTH_STR;
    int64_t diff = 0;
    int s_diff = 0;
    int serial_number = 0;
    int i = 0;

    (void)arg;

    printf("\ndepth image: [%d x %d @ %d]\n", gDepthWidth, gDepthHeight, gActualFps);
    
    if(gDepthImgBuf == NULL) {
         if(gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                m_BufferSize = 2 * gDepthWidth  * 2*gDepthHeight;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        } else {
                m_BufferSize = gDepthWidth * gDepthHeight * 2;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        }
    }
    
    if(gDepthImgBuf == NULL) {
        printf("alloc for gDepthImageBuf fail..\n");
        return NULL;
    }
#if defined(ONLY_PRINT_OVER_DIFF)
    max_calc_frame_count = 1000;
#endif
    while (mCount < 1)
    {
        ret = APC_GetDepthImageWithTimestamp(EYSD, &gsDevSelInfo, (BYTE*)gDepthImgBuf, &gDepthImgSize, &cur_serial_num, gDepthDataType, &cur_tv_sec, &cur_tv_usec);
        
        if(gDepthImgSize > m_BufferSize) {
            printf("Alloc size : %lu, but get depth size : %lu, check FW and close the application.\n",m_BufferSize, gDepthImgSize);
            break;
        }
        if (ret == APC_OK) {
            serial_number = 0;
            /*TODO: 
             * The following serail_number is for !AXES1 type models (ex: KIWI, PUMA)
             * The other type models should be implmented in the feature.
             */
            if (gColorFormat == 0) {   
                //V4L2_PIX_FMT_YUYV
                for ( i = 0; i < 16; i++ ) {
                    serial_number |= ( *(((unsigned char*)gDepthImgBuf)+i)&1) << i;
                }
            } else {
                //V4L2_PIX_FMT_MJPEG
                  serial_number  = *(((unsigned char*)gDepthImgBuf)+6)*256 + *(((unsigned char*)gDepthImgBuf)+7);
            }
            if (bAfterQCFG) {
                //printf("[%s]After setting of Quality, s_num: [%d] (0x%08x)\n", __func__, (int)cur_serial_num, (unsigned int)cur_serial_num);
                bAfterQCFG = false;
            }
            
            if (bSecondReceived) {
                bSecondReceived = false;
                //printf("[%s]SN: [%d],   TS: [%lu]\n", pre_str, (int)cur_serial_num, (cur_tv_sec * 1000000 + cur_tv_usec));
            }
            
            if (bFirstReceived){
                bFirstReceived = false;
                printf("[%s]SN: [%03d/%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu]\n", pre_str,
                       (int)cur_serial_num, serial_number, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                if (RegisterSettings::DM_Quality_Register_Setting(EYSD, &gsDevSelInfo, g_pDevInfo[gsDevSelInfo.index].wPID) == 0) {
                    bAfterQCFG = true;
                }
                bSecondReceived = true;
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
                         //printf("[%s]t_diff: [%lu] usec (s_diff: [%d])\n", __func__, (unsigned long)diff, s_diff);
                    }
                } else  if (gActualFps == 30) {
                    if (diff > (33333)) {
                        //printf("[%s]t_diff: [%lu] usec (s_diff: [%d])\n", __func__, (unsigned long)diff, s_diff);
                    }
                }
                if (s_diff > 1) {
                    printf("[%s][%03lu]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n",
                            pre_str, frame_rate_count,
                            (int)cur_serial_num, s_diff,
                           (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                }
#else     
                printf("[%s]SN: [%03d/%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu]\n", pre_str,
                       (int)cur_serial_num, serial_number, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
#endif
            }

            if (frame_rate_count == (max_calc_frame_count -1)) {              
                float fltotal_time = 0.0;
                fltotal_time = ((cur_tv_sec - first_tv_sec)*1000000+cur_tv_usec)-first_tv_usec;
#if defined(ONLY_PRINT_OVER_DIFF)
                printf("[%s] %lu usec per %ufs (fps = %6f)\n", pre_str,
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
        }
    }
    
    
        //s:[eys3D] 20200610 implement to save raw data to RGB format
    if(gDepthImgBuf != NULL){
            if(DEBUG_LOG) {
                printf("free gDepthImgBuf : %p\n",gDepthImgBuf);
            }
            free(gDepthImgBuf);
            gDepthImgBuf = NULL;
     }

    return NULL;
}


int saveRawFile(const char *pFilePath, BYTE *pBuffer, unsigned int sizeByte)
{
    if (!pFilePath || !pBuffer) { 
        return APC_NullPtr;
    }

    FILE *pFile = fopen(pFilePath, "wb");
    if (!pFile) return APC_NullPtr;

    fseek(pFile, 0, SEEK_SET);
    fwrite(&pBuffer[0], sizeof(unsigned char), sizeByte, pFile);
    fclose(pFile);
    return APC_OK;
}

static void *pthread_saving_depth(void *param) {
    int ret = APC_OK;
    int64_t cur_tv_sec = 0;
    int64_t cur_tv_usec = 0;
    int64_t first_tv_sec = 0;
    int64_t first_tv_usec = 0;
    int64_t prv_tv_sec = -1;
    int64_t prv_tv_usec = -1;
    int cur_serial_num = -1;
    int pre_serial_num = -1;
    uint64_t frame_rate_count = 0;
    unsigned int max_calc_frame_count = DEFAULT_SAVING_FRAME_COUNT;
    int mCount = 0;
    static unsigned int m_BufferSize = 0;
    bool bFirstReceived = true;
    bool bAfterQCFG = false;
    bool bSecondReceived = false;
    const char *pre_str = DEPTH_STR;
    int64_t diff = 0;
    int s_diff = 0;

    (void)param;
    std::string fileName = "";

    printf("\ndepth image: [%d x %d @ %d], snapshot %u frames!!\n", gDepthWidth, gDepthHeight, gActualFps, max_calc_frame_count);
    
    if(gDepthImgBuf == NULL) {
         if(gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                m_BufferSize = 2 * gDepthWidth  * 2 * gDepthHeight;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        } else {
                m_BufferSize = gDepthWidth * gDepthHeight * 2;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        }
    }
    
    if(gDepthImgBuf == NULL) {
        printf("alloc for gDepthImageBuf fail..\n");
        return NULL;
    }

    while (mCount < 1)
    {
        ret = APC_GetDepthImageWithTimestamp(EYSD, &gsDevSelInfo, (BYTE*)gDepthImgBuf, &gDepthImgSize, &cur_serial_num, 
                                                gDepthDataType, &cur_tv_sec, &cur_tv_usec);
        
        if(gDepthImgSize > m_BufferSize) {
            printf("Alloc size : %lu, but get depth size : %lu, check FW and close the application.\n",m_BufferSize, gDepthImgSize);
            break;
        }
        if (ret == APC_OK) {
            
            if (bAfterQCFG) {
                //printf("[%s]After setting of Quality, s_num: [%d] (0x%08x)\n", __func__, (int)cur_serial_num, (unsigned int)cur_serial_num);
                bAfterQCFG = false;
            }
            
            if (bSecondReceived) {
                bSecondReceived = false;
                //printf("[%s]SN: [%d],   TS: [%lu]\n", pre_str, (int)cur_serial_num, (cur_tv_sec * 1000000 + cur_tv_usec));
            }
            
            if (bFirstReceived){
                bFirstReceived = false;
                printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                       (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                if (RegisterSettings::DM_Quality_Register_Setting(EYSD, &gsDevSelInfo, g_pDevInfo[gsDevSelInfo.index].wPID) == 0) {
                    bAfterQCFG = true;
                }
                bSecondReceived = true;
            }
            
            // 001 Take care file name
            fileName.append(SAVE_FILE_PATH"DEPTH");
            fileName.append(std::to_string(cur_serial_num));
            fileName.append("-");
            fileName.append(std::to_string(cur_tv_sec * 1000000 + cur_tv_usec));
            saveRawFile(fileName.c_str(), gDepthImgBuf, gDepthImgSize);
            fileName.clear();

            if (frame_rate_count == 0) {
                first_tv_sec  = cur_tv_sec;
                first_tv_usec = cur_tv_usec;

            } else {
                diff = ((cur_tv_sec - prv_tv_sec)*1000000+cur_tv_usec)-prv_tv_usec;
                s_diff = cur_serial_num - pre_serial_num;
#if defined(ONLY_PRINT_OVER_DIFF)
                if (gActualFps == 60) {
                    if (diff > (16666)) {
                         //printf("[%s]t_diff: [%lu] usec (s_diff: [%d])\n", __func__, (unsigned long)diff, s_diff);
                    }
                } else  if (gActualFps == 30) {
                    if (diff > (33333)) {
                        //printf("[%s]t_diff: [%lu] usec (s_diff: [%d])\n", __func__, (unsigned long)diff, s_diff);
                    }
                }
                /*
                if (s_diff > 1)
                    printf("[%s][%d]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str, frame_rate_count,
                        (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                        */
#else     
                //printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                  //      (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
#endif
            }

            if (frame_rate_count == (max_calc_frame_count -1)) {
                float fltotal_time = 0.0;
                fltotal_time = ((cur_tv_sec - first_tv_sec)*1000000+cur_tv_usec)-first_tv_usec;
                printf("[%s] %lu usec per %ufs (fps = %6f)\n", pre_str,
                    (unsigned long)fltotal_time, max_calc_frame_count,(1000000*max_calc_frame_count)/fltotal_time);
                frame_rate_count = 0;
                mCount ++;
            } else {
                frame_rate_count++;
            }
            prv_tv_sec = cur_tv_sec;
            prv_tv_usec = cur_tv_usec;
            pre_serial_num = cur_serial_num;
        }
    }
    
        //s:[eys3D] 20200610 implement to save raw data to RGB format
    if(gDepthImgBuf != NULL){
            if(DEBUG_LOG) {
                printf("free gDepthImgBuf : %p\n",gDepthImgBuf);
            }
            free(gDepthImgBuf);
            gDepthImgBuf = NULL;
     }
}

static void *pthread_saving_color(void *param) {
    int ret = APC_OK;
    int64_t cur_tv_sec = 0;
    int64_t cur_tv_usec = 0;
    int64_t first_tv_sec = 0;
    int64_t first_tv_usec = 0;
    int64_t prv_tv_sec = -1;
    int64_t prv_tv_usec = -1;
    int cur_serial_num = -1;
    int pre_serial_num = -1;
    uint64_t frame_rate_count = 0;
    unsigned int max_calc_frame_count = DEFAULT_SAVING_FRAME_COUNT;
    int mCount = 0;
    bool bFirstReceived = true;
    const char *pre_str = COLOR_STR;
    int64_t diff = 0;
    int s_diff = 0;
    std::string fileName = "";

    printf("\ncolor image: [%d x %d @ %d], snapshot %u frames!!\n", gColorWidth, gColorHeight, gActualFps, max_calc_frame_count);
    

    if(gColorImgBuf == NULL) {
        gColorImgBuf = (unsigned char*)calloc(2 * gColorWidth * gColorHeight , sizeof(unsigned char));
    }
    if(gColorImgBuf == NULL) {
        printf("alloc ColorImgBuf fail..\n");
        return NULL;
    }

    const size_t RGB_SIZE = 3, saving_size = gColorWidth * gColorHeight * RGB_SIZE;

    BYTE* writingBuffer = new BYTE[gColorWidth * gColorHeight * RGB_SIZE];
    auto imageType = *(APCImageType::Value*)(param);

    while (mCount < 1)
    {
        // 000 Take out color
        ret = APC_GetColorImageWithTimestamp(EYSD, &gsDevSelInfo,
                                                (BYTE*)gColorImgBuf, &gColorImgSize, &cur_serial_num, 0, &cur_tv_sec, &cur_tv_usec);
        if ((ret == APC_OK) /*&& (gColorSerial > 0)*/) {
            if (bFirstReceived) {
                bFirstReceived = false;
                printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                       (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
            }
            
            // 000 Convert from to BGR color
            int convertRGBRet = APC_ColorFormat_to_RGB24(EYSD, &gsDevSelInfo, writingBuffer, gColorImgBuf, gColorImgSize,
                                                            gColorWidth, gColorHeight, imageType);
            // 000 Convert from to RGB color
            // int convertBGRRet = APC_ColorFormat_to_BGR24(EYSD, &gsDevSelInfo, writingBuffer, gColorImgBuf, gColorImgSize,
            //                                                 gColorWidth, gColorHeight, imageType);

            fileName.append(SAVE_FILE_PATH"COLOR");
            fileName.append(std::to_string(cur_serial_num));
            fileName.append("-");
            fileName.append(std::to_string(cur_tv_sec * 1000000 + cur_tv_usec));
            saveRawFile(fileName.c_str(), writingBuffer, saving_size);
            fileName.clear();

            if (frame_rate_count == 0) {
                first_tv_sec  = cur_tv_sec;
                first_tv_usec = cur_tv_usec;
            } else {
                diff = ((cur_tv_sec - prv_tv_sec)*1000000+cur_tv_usec)-prv_tv_usec;
                s_diff = cur_serial_num - pre_serial_num;

#if defined(ONLY_PRINT_OVER_DIFF)
                if (gActualFps == 60) {                 
                    if (diff > (16666)) {
                       // printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                         //   (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                    }

                } else  if (gActualFps == 30) {
                    if (diff > (33333)) {
                        //printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                          //  (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                    }
                }
                /*
                if (s_diff > 1)
                    printf("[%s][%d]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str, frame_rate_count,
                        (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
                        */
#else
                
               // printf("[%s]SN: [%03d],  SN_DIFF: [%03d],  TS: [%lu],  TS_DIFF: [%lu] \n", pre_str,
                 //       (int)cur_serial_num, s_diff, (cur_tv_sec * 1000000 + cur_tv_usec), diff);
#endif
            }

            if (frame_rate_count == (max_calc_frame_count -1)) {              
                float fltotal_time = 0.0;
                fltotal_time = ((cur_tv_sec - first_tv_sec)*1000000+cur_tv_usec)-first_tv_usec;
                printf("[%s] %lu usec per %ufs (fps = %6f)\n", pre_str,
                       (unsigned long)fltotal_time, max_calc_frame_count,(1000000*max_calc_frame_count)/fltotal_time);
                frame_rate_count = 0;
                mCount ++;
            } else {
                frame_rate_count++;
            }
            prv_tv_sec = cur_tv_sec;
            prv_tv_usec = cur_tv_usec;
            pre_serial_num = cur_serial_num;
        }
    }

    delete[] writingBuffer;
    
    if(gColorImgBuf != NULL){
        if(DEBUG_LOG) {
            printf("free gColorImgBuf : %p\n",gColorImgBuf);
        }
        free(gColorImgBuf);
        gColorImgBuf = NULL;
     }
    
    return NULL;
    
}
static void test_file_saving(APCImageType::Value arg)
{
    pthread_t color_saving_tid = -1;
    sched_param color_saving_thread_param;
    pthread_attr_t color_saving_thread_attr;
    pthread_attr_init(&color_saving_thread_attr);
    pthread_attr_setschedpolicy(&color_saving_thread_attr, SCHED_FIFO);
    pthread_attr_getschedparam (&color_saving_thread_attr, &color_saving_thread_param);
    color_saving_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&color_saving_thread_attr, &color_saving_thread_param);
    pthread_create(&color_saving_tid, &color_saving_thread_attr, pthread_saving_color, (void*) &arg);

    pthread_t depth_saving_tid = -1;
    sched_param depth_saving_thread_param;
    pthread_attr_t depth_saving_thread_attr;
    pthread_attr_init(&depth_saving_thread_attr);
    pthread_attr_setschedpolicy(&depth_saving_thread_attr, SCHED_FIFO);
    pthread_attr_getschedparam (&depth_saving_thread_attr, &depth_saving_thread_param);
    depth_saving_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&depth_saving_thread_attr, &depth_saving_thread_param);
    pthread_create(&depth_saving_tid, &depth_saving_thread_attr, pthread_saving_depth, NULL);
    
    pthread_join(color_saving_tid, NULL);
    pthread_join(depth_saving_tid, NULL);

    return;
}

static void init_device(void)
{
    int ret, i;
    char FWVersion[128];
    char devBuf[128];
    char devBuf_v4l[128];
    char devBuf_name[128];

    ret = APC_Init(&EYSD, true);
    if (ret == APC_OK) {
        printf("APC_Init() success! (EYSD : %p)\n", EYSD);
    } else {
        printf("APC_Init() fail.. (ret : %d, EYSD : %p)\n", ret, EYSD);
        print_APC_error(ret);
    }

    int nDevCount = APC_GetDeviceNumber(EYSD);
    printf("nDevCount = %d\n", nDevCount);
    g_pDevInfo = (DEVINFORMATION*)malloc(sizeof(DEVINFORMATION)*nDevCount);
    
    for( i = 0 ; i < nDevCount ; i++) {
        printf("select index = %d\n", i);
        g_DevSelInfo.index = i;
        APC_GetDeviceInfo(EYSD, &g_DevSelInfo ,g_pDevInfo+i);
        printf("Device Name = %s\n", g_pDevInfo[i].strDevName);
        printf("PID = 0x%04x\n", g_pDevInfo[i].wPID);
        printf("VID = 0x%04x\n", g_pDevInfo[i].wVID);
        printf("Chip ID = 0x%x\n", g_pDevInfo[i].nChipID);
        printf("device type = %d\n", g_pDevInfo[i].nDevType);

        int nActualLength = 0;
        if( APC_OK == APC_GetFwVersion(EYSD, &g_DevSelInfo, FWVersion, 256, &nActualLength)) {
            printf("FW Version = %s\n", FWVersion);
            strcpy(devBuf, &g_pDevInfo[i].strDevName[strlen("/dev/")]);
            sprintf(devBuf_v4l, "/sys/class/video4linux/%s/name", devBuf);
            get_product_name(devBuf_v4l, devBuf_name);
        }
    }
    
    if (nDevCount == 1) {
        gCameraPID = g_pDevInfo[0].wPID;
    }
    
    //s:[eys3D] 20200615 implement ZD table
    if(g_ColorPaletteZ14 == NULL){
            g_ColorPaletteZ14 = (RGBQUAD *)calloc(16384, sizeof(RGBQUAD));
    }
    if(g_ColorPaletteZ14 == NULL) {
            printf("alloc g_ColorPaletteZ14 fail..\n");
    }
    
    if(g_GrayPaletteZ14 == NULL){
            g_GrayPaletteZ14 = (RGBQUAD *)calloc(16384, sizeof(RGBQUAD));
    }
    if(g_GrayPaletteZ14 == NULL) {
        printf("alloc g_GrayPaletteZ14 fail..\n");
    }
    //e:[eys3D] 20200615 implement ZD table
}


static void open_device_default(bool two_open, int fps, WORD videoMode)
{
    int dtc = 0, ret;
    char input[64];
    int m_output_dtc = 0;
    bool bIsMJPEG = false; //true: V4L2_PIX_FMT_MJPEG, false: V4L2_PIX_FMT_YUYV
    
    if (!EYSD) {
        init_device();
    }
    
    if (g_DevSelInfo.index > 0) {
        printf("Please select device index: \n");
        for (int i = 0 ; i < g_DevSelInfo.index + 1 ; i++) {
            char* module = PidToModuleName(g_pDevInfo[i].wPID);
            if (module == nullptr) {
                module = g_pDevInfo[i].strDevName;
            }
            printf("%d: %s\n", i, module);
        }
        scanf("%d", &gsDevSelInfo.index);
    }
    //s:[eys3D] 20200610 implement hypatia config
    int m_VideoMode = 1;

    {

        if (APC_SetupBlock(EYSD, &gsDevSelInfo, false) != 0) {
                printf("setup Blocking Failed\n");
        }
        gColorFormat = bIsMJPEG;
        gColorWidth = 1280;
        gColorHeight = 720;
        gActualFps = fps;
        dtc = DEPTH_IMG_NON_TRANSFER; // DEPTH_IMG_NON_TRANSFER: non transfer, DEPTH_IMG_GRAY_TRANSFER: gray, DEPTH_IMG_COLORFUL_TRANSFER: colorful
        gDepthWidth = 1280;
        gDepthHeight = 720;
        gActualFps = fps; 
        gDepth_Transfer_ctrl = (DEPTH_TRANSFER_CTRL)dtc;
        
        if (gDepthWidth != 0) {
	        gDepthDataType = videoMode;
            TransformDepthDataType(&gDepthDataType, true);
            printf("APC_SetDepthDataType(%d)\n", gDepthDataType);
            ret = APC_SetDepthDataType(EYSD, &gsDevSelInfo, gDepthDataType); //4 ==> 11 bits
            if (ret == APC_OK) {
                printf("APC_SetDepthData() success!\n");
            } else {
                printf("APC_SetDepthData() fail.. (ret=%d)\n", ret);
                print_APC_error(ret);
            }
        }

        if (g_pDevInfo[gsDevSelInfo.index].wPID == 0x0124 || g_pDevInfo[gsDevSelInfo.index].wPID == 0x0147) {
            setupFWRegister_EX8038();
        }
    }
    
    //s:[eys3D] 20200615 implement ZD table
    ret =  getZDtable(g_pDevInfo, gsDevSelInfo, gDepthHeight, gColorHeight);
    if (APC_OK != ret) {
        printf("update ZDtable failed (ret=%d)\n", ret);
    }
    ColorPaletteGenerator::DmColorMode14(g_ColorPaletteZ14, (int) g_maxFar,(int)g_maxNear);
    ColorPaletteGenerator::DmGrayMode14(g_GrayPaletteZ14,  (int) g_maxFar,(int)g_maxNear);


    if (APC_OK != APC_Setup_v4l2_requestbuffers(EYSD, &gsDevSelInfo, g_v4l2_buffer_quque_size)) {
        printf("APC_Setup_v4l2_requestbuffers failed\n");
    }

    
    if (two_open) {
        ret = APC_OpenDevice2(EYSD,
                                  &gsDevSelInfo, gColorWidth,
                                  gColorHeight, (bool)gColorFormat,
                                  gDepthWidth, gDepthHeight,
                                  gDepth_Transfer_ctrl, false, NULL, &gActualFps, IMAGE_SN_SYNC);
        if (ret == APC_OK) {
            printf("APC_OpenDevice2() success! (FPS=%d)\n", gActualFps);
        } else {
            printf("APC_OpenDevice2() fail.. (ret=%d)\n", ret);
            print_APC_error(ret);
        }
    } else {
        ret= APC_OpenDevice(EYSD, &gsDevSelInfo,
                          gColorWidth, gColorHeight, (bool)gColorFormat,
                          gDepthWidth, gDepthHeight,
                          gDepth_Transfer_ctrl,
                          false, NULL,
                          &gActualFps, IMAGE_SN_SYNC/*IMAGE_SN_NONSYNC*/);
        if (ret == APC_OK) {
            printf("APC_OpenDevice() success! (FPS=%d)\n", gActualFps);
        } else {
            printf("APC_OpenDevice() fail.. (ret=%d)\n", ret);
            print_APC_error(ret);
        }
    }
    

    //s:[eys3D] 20200623, implement IR mode
    ret = setupIR(0xff);//input 0xff, will use default value (min+max)/2. 
    if (APC_OK != ret) {
        printf("set IR mode fail.. (ret=%d)\n", ret);
    }
    //e:[eys3D] 20200623, implement IR mode
}


static void open_device(void)
{
    if (!EYSD) {
	init_device();
    }
    int dtc = 0, ret;
    char input[64];
    int m_output_dtc = 0;

    if (g_DevSelInfo.index > 0) {
        printf("Please select device index: \n");
        for (int i = 0 ; i < g_DevSelInfo.index + 1 ; i++) {
            char* module = PidToModuleName(g_pDevInfo[i].wPID);
            if (module == nullptr) {
                module = g_pDevInfo[i].strDevName;
            }
            printf("%d: %s\n", i, module);
        }
        scanf("%d", &gsDevSelInfo.index);
    }
    //s:[eys3D] 20200610 implement hypatia config
    int m_VideoMode = 1;

    if (g_pDevInfo[gsDevSelInfo.index].wPID == 0x160) {
        printf("Please enter Mode: \n");
        printf("Mode 1 : L'+D (Color:MJPEG 640x400   30fps, Depth:640x400 30fps) \n");
        printf("Mode 2 : L'+D (Color:MJPEG 320x200   30fps, Depth:320x200 30fps) \n");
        printf("Mode 3 : L'+D (Color:MJPEG 320x104   30fps, Depth:320x104 30fps) \n");
        printf("Mode 4 : L'+D (Color:MJPEG 640x400x2 15fps, Depth:640x400 15fps) \n");
        printf("Mode 5 : L'+D (Color:MJPEG 320x200x2 15fps, Depth:320x200 15fps) \n");
        printf("Mode 6 : L'+D (Color:MJPEG 320x104x2 15fps, Depth:320x104 15fps) \n");
        printf("Mode 7 : L'+D (Color:YUV 640x400x2   15fps ) \n");
        printf("Mode 8 : L'+D (Color:YUV 320x200x2   15fps ) \n");
        printf("Mode 9 : L'+D (Color:YUV 320x104x2   15fps ) \n");
        scanf("%d", &m_VideoMode);

        setHypatiaVideoMode(m_VideoMode);
    }
    //e:[eys3D] 20200610 implement hypatia config
    else {
        printf("Blocking Mode Turn On? (Yes/No)\n");
        scanf("%s", input);

        if (input[0] == 'Y' || input[0] == 'y') {
            if (APC_SetupBlock(EYSD, &gsDevSelInfo, true) != 0) {
                printf("setup Blocking Failed\n");
            }
        } else {
            if (APC_SetupBlock(EYSD, &gsDevSelInfo, false) != 0) {
                printf("setup Blocking Failed\n");
            }
        }

        printf("Set Color stream format & resolution (format = 0: YUYV, 1: MJPEG)\n");
        printf("ex> 0 1280 720 30 (YUYV  foramt, 1280 x 720, 30(FPS))\n");
        printf("ex> 1 1280 720 30 (MJPEG foramt, 1280 x 720, 30(FPS))\n");

        scanf("%d %d %d %d", &gColorFormat, &gColorWidth, &gColorHeight, &gActualFps);

        dtc = 0; // 0: non transfer, 1: gray, 2: colorful

        printf("Set Depth stream resolution\n");
        printf("ex> 640 720 30 (640 x 720, 30(FPS))\n");
        printf("ex> 320 480 30 (320 x 480, 30(FPS))\n");

        scanf("%d %d %d", &gDepthWidth, &gDepthHeight, &gActualFps);

        gDepth_Transfer_ctrl = (DEPTH_TRANSFER_CTRL)dtc;
        if (gDepthWidth != 0) {
	        setupDepth();
            TransformDepthDataType(&gDepthDataType, true);
            printf("APC_SetDepthDataType(%d)\n", gDepthDataType);
            ret = APC_SetDepthDataType(EYSD, &gsDevSelInfo, gDepthDataType); //4 ==> 11 bits
            if (ret == APC_OK) {
                printf("APC_SetDepthData() success!\n");
            } else {
                printf("APC_SetDepthData() fail.. (ret=%d)\n", ret);
                print_APC_error(ret);
            }
        }

        m_output_dtc = 2; // 0: non transfer, 1: gray, 2: colorful
        g_depth_output= (DEPTH_TRANSFER_CTRL)m_output_dtc;

        if (g_pDevInfo[gsDevSelInfo.index].wPID == 0x0124 || g_pDevInfo[gsDevSelInfo.index].wPID == 0x0147) {
	    setupFWRegister_EX8038();
        }
    }
    //s:[eys3D] 20200615 implement ZD table
    ret =  getZDtable(g_pDevInfo, gsDevSelInfo, gDepthHeight, gColorHeight);
    if (APC_OK != ret) {
        printf("update ZDtable failed (ret=%d)\n", ret);
    }
    ColorPaletteGenerator::DmColorMode14(g_ColorPaletteZ14, (int) g_maxFar,(int)g_maxNear);
    ColorPaletteGenerator::DmGrayMode14(g_GrayPaletteZ14,  (int) g_maxFar,(int)g_maxNear);
    //e:[eys3D] 20200615 implement ZD table

    setV4L2buffer();
    
    /* APC_OpenDevice2 (Color + Depth) */
    ret = APC_OpenDevice2(EYSD, &gsDevSelInfo, gColorWidth, gColorHeight, (bool)gColorFormat, gDepthWidth, gDepthHeight, gDepth_Transfer_ctrl, false, NULL, &gActualFps, IMAGE_SN_SYNC);
    if (ret == APC_OK) {
        printf("APC_OpenDevice2() success! (FPS=%d)\n", gActualFps);
    } else {
        printf("APC_OpenDevice2() fail.. (ret=%d)\n", ret);
        print_APC_error(ret);
    }

    //s:[eys3D] 20200623, implement IR mode
    ret = setupIR(0xff);//input 0xff, will use default value (min+max)/2. 
    if (APC_OK != ret) {
        printf("set IR mode fail.. (ret=%d)\n", ret);
    }
    //e:[eys3D] 20200623, implement IR mode
}

static void *pfunc_thread_color(void *arg) {
    int ret,mCount = 0;
    long long start_time = calcByGetTimeOfDay();
    long long current_time = 0;
    long cnt_frme = 0;
    float fps_tmp = 0;
    
    UNUSED(arg);
    printf("\ngColorWidth = %d,  gColorHeight = %d\n", gColorWidth, gColorHeight);
    if(gColorImgBuf == NULL) {
        gColorImgBuf = (unsigned char*)calloc(2 * gColorWidth * gColorHeight , sizeof(unsigned char));
    }
    if(gColorImgBuf == NULL) {
        printf("alloc ColorImgBuf fail..\n");
        return NULL;
    }
    if(DEBUG_LOG) {printf("gColorImgBuf : %p\n",gColorImgBuf);}

    while(bTesting_color == true && mCount < TEST_RUN_NUMBER) {
        bTestEnd_color = false;
        usleep(1000 * 5);

        /* APC_GetColorImage */
       // printf("Enter calling APC_GetColorImage()...\n");
        ret = APC_GetColorImage(EYSD, &gsDevSelInfo, (BYTE*)gColorImgBuf, &gColorImgSize, &gColorSerial,0);
       // printf("Leave calling APC_GetColorImage()...\n");
        //printf("[%s][%d] ret = %d, errno = %d(%s)(%d)\n", __func__, __LINE__,ret, errno, strerror(errno), gColorSerial);
        if (ret == APC_OK && gColorSerial > 0) {
            
            cnt_frme++;

            if (cnt_frme > 99) {
                current_time = calcByGetTimeOfDay();
                fps_tmp = (float)(100 * 1000 * 1000) / (float)((current_time - start_time));
                cnt_frme = 0;
                printf("\nColor: FPS = %f (size : %lu, serial : %d)\n", fps_tmp, gColorImgSize, gColorSerial);
                start_time = current_time;
                mCount ++;

                if (snapShot_color == true ) {
                    printf("Doing Snapshot...\n");

                    //pthread_mutex_lock(&save_file_mutex);
                    save_file(gColorImgBuf, gColorImgSize,gColorWidth,  gColorHeight, gColorFormat, true);

                    //s:[eys3D] 20200610 implement to save raw data to RGB format
                    if (gColorFormat == 0){ //YUV to RGB
                        if(gColorRGBImgBuf == NULL) {
                                gColorRGBImgBuf = (unsigned char*)calloc(3 * gColorWidth * gColorHeight, sizeof(unsigned char));
                        }
                        if(gColorRGBImgBuf == NULL) {
                                printf("alloc gColorRGBImgBuf fail..\n");
                                return NULL;
                        }
                        APC_ColorFormat_to_RGB24(EYSD, &gsDevSelInfo, gColorRGBImgBuf, gColorImgBuf, gColorImgSize,
                                                            gColorWidth, gColorHeight, APCImageType::Value::COLOR_YUY2);
                    } else {//MJPEG to RGB
                        int size=0, type;
                        ret = tjpeg2yuv(gColorImgBuf,2 * gColorWidth * gColorHeight,&gTempImgBuf, &size,&type);
                        ret = tyuv2rgb(gTempImgBuf,size, gColorWidth, gColorHeight, type,&gColorRGBImgBuf,&size);

                        if(DEBUG_LOG) {
                            if(gTempImgBuf != NULL){
                                printf("gTempImgBuf : %p\n",gTempImgBuf);
                            }

                            if(gColorRGBImgBuf != NULL){
                                printf("gColorRGBImgBuf : %p\n",gColorRGBImgBuf);
                            }
                         }
                    }

                    if(ret == APC_OK) {
                        save_file(gColorRGBImgBuf, 0,gColorWidth,  gColorHeight, gColorFormat, false);
                     }

                    if(gTempImgBuf != NULL){
                            if(DEBUG_LOG) {
                                printf("free gTempImgBuf : %p\n",gTempImgBuf);
                            }
                            free(gTempImgBuf);
                            gTempImgBuf = NULL;
                    }
                    
                    if(gColorRGBImgBuf != NULL){
                            if(DEBUG_LOG) {
                                printf("free gColorRGBImgBuf : %p\n",gColorRGBImgBuf);
                            }
                            free(gColorRGBImgBuf);
                            gColorRGBImgBuf = NULL;
                    }
                    //e:[eys3D] 20200610 implement to save raw data to RGB format
                    //pthread_mutex_unlock(&save_file_mutex);
                }
            }
        } else {
           // printf("Failed to APC_GetColorImage().[ret = %d]\n", ret);
            //print_APC_error(ret);
            //break;
        }
    }
    
    bTestEnd_color = true;
    //s:[eys3D] 20200610 implement to save raw data to RGB format
    if(gColorImgBuf != NULL){
            if(DEBUG_LOG) {
                printf("free gColorImgBuf : %p\n",gColorImgBuf);
            }
            free(gColorImgBuf);
            gColorImgBuf = NULL;
     }
    //e:[eys3D] 20200610 implement to save raw data to RGB format
    //pthread_exit(NULL);
    return NULL;
}

static void get_color_image(void)
{
#ifdef USE_PTHREAD_TO_GET_COLOR_AND_DEPTH_IMAGE
    pthread_t color_thread_id;
    pthread_attr_t color_thread_attr;
    struct sched_param color_thread_param;

    pthread_attr_init(&color_thread_attr);
    pthread_attr_getschedparam (&color_thread_attr, &color_thread_param);
    color_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&color_thread_attr, &color_thread_param);
    pthread_create(&color_thread_id, &color_thread_attr, pfunc_thread_color, NULL);
    gcolor_thread_id = color_thread_id;
    //pthread_join(color_thread_id, NULL);
#else
    pfunc_thread_color(NULL);
#endif
}

static void *pfunc_thread_depth(void *arg) {
    int ret;
    long long start_time = calcByGetTimeOfDay();
    long long current_time = 0;
    long cnt_frme = 0;
    float fps_tmp = 0;
    int mCount = 0;
    unsigned long int m_BufferSize = 0;
    
    UNUSED(arg);

    printf("\ngDepthWidth = %d,  gDepthHeight = %d\n", gDepthWidth, gDepthHeight);

    if(gDepthImgBuf == NULL) {
         if(gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                m_BufferSize = 2 * gDepthWidth  * 2*gDepthHeight;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        } else {
                m_BufferSize = gDepthWidth * gDepthHeight * 2;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        }
    }
    
    if(gDepthImgBuf == NULL) {
        printf("alloc for gDepthImageBuf fail..\n");
        return NULL;
    }
    if(DEBUG_LOG) {
        printf("gDepthImgBuf : %p\n",gDepthImgBuf);
    }

    //s:[eys3D] 20200610 implement to save raw data to RGB format
    if(gDepthRGBImgBuf == NULL) {
        if(gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                gDepthRGBImgBuf = (unsigned char*)calloc(2 * gDepthWidth  * gDepthHeight * 3, sizeof(unsigned char)); 
        } else {
                gDepthRGBImgBuf = (unsigned char*)calloc(gDepthWidth * gDepthHeight * 3, sizeof(unsigned char));
        }
    }

    if(gDepthRGBImgBuf == NULL) {
        printf("alloc for gDepthRGBImgBuf fail..\n");
        return NULL;
    }
    if(DEBUG_LOG) {
        printf("gDepthRGBImgBuf : %p\n",gDepthRGBImgBuf);
     }
    //e:[eys3D] 20200610 implement to save raw data to RGB format
                     
    bool bFirstReceived = true; 
    while(bTesting_depth == true && mCount < TEST_RUN_NUMBER) {
        bTestEnd_depth = false;
        usleep(1000 * 5);
        //printf("Enter calling APC_GetDepthImage()...\n");
        ret = APC_GetDepthImage(EYSD, &gsDevSelInfo, (BYTE*)gDepthImgBuf, &gDepthImgSize, &gDepthSerial, gDepthDataType);
        //printf("Leave calling APC_GetDepthImage()...\n");
         if(gDepthImgSize > m_BufferSize)
          {
                printf("Alloc size : %lu, but get depth size : %lu, check FW and close the application.\n",m_BufferSize, gDepthImgSize);
                goto EXIT;
          }
       // printf("[%s][%d] ret = %d, errno = %d(%s)(%d)\n", __func__, __LINE__,ret, errno, strerror(errno), gDepthSerial);
        if(ret == APC_OK) {
            
            cnt_frme++;

            if (bFirstReceived){
                bFirstReceived = false;
                RegisterSettings::DM_Quality_Register_Setting(EYSD,
                                                              &gsDevSelInfo,
                                                              g_pDevInfo[gsDevSelInfo.index].wPID);
            }

            if (cnt_frme > 99 && gDepthSerial > 0) {
                current_time = calcByGetTimeOfDay();
                fps_tmp = (float)(100 * 1000 * 1000) / (float)((current_time - start_time));
                cnt_frme = 0;
                start_time = current_time;
                printf("\nDepth: FPS = %f, (size : %lu, serial : %d, datatype : %d)\n", fps_tmp, gDepthImgSize, gDepthSerial, gDepthDataType);
                mCount ++;
                if (snapShot_depth == true) {
                    printf("Doing Snapshot...\n");
                    //pthread_mutex_lock(&save_file_mutex);

                    save_file(gDepthImgBuf, gDepthImgSize, gDepthWidth, gDepthHeight,2, true);

                    //s:[eys3D] 20200615 implement to save raw data to RGB format
                    saveDepth2rgb(gDepthImgBuf, gDepthRGBImgBuf, gDepthWidth, gDepthHeight);
                    //e:[eys3D] 20200615 implement to save raw data to RGB format
                    //pthread_mutex_unlock(&save_file_mutex);
                }
            }
        } else {
           // printf("Failed to APC_GetDepthImage().[ret = %d]\n", ret);
           // print_APC_error(ret);
           // break;
        }

    }
    bTestEnd_depth = true;
    
    //s:[eys3D] 20200610 implement to save raw data to RGB format
    if(gDepthImgBuf != NULL){
            if(DEBUG_LOG) {
                printf("free gDepthImgBuf : %p\n",gDepthImgBuf);
            }
            free(gDepthImgBuf);
            gDepthImgBuf = NULL;
     }
    
    if(gDepthRGBImgBuf != NULL){
            if(DEBUG_LOG) {
                printf("free gDepthRGBImgBuf : %p\n",gDepthRGBImgBuf);
            }
            free(gDepthRGBImgBuf);
             gDepthRGBImgBuf = NULL;
    }
    //e:[eys3D] 20200610 implement to save raw data to RGB format
   // pthread_exit(NULL);
    EXIT:
   // exit (1);
    return NULL;
}

static void get_depth_image(void)
{
#ifdef USE_PTHREAD_TO_GET_COLOR_AND_DEPTH_IMAGE
    pthread_t depth_thread_id;
    pthread_attr_t depth_thread_attr;
    struct sched_param depth_thread_param;

    pthread_attr_init(&depth_thread_attr);
    pthread_attr_getschedparam (&depth_thread_attr, &depth_thread_param);
    depth_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&depth_thread_attr, &depth_thread_param);
    pthread_create(&depth_thread_id, &depth_thread_attr, pfunc_thread_depth, NULL);
    //pthread_join(depth_thread_id, NULL);
    gdepth_thread_id = depth_thread_id;
#else
    pfunc_thread_depth(NULL);
#endif
}

static void close_device(void)
{
/*
    pthread_t close_thread_id;
    pthread_attr_t close_thread_attr;
    struct sched_param close_thread_param;

    pthread_attr_init(&close_thread_attr);
    pthread_attr_getschedparam (&close_thread_attr, &close_thread_param);
    close_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&close_thread_attr, &close_thread_param);
    pthread_create(&close_thread_id, &close_thread_attr, pfunc_thread_close, NULL);
    pthread_join(close_thread_id, NULL);
*/
	pfunc_thread_close(NULL);
}

static void release_device(void)
{
    APC_Release(&EYSD);
}

static void *pfunc_thread_point_cloud(void *arg) {

    int ret = APC_OK;
    
    int64_t cur_tv_sec_depth = 0;
    int64_t cur_tv_usec_depth = 0;
    int64_t cur_tv_sec_color = 0;
    int64_t cur_tv_usec_color = 0;
    
    int cur_serial_num_depth = -1;
    int cur_serial_num_color = -1;
    
    unsigned int count = 0;
    unsigned int max_count = 10;
    
    static unsigned int m_BufferSize = 0;
    
    bool bFirstReceived = true;

    int i = 0;
    unsigned char *pPointCloudRGB = NULL;
    float *pPointCloudXYZ = NULL;

    (void)arg;

    printf("depth image: [%d x %d @ %d]\n", gDepthWidth, gDepthHeight, gActualFps);
    
    pPointCloudRGB = (unsigned char *)malloc(gDepthWidth * gDepthHeight * 3 * sizeof(unsigned char));
    pPointCloudXYZ = (float *)malloc(gDepthWidth * gDepthHeight * 3 * sizeof(float));
    if((pPointCloudRGB == NULL) || (pPointCloudXYZ == NULL)) {
        printf("alloc for pPointCloudRGB or  pPointCloudXYZ fail..\n");
        goto exit;
    }
    
    if(gDepthImgBuf == NULL) {
         if(gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                m_BufferSize = 2 * gDepthWidth  * 2*gDepthHeight;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        } else {
                m_BufferSize = gDepthWidth * gDepthHeight * 2;
                gDepthImgBuf = (unsigned char*)calloc(m_BufferSize, sizeof(unsigned char));
        }
    }
    if(gDepthImgBuf == NULL) {
        printf("alloc for gDepthImageBuf fail..\n");
         goto exit;
    }

    printf("color image: [%d x %d @ %d]\n", gColorWidth, gColorHeight, gActualFps);
    if(gColorImgBuf == NULL) {
        gColorImgBuf = (unsigned char*)calloc(2 * gColorWidth * gColorHeight , sizeof(unsigned char));
    }
    if(gColorImgBuf == NULL) {
        printf("alloc ColorImgBuf fail..\n");
         goto exit;
    }
    
    if(gColorRGBImgBuf == NULL) {
        gColorRGBImgBuf = (unsigned char*)calloc(3 * gColorWidth * gColorHeight, sizeof(unsigned char));
    }
    if(gColorRGBImgBuf == NULL) {
        printf("alloc gColorRGBImgBuf fail..\n");
         goto exit;
    }

    

    while (count < max_count) {
        
        ret = APC_GetColorImageWithTimestamp(EYSD, &gsDevSelInfo,
                                             (BYTE*)gColorImgBuf, &gColorImgSize, &cur_serial_num_color,
                                             0, &cur_tv_sec_color, &cur_tv_usec_color);
        
        ret = APC_GetDepthImageWithTimestamp(EYSD, &gsDevSelInfo,
                                             (BYTE*)gDepthImgBuf, &gDepthImgSize, &cur_serial_num_depth,
                                             gDepthDataType, &cur_tv_sec_depth, &cur_tv_usec_depth);
        
        if(gDepthImgSize > m_BufferSize) {
            printf("Alloc size : %lu, but get depth size : %lu, check FW and close the application.\n",m_BufferSize, gDepthImgSize);
            break;
        }
        
        if (ret == APC_OK) {
            if (bFirstReceived){
                bFirstReceived = false;
                RegisterSettings::DM_Quality_Register_Setting(EYSD, &gsDevSelInfo, g_pDevInfo[gsDevSelInfo.index].wPID);
            }
            
            convert_yuv_to_rgb_buffer(gColorImgBuf, gColorRGBImgBuf, gColorWidth, gColorHeight);
            ret = getPointCloud(EYSD, &gsDevSelInfo, gColorRGBImgBuf, gColorWidth, gColorHeight,
                                gDepthImgBuf, gDepthWidth, gDepthHeight, gDepthDataType,
                                pPointCloudRGB, pPointCloudXYZ, g_maxNear, g_maxFar);
        }
        count++;
    }
    
exit:
    if(gDepthImgBuf != NULL){
        if(DEBUG_LOG) {
            printf("free gDepthImgBuf : %p\n",gDepthImgBuf);
        }
        free(gDepthImgBuf);
        gDepthImgBuf = NULL;
     }

     
    if (pPointCloudRGB != NULL) {
        if(DEBUG_LOG) {
            printf("free pPointCloudRGB : %p\n",pPointCloudRGB);
        }
        free(pPointCloudRGB);
        pPointCloudRGB = NULL;
    }
    
    if (pPointCloudXYZ != NULL) {
        if(DEBUG_LOG) {
            printf("free pPointCloudXYZ : %p\n",pPointCloudXYZ);
        }
        free(pPointCloudXYZ);
        pPointCloudXYZ = NULL;
    }
    
    if (gColorRGBImgBuf != NULL) {
        if(DEBUG_LOG) {
            printf("free pPointCloudXYZ : %p\n",gColorRGBImgBuf);
        }  
        free(gColorRGBImgBuf);
        gColorRGBImgBuf = NULL;
    }
     
    return NULL;
    
}

static void get_point_cloud(void)
{
    pthread_t point_cloud_thread_id;
    pthread_attr_t point_cloud_thread_attr;
    struct sched_param point_cloud_thread_param;

    pthread_attr_init(&point_cloud_thread_attr);
    pthread_attr_getschedparam (&point_cloud_thread_attr, &point_cloud_thread_param);
    point_cloud_thread_param.sched_priority = sched_get_priority_max(SCHED_FIFO) -1;
    pthread_attr_setschedparam(&point_cloud_thread_attr, &point_cloud_thread_param);
    pthread_create(&point_cloud_thread_id, &point_cloud_thread_attr, pfunc_thread_point_cloud, NULL);
    pthread_join(point_cloud_thread_id, NULL);
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


int getPointCloud(void *pHandleEYSD, DEVSELINFO *pDevSelInfo, unsigned char *ImgColor, int CW, int CH,
                  unsigned char *ImgDepth, int DW, int DH, int depthDataType,
                  unsigned char *pPointCloudRGB,
                  float *pPointCloudXYZ,
                  float fNear,
                  float fFar)
{
    int ret = 0;
    PointCloudInfo pointCloudInfo;
    std::vector<CloudPoint> cloud;
    char DateTime[32] = {0};
    static unsigned int yuv_index = 0;
    char fname[256] = {0};
    int i = 0;
    CloudPoint cloudpoint = {0};
    
    memset(DateTime, 0, sizeof(DateTime));
    GetDateTime(DateTime);
    
    ret = getPointCloudInfo(pHandleEYSD, pDevSelInfo, &pointCloudInfo, depthDataType, DH);
    if (ret == APC_OK) {
        ret = APC_GetPointCloud(pHandleEYSD, pDevSelInfo, ImgColor, CW, CH, ImgDepth, DW, DH,
                                &pointCloudInfo, pPointCloudRGB, pPointCloudXYZ, fNear, fFar);
        if (ret == APC_OK) {
            snprintf(fname, sizeof(fname), SAVE_FILE_PATH"cloud_%d_%s.ply", yuv_index++, DateTime);
           
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


//s:[eys3D] 20200610 implement to save raw data to RGB format
static void SetSnapShotFlag()
{
        char input[64];
       printf("enable color image snapshot? (Yes or No)\n");
       scanf("%s", input);
    
       if (input[0] == 'Y' || input[0] == 'y') {
           if (gColorWidth > 0)
                   snapShot_color = true;
       }else{
            snapShot_color = false;
        }
       
        printf("enable depth image snapshot? (Yes or No)\n");
       scanf("%s", input);     
       if (input[0] == 'Y' || input[0] == 'y') {
           if (gDepthWidth > 0)
                   snapShot_depth = true;
       }else{
            snapShot_depth = false;
        }
}
//e:[eys3D] 20200610 implement to save raw data to RGB format

static void setupFWRegister_EX8038()
{
    int flag = 0;

    flag |= FG_Address_1Byte;
    flag |= FG_Value_1Byte;

    if (APC_OK != APC_SetFWRegister(EYSD, &gsDevSelInfo, 0xf0, 0x0d, flag)) {
        printf("APC_SetFWRegister failed\n");
    }
}

static void setupFWRegister()
{
    int flag = 0;
    static unsigned char addr;
    unsigned char value;
    
    flag |= FG_Address_1Byte;
    flag |= FG_Value_1Byte;

    printf("Please input one byte FW address: 0x");
    scanf("%02x", (int *)&addr);

    printf("Please input one byte FW setup value: 0x");
    scanf("%02x", (int *)&value);

    printf("Write address = 0x%02x, value = 0x%02x\n", addr, value);
    if (APC_OK != APC_SetFWRegister(EYSD, &gsDevSelInfo, addr, value, flag)) {
        printf("APC_SetFWRegister failed\n");
    }
}

static void readFWRegister(void)
{
    int flag = 0;
    static unsigned char  addr;
    unsigned char value;
    
    flag |= FG_Address_1Byte;
    flag |= FG_Value_1Byte;

    printf("Please input one byte FW address: 0x");
    scanf("%02x", (int *)&addr);

    if (APC_OK != APC_GetFWRegister(EYSD, &gsDevSelInfo, addr, (unsigned short *)&value, flag)) {
        printf("APC_GetFWRegister failed\n");
    }else {
        printf("FW addr 0x%02x = 0x%02x\n", addr, value);
    }
}

static void SetCounterMode()
{
    int nValue;

    printf("Please input value: 0: for Frame Counter, 1: For Serial Counter===> ");
    scanf("%02x", (int *)&nValue);

    if (APC_OK != APC_SetControlCounterMode(EYSD, &gsDevSelInfo, (unsigned char)nValue)) {
        printf("APC_SetControlCounterMode Failed\n");
        return;
    }
    printf("Setup Success\n");
}

static void GetCounterMode()
{
    unsigned char nValue;

    if (APC_OK != APC_GetControlCounterMode(EYSD, &gsDevSelInfo, &nValue)) {
        printf("APC_GetControlCounterMode Failed\n");
        return;
    } else {
        printf("ControlCounterMode = %d\n", nValue);
        if (nValue == 1)
            printf("ControlCounterMode: Serial Mode\n");
        else
            printf("ControlCounterMode: Frame Mode\n");
    }
}

static void setV4L2buffer()
{
    int value;

    printf("Please input V4L2 buffer size: (5~32)");
    scanf("%d", (int *)&value);

    g_v4l2_buffer_quque_size = value;
    printf("Set V4L2 buffer size = %d\n", g_v4l2_buffer_quque_size);
    if (APC_OK != APC_Setup_v4l2_requestbuffers(EYSD, &gsDevSelInfo, g_v4l2_buffer_quque_size)) {
        printf("APC_Setup_v4l2_requestbuffers failed\n");
    }
}

//s:[eys3D] 20200623, for IR mode
static void setIRValue()
{
    unsigned short value;
    int ret;
    unsigned short m_nIRMax, m_nIRMin;
    
    ret = APC_GetFWRegister(EYSD, &gsDevSelInfo,
                                0xE2, &m_nIRMax,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret || m_nIRMax == 0xff) {
        printf("get IR Max value failed\n");
        return;
     }

    ret = APC_GetFWRegister(EYSD, &gsDevSelInfo,
                                0xE1, &m_nIRMin,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret|| m_nIRMin == 0xff) {
        printf("get IR Min value failed\n");
        return;
     }
    printf("IR range: %d ~ %d\n",m_nIRMin,m_nIRMax);      
    printf("Please input IR value: ");
    scanf("%hu", &value);

    if (APC_OK != setupIR(value)) {
        printf("APC_SetFWRegister failed\n");
    }
}

int save_file(unsigned char *buf, int size, int width, int height,int type, bool saveRawData)
{
    int ret = 0;
    int fd = -1;
    char fname[256] = {0};

    static unsigned int yuv_index = 0;
    static unsigned int mjpeg_index = 0;
    static unsigned int depth_index = 0;
    static unsigned int yuv_rgb_index = 0;
    static unsigned int  mjpeg_rgb_index = 0;

    char DateTime[32] = {0};
     memset(DateTime, 0, sizeof(DateTime));
    
     ret = GetDateTime(DateTime);

    if(saveRawData) {
            switch(type) {
                case 0: // Color stream (YUYV)
                    snprintf(fname, sizeof(fname), SAVE_FILE_PATH"color_img_%d_%s.yuv", yuv_index++, DateTime);
                    break;
                case 1: // Color stream (MJPEG)
                    snprintf(fname, sizeof(fname), SAVE_FILE_PATH"color_img_%d_%s.jpg", mjpeg_index++, DateTime);
                    break;
                case 2: // Depth stream
                    snprintf(fname, sizeof(fname), SAVE_FILE_PATH"depth_img_%d_%s.yuv", depth_index++, DateTime);
                default:
                    break;
            }
            fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            if (fd < 0) {
                    printf("file open error (fd=%d)\n", fd);
                    ret = -1;
            } else if(write(fd, buf, size) != size) {
                    printf("write(fd, buf, size) != size\n");
                    ret = -1;
            }

            if (fd >= 0) {
                    close(fd);
                    sync();
            }
    } else {
            switch(type) {
                case 0: // YUV
                    snprintf(fname, sizeof(fname), SAVE_FILE_PATH"color_yuv2rgb_%d_%s.bmp", yuv_rgb_index++,DateTime);
                    break;
                case 1: // MJPEG
                    snprintf(fname, sizeof(fname), SAVE_FILE_PATH"color_mjpeg2rgb_%d_%s.bmp", mjpeg_rgb_index++,DateTime);
                    break;
                default:
                    break;
             }
            ret = tjSaveImage(fname,buf,width,0,height,TJPF_RGB,0);
     }

    printf("FILE_NAME = \"%s\" \n", fname);

    return ret;
}

//s:[eys3D] 20200610 implement to save raw data to RGB format
// YUYV to RGB +
int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
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

int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
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

// YUYV to RGB -
int tjpeg2yuv(unsigned char* jpeg_buffer, int jpeg_size, unsigned char** yuv_buffer, int* yuv_size, int* yuv_type)
{
    tjhandle handle = NULL;
    int width, height, subsample, colorspace;
    int flags = 0;
    int padding = 1;
    int ret = 0;

    handle = tjInitDecompress();
    tjDecompressHeader3(handle, jpeg_buffer, jpeg_size, &width, &height, &subsample, &colorspace);

    //printf("w: %d h: %d subsample: %d color: %d\n", width, height, subsample, colorspace);
    
    flags |= 0;
    
    *yuv_type = subsample;
    *yuv_size = tjBufSizeYUV2(width, padding, height, subsample);
    *yuv_buffer =(unsigned char *)malloc(*yuv_size);
    if (*yuv_buffer == NULL)
    {
        printf("malloc buffer for rgb failed.\n");
        return -1;
    }

    ret = tjDecompressToYUV2(handle, jpeg_buffer, jpeg_size, *yuv_buffer, width,
			padding, height, flags);
    if (ret < 0)
    {
        printf("compress to jpeg failed: %s\n", tjGetErrorStr());
    }
    tjDestroy(handle);

    return ret;
}

int tyuv2rgb(unsigned char* yuv_buffer, int yuv_size, int width, int height, int subsample, unsigned char** rgb_buffer, int* rgb_size)
{
    tjhandle handle = NULL;
    int flags = 0;
    int padding = 1;
    int pixelfmt = TJPF_RGB;
    int need_size = 0;
    int ret = 0;
 
    handle = tjInitDecompress();
   
    flags |= 0;
 
    need_size = tjBufSizeYUV2(width, padding, height, subsample);
    if (need_size != yuv_size)
    {
        printf("Conver to RGB failed! Expect yuv size: %d, but input size: %d, check again.\n", need_size, yuv_size);
        return -1;
    }
 
    *rgb_size = width*height*tjPixelSize[pixelfmt];
 
    *rgb_buffer =(unsigned char *)malloc(*rgb_size);
    if (*rgb_buffer == NULL)
    {
        printf("malloc buffer for rgb failed.\n");
        return -1;
    }
    ret = tjDecodeYUV(handle, yuv_buffer, padding, subsample, *rgb_buffer, width, 0, height, pixelfmt, flags);
    if (ret < 0)
    {
        printf("decode to rgb failed: %s\n", tjGetErrorStr());
    }

    tjDestroy(handle);
 
    return ret;
}

int saveDepth2rgb(unsigned char *m_pDepthImgBuf, unsigned char *m_pRGBBuf, unsigned int m_nImageWidth, unsigned int m_nImageHeight)
{
     char fname[256] = {0};
    static unsigned int yuv_index = 0;
    int ret = 0 ;
    int m_tmp_width= 0;
    char DateTime[32] = {0};
    
    memset(DateTime, 0, sizeof(DateTime));

     ret = GetDateTime(DateTime);
    
    if (gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
        m_tmp_width = m_nImageWidth * 2;
    } else {
        m_tmp_width = m_nImageWidth;
    }

     if(g_depth_output == DEPTH_IMG_COLORFUL_TRANSFER) {
        if (gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                UpdateD8bitsDisplayImage_DIB24(g_ColorPaletteZ14, &m_pDepthImgBuf[0], &m_pRGBBuf[0], m_tmp_width, m_nImageHeight);
        } else if (gDepthDataType == APC_DEPTH_DATA_11_BITS || gDepthDataType == APC_DEPTH_DATA_11_BITS_RAW) {
                UpdateD11DisplayImage_DIB24(g_ColorPaletteZ14, &m_pDepthImgBuf[0], &m_pRGBBuf[0], m_tmp_width, m_nImageHeight);
        } else if (gDepthDataType == APC_DEPTH_DATA_14_BITS || gDepthDataType == APC_DEPTH_DATA_14_BITS_RAW) {
                UpdateZ14DisplayImage_DIB24(g_ColorPaletteZ14, &m_pDepthImgBuf[0], &m_pRGBBuf[0], m_tmp_width, m_nImageHeight);
        }
    } else if(g_depth_output == DEPTH_IMG_GRAY_TRANSFER) {
        if (gDepthDataType == APC_DEPTH_DATA_8_BITS || gDepthDataType == APC_DEPTH_DATA_8_BITS_RAW) {
                UpdateD8bitsDisplayImage_DIB24(g_GrayPaletteZ14, &m_pDepthImgBuf[0], &m_pRGBBuf[0], m_nImageWidth, m_nImageHeight);
 
        } else if (gDepthDataType == APC_DEPTH_DATA_11_BITS || gDepthDataType == APC_DEPTH_DATA_11_BITS_RAW) {
                UpdateD11DisplayImage_DIB24(g_GrayPaletteZ14, &m_pDepthImgBuf[0], &m_pRGBBuf[0], m_nImageWidth, m_nImageHeight);

        } else if (gDepthDataType == APC_DEPTH_DATA_14_BITS || gDepthDataType == APC_DEPTH_DATA_14_BITS_RAW) {
                UpdateZ14DisplayImage_DIB24(g_GrayPaletteZ14, &m_pDepthImgBuf[0], &m_pRGBBuf[0], m_nImageWidth, m_nImageHeight);
        }

    } else {
       if (gDepthDataType == APC_DEPTH_DATA_8_BITS) {
                convert_yuv_to_rgb_buffer(m_pDepthImgBuf, m_pRGBBuf, m_nImageWidth, m_nImageHeight);
       } else {
                APC_ColorFormat_to_RGB24(EYSD, &gsDevSelInfo, m_pRGBBuf, m_pDepthImgBuf, gColorImgSize,
                                                            gColorWidth, gColorHeight, APCImageType::Value::COLOR_YUY2);
       }
    }

    snprintf(fname, sizeof(fname), SAVE_FILE_PATH"depth_yuv2rgb_%d_%s.bmp", yuv_index++, DateTime);
    ret = tjSaveImage(fname,m_pRGBBuf,m_tmp_width,0,m_nImageHeight,TJPF_RGB,0);
     
    printf("FILE_NAME = \"%s\" \n", fname);
    return ret;
}
//e:[eys3D] 20200610 implement to save raw data to RGB format

//s:[eys3D] 20200615 implement ZD table
int fillZDIndexByProductInfos(unsigned short  pid, int depthHeight,
		int colorHeight, bool isUSB3) {
		
        if (pid == 0x120) {//8036
		if (!isUSB3 && colorHeight && depthHeight && (colorHeight % depthHeight != 0) ) {
			// For mode 34 35 on PIF
			return 2;
		}
		if (depthHeight == 720){
			return 0;
		} else if (depthHeight >= 480) {
			return 1;
		}
		return 0;
	}
	if (pid == 0x121) {//8037
		if (depthHeight >= 720){
			return 0;
		} else if (depthHeight >= 480) {
			return 1;
		} else {
			return 0;
		}
	} 
	
	if (pid == 0x137) {//8052
		if (!isUSB3 && colorHeight && depthHeight && (colorHeight % depthHeight != 0) ) {
			return 2;
		}
		if (depthHeight == 720){
			return 0;
		} else if (depthHeight >= 480) {
			return 1;
		}
		return 0;
	}
        
    if (pid == 0x160) {//hypatia
		if (depthHeight ==400 ) {
			return 0;
		}
		else if (depthHeight == 200){
			return 1;
		} else if (depthHeight == 104) {
			return 2;
		}
		return 0;
	}

	if (pid == 0x167)
    {
        if (isUSB3) {
            if (colorHeight == 720)
                return 0;
            else 
                return 1;
        } else {
            if ((colorHeight == 720) && (depthHeight == 360)) {
                return 0;
            } else {
                return 1;
            }
        }
        
    }
	
    printf("not define the PID, since return 0\n");
    return 0;
}

int getZDtable(DEVINFORMATION *pDevInfo, DEVSELINFO DevSelInfo, int depthHeight,int colorHeight)
{
    ZDTABLEINFO zdTableInfo;
    int bufSize = 0;
    int nRet = -1;
    unsigned short nZValue;

    zdTableInfo.nDataType = APC_DEPTH_DATA_11_BITS;
    memset(g_pzdTable, 0, sizeof(g_pzdTable));
   
    if (pDevInfo[DevSelInfo.index].nDevType == PUMA) { // 8052, 8053 is used to smae ZD table
        bufSize = APC_ZD_TABLE_FILE_SIZE_11_BITS;
    } else {
        bufSize = APC_ZD_TABLE_FILE_SIZE_8_BITS;
    }

    zdTableInfo.nIndex = fillZDIndexByProductInfos(pDevInfo[DevSelInfo.index].wPID, depthHeight, colorHeight, false);
    printf("zdTableInfo nIndex : %d\n", zdTableInfo.nIndex);
    
    int actualLength = 0;
    if (zdTableInfo.nIndex < 0)
        zdTableInfo.nIndex = 0;

    nRet = APC_GetZDTable(EYSD, &DevSelInfo, g_pzdTable, bufSize, &actualLength, &zdTableInfo);
    if (nRet != APC_OK) {
        printf("Get ZD Table fail......%d\n", nRet);
        return nRet;
    }
    
    g_maxNear = 0xfff;
    g_maxFar = 0;

     printf("[%s][%d]Enter to calac mxaFar and maxNear...\n", __func__, __LINE__);
    for (int i = 0 ; i < APC_ZD_TABLE_FILE_SIZE_11_BITS ; ++i) {
        if ((i * 2) == APC_ZD_TABLE_FILE_SIZE_11_BITS)
            break;
        nZValue = (((unsigned short)g_pzdTable[i * 2]) << 8) +g_pzdTable[i * 2 + 1];
        if (nZValue) {
            g_maxNear = std::min<unsigned short>(g_maxNear, nZValue);
            g_maxFar = std::max<unsigned short>(g_maxFar, nZValue);
        }
    }
    printf("[%s][%d]Leave to calac mxaFar and maxNear...\n", __func__, __LINE__);
    
    if (g_maxNear > g_maxFar)
        g_maxNear = g_maxFar;
    
    if (g_maxFar > 1000)
        g_maxFar = 1000;
    
    g_zdTableInfo_index = zdTableInfo.nIndex;

    printf("Get ZD Table actualLength : %d, g_maxNear : %d, g_maxFar : %d\n", actualLength,g_maxNear ,g_maxFar);
    return nRet;
}

void UpdateD8bitsDisplayImage_DIB24(RGBQUAD *pColorPalette, unsigned char *pDepth, unsigned char *pRGB, int width, int height)
{
    int nBPS = width * 3;;
    BYTE* pDL    = pRGB;
    BYTE* pD     = NULL;
    const RGBQUAD* pClr = NULL;
    unsigned short z = 0;
    unsigned short zdIndex;

    if ((width <= 0) || (height <= 0)) return;

    for (int y = 0; y < height; y++) {
        pD = pDL;
        for (int x = 0; x < width; x++) {
            int pixelIndex = y * width + x;
            unsigned short depth = (unsigned short)pDepth[pixelIndex];
            
            if (g_pDevInfo->nDevType == PUMA)
                zdIndex = (depth << 3) * sizeof(unsigned short);
            else
                zdIndex = depth * sizeof(unsigned short);
            z = (((unsigned short)g_pzdTable[zdIndex]) << 8) + g_pzdTable[zdIndex + 1];
            if ( z >= COLOR_PALETTE_MAX_COUNT) continue;
           //printf("depth : %d, z value : %d\n",depth,z);
            pClr = &(pColorPalette[z]);
            pD[0] = pClr->rgbRed;
            pD[1] = pClr->rgbGreen;
            pD[2] = pClr->rgbBlue;

            pD += 3;
        }
        pDL += nBPS;
    }
}

void UpdateD11DisplayImage_DIB24(const RGBQUAD* pColorPalette, const unsigned char *pDepth, unsigned char *pRGB, int width, int height)
{
    if (width <=0 || height <= 0 ) return;

    int nBPS = ((width * 3 + 3 ) / 4 ) * 4;
    //BYTE* pDL    = pRGB + (height - 1 ) * nBPS;
    BYTE* pDL    = pRGB;
    BYTE* pD     = NULL;
    const RGBQUAD* pClr = NULL;
    unsigned short z = 0;

    for (int y = 0; y < height; y++) {
        pD = pDL;
        for (int x = 0; x < width; x++) {
            int pixelIndex = y * width + x;
            unsigned short depth = pDepth[pixelIndex * sizeof(unsigned short) + 1] << 8 |  pDepth[pixelIndex * sizeof(unsigned short)];
            unsigned short zdIndex = depth * sizeof(unsigned short);
            z = (((unsigned short)g_pzdTable[zdIndex]) << 8) + g_pzdTable[zdIndex + 1];
            //printf("depth : %d, z value : %d\n",depth,z);
            if ( z >= COLOR_PALETTE_MAX_COUNT) continue;
            pClr = &(pColorPalette[z]);
            pD[0] = pClr->rgbRed;
            pD[1] = pClr->rgbGreen;
            pD[2] = pClr->rgbBlue;
            pD += 3;
        }
        pDL += nBPS;
    }
}

void UpdateZ14DisplayImage_DIB24(RGBQUAD *pColorPaletteZ14, unsigned char *pDepthZ14, unsigned char *pDepthDIB24, int cx, int cy)
{
    int x,y,nBPS;
    unsigned short *pWSL,*pWS;
    unsigned char *pDL,*pD;
    RGBQUAD *pClr;
    //
    if ((cx <= 0) || (cy <= 0)) return;
    //
    nBPS = cx * 3;
    pWSL = (unsigned short *)pDepthZ14;
    pDL = pDepthDIB24;
    for (y=0; y<cy; y++) {
        pWS = pWSL;
        pD = pDL;
        for (x=0; x<cx; x++) {
            if ( pWS[x] >= COLOR_PALETTE_MAX_COUNT) continue;
            pClr = &(pColorPaletteZ14[pWS[x]]);
            pD[0] = pClr->rgbRed;
            pD[1] = pClr->rgbGreen;
            pD[2] = pClr->rgbBlue;
            pD += 3;
        }
        pWSL += cx;
        pDL += nBPS;
    }
}

//e:[eys3D] 20200615 implement ZD table

char* PidToModuleName(unsigned short pid)
{
    if (pid == 0x120) {
        return "EX8036";
    } else if (pid == 0x121) {
        return "EX8037";
    } else if (pid == 0x137) {
        return "EX8052";
    } else if (pid == 0x160) {
        return "Hypatia";
    } else if (pid == 0x0124) {
	return "EX8038-1";
    } else if (pid == 0x0147) {
	return "EX8038-2";
    } else {
        return nullptr;
    }
}

int get_product_name(char *path, char *out)
{
    FILE* f;
    char buffer[128];
    int i = 0;

    f = fopen(path, "r" );
    if (!f) {
        printf("Could not open %s\n", path);
        return -1;
    }

    fgets(buffer, sizeof(buffer), f);
    do {
        out[i] = buffer[i];
        i++;
    } while(buffer[i] != '\0');
    i--;
    out[i] = ':';
    i++;
    out[i] = '\0';
    fclose( f );

    return 0;
}

static void setupDepth(void)
{
    printf("APC_DEPTH_DATA_DEFAULT: %d\n", APC_DEPTH_DATA_DEFAULT);
    printf("APC_DEPTH_DATA_8_BITS: %d\n", APC_DEPTH_DATA_8_BITS);
    printf("APC_DEPTH_DATA_14_BITS: %d\n", APC_DEPTH_DATA_14_BITS);
    printf("APC_DEPTH_DATA_8_BITS_x80: %d\n", APC_DEPTH_DATA_8_BITS_x80);
    printf("APC_DEPTH_DATA_11_BITS: %d\n", APC_DEPTH_DATA_11_BITS);
    printf("APC_DEPTH_DATA_OFF_RECTIFY: %d\n", APC_DEPTH_DATA_OFF_RECTIFY);
    printf("APC_DEPTH_DATA_8_BITS_RAW: %d\n", APC_DEPTH_DATA_8_BITS_RAW);
    printf("APC_DEPTH_DATA_14_BITS_RAW: %d\n", APC_DEPTH_DATA_14_BITS_RAW);
    printf("APC_DEPTH_DATA_8_BITS_x80_RAW: %d\n", APC_DEPTH_DATA_8_BITS_x80_RAW);
    printf("APC_DEPTH_DATA_11_BITS_RAW: %d\n", APC_DEPTH_DATA_11_BITS_RAW);
    printf("APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY: %d\n", APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY);
    printf("Please input depth type value:\n");
    scanf("%d", &gDepthDataType);
}

static bool IsInterleaveMode(void)
{
    return false;
    
}
static int NormalizeDepthDataType(int *nDepthDataType)
{
    if (*nDepthDataType >= APC_DEPTH_DATA_INTERLEAVE_MODE_OFFSET + APC_DEPTH_DATA_SCALE_DOWN_MODE_OFFSET)
    {
        *nDepthDataType -= APC_DEPTH_DATA_INTERLEAVE_MODE_OFFSET + APC_DEPTH_DATA_SCALE_DOWN_MODE_OFFSET;
    }
    else if (*nDepthDataType >= APC_DEPTH_DATA_SCALE_DOWN_MODE_OFFSET)
    {
        *nDepthDataType -= APC_DEPTH_DATA_SCALE_DOWN_MODE_OFFSET;
    }
    else if (*nDepthDataType >= APC_DEPTH_DATA_INTERLEAVE_MODE_OFFSET)
    {
        *nDepthDataType -= APC_DEPTH_DATA_INTERLEAVE_MODE_OFFSET;
    }

    return APC_OK;
}

static int TransformDepthDataType(int *nDepthDataType, bool bRectifyData)
{
    NormalizeDepthDataType(nDepthDataType);

    switch (*nDepthDataType){
        case APC_DEPTH_DATA_8_BITS:
        case APC_DEPTH_DATA_8_BITS_RAW:
            *nDepthDataType = bRectifyData ? APC_DEPTH_DATA_8_BITS : APC_DEPTH_DATA_8_BITS_RAW; break;
        case APC_DEPTH_DATA_8_BITS_x80:
        case APC_DEPTH_DATA_8_BITS_x80_RAW:
            *nDepthDataType = bRectifyData ? APC_DEPTH_DATA_8_BITS_x80 : APC_DEPTH_DATA_8_BITS_x80_RAW; break;
        case APC_DEPTH_DATA_11_BITS:
        case APC_DEPTH_DATA_11_BITS_RAW:
        case APC_DEPTH_DATA_11_BITS_COMBINED_RECTIFY:
            *nDepthDataType = bRectifyData ? APC_DEPTH_DATA_11_BITS : APC_DEPTH_DATA_11_BITS_RAW;  break;
        case APC_DEPTH_DATA_14_BITS:
        case APC_DEPTH_DATA_14_BITS_RAW:
        case APC_DEPTH_DATA_14_BITS_COMBINED_RECTIFY:
            *nDepthDataType = bRectifyData ? APC_DEPTH_DATA_14_BITS : APC_DEPTH_DATA_14_BITS_RAW;  break;
        case APC_DEPTH_DATA_OFF_RAW:
        case APC_DEPTH_DATA_OFF_RECTIFY:
            *nDepthDataType = bRectifyData ? APC_DEPTH_DATA_OFF_RECTIFY : APC_DEPTH_DATA_OFF_RAW;  break;
        default:
            *nDepthDataType = bRectifyData ? APC_DEPTH_DATA_DEFAULT : APC_DEPTH_DATA_OFF_RECTIFY;  break;
    }

    if ((gCameraPID == APC_PID_8036 || gCameraPID == APC_PID_8052) && (gDepthWidth == 640 && gDepthHeight == 360)) {
        
        *nDepthDataType += APC_DEPTH_DATA_SCALE_DOWN_MODE_OFFSET;
    }

    if (IsInterleaveMode()) *nDepthDataType += APC_DEPTH_DATA_INTERLEAVE_MODE_OFFSET;

    printf("gCameraPID = 0x%08x\n", gCameraPID);
    return APC_OK;
}


static void *pfunc_thread_close(void *arg)
{
    int ret;

    UNUSED(arg);
    while (1) {
        sleep(1);
        //if (bTestEnd_color == true && bTestEnd_depth == true) {
            ret = APC_CloseDevice(EYSD, &gsDevSelInfo);
            if(ret == APC_OK) {
                printf("APC_CloseDevice() success!\n");
            } else {
                printf("APC_CloseDevice() fail.. (ret=%d)\n", ret);
                error_msg(ret);
            }
            break;
        //}
    }
    return NULL;
}

static long long calcByGetTimeOfDay() 
    {
    struct timeval startTime;
    long long elapsed;

    gettimeofday(&startTime, NULL);

    elapsed = (long long)(startTime.tv_sec) * 1000000 + (long long)(startTime.tv_usec);

    return elapsed;
}

void print_APC_error(int error)
{
	const char *errorstr = NULL;

	switch (error) {
	case APC_OK:
		errorstr = "APC_OK";
		break;
	case APC_NoDevice:
		errorstr = "APC_NoDevice";
		break;
	case APC_FIND_DEVICE_FAIL:
		errorstr = "APC_FIND_DEVICE_FAIL";
		break;
	case APC_NullPtr:
		errorstr = "APC_NullPtr";
		break;
	case APC_ErrBufLen:
		errorstr = "APC_ErrBufLen";
		break;
	case APC_RET_BAD_PARAM:
		errorstr = "APC_RET_BAD_PARAM";
		break;
	case APC_Init_Fail:
		errorstr = "APC_Init_Fail";
		break;
	case APC_NoZDTable:
		errorstr = "APC_NoZDTable";
		break;
	case APC_READFLASHFAIL:
		errorstr = "APC_READFLASHFAIL";
		break;
	case APC_WRITEFLASHFAIL:
		errorstr = "APC_WRITEFLASHFAIL";
		break;
	case APC_VERIFY_DATA_FAIL:
		errorstr = "APC_VERIFY_DATA_FAIL";
		break;
	case APC_KEEP_DATA_FAIL:
		errorstr = "APC_KEEP_DATA_FAIL";
		break;
	case APC_RECT_DATA_LEN_FAIL:
		errorstr = "APC_RECT_DATA_LEN_FAIL";
		break;
	case APC_RECT_DATA_PARSING_FAIL:
		errorstr = "APC_RECT_DATA_PARSING_FAIL";
		break;
	case APC_NO_CALIBRATION_LOG:
		errorstr = "APC_NO_CALIBRATION_LOG";
		break;
	case APC_POSTPROCESS_INIT_FAIL:
		errorstr = "APC_POSTPROCESS_INIT_FAIL";
		break;
	case APC_POSTPROCESS_NOT_INIT:
		errorstr = "APC_POSTPROCESS_NOT_INIT";
		break;
	case APC_POSTPROCESS_FRAME_FAIL:
		errorstr = "APC_POSTPROCESS_FRAME_FAIL";
		break;
	case APC_RET_OPEN_FILE_FAIL:
		errorstr = "APC_RET_OPEN_FILE_FAIL";
		break;
	case APC_OPEN_DEVICE_FAIL:
		errorstr = "APC_OPEN_DEVICE_FAIL";
		break;
	case APC_CLOSE_DEVICE_FAIL:
		errorstr = "APC_CLOSE_DEVICE_FAIL";
		break;
	case APC_GET_RES_LIST_FAIL:
		errorstr = "APC_GET_RES_LIST_FAIL";
		break;
	case APC_READ_REG_FAIL:
		errorstr = "APC_READ_REG_FAIL";
		break;
	case APC_WRITE_REG_FAIL:
		errorstr = "APC_WRITE_REG_FAIL";
		break;
	case APC_SET_FPS_FAIL:
		errorstr = "APC_SET_FPS_FAIL";
		break;
	case APC_VIDEO_RENDER_FAIL:
		errorstr = "APC_VIDEO_RENDER_FAIL";
		break;
	case APC_GET_IMAGE_FAIL:
		errorstr = "APC_GET_IMAGE_FAIL";
		break;
	case APC_CALLBACK_REGISTER_FAIL:
		errorstr = "APC_CALLBACK_REGISTER_FAIL";
		break;
	case APC_GET_CALIBRATIONLOG_FAIL:
		errorstr = "APC_GET_CALIBRATIONLOG_FAIL";
		break;
	case APC_SET_CALIBRATIONLOG_FAIL:
		errorstr = "APC_SET_CALIBRATIONLOG_FAIL";
		break;
	case APC_NotSupport:
		errorstr = "APC_NotSupport";
		break;
	case APC_NOT_SUPPORT_RES:
		errorstr = "APC_NOT_SUPPORT_RES";
		break;
	case APC_DEVICE_NOT_SUPPORT:
		errorstr = "APC_DEVICE_NOT_SUPPORT";
		break;
	case APC_DEVICE_BUSY:
		errorstr = "APC_DEVICE_BUSY";
		break;
	default:
		errorstr = "UNKNOWN..";
	}

	printf("%s\n", errorstr);
}

static int error_msg(int error)
{
    int ret = ERROR_NONE;
    const char *errstrEYSD = NULL;
    const char *errstrSensor = NULL;

	switch (error) {
	case APC_OK:
                errstrEYSD = "APC_OK";
		break;
	case APC_NoDevice:
                errstrEYSD = "APC_NoDevice";
		break;
	case APC_FIND_DEVICE_FAIL:
                errstrEYSD = "APC_FIND_DEVICE_FAIL";
		break;
	case APC_NullPtr:
                errstrEYSD = "APC_NullPtr";
		break;
	case APC_ErrBufLen:
                errstrEYSD = "APC_ErrBufLen";
		break;
	case APC_RET_BAD_PARAM:
                errstrEYSD = "APC_RET_BAD_PARAM";
		break;
	case APC_Init_Fail:
                errstrEYSD = "APC_Init_Fail";
		break;
	case APC_NoZDTable:
                errstrEYSD = "APC_NoZDTable";
		break;
	case APC_READFLASHFAIL:
                errstrEYSD = "APC_READFLASHFAIL";
		break;
	case APC_WRITEFLASHFAIL:
                errstrEYSD = "APC_WRITEFLASHFAIL";
		break;
	case APC_VERIFY_DATA_FAIL:
                errstrEYSD = "APC_VERIFY_DATA_FAIL";
		break;
	case APC_KEEP_DATA_FAIL:
                errstrEYSD = "APC_KEEP_DATA_FAIL";
		break;
	case APC_RECT_DATA_LEN_FAIL:
                errstrEYSD = "APC_RECT_DATA_LEN_FAIL";
		break;
	case APC_RECT_DATA_PARSING_FAIL:
                errstrEYSD = "APC_RECT_DATA_PARSING_FAIL";
		break;
	case APC_NO_CALIBRATION_LOG:
                errstrEYSD = "APC_NO_CALIBRATION_LOG";
		break;
	case APC_POSTPROCESS_INIT_FAIL:
                errstrEYSD = "APC_POSTPROCESS_INIT_FAIL";
		break;
	case APC_POSTPROCESS_NOT_INIT:
                errstrEYSD = "APC_POSTPROCESS_NOT_INIT";
		break;
	case APC_POSTPROCESS_FRAME_FAIL:
                errstrEYSD = "APC_POSTPROCESS_FRAME_FAIL";
		break;
	case APC_RET_OPEN_FILE_FAIL:
                errstrEYSD = "APC_RET_OPEN_FILE_FAIL";
		break;
	case APC_OPEN_DEVICE_FAIL:
                errstrEYSD = "APC_OPEN_DEVICE_FAIL";
		break;
	case APC_CLOSE_DEVICE_FAIL:
                errstrEYSD = "APC_CLOSE_DEVICE_FAIL";
		break;
	case APC_GET_RES_LIST_FAIL:
                errstrEYSD = "APC_GET_RES_LIST_FAIL";
		break;
	case APC_READ_REG_FAIL:
                errstrEYSD = "APC_READ_REG_FAIL";
		break;
	case APC_WRITE_REG_FAIL:
                errstrEYSD = "APC_WRITE_REG_FAIL";
		break;
	case APC_SET_FPS_FAIL:
                errstrEYSD = "APC_SET_FPS_FAIL";
		break;
	case APC_VIDEO_RENDER_FAIL:
                errstrEYSD = "APC_VIDEO_RENDER_FAIL";
		break;
	case APC_GET_IMAGE_FAIL:
                errstrEYSD = "APC_GET_IMAGE_FAIL";
		break;
	case APC_CALLBACK_REGISTER_FAIL:
                errstrEYSD = "APC_CALLBACK_REGISTER_FAIL";
		break;
	case APC_GET_CALIBRATIONLOG_FAIL:
                errstrEYSD = "APC_GET_CALIBRATIONLOG_FAIL";
		break;
	case APC_SET_CALIBRATIONLOG_FAIL:
                errstrEYSD = "APC_SET_CALIBRATIONLOG_FAIL";
		break;
	case APC_NotSupport:
                errstrEYSD = "APC_NotSupport";
		break;
	case APC_NOT_SUPPORT_RES:
                errstrEYSD = "APC_NOT_SUPPORT_RES";
		break;
	case APC_DEVICE_NOT_SUPPORT:
                errstrEYSD = "APC_DEVICE_NOT_SUPPORT";
		break;
	case APC_DEVICE_BUSY:
                errstrEYSD = "APC_DEVICE_BUSY";
		break;
	default:
                errstrEYSD = "APC_UNKNOWN..";
	}

	switch (error) {
	case APC_OK:
                ret = ERROR_NONE;
                errstrSensor = "ERROR_NONE";
		break;
	case APC_NoDevice:
	case APC_FIND_DEVICE_FAIL:
                ret = ERROR_NO_SUCH_DEVICE;
                errstrSensor = "ERROR_NO_SUCH_DEVICE";
		break;
	case APC_NullPtr:
	case APC_ErrBufLen:
	case APC_RET_BAD_PARAM:
                ret = ERROR_INVALID_PARAMETER;
                errstrSensor = "ERROR_INVALID_PARAMETER";
		break;
	case APC_Init_Fail:
	case APC_NoZDTable:
	case APC_READFLASHFAIL:
	case APC_WRITEFLASHFAIL:
	case APC_VERIFY_DATA_FAIL:
	case APC_KEEP_DATA_FAIL:
	case APC_RECT_DATA_LEN_FAIL:
	case APC_RECT_DATA_PARSING_FAIL:
	case APC_NO_CALIBRATION_LOG:
	case APC_POSTPROCESS_INIT_FAIL:
	case APC_POSTPROCESS_NOT_INIT:
	case APC_POSTPROCESS_FRAME_FAIL:
	case APC_RET_OPEN_FILE_FAIL:
	case APC_OPEN_DEVICE_FAIL:
	case APC_CLOSE_DEVICE_FAIL:
	case APC_GET_RES_LIST_FAIL:
	case APC_READ_REG_FAIL:
	case APC_WRITE_REG_FAIL:
	case APC_SET_FPS_FAIL:
	case APC_VIDEO_RENDER_FAIL:
	case APC_GET_IMAGE_FAIL:
	case APC_CALLBACK_REGISTER_FAIL:
	case APC_GET_CALIBRATIONLOG_FAIL:
	case APC_SET_CALIBRATIONLOG_FAIL:
                ret = ERROR_IO_ERROR;
                errstrSensor = "ERROR_IO_ERROR";
		break;
	case APC_NotSupport:
	case APC_NOT_SUPPORT_RES:
	case APC_DEVICE_NOT_SUPPORT:
                ret = ERROR_NOT_SUPPORTED;
                errstrSensor = "ERROR_NOT_SUPPORTED";
		break;
	case APC_DEVICE_BUSY:
                ret = ERROR_RESOURCE_BUSY;
                errstrSensor = "ERROR_RESOURCE_BUSY";
		break;
	default:
                ret = ERROR_UNKNOWN;
                errstrSensor = "ERROR_UNKNOWN";
	}

        printf("[ERROR] %s (0x%08x) -> %s (0x%08x)\n", errstrEYSD, error, errstrSensor, ret);

	return ret;
}

//s:[eys3D] 20200623, auto config video mode for Hypatia project
void setHypatiaVideoMode(int mode) {
        int ret = 0;

        if (APC_SetupBlock(EYSD, &gsDevSelInfo, true) != 0) {
            printf("setup Blocking Failed\n");
        }

        snapShot_color = true;
        snapShot_depth = true;
        g_depth_output = DEPTH_IMG_COLORFUL_TRANSFER;
        gDepth_Transfer_ctrl = (DEPTH_TRANSFER_CTRL)DEPTH_IMG_NON_TRANSFER;
        gDepthDataType = APC_DEPTH_DATA_11_BITS;

        switch (mode) {
            case 1:
                gColorFormat = 1;
                gColorWidth = 640;
                gColorHeight = 400;

                gDepthWidth = 640;
                gDepthHeight = 400;
                gActualFps = 30;
                break;
            case 2:
                gColorFormat = 1;
                gColorWidth = 320;
                gColorHeight = 200;

                gDepthWidth = 320;
                gDepthHeight = 200;
                gActualFps = 30;
                break;
            case 3:
                gColorFormat = 1;
                gColorWidth = 320;
                gColorHeight = 104;

                gDepthWidth = 320;
                gDepthHeight = 104;
                gActualFps = 30;
                break;
            case 4:
                gColorFormat = 1;
                gColorWidth = 1280;
                gColorHeight = 400;

                gDepthWidth = 640;
                gDepthHeight = 400;
                gActualFps = 15;
                break;
            case 5:
                gColorFormat = 1;
                gColorWidth = 640;
                gColorHeight = 200;

                gDepthWidth = 320;
                gDepthHeight = 200;
                gActualFps = 15;
                break;
            case 6:
                gColorFormat = 1;
                gColorWidth = 640;
                gColorHeight = 104;

                gDepthWidth = 320;
                gDepthHeight =104;
                gActualFps = 15;
                break;
            case 7:
                gColorFormat = 0;
                gColorWidth = 1280;
                gColorHeight = 400;

                gDepthWidth =0;
                gDepthHeight = 0;
                gActualFps = 15;
                gDepthDataType = APC_DEPTH_DATA_DEFAULT;
                break;
            case 8:
                gColorFormat = 0;
                gColorWidth = 640;
                gColorHeight =200;

                gDepthWidth = 0;
                gDepthHeight = 0;
                gActualFps = 15;
                gDepthDataType = APC_DEPTH_DATA_DEFAULT;
                break;
            case 9:
                gColorFormat = 0;
                gColorWidth = 640;
                gColorHeight = 104;

                gDepthWidth = 0;
                gDepthHeight = 0;
                gActualFps = 15;
                gDepthDataType = APC_DEPTH_DATA_DEFAULT;
                break;
            default:
                break;
        }

        ret = APC_SetDepthDataType(EYSD, &gsDevSelInfo, gDepthDataType); //4 ==> 11 bits
        if (ret == APC_OK) {
            printf("APC_SetDepthData() success!\n");
        } else {
            printf("APC_SetDepthData() fail.. (ret=%d)\n", ret);
            print_APC_error(ret);
        }
}
//e:[eys3D] 20200623, auto config video mode for Hypatia project

//s:[eys3D] 20200623, for IR mode
int setupIR(unsigned short IRvalue)
{
    int ret;
    unsigned short m_nIRMax, m_nIRMin, m_nIRValue;
    ret = APC_GetFWRegister(EYSD, &gsDevSelInfo,
                                0xE2, &m_nIRMax,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret) return ret;

    ret = APC_GetFWRegister(EYSD, &gsDevSelInfo,
                                0xE1, &m_nIRMin,FG_Address_1Byte | FG_Value_1Byte);
    if (APC_OK != ret) return ret;

    if (IRvalue > m_nIRMax || m_nIRMax < m_nIRMin) {
        m_nIRValue = (m_nIRMax - m_nIRMin) / 2;
    } else {
        m_nIRValue = IRvalue;
    }
    printf("IR range, IR Min : %d, IR Max : %d, set IR Value : %d\n", m_nIRMin, m_nIRMax, m_nIRValue);

    if (m_nIRValue != 0) {
        ret = APC_SetIRMode(EYSD, &gsDevSelInfo, 0x63); // 6 bits on for opening both 6 ir
        if (APC_OK != ret) return ret;
        printf("enable IR and set IR Value : %d\n",m_nIRValue);
        ret = APC_SetCurrentIRValue(EYSD, &gsDevSelInfo, m_nIRValue);
        if (APC_OK != ret) return ret;
        ret = APC_GetCurrentIRValue(EYSD, &gsDevSelInfo, &m_nIRValue);
        if (APC_OK != ret) return ret;
        printf("get IR Value : %d\n",m_nIRValue);
    } else {
        ret = APC_SetCurrentIRValue(EYSD, &gsDevSelInfo, m_nIRValue);
        if (APC_OK != ret) return ret;
        ret = APC_SetIRMode(EYSD,&gsDevSelInfo, 0x00); // turn off ir
        if (APC_OK != ret) return ret;
        printf("disable IR\n");
    }
    return APC_OK;
}
//e:[eys3D] 20200623, for IR mode
