#include "gtest/gtest.h"
#include "eSPDI.h"
#include "eSPDI_def.h"
#include "ModeConfig.h"
#include <thread>
#include <chrono>
#include <vector>
#include <math.h>
#include "FPSCalculator.h"

void *pEtronDI = nullptr;
DEVSELINFO devSelInfo;
DEVINFORMATION devInformation;
std::vector<ModeConfig::MODE_CONFIG> modeConfigs;

TEST(ModuleTest, Init)
{
    ASSERT_EQ(ETronDI_OK, EtronDI_Init(&pEtronDI, false));
}

TEST(ModuleTest, GetDeviceNumber)
{
    int device_count = EtronDI_GetDeviceNumber(&pEtronDI);
    EXPECT_TRUE(device_count > 0);
}

TEST(ModuleTest, GetDeviceInfo)
{
    devSelInfo.index = 0;
    ASSERT_EQ(ETronDI_OK, EtronDI_GetDeviceInfo(pEtronDI, &devSelInfo, &devInformation));
}

TEST(ModuleTest, SelectModeConfig)
{
    modeConfigs = ModeConfig::GetModeConfig().GetModeConfigList(devInformation.wPID);

    EXPECT_TRUE(modeConfigs.size() > 0);

#ifndef NDEBUG
    for (ModeConfig::MODE_CONFIG config : modeConfigs){
        printf("=======================\n");
        printf("Mode : %d\n", config.iMode);
        printf("Usb Type : %d\n", config.iUSB_Type);
        printf("InterLeave Mode FPS : %d\n", config.iInterLeaveModeFPS);
        printf("RectifyMode : %d\n", config.bRectifyMode);
        printf("DecodeType_L : %d\n", config.eDecodeType_L);
        printf("DecodeType_K : %d\n", config.eDecodeType_K);
        printf("DecodeType_T : %d\n", config.eDecodeType_T);
        printf("L_Resolution : [%d, %d]\n", config.L_Resolution.Width, config.L_Resolution.Height);
        printf("D_Resolution : [%d, %d]\n", config.D_Resolution.Width, config.D_Resolution.Height);
        printf("K_Resolution : [%d, %d]\n", config.K_Resolution.Width, config.K_Resolution.Height);
        printf("T_Resolution : [%d, %d]\n", config.T_Resolution.Width, config.T_Resolution.Height);
        printf("Depth Type [");
        for (int depth_type : config.vecDepthType)
            printf("%d,", depth_type);
        printf("]\n");
        printf("Color Fps [");
        for (int color_fps : config.vecColorFps)
            printf("%d,", color_fps);
        printf("]\n");
        printf("Depth Fps [");
        for (int depth_fps : config.vecDepthFps)
            printf("%d,", depth_fps);
        printf("]\n");
        printf("Mode Desc : %s\n", config.csModeDesc.c_str());
        printf("=======================\n");
    }
#endif
}

int GetDepthDataType(int bit, bool isRectify, bool isInterleave)
{
    int result = -1;
    switch (bit){
        case 0:
            result = isRectify ? ETronDI_DEPTH_DATA_DEFAULT : 
                                 ETronDI_DEPTH_DATA_OFF_RAW ;
            break;
        case 8:
            result = isRectify ? ETronDI_DEPTH_DATA_8_BITS : 
                                 ETronDI_DEPTH_DATA_8_BITS_RAW;
            break;
        case 11:
            result = isRectify ? ETronDI_DEPTH_DATA_11_BITS : 
                                 ETronDI_DEPTH_DATA_11_BITS_RAW;
            break;
        case 14:
            result = isRectify ? ETronDI_DEPTH_DATA_14_BITS : 
                                 ETronDI_DEPTH_DATA_14_BITS_RAW;
            break;
    }

    if (isInterleave)
        result += ETronDI_DEPTH_DATA_INTERLEAVE_MODE_OFFSET;

    return result;
}

void TEST_Preview(int usb_type)
{
    for (ModeConfig::MODE_CONFIG config : modeConfigs)
    {
        if (config.iUSB_Type != usb_type)
            continue;

        int color_width = config.L_Resolution.Width;
        int color_height = config.L_Resolution.Height;

        int depth_width = config.D_Resolution.Width;
        int depth_height = config.D_Resolution.Height;

        std::vector<int> vecFps = !config.vecColorFps.empty() ? config.vecColorFps : config.vecDepthFps;

        for (int fps : vecFps)
        {

            auto Preivew = [&](int depth_data_type) {
                EXPECT_EQ(ETronDI_OK, EtronDI_SetDepthDataType(pEtronDI, &devSelInfo, depth_data_type));

                bool has_color = (color_width != 0 && color_height != 0);
                bool has_depth = (depth_width != 0 && depth_height != 0);

                std::vector<BYTE> color_buffer;
                if (has_color)
                {
                    color_buffer.resize(color_width * color_height * 3);
                }

                std::vector<BYTE> depth_buffer;
                if (has_depth)
                {
                    EtronDIImageType::Value imageType = EtronDIImageType::DepthDataTypeToDepthImageType(depth_data_type);

                    ASSERT_NE(imageType, EtronDIImageType::IMAGE_UNKNOWN);
                    ASSERT_NE(imageType, EtronDIImageType::DEPTH_8BITS_0x80);

                    int byte_per_pixel = 0;
                    switch (EtronDIImageType::DepthDataTypeToDepthImageType(depth_data_type))
                    {
                        case EtronDIImageType::DEPTH_8BITS: 
                            byte_per_pixel = 1; 
                            break;
                        case EtronDIImageType::DEPTH_11BITS:
                        case EtronDIImageType::DEPTH_14BITS:
                            byte_per_pixel = 2;
                            break;
                        default: break;
                    }

                    depth_buffer.resize(depth_width * depth_height * byte_per_pixel);
                }

                EXPECT_EQ(ETronDI_OK, EtronDI_OpenDevice2(pEtronDI, &devSelInfo,
                                                          color_width, color_height,
                                                          config.eDecodeType_L == ModeConfig::MODE_CONFIG::MJPEG,
                                                          depth_width, depth_height,
                                                          DEPTH_IMG_NON_TRANSFER,
                                                          true, nullptr,
                                                          &fps,
                                                          IMAGE_SN_SYNC));
                
                std::clock_t start = std::clock();
                const int duration_sec = 10;

                FPSCalculator color_fps_calculator;
                FPSCalculator depth_fps_calculator;
                unsigned long int image_size;
                int serial_number;
                do
                {
                    if (has_color)
                    {
                        if (ETronDI_OK == EtronDI_GetColorImage(pEtronDI, &devSelInfo,
                                                                &color_buffer[0],
                                                                &image_size,
                                                                &serial_number,
                                                                depth_data_type))
                        {
                            color_fps_calculator.clock();
                        }
                    }

                    if (has_depth)
                    {
                        if (ETronDI_OK == EtronDI_GetDepthImage(pEtronDI, &devSelInfo,
                                                                &depth_buffer[0],
                                                                &image_size,
                                                                &serial_number,
                                                                depth_data_type))
                        {
                            depth_fps_calculator.clock();
                        }
                    }
                }
                while (((std::clock() - start) / CLOCKS_PER_SEC) <= duration_sec);

                printf("=========================\n");
                printf("Mode %d, depth data type:%d\n", config.iMode, depth_data_type);
                if (has_color)
                {
                    double color_fps = color_fps_calculator.GetFPS();
                    printf("Color FPS target[%d], current[%.2f]\n", fps, color_fps);
                    EXPECT_LE(fabs(color_fps - fps), 3);
                }

                if (has_depth){
                    double depth_fps = depth_fps_calculator.GetFPS();
                    printf("Depth FPS target[%d], current[%.2f]\n", fps, depth_fps);
                    EXPECT_LE(fabs(depth_fps - fps), 3);
                }
                printf("=========================\n");                
                EXPECT_EQ(ETronDI_OK, EtronDI_CloseDevice(pEtronDI, &devSelInfo));

                std::this_thread::sleep_for(std::chrono::seconds(2));
            };

            if (depth_width != 0 && depth_height != 0)
            {
                for (int depth_type : config.vecDepthType)
                {
                    Preivew(GetDepthDataType(depth_type, config.bRectifyMode, config.iInterLeaveModeFPS == fps));
                }
            }else
            {
                Preivew(GetDepthDataType(0, config.bRectifyMode, config.iInterLeaveModeFPS == fps));
            }
        }
    }
}

TEST(ModuleTest, Preview)
{
    USB_PORT_TYPE usb_type;
    EtronDI_GetDevicePortType(pEtronDI, &devSelInfo, &usb_type);
    ASSERT_NE(usb_type, USB_PORT_TYPE_UNKNOW);
    TEST_Preview((usb_type == USB_PORT_TYPE_3_0) ? 3 : 2);
}