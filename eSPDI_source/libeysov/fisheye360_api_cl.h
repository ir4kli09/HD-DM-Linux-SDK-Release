/**
  * @file  fisheye360_api_cl.h
  * @title API for Fisheye360 - OpenCL version
  * @version 1.1.6.2
  */
#ifndef __EYS_FISHEYE360_API_CL_H__
#define __EYS_FISHEYE360_API_CL_H__

/**
  * Define operation system
  * Include file
  */
#ifdef WIN32
#define EYS_EXPORTS __declspec(dllexport)
#include "eys_fisheye360.h"
#include "fisheye360_def.h"
#include "eysov\eys_def.h"
#endif

#ifdef linux
#define EYS_EXPORTS
#include "fisheye360_def.h"
#include "eys_def.h"
#include "utilbase.h"
#endif

/**
  * @defgroup fisheye360 FishEye360
  * Fisheye360 Project
  */

/**
  * @defgroup fisheye360_api_cl FishEye360 API OpenCL Version
  * @ingroup fisheye360
  * API for Fisheye360 - OpenCL version
  */
namespace eys
{
namespace fisheye360
{

/**
  * @addtogroup fisheye360_api_cl
  * @{
  */

/**
  * @brief  Get total number of platform
  * @anchor GPU_Get_Platform_Number_CL
  * @param[out]  platform_all_num Total number of platform
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Get platform number error
  *         \li \c -2 Get platform number is 0, it means no GPU platform
  *
  */
EYS_EXPORTS int GPU_Get_Platform_Number_CL(int &platform_num);

/**
  * @brief  Select GPU platform
  * @anchor GPU_Set_Platform_CL
  * @param[in]   platform_select  Select platform to be used, the value is started from 0.
  * @param[out]  platform_name    The name of selected platform with sizeof char[100], "nullptr" is no output
  * @param[out]  device_name      The name of device in selected platform with sizeof char[100], "nullptr" is no output
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Selected platform error
  *         \li \c -2 Get context from selected platform error
  *         \li \c -3 Can not create command queue
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Set_Platform_CL(int platform_select,
	                                char *platform_name,
	                                char *device_name);

/**
  * @brief  Create buffer for LUT 
  * @anchor GPU_Create_Buffer_LUT_CL
  * @param[in]  src_cols  Look up table width 
  * @param[in]  src_rows  Look up table height
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Can not create buffer
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Create_Buffer_LUT_CL(const int src_cols, const int src_rows);

/**
  * @brief  Write LUT (float type) values to buffer
  * @anchor GPU_Writes_Buffer_LUT_CL
  * @param[in]  src_cols   Look up table width
  * @param[in]  src_rows   Look up table height
  * @param[in]  map_cols   Look up table buffer for x position with size of [src_cols x src_rows] in float type
  * @param[in]  map_rows   Look up table buffer for y position with size of [src_cols x src_rows] in float type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Input buffer error
  *         \li \c -2 Write to GPU buffer error
  *         \li \c -3 Wait event error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Writes_Buffer_LUT_CL(const int src_cols, 
	                                     const int src_rows,
									     float *map_cols,
									     float *map_rows);

/**
  * @brief  Write LUT (uchar type) values to buffer
  * @anchor GPU_Writes_Buffer_LUT_CL
  * @param[in]  src_cols   Look up table width
  * @param[in]  src_rows   Look up table height
  * @param[in]  lut        Look up table buffer with size of [src_cols x src_rows x 4] in uchar type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Input buffer error
  *         \li \c -2 Write to GPU buffer error
  *         \li \c -3 Wait event error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Writes_Buffer_LUT_CL(const int src_cols,
	                                     const int src_rows,
	                                     unsigned char *lut);

/**
  * @brief  Create buffer for source image (RGB)
  * @anchor GPU_Create_Buffer_Img_Source_RGB_CL
  * @param[in] src_cols Source image width
  * @param[in] src_rows Source image height
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Can not create buffer
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Create_Buffer_Img_Source_RGB_CL(const int src_cols, const int src_rows);

/**
  * @brief  Write source image to buffer
  * @anchor GPU_Writes_Buffer_Img_Source_RGB_CL
  * @param[in]  src_cols   Image width in pixel
  * @param[in]  src_rows   Image height in pixel
  * @param[in]  img_Src    Souce image buffer with size of [src_cols x src_rows x 3] in uchar type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Input buffer error
  *         \li \c -2 Write to GPU buffer error
  *         \li \c -3 Wait event error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Writes_Buffer_Img_Source_RGB_CL(const int src_cols, 
	                                                const int src_rows,
													unsigned char *img_src);

/**
  * @brief  Create RGB/BGR buffer for dewarping image
  * @anchor GPU_Create_Buffer_Img_Dewarp_RGB_CL
  * @param[in] src_cols Dewarping image width
  * @param[in] src_rows Dewarping image height
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Can not create buffer
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Create_Buffer_Img_Dewarp_RGB_CL(const int src_cols, const int src_rows);

/**
  * @brief  Create RGBA/BGRA buffer for dewarping image
  * @anchor GPU_Create_Buffer_Img_Dewarp_RGBA_CL
  * @param[in] src_cols Dewarping image width
  * @param[in] src_rows Dewarping image height
  *
  * @return If the function succeeds, the return value is 1. If it fails,  
  *         the following error values:
  *
  *         \li \c -1 Can not create buffer
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Create_Buffer_Img_Dewarp_RGBA_CL(const int src_cols, const int src_rows);

/**
  * @brief  Create RGB/BGR buffer for blending image
  * @anchor GPU_Create_Buffer_Img_Blends_RGB_CL
  * @param[in] src_cols Blending image width
  * @param[in] src_rows Blending image height
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Can not create buffer
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Create_Buffer_Img_Blends_RGB_CL(const int src_cols, const int src_rows);

/**
  * @brief  Create RGBA/BGRA buffer for blending image
  * @anchor GPU_Create_Buffer_Img_Blends_RGBA_CL
  * @param[in] src_cols Blending image width
  * @param[in] src_rows Blending image height
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Can not create buffer
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Create_Buffer_Img_Blends_RGBA_CL(const int src_cols, const int src_rows);

/**
  * @brief  Load all GPU programs in this file 
  * @anchor GPU_Load_Programs_CL
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Kernel error : Remap_Bilinear_RGB2RGB_CL
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Load_Programs_CL();

/**
  * @brief  Run program - Remapping input RGB/BGR image by LUT table and output RGB/BGR uchar image buffer
  * @anchor GPU_Run_Remap_Bilinear_RGB2RGB_CL
  * @param[in]  img_src_cols Source image width (side-by-side) in pixel
  * @param[in]  img_src_rows Source image height in pixel
  * @param[in]  lut_cols     Look up table width
  * @param[in]  lut_rows     Look up table height
  * @param[out] img_dst      Output reamping image buffer with size of [lut_cols x lut_rows x 3] in uchar type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Set parameters to GPU buffer error
  *         \li \c -1 Get image buffer from GPU buffer error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Run_Remap_Bilinear_RGB2RGB_CL(const int img_src_cols,
	                                              const int img_src_rows,
												  const int lut_cols,
												  const int lut_rows,
												  unsigned char *img_dst);

/**
  * @brief  Run program - Remapping input RGB/BGR image by LUT table and output RGBA/BGRA uchar image buffer
  * @anchor GPU_Run_Remap_Bilinear_RGB2RGBA_CL
  * @param[in]  img_src_cols Source image width (side-by-side) in pixel
  * @param[in]  img_src_rows Source image height in pixel
  * @param[in]  lut_cols     Look up table width
  * @param[in]  lut_rows     Look up table height
  * @param[out] img_dst      Output reamping RGBA/BGRA image buffer with size of [lut_cols x lut_rows x 4] in uchar type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Set parameters to GPU buffer error
  *         \li \c -1 Get image buffer from GPU buffer error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Run_Remap_Bilinear_RGB2RGBA_CL(const int img_src_cols,
	                                               const int img_src_rows,
	                                               const int lut_cols,
	                                               const int lut_rows,
	                                               unsigned char *img_dst);

/**
  * @brief  Run program - Blending RGB/BGR image L/R boundary by using alpha-blending method
  * @anchor GPU_Run_Alpha_Blending_RGB_CL
  * @param[in]  img_src_cols Input image width (side-by-side) in pixel
  * @param[in]  img_src_rows Input image height in pixel
  * @param[in]  overlay_lr   Overlay value between L/R boundary in pixel
  * @param[in]  overlay_rl   Overlay value between R/L boundary in pixel
  * @param[out] img_dst      Output dewarping RGB/BGR image buffer with size of [(img_src_cols - overlay_lr - overlay_rl) x img_src_rows x 3] in uchar type
  * @param[in]  cols_scale   Enable when value bigger than 0, it is the working range in cols, and overlay value is according to it
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Set parameters to GPU buffer error
  *         \li \c -2 Get image buffer from GPU buffer error
  *         \li \c -3 Parameter of cols_scale error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Run_Alpha_Blending_RGB_CL(const int img_src_cols,
	                                          const int img_src_rows,
	                                          const int overlay_lr,
	                                          const int overlay_rl,
	                                          unsigned char *img_dst,
											  const int cols_scale = 0);

/**
  * @brief  Run program - Blending RGBA/BGRA image L/R boundary by using alpha-blending method
  * @anchor GPU_Run_Alpha_Blending_RGBA_CL
  * @param[in]  img_src_cols Input image width (side-by-side) in pixel
  * @param[in]  img_src_rows Input image height in pixel
  * @param[in]  overlay_lr   Overlay value between L/R boundary in pixel
  * @param[in]  overlay_rl   Overlay value between R/L boundary in pixel
  * @param[out] img_dst      Output dewarping RGBA/BGRA image buffer with size of [(img_src_cols - overlay_lr - overlay_rl) x img_src_rows x 4] in uchar type
  * @param[in]  cols_scale   Enable when value bigger than 0, it is the working range in cols, and overlay value is according to it
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Set parameters to GPU buffer error
  *         \li \c -2 Get image buffer from GPU buffer error
  *         \li \c -3 Parameter of cols_scale error
  *
  * @code
  int main(void)
  {
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Run_Alpha_Blending_RGBA_CL(const int img_src_cols,
	                                           const int img_src_rows,
	                                           const int overlay_lr,
	                                           const int overlay_rl,
	                                           unsigned char *img_dst,
											   const int cols_scale = 0);

/**
  * @brief  Close GPU platform
  * @anchor GPU_Close_Platform_CL
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Release GPU buffer error
  *
  * @code
  // EXAMPLE 1:
  int main(void)
  {
     // Produce LUT buffer
     eys::fisheye360::ParaLUT str_para;

     str_para.FOV                  = 203.0;
     str_para.semi_FOV_pixels      = 520;
     str_para.img_src_cols         = 1280;
     str_para.img_src_rows         = 960;
     str_para.img_L_src_col_center = 640.0;
     str_para.img_L_src_row_center = 480.0;
     str_para.img_R_src_col_center = 640.0;
     str_para.img_R_src_row_center = 480.0;
     str_para.img_L_rotation       = 0.0;
     str_para.img_R_rotation       = 0.0;
     str_para.spline_control_v1    = 0.0;
     str_para.spline_control_v2    = 0.0;
     str_para.spline_control_v3    = 0.0;
     str_para.spline_control_v4    = 0.0;
     str_para.spline_control_v5    = 0.0;
     str_para.spline_control_v6    = 0.0;
     str_para.spline_control_v7    = 0.0;
     str_para.img_dst_cols         = 980;
     str_para.img_dst_rows         = 920;
     str_para.img_L_dst_shift      = 0;
     str_para.img_R_dst_shift      = 0;

     str_para.unit_sphere_radius   = 212.429;
     str_para.min_col              = -461;
     str_para.max_col              = 1383;
     str_para.min_row              = 0;
     str_para.max_row              = 922;

     float*  map_cols = new float[str_para.img_dst_cols * 2 * str_para.img_dst_rows];
     float*  map_rows = new float[str_para.img_dst_cols * 2 * str_para.img_dst_rows];

     eys::fisheye360::Map_LUT_CC(str_para, nullptr, map_cols, map_rows);

     // RUN GPU
     int err;
     int platform_total;

     int img_src_cols = 1280 * 2;
     int img_src_rows = 960;
     int lut_cols     = 980 * 2;
     int lut_rows     = 920;
     int overlay_LR   = 40;
     int overlay_RL   = 38;
     int img_dst_cols = 980 * 2 - overlay_LR - overlay_RL;
     int img_dst_rows = 920;

     char *platform_name = new char[100];
     char *device_name   = new char[100];

     unsigned char *img_src  = new unsigned char[img_src_cols*img_src_rows * 3];
     unsigned char *img_dst  = new unsigned char[lut_cols*lut_rows * 3];
     unsigned char *img_dst2 = new unsigned char[img_dst_cols*img_dst_rows * 3];

     err = eys::img_io::Img_Read_CV(img_src_cols, img_src_rows, "GPUimg.jpg", img_src);

     err = eys::fisheye360::GPU_Get_Platform_Number_CL(platform_total);
     err = eys::fisheye360::GPU_Set_Platform_CL(0, platform_name, device_name);

     std::cout << platform_name << std::endl;
     std::cout << device_name << std::endl;

     err = eys::fisheye360::GPU_Create_Buffer_LUT_CL(lut_cols, lut_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Source_RGB_CL(img_src_cols, img_src_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Dewarp_RGB_CL(lut_cols, lut_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Blends_RGB_CL(img_dst_cols, img_dst_rows);

     err = eys::fisheye360::GPU_Load_Programs_CL();

     err = eys::fisheye360::GPU_Writes_Buffer_LUT_CL(lut_cols, lut_rows, map_cols, map_rows);
     err = eys::fisheye360::GPU_Writes_Buffer_Img_Source_RGB_CL(img_src_cols, img_src_rows, img_src);
     err = eys::fisheye360::GPU_Run_Remap_Bilinear_RGB2RGB_CL(img_src_cols, img_src_rows, lut_cols, lut_rows, img_dst);
     err = eys::fisheye360::GPU_Run_Alpha_Blending_RGB_CL(lut_cols, lut_rows, overlay_LR, overlay_RL, img_dst2);
	 err = eys::fisheye360::GPU_Close_Platform_CL();

     int key;
     eys::img_io::Img_Show_CV(lut_cols, lut_rows, img_dst, "img", 0.5f, key);
     eys::img_io::Img_Show_CV(img_dst_cols, img_dst_rows, img_dst2, "img", 0.5f, key);
  }

  // EXAMPLE 2:
  int main(void)
  {
     // Produce LUT file
     eys::fisheye360::ParaLUT str_para;

     str_para.FOV                  = 203.0;
     str_para.semi_FOV_pixels      = 520;
     str_para.img_src_cols         = 1280;
     str_para.img_src_rows         = 960;
     str_para.img_L_src_col_center = 640.0;
     str_para.img_L_src_row_center = 480.0;
     str_para.img_R_src_col_center = 640.0;
     str_para.img_R_src_row_center = 480.0;
     str_para.img_L_rotation       = 0.0;
     str_para.img_R_rotation       = 0.0;
     str_para.spline_control_v1    = 0.0;
     str_para.spline_control_v2    = 0.0;
     str_para.spline_control_v3    = 0.0;
     str_para.spline_control_v4    = 0.0;
     str_para.spline_control_v5    = 0.0;
     str_para.spline_control_v6    = 0.0;
     str_para.spline_control_v7    = 0.0;
     str_para.img_dst_cols         = 980;
     str_para.img_dst_rows         = 920;
     str_para.img_L_dst_shift      = 0;
     str_para.img_R_dst_shift      = 0;

     str_para.unit_sphere_radius   = 212.429;
     str_para.min_col              = -461;
     str_para.max_col              = 1383;
     str_para.min_row              = 0;
     str_para.max_row              = 922;

     eys::fisheye360::Map_LUT_CC(str_para, "Fisheye360LUT_GPU.lut");

     // RUN GPU
     int err;
     int platform_total;

     int img_src_cols = 1280 * 2;
     int img_src_rows = 960;
     int lut_cols     = 980 * 2;
     int lut_rows     = 920;
     int overlay_LR   = 40;
     int overlay_RL   = 38;
     int img_dst_cols = 980 * 2 - overlay_LR - overlay_RL;
     int img_dst_rows = 920;

     char *platform_name = new char[100];
     char *device_name   = new char[100];


     unsigned char *lut      = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char *img_src  = new unsigned char[img_src_cols*img_src_rows * 3];
     unsigned char *img_dst  = new unsigned char[lut_cols*lut_rows * 3];
     unsigned char *img_dst2 = new unsigned char[img_dst_cols*img_dst_rows * 3];

	 err = eys::img_io::Img_Read_CV(img_src_cols, img_src_rows, "GPUimg.jpg", img_src);
     err = eys::fisheye360::Load_LUT_CC(lut_cols, lut_rows, "Fisheye360LUT_GPU.lut", lut);

     err = eys::fisheye360::GPU_Get_Platform_Number_CL(platform_total);
     err = eys::fisheye360::GPU_Set_Platform_CL(0, platform_name, device_name);

     std::cout << platform_name << std::endl;
     std::cout << device_name << std::endl;

     err = eys::fisheye360::GPU_Create_Buffer_LUT_CL(lut_cols, lut_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Source_RGB_CL(img_src_cols, img_src_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Dewarp_RGB_CL(lut_cols, lut_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Blends_RGB_CL(img_dst_cols, img_dst_rows);

     err = eys::fisheye360::GPU_Load_Programs_CL();

     err = eys::fisheye360::GPU_Writes_Buffer_LUT_CL(lut_cols, lut_rows, lut);
     err = eys::fisheye360::GPU_Writes_Buffer_Img_Source_RGB_CL(img_src_cols, img_src_rows, img_src);
     err = eys::fisheye360::GPU_Run_Remap_Bilinear_RGB2RGB_CL(img_src_cols, img_src_rows, lut_cols, lut_rows, img_dst);
     err = eys::fisheye360::GPU_Run_Alpha_Blending_RGB_CL(lut_cols, lut_rows, overlay_LR, overlay_RL, img_dst2);
	 err = eys::fisheye360::GPU_Close_Platform_CL();

     int key;
     eys::img_io::Img_Show_CV(lut_cols, lut_rows, img_dst, "img", 0.5f, key);
     eys::img_io::Img_Show_CV(img_dst_cols, img_dst_rows, img_dst2, "img", 0.5f, key);
  }

  // EXAMPLE 3:
  int main(void)
  {
     // Produce LUT file
     eys::fisheye360::ParaLUT str_para;

     str_para.FOV                  = 203.0;
     str_para.semi_FOV_pixels      = 520;
     str_para.img_src_cols         = 1280;
     str_para.img_src_rows         = 960;
     str_para.img_L_src_col_center = 640.0;
     str_para.img_L_src_row_center = 480.0;
     str_para.img_R_src_col_center = 640.0;
     str_para.img_R_src_row_center = 480.0;
     str_para.img_L_rotation       = 0.0;
     str_para.img_R_rotation       = 0.0;
     str_para.spline_control_v1    = 0.0;
     str_para.spline_control_v2    = 0.0;
     str_para.spline_control_v3    = 0.0;
     str_para.spline_control_v4    = 0.0;
     str_para.spline_control_v5    = 0.0;
     str_para.spline_control_v6    = 0.0;
     str_para.spline_control_v7    = 0.0;
     str_para.img_dst_cols         = 980;
     str_para.img_dst_rows         = 920;
     str_para.img_L_dst_shift      = 0;
     str_para.img_R_dst_shift      = 0;

     str_para.unit_sphere_radius   = 212.429;
     str_para.min_col              = -461;
     str_para.max_col              = 1383;
     str_para.min_row              = 0;
     str_para.max_row              = 922;

     eys::fisheye360::Map_LUT_CC(str_para, "Fisheye360LUT_GPU.lut", nullptr, nullptr, eys::LUT_LXLYRXRY_16_3);

     // RUN GPU
     int err;
     int platform_total;

     int img_src_cols = 1280 * 2;
     int img_src_rows = 960;
     int lut_cols     = 980 * 2;
     int lut_rows     = 920;
     int overlay_LR   = 40;
     int overlay_RL   = 38;
     int img_dst_cols = 980 * 2 - overlay_LR - overlay_RL;
     int img_dst_rows = 920;

     char *platform_name = new char[100];
     char *device_name   = new char[100];


     unsigned char *lut     = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char *img_src = new unsigned char[img_src_cols*img_src_rows * 3];
     unsigned char *img_dst = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char *img_dst2 = new unsigned char[img_dst_cols*img_dst_rows * 4];

     err = eys::img_io::Img_Read_CV(img_src_cols, img_src_rows, "GPUimg.jpg", img_src);
     err = eys::fisheye360::Load_LUT_CC(lut_cols, lut_rows, "Fisheye360LUT_GPU.lut", lut);

     err = eys::fisheye360::GPU_Get_Platform_Number_CL(platform_total);
     err = eys::fisheye360::GPU_Set_Platform_CL(0, platform_name, device_name);

     std::cout << platform_name << std::endl;
     std::cout << device_name << std::endl;

     err = eys::fisheye360::GPU_Create_Buffer_LUT_CL(lut_cols, lut_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Source_RGB_CL(img_src_cols, img_src_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Dewarp_RGBA_CL(lut_cols, lut_rows);
     err = eys::fisheye360::GPU_Create_Buffer_Img_Blends_RGBA_CL(img_dst_cols, img_dst_rows);

     err = eys::fisheye360::GPU_Load_Programs_CL();

     err = eys::fisheye360::GPU_Writes_Buffer_LUT_CL(lut_cols, lut_rows, lut);
     err = eys::fisheye360::GPU_Writes_Buffer_Img_Source_RGB_CL(img_src_cols, img_src_rows, img_src);

     err = eys::fisheye360::GPU_Run_Remap_Bilinear_RGB2RGBA_CL(img_src_cols, img_src_rows, lut_cols, lut_rows, img_dst);
     err = eys::fisheye360::GPU_Run_Alpha_Blending_RGBA_CL(lut_cols, lut_rows, overlay_LR, overlay_RL, img_dst2);
     err = eys::fisheye360::GPU_Close_Platform_CL();
  
     int key;
     eys::img_io::Img_Show_CV(lut_cols, lut_rows, img_dst, "img", 0.5f, key, eys::IMG_COLOR_BGRA);
     eys::img_io::Img_Show_CV(img_dst_cols, img_dst_rows, img_dst2, "img", 0.5f, key, eys::IMG_COLOR_BGRA);
  }
  * @endcode
  */
EYS_EXPORTS int GPU_Close_Platform_CL();

/**
  * @} fisheye360_api_cl
  */

} // fisheye360
} // eys

#endif // __EYS_FISHEYE360_API_CL_H__