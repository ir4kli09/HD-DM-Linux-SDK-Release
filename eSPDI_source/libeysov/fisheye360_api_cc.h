/**
  * @file  fisheye360_api_cc.h
  * @title API for Fisheye360 - C++ only version
  * @version 2.1.2
  */
#ifndef __EYS_FISHEYE360_API_CC_H__
#define __EYS_FISHEYE360_API_CC_H__

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

#ifdef __linux__
#define EYS_EXPORTS
#include "fisheye360_def.h"
#include "eys_def.h"
/*
#ifdef __ANDROID__
#include "utilbase.h"
#endif
*/
#endif

/**
  * @defgroup fisheye360 FishEye360
  * Fisheye360 Project
  */

/**
  * @defgroup fisheye360_api_cc FishEye360 API C++ Version
  * @ingroup fisheye360
  * API for Fisheye360 - C++ only version
  */

namespace eys
{
namespace fisheye360
{

/**
  * @addtogroup fisheye360_api_cc
  * @{
  */

/**
  * @brief  Load fisheye360 dewarping LUT table
  * @anchor Load_LUT_CC
  * @param[in]  cols           Table width in pixel
  * @param[in]  rows           Table height in pixel
  * @param[in]  filename       Path and filename of table
  * @param[out] lut            uchar LUT talbe with size [cols x rows x 4], 2 byte for x and 2 byte for y
  * @param[in]  size_user_data The size of user data in byte above lut buffer, it work when turn_on_64bit is ture
  * @param[in]  turn_on_64_bit When the value is "true"  turn on insert  64bit user data above the lut buffer, "false" is disable 
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 File open error
  *         \li \c -2 LUT buffer error
  *
  */
EYS_EXPORTS int Load_LUT_CC(
	const int cols, 
	const int rows,
	const char *filename,
	unsigned char *lut,
	int size_user_data = 0,
	bool turn_on_64bit = false
	);

/**
  * @brief  Generate LUT table
  * @anchor Map_LUT_CC
  * @param[in]  str_para                Parameters for LUT @ref eys::fisheye360::ParaLUT
  * @param[in]  filename                Path file name of LUT table
  * @param[out] map_lut_cols            Output LUT for cols
  * @param[out] map_lut_rows            Output LUT for rows
  * @param[in]  lut_type                Output type of look-up table, @ref eys::LutModes
  * @param[in]  turn_on_img_resolution  Turn-on function of out_img_resolution in LUT table @ref eys::fisheye360::ParaLUT. "true" is turn-on and "false" is disable
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Create file error
  *         \li \c -2 Overlay value error with resize
  *
  * @code
  int main(void)
  {
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
     str_para.img_dst_cols         = 1000;
     str_para.img_dst_rows         = 960;
     str_para.img_L_dst_shift      = 0;
     str_para.img_R_dst_shift      = 0;

     double unit_sphere_radius;
     double min_col;
     double max_col;
     double min_row;
     double max_row;

     eys::fisheye360::Dewarp_GetXY_Parameters_CC(
		str_para.FOV,
		str_para.semi_FOV_pixels,
		unit_sphere_radius,
		min_col,
		max_col,
		min_row,
		max_row
		);

     str_para.unit_sphere_radius = unit_sphere_radius;
     str_para.min_col            = min_col;
     str_para.max_col            = max_col;
     str_para.min_row            = min_row;
     str_para.max_row            = max_row;

     eys::fisheye360::Map_LUT_CC(str_para, "FishEye.lut", nullptr, nullptr, eys::LUT_LXLYRXRY_16_3);
     return 1;
  }
  * @endcode
  *
  */
EYS_EXPORTS int Map_LUT_CC(
	eys::fisheye360::ParaLUT &str_para,
	const char *filename,
	float *map_lut_cols = nullptr,
	float *map_lut_rows = nullptr,
	int lut_type = eys::LUT_LXLYRXRY_16_3,
	bool turn_on_img_resolution = false
	);

/**
  * @brief  Resize LUT table to 1080p (1920 x 1080)
  * @anchor Resize_LUT_1080P_CC
  * @param[in]  lut_cols_src     Source LUT width in pixel
  * @param[in]  lut_rows_src     Source LUT height in pixel
  * @param[in]  lut_cols_dst     Destination LUT width in pixel
  * @param[in]  lut_rows_dst     Destination LUT height in pixel
  * @param[in]  overlay_LR_src   Source overlay value at L/R
  * @param[in]  overlay_RL_src   Source overlay value at R/L
  * @param[in]  map_src_cols     Source LUT x position buffer with size of [lut_cols_src x lut_rows_src] in float type
  * @param[in]  map_src_rows     Source LUT y position buffer with size of [lut_cols_src x lut_rows_src] in float type
  * @param[out] lut_cols_scale   Output the width of LUT after scaling, ie. effectiv pixels in lut_cols_dst
  * @param[out] lut_rows_scale   Output the height of LUT after scaling, ie. effective pixels in lut_rows_dst
  * @param[out] overlay_LR_dst   Output the overlay value at L/R after scaling
  * @param[out] overlay_RL_dst   Output the overlay value at R/L after scaling
  * @param[out] map_dst_cols     Output LUT x position buffer with size of [lut_cols_dst x lut_rows_dst] in float type
  * @param[out] map_dst_rows     Output LUT y position buffer with size of [lut_cols_dst x lut_rows_dst] in float type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Input buffer error
  *
  * @code
  void main()
  {
     float *map_src_cols = new float[lut_para.img_dst_cols * 2 * lut_para.img_dst_rows];
     float *map_src_rows = new float[lut_para.img_dst_cols * 2 * lut_para.img_dst_rows];

     float *map_dst_cols = new float[2048 * 1080];
     float *map_dst_rows = new float[2048 * 1080];

     unsigned char *img_dst0 = new unsigned char[lut_para.img_dst_cols * 2 * lut_para.img_dst_rows * 3];
     unsigned char *img_dst1 = new unsigned char[lut_para.img_stream_cols*lut_para.img_stream_rows * 3];
     unsigned char *img_dst2 = new unsigned char[2048 * 1080 * 3];
     unsigned char *img_dst3 = new unsigned char[1920 * 1080 * 3];

	 eys::fisheye360::Map_LUT_CC(lut_para, nullptr, map_src_cols, map_src_rows);

	 int lut_cols_scale;
	 int lut_rows_scale;
	 int overlay_LR_dst;
	 int overlay_RL_dst;

	 eys::fisheye360::Resize_LUT_1080P_CC(
		lut_para.img_dst_cols * 2,
		lut_para.img_dst_rows,
		2048,
		1080,
		14,
		11,
		map_src_cols,
		map_src_rows,
		lut_cols_scale,
		lut_rows_scale,
		overlay_LR_dst,
		overlay_RL_dst,
		map_dst_cols,
		map_dst_rows
		);

	 std::cout << lut_cols_scale << std::endl;
	 std::cout << lut_rows_scale << std::endl;
	 std::cout << overlay_LR_dst << std::endl;
	 std::cout << overlay_RL_dst << std::endl;

	 eys::fisheye360::Remap_Bilinear_RGB2RGB_CV(
		lut_para.img_src_cols * 2,
		lut_para.img_src_rows,
		2048,
		1080,
		map_dst_cols,
		map_dst_rows,
		img_src, 
		img_dst2
		);

     eys::img_io::Img_Show_CV(2048, 1080, img_dst2, "Image", 0.8f, key, eys::IMG_COLOR_BGR);
  }
  * @endcode
  */
EYS_EXPORTS int Resize_LUT_1080P_CC(
	const int lut_cols_src,
	const int lut_rows_src,
	const int lut_cols_dst,
	const int lut_rows_dst,
	const int overlay_LR_src,
	const int overlay_RL_src,
	float *map_src_cols,
	float *map_src_rows,
	int &lut_cols_scale,
	int &lut_rows_scale,
	int &overlay_LR_dst,
	int &overlay_RL_dst,
	float *map_dst_cols,
	float *map_dst_rows
	);

/**
  * @brief  Resize LUT table to 1440p (2560 x 1440)
  * @anchor Resize_LUT_1440P_CC
  * @param[in]  lut_cols_src     Source LUT width in pixel
  * @param[in]  lut_rows_src     Source LUT height in pixel
  * @param[in]  lut_cols_dst     Destination LUT width in pixel
  * @param[in]  lut_rows_dst     Destination LUT height in pixel
  * @param[in]  overlay_LR_src   Source overlay value at L/R
  * @param[in]  overlay_RL_src   Source overlay value at R/L
  * @param[in]  map_src_cols     Source LUT x position buffer with size of [lut_cols_src x lut_rows_src] in float type
  * @param[in]  map_src_rows     Source LUT y position buffer with size of [lut_cols_src x lut_rows_src] in float type
  * @param[out] lut_cols_scale   Output the width of LUT after scaling, ie. effectiv pixels in lut_cols_dst
  * @param[out] lut_rows_scale   Output the height of LUT after scaling, ie. effective pixels in lut_rows_dst
  * @param[out] overlay_LR_dst   Output the overlay value at L/R after scaling
  * @param[out] overlay_RL_dst   Output the overlay value at R/L after scaling
  * @param[out] map_dst_cols     Output LUT x position buffer with size of [lut_cols_dst x lut_rows_dst] in float type
  * @param[out] map_dst_rows     Output LUT y position buffer with size of [lut_cols_dst x lut_rows_dst] in float type
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Input buffer error
  *
  */
EYS_EXPORTS int Resize_LUT_1440P_CC(
	const int lut_cols_src,
	const int lut_rows_src,
	const int lut_cols_dst,
	const int lut_rows_dst,
	const int overlay_LR_src,
	const int overlay_RL_src,
	float *map_src_cols,
	float *map_src_rows,
	int &lut_cols_scale,
	int &lut_rows_scale,
	int &overlay_LR_dst,
	int &overlay_RL_dst,
	float *map_dst_cols,
	float *map_dst_rows
	);

/**
  * @brief  Trasfer Fisheye360 bin buffer to parameters with version 5.0
  * @anchor Buffer2Para_Bin_CC
  * @param[in]  buffer_flash Buffer read from flash
  * @param[out] str_para     Parameters for LUT @ref eys::fisheye360::ParaLUT
  * @param[in]  show_data    true for show data, false for disable
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Input buffer error
  *         \li \c -2 File ID 200 version error
  *         \li \c -3 File ID 200 header error
  *
  */
EYS_EXPORTS int Buffer2Para_Bin_CC(
	unsigned char *buffer_flash,
	eys::fisheye360::ParaLUT &str_para,
	bool show_data = false
	);

/**
  * @brief  Trasfer parameters of Fisheye360 to bin buffer with version 5.0
  * @anchor Para2Buffer_Bin_CC
  * @param[in]  str_para     Parameters for LUT @ref eys::fisheye360::ParaLUT
  * @param[out] buffer_flash Buffer for write to flash
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 File ID 200 version error
  *
  */
EYS_EXPORTS int Para2Buffer_Bin_CC(eys::fisheye360::ParaLUT &str_para, unsigned char *buffer_flash);

/**
  * @brief  Remapping input RGB/BGR image by LUT table and output uchar RGB/BGR image buffer
  * @anchor Remap_Bilinear_RGB2RGB_CC
  * @param[in]  cols_in   Image width of img_src
  * @param[in]  rows_in   Image height of img_src
  * @param[in]  cols_out  Image width of img_dst
  * @param[in]  rows_out  Image height of img_dst
  * @param[in]  lut       LUT table used to remap img_src to img_dst and the size is cols_out x rows_out
  * @param[in]  img_src   Input image RGB/BGR uchar buffer [cols_in x rows_in x 3]
  * @param[out] img_dst   Output image RGB/BGR uchar buffer [cols_out x rows_out x 3]
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source buffer error
  *
  */
EYS_EXPORTS int Remap_Bilinear_RGB2RGB_CC(
	const int cols_in,
	const int rows_in,
	const int cols_out,
	const int rows_out,
	const unsigned char *lut,
	unsigned char *img_src,
	unsigned char *img_dst
	);

/**
  * @brief  Remapping input RGB/BGR image by LUT table and output uchar RGBA/BGRA image buffer
  * @anchor Remap_Bilinear_RGB2RGBA_CC
  * @param[in]  cols_in   Image width of img_src
  * @param[in]  rows_in   Image height of img_src
  * @param[in]  cols_out  Image width of img_dst
  * @param[in]  rows_out  Image height of img_dst
  * @param[in]  lut       LUT table used to remap img_src to img_dst and the size is cols_out x rows_out
  * @param[in]  img_src   Input image RGB/BGR uchar buffer [cols_in x rows_in x 3]
  * @param[out] img_dst   Output image RGBA/BGRA uchar buffer [cols_out x rows_out x 4]
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source buffer error
  *         \li \c -2 Dimension error. Output dimension bigger than input dimension
  *
  */
EYS_EXPORTS int Remap_Bilinear_RGB2RGBA_CC(
	const int cols_in,
	const int rows_in,
	const int cols_out,
	const int rows_out,
	const unsigned char *lut,
	unsigned char *img_src,
	unsigned char *img_dst
	);

/**
  * @brief  Remapping input image by LUT table and output uchar image buffer
  * @anchor Remap_Bilinear_YUV2YUV_422_CC
  * @param[in]  cols_in   Image width of img_src
  * @param[in]  rows_in   Image height of img_src
  * @param[in]  cols_out  Image width of img_dst
  * @param[in]  rows_out  Image height of img_dst
  * @param[in]  lut       LUT table used to remap img_src to img_dst and the size is cols_out x rows_out
  * @param[in]  img_src   Input image YUV422 uchar buffer [cols_in x rows_in x 2]
  * @param[out] img_dst   Output image YUV422 uchar buffer [cols_out x rows_out x 2]
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source buffer error
  *         \li \c -2 Dimension error. Output dimension bigger than input dimension
  *
  * @code
  void main()
  {
     int lut_cols   = 2000;
	 int lut_rows   = 960;
	 int cols_in    = 2560;
	 int rows_in    = 960;
	 int cols_out   = 2000;
	 int rows_out   = 960;
	 int overlay_lr = 44;
	 int overlay_rl = 40;
	 int key;
	 int return_value;

	 unsigned char* psrc_yuy2 = new unsigned char[cols_in*rows_in * 2];
	 unsigned char* pdst_yuy2 = new unsigned char[cols_out*rows_out * 2];
	 unsigned char* plut = new unsigned char[lut_cols*lut_rows * 4];

	 return_value = eys::img_io::Img_Read_CV(cols_in, rows_in, "test.yuv", psrc_yuy2, eys::IMG_COLOR_YUY2);
	 return_value = eys::fisheye360::Load_LUT_CC(lut_cols, lut_rows, "Fisheye360LUT.lut", plut);
	 return_value = eys::fisheye360::Remap_Bilinear_YUV2YUV_422_CC(cols_in, rows_in, cols_out, rows_out, plut, psrc_yuy2, pdst_yuy2);
	 return_value = eys::img_io::Img_Show_CV(cols_out, rows_out, pdst_yuy2, "img", 0.5f, key, eys::IMG_COLOR_YUY2, 0);
  }
  * @endcode
  *
  */
EYS_EXPORTS int Remap_Bilinear_YUV2YUV_422_CC(
	const int cols_in,
	const int rows_in,
	const int cols_out,
	const int rows_out,
	const unsigned char *lut,
	unsigned char *img_src,
	unsigned char *img_dst
	);

/**
  * @brief  Blending image L/R boundary by using alpha-blending method (RGB)
  * @anchor Alpha_Blending_RGB_CC
  * @param[in]  cols        Side-by-side image width 
  * @param[in]  rows        Image height
  * @param[in]  overlay_lr  Overlay pixels at L/R boundary
  * @param[in]  overlay_rl  Overlay pixels at R/L boundary
  * @param[in]  img_src     Input image RGB/BGR uchar buffer [cols_in x rows_in x 3]
  * @param[out] img_dst     Output image RGB/BGR uchar buffer [cols_out x rows_out x 3]
  * @param[in]  cols_scale  Enable when value bigger than 0, it is the working range in cols, and overlay value is according to it
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source buffer error
  *         \li \c -2 cols_scale value error
  *
  * @code
  void main()
  {
     int lut_cols   = 2000;
	 int lut_rows   = 960;
	 int cols_in    = 2560;
	 int rows_in    = 960;
	 int cols_out   = 2000;
	 int rows_out   = 960;
	 int overlay_lr = 44;
	 int overlay_rl = 40;
	 int key;
	 int return_value;

	 unsigned char* plut  = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char* psrc  = new unsigned char[cols_in*rows_in * 3];
     unsigned char* pdst0 = new unsigned char[cols_out*rows_out * 3];
     unsigned char* pdst1 = new unsigned char[(cols_out*rows_out - overlay_lr - overlay_rl) * 3];

     return_value = eys::img_io::Img_Read_CV(cols_in, rows_in, "imgLR2.bmp", psrc);
	 return_value = eys::fisheye360::Load_LUT_CC(lut_cols, lut_rows, "FishEye360.lut", plut);
	 return_value = eys::fisheye360::Remap_Bilinear_RGB2RGB_CC(cols_in, rows_in, cols_out, rows_out, plut, psrc, pdst0);
	 return_value = eys::fisheye360::Alpha_Blending_RGB_CC(cols_out, rows_out, overlay_lr, overlay_rl, pdst0, pdst1);

     eys::img_io::Img_Show_CV(cols_out-overlay_lr-overlay_rl, rows_out, pdst1, "img", 0.5f, key);
     system("pause");
  }
  * @endcode
  *
  */
EYS_EXPORTS int Alpha_Blending_RGB_CC(
	const int cols,
	const int rows,
	const int overlay_lr,
	const int overlay_rl,
	unsigned char *img_src,
	unsigned char *img_dst,
	const int cols_scale = 0
	);

/**
  * @brief  Blending image L/R boundary by using alpha-blending method (RGBA)
  * @anchor Alpha_Blending_RGBA_CC
  * @param[in]  cols        Side-by-side image width
  * @param[in]  rows        Image height
  * @param[in]  overlay_lr  Overlay pixels at L/R boundary
  * @param[in]  overlay_rl  Overlay pixels at R/L boundary
  * @param[in]  img_src     Input image RGBA/BGRA uchar buffer [cols_in x rows_in x 4]
  * @param[out] img_dst     Output image RGBA/BGRA uchar buffer [cols_out x rows_out x 4]
  * @param[in]  cols_scale  Enable when value bigger than 0, it is the working range in cols, and overlay value is according to it
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source buffer error
  *
  * @code
  void main()
  {
     int lut_cols   = 980 * 2;
     int lut_rows   = 920;
     int cols_in    = 2560;
     int rows_in    = 960;
     int overlay_lr = 40;
     int overlay_rl = 40;
     int cols_out   = 980 * 2 - overlay_lr - overlay_rl;
     int rows_out   = 920;
     int key;
     int err;

     unsigned char* plut           = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char* psrc_rgb       = new unsigned char[cols_in*rows_in * 3];
     unsigned char* psrc_rgba      = new unsigned char[cols_in*rows_in * 4];
     unsigned char* pdst_dwp_rgba  = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char* pdst_bld_rgba  = new unsigned char[cols_out*rows_out * 4];

     err = eys::img_io::Img_Read_CV(cols_in, rows_in, "cali16.jpg", psrc_rgb);
     eys::img_format::BGR_To_BGRA_CV(cols_in, rows_in, psrc_rgb, psrc_rgba);

     err = eys::fisheye360::Load_LUT_CC(lut_cols, lut_rows, "Fisheye360LUT_GPU.lut", plut);
     err = eys::fisheye360::Remap_Bilinear_RGB2RGBA_CC(cols_in, rows_in, lut_cols, lut_rows, plut, psrc_rgb, pdst_dwp_rgba);
     err = eys::fisheye360::Alpha_Blending_RGBA_CC(lut_cols, lut_rows, overlay_lr, overlay_rl, pdst_dwp_rgba, pdst_bld_rgba);
     eys::img_io::Img_Show_CV(cols_out, rows_out, pdst_bld_rgba, "img", 0.5f, key, eys::IMG_COLOR_BGRA);
  }
  * @endcode
  *
  */
EYS_EXPORTS int Alpha_Blending_RGBA_CC(
	const int cols,
	const int rows,
	const int overlay_lr,
	const int overlay_rl,
	unsigned char *img_src,
	unsigned char *img_dst,
	const int cols_scale = 0
	);

/**
  * @brief  Blending image L/R boundary by using alpha-blending method (YUV422)
  * @anchor Alpha_Blending_YUV_422_CC
  * @param[in]  cols        Image width
  * @param[in]  rows        Image height
  * @param[in]  overlay_lr  Overlay pixels at L/R boundary
  * @param[in]  overlay_rl  Overlay pixels at R/L boundary
  * @param[in]  img_src     Input image YUV422 uchar buffer [cols_in x rows_in x 2]
  * @param[out] img_dst     Output image YUV422 uchar buffer [cols_out x rows_out x 2]
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source buffer error
  *         \li \c -2 Overlay value must be even for yuv422
  *
  * @code
  void main()
  {
     int lut_cols   = 2000;
     int lut_rows   = 960;
     int cols_in    = 2560;
     int rows_in    = 960;
     int cols_out   = 2000;
     int rows_out   = 960;
     int overlay_lr = 44;
     int overlay_rl = 40;
     int key;
     int return_value;

     unsigned char* plut  = new unsigned char[lut_cols*lut_rows * 4];
     unsigned char* psrc  = new unsigned char[cols_in*rows_in * 2];
     unsigned char* pdst0 = new unsigned char[cols_out*rows_out * 2];
     unsigned char* pdst1 = new unsigned char[(cols_out*rows_out - overlay_lr - overlay_rl) * 2];

     return_value = eys::img_io::Img_Read_CV(cols_in, rows_in, "imgLR2.bmp", psrc);
     return_value = eys::fisheye360::Load_LUT_CC(lut_cols, lut_rows, "FishEye360.lut", plut);
     return_value = eys::fisheye360::Remap_Bilinear_YUV_422_CC(cols_in, rows_in, cols_out, rows_out, plut, psrc, pdst0);
     return_value = eys::fisheye360::Alpha_Blending_YUV_422_CC(cols_out, rows_out, overlay_lr, overlay_rl, pdst0, pdst1);

     eys::img_io::Img_Show_CV(cols_out-overlay_lr-overlay_rl, rows_out, pdst1, "img", 0.5f, key);
  }
  * @endcode
  *
  */
EYS_EXPORTS int Alpha_Blending_YUV_422_CC(
	const int cols,
	const int rows,
	const int overlay_lr,
	const int overlay_rl,
	unsigned char *img_src,
	unsigned char *img_dst
	);

/**
  * @brief  Get parameters for Dewarp_GetXY
  * @anchor Dewarp_GetXY_Parameters_CC
  * @param[in]  FOV                   Field of view with degree
  * @param[in]  semi_FOV_pixels       Pixels for half FOV, 2 x semi_FOV_pixels = image width = image height
  * @param[in]  lens_projeciton_type  Lens projection type, @ref eys::LensProjectionModes
  * @param[out] spherical_radius      Unit spherical radius
  * @param[out] min_col               Start width of dewarping image
  * @param[out] max_col               Ended width of dewarping image
  * @param[out] min_row               Start height of dewarping image
  * @param[out] max_row               Ended width of dewarping image
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Results are not correct
  *         \li \c -2 Lens projection type is not correct
  *
  * @code
  void main()
  {
     double FOV           = 210.0;
     int    radius_pixels = 510;
	
     double spherical_radius;
     double min_col;
     double max_col;
     double min_row;
     double max_row;

     eys::fisheye360::Dewarp_GetXY_Parameters_CV(
		FOV, 
		radius_pixels,
		eys::LENS_PROJ_STEREOGRAPHIC,
		spherical_radius,
		min_col, 
		max_col, 
		min_row, 
		max_row
		);
  }
  * @endcode
  *
  */
EYS_EXPORTS int Dewarp_GetXY_Parameters_CC(
	const double FOV,
	const int semi_FOV_pixels,
	const int lens_projection_type,
	double &spherical_radius,
	double &min_col,
	double &max_col,
	double &min_row,
	double &max_row
	);

/**
  * @brief  Get position of src_col and src_row by dewarping from dst_col and dst_row
  * @anchor Dewarp_GetXY_CC
  * @param[in]  dst_col               Position of image width from destination image
  * @param[in]  dst_row               Position of image height from destination image
  * @param[in]  FOV                   Field of view with degree
  * @param[in]  semi_FOV_pixels       Pixels for half FOV, 2 x semi_FOV_pixels = image width = image height
  * @param[in]  lens_projeciton_type  Lens projection type, @ref eys::LensProjectionModes
  * @param[in]  spherical_radius      Unit spherical radius
  * @param[in]  min_col               Start width of dewarping image
  * @param[in]  max_col               Ended width of dewarping image
  * @param[in]  min_row               Start height of dewarping image
  * @param[in]  max_row               Ended width of dewarping image
  * @param[out] src_col               Position of image width from source image
  * @param[out] src_row               Position of image height from source image
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Lens projection type is not correct
  *
  * @code
  void main()
  {
     double src_col;
     double src_row;

     eys::fisheye360::Dewarp_GetXY_CC(
		100, 
		100, 
		FOV, 
		radius_pixels, 
		eys::LENS_PROJ_STEREOGRAPHIC,
		spherical_radius,
		min_col, 
		max_col, 
		min_row,
		max_row, 
		src_col,
		src_row
		);
  }
  * @endcode
  *
  */
EYS_EXPORTS int Dewarp_GetXY_CC(
	const double dst_col,
	const double dst_row,
	const double FOV,
	const int semi_FOV_pixels,
	const int lens_projection_type,
	const double spherical_radius,
	const double min_col,
	const double max_col,
	const double min_row,
	const double max_row,
	double &src_col,
	double &src_row
	);

/**
  * @brief  Flips a 2D image array around vertical
  * @anchor Img_Flip_CC
  * @param[in]  img_cols  Image width in pixel
  * @param[in]  img_rows  Image height in pixel
  * @param[in]  img_src   RGB/BGR image source
  * @param[out] img_dst   Destination image
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 Source data error
  *         \li \c -2 Destination data error
  *
  */
EYS_EXPORTS int Img_Flip_CC(
	const int img_cols,
	const int img_rows,
	unsigned char *img_src,
	unsigned char *img_dst
	);

/**
  * @brief  Transfer location from theoretical to measurement
  * @anchor Lens_Distortion_Fix_CC
  * @param[in]  src_col    Theoretical location in col
  * @param[in]  src_row    Theoretical location in row
  * @param[in]  img_cols   Image width in pixel
  * @param[in]  img_rows   Image height in pixel
  * @param[in]  para       Parameters for fix curvature. y = v6*x^6 + v5*x^5 + v4*x^4 + v3*x^3 + v2*x^2 + v1*x + v0 
  * @param[in]  pixel_size Sensor pixel size in um
  * @param[out] dst_col    Measurement location in col
  * @param[out] dst_row    Measurement location in row
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  * @code
  void main()
  {
     int err;
     int img_cols, img_rows;
     double src_col, src_row;
     double dst_col, dst_row;
     double pixel_size;
     double v0, v1, v2, v3, v4, v5, v6;
     double paras[7];

     img_cols   = 1024;
     img_rows   = 960;
     pixel_size = 3.75;

     v0 = -0.0001582190;
     v1 = +1.0052370879;
     v2 = -0.0450350496;
     v3 = +0.0820410723;
     v4 = -0.0249700953;
     v5 = -0.0081901419;
     v6 = +0.0015452958;

     paras[0] = v0;
     paras[1] = v1;
     paras[2] = v2;
     paras[3] = v3;
     paras[4] = v4;
     paras[5] = v5;
     paras[6] = v6;

     src_col = 512 + 342.06;
     src_row = 480 + 342.06;

     err = eys::fisheye360::Lens_Distortion_Fix_CC(
             src_col,
             src_row,
			 img_cols,
			 img_rows,
             paras,
             pixel_size,
             dst_col,
             dst_row
             );

     std::cout << dst_col << std::endl;
     std::cout << dst_row << std::endl;
  }
  * @endcode
  */
EYS_EXPORTS int Lens_Distortion_Fix_CC(
	double src_col,
	double src_row,
	int img_cols,
	int img_rows,
	const double *para,
	const double pixel_size,
	double &dst_col,
	double &dst_row
	);

/**
  * @brief  Insert MetaData 360 to video
  * @anchor MetaData_360_Insert_CC
  * @param[in]  filename  Path and filename of table
  * @param[in]  audio_in  "true" for video with audio, "false" for without audio
  *
  * @return If the function succeeds, the return value is 1. If it fails,
  *         the following error values:
  *
  *         \li \c -1 "filename" error
  *         \li \c -2 Open file error
  *         \li \c -3 Tag "moov" can not be found
  *         \li \c -4 Tag "trak" can not be found
  *         \li \c -5 Tag "360" can not be found
  *
  */
EYS_EXPORTS int MetaData_360_Insert_CC_eYs3D(const char *filename, bool audio_in);

/**
  * @} fisheye360_api_cc
  */

} // fisheye360
} // eys

int Point_Barrel_Compress_CC(
	double src_col,
	double src_row,
	int mat_cols,
	int mat_rows,
	const double* yh,
	const double* pixel_omit,
	int num_control_point,
	double &dst_col,
	double &dst_row
	);

int Spline_1D_CC(const double* xa, const double* ya, const int n, double x, double &y);

int Interpolation_2D_Bilinear_CC(
	const int mat_src_cols,
	const int mat_src_rows,
	const int mat_dst_cols,
	const int mat_dst_rows,
	float *mat_src,
	float *mat_dst
	);

#endif // __EYS_FISHEYE360_API_CC_H__
