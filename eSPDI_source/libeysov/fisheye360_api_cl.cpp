/**
  * @file  fisheye360_api_cl.cpp
  * @title API for Fisheye360 - OpenCL version
  */

/**
  * Include file
  */
#include "stdafx.h"
#include "fisheye360_api_cl.h"

/**
  * Define operation system
  */
#ifdef WIN32
#include <iostream>
#include <fstream>
#include <vector>
#include <sys\stat.h>
#include <CL\cl.h>
#endif

#ifdef linux
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>
#include <unistd.h>
#include <CL\cl.h>
#endif

/**
  * Define macro for opencl program
  */
#define KERNEL(...)#__VA_ARGS__

/**
  * OpenCL programs
  */
const char *kernel_source_code = KERNEL
(

__kernel void Remap_Bilinear_RGB2RGB(const int img_src_cols,
                                     const int img_src_rows,
                                     const int lut_cols,
                                     const int lut_rows,
                                     __global float *map_cols,
                                     __global float *map_rows,
                                     __global unsigned char *img_src,
                                     __global unsigned char *img_dst)
{
	int xd = get_global_id(0);
	int yd = get_global_id(1);

	int cols_out = get_global_size(0);
	int rows_out = get_global_size(1);

	int cols_in = img_src_cols;
	int rows_in = img_src_rows;

	float p_col = map_cols[yd*cols_out + xd];
	float p_row = map_rows[yd*cols_out + xd];

	int xs = (int)(p_col);
	int ys = (int)(p_row);

	int fx1 = 100 * (p_col - xs);
	int fy1 = 100 * (p_row - ys);
	int fx2 = 100 - fx1;
	int fy2 = 100 - fy1;

	int v00;
	int v01;
	int v10;
	int v11;

	// B value
	v00 = img_src[3 * (ys*cols_in + xs + 0) + 0];

	if (xs > img_src_cols - 2)
	{
		v01 = v00;
	}
	else
	{
		v01 = img_src[3 * (ys*cols_in + xs + 1) + 0];
	}

	if (ys > img_src_rows - 2)
	{
		v10 = v00;

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = v01;
		}

	}
	else
	{
		v10 = img_src[3 * (ys*cols_in + cols_in + xs + 0) + 0];

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = img_src[3 * (ys*cols_in + cols_in + xs + 1) + 0];
		}
	}

	img_dst[3 * (yd*cols_out + xd) + 0] = ((v00 * fx2 + v01 * fx1)*fy2 + (v10 * fx2 + v11 * fx1)*fy1) / 10000;

	// G value
	v00 = img_src[3 * (ys*cols_in + xs + 0) + 1];

	if (xs > img_src_cols - 2)
	{
		v01 = v00;
	}
	else
	{
		v01 = img_src[3 * (ys*cols_in + xs + 1) + 1];
	}

	if (ys > img_src_rows - 2)
	{
		v10 = v00;

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = v01;
		}

	}
	else
	{
		v10 = img_src[3 * (ys*cols_in + cols_in + xs + 0) + 1];

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = img_src[3 * (ys*cols_in + cols_in + xs + 1) + 1];
		}
	}

	img_dst[3 * (yd*cols_out + xd) + 1] = ((v00 * fx2 + v01 * fx1)*fy2 + (v10 * fx2 + v11 * fx1)*fy1) / 10000;

	// R value
	v00 = img_src[3 * (ys*cols_in + xs + 0) + 2];

	if (xs > img_src_cols - 2)
	{
		v01 = v00;
	}
	else
	{
		v01 = img_src[3 * (ys*cols_in + xs + 1) + 2];
	}

	if (ys > img_src_rows - 2)
	{
		v10 = v00;

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = v01;
		}

	}
	else
	{
		v10 = img_src[3 * (ys*cols_in + cols_in + xs + 0) + 2];

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = img_src[3 * (ys*cols_in + cols_in + xs + 1) + 2];
		}
	}

	img_dst[3 * (yd*cols_out + xd) + 2] = ((v00 * fx2 + v01 * fx1)*fy2 + (v10 * fx2 + v11 * fx1)*fy1) / 10000;
}

__kernel void Alpha_Blending_RGB(const int img_src_cols,
	                             const int img_src_rows,
								 const int overlay_lr,
								 const int overlay_rl,
								 __global unsigned char *img_src,
								 __global unsigned char *img_dst,
								 const int cols_scale)
{
	if (cols_scale > 0)
	{
		int xd = get_global_id(0);
		int yd = get_global_id(1);

		int img_cols2 = get_global_size(0); // cols for img_dst 
		int img_rows2 = get_global_size(1); // rows for img_dst and equal to img_src_rows

		int pt_0 = (img_src_cols / 2 - cols_scale / 2) / 2;
		int mask;
		int value_RGB;

		int img_cols = img_src_cols >> 1; // cols for half size of input img_src_cols
		int img_rows = img_src_rows;      // rows for input img_src_rows

		int overlay0 = overlay_lr;
		int overlay1 = overlay_rl;

		mask = (100 * xd) / overlay1;

		if (xd < overlay1)
		{
			// B value for mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 0] * mask +
				img_src[yd*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + xd - pt_0) * 3 + 0] * (100 - mask);

			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = value_RGB / 100;

			// G value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 1] * mask +
				img_src[yd*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + xd - pt_0) * 3 + 1] * (100 - mask);

			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = value_RGB / 100;

			// R value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 2] * mask +
				img_src[yd*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + xd - pt_0) * 3 + 2] * (100 - mask);

			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = value_RGB / 100;
		}
		else if (xd >= overlay1 && xd < cols_scale / 2 - overlay0)
		{
			// B, G, and R values of L image
			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 0];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 1];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 2];
		}
		else if (xd >= cols_scale / 2 - overlay0 && xd < cols_scale / 2)
		{
			mask = (100 * (xd - cols_scale / 2 + overlay0)) / overlay0;

			// B value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 0] * (100 - mask) +
				img_src[yd*img_cols * 2 * 3 + (img_cols + (xd - cols_scale / 2 + overlay0) + pt_0) * 3 + 0] * mask;

			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = value_RGB / 100;

			// G value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 1] * (100 - mask) +
				img_src[yd*img_cols * 2 * 3 + (img_cols + (xd - cols_scale / 2 + overlay0) + pt_0) * 3 + 1] * mask;

			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = value_RGB / 100;

			// R value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + (xd + pt_0) * 3 + 2] * (100 - mask) +
				img_src[yd*img_cols * 2 * 3 + (img_cols + (xd - cols_scale / 2 + overlay0) + pt_0) * 3 + 2] * mask;

			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = value_RGB / 100;
		}
		else
		{
			// B, G, and R values of R image
			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = img_src[yd*img_cols * 2 * 3 + (xd + overlay0 + pt_0 * 3) * 3 + 0];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = img_src[yd*img_cols * 2 * 3 + (xd + overlay0 + pt_0 * 3) * 3 + 1];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = img_src[yd*img_cols * 2 * 3 + (xd + overlay0 + pt_0 * 3) * 3 + 2];
		}
	}
	else
	{
		int xd = get_global_id(0);
		int yd = get_global_id(1);

		int img_cols2 = get_global_size(0); // cols for img_dst 
		int img_rows2 = get_global_size(1); // rows for img_dst and equal to img_src_rows

		int mask;
		int value_RGB;

		int img_cols = img_src_cols >> 1; // cols for half size of input img_src_cols
		int img_rows = img_src_rows;      // rows for input img_src_rows

		int overlay0 = overlay_lr;
		int overlay1 = overlay_rl;

		mask = (100 * xd) / overlay1;

		if (xd < overlay1)
		{
			// B value for mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + xd * 3 + 0] * mask +
				img_src[yd*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + xd) * 3 + 0] * (100 - mask);

			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = value_RGB / 100;

			// G value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + xd * 3 + 1] * mask +
				img_src[yd*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + xd) * 3 + 1] * (100 - mask);

			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = value_RGB / 100;

			// R value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + xd * 3 + 2] * mask +
				img_src[yd*img_cols * 2 * 3 + (img_cols * 2 - overlay1 + xd) * 3 + 2] * (100 - mask);

			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = value_RGB / 100;
		}
		else if (xd >= overlay1 && xd < img_cols - overlay0)
		{
			// B, G, and R values of L image
			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = img_src[yd*img_cols * 2 * 3 + xd * 3 + 0];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = img_src[yd*img_cols * 2 * 3 + xd * 3 + 1];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = img_src[yd*img_cols * 2 * 3 + xd * 3 + 2];
		}
		else if (xd >= img_cols - overlay0 && xd < img_cols)
		{
			mask = (100 * (xd - img_cols + overlay0)) / overlay0;

			// B value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + xd * 3 + 0] * (100 - mask) +
				img_src[yd*img_cols * 2 * 3 + (img_cols + overlay0 - (img_cols - xd)) * 3 + 0] * mask;

			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = value_RGB / 100;

			// G value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + xd * 3 + 1] * (100 - mask) +
				img_src[yd*img_cols * 2 * 3 + (img_cols + overlay0 - (img_cols - xd)) * 3 + 1] * mask;

			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = value_RGB / 100;

			// R value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 3 + xd * 3 + 2] * (100 - mask) +
				img_src[yd*img_cols * 2 * 3 + (img_cols + overlay0 - (img_cols - xd)) * 3 + 2] * mask;

			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = value_RGB / 100;
		}
		else
		{
			// B, G, and R values of R image
			img_dst[yd*img_cols2 * 3 + xd * 3 + 0] = img_src[yd*img_cols * 2 * 3 + (xd + overlay0) * 3 + 0];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 1] = img_src[yd*img_cols * 2 * 3 + (xd + overlay0) * 3 + 1];
			img_dst[yd*img_cols2 * 3 + xd * 3 + 2] = img_src[yd*img_cols * 2 * 3 + (xd + overlay0) * 3 + 2];
		}
	}
}

__kernel void Remap_Bilinear_RGB2RGBA(const int img_src_cols,
	                                  const int img_src_rows,
	                                  const int lut_cols,
	                                  const int lut_rows,
	                                  __global float *map_cols,
	                                  __global float *map_rows,
	                                  __global unsigned char *img_src,
	                                  __global unsigned char *img_dst)
{
	int xd = get_global_id(0);  // xd in lut_cols
	int yd = get_global_id(1);  // yd in lut_rows

	int cols_out = get_global_size(0); // lut_cols
	int rows_out = get_global_size(1); // lut_rows

	int cols_in = img_src_cols;
	int rows_in = img_src_rows;

	float p_col = map_cols[yd*cols_out + xd];
	float p_row = map_rows[yd*cols_out + xd];

	int xs = (int)(p_col);
	int ys = (int)(p_row);

	int fx1 = 100 * (p_col - xs);
	int fy1 = 100 * (p_row - ys);
	int fx2 = 100 - fx1;
	int fy2 = 100 - fy1;

	int v00;
	int v01;
	int v10;
	int v11;

	// B value
	v00 = img_src[3 * (ys*cols_in + xs + 0) + 0];

	if (xs > img_src_cols - 2)
	{
		v01 = v00;
	}
	else
	{
		v01 = img_src[3 * (ys*cols_in + xs + 1) + 0];
	}

	if (ys > img_src_rows - 2)
	{
		v10 = v00;

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = v01;
		}

	}
	else
	{
		v10 = img_src[3 * (ys*cols_in + cols_in + xs + 0) + 0];

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = img_src[3 * (ys*cols_in + cols_in + xs + 1) + 0];
		}
	}

	img_dst[4 * (yd*cols_out + xd) + 0] = ((v00 * fx2 + v01 * fx1)*fy2 + (v10 * fx2 + v11 * fx1)*fy1) / 10000;

	// G value
	v00 = img_src[3 * (ys*cols_in + xs + 0) + 1];

	if (xs > img_src_cols - 2)
	{
		v01 = v00;
	}
	else
	{
		v01 = img_src[3 * (ys*cols_in + xs + 1) + 1];
	}

	if (ys > img_src_rows - 2)
	{
		v10 = v00;

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = v01;
		}

	}
	else
	{
		v10 = img_src[3 * (ys*cols_in + cols_in + xs + 0) + 1];

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = img_src[3 * (ys*cols_in + cols_in + xs + 1) + 1];
		}
	}

	img_dst[4 * (yd*cols_out + xd) + 1] = ((v00 * fx2 + v01 * fx1)*fy2 + (v10 * fx2 + v11 * fx1)*fy1) / 10000;

	// R value
	v00 = img_src[3 * (ys*cols_in + xs + 0) + 2];

	if (xs > img_src_cols - 2)
	{
		v01 = v00;
	}
	else
	{
		v01 = img_src[3 * (ys*cols_in + xs + 1) + 2];
	}

	if (ys > img_src_rows - 2)
	{
		v10 = v00;

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = v01;
		}

	}
	else
	{
		v10 = img_src[3 * (ys*cols_in + cols_in + xs + 0) + 2];

		if (xs > img_src_cols - 2)
		{
			v11 = v10;
		}
		else
		{
			v11 = img_src[3 * (ys*cols_in + cols_in + xs + 1) + 2];
		}
	}

	img_dst[4 * (yd*cols_out + xd) + 2] = ((v00 * fx2 + v01 * fx1)*fy2 + (v10 * fx2 + v11 * fx1)*fy1) / 10000;
}

__kernel void Alpha_Blending_RGBA(const int img_src_cols,
	                              const int img_src_rows,
	                              const int overlay_lr,
	                              const int overlay_rl,
	                              __global unsigned char *img_src,
	                              __global unsigned char *img_dst,
								  const int cols_scale)
{
	if (cols_scale > 0)
	{
		int xd = get_global_id(0);
		int yd = get_global_id(1);

		int img_cols2 = get_global_size(0); // cols for img_dst 
		int img_rows2 = get_global_size(1); // rows for img_dst and equal to img_src_rows

		int pt_0 = (img_src_cols / 2 - cols_scale / 2) / 2;
		int mask;
		int value_RGB;

		int img_cols = img_src_cols >> 1; // cols for half size of input img_src_cols
		int img_rows = img_src_rows;      // rows for input img_src_rows

		int overlay0  = overlay_lr;
		int overlay1  = overlay_rl;

		mask = (100 * xd) / overlay1;

		if (xd < overlay1)
		{
			// B value for mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 0] * mask +
				img_src[yd*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + xd - pt_0) * 4 + 0] * (100 - mask);

			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = value_RGB / 100;

			// G value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 1] * mask +
				img_src[yd*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + xd - pt_0) * 4 + 1] * (100 - mask);

			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = value_RGB / 100;

			// R value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 2] * mask +
				img_src[yd*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + xd - pt_0) * 4 + 2] * (100 - mask);

			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = value_RGB / 100;
		}
		else if (xd >= overlay1 && xd < cols_scale / 2 - overlay0)
		{
			// B, G, and R values of L image
			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 0];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 1];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 2];
		}
		else if (xd >= cols_scale / 2 - overlay0 && xd < cols_scale / 2)
		{
			mask = (100 * (xd - cols_scale / 2 + overlay0)) / overlay0;

			// B value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 0] * (100 - mask) +
				img_src[yd*img_cols * 2 * 4 + (img_cols + (xd - cols_scale / 2 + overlay0) + pt_0) * 4 + 0] * mask;

			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = value_RGB / 100;

			// G value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 1] * (100 - mask) +
				img_src[yd*img_cols * 2 * 4 + (img_cols + (xd - cols_scale / 2 + overlay0) + pt_0) * 4 + 1] * mask;

			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = value_RGB / 100;

			// R value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + (xd + pt_0) * 4 + 2] * (100 - mask) +
				img_src[yd*img_cols * 2 * 4 + (img_cols + (xd - cols_scale / 2 + overlay0) + pt_0) * 4 + 2] * mask;

			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = value_RGB / 100;
		}
		else
		{
			// B, G, and R values of R image
			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = img_src[yd*img_cols * 2 * 4 + (xd + overlay0 + pt_0 * 3) * 4 + 0];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = img_src[yd*img_cols * 2 * 4 + (xd + overlay0 + pt_0 * 3) * 4 + 1];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = img_src[yd*img_cols * 2 * 4 + (xd + overlay0 + pt_0 * 3) * 4 + 2];
		}
	}
	else 
	{
		int xd = get_global_id(0);
		int yd = get_global_id(1);

		int img_cols2 = get_global_size(0); // cols for img_dst 
		int img_rows2 = get_global_size(1); // rows for img_dst and equal to img_src_rows

		int mask;
		int value_RGB;

		int img_cols = img_src_cols >> 1; // cols for half size of input img_src_cols
		int img_rows = img_src_rows;      // rows for input img_src_rows

		int overlay0 = overlay_lr;
		int overlay1 = overlay_rl;

		mask = (100 * xd) / overlay1;

		if (xd < overlay1)
		{
			// B value for mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + xd * 4 + 0] * mask +
				img_src[yd*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + xd) * 4 + 0] * (100 - mask);

			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = value_RGB / 100;

			// G value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + xd * 4 + 1] * mask +
				img_src[yd*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + xd) * 4 + 1] * (100 - mask);

			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = value_RGB / 100;

			// R value of mix R/L image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + xd * 4 + 2] * mask +
				img_src[yd*img_cols * 2 * 4 + (img_cols * 2 - overlay1 + xd) * 4 + 2] * (100 - mask);

			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = value_RGB / 100;
		}
		else if (xd >= overlay1 && xd < img_cols - overlay0)
		{
			// B, G, and R values of L image
			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = img_src[yd*img_cols * 2 * 4 + xd * 4 + 0];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = img_src[yd*img_cols * 2 * 4 + xd * 4 + 1];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = img_src[yd*img_cols * 2 * 4 + xd * 4 + 2];
		}
		else if (xd >= img_cols - overlay0 && xd < img_cols)
		{
			mask = (100 * (xd - img_cols + overlay0)) / overlay0;

			// B value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + xd * 4 + 0] * (100 - mask) +
				img_src[yd*img_cols * 2 * 4 + (img_cols + overlay0 - (img_cols - xd)) * 4 + 0] * mask;

			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = value_RGB / 100;

			// G value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + xd * 4 + 1] * (100 - mask) +
				img_src[yd*img_cols * 2 * 4 + (img_cols + overlay0 - (img_cols - xd)) * 4 + 1] * mask;

			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = value_RGB / 100;

			// R value of mix L/R image
			value_RGB =
				img_src[yd*img_cols * 2 * 4 + xd * 4 + 2] * (100 - mask) +
				img_src[yd*img_cols * 2 * 4 + (img_cols + overlay0 - (img_cols - xd)) * 4 + 2] * mask;

			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = value_RGB / 100;
		}
		else
		{
			// B, G, and R values of R image
			img_dst[yd*img_cols2 * 4 + xd * 4 + 0] = img_src[yd*img_cols * 2 * 4 + (xd + overlay0) * 4 + 0];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 1] = img_src[yd*img_cols * 2 * 4 + (xd + overlay0) * 4 + 1];
			img_dst[yd*img_cols2 * 4 + xd * 4 + 2] = img_src[yd*img_cols * 2 * 4 + (xd + overlay0) * 4 + 2];
		}
	}
	
}

);

/**
  * Temp parameters
  */
namespace api
{
	static cl_uint          api_platform_all_num;
	static cl_platform_id   api_platform_select;
	static cl_context       api_context;
	static cl_device_id     api_device;
	static cl_command_queue api_queue;

	static cl_mem           api_lut_cols;
	static cl_mem           api_lut_rows;
	static cl_mem           api_img_src_rgb;
	static cl_mem           api_img_dst_dwp_rgb;
	static cl_mem           api_img_dst_dwp_rgba;
	static cl_mem           api_img_dst_bld_rgb;
	static cl_mem           api_img_dst_bld_rgba;
	static cl_kernel        api_kernel_remap_bilinear_RGB2RGB;
	static cl_kernel        api_kernel_remap_bilinear_RGB2RGBA;
	static cl_kernel        api_kernel_alpha_blending_RGB;
	static cl_kernel        api_kernel_alpha_blending_RGBA;
} // api

/**
  * Function declaration
  */
namespace eys
{
namespace fisheye360
{

int GPU_Get_Platform_Number_CL(int &platform_num)
{
	// ****************************************** //
	// Get platform number                        //
	// ****************************************** //
	cl_int err;
	err = clGetPlatformIDs(0, nullptr, &api::api_platform_all_num);

	if (err != CL_SUCCESS)
	{
		return -1;
	}

	if (api::api_platform_all_num < 1)
	{
		return -2;
	}

	platform_num = api::api_platform_all_num;
	return 1;
}

int GPU_Set_Platform_CL(int platform_select,
	                    char *platform_name,
	                    char *device_name)
{
	cl_int err;
	// ****************************************** //
	// Get platform vender                        //
	// ****************************************** //
	if (platform_select > static_cast<int>(api::api_platform_all_num - 1) ||
		platform_select < 0)
	{
		return -1;
	}

	if (api::api_platform_all_num > 0)
	{
		std::string     cl_platform_vender;
		char            cl_platform_name[100];
		cl_platform_id *cl_platform_all = new cl_platform_id[api::api_platform_all_num];

		err = clGetPlatformIDs(api::api_platform_all_num, cl_platform_all, nullptr);

		err = clGetPlatformInfo(cl_platform_all[platform_select],
			                    CL_PLATFORM_VENDOR,
			                    sizeof(cl_platform_name),
			                    cl_platform_name,
			                    nullptr);

		api::api_platform_select = cl_platform_all[platform_select];
		cl_platform_vender.assign(cl_platform_name);

		if (platform_name != nullptr)
		{
			std::strcpy(platform_name, cl_platform_name);
		}
		delete[] cl_platform_all;
	}

	// ****************************************** //
	// Create context from selected platform      //
	// ****************************************** //
	cl_context_properties cl_prop[] = { CL_CONTEXT_PLATFORM,
		                                reinterpret_cast<cl_context_properties>(api::api_platform_select),
		                                0 };

	api::api_context = clCreateContextFromType(cl_prop,
		                                       CL_DEVICE_TYPE_GPU,
		                                       nullptr,
		                                       nullptr,
		                                       nullptr);

	if (api::api_context == 0)
	{
		return -2;
	}

	// ****************************************** //
	// Get device information from context        //
	// ****************************************** //
	size_t cb;
	char   cl_device_name[100];

	clGetContextInfo(api::api_context, CL_CONTEXT_DEVICES, 0, nullptr, &cb);
	clGetContextInfo(api::api_context, CL_CONTEXT_DEVICES, cb, &api::api_device, 0);
	clGetDeviceInfo(api::api_device, CL_DEVICE_NAME, 0, nullptr, &cb);
	clGetDeviceInfo(api::api_device, CL_DEVICE_NAME, cb, cl_device_name, 0);

	if (device_name != nullptr)
	{
		std::strcpy(device_name, cl_device_name);
	}

	// ****************************************** //
	// Create command queue                       //
	// ****************************************** //
	api::api_queue = clCreateCommandQueue(api::api_context, api::api_device, 0, 0);

	if (api::api_queue == 0)
	{
		clReleaseContext(api::api_context);
		return -3;
	}

	return 1;
}

int GPU_Create_Buffer_LUT_CL(const int src_cols, const int src_rows)
{
	api::api_lut_cols =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_ONLY,
		               src_cols*src_rows*sizeof(cl_float),
		               0,
		               nullptr);

	api::api_lut_rows =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_ONLY,
		               src_cols*src_rows*sizeof(cl_float),
		               0,
		               nullptr);

	if (!api::api_lut_cols ||
		!api::api_lut_rows)
	{
		clReleaseContext(api::api_context);
		clReleaseMemObject(api::api_lut_cols);
		clReleaseMemObject(api::api_lut_rows);
		return -1;
	}
	
	return 1;
}

int GPU_Writes_Buffer_LUT_CL(const int src_cols,
	                         const int src_rows,
	                         float *map_cols,
	                         float *map_rows)
{
	if (!map_cols ||
		!map_rows)
	{
		return -1;
	}

	cl_event write_event;
	cl_int   err;

	err = clEnqueueWriteBuffer(api::api_queue,
		                       api::api_lut_cols,
		                       CL_TRUE,
		                       0,
		                       src_cols*src_rows*sizeof(cl_float),
		                       map_cols,
		                       0,
		                       0,
		                       &write_event);

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	err = clEnqueueWriteBuffer(api::api_queue,
		                       api::api_lut_rows,
		                       CL_TRUE,
		                       0,
		                       src_cols*src_rows*sizeof(cl_float),
		                       map_rows,
		                       0,
		                       0,
		                       &write_event);

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	clFlush(api::api_queue);
	err = clWaitForEvents(1, &write_event);

	if (err != CL_SUCCESS)
	{
		return -3;
	}

	return 1;
}

int GPU_Writes_Buffer_LUT_CL(const int src_cols,
	                         const int src_rows,
	                         unsigned char *lut)
{
	if (!lut)
	{
		return -1;
	}
	
	// Convert lut to float buffer
	float *map_cols = new float[src_cols*src_rows];
	float *map_rows = new float[src_cols*src_rows];

	WORD *pW;
	pW = (WORD*)lut;

	int   x_int, y_int;
	float x_float, y_float;

	for (int yd = 0; yd < src_rows; yd++)
	{
		for (int xd = 0; xd < src_cols; xd++)
		{
			x_int = static_cast<int>(pW[yd*src_cols * 2 + xd * 2 + 0]);
			y_int = static_cast<int>(pW[yd*src_cols * 2 + xd * 2 + 1]);

			x_float = static_cast<float>((x_int - 0.5f) / 8.0f);
			y_float = static_cast<float>((y_int - 0.5f) / 8.0f);

			map_cols[yd*src_cols + xd] = x_float;
			map_rows[yd*src_cols + xd] = y_float;
		}
	}

	// Write to GPU buffer
	cl_event write_event;
	cl_int   err;

	err = clEnqueueWriteBuffer(api::api_queue,
		                       api::api_lut_cols,
		                       CL_TRUE,
		                       0,
		                       src_cols*src_rows*sizeof(cl_float),
		                       map_cols,
		                       0,
		                       0,
		                       &write_event);

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	err = clEnqueueWriteBuffer(api::api_queue,
		                       api::api_lut_rows,
		                       CL_TRUE,
		                       0,
		                       src_cols*src_rows*sizeof(cl_float),
		                       map_rows,
		                       0,
		                       0,
		                       &write_event);

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	clFlush(api::api_queue);
	err = clWaitForEvents(1, &write_event);

	if (err != CL_SUCCESS)
	{
		return -3;
	}

	return 1;
}

int GPU_Writes_Buffer_Img_Source_RGB_CL(const int src_cols,
	                                    const int src_rows,
	                                    unsigned char *img_src)
{
	if (!img_src)
	{
		return -1;
	}

	cl_event write_event;
	cl_int   err;

	err = clEnqueueWriteBuffer(api::api_queue,
		                       api::api_img_src_rgb,
		                       CL_TRUE,
		                       0,
		                       src_cols*src_rows*sizeof(cl_uchar) * 3,
		                       img_src,
		                       0,
		                       0,
		                       &write_event);

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	clFlush(api::api_queue);
	err = clWaitForEvents(1, &write_event);

	if (err != CL_SUCCESS)
	{
		return -3;
	}

	return 1;
}

int GPU_Create_Buffer_Img_Source_RGB_CL(const int src_cols, const int src_rows)
{
	api::api_img_src_rgb =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_ONLY,
		               src_cols*src_rows*sizeof(cl_uchar) * 3,
		               0,
		               nullptr);

	if (!api::api_img_src_rgb)
	{
		clReleaseContext(api::api_context);
		clReleaseMemObject(api::api_img_src_rgb);
		return -1;
	}

	return 1;
}

int GPU_Create_Buffer_Img_Dewarp_RGB_CL(const int src_cols, const int src_rows)
{
	api::api_img_dst_dwp_rgb =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_WRITE,
		               src_cols*src_rows *sizeof(cl_uchar) * 3,
		               0,
		               nullptr);

	if (!api::api_img_dst_dwp_rgb)
	{
		clReleaseContext(api::api_context);
		clReleaseMemObject(api::api_img_dst_dwp_rgb);
		return -1;
	}

	return 1;
}

int GPU_Create_Buffer_Img_Dewarp_RGBA_CL(const int src_cols, const int src_rows)
{
	api::api_img_dst_dwp_rgba =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_WRITE,
		               src_cols*src_rows *sizeof(cl_uchar) * 4,
		               0,
		               nullptr);

	if (!api::api_img_dst_dwp_rgba)
	{
		clReleaseContext(api::api_context);
		clReleaseMemObject(api::api_img_dst_dwp_rgba);
		return -1;
	}

	return 1;
}

int GPU_Create_Buffer_Img_Blends_RGB_CL(const int src_cols, const int src_rows)
{
	api::api_img_dst_bld_rgb =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_WRITE,
		               src_cols*src_rows *sizeof(cl_uchar) * 3,
		               0,
		               nullptr);

	if (!api::api_img_dst_bld_rgb)
	{
		clReleaseContext(api::api_context);
		clReleaseMemObject(api::api_img_dst_bld_rgb);
		return -1;
	}

	return 1;
}

int GPU_Create_Buffer_Img_Blends_RGBA_CL(const int src_cols, const int src_rows)
{
	api::api_img_dst_bld_rgba =
		clCreateBuffer(api::api_context,
		               CL_MEM_READ_WRITE,
		               src_cols*src_rows *sizeof(cl_uchar) * 4,
		               0,
		               nullptr);

	if (!api::api_img_dst_bld_rgba)
	{
		clReleaseContext(api::api_context);
		clReleaseMemObject(api::api_img_dst_bld_rgba);
		return -1;
	}

	return 1;
}

int GPU_Load_Programs_CL()
{
	cl_int err;
	cl_int err_k1, err_k2, err_k3, err_k4;
	size_t source_size[] = { strlen(kernel_source_code) };
	cl_program func = clCreateProgramWithSource(api::api_context,
		                                        1,
		                                        &kernel_source_code,
		                                        source_size,
		                                        &err);

	err = clBuildProgram(func, 0, 0, 0, 0, 0);

	api::api_kernel_remap_bilinear_RGB2RGB  = clCreateKernel(func, "Remap_Bilinear_RGB2RGB" , &err_k1);
	api::api_kernel_alpha_blending_RGB      = clCreateKernel(func, "Alpha_Blending_RGB"     , &err_k2);
	api::api_kernel_remap_bilinear_RGB2RGBA = clCreateKernel(func, "Remap_Bilinear_RGB2RGBA", &err_k3);
	api::api_kernel_alpha_blending_RGBA     = clCreateKernel(func, "Alpha_Blending_RGBA"    , &err_k4);

	if (err_k1 != CL_SUCCESS ||
		err_k2 != CL_SUCCESS ||
		err_k3 != CL_SUCCESS ||
		err_k4 != CL_SUCCESS)
	{
		return -1;
	}

	return 1;
}

int GPU_Run_Remap_Bilinear_RGB2RGB_CL(const int img_src_cols,
	                                  const int img_src_rows,
	                                  const int lut_cols,
	                                  const int lut_rows,
	                                  unsigned char *img_dst)
{
	int local_x, local_y;
	cl_int err1, err2, err3, err4, err5, err6, err7, err8;

	err1 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 0, sizeof(cl_int), &img_src_cols);
	err2 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 1, sizeof(cl_int), &img_src_rows);
	err3 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 2, sizeof(cl_int), &lut_cols);
	err4 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 3, sizeof(cl_int), &lut_rows);
	err5 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 4, sizeof(cl_mem), &api::api_lut_cols);
	err6 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 5, sizeof(cl_mem), &api::api_lut_rows);
	err7 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 6, sizeof(cl_mem), &api::api_img_src_rgb);
	err8 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGB, 7, sizeof(cl_mem), &api::api_img_dst_dwp_rgb);

	if (err1 != CL_SUCCESS ||
		err2 != CL_SUCCESS ||
		err3 != CL_SUCCESS ||
		err4 != CL_SUCCESS ||
		err5 != CL_SUCCESS ||
		err6 != CL_SUCCESS ||
		err7 != CL_SUCCESS ||
		err8 != CL_SUCCESS)
	{
		return -1;
	}
	
	cl_event event;
	size_t   global_threads[] = { lut_cols, lut_rows };

	if      (lut_cols % 16 == 0) local_x = 16;
	else if (lut_cols %  8 == 0) local_x = 8;
	else if (lut_cols %  4 == 0) local_x = 4;
	else if (lut_cols %  2 == 0) local_x = 2;
	else                         local_x = 1;

	if      (lut_rows % 16 == 0) local_y = 16;
	else if (lut_rows %  8 == 0) local_y = 8;
	else if (lut_rows %  4 == 0) local_y = 4;
	else if (lut_rows %  2 == 0) local_y = 2;
	else                         local_y = 1;
	size_t   local_threads[] = { local_x, local_y };

	cl_int err;
	err = clEnqueueNDRangeKernel(api::api_queue,
		                         api::api_kernel_remap_bilinear_RGB2RGB,
		                         2,
		                         nullptr,
		                         global_threads,
		                         local_threads,
		                         0,
		                         0,
		                         &event);

	clFlush(api::api_queue);
	clWaitForEvents(1, &event);

	if (img_dst != nullptr)
	{
		err = clEnqueueReadBuffer(api::api_queue, 
			                      api::api_img_dst_dwp_rgb, 
								  CL_TRUE, 
								  0, 
								  lut_cols*lut_rows*sizeof(cl_uchar) * 3, 
								  img_dst,
								  0, 
								  0,
								  0);
	}
	
	if (err != CL_SUCCESS)
	{
		return -2;
	}

	return 1;
}

int GPU_Run_Remap_Bilinear_RGB2RGBA_CL(const int img_src_cols,
	                                   const int img_src_rows,
	                                   const int lut_cols,
	                                   const int lut_rows,
	                                   unsigned char *img_dst)
{
	int local_x, local_y;
	cl_int err1, err2, err3, err4, err5, err6, err7, err8;

	err1 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 0, sizeof(cl_int), &img_src_cols);
	err2 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 1, sizeof(cl_int), &img_src_rows);
	err3 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 2, sizeof(cl_int), &lut_cols);
	err4 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 3, sizeof(cl_int), &lut_rows);
	err5 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 4, sizeof(cl_mem), &api::api_lut_cols);
	err6 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 5, sizeof(cl_mem), &api::api_lut_rows);
	err7 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 6, sizeof(cl_mem), &api::api_img_src_rgb);
	err8 = clSetKernelArg(api::api_kernel_remap_bilinear_RGB2RGBA, 7, sizeof(cl_mem), &api::api_img_dst_dwp_rgba);

	if (err1 != CL_SUCCESS ||
		err2 != CL_SUCCESS ||
		err3 != CL_SUCCESS ||
		err4 != CL_SUCCESS ||
		err5 != CL_SUCCESS ||
		err6 != CL_SUCCESS ||
		err7 != CL_SUCCESS ||
		err8 != CL_SUCCESS)
	{
		return -1;
	}


	cl_event event;
	size_t   global_threads[] = { lut_cols, lut_rows };

	if      (lut_cols % 16 == 0) local_x = 16;
	else if (lut_cols %  8 == 0) local_x = 8;
	else if (lut_cols %  4 == 0) local_x = 4;
	else if (lut_cols %  2 == 0) local_x = 2;
	else                         local_x = 1;

	if      (lut_rows % 16 == 0) local_y = 16;
	else if (lut_rows %  8 == 0) local_y = 8;
	else if (lut_rows %  4 == 0) local_y = 4;
	else if (lut_rows %  2 == 0) local_y = 2;
	else                         local_y = 1;
	size_t   local_threads[] = { local_x, local_y };

	cl_int err;
	err = clEnqueueNDRangeKernel(api::api_queue,
		                         api::api_kernel_remap_bilinear_RGB2RGBA,
		                         2,
		                         nullptr,
		                         global_threads,
		                         local_threads,
		                         0,
		                         0,
		                         &event);

	clFlush(api::api_queue);
	clWaitForEvents(1, &event);

	if (img_dst != nullptr)
	{
		err = clEnqueueReadBuffer(api::api_queue,
			                      api::api_img_dst_dwp_rgba,
			                      CL_TRUE,
			                      0,
			                      lut_cols*lut_rows*sizeof(cl_uchar) * 4,
			                      img_dst,
			                      0,
			                      0,
			                      0);
	}

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	return 1;
}

int GPU_Run_Alpha_Blending_RGB_CL(const int img_src_cols,
	                              const int img_src_rows,
	                              const int overlay_lr,
	                              const int overlay_rl,
	                              unsigned char *img_dst,
								  const int cols_scale)
{
	int img_dst_cols;

	if (cols_scale == 0)
	{
		img_dst_cols = img_src_cols - overlay_lr - overlay_rl;

	}
	else if (cols_scale > 0)
	{
		img_dst_cols = cols_scale - overlay_lr - overlay_rl;
	}
	else
	{
		return -3;
	}

	int img_dst_rows = img_src_rows;
	int local_x, local_y;

	cl_int err1, err2, err3, err4, err5, err6, err7;
	err1 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 0, sizeof(cl_int), &img_src_cols);
	err2 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 1, sizeof(cl_int), &img_src_rows);
	err3 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 2, sizeof(cl_int), &overlay_lr);
	err4 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 3, sizeof(cl_int), &overlay_rl);
	err5 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 4, sizeof(cl_mem), &api::api_img_dst_dwp_rgb);
	err6 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 5, sizeof(cl_mem), &api::api_img_dst_bld_rgb);
	err7 = clSetKernelArg(api::api_kernel_alpha_blending_RGB, 6, sizeof(cl_int), &cols_scale);

	if (err1 != CL_SUCCESS ||
		err2 != CL_SUCCESS ||
		err3 != CL_SUCCESS ||
		err4 != CL_SUCCESS ||
		err5 != CL_SUCCESS ||
		err6 != CL_SUCCESS ||
		err7 != CL_SUCCESS)
	{
		return -1;
	}

	cl_event event;
	size_t   global_threads[] = { img_dst_cols, img_dst_rows };

	if      (img_dst_cols % 16 == 0) local_x = 16;
	else if (img_dst_cols %  8 == 0) local_x = 8;
	else if (img_dst_cols %  4 == 0) local_x = 4;
	else if (img_dst_cols %  2 == 0) local_x = 2;
	else                             local_x = 1;

	if      (img_dst_rows % 16 == 0) local_y = 16;
	else if (img_dst_rows %  8 == 0) local_y = 8;
	else if (img_dst_rows %  4 == 0) local_y = 4;
	else if (img_dst_rows %  2 == 0) local_y = 2;
	else                             local_y = 1;
	size_t   local_threads[]  = { local_x, local_y };

	cl_int err;
	err = clEnqueueNDRangeKernel(api::api_queue,
		                         api::api_kernel_alpha_blending_RGB,
		                         2,
		                         nullptr,
		                         global_threads,
		                         local_threads,
		                         0,
		                         0,
		                         &event);

	clFlush(api::api_queue);
	clWaitForEvents(1, &event);

	if (img_dst != nullptr)
	{
		err = clEnqueueReadBuffer(api::api_queue,
			                      api::api_img_dst_bld_rgb,
			                      CL_TRUE,
			                      0,
			                      img_dst_cols*img_dst_rows*sizeof(cl_uchar) * 3,
			                      img_dst,
			                      0,
			                      0,
			                      0);
	}

	if (err != CL_SUCCESS)
	{
		return -2;
	}
	
	return 1;
}

int GPU_Run_Alpha_Blending_RGBA_CL(const int img_src_cols,
	                               const int img_src_rows,
	                               const int overlay_lr,
 	                               const int overlay_rl,
	                               unsigned char *img_dst,
								   const int cols_scale)
{
	int img_dst_cols;

	if (cols_scale == 0)
	{
		img_dst_cols = img_src_cols - overlay_lr - overlay_rl;

	}
	else if (cols_scale > 0)
	{
		img_dst_cols = cols_scale - overlay_lr - overlay_rl;
	}
	else
	{
		return -3;
	}
	
	int img_dst_rows = img_src_rows;
	int local_x, local_y;

	cl_int err1, err2, err3, err4, err5, err6, err7;
	err1 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 0, sizeof(cl_int), &img_src_cols);
	err2 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 1, sizeof(cl_int), &img_src_rows);
	err3 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 2, sizeof(cl_int), &overlay_lr);
	err4 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 3, sizeof(cl_int), &overlay_rl);
	err5 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 4, sizeof(cl_mem), &api::api_img_dst_dwp_rgba);
	err6 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 5, sizeof(cl_mem), &api::api_img_dst_bld_rgba);
	err7 = clSetKernelArg(api::api_kernel_alpha_blending_RGBA, 6, sizeof(cl_int), &cols_scale);

	if (err1 != CL_SUCCESS ||
		err2 != CL_SUCCESS ||
		err3 != CL_SUCCESS ||
		err4 != CL_SUCCESS ||
		err5 != CL_SUCCESS ||
		err6 != CL_SUCCESS ||
		err7 != CL_SUCCESS)
	{
		return -1;
	}

	cl_event event;
	size_t   global_threads[] = { img_dst_cols, img_dst_rows };

	if      (img_dst_cols % 16 == 0) local_x = 16;
	else if (img_dst_cols %  8 == 0) local_x = 8;
	else if (img_dst_cols %  4 == 0) local_x = 4;
	else if (img_dst_cols %  2 == 0) local_x = 2;
	else                             local_x = 1;

	if      (img_dst_rows % 16 == 0) local_y = 16;
	else if (img_dst_rows %  8 == 0) local_y = 8;
	else if (img_dst_rows %  4 == 0) local_y = 4;
	else if (img_dst_rows %  2 == 0) local_y = 2;
	else                             local_y = 1;
	size_t   local_threads[] = { local_x, local_y };

	cl_int err;
	err = clEnqueueNDRangeKernel(api::api_queue,
		                         api::api_kernel_alpha_blending_RGBA,
		                         2,
		                         nullptr,
		                         global_threads,
		                         local_threads,
		                         0,
		                         0,
		                         &event);

	clFlush(api::api_queue);
	clWaitForEvents(1, &event);

	if (img_dst != nullptr)
	{
		err = clEnqueueReadBuffer(api::api_queue,
			                      api::api_img_dst_bld_rgba,
			                      CL_TRUE,
			                      0,
			                      img_dst_cols*img_dst_rows*sizeof(cl_uchar) * 4,
			                      img_dst,
			                      0,
			                      0,
			                      0);
	}

	if (err != CL_SUCCESS)
	{
		return -2;
	}

	return 1;
}

int GPU_Close_Platform_CL()
{
	cl_int err01, err02, err03, err04, err05, err06, err07, err08, err09;
	cl_int err10, err11, err12, err13;

	err01 = clReleaseKernel(api::api_kernel_remap_bilinear_RGB2RGB);
	err02 = clReleaseKernel(api::api_kernel_alpha_blending_RGB);
	err03 = clReleaseKernel(api::api_kernel_remap_bilinear_RGB2RGBA);
	err04 = clReleaseKernel(api::api_kernel_alpha_blending_RGBA);
	err05 = clReleaseMemObject(api::api_lut_cols);
	err06 = clReleaseMemObject(api::api_lut_rows);
	err07 = clReleaseMemObject(api::api_img_src_rgb);
	err08 = clReleaseMemObject(api::api_img_dst_dwp_rgb);
	err09 = clReleaseMemObject(api::api_img_dst_bld_rgb);
	err10 = clReleaseMemObject(api::api_img_dst_dwp_rgba);
	err11 = clReleaseMemObject(api::api_img_dst_bld_rgba);
	err12 = clReleaseCommandQueue(api::api_queue);
	err13 = clReleaseContext(api::api_context);

	return 1;
}
} // fisheye360
} // eys

