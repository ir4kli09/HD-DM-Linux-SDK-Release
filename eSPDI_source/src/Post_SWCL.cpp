#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "Post_Header.h"
#include "Post_SW.h"


#define KERNEL(...)#__VA_ARGS__

const char* cl_code = KERNEL(

typedef struct TAG_POST_PAR
{
	int VERSION_ID;		//0
	int PAR_NUM;		//1
	int GPU_EN;			//2
	int RSVD3;			//3
	int RSVD4;			//4
	int HR_MODE;		//5
	int HR_CURVE_0;		//6
	int HR_CURVE_1;		//7
	int HR_CURVE_2;		//8
	int HR_CURVE_3;		//9
	int HR_CURVE_4;		//10
	int HR_CURVE_5;		//11
	int HR_CURVE_6;		//12
	int HR_CURVE_7;		//13
	int HR_CURVE_8;		//14
	int HR_CTRL;		//15
	int RSVD16;			//16
	int HF_MODE;		//17
	int RSVD5;			//18
	int RSVD6;			//19
	int DC_MODE;		//20
	int DC_CNT_THD;		//21
	int DC_GRAD_THD;	//22
	int SEG_MODE;		//23
	int SEG_THD_SUB;	//24
	int SEG_THD_SLP;	//25
	int SEG_THD_MAX;	//26
	int SEG_THD_MIN;	//27
	int SEG_FILL_MODE;	//28
	int RSVD29;			//29
	int RSVD30;			//30
	int HF2_MODE;		//31
	int RSVD32;			//32
	int RSVD33;			//33
	int GRAD_MODE;		//34
	int RSVD35;			//35
	int RSVD36;			//36
	int TEMP0_MODE;		//37
	int TEMP0_THD;		//38
	int RSVD39;			//39
	int RSVD40;			//40
	int TEMP1_MODE;		//41
	int TEMP1_LEVEL;	//42
	int TEMP1_THD;		//43
	int RSVD44;			//44
	int RSVD45;			//45
	int FC_MODE;		//46
	int FC_EDGE_THD;	//47
	int FC_AREA_THD;	//48
	int RSVD49;			//49
	int RSVD50;			//50
	int MF_MODE;		//51
	int ZM_MODE;		//52
	int RF_MODE;		//53
	int	RF_LEVEL;		//54
	int RF_BLK_SIZE;	//55

}POST_PAR;

__kernel void YUY2_Y(__global POST_PAR *pPar, const __global unsigned char * pIn, __global unsigned char *pLImg, int stride_in, int stride_out, __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	pLImg[y*stride_out + x] = pIn[y*stride_in + (x<<1)];
}

__kernel void RGB_Y(__global POST_PAR *pPar, const __global unsigned char * pIn, __global unsigned char *pLImg, int stride_in, int stride_out, __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	pLImg[y*stride_out + x] = (pIn[y*stride_in + 3*x] + (pIn[y*stride_in + 3*x + 1]<<1) + pIn[y*stride_in + 3*x + 2])>>2;
}

__kernel void RGB_Y_Flip(__global POST_PAR *pPar, const __global unsigned char * pIn, __global unsigned char *pLImg, int stride_in, int stride_out, __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int height = get_global_size(1);

	pLImg[(height-1-y)*stride_out + x] = (pIn[y*stride_in + 3*x] + (pIn[y*stride_in + 3*x + 1]<<1) + pIn[y*stride_in + 3*x + 2])>>2;
}

__kernel void Sobel(__global POST_PAR *pPar, __global unsigned char * pIn, __global unsigned char *pOut, int stride, __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);
	int height = get_global_size(1);

	int v = 1 <<  pPar->GRAD_MODE;
	int y0 = max(y-v, 0);
	int y1 = min(y+v, height-1);
	int x0 = max(x-v, 0);
	int x1 = min(x+v, width-1);

	
	int p00 = pIn[y0*stride + x0];
	int p01 = pIn[y0*stride + x];
	int p02 = pIn[y0*stride + x1];
	int p10 = pIn[y*stride + x0];
	int p12 = pIn[y*stride + x1];
	int p20 = pIn[y1*stride + x0];
	int p21 = pIn[y1*stride + x];
	int p22 = pIn[y1*stride + x1];
	
	int Hgrad, Vgrad, D1grad, D2grad;

	Hgrad =	abs(p00 - p02 + ((p10 - p12)<<1) + p20 - p22);

	Vgrad =	abs(p00  - p20 + ((p01 - p21)<<1) + p02  - p22);

	D1grad = abs(p10  - p21 + ((p00 - p22)<<1) + p01  - p12);

	D2grad = abs(p10  - p01 + ((p20 - p02)<<1) + p21  - p12);

	pOut[y*stride + x] = min(127, max(Hgrad+Vgrad, D1grad+D2grad));
}

__kernel void TemporalMed2	(__global POST_PAR *pPar, 
							 __global unsigned char* img_t2,
							 __global unsigned char* img_t3,
							 __global unsigned char* din, 
							 __global unsigned char* d_t0,
							 __global unsigned char* d_t1,
							 __global unsigned char* d_t2,
							 __global unsigned char* d_t3,
							 __global unsigned char* dout,
							 int stride,
							 __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int thd = pPar->TEMP0_THD;
	
	int d2 = d_t1[y*stride + x];
	
	if(abs(img_t2[y*stride + x] - img_t3[y*stride + x]) > thd)
	{
		dout[(y*stride + x)] = d2;
	}
	else
	{
		int d0 = din[y*stride + x];
		int d1 = d_t0[y*stride + x];
		int d3 = d_t2[y*stride + x];
		int d4 = d_t3[y*stride + x];

		int v0 = min(d0, d1);
		int v1 = max(d0, d1);
		int v3 = min(d3, d4);
		int v4 = max(d3, d4);

		int m1 = max( min( max( v0, v4), d2), min( v0, v4));

		dout[(y*stride + x)] = max( min( max( v3, m1), v1), min( v3, m1));
	}
}

__kernel void TemporalMed1	(__global POST_PAR *pPar,
							 __global unsigned char* img_t1,
							 __global unsigned char* img_t2,
							 __global unsigned char* din, 
							 __global unsigned char* d_t0,
							 __global unsigned char* d_t1,
							 __global unsigned char* dout,
							 int stride,
							 __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	int thd = pPar->TEMP0_THD;
	
	int d0 = din[y*stride + x];
	int d1 = d_t0[y*stride + x];
	int d2 = d_t1[y*stride + x];

	int out = max( min( max( d0, d1), d2), min( d0, d1));

	if(abs(img_t1[y*stride + x] - img_t2[y*stride + x]) > thd)
		dout[(y*stride + x)] = d1;
	else
		dout[(y*stride + x)] = out;
}

__kernel void TemporalIIR2	(__global POST_PAR *pPar,
							 __global unsigned char* img_in,
							 __global unsigned char* img_t,
							 __global unsigned char* din, 
							 __global unsigned char* d_t,
							 __global unsigned char* dout,
							 int stride,
							 __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);

	int dc = din[y*stride + x];
	int dt = d_t[y*stride + x];

	int x0 = max(x-2, 0);
	int x1 = max(x-1, 0);
	int x2 = min(x+1, width-1);
	int x3 = min(x+2, width-1);

	int diff[5];
	diff[0] = img_in[y*stride + x0] - img_t[y*stride + x0];
	diff[1] = img_in[y*stride + x1] - img_t[y*stride + x1];
	diff[2] = img_in[y*stride + x ] - img_t[y*stride + x ];
	diff[3] = img_in[y*stride + x2] - img_t[y*stride + x2];
	diff[4] = img_in[y*stride + x3] - img_t[y*stride + x3];

	int area_diff = (diff[0]+diff[1]+diff[2]+diff[3]+diff[4]);

	int thd = pPar->TEMP1_THD;
	int level = pPar->TEMP1_LEVEL;

	int out;

	if(abs(area_diff) > thd)
		out = dc;
	else
	{
		int min_val = min(dc, dt);
		int max_val = max(dc, dt);
		out = max_val - (((max_val - min_val)*level)>>6);
	}

	d_t[y*stride + x] = dout[(y*stride + x)] = out;
}

__kernel void Refine(__global POST_PAR *pPar, 
					 __global unsigned char* pLImg, 
					 __global unsigned char* din, 
					 __global unsigned char* dout,
					 __global const int* gauss, 
					 int stride,
					 __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);
	int height = get_global_size(1);
	int center = x + y * stride; // address of the center pixel

	int val_sum = 0;
	int wgt_sum = 0;
	int blk_size = (pPar->RF_BLK_SIZE<<1);
	for(int j = -blk_size; j <= blk_size; j+=2 ) // y-axis
	{ 
		int y1 = clamp(y+j, 0, height-1);

		for(int i = -blk_size; i <= blk_size; i+=2 ) // x-axis
		{ 
			int x1 = clamp(x+i, 0, width-1);	
			int diff = abs(pLImg[center] - pLImg[y1 * stride + x1]);

			int wgt = gauss[diff];
			int val = din[y1 * stride + x1] * wgt;

			wgt_sum += wgt;
			val_sum += val;
		}
	}

	dout[center] = val_sum / wgt_sum;
}

__kernel void HugeRemove(__global POST_PAR *pPar, __global unsigned char* pLImg, __global unsigned char* din, __global unsigned char* dout, __global int *debug)
{
}

__kernel void Median(__global POST_PAR *pPar, __global unsigned char* InputBuf, __global unsigned char* OutputBuf, int stride, __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);
	int height = get_global_size(1);

	int x0, x1, y0, y1;
	int a[9], b[9];
	int v_max, v_min;
	x0 = max(x-1, 0);
	x1 = min(x+1, width-1);
	y0 = max(y-1, 0);
	y1 = min(y+1, height-1);

	a[0] = InputBuf[y0*stride + x0];
	a[1] = InputBuf[y0*stride + x ];
	a[2] = InputBuf[y0*stride + x1];
	a[3] = InputBuf[y *stride + x0];
	a[4] = InputBuf[y *stride + x ];
	a[5] = InputBuf[y *stride + x1];
	a[6] = InputBuf[y1*stride + x0];
	a[7] = InputBuf[y1*stride + x ];
	a[8] = InputBuf[y1*stride + x1];

	v_max = max(a[0], a[1]);
	v_min = min(a[0], a[1]);

	b[1] = clamp(a[2], v_min, v_max);
	b[0] = min(v_min, a[2]);
	b[2] = max(v_max, a[2]);

	v_max = max(a[3], a[4]);
	v_min = min(a[3], a[4]);

	b[4] = clamp(a[5], v_min, v_max);
	b[3] = min(v_min, a[5]);
	b[5] = max(v_max, a[5]);

	v_max = max(a[6], a[7]);
	v_min = min(a[6], a[7]);

	b[7] = clamp(a[8], v_min, v_max);
	b[6] = min(v_min, a[8]);
	b[8] = max(v_max, a[8]);

	v_max = max(b[0], b[3]);

	a[2] = max(v_max, b[6]);

	v_max = max(b[1], b[4]);
	v_min = min(b[1], b[4]);

	a[4] = clamp(b[7], v_min, v_max);

	v_min = min(b[2], b[5]);

	a[6] = min(v_min, b[8]);

	OutputBuf[y*stride + x] = max( min( max( a[4], a[6]), a[2]), min( a[4], a[6])); //can't use clamp becos can't ensure the order of a2 a4 a6
}

__kernel void ZeroMask(__global POST_PAR *pPar, __global unsigned char* pLImg, __global unsigned char* pDImg, int stride, __global int *debug)
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	int center = x + y * stride; // address of the center pixel

	pDImg[center] = pLImg[center] ? pDImg[center] : 0;
}
							
);


void * CL_Init()
{
	PPOST_CL pCL = NULL;
	if((pCL = new POST_CL) == 0)
		return 0;

	cl_int err;
	cl_uint num;
	err = clGetPlatformIDs(0, 0, &num);
	if(err != CL_SUCCESS) {
		delete pCL;
		return 0;
	}

	std::vector<cl_platform_id> platforms(num);
	err = clGetPlatformIDs(num, &platforms[0], &num);
	if(err != CL_SUCCESS) {
		delete pCL;
		return 0;
	}

	cl_context_properties prop[] = { CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platforms[0]), 0 };
	pCL->context = clCreateContextFromType(prop, CL_DEVICE_TYPE_GPU, NULL, NULL, NULL);
	if(pCL->context == 0) {
		delete pCL;
		return 0;
	}

	size_t cb;
	clGetContextInfo(pCL->context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
	std::vector<cl_device_id> devices(cb / sizeof(cl_device_id));
	clGetContextInfo(pCL->context, CL_CONTEXT_DEVICES, cb, &devices[0], 0);


	clGetDeviceInfo(devices[0], CL_DEVICE_NAME, 0, NULL, &cb);
	char devname[cb + 1];

	clGetDeviceInfo(devices[0], CL_DEVICE_NAME, cb, &devname[0], 0);
	
	// ----- memory allocatione -------

	cl_image_format format;
	format.image_channel_data_type = CL_UNORM_INT8;
	format.image_channel_order = CL_INTENSITY;

	for(int i=0; i<10; i++)
	{
		if((pCL->cl_buf[i] = clCreateBuffer(pCL->context, CL_MEM_READ_WRITE, sizeof(cl_uchar) * SPPW16*SPPH4, 0, NULL)) == 0)
		{
			clReleaseContext(pCL->context);
			delete pCL;
			return 0;
		}

	/*	if((pCL->cl_buf[i] = clCreateImage2D (pCL->context, CL_MEM_READ_WRITE, &format, 1296, 724, 1296, 0, NULL)) == 0)
		{
			clReleaseContext(pCL->context);
			delete pCL;
			return 0;
		}*/
	}

	if( (pCL->cl_struct = clCreateBuffer(pCL->context, CL_MEM_READ_WRITE, sizeof(POST_PAR) , 0, NULL)) == 0)
	{
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}

	if((pCL->cl_in = clCreateBuffer(pCL->context, CL_MEM_READ_ONLY, sizeof(cl_uchar) * SPPW*SPPH*3, 0, NULL)) == 0)
	{
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}

	if((pCL->cl_edge = clCreateBuffer(pCL->context, CL_MEM_READ_WRITE, sizeof(cl_uchar) * SPPW16*SPPH4, 0, NULL)) == 0)
//	if((pCL->cl_edge = clCreateImage2D (pCL->context, CL_MEM_READ_WRITE, &format, SPPW16, SPPH4, SPPW16, 0, NULL)) == 0)
	{
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}
	
	if((pCL->cl_d_prev = clCreateBuffer(pCL->context, CL_MEM_READ_WRITE, sizeof(cl_uchar) * SPPW16*SPPH4, 0, NULL)) == 0)
	{
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}

	if((pCL->cl_debug = clCreateBuffer(pCL->context, CL_MEM_READ_WRITE, sizeof(cl_uchar) * 1000, 0, NULL)) == 0)
	{
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}
	

	pCL->cl_d_in	= &pCL->cl_buf[0];
	pCL->cl_d_t[0]	= &pCL->cl_buf[1];
	pCL->cl_d_t[1]	= &pCL->cl_buf[2];
	pCL->cl_d_t[2]	= &pCL->cl_buf[3];
	pCL->cl_d_t[3]	= &pCL->cl_buf[4];
	pCL->cl_d_out	= &pCL->cl_buf[5];
	pCL->cl_y_t[0]	= &pCL->cl_buf[6];
	pCL->cl_y_t[1]	= &pCL->cl_buf[7];
	pCL->cl_y_t[2]	= &pCL->cl_buf[8];
	pCL->cl_y_t[3]	= &pCL->cl_buf[9];

	// ----- end of memory allocation -------

	pCL->queue = clCreateCommandQueue(pCL->context, devices[0], 0, 0);
	if(pCL->queue == 0) {
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}

	pCL->program = clCreateProgramWithSource(pCL->context, 1, &cl_code, 0, 0);

	if(pCL->program == 0) {
		clReleaseCommandQueue(pCL->queue);
		clReleaseContext(pCL->context);
		delete pCL;
		return 0;
	}

	if(clBuildProgram(pCL->program, 0, 0, 0, 0, 0) != CL_SUCCESS) {
		cl_build_status status;
		size_t logSize;
		char *programLog;
		FILE *f = fopen("error log.txt", "w");

		// check build error and build status first
		clGetProgramBuildInfo(pCL->program, devices[0], CL_PROGRAM_BUILD_STATUS, 
				sizeof(cl_build_status), &status, NULL);

		// check build log
		clGetProgramBuildInfo(pCL->program, devices[0], 
				CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
		programLog = (char*) calloc (logSize+1, sizeof(char));
		clGetProgramBuildInfo(pCL->program, devices[0], 
				CL_PROGRAM_BUILD_LOG, logSize+1, programLog, NULL);
		fprintf(f, "Build failed:\nstatus = %d\nprogramLog : \n %s\n", 
				status, programLog);
		free(programLog);
		fclose(f);

		clReleaseCommandQueue(pCL->queue);
		clReleaseContext(pCL->context);
		clReleaseProgram(pCL->program);
		delete pCL;
		return 0;
	}

	return (void *)pCL;
}

void CL_Destroy(void * str)
{
	if(!str)
		return;

	PPOST_CL pCL = (PPOST_CL) str;
	clReleaseProgram(pCL->program);
	clReleaseCommandQueue(pCL->queue);
	clReleaseContext(pCL->context);

	for(int i=0; i<10; i++)
	{
		clReleaseMemObject(pCL->cl_buf[i]);
	}
	clReleaseMemObject(pCL->cl_struct);
	clReleaseMemObject(pCL->cl_in);
	clReleaseMemObject(pCL->cl_edge);
	clReleaseMemObject(pCL->cl_d_prev);
	clReleaseMemObject(pCL->cl_debug);

	delete [] pCL;
}
