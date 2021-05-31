/*! \file Post_Header.h
    \brief The definition of Post Process class member function and data
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include "CL/cl.h"
#endif

#pragma pack(push, 1)

#define SPPW	1920//1280
#define SPPW1	1921//1281
#define SPPW2	1922//1282
#define SPPW15	1935//1295
#define SPPW16	1936//1296
#define SPPW17	1937//1297

#define SPPH	1080//720
#define SPPH2	1082//722
#define SPPH4	1084//724

typedef struct POST_CL
{
	cl_program program;
	cl_command_queue queue;
	cl_context context;
	cl_mem cl_struct;
	cl_mem cl_in;
	cl_mem cl_buf[10];
	cl_mem cl_edge;
	cl_mem cl_d_prev;
	cl_mem *cl_d_in;
	cl_mem *cl_d_out;
	cl_mem *cl_d_t[4];
	cl_mem *cl_y_in;
	cl_mem *cl_y_prev;
	cl_mem *cl_y_t[4];
	cl_mem cl_debug;

}*PPOST_CL;

#define GPU_GRAD	0
#define GPU_HR		1
#define GPU_HF		2
#define GPU_DC		3
#define GPU_SEG		4
#define GPU_HF2		5
#define GPU_TEMP0	6
#define GPU_TEMP1	7
#define GPU_MF		8
#define GPU_ZM		9
#define GPU_FC		10
#define GPU_RF		11


typedef struct POST_PAR
{
	cl_int VERSION_ID;		//0
	cl_int PAR_NUM;			//1
	cl_int GPU_EN;			//2
	cl_int RSVD3;			//3
	cl_int RSVD4;			//4
	cl_int HR_MODE;			//5
	cl_int HR_CURVE_0;		//6
	cl_int HR_CURVE_1;		//7
	cl_int HR_CURVE_2;		//8
	cl_int HR_CURVE_3;		//9
	cl_int HR_CURVE_4;		//10
	cl_int HR_CURVE_5;		//11
	cl_int HR_CURVE_6;		//12
	cl_int HR_CURVE_7;		//13
	cl_int HR_CURVE_8;		//14
	cl_int HR_CTRL;			//15
	cl_int RSVD16;			//16
	cl_int HF_MODE;			//17
	cl_int RSVD5;			//18
	cl_int RSVD6;			//19
	cl_int DC_MODE;			//20
	cl_int DC_CNT_THD;		//21
	cl_int DC_GRAD_THD;		//22
	cl_int SEG_MODE;		//23
	cl_int SEG_THD_SUB;		//24
	cl_int SEG_THD_SLP;		//25
	cl_int SEG_THD_MAX;		//26
	cl_int SEG_THD_MIN;		//27
	cl_int SEG_FILL_MODE;	//28
	cl_int RSVD29;			//29
	cl_int RSVD30;			//30
	cl_int HF2_MODE;		//31
	cl_int RSVD32;			//32
	cl_int RSVD33;			//33
	cl_int GRAD_MODE;		//34
	cl_int RSVD35;			//35
	cl_int RSVD36;			//36
	cl_int TEMP0_MODE;		//37
	cl_int TEMP0_THD;		//38
	cl_int RSVD39;			//39
	cl_int RSVD40;			//40
	cl_int TEMP1_MODE;		//41
	cl_int TEMP1_LEVEL;		//42
	cl_int TEMP1_THD;		//43
	cl_int RSVD44;			//44
	cl_int RSVD45;			//45
	cl_int FC_MODE;			//46
	cl_int FC_EDGE_THD;		//47
	cl_int FC_AREA_THD;		//48
	cl_int RSVD49;			//49
	cl_int RSVD50;			//50
	cl_int MF_MODE;			//51
	cl_int ZM_MODE;			//52
	cl_int RF_MODE;			//53
	cl_int RF_LEVEL;		//54
	cl_int RF_BLK_SIZE;		//55

}*PPOST_PAR;


typedef struct POST_STRUCT
{
	unsigned char *pHugeBuf;

	unsigned char *ppLImgTmp[4];
	unsigned char *ppDImgTmp[4];
	unsigned char *pDImgTmpI;
	unsigned char *pDImg;
	unsigned char *pTmpImg;

	unsigned char *pStrPtr;

	int	iFrameCnt;
	int iOpenCL;
	int iPrevFromGPU;

	int iInputFmt;
	int iInputType;
	int iInputFlip;
	int iInputStride;

	POST_PAR Par;
	PPOST_CL pCL;

}*PPOST_STRUCT;


#pragma pack(pop)

const int tab_size = 4096;

//const int huge_buffer_size = 4*1296*724 + 11*1296*722 + 1296*720 + tab_size*64*4 + 16;
const int huge_buffer_size = 4*SPPW16*SPPH4 + 11*SPPW16*SPPH2 + SPPW16*SPPH + tab_size*64*4 + 16;



void * CL_Init();
void CL_Destroy(void * str);
void CL_Reset(void * str);


