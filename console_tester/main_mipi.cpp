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


#define SAVE_FILE_DIR "./out_img/"
#define MAX_FILE_PATH_LEN 1024

#define COLOR_DEPTH_STR "COLOR_DEPTH"

#define ONLY_PRINT_OVER_DIFF 1

static void* EYSD = NULL;
static DEVSELINFO gsDevSelInfo;

static int gColorFormat = 0; // 0:YUYV (only support YUYV) 
static int gColorWidth = 1280;
static int gColorHeight = 720;

static int gDepthWidth = 1280; // Depth is only YUYV format
static int gDepthHeight = 720;

static int gActualFps = 30;
DEPTH_TRANSFER_CTRL gDepth_Transfer_ctrl = DEPTH_IMG_NON_TRANSFER;

static unsigned char *gColorImgBuf = NULL;
static unsigned char *gDepthImgBuf = NULL;
static unsigned long int gColorImgSize = 0;
static unsigned long int gDepthImgSize = 0;

static bool bSnapshot = true;

static int init_device(void);
static void release_device(void);
static int open_device(void);
static int close_device(void);
static void *get_color_depth_image(void *arg);

int main(void)
{
    int ret = 0;
    
    printf("MIPI SDK sample code !!\n");
    
    ret = init_device();
    if (ret == APC_OK) {
        ret = open_device();
        if (ret == APC_OK) { 
            get_color_depth_image(NULL);
            ret = close_device();
            if (ret != APC_OK) {
                printf("Failed to call close_device()!!\n");
            }
        }
    }

    release_device();
    
    return 0;
}



static int init_device(void)
{
    int ret = 0;
    char FWVersion[256] = {0x0};
    int nActualLength = 0;
    
    ret = APC_Init(&EYSD, true);
    if (ret == APC_OK) {
        printf("xxAPC_Init() success! (EYSD : %p)\n", EYSD);
    } else {
        printf("APC_Init() fail.. (ret : %d, EYSD : %p)\n", ret, EYSD);
    }

    if( APC_OK == APC_GetFwVersion(EYSD, &gsDevSelInfo, FWVersion, 256, &nActualLength)) {
        printf("FW Version = [%s]\n", FWVersion);
    }

    return ret;    
}

static void release_device(void)
{
    APC_Release(&EYSD);
}

static int open_device(void)
{
    int ret = 0;
    
    if (!EYSD) {
        init_device();
    }

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
    }
    
    return ret;
}



static int close_device(void)
{
    int ret;
    
    ret = APC_CloseDevice(EYSD, &gsDevSelInfo);
    if(ret == APC_OK) {
        printf("APC_CloseDevice() success!\n");
    } else {
        printf("APC_CloseDevice() fail.. (ret=%d)\n", ret);
    }
    
    return ret;
}



static void *get_color_depth_image(void *arg)
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
    unsigned int max_calc_frame_count = 10;
    int mCount = 0;
    bool bFirstReceived = true;
    const char *pre_str = COLOR_DEPTH_STR;
    int64_t diff = 0;
    int s_diff = 0;
    int serial_number = 0;
    int i = 0;
    uint64_t index = 0;
    unsigned int BytesPerPixel = 2; //Only support YUYV
    char *color_file_name = NULL;
    char *depth_file_name = NULL;
    FILE *color_fp = NULL;
    FILE *depth_fp = NULL;
    
    color_file_name = (char *)calloc(MAX_FILE_PATH_LEN, sizeof(char));
    depth_file_name = (char *)calloc(MAX_FILE_PATH_LEN, sizeof(char));
    
    // snprintf(fname, sizeof(fname), SAVE_FILE_PATH"cloud_%d_%s.ply", yuv_index++, DateTime);
    
    printf("color image: [%d x %d @ %d]\n", gColorWidth, gColorHeight, gActualFps);
    printf("depth image: [%d x %d @ %d]\n", gDepthWidth, gDepthHeight, gActualFps);

    if(gColorImgBuf == NULL) {
        gColorImgBuf = (unsigned char*)calloc(gColorWidth * gColorHeight * BytesPerPixel, sizeof(unsigned char));
    }
    if(gColorImgBuf == NULL) {
        printf("alloc ColorImgBuf fail..\n");
        return NULL;
    }
    
    if(gDepthImgBuf == NULL) {
        gDepthImgBuf = (unsigned char*)calloc(gDepthWidth * gDepthHeight * BytesPerPixel, sizeof(unsigned char));
    }
    
    if(gDepthImgBuf == NULL) {
        printf("alloc DepthImgBuf fail..\n");
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

        ret = APC_Get2ImageWithTimestamp(EYSD, &gsDevSelInfo, (uint8_t*)gColorImgBuf,
                                         (uint8_t *)gDepthImgBuf, &gColorImgSize, &gDepthImgSize,
                                         &cur_serial_num, &cur_depth_serial_num, APC_DEPTH_DATA_11_BITS, &cur_tv_sec, &cur_tv_usec);
        if (ret == APC_OK) {
            serial_number = 0;
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
            
            if (bSnapshot) {
                if (color_fp && gColorImgBuf)
                    fwrite(gColorImgBuf, sizeof(unsigned char), gColorImgSize, color_fp);
                if (depth_fp && gDepthImgBuf)
                    fwrite(gDepthImgBuf, sizeof(unsigned char), gDepthImgSize, depth_fp);
                
            }
            index++;
            
        }
    }
    
    if(gColorImgBuf){
        printf("free gColorImgBuf : %p\n",gColorImgBuf);
        free(gColorImgBuf);
        gColorImgBuf = NULL;
    }
    
    if(gDepthImgBuf){
        printf("free gDepthImgBuf : %p\n",gDepthImgBuf);
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


