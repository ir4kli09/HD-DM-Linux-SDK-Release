/**
  * @file  fisheye360_api_cc.cpp
  * @title API for Fisheye360 - C++ only version
  */

/**
  * Include file
  */
#include "stdafx.h"
#include "fisheye360_api_cc.h"
#include <string>

#include "fisheye360_api_cc.h"
//#include "spatialmedia/parser.h"
//#include "spatialmedia/metadata_utils.h"
/**
  * Define operation system
  */
#ifdef WIN32
#include <iostream>
#include <iomanip>
#include <sys\stat.h>
#endif

#ifdef __linux__
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
//#ifndef __ANDROID__
#include <stdarg.h>
void ignoreLog(...)
{}
#define LOGI ignoreLog
#define LOGE ignoreLog
#define WORD unsigned short
//#endif
#endif
#define  __int64        long long


/**
  * Function declaration
  */
namespace eys
{
namespace fisheye360
{

int Load_LUT_CC(
	const int cols, 
	const int rows,
	const char *filename,
	unsigned char *lut,
	int size_user_data,
	bool turn_on_64bit
	)
{
	if (turn_on_64bit == true)
	{
		if (lut == nullptr) 
		{
			return -2;
		}

		FILE *pfile;
		pfile = fopen(filename, "rb+");

		if (!pfile)
		{
			return -1;
		}

		fseek(pfile, size_user_data, SEEK_CUR);
		fread(lut, sizeof(unsigned char), cols*rows * 4, pfile);
		fclose(pfile);
	}
	else
	{
		FILE *pfile;

		pfile = fopen(filename, "rb+");

		if (!pfile)
		{
			return -1;
		}

		if (lut == nullptr)
		{
			return -2;
		}

		fread(lut, sizeof(unsigned char), cols*rows * 4, pfile);
		fclose(pfile);
	}
	return 1;
}

int Map_LUT_CC(
	eys::fisheye360::ParaLUT &str_para,
	const char *filename,
	float *map_lut_cols,
	float *map_lut_rows,
	int lut_type,
	bool turn_on_img_resolution
	)
{
	if (turn_on_img_resolution == true)
	{
		if (str_para.out_img_resolution == eys::IMG_RESOLUTION_1080P ||
			str_para.out_img_resolution == eys::IMG_RESOLUTION_2560x1440
			)
		{
			// Set parameters
			double unit_sphere_radius;     // Unit spherical radius for dewarping get x and y
			double min_col, min_row;       // Parameters of min position image width and height
			double max_col, max_row;       // Parameters of max position image width and height
			int    img_src_FOV_cols;       // Source image width within FOV
			int    img_src_FOV_rows;       // Source image height within FOV
			double img_src_FOV_center_col; // Source image center width position within FOV
			double img_src_FOV_center_row; // Source image center height position within FOV
			int    img_dst_cols;           // Destination image width
			int    img_dst_rows;           // Destination image height

			unit_sphere_radius = str_para.unit_sphere_radius;
			min_col = str_para.min_col;
			max_col = str_para.max_col;
			min_row = str_para.min_row;
			max_row = str_para.max_row;

			img_src_FOV_cols = static_cast<int>(str_para.semi_FOV_pixels * 2); // L/R side is the same
			img_src_FOV_rows = static_cast<int>(str_para.semi_FOV_pixels * 2); // L/R side is the same

			img_src_FOV_center_col = img_src_FOV_cols / 2.0; // L/R side is the same
			img_src_FOV_center_row = img_src_FOV_rows / 2.0; // L/R side is the same

			img_dst_cols = static_cast<int>(max_col - min_col + 1);
			img_dst_rows = static_cast<int>(max_row - min_row + 1);

			// Set value to compress parameters 
			double div_rows       = img_dst_rows / (7 - 1);
			double control_point1 = div_rows * 0;
			double control_point2 = div_rows * 1;
			double control_point3 = div_rows * 2;
			double control_point4 = div_rows * 3;
			double control_point5 = div_rows * 4;
			double control_point6 = div_rows * 5;
			double control_point7 = div_rows * 6;

			double mat_compress_rows[] = {
				control_point1,
				control_point2,
				control_point3,
				control_point4,
				control_point5,
				control_point6,
				control_point7 
			};

			div_rows = img_dst_rows / (7 - 1);

			control_point1 = div_rows * 0;
			control_point2 = div_rows * 1;
			control_point3 = div_rows * 2;
			control_point4 = div_rows * 3;
			control_point5 = div_rows * 4;
			control_point6 = div_rows * 5;
			control_point7 = div_rows * 6;

			double mat_compress_values[] = {
				str_para.spline_control_v1,
				str_para.spline_control_v2,
				str_para.spline_control_v3,
				str_para.spline_control_v4,
				str_para.spline_control_v5,
				str_para.spline_control_v6,
				str_para.spline_control_v7 
			};

			// Set lens distortion parameters
			double lens_distoriton_para[] = {
				str_para.distortion_fix_v0,
				str_para.distortion_fix_v1,
				str_para.distortion_fix_v2,
				str_para.distortion_fix_v3,
				str_para.distortion_fix_v4,
				str_para.distortion_fix_v5,
				str_para.distortion_fix_v6
			};

			// Set map buffer and others
			float  *pmap_L_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_L_rows        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_rows        = new float[img_dst_cols*img_dst_rows];
			double *protation_L_matrix = new double[4];
			double *protation_R_matrix = new double[4];

			// Set variables for LUT calculation
			double point_dst_col, point_dst_row;
			double point_dst_compress_col, point_dst_compress_row;
			double point_src1_FOV_col, point_src1_FOV_row;
			double point_src2_FOV_col, point_src2_FOV_row;
			double point_src3_FOV_col, point_src3_FOV_row;
			double point_src4_col, point_src4_row;
			double point_dist_x, point_dist_y;

			// *************************************************** //
			// STEP : start local image calculation                //
			// *************************************************** //
			// Set rotation matrix
			protation_L_matrix[0] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[1] = -1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[2] = +1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[3] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);

			protation_R_matrix[0] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[1] = -1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[2] = +1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[3] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);

			// Local image calculation, the rows and cols are the same in L/R
			for (int i = 0; i < img_dst_rows; i++)
			{
				for (int j = 0; j < img_dst_cols; j++)
				{
					// Proc : compress
					if (str_para.spline_control_v1 == 0 &&
						str_para.spline_control_v2 == 0 &&
						str_para.spline_control_v3 == 0 &&
						str_para.spline_control_v4 == 0 &&
						str_para.spline_control_v5 == 0 &&
						str_para.spline_control_v6 == 0 &&
						str_para.spline_control_v7 == 0)
					{
						point_dst_col = j;
						point_dst_row = i;
					}
					else
					{
						Point_Barrel_Compress_CC(
							j,
							i,
							img_dst_cols,
							img_dst_rows,
							mat_compress_rows,
							mat_compress_values,
							7,
							point_dst_col,
							point_dst_row
							);
					}

					// Proc : get destination position from original position
					point_dst_compress_col = point_dst_col + min_col;
					point_dst_compress_row = point_dst_row + min_row;

					// Proc : dewarping
					eys::fisheye360::Dewarp_GetXY_CC(
						point_dst_compress_col,
						point_dst_compress_row,
						str_para.FOV,
						static_cast<int>(str_para.semi_FOV_pixels),
						static_cast<int>(str_para.lens_projection_type),
						unit_sphere_radius,
						min_col,
						max_col,
						min_row,
						max_row,
						point_src1_FOV_col,
						point_src1_FOV_row
						);

					if (point_src1_FOV_col < 0)
					{
						point_src1_FOV_col = 0;
					}
					else if (point_src1_FOV_col > img_src_FOV_cols - 1)
					{
						point_src1_FOV_col = img_src_FOV_cols - 1;
					}

					if (point_src1_FOV_row < 0)
					{
						point_src1_FOV_row = 0;
					}
					else if (point_src1_FOV_row > img_src_FOV_rows - 1)
					{
						point_src1_FOV_row = img_src_FOV_rows - 1;
					}

					// L side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_L_matrix[0] * point_dist_x +
							protation_L_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_L_matrix[2] * point_dist_x +
							protation_L_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						if (point_src3_FOV_col < 0)
						{
							point_src3_FOV_col = 0;
						}
						else if (point_src3_FOV_col > img_src_FOV_cols - 1)
						{
							point_src3_FOV_col = img_src_FOV_cols - 1;
						}

						if (point_src3_FOV_row < 0)
						{
							point_src3_FOV_row = 0;
						}
						else if (point_src3_FOV_row > img_src_FOV_rows - 1)
						{
							point_src3_FOV_row = img_src_FOV_rows - 1;
						}

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col =
							(point_src3_FOV_col - img_src_FOV_center_col) +
							str_para.img_L_src_col_center;

						point_src4_row =
							(point_src3_FOV_row - img_src_FOV_center_row) +
							str_para.img_L_src_row_center;

						if (fabs(point_src4_col - str_para.img_L_src_col_center) > str_para.semi_FOV_pixels)
						{
							point_src4_col = 0;
						}

						if (fabs(point_src4_row - str_para.img_L_src_row_center) > str_para.semi_FOV_pixels)
						{
							point_src4_row = 0;
						}

						pmap_L_cols[i*img_dst_cols + j] = static_cast<float>(point_src4_col);
						pmap_L_rows[i*img_dst_cols + j] = static_cast<float>(point_src4_row);
					}

					// R side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_R_matrix[0] * point_dist_x +
							protation_R_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_R_matrix[2] * point_dist_x +
							protation_R_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						if (point_src3_FOV_col < 0)
						{
							point_src3_FOV_col = 0;
						}
						else if (point_src3_FOV_col > img_src_FOV_cols - 1)
						{
							point_src3_FOV_col = img_src_FOV_cols - 1;
						}

						if (point_src3_FOV_row < 0)
						{
							point_src3_FOV_row = 0;
						}
						else if (point_src3_FOV_row > img_src_FOV_rows - 1)
						{
							point_src3_FOV_row = img_src_FOV_rows - 1;
						}

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col =
							(point_src3_FOV_col - img_src_FOV_center_col) +
							str_para.img_R_src_col_center;

						point_src4_row =
							(point_src3_FOV_row - img_src_FOV_center_row) +
							str_para.img_R_src_row_center;

						if (fabs(point_src4_col - str_para.img_R_src_col_center) > str_para.semi_FOV_pixels)
						{
							point_src4_col = 0;
						}

						if (fabs(point_src4_row - str_para.img_R_src_row_center) > str_para.semi_FOV_pixels)
						{
							point_src4_row = 0;
						}

						pmap_R_cols[i*img_dst_cols + j] =
							static_cast<float>(point_src4_col)
							+str_para.img_src_cols;

						pmap_R_rows[i*img_dst_cols + j] =
							static_cast<float>(point_src4_row);
					}
				}
			}

			// *************************************************** //
			// STEP : start L side global image calculation        //
			// *************************************************** //
			// Set parameters
			int img_local_cols = img_dst_cols;
			int img_local_rows = img_dst_rows;

			int img_global_cols = static_cast<int>(str_para.img_dst_cols);
			int img_global_rows = static_cast<int>(str_para.img_dst_rows);

			int img_global_center_col = static_cast<int>(str_para.img_dst_cols / 2);
			int img_global_center_row = static_cast<int>(str_para.img_dst_rows / 2 + str_para.img_L_dst_shift);

			int img_local_center_col = img_local_cols / 2;
			int img_local_center_row = img_local_rows / 2;

			int point_col, point_row;

			// Set map buffer
			float *pmap_L2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_L2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
						point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
						point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
						point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
						point_row = 0;
					}

					pmap_L2_cols[i*img_global_cols + j] =
						pmap_L_cols[point_row*img_local_cols + point_col];

					pmap_L2_rows[i*img_global_cols + j] =
						pmap_L_rows[point_row*img_local_cols + point_col];
				}
			}

			// *************************************************** //
			// STEP : start R side global image calculation        //
			// *************************************************** //
			// Set parameters
			img_local_cols = img_dst_cols;
			img_local_rows = img_dst_rows;

			img_global_cols = static_cast<int>(str_para.img_dst_cols);
			img_global_rows = static_cast<int>(str_para.img_dst_rows);

			img_global_center_col = static_cast<int>(str_para.img_dst_cols / 2);
			img_global_center_row = static_cast<int>(str_para.img_dst_rows / 2 + str_para.img_R_dst_shift);

			img_local_center_col = img_local_cols / 2;
			img_local_center_row = img_local_rows / 2;

			point_col = 0;
			point_row = 0;

			// Set map buffer
			float *pmap_R2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_R2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
						point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
						point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
						point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
						point_row = 0;
					}

					pmap_R2_cols[i*img_global_cols + j] =
						pmap_R_cols[point_row*img_local_cols + point_col];

					pmap_R2_rows[i*img_global_cols + j] =
						pmap_R_rows[point_row*img_local_cols + point_col];
				}
			}

			delete pmap_L_cols;
			delete pmap_L_rows;
			delete pmap_R_cols;
			delete pmap_R_rows;

			// *************************************************** //
			// STEP : Fix dark line L/R                            //
			// *************************************************** //
			if (1)
			{
				float p_col, p_row;
				short write_col, write_row;

				// L-side
				for (int i = 0; i < img_global_rows; i++)
				{
					for (int j = 0; j < img_global_cols; j++)
					{
						p_col = pmap_L2_cols[i * img_global_cols + j];
						p_row = pmap_L2_rows[i * img_global_cols + j];

						write_col = static_cast<short>(p_col);
						write_row = static_cast<short>(p_row);

						if (write_col == 0 && write_row == 0)
						{
							if (j > 0)
							{
								pmap_L2_cols[i * img_global_cols + j] = pmap_L2_cols[i * img_global_cols + j - 1];
								pmap_L2_rows[i * img_global_cols + j] = pmap_L2_rows[i * img_global_cols + j - 1];
							}

#ifdef WIN32
							if (0)
							{
								std::cout << "------------------------------" << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : L side - cols : " << j << ", value = " << write_col << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : L side - rows : " << i << ", value = " << write_row << std::endl;
							}
#endif

#ifdef linux
							if (0)
							{
								LOGI("------------------------------------");
								LOGI("(DM ERR) Map_LUT_CC : L side - cols : %d, value = %d", j, write_col);
								LOGI("(DM ERR) Map_LUT_CC : L side - rows : %d, value = %d", i, write_row);
							}
#endif
						}
					}
				}

				// R-side
				for (int i = 0; i < img_global_rows; i++)
				{
					for (int j = 0; j < img_global_cols; j++)
					{
						p_col = pmap_R2_cols[i * img_global_cols + j];
						p_row = pmap_R2_rows[i * img_global_cols + j];

						write_col = static_cast<short>(p_col);
						write_row = static_cast<short>(p_row);

						if (write_col == 0 && write_row == 0)
						{
							if (j > 0)
							{
								pmap_R2_cols[i * img_global_cols + j] = pmap_R2_cols[i * img_global_cols + j - 1];
								pmap_R2_rows[i * img_global_cols + j] = pmap_R2_rows[i * img_global_cols + j - 1];
							}

#ifdef WIN32
							if (0)
							{
								std::cout << "------------------------------" << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : R side - cols : " << j << ", value = " << write_col << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : R side - rows : " << i << ", value = " << write_row << std::endl;
							}
#endif

#ifdef linux
							if (0)
							{
								LOGI("------------------------------------");
								LOGI("(DM ERR) Map_LUT_CC : R side - cols : %d, value = %d", j, write_col);
								LOGI("(DM ERR) Map_LUT_CC : R side - rows : %d, value = %d", i, write_row);
							}
#endif
						}
					}
				}
			}

			// *************************************************** //
			// STEP : start L/R LUT to one LUT                     //
			// *************************************************** //
			float *pmap_LR_cols = new float[img_global_cols * 2 * img_global_rows];
			float *pmap_LR_rows = new float[img_global_cols * 2 * img_global_rows];

			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols * 2; j++)
				{
					if (j < img_global_cols)
					{
						pmap_LR_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
						pmap_LR_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];

						if (map_lut_cols != nullptr &&
							map_lut_rows != nullptr)
						{
							map_lut_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
							map_lut_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];
						}
					}
					else
					{
						pmap_LR_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
						pmap_LR_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];

						if (map_lut_cols != nullptr &&
							map_lut_rows != nullptr)
						{
							map_lut_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
							map_lut_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];
						}
					}
				}
			}

			delete pmap_L2_cols;
			delete pmap_L2_rows;
			delete pmap_R2_cols;
			delete pmap_R2_rows;

			// *************************************************** //
			// STEP : resize to output resolution                  //
			// *************************************************** //
			float *pmap_LR_cols_scale = new float[static_cast<unsigned int>(str_para.out_lut_cols) *
				static_cast<unsigned int>(str_para.out_lut_rows)];
			float *pmap_LR_rows_scale = new float[static_cast<unsigned int>(str_para.out_lut_cols) *
				static_cast<unsigned int>(str_para.out_lut_rows)];

			if (str_para.out_img_resolution == eys::IMG_RESOLUTION_1080P)
			{
				/*
				eys::fisheye360::Resize_LUT_1080P_CC(
					static_cast<int>(str_para.img_dst_cols * 2),
					static_cast<int>(str_para.img_dst_rows),
					static_cast<int>(str_para.out_lut_cols),
					static_cast<int>(str_para.out_lut_rows),
					static_cast<int>(str_para.img_overlay_LR),
					static_cast<int>(str_para.img_overlay_RL),
					pmap_LR_cols,
					pmap_LR_rows,
					lut_cols_scale,
					lut_rows_scale,
					overlay_LR_dst,
					overlay_RL_dst,
					pmap_LR_cols_scale,
					pmap_LR_rows_scale
					);
					*/

				float mag = 1920.0f / ((str_para.img_dst_cols * 2) - (str_para.img_overlay_LR) - (str_para.img_overlay_RL));

				int   lut_cols_mag = static_cast<int>((str_para.img_dst_cols * 2)*mag + 0.5);
				int   lut_rows_mag = 1080;
				int   lut_cols_src_single = (str_para.img_dst_cols * 2) / 2;
				int   lut_cols_dst_single = (str_para.out_lut_cols) / 2;
				int   lut_cols_mag_single = static_cast<int>((lut_cols_mag + 0.5) / 2.0);

				lut_cols_mag = lut_cols_mag_single * 2;





				int ptr = ((str_para.out_lut_cols) / 2 - lut_cols_mag / 2) / 2;

				//////////////////////////////////////////// L cols ///////////////////////////////////////////////////////////
				float *map_src_L_cols = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_L_cols_mag = new float[lut_cols_mag*lut_rows_mag];

				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_L_cols[i*lut_cols_src_single + j] = pmap_LR_cols[i*(str_para.img_dst_cols * 2) + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_L_cols,
					map_dst_L_cols_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_cols_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
					}
				}
				delete map_src_L_cols;
				delete map_dst_L_cols_mag;

				/////////////////////////////////////////////////////////////// R cols  //////////////////////////////////////////
				float *map_src_R_cols = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_R_cols_mag = new float[lut_cols_mag*lut_rows_mag];


				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_R_cols[i*lut_cols_src_single + j] = pmap_LR_cols[i*(str_para.img_dst_cols * 2) + lut_cols_src_single + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_R_cols,
					map_dst_R_cols_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
					}
				}

				delete map_src_R_cols;
				delete map_dst_R_cols_mag;

				delete pmap_LR_cols;


				//////////////////////////////////////////// L rows ////////////////////////////////////////////////////////////
				float *map_src_L_rows = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_L_rows_mag = new float[lut_cols_mag*lut_rows_mag];


				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_L_rows[i*lut_cols_src_single + j] = pmap_LR_rows[i*(str_para.img_dst_cols * 2) + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_L_rows,
					map_dst_L_rows_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_rows_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];

					}
				}

				delete map_src_L_rows;
				delete map_dst_L_rows_mag;

				////////////////////////////////////////// R rows  ///////////////////////////////////////////////////////
				float *map_src_R_rows = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_R_rows_mag = new float[lut_cols_mag*lut_rows_mag];


				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_R_rows[i*lut_cols_src_single + j] = pmap_LR_rows[i*(str_para.img_dst_cols * 2) + lut_cols_src_single + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_R_rows,
					map_dst_R_rows_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
					}
				}


				delete map_src_R_rows;
				delete map_dst_R_rows_mag;

				delete pmap_LR_rows;

			}
			if (str_para.out_img_resolution == eys::IMG_RESOLUTION_2560x1440)
			{
				/*
				eys::fisheye360::Resize_LUT_1440P_CC(
					static_cast<int>(str_para.img_dst_cols * 2),
					static_cast<int>(str_para.img_dst_rows),
					static_cast<int>(str_para.out_lut_cols),
					static_cast<int>(str_para.out_lut_rows),
					static_cast<int>(str_para.img_overlay_LR),
					static_cast<int>(str_para.img_overlay_RL),
					pmap_LR_cols,
					pmap_LR_rows,
					lut_cols_scale,
					lut_rows_scale,
					overlay_LR_dst,
					overlay_RL_dst,
					pmap_LR_cols_scale,
					pmap_LR_rows_scale
					);
					*/
				float mag = 2560.0f / ((str_para.img_dst_cols * 2) - (str_para.img_overlay_LR) - (str_para.img_overlay_RL));

				int   lut_cols_mag = static_cast<int>((str_para.img_dst_cols * 2)*mag + 0.5);
				int   lut_rows_mag = 1440;
				int   lut_cols_src_single = (str_para.img_dst_cols * 2) / 2;
				int   lut_cols_dst_single = (str_para.out_lut_cols) / 2;
				int   lut_cols_mag_single = static_cast<int>((lut_cols_mag + 0.5) / 2.0);

				lut_cols_mag = lut_cols_mag_single * 2;





				int ptr = ((str_para.out_lut_cols) / 2 - lut_cols_mag / 2) / 2;

				//////////////////////////////////////////// L cols ///////////////////////////////////////////////////////////
				float *map_src_L_cols = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_L_cols_mag = new float[lut_cols_mag*lut_rows_mag];

				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_L_cols[i*lut_cols_src_single + j] = pmap_LR_cols[i*(str_para.img_dst_cols * 2) + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_L_cols,
					map_dst_L_cols_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_cols_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
					}
				}
				delete map_src_L_cols;
				delete map_dst_L_cols_mag;

				/////////////////////////////////////////////////////////////// R cols  //////////////////////////////////////////
				float *map_src_R_cols = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_R_cols_mag = new float[lut_cols_mag*lut_rows_mag];


				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_R_cols[i*lut_cols_src_single + j] = pmap_LR_cols[i*(str_para.img_dst_cols * 2) + lut_cols_src_single + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_R_cols,
					map_dst_R_cols_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_cols_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
					}
				}

				delete map_src_R_cols;
				delete map_dst_R_cols_mag;
				delete pmap_LR_cols;


				//////////////////////////////////////////// L rows ////////////////////////////////////////////////////////////
				float *map_src_L_rows = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_L_rows_mag = new float[lut_cols_mag*lut_rows_mag];


				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_L_rows[i*lut_cols_src_single + j] = pmap_LR_rows[i*(str_para.img_dst_cols * 2) + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_L_rows,
					map_dst_L_rows_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_rows_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];

					}
				}

				delete map_src_L_rows;
				delete map_dst_L_rows_mag;

				////////////////////////////////////////// R rows  ///////////////////////////////////////////////////////
				float *map_src_R_rows = new float[(str_para.img_dst_cols * 2)*(str_para.img_dst_rows)];
				float *map_dst_R_rows_mag = new float[lut_cols_mag*lut_rows_mag];


				for (int i = 0; i < (str_para.img_dst_rows); i++)
					for (int j = 0; j < lut_cols_src_single; j++)
						map_src_R_rows[i*lut_cols_src_single + j] = pmap_LR_rows[i*(str_para.img_dst_cols * 2) + lut_cols_src_single + j];

				Interpolation_2D_Bilinear_CC((str_para.img_dst_cols * 2),
					(str_para.img_dst_rows),
					lut_cols_mag,
					lut_rows_mag,
					map_src_R_rows,
					map_dst_R_rows_mag);

				for (int i = 0; i < (str_para.out_lut_rows); i++)
				{
					for (int j = 0; j < lut_cols_dst_single; j++)
					{
						if (j < ptr)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single];
						else if (j >= ptr && j < ptr + lut_cols_mag_single)
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + j - ptr];
						else
							pmap_LR_rows_scale[i*(str_para.out_lut_cols) + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
					}
				}


				delete map_src_R_rows;
				delete map_dst_R_rows_mag;

				delete pmap_LR_rows;
			}

			// *************************************************** //
			// STEP : LUT output                                   //
			// *************************************************** //
			if (filename == nullptr)
			{
			}
			else
			{
				if (lut_type == eys::LUT_LXLYRXRY_16_3)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float     p_col, p_row;
					short     write_col, write_row;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols; j++)
						{
							p_col = pmap_LR_cols_scale[i * str_para.out_lut_cols + j];
							p_row = pmap_LR_rows_scale[i * str_para.out_lut_cols + j];

							if (p_col < 0)
							{
								p_col = 0;
							}
							else if (p_col >= str_para.img_src_cols * 2 - 1)
							{
								p_col = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row < 0)
							{
								p_row = 0;
							}
							else if (p_row >= str_para.img_src_rows - 1)
							{
								p_row = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col = static_cast<short>(float_bit_value*p_col + 0.5f);
							write_row = static_cast<short>(float_bit_value*p_row + 0.5f);

							if (fwrite(&write_col, 1, 2, f) != 2 || fwrite(&write_row, 1, 2, f) != 2)
							{
								delete pmap_LR_cols_scale;
								delete pmap_LR_rows_scale;
								delete protation_L_matrix;
								delete protation_R_matrix;
								return -1;
							}
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_3)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float     p_col_L, p_row_L;
					float     p_col_R, p_row_R;
					short     write_col_L, write_row_L;
					short     write_col_R, write_row_R;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols / 2; j++)
						{
							p_col_L = pmap_LR_cols_scale[i * str_para.out_lut_cols + j];
							p_row_L = pmap_LR_rows_scale[i * str_para.out_lut_cols + j];
							p_col_R = pmap_LR_cols_scale[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j] - str_para.img_src_cols;
							p_row_R = pmap_LR_rows_scale[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							if (fwrite(&write_col_L, 1, 2, f) != 2 || fwrite(&write_row_L, 1, 2, f) != 2 || fwrite(&write_col_R, 1, 2, f) != 2 || fwrite(&write_row_R, 1, 2, f) != 2)
							{
								delete pmap_LR_cols_scale;
								delete pmap_LR_rows_scale;
								delete protation_L_matrix;
								delete protation_R_matrix;
								return -1;
							}
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_3_BIT0)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols / 2; j++)
						{
							p_col_L = pmap_LR_cols_scale[i * str_para.out_lut_cols + j];
							p_row_L = pmap_LR_rows_scale[i * str_para.out_lut_cols + j];
							p_col_R = pmap_LR_cols_scale[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j] - str_para.img_src_cols;
							p_row_R = pmap_LR_rows_scale[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							write_col_L = write_col_L >> 1;
							write_row_L = write_row_L >> 1;
							write_col_R = write_col_R >> 1;
							write_row_R = write_row_R >> 1;

							write_col_L = write_col_L << 1;
							write_row_L = write_row_L << 1;
							write_col_R = write_col_R << 1;
							write_row_R = write_row_R << 1;

							if (fwrite(&write_col_L, 1, 2, f) != 2 || fwrite(&write_row_L, 1, 2, f) != 2 || fwrite(&write_col_R, 1, 2, f) != 2 || fwrite(&write_row_R, 1, 2, f) != 2)
							{
								delete pmap_LR_cols_scale;
								delete pmap_LR_rows_scale;
								delete protation_L_matrix;
								delete protation_R_matrix;
								return -1;
							}
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_5)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 32; // Bit value for float, 2^5 = 32 for 5bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols / 2; j++)
						{
							p_col_L = pmap_LR_cols_scale[i * str_para.out_lut_cols + j];
							p_row_L = pmap_LR_rows_scale[i * str_para.out_lut_cols + j];
							p_col_R = pmap_LR_cols_scale[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j] - str_para.img_src_cols;
							p_row_R = pmap_LR_rows_scale[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							if (fwrite(&write_col_L, 1, 2, f) != 2 || fwrite(&write_row_L, 1, 2, f) != 2 || fwrite(&write_col_R, 1, 2, f) != 2 || fwrite(&write_row_R, 1, 2, f) != 2)
							{
								delete pmap_LR_cols_scale;
								delete pmap_LR_rows_scale;
								delete protation_L_matrix;
								delete protation_R_matrix;
								return -1;
							}
						}
					}
					fclose(f);
				}
			}

			delete pmap_LR_cols_scale;
			delete pmap_LR_rows_scale;
			//delete pmap_L_cols;
			//delete pmap_L_rows;
			//delete pmap_R_cols;
			//delete pmap_R_rows;
			delete protation_L_matrix;
			delete protation_R_matrix;
			//delete pmap_L2_cols;
			//delete pmap_L2_rows;
			//delete pmap_R2_cols;
			//delete pmap_R2_rows;
			//delete pmap_LR_cols;
			//delete pmap_LR_rows;
		}
		
		if (str_para.out_img_resolution == eys::IMG_RESOLUTION_NORMAL)
		{
			// Set parameters
			double unit_sphere_radius;     // Unit spherical radius for dewarping get x and y
			double min_col, min_row;       // Parameters of min position image width and height
			double max_col, max_row;       // Parameters of max position image width and height
			int    img_src_FOV_cols;       // Source image width within FOV
			int    img_src_FOV_rows;       // Source image height within FOV
			double img_src_FOV_center_col; // Source image center width position within FOV
			double img_src_FOV_center_row; // Source image center height position within FOV
			int    img_dst_cols;           // Destination image width
			int    img_dst_rows;           // Destination image height

			unit_sphere_radius = str_para.unit_sphere_radius;
			min_col = str_para.min_col;
			max_col = str_para.max_col;
			min_row = str_para.min_row;
			max_row = str_para.max_row;

			img_src_FOV_cols = (int)(str_para.semi_FOV_pixels * 2); // L/R side is the same
			img_src_FOV_rows = (int)(str_para.semi_FOV_pixels * 2); // L/R side is the same

			img_src_FOV_center_col = img_src_FOV_cols / 2.0; // L/R side is the same
			img_src_FOV_center_row = img_src_FOV_rows / 2.0; // L/R side is the same

			img_dst_cols = (int)(max_col - min_col + 1);
			img_dst_rows = (int)(max_row - min_row + 1);

			// Set value to compress parameters 
			double div_rows       = img_dst_rows / (7 - 1);
			double control_point1 = div_rows * 0;
			double control_point2 = div_rows * 1;
			double control_point3 = div_rows * 2;
			double control_point4 = div_rows * 3;
			double control_point5 = div_rows * 4;
			double control_point6 = div_rows * 5;
			double control_point7 = div_rows * 6;

			double mat_compress_rows[] = {
				control_point1,
				control_point2,
				control_point3,
				control_point4,
				control_point5,
				control_point6,
				control_point7
			};

			div_rows = img_dst_rows / (7 - 1);

			control_point1 = div_rows * 0;
			control_point2 = div_rows * 1;
			control_point3 = div_rows * 2;
			control_point4 = div_rows * 3;
			control_point5 = div_rows * 4;
			control_point6 = div_rows * 5;
			control_point7 = div_rows * 6;

			double mat_compress_values[] = {
				str_para.spline_control_v1,
				str_para.spline_control_v2,
				str_para.spline_control_v3,
				str_para.spline_control_v4,
				str_para.spline_control_v5,
				str_para.spline_control_v6,
				str_para.spline_control_v7
			};

			// Set lens distortion parameters
			double lens_distoriton_para[] = {
				str_para.distortion_fix_v0,
				str_para.distortion_fix_v1,
				str_para.distortion_fix_v2,
				str_para.distortion_fix_v3,
				str_para.distortion_fix_v4,
				str_para.distortion_fix_v5,
				str_para.distortion_fix_v6
			};

			// Set map buffer and others
			float  *pmap_L_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_L_rows        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_rows        = new float[img_dst_cols*img_dst_rows];
			double *protation_L_matrix = new double[4];
			double *protation_R_matrix = new double[4];

			// Set variables for LUT calculation
			double point_dst_col, point_dst_row;
			double point_dst_compress_col, point_dst_compress_row;
			double point_src1_FOV_col, point_src1_FOV_row;
			double point_src2_FOV_col, point_src2_FOV_row;
			double point_src3_FOV_col, point_src3_FOV_row;
			double point_src4_col, point_src4_row;
			double point_dist_x, point_dist_y;

			// *************************************************** //
			// STEP : start local image calculation                //
			// *************************************************** //
			// Set rotation matrix
			protation_L_matrix[0] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[1] = -1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[2] = +1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[3] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);

			protation_R_matrix[0] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[1] = -1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[2] = +1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[3] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);

			// Local image calculation, the rows and cols are the same in L/R
			for (int i = 0; i < img_dst_rows; i++)
			{
				for (int j = 0; j < img_dst_cols; j++)
				{
					// Proc : compress
					if (str_para.spline_control_v1 == 0 &&
						str_para.spline_control_v2 == 0 &&
						str_para.spline_control_v3 == 0 &&
						str_para.spline_control_v4 == 0 &&
						str_para.spline_control_v5 == 0 &&
						str_para.spline_control_v6 == 0 &&
						str_para.spline_control_v7 == 0)
					{
						point_dst_col = j;
						point_dst_row = i;
					}
					else
					{
						Point_Barrel_Compress_CC(
							j,
							i,
							img_dst_cols,
							img_dst_rows,
							mat_compress_rows,
							mat_compress_values,
							7,
							point_dst_col,
							point_dst_row
							);
					}

					// Proc : get destination position from original position
					point_dst_compress_col = point_dst_col + min_col;
					point_dst_compress_row = point_dst_row + min_row;

					// Proc : dewarping
					eys::fisheye360::Dewarp_GetXY_CC(
						point_dst_compress_col,
						point_dst_compress_row,
						str_para.FOV,
						static_cast<int>(str_para.semi_FOV_pixels),
						static_cast<int>(str_para.lens_projection_type),
						unit_sphere_radius,
						min_col,
						max_col,
						min_row,
						max_row,
						point_src1_FOV_col,
						point_src1_FOV_row
						);

					if (point_src1_FOV_col < 0)
					{
						point_src1_FOV_col = 0;
					}
					else if (point_src1_FOV_col > img_src_FOV_cols - 1)
					{
						point_src1_FOV_col = img_src_FOV_cols - 1;
					}

					if (point_src1_FOV_row < 0)
					{
						point_src1_FOV_row = 0;
					}
					else if (point_src1_FOV_row > img_src_FOV_rows - 1)
					{
						point_src1_FOV_row = img_src_FOV_rows - 1;
					}

					// L side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_L_matrix[0] * point_dist_x +
							protation_L_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_L_matrix[2] * point_dist_x +
							protation_L_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						if (point_src3_FOV_col < 0)
						{
							point_src3_FOV_col = 0;
						}
						else if (point_src3_FOV_col > img_src_FOV_cols - 1)
						{
							point_src3_FOV_col = img_src_FOV_cols - 1;
						}

						if (point_src3_FOV_row < 0)
						{
							point_src3_FOV_row = 0;
						}
						else if (point_src3_FOV_row > img_src_FOV_rows - 1)
						{
							point_src3_FOV_row = img_src_FOV_rows - 1;
						}

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col =
							(point_src3_FOV_col - img_src_FOV_center_col) +
							str_para.img_L_src_col_center;

						point_src4_row =
							(point_src3_FOV_row - img_src_FOV_center_row) +
							str_para.img_L_src_row_center;

						if (fabs(point_src4_col - str_para.img_L_src_col_center) > str_para.semi_FOV_pixels)
						{
							point_src4_col = 0;
						}

						if (fabs(point_src4_row - str_para.img_L_src_row_center) > str_para.semi_FOV_pixels)
						{
							point_src4_row = 0;
						}

						pmap_L_cols[i*img_dst_cols + j] = static_cast<float>(point_src4_col);
						pmap_L_rows[i*img_dst_cols + j] = static_cast<float>(point_src4_row);
					}

					// R side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_R_matrix[0] * point_dist_x +
							protation_R_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_R_matrix[2] * point_dist_x +
							protation_R_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						if (point_src3_FOV_col < 0)
						{
							point_src3_FOV_col = 0;
						}
						else if (point_src3_FOV_col > img_src_FOV_cols - 1)
						{
							point_src3_FOV_col = img_src_FOV_cols - 1;
						}

						if (point_src3_FOV_row < 0)
						{
							point_src3_FOV_row = 0;
						}
						else if (point_src3_FOV_row > img_src_FOV_rows - 1)
						{
							point_src3_FOV_row = img_src_FOV_rows - 1;
						}

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col =
							(point_src3_FOV_col - img_src_FOV_center_col) +
							str_para.img_R_src_col_center;

						point_src4_row =
							(point_src3_FOV_row - img_src_FOV_center_row) +
							str_para.img_R_src_row_center;

						if (fabs(point_src4_col - str_para.img_R_src_col_center) > str_para.semi_FOV_pixels)
						{
							point_src4_col = 0;
						}

						if (fabs(point_src4_row - str_para.img_R_src_row_center) > str_para.semi_FOV_pixels)
						{
							point_src4_row = 0;
						}

						pmap_R_cols[i*img_dst_cols + j] =
							static_cast<float>(point_src4_col)
							+str_para.img_src_cols;

						pmap_R_rows[i*img_dst_cols + j] =
							static_cast<float>(point_src4_row);
					}
				}
			}

			// *************************************************** //
			// STEP : start L side global image calculation        //
			// *************************************************** //
			// Set parameters
			int img_local_cols = img_dst_cols;
			int img_local_rows = img_dst_rows;

			int img_global_cols = static_cast<int>(str_para.img_dst_cols);
			int img_global_rows = static_cast<int>(str_para.img_dst_rows);

			int img_global_center_col = static_cast<int>(str_para.img_dst_cols / 2);
			int img_global_center_row = static_cast<int>(str_para.img_dst_rows / 2 + str_para.img_L_dst_shift);

			int img_local_center_col = img_local_cols / 2;
			int img_local_center_row = img_local_rows / 2;

			int point_col, point_row;

			// Set map buffer
			float *pmap_L2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_L2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
						point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
						point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
						point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
						point_row = 0;
					}

					pmap_L2_cols[i*img_global_cols + j] =
						pmap_L_cols[point_row*img_local_cols + point_col];

					pmap_L2_rows[i*img_global_cols + j] =
						pmap_L_rows[point_row*img_local_cols + point_col];
				}
			}

			// *************************************************** //
			// STEP : start R side global image calculation        //
			// *************************************************** //
			// Set parameters
			img_local_cols = img_dst_cols;
			img_local_rows = img_dst_rows;

			img_global_cols = static_cast<int>(str_para.img_dst_cols);
			img_global_rows = static_cast<int>(str_para.img_dst_rows);

			img_global_center_col = static_cast<int>(str_para.img_dst_cols / 2);
			img_global_center_row = static_cast<int>(str_para.img_dst_rows / 2 + str_para.img_R_dst_shift);

			img_local_center_col = img_local_cols / 2;
			img_local_center_row = img_local_rows / 2;

			point_col = 0;
			point_row = 0;

			// Set map buffer
			float *pmap_R2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_R2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
						point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
						point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
						point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
						point_row = 0;
					}

					pmap_R2_cols[i*img_global_cols + j] =
						pmap_R_cols[point_row*img_local_cols + point_col];

					pmap_R2_rows[i*img_global_cols + j] =
						pmap_R_rows[point_row*img_local_cols + point_col];
				}
			}

			// *************************************************** //
			// STEP : Fix dark line L/R                            //
			// *************************************************** //
			if (1)
			{
				float p_col, p_row;
				short write_col, write_row;

				// L-side
				for (int i = 0; i < img_global_rows; i++)
				{
					for (int j = 0; j < img_global_cols; j++)
					{
						p_col = pmap_L2_cols[i * img_global_cols + j];
						p_row = pmap_L2_rows[i * img_global_cols + j];

						write_col = static_cast<short>(p_col);
						write_row = static_cast<short>(p_row);

						if (write_col == 0 && write_row == 0)
						{
							if (j > 0)
							{
								pmap_L2_cols[i * img_global_cols + j] = pmap_L2_cols[i * img_global_cols + j - 1];
								pmap_L2_rows[i * img_global_cols + j] = pmap_L2_rows[i * img_global_cols + j - 1];
							}

#ifdef WIN32
							if (0)
							{
								std::cout << "------------------------------" << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : L side - cols : " << j << ", value = " << write_col << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : L side - rows : " << i << ", value = " << write_row << std::endl;
							}
#endif

#ifdef linux
							if (0)
							{
								LOGI("------------------------------------");
								LOGI("(DM ERR) Map_LUT_CC : L side - cols : %d, value = %d", j, write_col);
								LOGI("(DM ERR) Map_LUT_CC : L side - rows : %d, value = %d", i, write_row);
							}
#endif
						}
					}
				}

				// R-side
				for (int i = 0; i < img_global_rows; i++)
				{
					for (int j = 0; j < img_global_cols; j++)
					{
						p_col = pmap_R2_cols[i * img_global_cols + j];
						p_row = pmap_R2_rows[i * img_global_cols + j];

						write_col = static_cast<short>(p_col);
						write_row = static_cast<short>(p_row);

						if (write_col == 0 && write_row == 0)
						{
							if (j > 0)
							{
								pmap_R2_cols[i * img_global_cols + j] = pmap_R2_cols[i * img_global_cols + j - 1];
								pmap_R2_rows[i * img_global_cols + j] = pmap_R2_rows[i * img_global_cols + j - 1];
							}

#ifdef WIN32
							if (0)
							{
								std::cout << "------------------------------" << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : R side - cols : " << j << ", value = " << write_col << std::endl;
								std::cout << "(DM ERR) Map_LUT_CC : R side - rows : " << i << ", value = " << write_row << std::endl;
							}
#endif

#ifdef linux
							if (0)
							{
								LOGI("------------------------------------");
								LOGI("(DM ERR) Map_LUT_CC : R side - cols : %d, value = %d", j, write_col);
								LOGI("(DM ERR) Map_LUT_CC : R side - rows : %d, value = %d", i, write_row);
							}
#endif
						}
					}
				}
			}

			// *************************************************** //
			// STEP : start L/R LUT to one LUT                     //
			// *************************************************** //
			float *pmap_LR_cols = new float[img_global_cols * 2 * img_global_rows];
			float *pmap_LR_rows = new float[img_global_cols * 2 * img_global_rows];

			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols * 2; j++)
				{
					if (j < img_global_cols)
					{
						pmap_LR_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
						pmap_LR_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];

						if (map_lut_cols != nullptr &&
							map_lut_rows != nullptr)
						{
							map_lut_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
							map_lut_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];
						}
					}
					else
					{
						pmap_LR_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
						pmap_LR_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];

						if (map_lut_cols != nullptr &&
							map_lut_rows != nullptr)
						{
							map_lut_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
							map_lut_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];
						}
					}
				}
			}

			// *************************************************** //
			// STEP : LUT output                                   //
			// *************************************************** //
			if (filename == nullptr)
			{
			}
			else
			{
				if (lut_type == eys::LUT_LXLYRXRY_16_3)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float     p_col, p_row;
					short     write_col, write_row;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols; j++)
						{
							p_col = pmap_LR_cols[i * str_para.out_lut_cols + j];
							p_row = pmap_LR_rows[i * str_para.out_lut_cols + j];

							if (p_col < 0)
							{
								p_col = 0;
							}
							else if (p_col >= str_para.img_src_cols * 2 - 1)
							{
								p_col = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row < 0)
							{
								p_row = 0;
							}
							else if (p_row >= str_para.img_src_rows - 1)
							{
								p_row = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col = static_cast<short>(float_bit_value*p_col + 0.5f);
							write_row = static_cast<short>(float_bit_value*p_row + 0.5f);

							if (fwrite(&write_col, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_3)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float     p_col_L, p_row_L;
					float     p_col_R, p_row_R;
					short     write_col_L, write_row_L;
					short     write_col_R, write_row_R;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols / 2; j++)
						{
							p_col_L = pmap_LR_cols[i * str_para.out_lut_cols + j];
							p_row_L = pmap_LR_rows[i * str_para.out_lut_cols + j];
							p_col_R = pmap_LR_cols[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j] - str_para.img_src_cols;
							p_row_R = pmap_LR_rows[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							if(fwrite(&write_col_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_col_R, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_R, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_3_BIT0)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols / 2; j++)
						{
							p_col_L = pmap_LR_cols[i * str_para.out_lut_cols + j];
							p_row_L = pmap_LR_rows[i * str_para.out_lut_cols + j];
							p_col_R = pmap_LR_cols[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j] - str_para.img_src_cols;
							p_row_R = pmap_LR_rows[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							write_col_L = write_col_L >> 1;
							write_row_L = write_row_L >> 1;
							write_col_R = write_col_R >> 1;
							write_row_R = write_row_R >> 1;

							write_col_L = write_col_L << 1;
							write_row_L = write_row_L << 1;
							write_col_R = write_col_R << 1;
							write_row_R = write_row_R << 1;

							if (fwrite(&write_col_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_col_R, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_R, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_5)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 32; // Bit value for float, 2^5 = 32 for 5bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < str_para.out_lut_rows; i++)
					{
						for (int j = 0; j < str_para.out_lut_cols / 2; j++)
						{
							p_col_L = pmap_LR_cols[i * str_para.out_lut_cols + j];
							p_row_L = pmap_LR_rows[i * str_para.out_lut_cols + j];
							p_col_R = pmap_LR_cols[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j] - str_para.img_src_cols;
							p_row_R = pmap_LR_rows[i * str_para.out_lut_cols + str_para.out_lut_cols / 2 + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							if (fwrite(&write_col_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_col_R, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_R, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}
			}

			delete pmap_L_cols;
			delete pmap_L_rows;
			delete pmap_R_cols;
			delete pmap_R_rows;
			delete protation_L_matrix;
			delete protation_R_matrix;
			delete pmap_L2_cols;
			delete pmap_L2_rows;
			delete pmap_R2_cols;
			delete pmap_R2_rows;
			delete pmap_LR_cols;
			delete pmap_LR_rows;
		}
	}

	if (turn_on_img_resolution == false)
	{
		if (str_para.out_img_resolution == eys::IMG_RESOLUTION_FAST_64)
		{
			// Set parameters
			double unit_sphere_radius;     // Unit spherical radius for dewarping get x and y
			double min_col, min_row;       // Parameters of min position image width and height
			double max_col, max_row;       // Parameters of max position image width and height
			int    img_src_FOV_cols;       // Source image width within FOV
			int    img_src_FOV_rows;       // Source image height within FOV
			double img_src_FOV_center_col; // Source image center width position within FOV
			double img_src_FOV_center_row; // Source image center height position within FOV
			int    img_dst_cols;           // Destination image width
			int    img_dst_rows;           // Destination image height

			unit_sphere_radius = str_para.unit_sphere_radius;
			min_col            = str_para.min_col;
			max_col            = str_para.max_col;
			min_row            = str_para.min_row;
			max_row            = str_para.max_row;

			img_src_FOV_cols = (int)(str_para.semi_FOV_pixels * 2); // L/R side is the same
			img_src_FOV_rows = (int)(str_para.semi_FOV_pixels * 2); // L/R side is the same

			img_src_FOV_center_col = img_src_FOV_cols / 2.0; // L/R side is the same
			img_src_FOV_center_row = img_src_FOV_rows / 2.0; // L/R side is the same

			img_dst_cols = (int)(max_col - min_col + 1);
			img_dst_rows = (int)(max_row - min_row + 1);

			// Set value to compress parameters 
			double div_rows       = img_dst_rows / (7 - 1);
			double control_point1 = div_rows * 0;
			double control_point2 = div_rows * 1;
			double control_point3 = div_rows * 2;
			double control_point4 = div_rows * 3;
			double control_point5 = div_rows * 4;
			double control_point6 = div_rows * 5;
			double control_point7 = div_rows * 6;

			double mat_compress_rows[] = {
				control_point1,
				control_point2,
				control_point3,
				control_point4,
				control_point5,
				control_point6,
				control_point7 
			};

			div_rows = img_dst_rows / (7 - 1);

			control_point1 = div_rows * 0;
			control_point2 = div_rows * 1;
			control_point3 = div_rows * 2;
			control_point4 = div_rows * 3;
			control_point5 = div_rows * 4;
			control_point6 = div_rows * 5;
			control_point7 = div_rows * 6;

			double mat_compress_values[] = {
				str_para.spline_control_v1,
				str_para.spline_control_v2,
				str_para.spline_control_v3,
				str_para.spline_control_v4,
				str_para.spline_control_v5,
				str_para.spline_control_v6,
				str_para.spline_control_v7 
			};

			// Set lens distortion parameters
			double lens_distoriton_para[] = {
				str_para.distortion_fix_v0,
				str_para.distortion_fix_v1,
				str_para.distortion_fix_v2,
				str_para.distortion_fix_v3,
				str_para.distortion_fix_v4,
				str_para.distortion_fix_v5,
				str_para.distortion_fix_v6
			};

			// Set map buffer and others
			float  *pmap_L_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_L_rows        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_rows        = new float[img_dst_cols*img_dst_rows];
			double *protation_L_matrix = new double[4];
			double *protation_R_matrix = new double[4];

			// Set variables for LUT calculation
			double point_dst_col, point_dst_row;
			double point_dst_compress_col, point_dst_compress_row;
			double point_src1_FOV_col, point_src1_FOV_row;
			double point_src2_FOV_col, point_src2_FOV_row;
			double point_src3_FOV_col, point_src3_FOV_row;
			double point_src4_col, point_src4_row;
			double point_dist_x, point_dist_y;

			// --------------------------------------------------//
			// STEP : start local image calculation               //
			// --------------------------------------------------//
			// Set rotation matrix
			protation_L_matrix[0] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[1] = -1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[2] = +1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[3] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);

			protation_R_matrix[0] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[1] = -1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[2] = +1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[3] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);

			// Local image calculation, the rows and cols are the same in L/R in left
			for (int i = 0; i < img_dst_rows; i++)
			{
				for (int j = img_dst_cols / 2 - (int)str_para.img_dst_cols / 2; 
					     j < img_dst_cols / 2 - (int)str_para.img_dst_cols / 2 + 64; j++)
				{
					// Proc : compress
					if (str_para.spline_control_v1 == 0 &&
						str_para.spline_control_v2 == 0 &&
						str_para.spline_control_v3 == 0 &&
						str_para.spline_control_v4 == 0 &&
						str_para.spline_control_v5 == 0 &&
						str_para.spline_control_v6 == 0 &&
						str_para.spline_control_v7 == 0)
					{
						point_dst_col = j;
						point_dst_row = i;
					}
					else
					{
						Point_Barrel_Compress_CC(
							j,
							i,
							img_dst_cols,
							img_dst_rows,
							mat_compress_rows,
							mat_compress_values,
							7,
							point_dst_col,
							point_dst_row
							);
					}

					// Proc : get destination position from original position
					point_dst_compress_col = point_dst_col + min_col;
					point_dst_compress_row = point_dst_row + min_row;

					// Proc : dewarping
					eys::fisheye360::Dewarp_GetXY_CC(
						point_dst_compress_col,
						point_dst_compress_row,
						str_para.FOV,
						(int)(str_para.semi_FOV_pixels),
						(int)(str_para.lens_projection_type),
						unit_sphere_radius,
						min_col,
						max_col,
						min_row,
						max_row,
						point_src1_FOV_col,
						point_src1_FOV_row
						);

					// L side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_L_matrix[0] * point_dist_x +
							protation_L_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_L_matrix[2] * point_dist_x +
							protation_L_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col = (point_src3_FOV_col - img_src_FOV_center_col) + str_para.img_L_src_col_center;
						point_src4_row = (point_src3_FOV_row - img_src_FOV_center_row) + str_para.img_L_src_row_center;

						pmap_L_cols[i*img_dst_cols + j] = (float)(point_src4_col);
						pmap_L_rows[i*img_dst_cols + j] = (float)(point_src4_row);
					}

					// R side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_R_matrix[0] * point_dist_x +
							protation_R_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_R_matrix[2] * point_dist_x +
							protation_R_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col = (point_src3_FOV_col - img_src_FOV_center_col) + str_para.img_R_src_col_center;
						point_src4_row = (point_src3_FOV_row - img_src_FOV_center_row) + str_para.img_R_src_row_center;

						pmap_R_cols[i*img_dst_cols + j] = (float)(point_src4_col)+str_para.img_src_cols;
						pmap_R_rows[i*img_dst_cols + j] = (float)(point_src4_row);
					}
				}
			}

			// Local image calculation, the rows and cols are the same in L/R in right
			for (int i = 0; i < img_dst_rows; i++)
			{
				for (int j = img_dst_cols / 2 + (int)str_para.img_dst_cols / 2 - 1;
					     j > img_dst_cols / 2 + (int)str_para.img_dst_cols / 2 - 64; j--)
				{
					// Proc : compress
					if (str_para.spline_control_v1 == 0 &&
						str_para.spline_control_v2 == 0 &&
						str_para.spline_control_v3 == 0 &&
						str_para.spline_control_v4 == 0 &&
						str_para.spline_control_v5 == 0 &&
						str_para.spline_control_v6 == 0 &&
						str_para.spline_control_v7 == 0)
					{
						point_dst_col = j;
						point_dst_row = i;
					}
					else
					{
						Point_Barrel_Compress_CC(
							j,
							i,
							img_dst_cols,
							img_dst_rows,
							mat_compress_rows,
							mat_compress_values,
							7,
							point_dst_col,
							point_dst_row
							);
					}

					// Proc : get destination position from original position
					point_dst_compress_col = point_dst_col + min_col;
					point_dst_compress_row = point_dst_row + min_row;

					// Proc : dewarping
					eys::fisheye360::Dewarp_GetXY_CC(
						point_dst_compress_col,
						point_dst_compress_row,
						str_para.FOV,
						(int)(str_para.semi_FOV_pixels),
						(int)(str_para.lens_projection_type),
						unit_sphere_radius,
						min_col,
						max_col,
						min_row,
						max_row,
						point_src1_FOV_col,
						point_src1_FOV_row
						);

					// L side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_L_matrix[0] * point_dist_x +
							protation_L_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_L_matrix[2] * point_dist_x +
							protation_L_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col = (point_src3_FOV_col - img_src_FOV_center_col) + str_para.img_L_src_col_center;
						point_src4_row = (point_src3_FOV_row - img_src_FOV_center_row) + str_para.img_L_src_row_center;

						pmap_L_cols[i*img_dst_cols + j] = (float)(point_src4_col);
						pmap_L_rows[i*img_dst_cols + j] = (float)(point_src4_row);
					}

					// R side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_R_matrix[0] * point_dist_x +
							protation_R_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_R_matrix[2] * point_dist_x +
							protation_R_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col = (point_src3_FOV_col - img_src_FOV_center_col) + str_para.img_R_src_col_center;
						point_src4_row = (point_src3_FOV_row - img_src_FOV_center_row) + str_para.img_R_src_row_center;

						pmap_R_cols[i*img_dst_cols + j] = (float)(point_src4_col)+str_para.img_src_cols;
						pmap_R_rows[i*img_dst_cols + j] = (float)(point_src4_row);
					}
				}
			}

			// --------------------------------------------------//
			// STEP : start L side global image calculation       //
			// --------------------------------------------------//
			// Set parameters
			int img_local_cols = img_dst_cols;
			int img_local_rows = img_dst_rows;

			int img_global_cols = (int)(str_para.img_dst_cols);
			int img_global_rows = (int)(str_para.img_dst_rows);

			int img_global_center_col = (int)(str_para.img_dst_cols / 2);
			int img_global_center_row = (int)(str_para.img_dst_rows / 2 + str_para.img_L_dst_shift);

			int img_local_center_col = img_local_cols / 2;
			int img_local_center_row = img_local_rows / 2;

			int point_col, point_row;

			// Set map buffer
			float *pmap_L2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_L2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
						point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
						point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
						point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
						point_row = 0;
					}
					
					pmap_L2_cols[i*img_global_cols + j] = pmap_L_cols[point_row*img_local_cols + point_col];
					pmap_L2_rows[i*img_global_cols + j] = pmap_L_rows[point_row*img_local_cols + point_col];
				}
			}

			// --------------------------------------------------//
			// STEP : start R side global image calculation       //
			// --------------------------------------------------//
			// Set parameters
			img_local_cols = img_dst_cols;
			img_local_rows = img_dst_rows;

			img_global_cols = (int)(str_para.img_dst_cols);
			img_global_rows = (int)(str_para.img_dst_rows);

			img_global_center_col = (int)(str_para.img_dst_cols / 2);
			img_global_center_row = (int)(str_para.img_dst_rows / 2 + str_para.img_R_dst_shift);

			img_local_center_col = img_local_cols / 2;
			img_local_center_row = img_local_rows / 2;

			point_col = 0;
			point_row = 0;

			// Set map buffer
			float *pmap_R2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_R2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
						point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
						point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
						point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
						point_row = 0;
					}

					pmap_R2_cols[i*img_global_cols + j] = pmap_R_cols[point_row*img_local_cols + point_col];
					pmap_R2_rows[i*img_global_cols + j] = pmap_R_rows[point_row*img_local_cols + point_col];
				}
			}

			// --------------------------------------------------//
			// STEP : start L/R LUT to one LUT                    //
			// --------------------------------------------------//
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols * 2; j++)
				{
					if (j < img_global_cols)
					{
						map_lut_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
						map_lut_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];
					}
					else
					{
						map_lut_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
						map_lut_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];
					}
				}
			}

			// --------------------------------------------------//
			// delete buffer                                      //
			// --------------------------------------------------//
			delete pmap_L_cols;
			delete pmap_L_rows;
			delete pmap_R_cols;
			delete pmap_R_rows;
			delete protation_L_matrix;
			delete protation_R_matrix;

			delete pmap_L2_cols;
			delete pmap_L2_rows;
			delete pmap_R2_cols;
			delete pmap_R2_rows;
		}
		else
		{
			// Set parameters
			double unit_sphere_radius;     // Unit spherical radius for dewarping get x and y
			double min_col, min_row;       // Parameters of min position image width and height
			double max_col, max_row;       // Parameters of max position image width and height
			int    img_src_FOV_cols;       // Source image width within FOV
			int    img_src_FOV_rows;       // Source image height within FOV
			double img_src_FOV_center_col; // Source image center width position within FOV
			double img_src_FOV_center_row; // Source image center height position within FOV
			int    img_dst_cols;           // Destination image width
			int    img_dst_rows;           // Destination image height

			unit_sphere_radius = str_para.unit_sphere_radius;
			min_col            = str_para.min_col;
			max_col            = str_para.max_col;
			min_row            = str_para.min_row;
			max_row            = str_para.max_row;

			img_src_FOV_cols = (int)(str_para.semi_FOV_pixels * 2); // L/R side is the same
			img_src_FOV_rows = (int)(str_para.semi_FOV_pixels * 2); // L/R side is the same

			img_src_FOV_center_col = img_src_FOV_cols / 2.0; // L/R side is the same
			img_src_FOV_center_row = img_src_FOV_rows / 2.0; // L/R side is the same

			img_dst_cols = (int)(max_col - min_col + 1);
			img_dst_rows = (int)(max_row - min_row + 1);

			// Set value to compress parameters 
			double div_rows       = img_dst_rows / (7 - 1);
			double control_point1 = div_rows * 0;
			double control_point2 = div_rows * 1;
			double control_point3 = div_rows * 2;
			double control_point4 = div_rows * 3;
			double control_point5 = div_rows * 4;
			double control_point6 = div_rows * 5;
			double control_point7 = div_rows * 6;

			double mat_compress_rows[] = {
				control_point1,
				control_point2,
				control_point3,
				control_point4,
				control_point5,
				control_point6,
				control_point7 
			};

			div_rows = img_dst_rows / (7 - 1);

			control_point1 = div_rows * 0;
			control_point2 = div_rows * 1;
			control_point3 = div_rows * 2;
			control_point4 = div_rows * 3;
			control_point5 = div_rows * 4;
			control_point6 = div_rows * 5;
			control_point7 = div_rows * 6;

			double mat_compress_values[] = {
				str_para.spline_control_v1,
				str_para.spline_control_v2,
				str_para.spline_control_v3,
				str_para.spline_control_v4,
				str_para.spline_control_v5,
				str_para.spline_control_v6,
				str_para.spline_control_v7 
			};

			// Set lens distortion parameters
			double lens_distoriton_para[] = {
				str_para.distortion_fix_v0,
				str_para.distortion_fix_v1,
				str_para.distortion_fix_v2,
				str_para.distortion_fix_v3,
				str_para.distortion_fix_v4,
				str_para.distortion_fix_v5,
				str_para.distortion_fix_v6
			};

			// Set map buffer and others
			float  *pmap_L_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_L_rows        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_cols        = new float[img_dst_cols*img_dst_rows];
			float  *pmap_R_rows        = new float[img_dst_cols*img_dst_rows];
			double *protation_L_matrix = new double[4];
			double *protation_R_matrix = new double[4];

			// Set variables for LUT calculation
			double point_dst_col, point_dst_row;
			double point_dst_compress_col, point_dst_compress_row;
			double point_src1_FOV_col, point_src1_FOV_row;
			double point_src2_FOV_col, point_src2_FOV_row;
			double point_src3_FOV_col, point_src3_FOV_row;
			double point_src4_col, point_src4_row;
			double point_dist_x, point_dist_y;

			// *************************************************** //
			// STEP : start local image calculation                //
			// *************************************************** //
			// Set rotation matrix
			protation_L_matrix[0] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[1] = -1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[2] = +1.0*std::sin(str_para.img_L_rotation*EYS_PI / 180.0);
			protation_L_matrix[3] = +1.0*std::cos(str_para.img_L_rotation*EYS_PI / 180.0);

			protation_R_matrix[0] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[1] = -1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[2] = +1.0*std::sin(str_para.img_R_rotation*EYS_PI / 180.0);
			protation_R_matrix[3] = +1.0*std::cos(str_para.img_R_rotation*EYS_PI / 180.0);

			// Local image calculation, the rows and cols are the same in L/R
			for (int i = 0; i < img_dst_rows; i++)
			{
				for (int j = 0; j < img_dst_cols; j++)
				{
					// Proc : compress
					if (str_para.spline_control_v1 == 0 &&
						str_para.spline_control_v2 == 0 &&
						str_para.spline_control_v3 == 0 &&
						str_para.spline_control_v4 == 0 &&
						str_para.spline_control_v5 == 0 &&
						str_para.spline_control_v6 == 0 &&
						str_para.spline_control_v7 == 0)
					{
						point_dst_col = j;
						point_dst_row = i;
					}
					else
					{
						Point_Barrel_Compress_CC(
							j,
							i,
							img_dst_cols,
							img_dst_rows,
							mat_compress_rows,
							mat_compress_values,
							7,
							point_dst_col,
							point_dst_row
							);
					}

					// Proc : get destination position from original position
					point_dst_compress_col = point_dst_col + min_col;
					point_dst_compress_row = point_dst_row + min_row;

					// Proc : dewarping
					eys::fisheye360::Dewarp_GetXY_CC(
						point_dst_compress_col,
						point_dst_compress_row,
						str_para.FOV,
						static_cast<int>(str_para.semi_FOV_pixels),
						static_cast<int>(str_para.lens_projection_type),
						unit_sphere_radius,
						min_col,
						max_col,
						min_row,
						max_row,
						point_src1_FOV_col,
						point_src1_FOV_row
						);

					/*
					if (point_src1_FOV_col < 0)
					{
					point_src1_FOV_col = 0;
					}
					else if (point_src1_FOV_col > img_src_FOV_cols - 1)
					{
					point_src1_FOV_col = img_src_FOV_cols - 1;
					}

					if (point_src1_FOV_row < 0)
					{
					point_src1_FOV_row = 0;
					}
					else if (point_src1_FOV_row > img_src_FOV_rows - 1)
					{
					point_src1_FOV_row = img_src_FOV_rows - 1;
					}
					*/

					// L side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_L_matrix[0] * point_dist_x +
							protation_L_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_L_matrix[2] * point_dist_x +
							protation_L_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						/*
						if (point_src3_FOV_col < 0)
						{
						point_src3_FOV_col = 0;
						}
						else if (point_src3_FOV_col > img_src_FOV_cols - 1)
						{
						point_src3_FOV_col = img_src_FOV_cols - 1;
						}

						if (point_src3_FOV_row < 0)
						{
						point_src3_FOV_row = 0;
						}
						else if (point_src3_FOV_row > img_src_FOV_rows - 1)
						{
						point_src3_FOV_row = img_src_FOV_rows - 1;
						}
						*/

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col =
							(point_src3_FOV_col - img_src_FOV_center_col) +
							str_para.img_L_src_col_center;

						point_src4_row =
							(point_src3_FOV_row - img_src_FOV_center_row) +
							str_para.img_L_src_row_center;

						/*
						if (fabs(point_src4_col - str_para.img_L_src_col_center) > str_para.semi_FOV_pixels)
						{
						point_src4_col = 0;
						}

						if (fabs(point_src4_row - str_para.img_L_src_row_center) > str_para.semi_FOV_pixels)
						{
						point_src4_row = 0;
						}
						*/

						pmap_L_cols[i*img_dst_cols + j] = (float)(point_src4_col);
						pmap_L_rows[i*img_dst_cols + j] = (float)(point_src4_row);
					}

					// R side image
					if (1)
					{
						// Proc : rotation
						point_src2_FOV_col = point_src1_FOV_col;
						point_src2_FOV_row = point_src1_FOV_row;

						point_dist_x = point_src2_FOV_col - img_src_FOV_center_col;
						point_dist_y = point_src2_FOV_row - img_src_FOV_center_row;

						point_src3_FOV_col =
							protation_R_matrix[0] * point_dist_x +
							protation_R_matrix[1] * point_dist_y +
							img_src_FOV_center_col;

						point_src3_FOV_row =
							protation_R_matrix[2] * point_dist_x +
							protation_R_matrix[3] * point_dist_y +
							img_src_FOV_center_row;

						/*
						if (point_src3_FOV_col < 0)
						{
						point_src3_FOV_col = 0;
						}
						else if (point_src3_FOV_col > img_src_FOV_cols - 1)
						{
						point_src3_FOV_col = img_src_FOV_cols - 1;
						}

						if (point_src3_FOV_row < 0)
						{
						point_src3_FOV_row = 0;
						}
						else if (point_src3_FOV_row > img_src_FOV_rows - 1)
						{
						point_src3_FOV_row = img_src_FOV_rows - 1;
						}
						*/

						// Proc : lens distortion
						if (str_para.distortion_fix_v0 == 0 &&
							str_para.distortion_fix_v1 == 0 &&
							str_para.distortion_fix_v2 == 0 &&
							str_para.distortion_fix_v3 == 0 &&
							str_para.distortion_fix_v4 == 0 &&
							str_para.distortion_fix_v5 == 0 &&
							str_para.distortion_fix_v6 == 0)
						{
						}
						else
						{
							eys::fisheye360::Lens_Distortion_Fix_CC(
								point_src3_FOV_col,
								point_src3_FOV_row,
								img_src_FOV_cols,
								img_src_FOV_rows,
								lens_distoriton_para,
								str_para.sensor_pixel_size,
								point_src3_FOV_col,
								point_src3_FOV_row
								);
						}

						// Proc : shift FOV x FOV to full source image size + shift
						point_src4_col =
							(point_src3_FOV_col - img_src_FOV_center_col) +
							str_para.img_R_src_col_center;

						point_src4_row =
							(point_src3_FOV_row - img_src_FOV_center_row) +
							str_para.img_R_src_row_center;

						/*
						if (fabs(point_src4_col - str_para.img_R_src_col_center) > str_para.semi_FOV_pixels)
						{
						point_src4_col = 0;
						}

						if (fabs(point_src4_row - str_para.img_R_src_row_center) > str_para.semi_FOV_pixels)
						{
						point_src4_row = 0;
						}
						*/

						pmap_R_cols[i*img_dst_cols + j] = (float)(point_src4_col)+str_para.img_src_cols;
						pmap_R_rows[i*img_dst_cols + j] = (float)(point_src4_row);
					}
				}
			}

			// *************************************************** //
			// STEP : start L side global image calculation        //
			// *************************************************** //
			// Set parameters
			int img_local_cols = img_dst_cols;
			int img_local_rows = img_dst_rows;

			int img_global_cols = (int)(str_para.img_dst_cols);
			int img_global_rows = (int)(str_para.img_dst_rows);

			int img_global_center_col = (int)(str_para.img_dst_cols / 2);
			int img_global_center_row = (int)(str_para.img_dst_rows / 2 + str_para.img_L_dst_shift);

			int img_local_center_col = img_local_cols / 2;
			int img_local_center_row = img_local_rows / 2;

			int point_col, point_row;

			// Set map buffer
			float *pmap_L2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_L2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
					point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
					point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
					point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
					point_row = 0;
					}
					
					pmap_L2_cols[i*img_global_cols + j] = pmap_L_cols[point_row*img_local_cols + point_col];
					pmap_L2_rows[i*img_global_cols + j] = pmap_L_rows[point_row*img_local_cols + point_col];
				}
			}

			// *************************************************** //
			// STEP : start R side global image calculation        //
			// *************************************************** //
			// Set parameters
			img_local_cols = img_dst_cols;
			img_local_rows = img_dst_rows;

			img_global_cols = (int)(str_para.img_dst_cols);
			img_global_rows = (int)(str_para.img_dst_rows);

			img_global_center_col = (int)(str_para.img_dst_cols / 2);
			img_global_center_row = (int)(str_para.img_dst_rows / 2 + str_para.img_R_dst_shift);

			img_local_center_col = img_local_cols / 2;
			img_local_center_row = img_local_rows / 2;

			point_col = 0;
			point_row = 0;

			// Set map buffer
			float *pmap_R2_cols = new float[img_global_cols*img_global_rows];
			float *pmap_R2_rows = new float[img_global_cols*img_global_rows];

			// Global image calculation
			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols; j++)
				{
					point_col = (j - img_global_center_col) + img_local_center_col;
					point_row = (i - img_global_center_row) + img_local_center_row;

					if (point_col > img_local_cols - 1)
					{
					point_col = img_local_cols - 1;
					}
					else if (point_col < 0)
					{
					point_col = 0;
					}

					if (point_row > img_local_rows - 1)
					{
					point_row = img_local_rows - 1;
					}
					else if (point_row < 0)
					{
					point_row = 0;
					}
					
					pmap_R2_cols[i*img_global_cols + j] = pmap_R_cols[point_row*img_local_cols + point_col];
					pmap_R2_rows[i*img_global_cols + j] = pmap_R_rows[point_row*img_local_cols + point_col];
				}
			}

			// *************************************************** //
			// STEP : start L/R LUT to one LUT                     //
			// *************************************************** //
			float *pmap_LR_cols = new float[img_global_cols * 2 * img_global_rows];
			float *pmap_LR_rows = new float[img_global_cols * 2 * img_global_rows];

			for (int i = 0; i < img_global_rows; i++)
			{
				for (int j = 0; j < img_global_cols * 2; j++)
				{
					if (j < img_global_cols)
					{
						pmap_LR_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
						pmap_LR_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];

						if (map_lut_cols != nullptr &&
							map_lut_rows != nullptr)
						{
							map_lut_cols[i*img_global_cols * 2 + j] = pmap_L2_cols[i*img_global_cols + j];
							map_lut_rows[i*img_global_cols * 2 + j] = pmap_L2_rows[i*img_global_cols + j];
						}
					}
					else
					{
						pmap_LR_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
						pmap_LR_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];

						if (map_lut_cols != nullptr &&
							map_lut_rows != nullptr)
						{
							map_lut_cols[i*img_global_cols * 2 + j] = pmap_R2_cols[i*img_global_cols + j - img_global_cols];
							map_lut_rows[i*img_global_cols * 2 + j] = pmap_R2_rows[i*img_global_cols + j - img_global_cols];
						}
					}
				}
			}

			// *************************************************** //
			// STEP : LUT output                                   //
			// *************************************************** //
			if (filename == nullptr)
			{
			}
			else
			{
				if (lut_type == eys::LUT_LXLYRXRY_16_3)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float p_col, p_row;
					short write_col, write_row;

					for (int i = 0; i < img_global_rows; i++)
					{
						for (int j = 0; j < img_global_cols * 2; j++)
						{
							p_col = pmap_LR_cols[i * img_global_cols * 2 + j];
							p_row = pmap_LR_rows[i * img_global_cols * 2 + j];

							if (p_col < 0)
							{
								p_col = 0;
							}
							else if (p_col >= str_para.img_src_cols * 2 - 1)
							{
								p_col = (float)(str_para.img_src_cols * 2 - 2);
							}

							if (p_row < 0)
							{
								p_row = 0;
							}
							else if (p_row >= str_para.img_src_rows - 1)
							{
								p_row = (float)(str_para.img_src_rows - 2);
							}

							write_col = (short)(float_bit_value*p_col + 0.5f);
							write_row = (short)(float_bit_value*p_row + 0.5f);

							// ************************************ // 
							// Error modify start                   //
							// ************************************ //
							if ((write_col == 0 && write_row == 0) ||
								(write_col >= str_para.img_src_cols || write_col < 0) ||
								(write_row >= str_para.img_src_rows || write_row < 0))
							{

#ifdef WIN32
								if (0)
								{
									std::cout << "------------------------------" << std::endl;
									std::cout << "cols : " << j << ", value = " << write_col << std::endl;
									std::cout << "rows : " << i << ", value = " << write_row << std::endl;
								}
#endif

#ifdef linux
								if (1)
								{
									LOGI("------------------------------------");
									LOGI("(DM ERR) Map_LUT_CC : cols : %d, value = %d", j, write_col);
									LOGI("(DM ERR) Map_LUT_CC : rows : %d, value = %d", i, write_row);
								}
#endif

								if (j > 0)
								{
									p_col = pmap_LR_cols[i * img_global_cols * 2 + j - 1];
									p_row = pmap_LR_rows[i * img_global_cols * 2 + j - 1];

									write_col = (short)(float_bit_value*p_col + 0.5f);
									write_row = (short)(float_bit_value*p_row + 0.5f);

									fseek(f, -4, SEEK_CUR);
                                    if (fwrite(&write_col, 1, 2, f) != 2)
										return -1;
                                    if (fwrite(&write_row, 1, 2, f) != 2)
										return -1;
								}

								if (j < img_global_cols * 2 - 1)
								{
									p_col = pmap_LR_cols[i * img_global_cols * 2 + j + 1];
									p_row = pmap_LR_rows[i * img_global_cols * 2 + j + 1];
								}
							}

							write_col = (short)(float_bit_value*p_col + 0.5f);
							write_row = (short)(float_bit_value*p_row + 0.5f);

							// ************************************ //
							// Error modify end                     //
							// ************************************ //

							if (fwrite(&write_col, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_3)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < img_global_rows; i++)
					{
						for (int j = 0; j < img_global_cols; j++)
						{
							p_col_L = pmap_L2_cols[i * img_global_cols + j];
							p_row_L = pmap_L2_rows[i * img_global_cols + j];
							p_col_R = pmap_R2_cols[i * img_global_cols + j] - str_para.img_src_cols;
							p_row_R = pmap_R2_rows[i * img_global_cols + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols * 2 - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols * 2 - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							if (fwrite(&write_col_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_col_R, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_R, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_3_BIT0)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}

					const int float_bit_value = 8;  // Bit value for float, 2^3 = 8 for 3bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < img_global_rows; i++)
					{
						for (int j = 0; j < img_global_cols; j++)
						{
							p_col_L = pmap_L2_cols[i * img_global_cols + j];
							p_row_L = pmap_L2_rows[i * img_global_cols + j];
							p_col_R = pmap_R2_cols[i * img_global_cols + j] - str_para.img_src_cols;
							p_row_R = pmap_R2_rows[i * img_global_cols + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols * 2 - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols * 2 - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);

							write_col_L = write_col_L >> 1;
							write_row_L = write_row_L >> 1;
							write_col_R = write_col_R >> 1;
							write_row_R = write_row_R >> 1;

							write_col_L = write_col_L << 1;
							write_row_L = write_row_L << 1;
							write_col_R = write_col_R << 1;
							write_row_R = write_row_R << 1;

							if (fwrite(&write_col_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_col_R, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_R, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}

				if (lut_type == eys::LUT_LXRXLYRY_16_5)
				{
					FILE* f;
					f = fopen(filename, "wb");

					if (!f)
					{
						return -1;
					}


					const int float_bit_value = 32; // Bit value for float, 2^5 = 32 for 5bit float
					float p_col_L, p_row_L;
					float p_col_R, p_row_R;
					short write_col_L, write_row_L;
					short write_col_R, write_row_R;

					for (int i = 0; i < img_global_rows; i++)
					{
						for (int j = 0; j < img_global_cols; j++)
						{
							p_col_L = pmap_L2_cols[i * img_global_cols + j];
							p_row_L = pmap_L2_rows[i * img_global_cols + j];
							p_col_R = pmap_R2_cols[i * img_global_cols + j] - str_para.img_src_cols;
							p_row_R = pmap_R2_rows[i * img_global_cols + j];

							if (p_col_L < 0)
							{
								p_col_L = 0;
							}
							else if (p_col_L >= str_para.img_src_cols * 2 - 1)
							{
								p_col_L = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row_L < 0)
							{
								p_row_L = 0;
							}
							else if (p_row_L >= str_para.img_src_rows - 1)
							{
								p_row_L = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_L = static_cast<short>(float_bit_value*p_col_L + 0.5f);
							write_row_L = static_cast<short>(float_bit_value*p_row_L + 0.5f);

							if (p_col_R < 0)
							{
								p_col_R = 0;
							}
							else if (p_col_R >= str_para.img_src_cols * 2 - 1)
							{
								p_col_R = static_cast<float>(str_para.img_src_cols * 2 - 2);
							}

							if (p_row_R < 0)
							{
								p_row_R = 0;
							}
							else if (p_row_R >= str_para.img_src_rows - 1)
							{
								p_row_R = static_cast<float>(str_para.img_src_rows - 2);
							}

							write_col_R = static_cast<short>(float_bit_value*p_col_R + 0.5f);
							write_row_R = static_cast<short>(float_bit_value*p_row_R + 0.5f);



							if (fwrite(&write_col_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_L, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_col_R, 1, 2, f) != 2)
								return -1;
							if (fwrite(&write_row_R, 1, 2, f) != 2)
								return -1;
						}
					}
					fclose(f);
				}
			}

			delete pmap_L_cols;
			delete pmap_L_rows;
			delete pmap_R_cols;
			delete pmap_R_rows;
			delete protation_L_matrix;
			delete protation_R_matrix;
			delete pmap_L2_cols;
			delete pmap_L2_rows;
			delete pmap_R2_cols;
			delete pmap_R2_rows;
			delete pmap_LR_cols;
			delete pmap_LR_rows;
		}
	}
	return 1;
}

int Resize_LUT_1080P_CC(
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
	)
{
	if (!map_src_cols ||
		!map_src_rows)
	{
		return -1;
	}
	
	float mag = 1920.0f / (lut_cols_src - overlay_LR_src - overlay_RL_src);
	
	int   lut_cols_mag          = static_cast<int>(lut_cols_src*mag+0.5);
	int   lut_rows_mag          = 1080;
	int   lut_cols_src_single   = lut_cols_src / 2;
	int   lut_cols_dst_single   = lut_cols_dst / 2;
	int   lut_cols_mag_single   = static_cast<int>((lut_cols_mag + 0.5) / 2.0);
	
	lut_cols_mag   = lut_cols_mag_single * 2;
	lut_cols_scale = lut_cols_mag;
    lut_rows_scale = lut_rows_mag;
	overlay_LR_dst = static_cast<int>(overlay_LR_src*mag + 0.5);
	overlay_RL_dst = lut_cols_mag - 1920 - overlay_LR_dst;

	int ptr = (lut_cols_dst / 2 - lut_cols_mag / 2) / 2;

	//////////////////////////////////////////// L cols ///////////////////////////////////////////////////////////
	float *map_src_L_cols = new float[lut_cols_src*lut_rows_src];
	float *map_dst_L_cols_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_L_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + j];	

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_L_cols,
		map_dst_L_cols_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
		}
	}
	delete map_src_L_cols;
	delete map_dst_L_cols_mag;

	//////////////////////////////////////////// L rows ////////////////////////////////////////////////////////////
	float *map_src_L_rows = new float[lut_cols_src*lut_rows_src];
	float *map_dst_L_rows_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_L_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_L_rows,
		map_dst_L_rows_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];

		}
	}


	delete map_src_L_rows;
	delete map_dst_L_rows_mag;

	/////////////////////////////////////////////////////////////// R cols  //////////////////////////////////////////
	float *map_src_R_cols = new float[lut_cols_src*lut_rows_src];
	float *map_dst_R_cols_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_R_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + lut_cols_src_single + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_R_cols,
		map_dst_R_cols_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)	
				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
		}
	}

	

	////////////////////////////////////////// R rows  ///////////////////////////////////////////////////////
	float *map_src_R_rows = new float[lut_cols_src*lut_rows_src];
	float *map_dst_R_rows_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_R_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + lut_cols_src_single + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_R_rows,
		map_dst_R_rows_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
		}
	}

	
	delete map_src_R_cols;
	delete map_dst_R_cols_mag;
	delete map_src_R_rows;
	delete map_dst_R_rows_mag;

	

	return 1;
}

int Resize_LUT_1440P_CC(
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
	)
{
	if (!map_src_cols ||
		!map_src_rows)
	{
		return -1;
	}
	
	float mag = 2560.0f / (lut_cols_src - overlay_LR_src - overlay_RL_src);
	
	int   lut_cols_mag          = static_cast<int>(lut_cols_src*mag+0.5);
	int   lut_rows_mag          = 1440;
	int   lut_cols_src_single   = lut_cols_src / 2;
	int   lut_cols_dst_single   = lut_cols_dst / 2;
	int   lut_cols_mag_single   = static_cast<int>((lut_cols_mag + 0.5) / 2.0);
	
	lut_cols_mag   = lut_cols_mag_single * 2;
	lut_cols_scale = lut_cols_mag;
	lut_rows_scale = lut_rows_mag;
	overlay_LR_dst = static_cast<int>(overlay_LR_src*mag + 0.5);
	overlay_RL_dst = lut_cols_mag - 2560 - overlay_LR_dst;

	int ptr = (lut_cols_dst / 2 - lut_cols_mag / 2) / 2;

	//////////////////////////////////////////// L cols ///////////////////////////////////////////////////////////
	float *map_src_L_cols = new float[lut_cols_src*lut_rows_src];
	float *map_dst_L_cols_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_L_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_L_cols,
		map_dst_L_cols_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
		}
	}
	delete map_src_L_cols;
	delete map_dst_L_cols_mag;

	//////////////////////////////////////////// L rows ////////////////////////////////////////////////////////////
	float *map_src_L_rows = new float[lut_cols_src*lut_rows_src];
	float *map_dst_L_rows_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_L_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_L_rows,
		map_dst_L_rows_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];

		}
	}

	delete map_src_L_rows;
	delete map_dst_L_rows_mag;

	/////////////////////////////////////////////////////////////// R cols  //////////////////////////////////////////
	float *map_src_R_cols = new float[lut_cols_src*lut_rows_src];
	float *map_dst_R_cols_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_R_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + lut_cols_src_single + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_R_cols,
		map_dst_R_cols_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
		}
	}

	delete map_src_R_cols;
	delete map_dst_R_cols_mag;

	////////////////////////////////////////// R rows  ///////////////////////////////////////////////////////
	float *map_src_R_rows = new float[lut_cols_src*lut_rows_src];
	float *map_dst_R_rows_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
		for (int j = 0; j < lut_cols_src_single; j++)
			map_src_R_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + lut_cols_src_single + j];

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		lut_rows_src,
		lut_cols_mag,
		lut_rows_mag,
		map_src_R_rows,
		map_dst_R_rows_mag);

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single];
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + j - ptr];
			else
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
		}
	}

	delete map_src_R_rows;
	delete map_dst_R_rows_mag;

	return 1;
}

#if 0
int Resize_LUT_1080P_CC(
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
	)
{
	if (!map_src_cols ||
		!map_src_rows)
	{
		return -1;
	}
	
	float mag = 1920.0f / (lut_cols_src - overlay_LR_src - overlay_RL_src);
	
	int   lut_cols_mag          = static_cast<int>(lut_cols_src*mag+0.5);
	int   lut_rows_mag          = 1080;
	int   lut_cols_src_single   = lut_cols_src / 2;
	int   lut_cols_dst_single   = lut_cols_dst / 2;
	int   lut_cols_mag_single   = static_cast<int>((lut_cols_mag + 0.5) / 2.0);
	
	lut_cols_mag   = lut_cols_mag_single * 2;
	lut_cols_scale = lut_cols_mag;
	lut_rows_scale = lut_rows_mag;
	overlay_LR_dst = static_cast<int>(overlay_LR_src*mag + 0.5);
	overlay_RL_dst = lut_cols_mag - 1920 - overlay_LR_dst;
	
	float *map_src_L_cols = new float[lut_cols_src*lut_rows_src];
	float *map_src_L_rows = new float[lut_cols_src*lut_rows_src];
	float *map_src_R_cols = new float[lut_cols_src*lut_rows_src];
	float *map_src_R_rows = new float[lut_cols_src*lut_rows_src];

	float *map_dst_L_cols_mag = new float[lut_cols_mag*lut_rows_mag];
	float *map_dst_L_rows_mag = new float[lut_cols_mag*lut_rows_mag];
	float *map_dst_R_cols_mag = new float[lut_cols_mag*lut_rows_mag];
	float *map_dst_R_rows_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
	{
		for (int j = 0; j < lut_cols_src_single; j++)
		{
			map_src_L_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + j];
			map_src_L_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + j];
			map_src_R_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + lut_cols_src_single + j];
			map_src_R_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + lut_cols_src_single + j];
		}
	}

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_L_cols,
		                         map_dst_L_cols_mag);

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_L_rows,
		                         map_dst_L_rows_mag);

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_R_cols,
		                         map_dst_R_cols_mag);

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_R_rows,
		                         map_dst_R_rows_mag);

	int ptr = (lut_cols_dst / 2 - lut_cols_mag / 2) / 2;

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
			{
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single];
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single];

				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single];
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single];
			}
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
			{
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + j - ptr];
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + j - ptr];

				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + j - ptr];
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + j - ptr];
			}
			else
			{
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];

				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
			}
		}
	}

	delete map_src_L_cols;
	delete map_src_L_rows;
	delete map_src_R_cols;
	delete map_src_R_rows;

	delete map_dst_L_cols_mag;
	delete map_dst_L_rows_mag;
	delete map_dst_R_cols_mag;
	delete map_dst_R_rows_mag;
	
	return 1;
}

int Resize_LUT_1440P_CC(
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
	)
{
	if (!map_src_cols ||
		!map_src_rows)
	{
		return -1;
	}
	
	float mag = 2560.0f / (lut_cols_src - overlay_LR_src - overlay_RL_src);
	
	int   lut_cols_mag          = static_cast<int>(lut_cols_src*mag+0.5);
	int   lut_rows_mag          = 1440;
	int   lut_cols_src_single   = lut_cols_src / 2;
	int   lut_cols_dst_single   = lut_cols_dst / 2;
	int   lut_cols_mag_single   = static_cast<int>((lut_cols_mag + 0.5) / 2.0);
	
	lut_cols_mag   = lut_cols_mag_single * 2;
	lut_cols_scale = lut_cols_mag;
	lut_rows_scale = lut_rows_mag;
	overlay_LR_dst = static_cast<int>(overlay_LR_src*mag + 0.5);
	overlay_RL_dst = lut_cols_mag - 2560 - overlay_LR_dst;
	
	float *map_src_L_cols = new float[lut_cols_src*lut_rows_src];
	float *map_src_L_rows = new float[lut_cols_src*lut_rows_src];
	float *map_src_R_cols = new float[lut_cols_src*lut_rows_src];
	float *map_src_R_rows = new float[lut_cols_src*lut_rows_src];

	float *map_dst_L_cols_mag = new float[lut_cols_mag*lut_rows_mag];
	float *map_dst_L_rows_mag = new float[lut_cols_mag*lut_rows_mag];
	float *map_dst_R_cols_mag = new float[lut_cols_mag*lut_rows_mag];
	float *map_dst_R_rows_mag = new float[lut_cols_mag*lut_rows_mag];

	for (int i = 0; i < lut_rows_src; i++)
	{
		for (int j = 0; j < lut_cols_src_single; j++)
		{
			map_src_L_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + j];
			map_src_L_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + j];
			map_src_R_cols[i*lut_cols_src_single + j] = map_src_cols[i*lut_cols_src + lut_cols_src_single + j];
			map_src_R_rows[i*lut_cols_src_single + j] = map_src_rows[i*lut_cols_src + lut_cols_src_single + j];
		}
	}

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_L_cols,
		                         map_dst_L_cols_mag);

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_L_rows,
		                         map_dst_L_rows_mag);

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_R_cols,
		                         map_dst_R_cols_mag);

	Interpolation_2D_Bilinear_CC(lut_cols_src,
		                         lut_rows_src,
		                         lut_cols_mag,
		                         lut_rows_mag,
		                         map_src_R_rows,
		                         map_dst_R_rows_mag);

	int ptr = (lut_cols_dst / 2 - lut_cols_mag / 2) / 2;

	for (int i = 0; i < lut_rows_dst; i++)
	{
		for (int j = 0; j < lut_cols_dst_single; j++)
		{
			if (j < ptr)
			{
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single];
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single];

				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single];
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single];
			}
			else if (j >= ptr && j < ptr + lut_cols_mag_single)
			{
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + j - ptr];
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + j - ptr];

				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + j - ptr];
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + j - ptr];
			}
			else
			{
				map_dst_cols[i*lut_cols_dst + j] = map_dst_L_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
				map_dst_rows[i*lut_cols_dst + j] = map_dst_L_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];

				map_dst_cols[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_cols_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
				map_dst_rows[i*lut_cols_dst + lut_cols_dst_single + j] = map_dst_R_rows_mag[i*lut_cols_mag_single + lut_cols_mag_single - 1];
			}
		}
	}

	delete map_src_L_cols;
	delete map_src_L_rows;
	delete map_src_R_cols;
	delete map_src_R_rows;

	delete map_dst_L_cols_mag;
	delete map_dst_L_rows_mag;
	delete map_dst_R_cols_mag;
	delete map_dst_R_rows_mag;
	return 1;
}
#endif

int Buffer2Para_Bin_CC(
	unsigned char *buffer_flash,
	eys::fisheye360::ParaLUT &str_para,
	bool show_data
	)
{
	if (!buffer_flash)
	{
		return -1;
	}

	memcpy(&str_para, buffer_flash, 1024);

	if (show_data == true)
	{
#ifdef WIN32
		std::cout << "file_ID_header       : " << str_para.file_ID_header       << std::endl;
		std::cout << "file_ID_version      : " << str_para.file_ID_version      << std::endl;
		std::cout << "FOV                  : " << str_para.FOV                  << std::endl;
		std::cout << "semi_FOV_pixels      : " << str_para.semi_FOV_pixels      << std::endl;
		std::cout << "img_src_cols         : " << str_para.img_src_cols         << std::endl;
		std::cout << "img_src_rows         : " << str_para.img_src_rows         << std::endl;
		std::cout << "img_L_src_col_center : " << str_para.img_L_src_col_center << std::endl;
		std::cout << "img_L_src_row_center : " << str_para.img_L_src_row_center << std::endl;
		std::cout << "img_R_src_col_center : " << str_para.img_R_src_col_center << std::endl;
		std::cout << "img_R_src_row_center : " << str_para.img_R_src_row_center << std::endl;
		std::cout << "img_L_rotation       : " << str_para.img_L_rotation       << std::endl;
		std::cout << "img_R_rotation       : " << str_para.img_R_rotation       << std::endl;
		std::cout << "spline_control_v1    : " << str_para.spline_control_v1    << std::endl;
		std::cout << "spline_control_v2    : " << str_para.spline_control_v2    << std::endl;
		std::cout << "spline_control_v3    : " << str_para.spline_control_v3    << std::endl;
		std::cout << "spline_control_v4    : " << str_para.spline_control_v4    << std::endl;
		std::cout << "spline_control_v5    : " << str_para.spline_control_v5    << std::endl;
		std::cout << "spline_control_v6    : " << str_para.spline_control_v6    << std::endl;
		std::cout << "spline_control_v7    : " << str_para.spline_control_v7    << std::endl;
		std::cout << "img_dst_cols         : " << str_para.img_dst_cols         << std::endl;
		std::cout << "img_dst_rows         : " << str_para.img_dst_rows         << std::endl;
		std::cout << "img_L_dst_shift      : " << str_para.img_L_dst_shift      << std::endl;
		std::cout << "img_R_dst_shift      : " << str_para.img_R_dst_shift      << std::endl;
		std::cout << "img_overlay_LR       : " << str_para.img_overlay_LR       << std::endl;
		std::cout << "img_overlay_RL       : " << str_para.img_overlay_RL       << std::endl;
		std::cout << "img_stream_cols      : " << str_para.img_stream_cols      << std::endl;
		std::cout << "img_stream_rows      : " << str_para.img_stream_rows      << std::endl;
		std::cout << "video_stream_cols    : " << str_para.video_stream_cols    << std::endl;
		std::cout << "video_stream_rows    : " << str_para.video_stream_rows    << std::endl;
		std::cout << "usb_type             : " << str_para.usb_type             << std::endl;
		std::cout << "img_type             : " << str_para.img_type             << std::endl;
		std::cout << "lut_type             : " << str_para.lut_type             << std::endl;
		std::cout << "blending_type        : " << str_para.blending_type        << std::endl;
		std::cout << "overlay_ratio        : " << str_para.overlay_ratio        << std::endl;
		std::cout << "serial_number_date0  : " << str_para.serial_number_date0  << std::endl;
		std::cout << "serial_number_date1  : " << str_para.serial_number_date1  << std::endl;
		std::cout << std::fixed << std::setprecision(4) << "unit_sphere_radius   : " << str_para.unit_sphere_radius << std::endl;
		std::cout.unsetf(std::ios::fixed);
		std::cout << "min_col              : " << str_para.min_col              << std::endl;
		std::cout << "max_col              : " << str_para.max_col              << std::endl;
		std::cout << "min_row              : " << str_para.min_row              << std::endl;
		std::cout << "max_row              : " << str_para.max_row              << std::endl;
		std::cout << "AGD_LR               : " << str_para.AGD_LR               << std::endl;
		std::cout << "AGD_RL               : " << str_para.AGD_RL               << std::endl;
		std::cout << "out_img_resolution   : " << str_para.out_img_resolution << std::endl;
		std::cout << "out_lut_cols         : " << str_para.out_lut_cols         << std::endl;
		std::cout << "out_lut_rows         : " << str_para.out_lut_rows         << std::endl;
		std::cout << "out_lut_cols_eff     : " << str_para.out_lut_cols_eff     << std::endl;
		std::cout << "out_lut_rows_eff     : " << str_para.out_lut_rows_eff     << std::endl;
		std::cout << "out_img_cols         : " << str_para.out_img_cols         << std::endl;
		std::cout << "out_img_rows         : " << str_para.out_img_rows         << std::endl;
		std::cout << "out_overlay_LR       : " << str_para.out_overlay_LR       << std::endl;
		std::cout << "out_overlay_RL       : " << str_para.out_overlay_RL       << std::endl;
		std::cout << "distortion_fix_v0    : " << str_para.distortion_fix_v0    << std::endl;
		std::cout << "distortion_fix_v1    : " << str_para.distortion_fix_v1    << std::endl;
		std::cout << "distortion_fix_v2    : " << str_para.distortion_fix_v2    << std::endl;
		std::cout << "distortion_fix_v3    : " << str_para.distortion_fix_v3    << std::endl;
		std::cout << "distortion_fix_v4    : " << str_para.distortion_fix_v4    << std::endl;
		std::cout << "distortion_fix_v5    : " << str_para.distortion_fix_v5    << std::endl;
		std::cout << "distortion_fix_v6    : " << str_para.distortion_fix_v6    << std::endl;
		std::cout << "sensor_pixel_size    : " << str_para.sensor_pixel_size    << std::endl;
		std::cout << "lens_projection_type : " << str_para.lens_projection_type << std::endl;
#endif

#ifdef linux
		LOGI("file_ID_header       : %d\n", str_para.file_ID_header);
		LOGI("file_ID_version      : %d\n", str_para.file_ID_version);
		LOGI("FOV                  : %f\n", str_para.FOV);
		LOGI("semi_FOV_pixels      : %d\n", str_para.semi_FOV_pixels);
		LOGI("img_src_cols         : %d\n", str_para.img_src_cols);
		LOGI("img_src_rows         : %d\n", str_para.img_src_rows);
		LOGI("img_L_src_col_center : %f\n", str_para.img_L_src_col_center);
		LOGI("img_L_src_row_center : %f\n", str_para.img_L_src_row_center);
		LOGI("img_R_src_col_center : %f\n", str_para.img_R_src_col_center);
		LOGI("img_R_src_row_center : %f\n", str_para.img_R_src_row_center);
		LOGI("img_L_rotation       : %f\n", str_para.img_L_rotation);
		LOGI("img_R_rotation       : %f\n", str_para.img_R_rotation);
		LOGI("spline_control_v1    : %f\n", str_para.spline_control_v1);
		LOGI("spline_control_v2    : %f\n", str_para.spline_control_v2);
		LOGI("spline_control_v3    : %f\n", str_para.spline_control_v3);
		LOGI("spline_control_v4    : %f\n", str_para.spline_control_v4);
		LOGI("spline_control_v5    : %f\n", str_para.spline_control_v5);
		LOGI("spline_control_v6    : %f\n", str_para.spline_control_v6);
		LOGI("spline_control_v7    : %f\n", str_para.spline_control_v7);
		LOGI("img_dst_cols         : %d\n", str_para.img_dst_cols);
		LOGI("img_dst_rows         : %d\n", str_para.img_dst_rows);
		LOGI("img_L_dst_shift      : %d\n", str_para.img_L_dst_shift);
		LOGI("img_R_dst_shift      : %d\n", str_para.img_R_dst_shift);
		LOGI("img_overlay_LR       : %d\n", str_para.img_overlay_LR);
		LOGI("img_overlay_RL       : %d\n", str_para.img_overlay_RL);
		LOGI("img_stream_cols      : %d\n", str_para.img_stream_cols);
		LOGI("img_stream_rows      : %d\n", str_para.img_stream_rows);
		LOGI("video_stream_cols    : %d\n", str_para.video_stream_cols);
		LOGI("video_stream_rows    : %d\n", str_para.video_stream_rows);
		LOGI("usb_type             : %d\n", str_para.usb_type);
		LOGI("img_type             : %d\n", str_para.img_type);
		LOGI("lut_type             : %d\n", str_para.lut_type);
		LOGI("blending_type        : %d\n", str_para.blending_type);
		LOGI("overlay_ratio        : %f\n", str_para.overlay_ratio);
		LOGI("serial_number_date0  : %lld\n", str_para.serial_number_date0);
		LOGI("serial_number_date1  : %lld\n", str_para.serial_number_date1);
		LOGI("unit_sphere_radius   : %f\n", str_para.unit_sphere_radius);
		LOGI("min_col              : %f\n", str_para.min_col);
		LOGI("max_col              : %f\n", str_para.max_col);
		LOGI("min_row              : %f\n", str_para.min_row);
		LOGI("max_row              : %f\n", str_para.max_row);
		LOGI("AGD_LR               : %d\n", str_para.AGD_LR);
		LOGI("AGD_RL               : %d\n", str_para.AGD_RL);
		LOGI("out_img_resolution   : %d\n", str_para.out_img_resolution);
		LOGI("out_lut_cols         : %d\n", str_para.out_lut_cols);
		LOGI("out_lut_rows         : %d\n", str_para.out_lut_rows);
		LOGI("out_lut_cols_eff     : %d\n", str_para.out_lut_cols_eff);
		LOGI("out_lut_rows_eff     : %d\n", str_para.out_lut_rows_eff);
		LOGI("out_img_cols         : %d\n", str_para.out_img_cols);
		LOGI("out_img_rows         : %d\n", str_para.out_img_rows);
		LOGI("out_overlay_LR       : %d\n", str_para.out_overlay_LR);
		LOGI("out_overlay_RL       : %d\n", str_para.out_overlay_RL);
		LOGI("distortion_fix_v0    : %f\n", str_para.distortion_fix_v0);
		LOGI("distortion_fix_v1    : %f\n", str_para.distortion_fix_v1);
		LOGI("distortion_fix_v2    : %f\n", str_para.distortion_fix_v2);
		LOGI("distortion_fix_v3    : %f\n", str_para.distortion_fix_v3);
		LOGI("distortion_fix_v4    : %f\n", str_para.distortion_fix_v4);
		LOGI("distortion_fix_v5    : %f\n", str_para.distortion_fix_v5);
		LOGI("distortion_fix_v6    : %f\n", str_para.distortion_fix_v6);
		LOGI("sensor_pixel_size    : %f\n", str_para.sensor_pixel_size);
		LOGI("lens_projection_type : %f\n", str_para.lens_projection_type);
#endif
	}

	if (str_para.file_ID_header != 2230)
	{
		return -3;
	}

	if (str_para.file_ID_version == 4)
	{
		str_para.distortion_fix_v0 = 0.0;
		str_para.distortion_fix_v1 = 0.0;
		str_para.distortion_fix_v2 = 0.0;
		str_para.distortion_fix_v3 = 0.0;
		str_para.distortion_fix_v4 = 0.0;
		str_para.distortion_fix_v5 = 0.0;
		str_para.distortion_fix_v6 = 0.0;
		str_para.sensor_pixel_size = 3.75;
		str_para.lens_projection_type = eys::LENS_PROJ_STEREOGRAPHIC;

		return 1;
	}

	if (str_para.file_ID_version < 5)
	{
		return -2;
	}

	return 1;
}

int Para2Buffer_Bin_CC(eys::fisheye360::ParaLUT &str_para, unsigned char *buffer_flash)
{
	if (str_para.file_ID_version == 4)
	{
		str_para.distortion_fix_v0 = 0.0;
		str_para.distortion_fix_v1 = 0.0;
		str_para.distortion_fix_v2 = 0.0;
		str_para.distortion_fix_v3 = 0.0;
		str_para.distortion_fix_v4 = 0.0;
		str_para.distortion_fix_v5 = 0.0;
		str_para.distortion_fix_v6 = 0.0;

		str_para.sensor_pixel_size = 3.75;
		str_para.lens_projection_type = eys::LENS_PROJ_STEREOGRAPHIC;
	}
	else if (str_para.file_ID_version < 5)
	{
		return -1;
	}
	
	memcpy(buffer_flash, &str_para, 1024);
	return 1;
}

int Remap_Bilinear_RGB2RGB_CC(
	const int cols_in,
	const int rows_in,
	const int cols_out,
	const int rows_out,
	const unsigned char *lut,
	unsigned char *img_src,
	unsigned char *img_dst
	)
{	
	if (!img_src)
	{
		return -1;
	}

	int xd, yd, xs, ys;
	int fx1, fy1, fx2, fy2;
	int bps = cols_out * 3;

	WORD *pW;
	BYTE *pD0, *pD;
	BYTE *pS0, *pS1;
	BYTE v00, v01, v10, v11;

	pW = (WORD*)lut;
	pD = img_dst;

	for (yd = 0; yd < rows_out; yd++)
	{
		pD0 = pD;
		for (xd = 0; xd < cols_out; xd++)
		{
			xs  = pW[0] >> 3;
			ys  = pW[1] >> 3;

			fx1 = pW[0] & 0x07;
			fy1 = pW[1] & 0x07;
			fx2 = 8 - fx1;
			fy2 = 8 - fy1;
			pW += 2;

			// B
			pS0 = img_src + (ys*cols_in + xs) * 3;
			pS1 = (ys > rows_in - 2) ? (pS0) : (pS0 + cols_in * 3);

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[3]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[3]);
			
			pD0[0] =
				((v00 * (fx2)+v01 * fx1)*(fy2)+
				 (v10 * (fx2)+v11 * fx1)*(fy1)+32) >> 6;

			// G
			pS0 = pS0 + 1;
			pS1 = pS1 + 1;

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[3]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[3]);

			pD0[1] =
				((v00 * (fx2)+v01 * fx1)*(fy2)+
				 (v10 * (fx2)+v11 * fx1)*(fy1)+32) >> 6;

			// R
			pS0 = pS0 + 1;
			pS1 = pS1 + 1;

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[3]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[3]);

			pD0[2] =
				((v00 * (fx2)+v01 * fx1)*(fy2)+
				 (v10 * (fx2)+v11 * fx1)*(fy1)+32) >> 6;

			pD0 += 3;
		}
		pD += bps;
	}

	return 1;
}

int Remap_Bilinear_RGB2RGBA_CC(
	const int cols_in,
	const int rows_in,
	const int cols_out,
	const int rows_out,
	const unsigned char *lut,
	unsigned char *img_src,
	unsigned char *img_dst
	)
{
	if (!img_src)
	{
		return -1;
	}

	int xd, yd, xs, ys;
	int fx1, fy1, fx2, fy2;
	int bps = cols_out * 4;

	WORD *pW;
	BYTE *pD0, *pD;
	BYTE *pS0, *pS1;
	BYTE v00, v01, v10, v11;

	pW = (WORD*)lut;
	pD = img_dst;

	for (yd = 0; yd < rows_out; yd++)
	{
		pD0 = pD;
		for (xd = 0; xd < cols_out; xd++)
		{
			xs = pW[0] >> 3;
			ys = pW[1] >> 3;

			fx1 = pW[0] & 0x07;
			fy1 = pW[1] & 0x07;
			fx2 = 8 - fx1;
			fy2 = 8 - fy1;
			pW += 2;

			// B
			pS0 = img_src + (ys*cols_in + xs) * 3;
			pS1 = (ys > rows_in - 2) ? (pS0) : (pS0 + cols_in * 3);

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[3]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[3]);

			pD0[0] =
				((v00 * (fx2)+v01 * fx1)*(fy2)+
				(v10 * (fx2)+v11 * fx1)*(fy1)+32) >> 6;

			// G
			pS0 = pS0 + 1;
			pS1 = pS1 + 1;

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[3]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[3]);

			pD0[1] =
				((v00 * (fx2)+v01 * fx1)*(fy2)+
				(v10 * (fx2)+v11 * fx1)*(fy1)+32) >> 6;

			// R
			pS0 = pS0 + 1;
			pS1 = pS1 + 1;

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[3]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[3]);

			pD0[2] =
				((v00 * (fx2)+v01 * fx1)*(fy2)+
				(v10 * (fx2)+v11 * fx1)*(fy1)+32) >> 6;

			pD0 += 4;
		}
		pD += bps;
	}

	return 1;
}

int Remap_Bilinear_YUV2YUV_422_CC(
	const int cols_in,
	const int rows_in,
	const int cols_out,
	const int rows_out,
	const unsigned char *lut,
	unsigned char *img_src,
	unsigned char *img_dst
	)
{
	if (!img_src)
	{
		return -1;
	}

	if (cols_out > cols_in ||
		rows_out > rows_in)
	{
		return -2;
	}
	
	int  cx  = cols_out;  

	int  bps = cx * 2;
	int  xd, yd, xs, ys, fx, fy;
	int  v00, v01, v10, v11, v0, v1;
	WORD *pW;
    BYTE *pS0, *pS1, *pD, *pD0;

	pW = (WORD*)lut;
	pD = img_dst;

	for (yd = 0; yd < rows_out; yd++)
	{
		pD0 = pD;

		for (xd = 0; xd < cols_out; xd++)
		{
			xs = pW[0] >> 3;
			ys = pW[1] >> 3;
			fx = pW[0] & 0x07;
			fy = pW[1] & 0x07;
			pW += 2;

			pS0 = img_src + (ys*cols_in + xs) * 2;
			pS1 = (ys > rows_in - 2) ? (pS0) : (pS0 + cols_in * 2);
			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs == cols_in - 1) ? (v00) : (pS0[2]);
			v11 = (xs == cols_in - 1) ? (v10) : (pS1[2]);

			v0 = v00*(8 - fx) + v01*fx;
			v1 = v10*(8 - fx) + v11*fx;

			pD0[0] = (BYTE)((v0*(8 - fy) + v1*fy + 32) >> 6);
			pD0[1] = (((xd + xs) & 1) == 0) ? (pS0[1]) : (xs == 0) ? (pS0[3]) : (pS0[-1]);
		
			pD0 += 2;
		}
		pD += bps;
	}
	return 1;
}

int Alpha_Blending_RGB_CC(
	const int cols,
	const int rows,
	const int overlay_lr,
	const int overlay_rl,
	unsigned char *img_src,
	unsigned char *img_dst,
	const int cols_scale
	)
{
	if (!img_src)
	{
		return -1;
	}

	if (cols_scale > 0)
	{
		if (cols_scale >= cols)
		{
			return -2;
		}

		int pt_0 = (cols / 2 - cols_scale / 2) / 2;
		int ni, nj;
		int mask;
		int value_RGB;

		int img_cols = cols >> 1;
		int img_rows = rows;

		int overlay0  = overlay_lr;
		int overlay1  = overlay_rl;
		int img_cols2 = cols_scale - overlay0 - overlay1;

		BYTE* psrc = img_src;
		BYTE* pdst = img_dst;


		for (ni = 0; ni < img_rows; ni++)
		{
			for (nj = 0; nj < img_cols2; nj++)
			{
				if (nj < overlay1)
				{
					mask = (100 * nj) / overlay1;

					// B value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 0] * mask +
						psrc[ni*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + nj - pt_0) * 3 + 0] * (100 - mask);

					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = value_RGB / 100;

					// G value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 1] * mask +
						psrc[ni*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + nj - pt_0) * 3 + 1] * (100 - mask);

					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = value_RGB / 100;

					// R value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 2] * mask +
						psrc[ni*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + nj - pt_0) * 3 + 2] * (100 - mask);

					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = value_RGB / 100;
				}
				else if (nj >= overlay1 && nj < cols_scale / 2 - overlay0)
				{
					// B, G, and R values of L image
					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 0];
					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 1];
					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 2];
				}
				else if (nj >= cols_scale / 2 - overlay0 && nj < cols_scale / 2)
				{
					mask = (100 * (nj - cols_scale / 2 + overlay0)) / overlay0;
					
					// B value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 0] * (100 - mask) +
						psrc[ni*img_cols * 2 * 3 + (img_cols + (nj - cols_scale / 2 + overlay0) + pt_0) * 3 + 0] * mask;

					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = value_RGB / 100;

					// G value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 1] * (100 - mask) +
						psrc[ni*img_cols * 2 * 3 + (img_cols + (nj - cols_scale / 2 + overlay0) + pt_0) * 3 + 1] * mask;

					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = value_RGB / 100;

					// R value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + (nj + pt_0) * 3 + 2] * (100 - mask) +
						psrc[ni*img_cols * 2 * 3 + (img_cols + (nj - cols_scale / 2 + overlay0) + pt_0) * 3 + 2] * mask;

					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = value_RGB / 100;
				}
				else
				{
					// B, G, and R values of R image
					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = psrc[ni*img_cols * 2 * 3 + (nj + overlay0 + pt_0 * 3) * 3 + 0];
					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = psrc[ni*img_cols * 2 * 3 + (nj + overlay0 + pt_0 * 3) * 3 + 1];
					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = psrc[ni*img_cols * 2 * 3 + (nj + overlay0 + pt_0 * 3) * 3 + 2];
				}
			}
		}
	}
	else
	{
		int ni, nj;
		int mask;
		int value_RGB;

		int img_cols = cols >> 1;
		int img_rows = rows;

		int overlay0 = overlay_lr;
		int overlay1 = overlay_rl;
		int img_cols2 = img_cols * 2 - overlay0 - overlay1;

		BYTE* psrc = img_src;
		BYTE* pdst = img_dst;

		for (ni = 0; ni < img_rows; ni++)
		{
			for (nj = 0; nj < img_cols2; nj++)
			{
				if (nj < overlay1)
				{
					mask = (100 * nj) / overlay1;

					// B value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + nj * 3 + 0] * mask +
						psrc[ni*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + nj) * 3 + 0] * (100 - mask);

					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = value_RGB / 100;

					// G value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + nj * 3 + 1] * mask +
						psrc[ni*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + nj) * 3 + 1] * (100 - mask);

					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = value_RGB / 100;

					// R value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + nj * 3 + 2] * mask +
						psrc[ni*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + nj) * 3 + 2] * (100 - mask);

					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = value_RGB / 100;
				}
				else if (nj >= overlay1 && nj < img_cols - overlay0)
				{
					// B, G, and R values of L image
					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = psrc[ni*img_cols * 2 * 3 + nj * 3 + 0];
					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = psrc[ni*img_cols * 2 * 3 + nj * 3 + 1];
					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = psrc[ni*img_cols * 2 * 3 + nj * 3 + 2];
				}
				else if (nj >= img_cols - overlay0 && nj < img_cols)
				{
					mask = (100 * (nj - img_cols + overlay0)) / overlay0;

					// B value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + nj * 3 + 0] * (100 - mask) +
						psrc[ni*img_cols * 2 * 3 + (img_cols + overlay0 - (img_cols - nj)) * 3 + 0] * mask;


					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = value_RGB / 100;

					// G value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + nj * 3 + 1] * (100 - mask) +
						psrc[ni*img_cols * 2 * 3 + (img_cols + overlay0 - (img_cols - nj)) * 3 + 1] * mask;

					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = value_RGB / 100;


					// R value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 3 + nj * 3 + 2] * (100 - mask) +
						psrc[ni*img_cols * 2 * 3 + (img_cols + overlay0 - (img_cols - nj)) * 3 + 2] * mask;

					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = value_RGB / 100;
				}
				else
				{
					// B, G, and R values of R image
					pdst[ni*img_cols2 * 3 + nj * 3 + 0] = psrc[ni*img_cols * 2 * 3 + (nj + overlay0) * 3 + 0];
					pdst[ni*img_cols2 * 3 + nj * 3 + 1] = psrc[ni*img_cols * 2 * 3 + (nj + overlay0) * 3 + 1];
					pdst[ni*img_cols2 * 3 + nj * 3 + 2] = psrc[ni*img_cols * 2 * 3 + (nj + overlay0) * 3 + 2];
				}
			}
		}
	}
	return 1;
}

int Alpha_Blending_YUV_422_CC(
	const int cols,
	const int rows,
	const int overlay_lr,
	const int overlay_rl,
	unsigned char *img_src,
	unsigned char *img_dst
	)
{
	if (!img_src)
	{
		return -1;
	}
	
	if (overlay_lr % 2 != 0 ||
		overlay_rl % 2 != 0)
	{
		return -2;
	}
	
	int ni, nj;
	int mask;
	int value_YUV;

	int img_cols = cols >> 1;
	int img_rows = rows;

	int overlay0  = overlay_lr;
	int overlay1  = overlay_rl;
	int img_cols2 = img_cols * 2 - overlay0 - overlay1;

	BYTE* psrc = img_src;
	BYTE* pdst = img_dst;

	for (ni = 0; ni < img_rows; ni++)
	{
		for (nj = 0; nj < img_cols2; nj++)
		{
			if (nj < overlay1)
			{
				mask = (100 * nj) / overlay1;

				// Y value of mix R/L image
				value_YUV =
					psrc[ni*img_cols * 2 * 2 + nj * 2 + 0] * mask +
					psrc[ni*img_cols * 2 * 2 + (img_cols * 2 - overlay1 + nj) * 2 + 0] * (100 - mask);

				pdst[ni*img_cols2 * 2 + nj * 2 + 0] = value_YUV / 100;

				// U,V value of mix R/L image
				value_YUV =
					psrc[ni*img_cols * 2 * 2 + nj * 2 + 1] * mask +
					psrc[ni*img_cols * 2 * 2 + (img_cols * 2 - overlay1 + nj) * 2 + 1] * (100 - mask);

				pdst[ni*img_cols2 * 2 + nj * 2 + 1] = value_YUV / 100;
			}
			else if (nj >= overlay1 && nj < img_cols - overlay0)
			{
				// Y, U, and V values of L image
				pdst[ni*img_cols2 * 2 + nj * 2 + 0] = psrc[ni*img_cols * 2 * 2 + nj * 2 + 0];
				pdst[ni*img_cols2 * 2 + nj * 2 + 1] = psrc[ni*img_cols * 2 * 2 + nj * 2 + 1];
			}
			else if (nj >= img_cols - overlay0 && nj < img_cols)
			{
				mask = (100 * (nj - img_cols + overlay0)) / overlay0;

				// Y value of mix L/R image
				value_YUV =
					psrc[ni*img_cols * 2 * 2 + nj * 2 + 0] * (100 - mask) +
					psrc[ni*img_cols * 2 * 2 + (nj + overlay0) * 2 + 0] * mask;

				pdst[ni*img_cols2 * 2 + nj * 2 + 0] = value_YUV / 100;

				// U and V value of mix L/R image
				value_YUV =
					psrc[ni*img_cols * 2 * 2 + nj * 2 + 1] * (100 - mask) +
					psrc[ni*img_cols * 2 * 2 + (nj + overlay0) * 2 + 1] * mask;

				pdst[ni*img_cols2 * 2 + nj * 2 + 1] = value_YUV / 100;
			}
			else
			{
				// Y, U, and V values of R image
				pdst[ni*img_cols2 * 2 + nj * 2 + 0] = psrc[ni*img_cols * 2 * 2 + (nj + overlay0) * 2 + 0];
				pdst[ni*img_cols2 * 2 + nj * 2 + 1] = psrc[ni*img_cols * 2 * 2 + (nj + overlay0) * 2 + 1];
			}
		}
	}
	return 1;
}

int Alpha_Blending_RGBA_CC(
	const int cols,
	const int rows,
	const int overlay_lr,
	const int overlay_rl,
	unsigned char *img_src,
	unsigned char *img_dst,
	const int cols_scale
	)
{
	if (!img_src)
	{
		return -1;
	}

	if (cols_scale > 0)
	{
		if (cols_scale >= cols)
		{
			return -2;
		}
		
		int pt_0 = (cols / 2 - cols_scale / 2) / 2;
		int ni, nj;
		int mask;
		int value_RGB;

		int img_cols = cols >> 1;
		int img_rows = rows;

		int overlay0  = overlay_lr;
		int overlay1  = overlay_rl;
		int img_cols2 = cols_scale - overlay0 - overlay1;

		BYTE* psrc = img_src;
		BYTE* pdst = img_dst;

		for (ni = 0; ni < img_rows; ni++)
		{
			for (nj = 0; nj < img_cols2; nj++)
			{
				if (nj < overlay1)
				{
					mask = (100 * nj) / overlay1;

					// B value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 0] * mask +
						psrc[ni*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + nj - pt_0) * 4 + 0] * (100 - mask);

					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = value_RGB / 100;

					// G value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 1] * mask +
						psrc[ni*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + nj - pt_0) * 4 + 1] * (100 - mask);

					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = value_RGB / 100;

					// R value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 2] * mask +
						psrc[ni*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + nj - pt_0) * 4 + 2] * (100 - mask);

					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = value_RGB / 100;
				}
				else if (nj >= overlay1 && nj < cols_scale / 2 - overlay0)
				{
					// B, G, and R values of L image
					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 0];
					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 1];
					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 2];
				}
				else if (nj >= cols_scale / 2 - overlay0 && nj < cols_scale / 2)
				{
					mask = (100 * (nj - cols_scale / 2 + overlay0)) / overlay0;

					// B value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 0] * (100 - mask) +
						psrc[ni*img_cols * 2 * 4 + (img_cols + (nj - cols_scale / 2 + overlay0) + pt_0) * 4 + 0] * mask;

					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = value_RGB / 100;

					// G value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 1] * (100 - mask) +
						psrc[ni*img_cols * 2 * 4 + (img_cols + (nj - cols_scale / 2 + overlay0) + pt_0) * 4 + 1] * mask;

					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = value_RGB / 100;

					// R value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + (nj + pt_0) * 4 + 2] * (100 - mask) +
						psrc[ni*img_cols * 2 * 4 + (img_cols + (nj - cols_scale / 2 + overlay0) + pt_0) * 4 + 2] * mask;

					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = value_RGB / 100;
				}
				else
				{
					// B, G, and R values of R image
					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = psrc[ni*img_cols * 2 * 4 + (nj + overlay0 + pt_0 * 3) * 4 + 0];
					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = psrc[ni*img_cols * 2 * 4 + (nj + overlay0 + pt_0 * 3) * 4 + 1];
					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = psrc[ni*img_cols * 2 * 4 + (nj + overlay0 + pt_0 * 3) * 4 + 2];
				}
			}
		}
	}
	else
	{
		int ni, nj;
		int mask;
		int value_RGB;

		int img_cols = cols >> 1;
		int img_rows = rows;

		int overlay0 = overlay_lr;
		int overlay1 = overlay_rl;
		int img_cols2 = img_cols * 2 - overlay0 - overlay1;

		BYTE* psrc = img_src;
		BYTE* pdst = img_dst;

		for (ni = 0; ni < img_rows; ni++)
		{
			for (nj = 0; nj < img_cols2; nj++)
			{
				if (nj < overlay1)
				{
					mask = (100 * nj) / overlay1;

					// B value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + nj * 4 + 0] * mask +
						psrc[ni*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + nj) * 4 + 0] * (100 - mask);

					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = value_RGB / 100;

					// G value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + nj * 4 + 1] * mask +
						psrc[ni*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + nj) * 4 + 1] * (100 - mask);

					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = value_RGB / 100;

					// R value of mix R/L image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + nj * 4 + 2] * mask +
						psrc[ni*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + nj) * 4 + 2] * (100 - mask);

					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = value_RGB / 100;
				}
				else if (nj >= overlay1 && nj < img_cols - overlay0)
				{
					// B, G, and R values of L image
					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = psrc[ni*img_cols * 2 * 4 + nj * 4 + 0];
					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = psrc[ni*img_cols * 2 * 4 + nj * 4 + 1];
					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = psrc[ni*img_cols * 2 * 4 + nj * 4 + 2];
				}
				else if (nj >= img_cols - overlay0 && nj < img_cols)
				{
					mask = (100 * (nj - img_cols + overlay0)) / overlay0;

					// B value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + nj * 4 + 0] * (100 - mask) +
						psrc[ni*img_cols * 2 * 4 + (img_cols + overlay0 - (img_cols - nj)) * 4 + 0] * mask;


					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = value_RGB / 100;

					// G value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + nj * 4 + 1] * (100 - mask) +
						psrc[ni*img_cols * 2 * 4 + (img_cols + overlay0 - (img_cols - nj)) * 4 + 1] * mask;

					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = value_RGB / 100;


					// R value of mix L/R image
					value_RGB =
						psrc[ni*img_cols * 2 * 4 + nj * 4 + 2] * (100 - mask) +
						psrc[ni*img_cols * 2 * 4 + (img_cols + overlay0 - (img_cols - nj)) * 4 + 2] * mask;

					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = value_RGB / 100;
				}
				else
				{
					// B, G, and R values of R image
					pdst[ni*img_cols2 * 4 + nj * 4 + 0] = psrc[ni*img_cols * 2 * 4 + (nj + overlay0) * 4 + 0];
					pdst[ni*img_cols2 * 4 + nj * 4 + 1] = psrc[ni*img_cols * 2 * 4 + (nj + overlay0) * 4 + 1];
					pdst[ni*img_cols2 * 4 + nj * 4 + 2] = psrc[ni*img_cols * 2 * 4 + (nj + overlay0) * 4 + 2];
				}
			}
		}
	}

	return 1;
}

int Dewarp_GetXY_Parameters_CC(
	const double FOV,
	const int semi_FOV_pixels,
	const int lens_projection_type,
	double &spherical_radius,
	double &min_col,
	double &max_col,
	double &min_row,
	double &max_row
	)
{
	
	// STEP 1
	// Initial parameters       

	double FOV_radius = FOV*(EYS_PI / 180.0);
	int    img_cols   = 2 * semi_FOV_pixels; // Output image size of width
	int    img_rows   = 2 * semi_FOV_pixels; // Output image size of height

	double focal_length;
	
	if      (lens_projection_type == eys::LENS_PROJ_STEREOGRAPHIC){ focal_length = semi_FOV_pixels / (2.0*std::tan(FOV_radius / 4.0)); }
	else if (lens_projection_type == eys::LENS_PROJ_EQUIDISTANT)  { focal_length = semi_FOV_pixels / (FOV_radius / 2.0); }
	else if (lens_projection_type == eys::LENS_PROJ_ORTHOGRAPHIC) { focal_length = semi_FOV_pixels / std::sin(FOV_radius / 2.0); }
	else if (lens_projection_type == eys::LENS_PROJ_EQUISOLID)    { focal_length = semi_FOV_pixels / (2.0*std::sin(FOV_radius / 4.0)); }
	else{ return -2; }

	double div_cols = FOV_radius / img_cols;
	double div_rows = div_cols;
	double u, v;
	double uu, vv;
	double ro, rr;
	double phi, theta;
	double fai, sita;
	double x, y, z;

	// STEP 2
	// Convert Geographic to Rect coordinate system  
	double min_uu = 1000;
	double min_vv = 1000;
	double max_uu = 0;
	double max_vv = 0;


	// Find min_uu, min_vv, matU, matV
	for (int yh = 0; yh < img_rows; yh++)
	{
		for (int xw = 0; xw < img_cols; xw++)
		{
			u = xw - semi_FOV_pixels;  // �����y�� x
			v = semi_FOV_pixels - yh;  // �����y�� y
			ro = std::sqrt(u*u + v*v); // �b�|

			if (ro == 0.0)
			{
				phi = 0.0;
			}
			else if (u > 0)
			{
				phi = std::asin((double)(v / ro));
			}
			else
			{
				phi = EYS_PI - std::asin((double)(v / ro));

			}

			if      (lens_projection_type == eys::LENS_PROJ_STEREOGRAPHIC){ theta = 2.0*std::atan2(ro, 2 * focal_length); }
			else if (lens_projection_type == eys::LENS_PROJ_EQUIDISTANT)  { theta = ro / focal_length; }
			else if (lens_projection_type == eys::LENS_PROJ_ORTHOGRAPHIC) { theta = std::asin((double)(ro / focal_length)); }
			else if (lens_projection_type == eys::LENS_PROJ_EQUISOLID)    { theta = 2.0*std::asin((double)(ro / (2.0*focal_length))); }
			else{ return -2; }

			x = focal_length*std::sin(theta)*std::cos(phi); // ���y���y�� x
			y = focal_length*std::sin(theta)*std::sin(phi); // ���y���y�� y
			z = focal_length*std::cos(theta);               // ���y���y�� z

			rr = std::sqrt(x*x + z*z);
			sita = EYS_PI / 2.0 - std::atan2(y, rr);

			if (z > 0)
			{
				fai = std::acos((double)(x / rr));
				uu = round(fai / div_cols);
			}
			else
			{
				if (x < 0)
				{
					fai = 2.0*EYS_PI - std::acos((double)(x / rr));
					uu  = round(fai / div_cols);
				}
				else
				{
					fai = -std::acos((double)(x / rr));
					uu  = round(fai / div_cols);
				}
			}

			vv = round(sita / div_rows);

			if (min_uu > uu)
			{
				min_uu = uu;
			}

			if (max_uu < uu)
			{
				max_uu = uu;
			}

			if (min_vv > vv)
			{
				min_vv = vv;
			}

			if (max_vv < vv)
			{
				max_vv = vv;
			}
		}
	}

	spherical_radius = std::sqrt(rr*rr + y*y); // ���y�b�|
	min_col = min_uu; // Width start value after dewarping
	max_col = max_uu; // Width ended value after dewarping
	min_row = min_vv; // Height start value after dewarping
	max_row = max_vv; // Height ended value after dewarping
	
	if (min_col == 1000 ||
		min_row == 1000 ||
		max_col == 0 ||
		max_row == 0)
	{
		return -1;
	}

	return 1;
}

int Dewarp_GetXY_CC(
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
	)
{
	double FOV_radius   = FOV*(EYS_PI / 180.0);
	double focal_length;

	if      (lens_projection_type == eys::LENS_PROJ_STEREOGRAPHIC){ focal_length = semi_FOV_pixels / (2.0*std::tan(FOV_radius / 4.0)); }
	else if (lens_projection_type == eys::LENS_PROJ_EQUIDISTANT)  { focal_length = semi_FOV_pixels / (FOV_radius / 2.0); }
	else if (lens_projection_type == eys::LENS_PROJ_ORTHOGRAPHIC) { focal_length = semi_FOV_pixels / std::sin(FOV_radius / 2.0); }
	else if (lens_projection_type == eys::LENS_PROJ_EQUISOLID)    { focal_length = semi_FOV_pixels / (2.0*std::sin(FOV_radius / 4.0)); }
	else{ return -1; }

	double div_cols = FOV_radius / (2 * semi_FOV_pixels);
	double div_rows = FOV_radius / (2 * semi_FOV_pixels);

	double u, v;
	double uu, vv;
	double sita, fai;
	double rr;
	double x, y, z;
	double theta, phi, ro;

	int num_uu = (int)(max_col - min_col + 1);

	// Start calculation
	uu = dst_col;
	vv = dst_row;

	sita = vv*div_rows;
	fai  = uu*div_cols;

	rr = spherical_radius*std::cos(EYS_PI / 2.0 - sita);
	y  = rr*std::tan(EYS_PI / 2.0 - sita);

	if (fai < 0)
	{
		x = rr*std::cos(-1 * fai);
		z = -1.0*std::sqrt(rr*rr - x*x);
	}
	else if (fai > EYS_PI)
	{
		x = rr*std::cos(2.0f * EYS_PI - fai);
		z = -1.0*std::sqrt(rr*rr - x*x);
	}
	else
	{
		x = rr*std::cos(fai);
		z = +1.0*std::sqrt(rr*rr - x*x);
	}

	// Calculate : theta
	if (z / focal_length < -1)
	{
		theta = std::acos((double)-1);
	}
	else if (z / focal_length > 1)
	{
		theta = std::acos((double)1);
	}
	else
	{
		theta = std::acos((double)(z / focal_length));
	}

	// Calculate : phi
	if (y / (focal_length*std::sin(theta)) < -1)
	{
		phi = std::asin((double)-1);
	}
	else if (y / (focal_length*std::sin(theta)) > 1)
	{
		phi = std::asin((double)1);
	}
	else
	{
		phi = std::asin((double)(y / (focal_length*std::sin(theta)))); // y=f*sin(theta)*sin(phi) =>
	}
	
	// Calculate : ro
	if (abs(theta / 2.0f - EYS_PI/2.0) < 0.001) // |theta/2-pi/2| < 0.001
	{
		if (theta / 2.0f > EYS_PI / 2.0) // theta/2 > pi/2
		{
			if      (lens_projection_type == eys::LENS_PROJ_STEREOGRAPHIC){ ro = 2.0*focal_length*std::tan(theta / 2.0f + 0.001); }
			else if (lens_projection_type == eys::LENS_PROJ_EQUIDISTANT)  { ro = focal_length*(theta + 0.001); }
			else if (lens_projection_type == eys::LENS_PROJ_ORTHOGRAPHIC) { ro = focal_length*std::sin(theta + 0.001); }
			else if (lens_projection_type == eys::LENS_PROJ_EQUISOLID)    { ro = 2.0*focal_length*std::sin(theta / 2.0 + 0.001); }
			else{ return -1; }
		}
		else // theta/2 < pi/2
		{
			if      (lens_projection_type == eys::LENS_PROJ_STEREOGRAPHIC){ ro = 2.0*focal_length*std::tan(theta / 2.0f - 0.001); }
			else if (lens_projection_type == eys::LENS_PROJ_EQUIDISTANT)  { ro = focal_length*(theta - 0.001); }
			else if (lens_projection_type == eys::LENS_PROJ_ORTHOGRAPHIC) { ro = focal_length*std::sin(theta - 0.001); }
			else if (lens_projection_type == eys::LENS_PROJ_EQUISOLID)    { ro = 2.0*focal_length*std::sin(theta / 2.0 - 0.001); }
			else{ return -1; }
		}
	}
	else
	{
		if      (lens_projection_type == eys::LENS_PROJ_STEREOGRAPHIC){ ro = 2.0*focal_length*std::tan(theta / 2.0f); }
		else if (lens_projection_type == eys::LENS_PROJ_EQUIDISTANT)  { ro = focal_length*(theta); }
		else if (lens_projection_type == eys::LENS_PROJ_ORTHOGRAPHIC) { ro = focal_length*std::sin(theta); }
		else if (lens_projection_type == eys::LENS_PROJ_EQUISOLID)    { ro = 2.0*focal_length*std::sin(theta / 2.0); }
		else{ return -1; }
	}
	
	// Calculate : v
	if (phi > -1.0*EYS_PI / 2.0 &&
		phi < +1.0*EYS_PI / 2.0) // -pi/2 < phi < pi/2 , phi :0~pi,0~-pi
	{
		v = ro*std::sin(phi); // y=ro*sin(phi)
	}
	else // pi/2 <= phi <= pi or  -pi <= phi <= -pi/2 
	{
		v = ro*std::sin(EYS_PI - phi); // y=ro*sin(pi-phi) 
	}

    // Calculate : u
	if (uu - min_col <= (int)(num_uu*0.5) - 1) // u'<=length(u')/2
	{
		u = -std::sqrt(ro*ro - v*v); // x=-sqrt(ro^2-y^2)
	}
	else // u'>length(u')/2
	{
		u = +std::sqrt(ro*ro - v*v); // x^2+y^2=ro^2 => x=sqrt(ro^2-y^2)
	}

	src_col = u + semi_FOV_pixels; // x=u-u0 => u=x+u0
	src_row = semi_FOV_pixels - v; // y=-(v-v0) => v=v0-y

#ifdef WIN32
	if (0)
	{
		if ((int)src_col > 100000)
		{
			std::cout << "--------------------------------" << std::endl;
			std::cout << dst_col << std::endl;
			std::cout << dst_row << std::endl;
			std::cout << src_col << std::endl;
			std::cout << src_row << std::endl;
		}


		if ((int)src_col < -100000)
		{
			std::cout << "--------------------------------" << std::endl;
			std::cout << dst_col << std::endl;
			std::cout << dst_row << std::endl;
			std::cout << src_col << std::endl;
			std::cout << src_row << std::endl;
		}
	}
#endif

#ifdef linux
	if (0)
	{
		if ((int)src_col > 100000)
		{
			LOGI("--------------------------------");
			LOGI("(DM ERR) Dewarp_GetXY_CC : dst_col : %d", dst_col);
			LOGI("(DM ERR) Dewarp_GetXY_CC : dst_row : %d", dst_row);
			LOGI("(DM ERR) Dewarp_GetXY_CC : src_col : %d", src_col);
			LOGI("(DM ERR) Dewarp_GetXY_CC : src_row : %d", src_row);
		}


		if ((int)src_col < -100000)
		{
			LOGI("--------------------------------");
			LOGI("(DM ERR) Dewarp_GetXY_CC : dst_col : %d", dst_col);
			LOGI("(DM ERR) Dewarp_GetXY_CC : dst_row : %d", dst_row);
			LOGI("(DM ERR) Dewarp_GetXY_CC : src_col : %d", src_col);
			LOGI("(DM ERR) Dewarp_GetXY_CC : src_row : %d", src_row);
		}
	}
#endif

	return 1;
}

int Img_Flip_CC(
	const int img_cols,
	const int img_rows,
	unsigned char *img_src,
	unsigned char *img_dst
	)
{
	if (!img_src)
	{
		return -1;
	}

	unsigned char *tmp = new unsigned char[img_cols*img_rows * 3];
	memcpy(tmp, img_src, img_cols*img_rows * 3);

	for (int i = 0; i < img_rows; i++)
	{
		for (int j = 0; j < img_cols; j++)
		{
			img_dst[(img_rows - i - 1)*img_cols * 3 + j * 3 + 0] = tmp[i*img_cols * 3 + j * 3 + 0];
			img_dst[(img_rows - i - 1)*img_cols * 3 + j * 3 + 1] = tmp[i*img_cols * 3 + j * 3 + 1];
			img_dst[(img_rows - i - 1)*img_cols * 3 + j * 3 + 2] = tmp[i*img_cols * 3 + j * 3 + 2];
		}
	}

	if (!img_dst)
	{
		delete tmp;
		return -2;
	}

	delete tmp;
	return 1;
}

int Lens_Distortion_Fix_CC(
	double src_col,
	double src_row,
	int img_cols,
	int img_rows,
	const double *para,
	const double pixel_size,
	double &dst_col,
	double &dst_row
	)
{
	double r0, r1, t0;
	double cx, cy;
	double x, y;

	x  = src_col;
	y  = src_row;
	cx = img_cols / 2.0;
	cy = img_rows / 2.0;

	r0 = sqrt((x - cx)*(x - cx) + (y - cy)*(y - cy))*pixel_size / 1000.0;
	t0 = atan2(y - cy, x - cx);

	r1 = para[6] * pow(r0, 6) +
		 para[5] * pow(r0, 5) +
		 para[4] * pow(r0, 4) +
		 para[3] * pow(r0, 3) +
		 para[2] * pow(r0, 2) +
		 para[1] * r0 +
		 para[0];

	dst_col = r1*cos(t0) * 1000 / pixel_size + cx;
	dst_row = r1*sin(t0) * 1000 / pixel_size + cy;

	return 1;
}



#if 1
//#define _LARGEFILE64_SOURCE
#define eys64 off64_t
#define eyslseek64 lseek64
#define eyslog(...) LOGI(__VA_ARGS__)
#else
#define eys64 __int64
#define eyslseek64 _lseeki64
#define eyslog(...) printf(__VA_ARGS__)
#endif

int fseek_64(FILE *stream, eys64 offset, int origin)
{
	if (feof(stream)) {
		rewind(stream);
	}
	else {
		setbuf(stream, NULL);
	}

	int fd = fileno(stream);
	if (eyslseek64(fd, offset, origin) == -1)
	{
		eyslog("_lseeki64() failed with %d\n", errno);
		return errno;
	}
	return 0;
}

void Insert_tag_clean_up(FILE *fptr, unsigned char *cell, unsigned char *metadata_buf, unsigned long long *co64_offset_len)
{
	if (fptr)
		fclose(fptr);
	if (cell)
		delete cell;
	if (metadata_buf)
		free(metadata_buf);
	if (co64_offset_len)
		free(co64_offset_len);
}

int MetaData_360_Insert_CC_eYs3D(const char *filename, bool audio_in)
{
	// ************************************************************** //
	// Get file size                                                  //
	// ************************************************************** //
	if (!filename)
	{
		eyslog("Cannot get file!\n");
		return -1;
	}

	FILE *file_ptr, *file_ptr1;
	FILE *file_tmp;
	const char *tag_tmp_file = "tag_tmp.bin";

	file_ptr = file_ptr1 = fopen(filename, "rb+");

	if (!file_ptr)
	{
		eyslog("Error: Cannot get file_ptr!\n");
		return -2;
	}

	unsigned long file_size;
	unsigned long long llfile_size;
	struct stat st;

	int ret_stat = stat(filename, &st);
	if (ret_stat != 0)
	{
		eyslog("Error: Cannot get ret_stat!\n");
		return -1;
	}
	llfile_size = file_size = st.st_size;
    eyslog("file size is 0x%08x\n", llfile_size);
	// due to fseek cannot handle more than 4G file size
	// so we force back to AP that we can not handle
	if (llfile_size > 0xffffffff)
	{
		eyslog("Error: File size too large!\n");
		fclose(file_ptr);
		return -11;
	}

	file_size = (unsigned long)llfile_size;

	// ************************************************************** //
	// Open file and set variables                                    //
	// ************************************************************** //
	unsigned long tag_uuid = 0;
	unsigned long tag_moov = 0;
	unsigned long tag_trak = 0;
	unsigned long tag_trak_end = 0;
	unsigned long tag_mdia = 0;
	unsigned long tag_hdlr = 0;
	unsigned long tag_co64 = 0;

	bool write_tag_moov = true;
	bool write_tag_trak = true;
	bool write_tag_uuid = true;

	// Backwards Searching
#define moov_Hex 0x766F6F6D
#define trak_Hex 0x6B617274

#define mdia_Hex 0x6169646D
#define hdlr_Hex 0x726C6468
#define vide_Hex 0x65646976

#define co64_Hex 0x34366F63

#define uuidLength_Hex 0x1c6

	unsigned char *cell = new unsigned char[1];

	unsigned char findCell;
	unsigned long findFullCell;
	int j, l;

	unsigned long orig_moov_len = 0;
	unsigned long new_moov = 0;
	unsigned long orig_trak_len = 0;
	unsigned long new_trak = 0;

	bool trakWithVIDE = false;
	bool trakWithCO64 = false;

	unsigned long co64_offset_count = 0;
	unsigned long long *porig_co64_len = NULL;

	bool moov_at_beginning = false;
	bool parsing_order_found = false;

	// check moov at the beginning or end
	// we search 0x1000 bytes from file beginning
	// if we hit "mdat" then it means "moov" at file end or
	// we should hit "moov" that means "moov" at file beginning
	for (int k = 0; k < 0x2000; k++)
	{
		fread(cell, sizeof(unsigned char), 1, file_ptr1);
		if (cell[0] == 0x6D) // this is 'm'
		{
			unsigned long ltemp = 0L;
			fread(&ltemp, 3, 1, file_ptr1);
			if (ltemp == 0x766F6F) // this is 'oov'
			{
				moov_at_beginning = true;
				eyslog("Tag moov Position at beginning position : %d\n", k);
				parsing_order_found = true;
				break;
			}
			else if (ltemp == 0x746164) // this is 'dat'
			{
				moov_at_beginning = false;
				eyslog("Tag mdat Position at beginning position : %d (moov at end)\n", k);
				parsing_order_found = true;
				break;
			}
			else
				fseek(file_ptr1, k, SEEK_SET);
		}
	}
	if (!parsing_order_found)
	{
		eyslog("Error : can not find parsing order\n");
		Insert_tag_clean_up(file_ptr1, cell, NULL, NULL);
		return -10;
	}

	// ************************************************************** //
	// Compare file and find position of moov (6D 6F 6F 76)           //
	// ************************************************************** //
	for (unsigned long i = 10; i < file_size; i+=1)
	{
		//==========Find Tag MOOV==========
		// searching 'm' from the end of the file
		moov_at_beginning == true ? fseek_64(file_ptr, i, SEEK_SET) : fseek_64(file_ptr, -((eys64)i), SEEK_END);
		fread(&findCell, sizeof(unsigned char), 1, file_ptr);
		//eyslog("i=%lld %c\n", i, findCell);

		if (findCell == 0x6D)
		{
			// searching 'moov'
			moov_at_beginning == true ? fseek_64(file_ptr, i, SEEK_SET) : fseek_64(file_ptr, -((eys64)i), SEEK_END);
			// have to set inital value or some phone will get wrong value
			findFullCell = 0L;
			fread(&findFullCell, 4, 1, file_ptr);
			//eyslog("findFullCell:0x%x", findFullCell);

			if (findFullCell == moov_Hex)
			{
				// save the tag position(backward counting)
				tag_moov = i;
				eyslog("Tag MOOV Position: %d\n", tag_moov);

				// save 4 byte(the length of moov box) before moov tag position
				moov_at_beginning == true ? fseek_64(file_ptr, i - 4, SEEK_SET) : fseek_64(file_ptr, -((eys64)i) - 4, SEEK_END);

				for (j = 3; j >= 0; j--)
				{
					fread(cell, sizeof(unsigned char), 1, file_ptr);
					orig_moov_len += cell[0] << j * 8;
				}
				eyslog("original length of moov atom = 0x%08x\n", orig_moov_len);
				break;
			}
		}
	}

	if (tag_moov == 0)
	{
		eyslog("Error: Cannot get tag_moov!\n");
		Insert_tag_clean_up(file_ptr, cell, NULL, NULL);

		return -3;
	}

	// ************************************************************** //
	// Compare file and find position of trak (74 72 61 6B)           //
	// mdia (6D 64 69 61)											  //
	// hdlr (68 64 6C 72)											  //
	// vide (76 69 64 65)                                             //
	// co64 (63 6F 36 34) / stco (73 74 63 6F)                        //
	// ************************************************************** //
	for (unsigned long i = 10; i < file_size; i++)
	{
		//==========Find Tag TRAK==========
		// searching 't' from the end of the file
		moov_at_beginning == true ? fseek_64(file_ptr, i, SEEK_SET) : fseek_64(file_ptr, -((eys64)i), SEEK_END);
		fread(&findCell, sizeof(unsigned char), 1, file_ptr);

		if (findCell == 0x74)
		{
			// searching 'trak'
			moov_at_beginning == true ? fseek_64(file_ptr, i, SEEK_SET) : fseek_64(file_ptr, -((eys64)i), SEEK_END);
			findFullCell = 0L;
			fread(&findFullCell, 4, 1, file_ptr);

			if (findFullCell == trak_Hex)
			{
				tag_trak = i;
				eyslog("Tag TRAK Position: %d\n", tag_trak);
				moov_at_beginning == true ? fseek_64(file_ptr, i - 4, SEEK_SET) : fseek_64(file_ptr, -((eys64)i) - 4, SEEK_END);
				orig_trak_len = 0;
				for (j = 3; j >= 0; j--)
				{
					fread(cell, sizeof(unsigned char), 1, file_ptr);
					orig_trak_len += cell[0] << j * 8;
				}
				tag_trak_end = tag_trak + orig_trak_len;
				eyslog("original length of trak atom = 0x%08x\n", orig_trak_len);
				eyslog("original Tag TRAK end position: %d\n", tag_trak_end);

				//==========Find Tag MDIA==========
				for (unsigned long w = ((moov_at_beginning == true) ? tag_trak : (tag_trak - orig_trak_len));
					w < ((moov_at_beginning == true) ? (long)orig_trak_len : tag_trak);
					w++)
				{
					// searching 'm' in the trak box
					moov_at_beginning == true ? fseek_64(file_ptr, w, SEEK_SET) : fseek_64(file_ptr, -((eys64)w), SEEK_END);
					fread(&findCell, sizeof(unsigned char), 1, file_ptr);

					if (findCell == 0x6D)
					{
						// searching 'mdia'
						moov_at_beginning == true ? fseek_64(file_ptr, w, SEEK_SET) : fseek_64(file_ptr, -((eys64)w), SEEK_END);
						findFullCell = 0L;
						fread(&findFullCell, 4, 1, file_ptr);

						if (findFullCell == mdia_Hex)
						{
							tag_mdia = w;
							eyslog("Tag MDIA Position: %d\n", tag_mdia);

							//==========Find Tag HDLR==========
							for (unsigned long u = (moov_at_beginning == true) ? tag_trak : tag_trak - orig_trak_len;
								//u < ((moov_at_beginning == true) ? (long)orig_trak_len : tag_mdia);
								u < ((moov_at_beginning == true) ? (long)tag_trak_end : tag_mdia);
								u++)
							{
								// searching 'h' in the trak box
								moov_at_beginning == true ? fseek_64(file_ptr, u, SEEK_SET) : fseek_64(file_ptr, -((eys64)u), SEEK_END);
								fread(&findCell, sizeof(unsigned char), 1, file_ptr);

								if (findCell == 0x68)
								{
									// searching 'hdlr'
									moov_at_beginning == true ? fseek_64(file_ptr, u, SEEK_SET) : fseek_64(file_ptr, -((eys64)u), SEEK_END);
									findFullCell = 0L;
									fread(&findFullCell, 4, 1, file_ptr);

									if (findFullCell == hdlr_Hex)
									{
										tag_hdlr = u;
										eyslog("Tag HDLR Position: %d\n", tag_hdlr);
										trakWithVIDE = true;

										//==========Find Tag VIDE==========
										// searching 'vide' after tag_hdlr
										moov_at_beginning == true ? fseek_64(file_ptr, tag_hdlr + 12, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_hdlr) + 12, SEEK_END);
										findFullCell = 0L;
										fread(&findFullCell, 4, 1, file_ptr);
										eyslog("vide atom = 0x%08x\n", findFullCell);
										trakWithVIDE = (findFullCell == vide_Hex ? true : false);

										if (trakWithCO64) break;
									}
								}
								else if (findCell == 0x63)
								{
									// searching 'co64'
									moov_at_beginning == true ? fseek_64(file_ptr, u, SEEK_SET) : fseek_64(file_ptr, -((eys64)u), SEEK_END);
									findFullCell = 0L;
									fread(&findFullCell, 4, 1, file_ptr);
									//eyslog("findFullCell:0x%x\n", findFullCell);

									if (findFullCell == co64_Hex)
									{
										tag_co64 = u;
										eyslog("Tag CO64 Position: %d\n", tag_co64);
										trakWithCO64 = true;

										//Parsing CO64
										moov_at_beginning == true ? fseek_64(file_ptr, tag_co64 + 8, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_co64) + 8, SEEK_END);
										for (j = 3; j >= 0; j--)
										{
											fread(cell, sizeof(unsigned char), 1, file_ptr);
											co64_offset_count += cell[0] << j * 8;
										}
										eyslog("co64 offset count = %d\n", co64_offset_count);

										porig_co64_len = (unsigned long long*)calloc(co64_offset_count, sizeof(unsigned long long));
										if (porig_co64_len == NULL)
										{
											eyslog("Error: allocate co64 offsets buffer failed\n");
											return -13;
										}
										for (unsigned long k = 0; k < co64_offset_count; k++)
										{
											moov_at_beginning == true ? fseek_64(file_ptr, tag_co64 + 12 + k * 8, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_co64) + 12 + k * 8, SEEK_END);
											for (l = 7; l >= 0; l--)
											{
												fread(cell, sizeof(unsigned char), 1, file_ptr);
												*(porig_co64_len + k) += cell[0] << l * 8;
											}
											eyslog("co64 offsets[%d] = 0x%08x\n", k, *(porig_co64_len + k));
										}

										if (trakWithVIDE) break;
									}
								}
							}

							break;
						}
					}
				}

				// this trak box has vide tag
				if (trakWithVIDE)
				{
					eyslog("Tag VIDE Position: %d\n", tag_hdlr - 12);
					break;
				}

			}
		}
	}

	if (tag_trak == 0)
	{
		eyslog("Error: Cannot get tag_trak!\n");
		Insert_tag_clean_up(file_ptr, cell, NULL, NULL);

		return -4;
	}

	unsigned char temp;
	unsigned long tag_trak_temp = tag_trak;
	size_t w_size;//, r_size;


	/* allocate backup metadata buffer */
	unsigned char *originalMetadata = NULL;

	//==========Add UUID Tag==========
	// add 4 byte box length for the entire box's size
	(moov_at_beginning == true) ? tag_trak_temp -= 4 : tag_trak_temp += 4;
	// Add UUID in the bottom of trak box
	tag_uuid = ((moov_at_beginning == true) ? (tag_trak_temp + orig_trak_len) : (tag_trak_temp - orig_trak_len));
	eyslog("Tag UUID Position: %d\n", tag_uuid);

	// Copy the original metadata from uuid to the end of file
	if (moov_at_beginning)
	{
		originalMetadata = (unsigned char *)calloc(0x300000, sizeof(unsigned char));

		if (!originalMetadata)
		{
			eyslog("Error: allocate metadata buffer failed\n");
			Insert_tag_clean_up(file_ptr, cell, NULL, NULL);

			return -12;
		}

		///Add stco tag offset length for only 1 offset, TODO: for multiple Offsets
		long uuid_len = 0;
		// read original length
		fseek_64(file_ptr, tag_uuid - 4, SEEK_SET);
		for (j = 3; j >= 0; j--)
		{
			fread(cell, sizeof(unsigned char), 1, file_ptr);
			uuid_len += cell[0] << j * 8;
		}
		eyslog("original length before UUID : 0x%08x\n", uuid_len);

		// reserved metadata
		unsigned long long total_left_to_move = llfile_size - tag_uuid;
		unsigned long amount_to_move = 0x300000;

		file_tmp = fopen(tag_tmp_file, "wb");
		if (!file_tmp)
		{
			eyslog("Error: create allocate metadata file failed\n");
			return -13;
		}

		fseek_64(file_ptr, tag_uuid, SEEK_SET);

		for (;;)
		{
			if (total_left_to_move < amount_to_move)
				amount_to_move = (unsigned long)total_left_to_move;

			fread((unsigned char*)originalMetadata, 1, amount_to_move, file_ptr);
			fwrite((unsigned char*)originalMetadata, 1, amount_to_move, file_tmp);

			total_left_to_move -= amount_to_move;
			if (!total_left_to_move) break;
		}
		fclose(file_tmp);
		file_tmp = NULL;

		fread(&temp, 1, 1, file_ptr);
		if (feof(file_ptr))
			fseek_64(file_ptr, 0L, SEEK_SET);

		//rewrite updated length for stco tag
		uuid_len += uuidLength_Hex;
		for (j = 1; j <= 4; j++)
		{
			fseek_64(file_ptr, tag_uuid - j, SEEK_SET);
			temp = (unsigned char)((uuid_len >> (j - 1) * 8) & 0xff);
			w_size = fwrite(&temp, sizeof(unsigned char), 1, file_ptr);
			if (!w_size)
			{
				eyslog("fwrite() failed with write size(%d) %d\n", w_size, ferror(file_ptr));
				return -30;
			}
		}
	}
	else
	{
		originalMetadata = new unsigned char[tag_uuid + 1];
		if (!originalMetadata)
		{
			eyslog("Error: allocate metadata buffer failed\n");
			Insert_tag_clean_up(file_ptr, cell, NULL, NULL);

			return -12;
		}

		// reserved metadata
        for (unsigned long i = 0; i < tag_uuid; i++)
		{
			fseek_64(file_ptr, -((eys64)tag_uuid) + 1 + i, SEEK_END);
			fread(cell, sizeof(unsigned char), 1, file_ptr);
			originalMetadata[i] = cell[0];
		}
	}

	//==========Add MOOV Box Size==========
	new_moov = orig_moov_len + uuidLength_Hex;
	eyslog("moov atom length after spherical uuid add = 0x%08x\n", new_moov);
	if (write_tag_moov)
	{
		for (j = 1; j <= 4; j++)
		{
			moov_at_beginning == true ? fseek_64(file_ptr, tag_moov - j, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_moov) - j, SEEK_END);
			temp = (unsigned char)((new_moov >> (j - 1) * 8) & 0xff);
			w_size = fwrite(&temp, sizeof(unsigned char), 1, file_ptr);
			if (!w_size)
			{
				eyslog("fwrite() failed with write size(%d) %d\n", w_size, ferror(file_ptr));
				return -31;
			}
		}
	}

	//==========Add TRAK Box Size==========
	new_trak = orig_trak_len + uuidLength_Hex;
	eyslog("trak atom length after spherical uuid add = 0x%08x\n", new_trak);
	if (write_tag_trak)
	{
		for (j = 1; j <= 4; j++)
		{
			moov_at_beginning == true ? fseek_64(file_ptr, tag_trak - j, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_trak) - j, SEEK_END);
			temp = (unsigned char)((new_trak >> (j - 1) * 8) & 0xff);
			w_size = fwrite(&temp, sizeof(unsigned char), 1, file_ptr);
			if (!w_size)
			{
				eyslog("fwrite() failed with write size(%d) %d\n", w_size, ferror(file_ptr));
				return -32;
			}
		}
	}

	//==========Add CO64 Box Size==========
	if (trakWithCO64 && moov_at_beginning)
	{
		for (unsigned long k = 0; k < co64_offset_count; k++)
		{
			unsigned long long new_co64_len = *(porig_co64_len + k) + uuidLength_Hex;
			eyslog("co64 offset %d atom length after spherical uuid add = 0x%08x\n", k, new_co64_len);
			for (j = 1; j <= 8; j++)
			{
				moov_at_beginning == true ? fseek_64(file_ptr, tag_co64 + 20 + k * 8 - j, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_co64) + 20 + k * 8 - j, SEEK_END);
				temp = (unsigned char)((new_co64_len >> (j - 1) * 8) & 0xff);
				w_size = fwrite(&temp, sizeof(unsigned char), 1, file_ptr);
				if (!w_size)
				{
					eyslog("fwrite() failed with write size(%d) %d\n", w_size, ferror(file_ptr));
					return -33;
				}
			}
		}
	}

	// Start insert uuid
	moov_at_beginning == true ? fseek_64(file_ptr, tag_uuid, SEEK_SET) : fseek_64(file_ptr, -((eys64)tag_uuid), SEEK_END);
	if (write_tag_uuid)
	{
		cell[0] = 0x00;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x00;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x01;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0xC6;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x75;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x75;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x69;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x64;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		// Insert 360 prefix tag
		// FF  CC  82  63  f8  55  4A  93  88  14  58  7A  02  52  1F  DD  ->HEX
		// 255 204 130 099 248 085 074 147 136 020 088 122 002 082 031 221 ->DEC
		cell[0] = 0xFF;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0xCC;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x82;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x63;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0xF8;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x55;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x4A;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x93;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x88;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x14;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x58;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x7A;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x02;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x52;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0x1F;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		cell[0] = 0xDD;
		fwrite(cell, sizeof(unsigned char), 1, file_ptr);

		// Insert SPHERICAL_XML_HEADER
		unsigned char header1[] = "<?xml version=\"1.0\"?>";
		unsigned char header2[] = "<rdf:SphericalVideo\n";
		unsigned char header3[] = "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n";
		unsigned char header4[] = "xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\">";

		fwrite(header1, sizeof(unsigned char), sizeof(header1) / sizeof(header1[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(header2, sizeof(unsigned char), sizeof(header2) / sizeof(header2[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(header3, sizeof(unsigned char), sizeof(header3) / sizeof(header3[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(header4, sizeof(unsigned char), sizeof(header4) / sizeof(header4[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);

		// Insert SPHERICAL_XML_CONTENTS
		unsigned char cont1[] = "<GSpherical:Spherical>true</GSpherical:Spherical>";
		unsigned char cont2[] = "<GSpherical:Stitched>true</GSpherical:Stitched>";
		unsigned char cont3[] = "<GSpherical:StitchingSoftware>";
		unsigned char cont4[] = "Spherical Metadata Tool";
		unsigned char cont5[] = "</GSpherical:StitchingSoftware>";
		unsigned char cont6[] = "<GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>";

		fwrite(cont1, sizeof(unsigned char), sizeof(cont1) / sizeof(cont1[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(cont2, sizeof(unsigned char), sizeof(cont2) / sizeof(cont2[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(cont3, sizeof(unsigned char), sizeof(cont3) / sizeof(cont3[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(cont4, sizeof(unsigned char), sizeof(cont4) / sizeof(cont4[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(cont5, sizeof(unsigned char), sizeof(cont5) / sizeof(cont5[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);
		fwrite(cont6, sizeof(unsigned char), sizeof(cont6) / sizeof(cont6[0]), file_ptr);
		fseek_64(file_ptr, -1, SEEK_CUR);

		// Insert SPHERICAL_XML_FOOTER
		unsigned char footer[] = "</rdf:SphericalVideo>";
		fwrite(footer, sizeof(unsigned char), sizeof(footer) / sizeof(footer[0]), file_ptr);

		// Insert original metadata
		if (moov_at_beginning)
		{
			fseek_64(file_ptr, -1, SEEK_CUR);
			unsigned long long total_left_to_move = llfile_size - tag_uuid;
			unsigned long amount_to_move = 0x300000;

			file_tmp = fopen(tag_tmp_file, "rb");
			if (!file_tmp)
			{
				eyslog("Error: read allocate metadata file failed\n");
				return -14;
			}
			for (;;)
			{
				if (total_left_to_move < amount_to_move)
					amount_to_move = (unsigned long)total_left_to_move;

				fread((unsigned char*)originalMetadata, 1, amount_to_move, file_tmp);
				fwrite((unsigned char*)originalMetadata, 1, amount_to_move, file_ptr);

				total_left_to_move -= amount_to_move;
				if (!total_left_to_move) break;
			}
			fclose(file_tmp);
		}
		else
		{
			for (unsigned long i = 0; i < tag_uuid - 1; i++)
			{
				cell[0] = originalMetadata[i];
				fwrite(cell, sizeof(unsigned char), 1, file_ptr);
			}
		}
	}

	Insert_tag_clean_up(file_ptr, cell, originalMetadata, porig_co64_len);

	return 1;
}

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
	)
{
	double omit_value;
	double factor;

	double ncx = mat_cols / 2.0;
	double nxv, nyv;

	nyv = src_row;

	Spline_1D_CC(yh, pixel_omit, num_control_point, nyv, omit_value);
	factor = (mat_cols - omit_value) / mat_cols;
	nxv = (src_col - ncx) * factor + ncx;

	if (nxv < 0)
	{
		nxv = 0;
	}

	if (nxv > mat_cols - 1)
	{
		nxv = mat_cols - 1;
	}

	dst_col = nxv;
	dst_row = nyv;

	return 1;
}

int Spline_1D_CC(const double* xa, const double* ya, const int n, double x, double &y)
{
	//	PURPOSE																			 
	//  Given arrays xa[0..n - 1], ya[0..n - 1] which tabulate a function with			 
	//  xa[i] < xa[i + 1], this routine returns a linear interpolation value y at		 
	//  location x.

	int    lo = 0;     // Index just below x, or 0.
	int    hi = n - 1; // Index just above x, or n - 1.
	int    ex = 0;
	int    dir = 0;
	int    k;          // Current midpoint in binary search.
	double dx;         // Change in x (xa[hi] - xa[lo]). 

	if (n == 0)
	{
		return -1;
	}

	if (xa[n - 1] > xa[0])
	{
		dir = 0;
	}
	else
	{
		dir = 1;
	}

	// Perform a binary search through xa to find values where: 
	// 1) xa[lo] < x < xa[hi], and 2) hi - lo = 1 
	// xa[0] < xa[n-1]
	if (dir == 0)
	{
		while (hi - lo > 1)
		{
			k = (hi + lo) >> 1;

			if (xa[k] > x)
			{
				hi = k;
			}
			else
			{
				lo = k;
			}
		}

		dx = xa[hi] - xa[lo];

		if (dx == 0.0)
		{
			return -2;
		}

		y = ((xa[hi] - x) * ya[lo] + (x - xa[lo]) * ya[hi]) / dx;
	}

	// Perform a binary search through xa to find values where: 
	// 1) xa[lo] < x < xa[hi], and 2) hi - lo = 1 
	// xa[0] > xa[n-1]
	if (dir == 1)
	{
		while (hi - lo > 1)
		{
			k = (hi + lo) >> 1;

			if (xa[k] > x)
			{
				lo = k;
			}
			else
			{
				hi = k;
			}
		}

		ex = lo;
		lo = hi;
		hi = ex;

		dx = xa[hi] - xa[lo];

		if (dx == 0.0)
		{
			return -2;
		}

		y = ((xa[hi] - x) * ya[lo] + (x - xa[lo]) * ya[hi]) / dx;
	}
	return 1;
}


int Interpolation_2D_Bilinear_CC(
	const int mat_src_cols,
	const int mat_src_rows,
	const int mat_dst_cols,
	const int mat_dst_rows,
	float *mat_src,
	float *mat_dst
	)
{
	if (!mat_src)
	{
		return -1;
	}

	int xd, yd, xs, ys;
	int fx1, fy1, fx2, fy2;

	float div_col = ((mat_src_cols - 1.0f) - 0.0f) / (mat_dst_cols - 1.0f);
	float div_row = ((mat_src_rows - 1.0f) - 0.0f) / (mat_dst_rows - 1.0f);
	float p_col, p_row;
	float *pS0, *pS1;
	float v00, v01, v10, v11;

	for (yd = 0; yd < mat_dst_rows; yd++)
	{

		for (xd = 0; xd < mat_dst_cols; xd++)
		{
			p_col = xd*div_col;
			p_row = yd*div_row;

			xs = static_cast<int>(p_col);
			ys = static_cast<int>(p_row);

			fx1 = static_cast<int>(100 * (p_col - xs));
			fy1 = static_cast<int>(100 * (p_row - ys));
			fx2 = 100 - fx1;
			fy2 = 100 - fy1;

			pS0 = mat_src + (ys*mat_src_cols + xs);
			pS1 = (ys >= mat_src_rows - 1) ? (pS0) : (pS0 + mat_src_cols);

			v00 = pS0[0];
			v10 = pS1[0];
			v01 = (xs >= mat_src_cols - 1) ? (v00) : (pS0[1]);
			v11 = (xs >= mat_src_cols - 1) ? (v10) : (pS1[1]);

			mat_dst[yd*mat_dst_cols + xd] =
				((v00 * fx2 + v01 * fx1)*fy2 +
				(v10 * fx2 + v11 * fx1)*fy1) / 10000;
		}
	}

	return 1;
}

