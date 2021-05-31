#include "Post_Header.h"
#include "Post_SW.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef X86_CONSOLE
#define _SSE2
#endif

#ifdef _SSE2

#include <X11/Intrinsic.h>
#include <cpuid.h>
#endif

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include "CL/cl.h"
#endif


#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define SIMD_SSE_NONE	0    // not support
#define SIMD_SSE_1		1    // SSE
#define SIMD_SSE_2		2    // SSE2
#define SIMD_SSE_3		3    // SSE3
#define SIMD_SSE_3S		4    // SSSE3
#define SIMD_SSE_41		5    // SSE4.1
#define SIMD_SSE_42		6    // SSE4.2
#define SIMD_SSE_ADV	7	 // advanced one


inline int simd_sse_level(int* phwsse);

void Sobel(PPOST_STRUCT pPost, unsigned char *pIn, unsigned char *pOut, int width, int height, int stride);
void DepthClear(PPOST_STRUCT pPost, unsigned char * pEImg, unsigned char * pDin, int width, int height);
void HoleFill(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride);
void HoleFill2(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride);
void HugeRemove(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride);
void ZeroMask(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * pDin, int width, int height, int stride);
void Segment(PPOST_STRUCT pPost, unsigned char *pLImg, unsigned short *pSImg, unsigned char *&pDin, unsigned char *&pDout, unsigned char *pEImg, unsigned char *pIntraBuf,
             int width, int height, int stride);
void SegmentFill(PPOST_STRUCT pPost, unsigned char *pLImg, unsigned short *&pSImg, unsigned short *&pSTmpImg, int width, int height, int stride);

void Fragment(PPOST_STRUCT pPost, unsigned char *pLImg, unsigned short *pSImg, unsigned char *&pDin, unsigned char *&pDout, int width, int height, int stride);

void TemporalMedian(PPOST_STRUCT pPost, unsigned char * &img_in, unsigned char * &img_prev, unsigned char ** img_t,
                         unsigned char * &din, unsigned char ** d_t,
                         unsigned char * &dout, int width, int height, int stride);

void TemporalIIR(PPOST_STRUCT pPost, unsigned char * img_in, unsigned char * img_tin,
                         unsigned char * &din, unsigned char * &dout, unsigned char * disp_tin, int width, int height, int stride);

void MedianFilter(PPOST_STRUCT pPost, unsigned char * &pIn, unsigned char * &pOut, int width, int height, int stride);

void Convert_Color_Y(PPOST_STRUCT pPost, unsigned char *pIn, unsigned char *pLImg, int width, int height, int stride);
void Convert_D_D(unsigned char *pDepthBuf, unsigned char *pDImg, int width, int height, int stride);

void Refine(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride);

void DepthOutput(PPOST_STRUCT pPost, unsigned char *pDImg, unsigned char *pOut, int width, int height, int stride);
void DepthOutput(unsigned short *pDImg, unsigned char *pOut, int width, int height); //for test

inline void swap(unsigned char *&a, unsigned char *&b);

#ifndef _SSE2

inline int M3i(int In0, int In1, int In2)
{
    return max( min( max( In0, In1), In2), min( In0, In1));
}

inline int M3p(int *In)
{
    return max( min( max( In[0], In[1]), In[2]), min( In[0], In[1]));
}

inline void S3i(int &In0, int &In1, int &In2)
{
    int v_max, v_min;

    v_max = max(In0, In1);
    v_min = min(In0, In1);

    In1 = max(min(v_max, In2), v_min);
    In0 = min(v_min, In2);
    In2 = max(v_max, In2);
}

void S3p(int *IB)
{
    int v_max, v_min;

    v_max = max(IB[0], IB[1]);
    v_min = min(IB[0], IB[1]);

    IB[1] = max(min(v_max, IB[2]), v_min);
    IB[0] = min(v_min, IB[2]);
    IB[2] = max(v_max, IB[2]);
}

int M9p(int *IB)
{
    S3p(IB);
    S3p(IB+3);
    S3p(IB+6);

    S3i(IB[0], IB[3], IB[6]);
    S3i(IB[1], IB[4], IB[7]);
    S3i(IB[2], IB[5], IB[8]);

    return M3i(IB[6], IB[4], IB[2]);
}

int M5p(int *IB)
{
    int v[5],m[3];

    v[0] = min(IB[0], IB[1]);
    v[1] = max(IB[0], IB[1]);
    v[2] = IB[2];
    v[3] = min(IB[3], IB[4]);
    v[4] = max(IB[3], IB[4]);

    m[0] = v[3];
    m[1] = M3i(v[0],v[2],v[4]);
    m[2] = v[1];

    return M3p(m);
}

#endif

inline void swap(cl_mem * &a, cl_mem * &b)
{
    cl_mem * tmp = a;
    a = b;
    b = tmp;
}

inline void swap(unsigned char * &a, unsigned char * &b)
{
    unsigned char * tmp = a;
    a = b;
    b = tmp;
}

inline void swap(unsigned short * &a, unsigned short * &b)
{
    unsigned short * tmp = a;
    a = b;
    b = tmp;
}

int simd_sse_level(int* phwsse)
{
#ifdef _SSE2
    const int BIT_D_SSE = 0x02000000;		// bit 25
    const int BIT_D_SSE2 = 0x04000000;		// bit 26
    const int BIT_C_SSE3 = 0x00000001;		// bit 0
    const int BIT_C_SSSE3 = 0x00000100;		// bit 9
    const int BIT_C_SSE41 = 0x00080000;		// bit 19
    const int BIT_C_SSE42 = 0x00100000;		// bit 20
    int rt = SIMD_SSE_NONE; // result
    unsigned int dwBuf[4];

    // check processor support
    //__cpuid(dwBuf, 1);  // Function 1: Feature Information

    __get_cpuid_max(1, dwBuf);  // Function 1: Feature Information

    if ( dwBuf[3] & BIT_D_SSE )
    {
        rt = SIMD_SSE_1;
        if ( dwBuf[3] & BIT_D_SSE2 )
        {
            rt = SIMD_SSE_2;
            if ( dwBuf[2] & BIT_C_SSE3 )
            {
                rt = SIMD_SSE_3;
                if ( dwBuf[2] & BIT_C_SSSE3 )
                {
                    rt = SIMD_SSE_3S;
                    if ( dwBuf[2] & BIT_C_SSE41 )
                    {
                        rt = SIMD_SSE_41;
                        if ( dwBuf[2] & BIT_C_SSE42 )
                        {
                            rt = SIMD_SSE_42;
                        }
                    }
                }
            }
        }
    }
    if (NULL!=phwsse)   *phwsse=rt;

    // check OS support

    __m128 xmm1 = _mm_setzero_ps(); // SSE instruction: xorps
    if (0 != *(int*)&xmm1)
        rt = SIMD_SSE_NONE; //


    return rt;

#else
    return 0xff;

#endif

}


#define ID(a,b,c,d) ((a<<24) | (b<<16) | (c<<8) | d)


POST_DLL_API
void * Post_Init()
{
    PPOST_STRUCT pPost = NULL;

    if((pPost = new POST_STRUCT) == 0)
        return 0;

    memset(pPost, 0, sizeof(POST_STRUCT));

    PPOST_PAR pPar = &pPost->Par;

    // ---------- Version ID ----------

    pPar->VERSION_ID = ID(0, 0, 2, 0);
    pPar->PAR_NUM = sizeof(POST_PAR)/sizeof(int);

    // --------------------------------

    pPar->GPU_EN = 1;

    pPar->HR_MODE = 1;
    pPar->HR_CURVE_0 = 30;
    pPar->HR_CURVE_1 = 30;
    pPar->HR_CURVE_2 = 6;
    pPar->HR_CURVE_3 = 5;
    pPar->HR_CURVE_4 = 5;
    pPar->HR_CURVE_5 = 5;
    pPar->HR_CURVE_6 = 13;
    pPar->HR_CURVE_7 = 15;
    pPar->HR_CURVE_8 = 15;
    pPar->HR_CTRL = 9 | (3<<8) | (8<<16);

    pPar->DC_MODE = 0;
    pPar->DC_GRAD_THD = 5;
    pPar->DC_CNT_THD = 4;

    pPar->HF_MODE = 2;

    pPar->SEG_MODE = 1;
    pPar->SEG_THD_SUB = 60;
    pPar->SEG_THD_SLP = 64;
    pPar->SEG_THD_MAX = 10;//20;
    pPar->SEG_THD_MIN = 4;////5;

    pPar->SEG_FILL_MODE = 0;

    pPar->HF2_MODE = 0;

    pPar->GRAD_MODE = 1;

    pPar->TEMP0_MODE = 2;
    pPar->TEMP0_THD = 4;

    pPar->TEMP1_MODE = 2;
    pPar->TEMP1_THD = 7;
    pPar->TEMP1_LEVEL = 60;

    pPar->FC_MODE = 1;
    pPar->FC_EDGE_THD = 5;
    pPar->FC_AREA_THD = 100;

    pPar->MF_MODE = 1;
    pPar->ZM_MODE = 1;

    pPar->RF_MODE = 1;
    pPar->RF_LEVEL = 28;
    pPar->RF_BLK_SIZE = 4; //8

    pPost->pCL = (PPOST_CL) CL_Init();

    pPost->iOpenCL = pPost->pCL? 1: 0;

    pPost->iInputFmt	= 0;
    pPost->iInputType	= 0;
    pPost->iInputFlip	= 0;
    pPost->iInputStride	= 0;

    if(pPost->iOpenCL)
    {
        clEnqueueWriteBuffer(pPost->pCL->queue, pPost->pCL->cl_struct, CL_TRUE, 0, sizeof(POST_PAR), &(pPost->Par), 0, 0, 0);
    }
    else
    {
        if(simd_sse_level(0) < SIMD_SSE_2)
            return 0;
    }

    if((pPost->pHugeBuf = new unsigned char[huge_buffer_size]) == 0)
        return 0;

    Post_Reset(pPost);

    return (void *) pPost;
}

POST_DLL_API
void Post_SetParam(void * pStr, int Idx, int Val)
{
    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;
    int * pPar = (int *) &(pPost->Par);

    if(Idx >= (int)(sizeof(POST_PAR)/sizeof(int)) || Idx < 2)
        return;

    pPar[Idx] = Val;

    if(pPost->iOpenCL)
    {
        clEnqueueWriteBuffer(pPost->pCL->queue, pPost->pCL->cl_struct, CL_TRUE, 0, sizeof(POST_PAR), &(pPost->Par), 0, 0, 0);
    }
}

POST_DLL_API
int Post_GetParam(void * pStr, int Idx)
{
    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;
    int * pPar = (int *) &(pPost->Par);

    if(Idx >= (int)(sizeof(POST_PAR)/sizeof(int)))
        return 0;

    return pPar[Idx];
}

POST_DLL_API
void Post_Reset(void * pStr)
{
    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;

    memset(pPost->pHugeBuf, 0 , huge_buffer_size);

    pPost->pStrPtr = pPost->pHugeBuf + 16 - ((long long)pPost->pStrPtr & 0xf); //first 16byte aligned point

    pPost->ppLImgTmp[0] = pPost->pStrPtr		+ 2*SPPW16;	// ppLImgTmp[0] =>1296x724

    pPost->ppLImgTmp[1]	= pPost->ppLImgTmp[0]	+ SPPW16*SPPH4;	// ppLImgTmp[1] =>1296x724
    pPost->ppLImgTmp[2]	= pPost->ppLImgTmp[1]	+ SPPW16*SPPH4;	// ppLImgTmp[2] =>1296x724
    pPost->ppLImgTmp[3]	= pPost->ppLImgTmp[2]	+ SPPW16*SPPH4;	// ppLImgTmp[2] =>1296x724

    pPost->ppDImgTmp[0]	= pPost->ppLImgTmp[3]	+ SPPW16*SPPH2; // ppDImgTmp[0] =>1296x722
    pPost->ppDImgTmp[1]	= pPost->ppDImgTmp[0]	+ SPPW16*SPPH2;	// ppDImgTmp[1] =>1296x722
    pPost->ppDImgTmp[2]	= pPost->ppDImgTmp[1]	+ SPPW16*SPPH2;	// ppDImgTmp[2] =>1296x722
    pPost->ppDImgTmp[3]	= pPost->ppDImgTmp[2]	+ SPPW16*SPPH2;	// ppDImgTmp[3] =>1296x722


    pPost->pDImg		= pPost->ppDImgTmp[3]	+ SPPW16*SPPH2; // pDImg        =>1296x722
    pPost->pTmpImg		= pPost->pDImg			+ SPPW16*SPPH2;	// pTmpImg      =>1296x722
    pPost->pDImgTmpI	= pPost->pTmpImg		+ SPPW16*SPPH2;	// pDImgTmpI    =>1296x722

    pPost->iFrameCnt = 0;

    if(pPost->iOpenCL)
    {
        PPOST_CL pCL = pPost->pCL;
        unsigned char *tmp;
        if((tmp = new unsigned char[sizeof(cl_uchar) *SPPW16*SPPH4]) == 0)
        {
            return ;
        }

        memset(tmp, 0, sizeof(cl_uchar) *SPPW16*SPPH4);

        for(int i=0; i<10; i++)
        {
            clEnqueueWriteBuffer(pCL->queue, pCL->cl_buf[i], CL_TRUE, 0, sizeof(cl_uchar) * SPPW16*SPPH4, tmp, 0, 0, 0);
        }

        clEnqueueWriteBuffer(pCL->queue, pCL->cl_edge, CL_TRUE, 0, sizeof(cl_uchar) * SPPW16*SPPH4, tmp, 0, 0, 0);
        clEnqueueWriteBuffer(pCL->queue, pCL->cl_d_prev, CL_TRUE, 0, sizeof(cl_uchar) * SPPW16*SPPH4, tmp, 0, 0, 0);
        clEnqueueWriteBuffer(pCL->queue, pCL->cl_debug, CL_TRUE, 0, sizeof(cl_uchar) * 1000, tmp, 0, 0, 0);

        delete [] tmp;
    }
}

POST_DLL_API
void Post_Destroy(void * pStr)
{
    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;

    if(!pPost)
        return;

    if(pPost->pHugeBuf)
        delete [] pPost->pHugeBuf;

    CL_Destroy(pPost->pCL);

    delete [] pPost;
}

POST_DLL_API
void Post_SetColorFmt(void * pStr, int Fmt, int Type, int Flip, int Stride)
{
    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;

    pPost->iInputFmt	= Fmt;
    pPost->iInputType	= Type;
    pPost->iInputFlip	= Flip;
    pPost->iInputStride	= Stride;
}

POST_DLL_API
int Post_GetInfo(void * pStr, int Command)
{
    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;

    switch(Command)
    {
    case INFO_IS_GPU_AVAILABLE:
        return pPost->iOpenCL;
    default:
        return 0;
    }
}

POST_DLL_API
int Post_Process(void * pStr, unsigned char *pIn, unsigned char *pDepthBuf, unsigned char *pOut, int width, int height)
{
    if(!pStr)
        return 0;

    PPOST_STRUCT pPost = (PPOST_STRUCT) pStr;

    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    //if(width > SPPW || height > SPPH || width <= 0 || height <= 0) //max support size SPPW x SPPH
    if(width > SPPW || width * height > SPPW * SPPH || width <= 0 || height <= 0)
        return 0;

    unsigned char *pLImgPrev = pPost->ppLImgTmp[1];
    unsigned char *pLImg	= pPost->ppLImgTmp[0];
    unsigned char *pDImg	= pPost->pDImg;
    unsigned char *pTmpImg	= pPost->pTmpImg;
    unsigned char **ppLImgTmp = pPost->ppLImgTmp;
    unsigned char **ppDImgTmp = pPost->ppDImgTmp;
    unsigned char *pDImgTmpI = pPost->pDImgTmpI;

    unsigned char *pEImg	= pPost->pStrPtr + SPPW16*SPPH4*4 + SPPW16*SPPH2*7; // pEImg =>1296x720
    unsigned short *pSImg	= (unsigned short *)(pEImg + SPPW16*(SPPH+1)); // pSImg =>2*1296x722
    unsigned char *pHist	= (unsigned char *)(pSImg + SPPW16*(2*SPPH2-1));

    int stride = (pPost->iOpenCL & pPar->GPU_EN)? (((width>>4)+1)<<4) : SPPW16;

    Convert_Color_Y(pPost, pIn, pLImg, width, height, stride);

    Convert_D_D(pDepthBuf, pDImg, width, height, stride);

    Sobel(pPost, pLImg, pEImg, width , height, stride); //this function must be called after cl Y buffer ready.

    HugeRemove(pPost, pLImg, pDImg, pTmpImg, width , height, stride);

    HoleFill(pPost, pLImg, pDImg, pTmpImg, width , height, stride);

//	DepthClear(pPost, pEImg, pDImg, width , height);

    Segment(pPost, pLImg, pSImg, pDImg, pTmpImg, pEImg, pHist, width, height, stride);

    HoleFill2(pPost, pLImg, pDImg, pTmpImg, width , height, stride);

    TemporalMedian(pPost, pLImg, pLImgPrev, ppLImgTmp,
                pDImg, ppDImgTmp,
                pTmpImg, width, height, stride);

    // must call TemporalIIR after TemporalMedian
    TemporalIIR(pPost, pLImg, pLImgPrev,
                pDImg, pTmpImg, pDImgTmpI, width, height, stride);

    MedianFilter(pPost, pDImg, pTmpImg, width, height, stride);

    ZeroMask(pPost, pLImg, pDImg, width, height, stride);

    Fragment(pPost, pLImg, pSImg, pDImg, pTmpImg, width, height, stride);

    Refine(pPost, pLImg, pDImg, pTmpImg, width , height, stride);

    DepthOutput(pPost, pDImg, pOut, width, height, stride);

    pPost->iFrameCnt++;

    return 1;

}

void DepthClear(PPOST_STRUCT pPost, unsigned char * pEImg, unsigned char * pDin, int width, int height)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->DC_MODE == 0)
        return;

    unsigned char *din = pDin;

#ifdef _SSE2

    __m128i grd_thd = _mm_set1_epi8(pPar->DC_GRAD_THD);
    __m128i cnt_thd = _mm_set1_epi8(pPar->DC_CNT_THD);
    __m128i cnt = _mm_setzero_si128();
    __m128i one = _mm_set1_epi8(1);


    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x+=16 )
        {
            int center = x + y * SPPW16; // address of the center pixel

            __m128i p0 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center-SPPW17)), grd_thd));
            __m128i p1 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center-SPPW16)), grd_thd));
            __m128i p2 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center-SPPW15)), grd_thd));
            __m128i p3 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center-1   )), grd_thd));
            __m128i p4 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center+1   )), grd_thd));
            __m128i p5 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center+SPPW15)), grd_thd));
            __m128i p6 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center+SPPW16)), grd_thd));
            __m128i p7 = _mm_and_si128(one, _mm_cmplt_epi8(_mm_loadu_si128((__m128i*)(pEImg + center+SPPW17)), grd_thd));

            p0 = _mm_adds_epi8(p0, p1);
            p2 = _mm_adds_epi8(p2, p3);
            p4 = _mm_adds_epi8(p4, p5);
            p6 = _mm_adds_epi8(p6, p7);

            p0 = _mm_adds_epi8(p0, p2);
            p4 = _mm_adds_epi8(p4, p6);

            p0 = _mm_adds_epi8(p0, p4);

            __m128i p = _mm_cmplt_epi8(_mm_load_si128((__m128i*)(pEImg + center)), grd_thd);
            __m128i cond = _mm_andnot_si128(p, _mm_cmplt_epi8(p0, cnt_thd));
            __m128i out = _mm_and_si128(_mm_load_si128((__m128i*)(din + center)), cond);

            _mm_store_si128((__m128i*)(din + center), out);

        }
    }
#else

    int thd = pPar->DC_GRAD_THD;
    int cnt_thd = pPar->DC_CNT_THD;

    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++ )
        {
            int center = x + y * SPPW16; // address of the center pixel
            int cnt =	(pEImg[center-SPPW17] < thd) + (pEImg[center-SPPW16] < thd) + (pEImg[center-SPPW15] < thd) +
                        (pEImg[center-1   ] < thd) +                              (pEImg[center+1]    < thd) +
                        (pEImg[center+SPPW15] < thd) + (pEImg[center+SPPW16] < thd) + (pEImg[center+SPPW17] < thd) ;

            if( pEImg[center] < thd || cnt >= cnt_thd)
            {
                din[center] = 0;
            }
        }
    }
#endif

    pPost->iPrevFromGPU = 0;
}


void HoleFill(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->HF_MODE == 0)
        return;

    unsigned char *din = pDin;
    unsigned char *dout = pDout;

    if(pPost->iPrevFromGPU)
    {
        clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDin, 0, 0, 0);
    }

    if(pPar->HF_MODE == 1)
    {
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++ )
            {
                if(x==65 && y==36)
                    x=x;

                int center = x + y * stride; // address of the center pixel
                int left = center - 1;
                int top = center - stride;

                if( pLImg[center] && (din[center] == 0) && (din[left] != 0 || din[top] != 0))
                {
                    int v0 = din[left] == 0? 255 : din[left]; //left
                    int v1 = din[top] == 0? 255 : din[top]; //top

                    din[center] = min(v0, v1);
                }
            //	else
            //		din[center] = din[center];

            }
        }
    }
    else
    {
        int fw_line[SPPW2];
        int bw_line[SPPW2];

        fw_line[0] = fw_line[SPPW1] = bw_line[0] = bw_line[SPPW1] = 0;

        int *fw_ptr = fw_line + 1; //for boundary issue
        int *bw_ptr = bw_line + 1;

        for(int y = 0; y < height; y++)
        {
            for(int x = width-1; x >= 0; x--)
            {
                int center = x + y * stride; // address of the center pixel
                int top = center - stride;

                if( pLImg[center] && (din[center] == 0) && (bw_ptr[x+1] != 0 || din[top] != 0))
                {
                    int v0 = bw_ptr[x+1] ? bw_ptr[x+1] : 255;	//right
                    int v1 = din[top] ? din[top] : 255;		//top

                    bw_ptr[x] = min(v0, v1);
                }
                else
                    bw_ptr[x] = din[center];
            }

            for(int x = 0; x < width; x++ )
            {
                int center = x + y * stride; // address of the center pixel
                int top = center - stride;

                if( pLImg[center] && (din[center] == 0) && (fw_ptr[x-1] != 0 || din[top] != 0))
                {
                    int v0 = fw_ptr[x-1] ? fw_ptr[x-1] : 255;	//left
                    int v1 = din[top] ? din[top] : 255;		//top

                    fw_ptr[x] = min(v0, v1);
                }
                else
                    fw_ptr[x] = din[center];

                if(fw_ptr[x] || bw_ptr[x])
                {
                    int v0 = fw_ptr[x] ? fw_ptr[x]: 255 ; //forward
                    int v1 = bw_ptr[x] ? bw_ptr[x]: 255 ; //backward

                    din[center] = min(v0, v1);
                }
                else
                    din[center] = 0;
            }
        }
    }

    pPost->iPrevFromGPU = 0;
}

void Refine(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->RF_MODE == 0)
        return;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {
        //guassian, for openCL only

        cl_int gauss[256];
        float s = (float)pPar->RF_LEVEL;

        for(int i = 0; i < 256; i++)
        {
            float i2 = (float)i*i;
            gauss[i] = (cl_int)(65535*(exp(-(i2)/s))/(3.1415926 * s));
        }

        if(!pPost->iPrevFromGPU) //last stage from CPU
        {
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDin, 0, 0, 0);
        }
    //	else
    //	{
    //		clFinish(pCL->queue);
    //	}

        cl_mem cl_gauss = clCreateBuffer(pCL->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , sizeof(cl_int) * 256, gauss, NULL);

        cl_kernel clRefine = clCreateKernel(pCL->program, "Refine", 0);

        clSetKernelArg(clRefine, 0, sizeof(cl_mem), &pCL->cl_struct);
        clSetKernelArg(clRefine, 1, sizeof(cl_mem), pCL->cl_y_in);
        clSetKernelArg(clRefine, 2, sizeof(cl_mem), pCL->cl_d_in);
        clSetKernelArg(clRefine, 3, sizeof(cl_mem), pCL->cl_d_out);
        clSetKernelArg(clRefine, 4, sizeof(cl_mem), &cl_gauss);
        clSetKernelArg(clRefine, 5, sizeof(stride), &stride);
        clSetKernelArg(clRefine, 6, sizeof(cl_mem), &pCL->cl_debug);

        size_t global_size[] = {(size_t)width, (size_t)height};
        clEnqueueNDRangeKernel(pCL->queue, clRefine, 2, 0, global_size, 0, 0, 0, 0);


        clReleaseKernel(clRefine);
        clReleaseMemObject(cl_gauss);

        pPost->iPrevFromGPU = 1;

        swap(pCL->cl_d_in, pCL->cl_d_out);
    }
    else //CPU
    {
        unsigned char *din = pDin;
        unsigned char *dout = pDout;

        if(pPost->iPrevFromGPU)
        {
            clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDin, 0, 0, 0);
        }

        int i,j;
        int w_level = pPar->RF_LEVEL;
        int blk_size = pPar->RF_BLK_SIZE;

#ifdef _SSE2

        __m128i z = _mm_setzero_si128();
        __m128i level = _mm_set1_epi16(w_level);

        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x+=16 )
            {
                int center = x + y * SPPW16; // address of the center pixel

                if(x == 364 && y == 18)
                    x = x;

                // ----------------------------------------------

                //    |  LL  |  LH  |  HL  |  HH  |
                //       0~3    4~7   8~11   12~15   (pixel index)

                //    |      L      |      H      |
                //          0~7           8~15       (pixel index)

                // ----------------------------------------------

                __m128i val_sum_HH = _mm_setzero_si128();
                __m128i val_sum_HL = _mm_setzero_si128();
                __m128i val_sum_LH = _mm_setzero_si128();
                __m128i val_sum_LL = _mm_setzero_si128();

                __m128i wgt_sum_H = _mm_setzero_si128();
                __m128i wgt_sum_L = _mm_setzero_si128();

                __m128i c_val = _mm_load_si128((__m128i*)(pLImg + center));

                __m128i c_val_H = _mm_unpackhi_epi8(c_val, z);
                __m128i c_val_L = _mm_unpacklo_epi8(c_val, z);

                for( j = -8; j <= 8; j+=2 ) // y-axis
                {
                    for( i = -8; i <= 8; i+=2 ) // x-axis
                    {
                        int y1 = max(min(y+j, height-1),0);
                        int addr = y1 * SPPW16 + x+i;

                        __m128i s_val = _mm_loadu_si128((__m128i*)(pLImg + addr));

                        __m128i s_val_H = _mm_unpackhi_epi8(s_val, z);
                        __m128i s_val_L = _mm_unpacklo_epi8(s_val, z);

                        __m128i diff_H = _mm_sub_epi16(c_val_H, s_val_H);
                        __m128i diff_L = _mm_sub_epi16(c_val_L, s_val_L);

                        __m128i abs_diff_H = _mm_sub_epi16( _mm_max_epi16( diff_H, z ), _mm_min_epi16( diff_H, z ) );
                        __m128i abs_diff_L = _mm_sub_epi16( _mm_max_epi16( diff_L, z ), _mm_min_epi16( diff_L, z ) );

                        __m128i wgt_H = _mm_max_epi16(z, _mm_sub_epi16(_mm_set1_epi16(8), _mm_srai_epi16(_mm_mullo_epi16(abs_diff_H, level), 5)));
                        __m128i wgt_L = _mm_max_epi16(z, _mm_sub_epi16(_mm_set1_epi16(8), _mm_srai_epi16(_mm_mullo_epi16(abs_diff_L, level), 5)));

                        __m128i disp = _mm_loadu_si128((__m128i*)(din + addr));
                        __m128i disp_H = _mm_unpackhi_epi8(disp, z);
                        __m128i disp_L = _mm_unpacklo_epi8(disp, z);

                        __m128i mul_HH = _mm_mulhi_epu16 (disp_H, wgt_H);
                        __m128i mul_HL = _mm_mullo_epi16 (disp_H, wgt_H);
                        __m128i mul_LH = _mm_mulhi_epu16 (disp_L, wgt_L);
                        __m128i mul_LL = _mm_mullo_epi16 (disp_L, wgt_L);

                        __m128i val_HH = _mm_unpackhi_epi16 (mul_HL, mul_HH); //32 bit
                        __m128i val_HL = _mm_unpacklo_epi16 (mul_HL, mul_HH);
                        __m128i val_LH = _mm_unpackhi_epi16 (mul_LL, mul_LH); //32 bit
                        __m128i val_LL = _mm_unpacklo_epi16 (mul_LL, mul_LH);

                        val_sum_HH = _mm_add_epi32(val_sum_HH, val_HH);
                        val_sum_HL = _mm_add_epi32(val_sum_HL, val_HL);
                        val_sum_LH = _mm_add_epi32(val_sum_LH, val_LH);
                        val_sum_LL = _mm_add_epi32(val_sum_LL, val_LL);

                        wgt_sum_H = _mm_add_epi16(wgt_sum_H, wgt_H);
                        wgt_sum_L = _mm_add_epi16(wgt_sum_L, wgt_L);
                    }
                }

                __m128i wgt_sum_HH = _mm_unpackhi_epi16 (wgt_sum_H, z); //to 32 bit
                __m128i wgt_sum_HL = _mm_unpacklo_epi16 (wgt_sum_H, z);
                __m128i wgt_sum_LH = _mm_unpackhi_epi16 (wgt_sum_L, z); //to 32 bit
                __m128i wgt_sum_LL = _mm_unpacklo_epi16 (wgt_sum_L, z);

                __m128 f_wgt_sum_HH = _mm_cvtepi32_ps(wgt_sum_HH); //to 32 bit float
                __m128 f_wgt_sum_HL = _mm_cvtepi32_ps(wgt_sum_HL); //to 32 bit float
                __m128 f_wgt_sum_LH = _mm_cvtepi32_ps(wgt_sum_LH); //to 32 bit float
                __m128 f_wgt_sum_LL = _mm_cvtepi32_ps(wgt_sum_LL); //to 32 bit float
                __m128 f_val_sum_HH = _mm_cvtepi32_ps(val_sum_HH); //to 32 bit float
                __m128 f_val_sum_HL = _mm_cvtepi32_ps(val_sum_HL); //to 32 bit float
                __m128 f_val_sum_LH = _mm_cvtepi32_ps(val_sum_LH); //to 32 bit float
                __m128 f_val_sum_LL = _mm_cvtepi32_ps(val_sum_LL); //to 32 bit float

                __m128 f_out_HH = _mm_div_ps (f_val_sum_HH, f_wgt_sum_HH);
                __m128 f_out_HL = _mm_div_ps (f_val_sum_HL, f_wgt_sum_HL);
                __m128 f_out_LH = _mm_div_ps (f_val_sum_LH, f_wgt_sum_LH);
                __m128 f_out_LL = _mm_div_ps (f_val_sum_LL, f_wgt_sum_LL);

                __m128i out_HH = _mm_cvtps_epi32(f_out_HH); //to 32 bits int
                __m128i out_HL = _mm_cvtps_epi32(f_out_HL); //to 32 bits int
                __m128i out_LH = _mm_cvtps_epi32(f_out_LH); //to 32 bits int
                __m128i out_LL = _mm_cvtps_epi32(f_out_LL); //to 32 bits int

                __m128i out_H = _mm_packs_epi32(out_HL, out_HH);
                __m128i out_L = _mm_packs_epi32(out_LL, out_LH);
                __m128i out = _mm_packus_epi16(out_L, out_H);

            //	_mm_storeu_si128((__m128i*)(dout + center), out);
                _mm_store_si128((__m128i*)(dout + center), out);
            }
        }
#else

        for(int y = blk_size; y < height - blk_size; y++)
        {
            for(int x = blk_size; x < width - blk_size; x++ )
            {
                int center = x + y * SPPW16; // address of the center pixel

                if(x == 277 && y == 108)
                    x = x;


                int val_sum = 0;
                int wgt_sum = 0;
                for( j = -blk_size; j <= blk_size; j+=2 ) // y-axis
                {
                    for( i = -blk_size; i <= blk_size; i+=2 ) // x-axis
                    {
                        int x1 = x+i;
                        int y1 = y+j;

                        int diff = abs(pLImg[center] - pLImg[y1 * SPPW16 + x1]);
                        int val = 0, wgt = 0;

                        wgt = max(0, 8 - ((diff*28)>>5));
                        val = din[y1 * SPPW16 + x1] * wgt;

                        wgt_sum += wgt;
                        val_sum += val;

                    }
                }

                if(wgt_sum)
                    dout[center] = val_sum / wgt_sum;
                else
                    dout[center] = 0;

            }
        }
#endif
        pPost->iPrevFromGPU = 0;

        swap(pDin, pDout);
    }
}


void HoleFill2(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    //PPOST_CL pCL = pPost->pCL;

    if(pPar->HF2_MODE == 0)
        return;

    unsigned char *din = pDin;
//	unsigned char *dout = pDout;

    if(pPar->HF2_MODE == 1)
    {
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++ )
            {
                if(x==65 && y==36)
                    x=x;

                int center = x + y * stride; // address of the center pixel
                int left = center - 1;
                int top = center - stride;

                if( pLImg[center] && (din[center] == 0) && (din[left] != 0 || din[top] != 0))
                {
                    int v0 = din[left] == 0? 255 : din[left]; //left
                    int v1 = din[top] == 0? 255 : din[top]; //top

                    din[center] = min(v0, v1);
                }
            //	else
            //		din[center] = din[center];

            }
        }
    }
    else
    {
        int fw_line[SPPW2];
        int bw_line[SPPW2];

        fw_line[0] = fw_line[SPPW1] = bw_line[0] = bw_line[SPPW1] = 0;

        int *fw_ptr = fw_line + 1; //for boundary issue
        int *bw_ptr = bw_line + 1;

        for(int y = 0; y < height; y++)
        {
            for(int x = width-1; x >= 0; x--)
            {
                int center = x + y * stride; // address of the center pixel
                int top = center - stride;

                if( pLImg[center] && (din[center] == 0) && (bw_ptr[x+1] != 0 || din[top] != 0))
                {
                    int v0 = bw_ptr[x+1] ? bw_ptr[x+1] : 255;	//right
                    int v1 = din[top] ? din[top] : 255;		//top

                    bw_ptr[x] = min(v0, v1);
                }
                else
                    bw_ptr[x] = din[center];
            }

            for(int x = 0; x < width; x++ )
            {
                int center = x + y * stride; // address of the center pixel
                int top = center - stride;

                if( pLImg[center] && (din[center] == 0) && (fw_ptr[x-1] != 0 || din[top] != 0))
                {
                    int v0 = fw_ptr[x-1] ? fw_ptr[x-1] : 255;	//left
                    int v1 = din[top] ? din[top] : 255;		//top

                    fw_ptr[x] = min(v0, v1);
                }
                else
                    fw_ptr[x] = din[center];

                if(fw_ptr[x] || bw_ptr[x])
                {
                    int v0 = fw_ptr[x] ? fw_ptr[x]: 255 ; //forward
                    int v1 = bw_ptr[x] ? bw_ptr[x]: 255 ; //backward

                    din[center] = min(v0, v1);
                }
                else
                    din[center] = 0;
            }
        }
    }

    pPost->iPrevFromGPU = 0;
}


void HugeRemove(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * &pDin, unsigned char * &pDout, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->HR_MODE == 0)
        return;

    if(0) //GPU, pPost->iOpenCL & (pPar->GPU_EN >> GPU_HR)
    {
        if(!pPost->iPrevFromGPU) //last stage from CPU
        {
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDin, 0, 0, 0);
            // write here temporarily
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_y_in, CL_TRUE, 0, stride*height, pLImg, 0, 0, 0);
        }
    //	else
    //	{
    //		clFinish(pCL->queue);
    //	}

        cl_kernel clHugeRemove = clCreateKernel(pCL->program, "HugeRemove", 0);

        clSetKernelArg(clHugeRemove, 0, sizeof(cl_mem), &pCL->cl_struct);
        clSetKernelArg(clHugeRemove, 1, sizeof(cl_mem), pCL->cl_y_in);
        clSetKernelArg(clHugeRemove, 2, sizeof(cl_mem), pCL->cl_d_in);
        clSetKernelArg(clHugeRemove, 3, sizeof(cl_mem), pCL->cl_d_out);
        clSetKernelArg(clHugeRemove, 4, sizeof(cl_mem), &pCL->cl_debug);

        size_t global_size[] = {(size_t)width, (size_t)height};
        clEnqueueNDRangeKernel(pCL->queue, clHugeRemove, 2, 0, global_size, 0, 0, 0, 0);

        clReleaseKernel(clHugeRemove);

        pPost->iPrevFromGPU = 1;

        swap(pCL->cl_d_in, pCL->cl_d_out);
    }
    else //CPU
    {

        unsigned char *din = pDin;
        unsigned char *dout = pDout;

        int hist[64+6];
        int r_valid[3];
        int l_valid[3];
        int d;
        int sum = 0, cnt = 0;

        int iCurve[9];

        iCurve[0] = pPar->HR_CURVE_0;
        iCurve[1] = pPar->HR_CURVE_1;
        iCurve[2] = pPar->HR_CURVE_2;
        iCurve[3] = pPar->HR_CURVE_3;
        iCurve[4] = pPar->HR_CURVE_4;
        iCurve[5] = pPar->HR_CURVE_5;
        iCurve[6] = pPar->HR_CURVE_6;
        iCurve[7] = pPar->HR_CURVE_7;
        iCurve[8] = pPar->HR_CURVE_8;

        for(int y = 0; y < height; y++)
        {
            memset(hist, 0, sizeof(hist));
            sum = 0;
            cnt = 0;

            for(int x = 0; x < width; x++ )
            {
                int center = x + y * stride; // address of the center pixel
                dout[center] = din[center];
                if(din[center])
                {
                    d = 3 + ((din[center]-1)>>2);

                    hist[d]++;
                    sum += din[center];
                    cnt++;
                }

                if(x >= 63)
                {
                    int pos_last = center - 63;
                    int pos_out = center - 32;
                    int d_last = 3 + ((din[pos_last]-1)>>2);
                    int d_out = 3 + ((din[pos_out]-1)>>2);

                    l_valid[0] = hist[d_out-1] ? 1: 0;
                    l_valid[1] = hist[d_out-2] && l_valid[0]? 1: 0;
                    l_valid[2] = hist[d_out-3] && l_valid[1]? 1: 0;
                    r_valid[0] = hist[d_out+1] ? 1: 0;
                    r_valid[1] = hist[d_out+2] && r_valid[0]? 1: 0;
                    r_valid[2] = hist[d_out+3] && r_valid[1]? 1: 0;

                    int sum = hist[d_out];

                    if(hist[d_out-1])
                    {
                        sum += hist[d_out-1];

                        if( hist[d_out-2])
                        {
                            sum += hist[d_out-2];
                            sum += hist[d_out-3];
                        }
                    }

                    if( hist[d_out+1])
                    {
                        sum += hist[d_out+1];
                        if( hist[d_out+2])
                        {
                            sum += hist[d_out+2];
                            sum += hist[d_out+3];
                        }
                    }

                    int thd, curve_idx;

                    curve_idx = din[pos_out] >> 5;
                    thd = iCurve[curve_idx] + ((iCurve[curve_idx+1] - iCurve[curve_idx])*(din[pos_out] & 0x1f)>>5);

                    if(sum < thd)
                    {
                        dout[pos_out] = 0;
                    }

                    if(din[pos_last])
                    {
                        hist[d_last]--;
                        sum -= din[pos_last];
                        cnt--;
                    }
                }
            }
        }

        pPost->iPrevFromGPU = 0;

        swap(pDin, pDout);
    }

}

void ZeroMask(PPOST_STRUCT pPost, unsigned char * pLImg, unsigned char * pDin, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->ZM_MODE == 0)
        return;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {
        if(!pPost->iPrevFromGPU) //last stage from CPU
        {
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDin, 0, 0, 0);
        }
    //	else
    //	{
    //		clFinish(pCL->queue);
    //	}

        cl_kernel clZeroMask = clCreateKernel(pCL->program, "ZeroMask", 0);

        clSetKernelArg(clZeroMask, 0, sizeof(cl_mem), &pCL->cl_struct);
        clSetKernelArg(clZeroMask, 1, sizeof(cl_mem), pCL->cl_y_in);
        clSetKernelArg(clZeroMask, 2, sizeof(cl_mem), pCL->cl_d_in);
        clSetKernelArg(clZeroMask, 3, sizeof(stride), &stride);
        clSetKernelArg(clZeroMask, 4, sizeof(cl_mem), &pCL->cl_debug);

        size_t global_size[] = {(size_t)width, (size_t)height};
        clEnqueueNDRangeKernel(pCL->queue, clZeroMask, 2, 0, global_size, 0, 0, 0, 0);

        clReleaseKernel(clZeroMask);

        pPost->iPrevFromGPU = 1;
    }
    else //CPU
    {
        unsigned char *din = pDin;

        if(pPost->iPrevFromGPU)
        {
            clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, din, 0, 0, 0);
        }

#ifdef _SSE2

        __m128i z = _mm_setzero_si128();

        for(int y = 0; y < height; y++)
        {
            for(int x= 0; x<width; x+=16)
            {

                __m128i out = _mm_andnot_si128(_mm_cmpeq_epi8(_mm_load_si128((__m128i*)(pLImg + y*SPPW16 + x)), z), _mm_load_si128((__m128i*)(din + y*SPPW16 + x)));


                _mm_store_si128((__m128i*)(din + y*SPPW16 + x), out);
            }

        }

#else

        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++ )
            {
                int center = x + y * SPPW16; // address of the center pixel

                din[center] = pLImg[center] ? din[center] : 0;
            }
        }

#endif
        pPost->iPrevFromGPU = 0;
    }
}

void Segment(PPOST_STRUCT pPost, unsigned char *pLImg, unsigned short *pSImg, unsigned char *&pDin, unsigned char *&pDout, unsigned char *pEImg, unsigned char *pIntraBuf,
             int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    //PPOST_CL pCL = pPost->pCL;

    if(pPar->SEG_MODE == 0)
        return;

    unsigned short *Segment = pSImg;
    unsigned short *SegmentTmp = pSImg + SPPW16*SPPH2;

    int iSub = pPar->SEG_THD_SUB;
    int iSlp = pPar->SEG_THD_SLP;
    int iMax = pPar->SEG_THD_MAX;
    int iMin = pPar->SEG_THD_MIN;


    unsigned int *seg_idx = (unsigned int *)pIntraBuf;
    unsigned int *seg_map = seg_idx + tab_size;
    unsigned int *seg_sum = seg_idx; //share the same buffer
    unsigned int *seg_cnt = seg_sum + tab_size;

    unsigned int *seg_min = seg_sum; //share the same buffer
    unsigned int *seg_hist = seg_sum; //share the same buffer

    for(int i=0; i<tab_size; i++)
    {
        seg_idx[i] = i;
    }

    int idx = 1;

    int line_seg[SPPW] = {0};
    int line_seg_idx[SPPW];
    int line_idx;

//	int grad[SPPW];

//	if(pPar->SEG_MODE == 1) // old version
    {
        for(int y=0; y<height; y++)
        {
            line_idx = 1;

            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int left = center - 1;

                int thd = min(iMax, max(iMin, iSlp*(pLImg[center] - iSub)>>6));

                if(pEImg[center] > thd)
                {
                    line_seg[x] = 0;
                }
                else
                {
                    if(x == 0)
                    {
                        line_seg[0] = 1;
                        line_idx++;
                    }
                    else
                    {
                        if(pEImg[left] <= thd)
                        {
                            line_seg[x] = line_seg[x-1];
                        }
                        else //new segment
                        {
                            line_seg[x] = line_idx;
                            line_idx++;
                        }
                    }
                }
            }

            memset(line_seg_idx, 0, sizeof(line_seg_idx));

            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int top = center - stride;

                int *ptr = &line_seg_idx[line_seg[x]];

                if(Segment[top] && line_seg[x])
                {
                    if(*ptr)
                    {
                        if(*ptr != Segment[top])
                        {
                            int min_idx = min(Segment[top], *ptr);
                            int max_idx = max(Segment[top], *ptr);
                            *ptr = min_idx;
                            seg_idx[max_idx] = min_idx;
                        }
                    }
                    else
                    {
                        *ptr = Segment[top];
                    }
                }
                else
                {
                    //check if small point, clear if true
                    if(x > 0 && x < width - 1)
                    {
                        if(!(line_seg[x-1] | line_seg[x+1]))
                            line_seg[x] = 0;
                    }
                }
            }

            if(y == 0)
            {
                //at 1st line, line segment = frame segment
                int max_idx = 0;
                for(int x= 0; x<width; x++)
                {
                    Segment[x] = line_seg[x];
                    max_idx = max(max_idx, line_seg[x]);
                }

                idx = max_idx+1;
            }
            else
            {
                for(int x= 0; x<width; x++)
                {
                    int center = y*stride + x;

                    if(line_seg[x])
                    {
                        if(line_seg_idx[line_seg[x]])
                        {
                            Segment[center] = line_seg_idx[line_seg[x]];
                        }
                        else
                        {
                            if(idx < tab_size)
                            {
                                line_seg_idx[line_seg[x]] = Segment[center] = idx;
                                idx++;
                            }
                            else
                            {
                                Segment[center] = 0;
                            }
                        }

                    }
                    else
                    {
                        Segment[center] = 0;
                    }
                }
            }
        }
    }

    for(int i = idx-1; i >= 1; i--)
    {
        int remap_idx = i;
        while(seg_idx[remap_idx] != (unsigned int)remap_idx)
        {
            remap_idx = seg_idx[remap_idx];
        }

        seg_map[i] = remap_idx;
    }

    seg_map[0] = 0;

    //segment remapping
    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x++)
        {
            int center = y*stride + x;

            Segment[center] = seg_map[Segment[center]];
        }
    }

    SegmentFill(pPost, pLImg, pSImg, SegmentTmp, width, height, stride);


    if(pPar->SEG_MODE == 1)
    {
        memset(seg_sum, 0, tab_size*sizeof(int));
        memset(seg_cnt, 0, tab_size*sizeof(int));

        //statistics
        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;

                if(pDin[center] && Segment[center])
                {
                    seg_sum[Segment[center]] += pDin[center];
                    seg_cnt[Segment[center]] ++;
                }
            }
        }

        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int seg = Segment[center];

                if(seg)
                {
                    unsigned char val = 0;
                    if(seg_cnt[seg])
                    {
                        val = seg_sum[seg]/seg_cnt[seg];

                        if(abs(val - pDin[center]) >= 20)
                            pDout[center] = val;
                        else
                            pDout[center] = pDin[center];
                    }
                    else
                    {
                        pDout[center] = pDin[center];
                    }
                }
                else
                {
                    pDout[center] = pDin[center];
                }
            }
        }
    }
    else if(pPar->SEG_MODE == 2)
    {
        memset(seg_min, 0xff, tab_size*sizeof(int));

        //statistics
        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;

                if(pDin[center] && Segment[center])
                {
                    if(seg_min[Segment[center]] > pDin[center])
                        seg_min[Segment[center]] = pDin[center];
                }
            }
        }

        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int seg = Segment[center];

                if(seg)
                {
                    unsigned char val;

                    val = seg_min[seg];

                    if(abs(val - pDin[center]) >= 20)
                        pDout[center] = val;
                    else
                        pDout[center] = pDin[center];

                }
                else
                {
                    pDout[center] = pDin[center];
                }
            }
        }
    }
    else if(pPar->SEG_MODE == 3)
    {
        memset(seg_hist, 0, 64*tab_size*sizeof(int));

        //statistics
        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;

                if(pDin[center] && Segment[center])
                {
                    seg_hist[(Segment[center]<<6) + (pDin[center]>>2)]++;
                }
            }
        }

        //get max cnt in hist for each segment
        for(int i=0; i<idx; i++)
        {
            unsigned int max_cnt = 0;
            unsigned int val = 0;
            for(int j=0; j<64; j++)
            {
                if(max_cnt < seg_hist[(i<<6) + j])
                {
                    max_cnt = seg_hist[(i<<6) + j];
                    val = j;
                }
            }
            seg_hist[i] = (val<<2) + 3;
        }

        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int seg = Segment[center];

                if(seg)
                {
                    unsigned char val;

                    val = seg_hist[seg];

                    if(abs(val - pDin[center]) >= 20)
                        pDout[center] = val;
                    else
                        pDout[center] = pDin[center];

                }
                else
                {
                    pDout[center] = pDin[center];
                }
            }
        }
    }

    pPost->iPrevFromGPU = 0;

    swap(pDin, pDout);
}

void SegmentFill(PPOST_STRUCT pPost, unsigned char *pLImg, unsigned short *&pSImg, unsigned short *&pTmp, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    //PPOST_CL pCL = pPost->pCL;

    if(pPar->SEG_FILL_MODE == 0)
        return;

    for(int i=0; i< pPar->SEG_FILL_MODE; i++)
    {
        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int top = center - stride;
                int bot = center + stride;
                int lft = center - 1;
                int rgt = center + 1;
                int t_l = center - stride - 1;
                int t_r = center - stride + 1;
                int b_l = center + stride - 1;
                int b_r = center + stride + 1;

                if(x == 414 && y == 162)
                    x=x;

                if(pSImg[center] == 0) //no segmentation idx
                {
                    int diff_top = abs(pLImg[center] - pLImg[top]);
                    int diff_bot = abs(pLImg[center] - pLImg[bot]);
                    int diff_lft = abs(pLImg[center] - pLImg[lft]);
                    int diff_rgt = abs(pLImg[center] - pLImg[rgt]);

                    diff_top = pSImg[top] ? diff_top : 0xffff;
                    diff_bot = pSImg[bot] ? diff_bot : 0xffff;
                    diff_lft = pSImg[lft] ? diff_lft : 0xffff;
                    diff_rgt = pSImg[rgt] ? diff_rgt : 0xffff;

                    if(diff_top < 4 || diff_bot < 4 || diff_lft < 4 || diff_rgt < 4)
                    {
                        int diff_min = diff_top;
                        int case_val = 0;
                        if(diff_lft < diff_min)
                        {
                            diff_min = diff_lft;
                            case_val = 1;
                        }
                        else if(diff_rgt < diff_min)
                        {
                            diff_min = diff_rgt;
                            case_val = 2;
                        }
                        else if(diff_bot < diff_min)
                        {
                            diff_min = diff_bot;
                            case_val = 3;
                        }

                        switch(case_val)
                        {
                            case 0:
                                pTmp[center] = pSImg[top];
                            break;

                            case 1:
                                pTmp[center] = pSImg[lft];
                            break;

                            case 2:
                                pTmp[center] = pSImg[rgt];
                            break;

                            case 3:
                                pTmp[center] = pSImg[bot];
                            break;
                        }
                    }
                    else
                    {
                        pTmp[center] = pSImg[center];
                    }
                }
                else
                {
                    pTmp[center] = pSImg[center];
                }
            }
        }

        swap(pSImg, pTmp);
    }

    pPost->iPrevFromGPU = 0;
}

void Fragment(PPOST_STRUCT pPost, unsigned char *pLImg, unsigned short *pSImg, unsigned char *&pDin, unsigned char *&pDout, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;


    if(pPar->FC_MODE == 0)
        return;

    unsigned short *Segment = pSImg;

    int seg_idx[tab_size];
    int seg_cnt[tab_size];

    memset(seg_cnt, 0, sizeof(seg_cnt));

    for(int i=0; i<tab_size; i++)
    {
        seg_idx[i] = i;
    }

    int edge_thd = pPar->FC_EDGE_THD;
    int area_thd = pPar->FC_AREA_THD;
    int idx;

    int line_seg[SPPW];
    int line_seg_idx[SPPW];
    int line_idx;

    if(pPost->iPrevFromGPU)
    {
        clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDin, 0, 0, 0);
    }

    for(int y=0; y<height; y++)
    {
        line_idx = 2;

        for(int x= 0; x<width; x++)
        {
            int center = y*stride + x;
            int left = center-1;

            if(pDin[center])
            {
                if(abs(pDin[left]-pDin[center]) <= edge_thd && pDin[left])
                {
                    line_seg[x] = line_seg[x-1];
                }
                else //new segment
                {
                    line_seg[x] = line_idx;
                    line_idx++;
                }
            }
            else
            {
                line_seg[x] = 0;
            }
        }

        memset(line_seg_idx, 0, sizeof(line_seg_idx));

        if(y == 0)
        {
            //skip
        }
        else
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;
                int top = center - stride;

                int *ptr = &line_seg_idx[line_seg[x]];

                if(abs(pDin[top]-pDin[center]) <= edge_thd && Segment[top] && pDin[top])
                {
                    if(*ptr)
                    {
                        if(*ptr != Segment[top])
                        {
                            int min_idx = min(Segment[top], *ptr);
                            int max_idx = max(Segment[top], *ptr);
                            *ptr = min_idx;
                            seg_idx[max_idx] = min_idx;
                        }
                    }
                    else
                    {
                        *ptr = Segment[top];
                    }
                }
                else
                {
                    //check if small point, clear if true
                    if(x > 0 && x < width - 1)
                    {
                        if(line_seg[x] != line_seg[x-1] && line_seg[x] !=line_seg[x+1])
                            line_seg[x] = 0;
                    }
                }
            }
        }

        if(y == 0)
        {
            //at 1st line, line segment = frame segment
            int max_idx = 0;
            for(int x= 0; x<width; x++)
            {
                Segment[x] = line_seg[x];
                max_idx = max(max_idx, line_seg[x]);

                seg_cnt[Segment[x]] ++;
            }

            idx = max_idx+1;
        }
        else
        {
            for(int x= 0; x<width; x++)
            {
                int center = y*stride + x;

                if(line_seg[x])
                {
                    if(line_seg_idx[line_seg[x]])
                    {
                        Segment[center] = line_seg_idx[line_seg[x]];

                        seg_cnt[Segment[center]] ++;
                    }
                    else
                    {
                        if(idx < tab_size)
                        {
                            line_seg_idx[line_seg[x]] = Segment[center] = idx;
                            idx++;

                            seg_cnt[Segment[center]] ++;
                        }
                        else
                        {
                            Segment[center] = 1; //don't care value
                        }
                    }
                }
                else
                {
                    Segment[center] = 0;
                }
            }
        }
    }

    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x++)
        {
            int center = y*stride + x;
            int seg = Segment[center];


            while(seg_idx[seg] != seg)
            {
                seg = seg_idx[seg];
            }

            Segment[center] = seg;
        }
    }

    seg_cnt[1] = 0xffff; //make sure seg 1 (don't care) will keep the same depth.
    for(int i = idx-1; i > 1; i--) // 2 to idx-1, don't need to count seg 0 and 1
    {
        if(seg_idx[i] != i)
        {
        //	seg_sum[seg_idx[i]] += seg_sum[i];
            seg_cnt[seg_idx[i]] += seg_cnt[i];
        }
    }

    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x++)
        {
            int center = y*stride + x;
            int seg = Segment[center];

            if(seg_cnt[seg] < area_thd || Segment[center] == 0)
                pDout[center] = 0;
            else
                pDout[center] = pDin[center];

        }
    }

    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++ )
        {

            int center = x + y * stride; // address of the center pixel
            int left = center - 1;
            int top = center - stride;

            if(pLImg[center] && pDout[center] == 0 && (pDout[left] != 0 || pDout[top] != 0))
            {
                int v0 = pDout[left] == 0? 255 : pDout[left]; //left
                int v1 = pDout[top] == 0? 255 : pDout[top]; //top

                pDout[center] = min(v0, v1);
            }
        }
    }

    pPost->iPrevFromGPU = 0;

    swap(pDin, pDout);
}

void Sobel(PPOST_STRUCT pPost, unsigned char *pIn, unsigned char *pOut, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {

        cl_kernel clSobel = clCreateKernel(pCL->program, "Sobel", 0);

        clSetKernelArg(clSobel, 0, sizeof(cl_mem), &pCL->cl_struct);
        clSetKernelArg(clSobel, 1, sizeof(cl_mem), pCL->cl_y_in);
        clSetKernelArg(clSobel, 2, sizeof(cl_mem), &pCL->cl_edge);
        clSetKernelArg(clSobel, 3, sizeof(stride), &stride);
        clSetKernelArg(clSobel, 4, sizeof(cl_mem), &pCL->cl_debug);

        size_t global_size[] = {(size_t)width, (size_t)height};
        clEnqueueNDRangeKernel(pCL->queue, clSobel, 2, 0, global_size, 0, 0, 0, 0);

        //write to CPU edge buffer, because most of function which used it are in CPU code.
        clEnqueueReadBuffer(pCL->queue, pCL->cl_edge, CL_TRUE, 0, stride*height, pOut, 0, 0, 0);

        clReleaseKernel(clSobel);
    }
    else //CPU
    {
        int v;

        if(pPar->GRAD_MODE)
            v = 2;
        else
            v = 1;

#ifdef _SSE2

        __m128i z = _mm_setzero_si128();

        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x+=16)
            {
                int x0, x1, y0_width, y1_width, y_width;
                x0 = x-v;
                x1 = x+v;

                y_width = SPPW16 * y;
                y0_width = SPPW16 * (y-v);
                y1_width = SPPW16 * (y+v);

                __m128i Hh, Hl, Vh, Vl, D1h, D1l, D2h, D2l;

                // (x, y)

                //+1x in H (-1, -1) and +1x in V (-1, -1) and +2x in D1 (-1, -1)
                __m128i P0 = _mm_loadu_si128((__m128i*)(pIn + y0_width + x0));
                Vh = Hh = _mm_unpackhi_epi8(P0, z);
                Vl = Hl = _mm_unpacklo_epi8(P0, z);
                D1h = _mm_slli_epi16(Vh,1);
                D1l = _mm_slli_epi16(Vl,1);
                D2h = z;
                D2l = z;

                //+2x in V (0, -1), +1x in D1 (0, -1) - 1x in D2 (0, -1)
                __m128i P1 = _mm_loadu_si128((__m128i*)(pIn + y0_width + x));
                __m128i P1h = _mm_unpackhi_epi8(P1, z);
                __m128i P1l = _mm_unpacklo_epi8(P1, z);
                Vh = _mm_add_epi16(Vh,_mm_slli_epi16(P1h,1));
                Vl = _mm_add_epi16(Vl,_mm_slli_epi16(P1l,1));
                D1h = _mm_add_epi16(D1h,P1h);
                D1l = _mm_add_epi16(D1l,P1l);
                D2h = _mm_sub_epi16(D2h,P1h);
                D2l = _mm_sub_epi16(D2l,P1l);

                //-1x in H (1, -1) and +1x in V (1, -1) and -2x in D2 (1, -1)
                __m128i P2 = _mm_loadu_si128((__m128i*)(pIn + y0_width + x1));
                __m128i P2h = _mm_unpackhi_epi8(P2, z);
                __m128i P2l = _mm_unpacklo_epi8(P2, z);
                Hh = _mm_sub_epi16(Hh,P2h);
                Hl = _mm_sub_epi16(Hl,P2l);
                Vh = _mm_add_epi16(Vh,P2h);
                Vl = _mm_add_epi16(Vl,P2l);
                D2h = _mm_sub_epi16(D2h,_mm_slli_epi16(P2h,1));
                D2l = _mm_sub_epi16(D2l,_mm_slli_epi16(P2l,1));

                //+2x in H (-1, 0) and +1x in D1 (-1, 0) and +1x in D2 (-1, 0)
                __m128i P3 = _mm_loadu_si128((__m128i*)(pIn + y_width + x0));
                __m128i P3h = _mm_unpackhi_epi8(P3, z);
                __m128i P3l = _mm_unpacklo_epi8(P3, z);
                Hh = _mm_add_epi16(Hh,_mm_slli_epi16(P3h,1));
                Hl = _mm_add_epi16(Hl,_mm_slli_epi16(P3l,1));
                D1h = _mm_add_epi16(D1h, P3h);
                D1l = _mm_add_epi16(D1l, P3l);
                D2h = _mm_add_epi16(D2h, P3h);
                D2l = _mm_add_epi16(D2l, P3l);

                //-2x in H (1, 0) and -1x in D1 (1, 0) and -1x in D2 (1, 0)
                __m128i P4 = _mm_loadu_si128((__m128i*)(pIn + y_width + x1));
                __m128i P4h = _mm_unpackhi_epi8(P4, z);
                __m128i P4l = _mm_unpacklo_epi8(P4, z);
                Hh = _mm_sub_epi16(Hh,_mm_slli_epi16(P4h,1));
                Hl = _mm_sub_epi16(Hl,_mm_slli_epi16(P4l,1));
                D1h = _mm_sub_epi16(D1h,P4h);
                D1l = _mm_sub_epi16(D1l,P4l);
                D2h = _mm_sub_epi16(D2h,P4h);
                D2l = _mm_sub_epi16(D2l,P4l);

                //+1x in H (-1, 1) and -1x in V (-1, 1) and +2x in D2 (-1, 1)
                __m128i P5 = _mm_loadu_si128((__m128i*)(pIn + y1_width + x0));
                __m128i P5h = _mm_unpackhi_epi8(P5, z);
                __m128i P5l = _mm_unpacklo_epi8(P5, z);
                Hh = _mm_add_epi16(Hh,P5h);
                Hl = _mm_add_epi16(Hl,P5l);
                Vh = _mm_sub_epi16(Vh,P5h);
                Vl = _mm_sub_epi16(Vl,P5l);
                D2h = _mm_add_epi16(D2h,_mm_slli_epi16(P5h,1));
                D2l = _mm_add_epi16(D2l,_mm_slli_epi16(P5l,1));

                //-2x in V (0, 1) and -1x in D1 (0, 1) and +1x in D2 (0, 1)
                __m128i P6 = _mm_loadu_si128((__m128i*)(pIn + y1_width + x));
                __m128i P6h = _mm_unpackhi_epi8(P6, z);
                __m128i P6l = _mm_unpacklo_epi8(P6, z);
                Vh = _mm_sub_epi16(Vh,_mm_slli_epi16(P6h,1));
                Vl = _mm_sub_epi16(Vl,_mm_slli_epi16(P6l,1));
                D1h = _mm_sub_epi16(D1h,P6h);
                D1l = _mm_sub_epi16(D1l,P6l);
                D2h = _mm_add_epi16(D2h,P6h);
                D2l = _mm_add_epi16(D2l,P6l);

                //-1x in H (1, 1) and -1x in V (1, 1) and -2x in D1(1, 1)
                __m128i P7 = _mm_loadu_si128((__m128i*)(pIn + y1_width + x1));
                __m128i P7h = _mm_unpackhi_epi8(P7, z);
                __m128i P7l = _mm_unpacklo_epi8(P7, z);
                Hh = _mm_sub_epi16(Hh,P7h);
                Hl = _mm_sub_epi16(Hl,P7l);
                Vh = _mm_sub_epi16(Vh,P7h);
                Vl = _mm_sub_epi16(Vl,P7l);
                D1h = _mm_sub_epi16(D1h,_mm_slli_epi16(P7h,1));
                D1l = _mm_sub_epi16(D1l,_mm_slli_epi16(P7l,1));

                __m128i absHh = _mm_sub_epi16( _mm_max_epi16( Hh, z ), _mm_min_epi16( Hh, z ) );
                __m128i absHl = _mm_sub_epi16( _mm_max_epi16( Hl, z ), _mm_min_epi16( Hl, z ) );
                __m128i absVh = _mm_sub_epi16( _mm_max_epi16( Vh, z ), _mm_min_epi16( Vh, z ) );
                __m128i absVl = _mm_sub_epi16( _mm_max_epi16( Vl, z ), _mm_min_epi16( Vl, z ) );
                __m128i absD1h = _mm_sub_epi16( _mm_max_epi16( D1h, z ), _mm_min_epi16( D1h, z ) );
                __m128i absD1l = _mm_sub_epi16( _mm_max_epi16( D1l, z ), _mm_min_epi16( D1l, z ) );
                __m128i absD2h = _mm_sub_epi16( _mm_max_epi16( D2h, z ), _mm_min_epi16( D2h, z ) );
                __m128i absD2l = _mm_sub_epi16( _mm_max_epi16( D2l, z ), _mm_min_epi16( D2l, z ) );
                __m128i GradH = _mm_max_epi16(_mm_adds_epu16( absVh, absHh), _mm_adds_epu16( absD1h, absD2h));
                __m128i GradL = _mm_max_epi16(_mm_adds_epu16( absVl, absHl), _mm_adds_epu16( absD1l, absD2l));
                __m128i out = _mm_packs_epi16(GradL, GradH);


                _mm_store_si128((__m128i*)(pOut + y*SPPW16 + x), out);
            }
        }

#else
        for(int y=0; y<height; y++)
        {
            for(int x= 0; x<width; x++)
            {
                int Hgrad, Vgrad, D1grad, D2grad;

                int x0, x1, y0_width, y1_width, y_width;
                x0 = x-v;
                x1 = x+v;

                y_width = SPPW16 * y;
                y0_width = SPPW16 * (y-v);
                y1_width = SPPW16 * (y+v);

                Hgrad =	abs(	1*pIn[y0_width + x0] + 0*pIn[y0_width + x  ] - 1*pIn[y0_width + x1] +
                                2*pIn[y_width  + x0] + 0*pIn[y_width  + x  ] - 2*pIn[y_width  + x1] +
                                1*pIn[y1_width + x0] + 0*pIn[y1_width + x  ] - 1*pIn[y1_width + x1]);

                Vgrad =	abs(	1*pIn[y0_width + x0] + 2*pIn[y0_width + x  ] + 1*pIn[y0_width + x1] +
                                0*pIn[y_width  + x0] + 0*pIn[y_width  + x  ] + 0*pIn[y_width  + x1] +
                                -1*pIn[y1_width + x0] - 2*pIn[y1_width + x  ] - 1*pIn[y1_width + x1]);

                D1grad = abs(	2*pIn[y0_width + x0] + 1*pIn[y0_width + x  ] + 0*pIn[y0_width + x1] +
                                1*pIn[y_width  + x0] + 0*pIn[y_width  + x  ] - 1*pIn[y_width  + x1] +
                                0*pIn[y1_width + x0] - 1*pIn[y1_width + x  ] - 2*pIn[y1_width + x1]);

                D2grad = abs(	0*pIn[y0_width + x0] - 1*pIn[y0_width + x  ] - 2*pIn[y0_width + x1] +
                                1*pIn[y_width  + x0] + 0*pIn[y_width  + x  ] - 1*pIn[y_width  + x1] +
                                2*pIn[y1_width + x0] + 1*pIn[y1_width + x  ] - 0*pIn[y1_width + x1]);

                int diff = max(Hgrad+Vgrad, D1grad+D2grad);

                pOut[y*SPPW16 + x] = min(127, diff);
            }
        }
#endif

    }

}



void TemporalIIR(PPOST_STRUCT pPost, unsigned char * img_in, unsigned char * img_tin,
                         unsigned char * &din, unsigned char * &dout, unsigned char * disp_tin, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->TEMP1_MODE == 0)
        return;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {
        if(!pPost->iPrevFromGPU) //last stage from CPU
        {
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, din, 0, 0, 0);
        }
    //	else
    //	{
    //		clFinish(pCL->queue);
    //	}

        cl_kernel clTemporalIIR2 = clCreateKernel(pCL->program, "TemporalIIR2", 0);
        clSetKernelArg(clTemporalIIR2, 0, sizeof(cl_mem), &pCL->cl_struct);
        clSetKernelArg(clTemporalIIR2, 1, sizeof(cl_mem), pCL->cl_y_in);
        clSetKernelArg(clTemporalIIR2, 2, sizeof(cl_mem), pCL->cl_y_prev);
        clSetKernelArg(clTemporalIIR2, 3, sizeof(cl_mem), pCL->cl_d_in);
        clSetKernelArg(clTemporalIIR2, 4, sizeof(cl_mem), &pCL->cl_d_prev);
        clSetKernelArg(clTemporalIIR2, 5, sizeof(cl_mem), pCL->cl_d_out);
        clSetKernelArg(clTemporalIIR2, 6, sizeof(stride), &stride);
        clSetKernelArg(clTemporalIIR2, 7, sizeof(cl_mem), &pCL->cl_debug);

        size_t global_size[] = {(size_t)width, (size_t)height};
        clEnqueueNDRangeKernel(pCL->queue, clTemporalIIR2, 2, 0, global_size, 0, 0, 0, 0);

        clReleaseKernel(clTemporalIIR2);

        pPost->iPrevFromGPU = 1;

        swap(pCL->cl_d_in, pCL->cl_d_out);
    }
    else //CPU
    {
        if(pPost->iPrevFromGPU)
        {
            clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, din, 0, 0, 0);
            // pCL->cl_y_in has given to img_in in last stage
            clEnqueueReadBuffer(pCL->queue, *pCL->cl_y_prev, CL_TRUE, 0, stride*height, img_tin, 0, 0, 0);
        }

        int thd = pPar->TEMP1_THD;
        int level = (pPar->TEMP1_LEVEL > 64)? 64 : pPar->TEMP1_LEVEL;

        int Flag = (pPost->iFrameCnt <= pPar->TEMP0_MODE);

        if(!Flag)
        {

#ifdef _SSE2
            __m128i z = _mm_setzero_si128();

            if(pPar->TEMP1_MODE == 1)
            {
                for(int y=0; y<height; y++)
                {
                    for(int x=0; x<width; x+=16)
                    {
                        __m128i Pc0 = _mm_load_si128((__m128i*)(img_in + y*SPPW16 + x));
                        __m128i ValH = _mm_unpackhi_epi8(Pc0, z);
                        __m128i ValL = _mm_unpacklo_epi8(Pc0, z);

                        __m128i Pc1 = _mm_loadu_si128((__m128i*)(img_in + y*SPPW16 + x-1));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc1, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc1, z));

                        __m128i Pc2 = _mm_loadu_si128((__m128i*)(img_in + y*SPPW16 + x+1));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc2, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc2, z));

                        __m128i Pc3 = _mm_loadu_si128((__m128i*)(img_in + (y-1)*SPPW16 + x));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc3, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc3, z));

                        __m128i Pc4 = _mm_loadu_si128((__m128i*)(img_in + (y+1)*SPPW16 + x));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc4, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc4, z));

                        __m128i Pt0 = _mm_load_si128((__m128i*)(img_tin + y*SPPW16 + x));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt0, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt0, z));

                        __m128i Pt1 = _mm_loadu_si128((__m128i*)(img_tin + y*SPPW16 + x-1));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt1, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt1, z));

                        __m128i Pt2 = _mm_loadu_si128((__m128i*)(img_tin + y*SPPW16 + x+1));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt2, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt2, z));

                        __m128i Pt3 = _mm_loadu_si128((__m128i*)(img_tin + (y-1)*SPPW16 + x));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt3, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt3, z));

                        __m128i Pt4 = _mm_loadu_si128((__m128i*)(img_tin + (y+1)*SPPW16 + x));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt4, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt4, z));

                        __m128i AbsValH = _mm_sub_epi16( _mm_max_epi16( ValH, z ), _mm_min_epi16( ValH, z ) );
                        __m128i AbsValL = _mm_sub_epi16( _mm_max_epi16( ValL, z ), _mm_min_epi16( ValL, z ) );


                        __m128i Dc = _mm_load_si128((__m128i*)(din + y*SPPW16 + x));
                        __m128i Dt = _mm_load_si128((__m128i*)(disp_tin + y*SPPW16 + x));

                        __m128i _thd = _mm_set1_epi16(thd);
                        __m128i cmpH = _mm_cmpgt_epi16(AbsValH, _thd);
                        __m128i cmpL = _mm_cmpgt_epi16(AbsValL, _thd);

                        __m128i DcH = _mm_unpackhi_epi8(Dc, z);
                        __m128i DcL = _mm_unpacklo_epi8(Dc, z);
                        __m128i DtH = _mm_unpackhi_epi8(Dt, z);
                        __m128i DtL = _mm_unpacklo_epi8(Dt, z);

                        __m128i MaxH = _mm_max_epi16(DcH, DtH);
                        __m128i MaxL = _mm_max_epi16(DcL, DtL);
                        __m128i MinH = _mm_min_epi16(DcH, DtH);
                        __m128i MinL = _mm_min_epi16(DcL, DtL);

                        DtH = _mm_add_epi16(DcH, _mm_srai_epi16(_mm_mullo_epi16(_mm_sub_epi16(DtH, DcH), _mm_set1_epi16(level)), 6));
                        DtL = _mm_add_epi16(DcL, _mm_srai_epi16(_mm_mullo_epi16(_mm_sub_epi16(DtL, DcL), _mm_set1_epi16(level)), 6));

                        __m128i outL = _mm_or_si128(_mm_and_si128(cmpL, DcL), _mm_andnot_si128(cmpL, DtL));
                        __m128i outH = _mm_or_si128(_mm_and_si128(cmpH, DcH), _mm_andnot_si128(cmpH, DtH));

                        _mm_store_si128((__m128i*)(dout + y*SPPW16 + x), _mm_packus_epi16(outL, outH));
                    }
                }
            }
            else //if(pPar->TEMP1_MODE == 2)
            {
                for(int y=0; y<height; y++)
                {
                    for(int x=0; x<width; x+=16)
                    {
                //		if(x == 80 && y == 402)
                //		x=  x;

                        __m128i Pc0 = _mm_load_si128((__m128i*)(img_in + y*SPPW16 + x));
                        __m128i ValH = _mm_unpackhi_epi8(Pc0, z);
                        __m128i ValL = _mm_unpacklo_epi8(Pc0, z);

                        __m128i Pc1 = _mm_loadu_si128((__m128i*)(img_in + y*SPPW16 + x-1));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc1, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc1, z));

                        __m128i Pc2 = _mm_loadu_si128((__m128i*)(img_in + y*SPPW16 + x+1));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc2, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc2, z));

                        __m128i Pc3 = _mm_loadu_si128((__m128i*)(img_in + (y-1)*SPPW16 + x));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc3, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc3, z));

                        __m128i Pc4 = _mm_loadu_si128((__m128i*)(img_in + (y+1)*SPPW16 + x));
                        ValH = _mm_add_epi16(ValH, _mm_unpackhi_epi8(Pc4, z));
                        ValL = _mm_add_epi16(ValL, _mm_unpacklo_epi8(Pc4, z));

                        __m128i Pt0 = _mm_load_si128((__m128i*)(img_tin + y*SPPW16 + x));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt0, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt0, z));

                        __m128i Pt1 = _mm_loadu_si128((__m128i*)(img_tin + y*SPPW16 + x-1));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt1, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt1, z));

                        __m128i Pt2 = _mm_loadu_si128((__m128i*)(img_tin + y*SPPW16 + x+1));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt2, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt2, z));

                        __m128i Pt3 = _mm_loadu_si128((__m128i*)(img_tin + (y-1)*SPPW16 + x));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt3, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt3, z));

                        __m128i Pt4 = _mm_loadu_si128((__m128i*)(img_tin + (y+1)*SPPW16 + x));
                        ValH = _mm_sub_epi16(ValH, _mm_unpackhi_epi8(Pt4, z));
                        ValL = _mm_sub_epi16(ValL, _mm_unpacklo_epi8(Pt4, z));

                        __m128i AbsValH = _mm_sub_epi16( _mm_max_epi16( ValH, z ), _mm_min_epi16( ValH, z ) );
                        __m128i AbsValL = _mm_sub_epi16( _mm_max_epi16( ValL, z ), _mm_min_epi16( ValL, z ) );


                        __m128i Dc = _mm_load_si128((__m128i*)(din + y*SPPW16 + x));
                        __m128i Dt = _mm_load_si128((__m128i*)(disp_tin + y*SPPW16 + x));

                        __m128i _thd = _mm_set1_epi16(thd);
                        __m128i cmpH = _mm_cmpgt_epi16(AbsValH, _thd);
                        __m128i cmpL = _mm_cmpgt_epi16(AbsValL, _thd);

                        __m128i DcH = _mm_unpackhi_epi8(Dc, z);
                        __m128i DcL = _mm_unpacklo_epi8(Dc, z);
                        __m128i DtH = _mm_unpackhi_epi8(Dt, z);
                        __m128i DtL = _mm_unpacklo_epi8(Dt, z);

                        __m128i MaxH = _mm_max_epi16(DcH, DtH);
                        __m128i MaxL = _mm_max_epi16(DcL, DtL);
                        __m128i MinH = _mm_min_epi16(DcH, DtH);
                        __m128i MinL = _mm_min_epi16(DcL, DtL);

                        DtH = _mm_sub_epi16(MaxH, _mm_srai_epi16(_mm_mullo_epi16(_mm_sub_epi16(MaxH, MinH), _mm_set1_epi16(level)), 6));
                        DtL = _mm_sub_epi16(MaxL, _mm_srai_epi16(_mm_mullo_epi16(_mm_sub_epi16(MaxL, MinL), _mm_set1_epi16(level)), 6));

                        __m128i outL = _mm_or_si128(_mm_and_si128(cmpL, DcL), _mm_andnot_si128(cmpL, DtL));
                        __m128i outH = _mm_or_si128(_mm_and_si128(cmpH, DcH), _mm_andnot_si128(cmpH, DtH));

                        _mm_store_si128((__m128i*)(dout + y*SPPW16 + x), _mm_packus_epi16(outL, outH));
                    }
                }
            }
#else
            unsigned char *Pc, *Pt;
            int Dc, Dt;

            if(pPar->TEMP1_MODE == 1)
            {
                for(int y=0; y<height; y++)
                {
                    for(int x=0; x<width; x++)
                    {
                        Dc = din[y*SPPW16 + x];
                        Dt = disp_tin[y*SPPW16 + x];
                        Pc = img_in + y*SPPW16 + x;
                        Pt = img_tin + y*SPPW16 + x;

                        int B[5];
                        B[0] = Pc[-2] - Pt[-2];
                        B[1] = Pc[-1] - Pt[-1];
                        B[2] = Pc[0] - Pt[0];
                        B[3] = Pc[1] - Pt[1];
                        B[4] = Pc[2] - Pt[2];

                        int area_diff = (B[0]+B[1]+B[2]+B[3]+B[4]);

                        int out;


                        if(abs(area_diff) > thd)
                            out = Dc;
                        else
                        {
                        //	if(Dt - Dc > 0)
                                out = Dc + (((Dt - Dc)*level)>>6);
                        //	else
                        //		out = Dc - (((Dc - Dt)*level)>>6);
                        }

                        dout[(y*SPPW16 + x)] = out;
                    }
                }
            }
            else //if(pPar->TEMP1_MODE == 2)
            {
                for(int y=0; y<height; y++)
                {
                    for(int x=0; x<width; x++)
                    {
                        Dc = din[y*SPPW16 + x];
                        Dt = disp_tin[y*SPPW16 + x];
                        Pc = img_in + y*SPPW16 + x;
                        Pt = img_tin + y*SPPW16 + x;

                        int B[5];
                        B[0] = Pc[-2] - Pt[-2];
                        B[1] = Pc[-1] - Pt[-1];
                        B[2] = Pc[0] - Pt[0];
                        B[3] = Pc[1] - Pt[1];
                        B[4] = Pc[2] - Pt[2];

                        int area_diff = (B[0]+B[1]+B[2]+B[3]+B[4]);

                        int out;


                        if(abs(area_diff) > thd)
                            out = Dc;
                        else
                        {
                            int min_val = min(Dc, Dt);
                            int max_val = max(Dc, Dt);
                            out = max_val - (((max_val - min_val)*level)>>6);

                            //new, not in SSE
                            if(abs(out - Dc) < 10)
                                out = Dc;
                        }

                        dout[(y*SPPW16 + x)] = out;
                    }
                }
            }
#endif

        }
        else
        {
            memcpy(dout, din, SPPW16*height);
        }

        memcpy(disp_tin, dout, SPPW16*height);

        pPost->iPrevFromGPU = 0;

        swap(din, dout);
    }
}

void TemporalMedian(PPOST_STRUCT pPost, unsigned char * &img_in, unsigned char * &img_prev, unsigned char ** img_t,
                         unsigned char * &din, unsigned char ** d_t,
                         unsigned char * &dout, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {
        if(pPar->TEMP0_MODE == 0)
        {
            swap(pCL->cl_y_t[0], pCL->cl_y_t[1]);
            return;
        }

        if(!pPost->iPrevFromGPU) //last stage from CPU
        {
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, din, 0, 0, 0);
        }
    //	else
    //	{
    //		clFinish(pCL->queue);
    //	}

        if(pPar->TEMP0_MODE == 2)
        {
            cl_kernel clTemporalMed2 = clCreateKernel(pCL->program, "TemporalMed2", 0);

            clSetKernelArg(clTemporalMed2, 0, sizeof(cl_mem), &pCL->cl_struct);
            clSetKernelArg(clTemporalMed2, 1, sizeof(cl_mem), pCL->cl_y_t[2]);
            clSetKernelArg(clTemporalMed2, 2, sizeof(cl_mem), pCL->cl_y_t[3]);
            clSetKernelArg(clTemporalMed2, 3, sizeof(cl_mem), pCL->cl_d_in);
            clSetKernelArg(clTemporalMed2, 4, sizeof(cl_mem), pCL->cl_d_t[0]);
            clSetKernelArg(clTemporalMed2, 5, sizeof(cl_mem), pCL->cl_d_t[1]);
            clSetKernelArg(clTemporalMed2, 6, sizeof(cl_mem), pCL->cl_d_t[2]);
            clSetKernelArg(clTemporalMed2, 7, sizeof(cl_mem), pCL->cl_d_t[3]);
            clSetKernelArg(clTemporalMed2, 8, sizeof(cl_mem), pCL->cl_d_out);
            clSetKernelArg(clTemporalMed2, 9, sizeof(stride), &stride);
            clSetKernelArg(clTemporalMed2, 10, sizeof(cl_mem), &pCL->cl_debug);

            size_t global_size[] = {(size_t)width, (size_t)height};
            clEnqueueNDRangeKernel(pCL->queue, clTemporalMed2, 2, 0, global_size, 0, 0, 0, 0);

            clReleaseKernel(clTemporalMed2);
        }
        else
        {
            cl_kernel clTemporalMed1 = clCreateKernel(pCL->program, "TemporalMed1", 0);

            clSetKernelArg(clTemporalMed1, 0, sizeof(cl_mem), &pCL->cl_struct);
            clSetKernelArg(clTemporalMed1, 1, sizeof(cl_mem), pCL->cl_y_t[1]);
            clSetKernelArg(clTemporalMed1, 2, sizeof(cl_mem), pCL->cl_y_t[2]);
            clSetKernelArg(clTemporalMed1, 3, sizeof(cl_mem), pCL->cl_d_in);
            clSetKernelArg(clTemporalMed1, 4, sizeof(cl_mem), pCL->cl_d_t[0]);
            clSetKernelArg(clTemporalMed1, 5, sizeof(cl_mem), pCL->cl_d_t[1]);
            clSetKernelArg(clTemporalMed1, 6, sizeof(cl_mem), pCL->cl_d_out);
            clSetKernelArg(clTemporalMed1, 7, sizeof(stride), &stride);
            clSetKernelArg(clTemporalMed1, 8, sizeof(cl_mem), &pCL->cl_debug);

            size_t global_size[] = {(size_t)width, (size_t)height};
            clEnqueueNDRangeKernel(pCL->queue, clTemporalMed1, 2, 0, global_size, 0, 0, 0, 0);

            clReleaseKernel(clTemporalMed1);
        }

        pPost->iPrevFromGPU = 1;

        //current buffer assignment after TemporalMedian, before buffer re-assignment
        pCL->cl_y_in = pCL->cl_y_t[pPar->TEMP0_MODE];
        pCL->cl_y_prev = pCL->cl_y_t[pPar->TEMP0_MODE+1];

        //temporal buffer re-assignment
        cl_mem *pTmp;
        pTmp			= pCL->cl_y_t[3];
        pCL->cl_y_t[3]	= pCL->cl_y_t[2];
        pCL->cl_y_t[2]	= pCL->cl_y_t[1];
        pCL->cl_y_t[1]	= pCL->cl_y_t[0];
        pCL->cl_y_t[0]	= pTmp;

        pTmp			= pCL->cl_d_t[3];
        pCL->cl_d_t[3]	= pCL->cl_d_t[2];
        pCL->cl_d_t[2]	= pCL->cl_d_t[1];
        pCL->cl_d_t[1]	= pCL->cl_d_t[0];
        pCL->cl_d_t[0]	= pCL->cl_d_in;
        pCL->cl_d_in	= pTmp;

        swap(pCL->cl_d_in, pCL->cl_d_out);
    }
    else //CPU
    {
        if(pPar->TEMP0_MODE == 0)
        {
            img_in = img_t[0];
            img_prev = img_t[1];
            swap(img_t[0], img_t[1]);
            return;
        }

        if(pPost->iPrevFromGPU)
        {
            clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, din, 0, 0, 0);
        }

        //SSE
        char thd = pPar->TEMP0_THD;

#ifdef _SSE2

        if(pPar->TEMP0_MODE == 2)
        {
            for(int y=0; y<height; y++)
            {
                for(int x=0; x<width; x+=16)
                {
                    __m128i D0 = _mm_load_si128((__m128i*)(din + y*SPPW16 + x));
                    __m128i D1 = _mm_load_si128((__m128i*)(d_t[0] + y*SPPW16 + x));
                    __m128i D2 = _mm_load_si128((__m128i*)(d_t[1] + y*SPPW16 + x));
                    __m128i D3 = _mm_load_si128((__m128i*)(d_t[2] + y*SPPW16 + x));
                    __m128i D4 = _mm_load_si128((__m128i*)(d_t[3] + y*SPPW16 + x));

                    __m128i v0, v1, v3, v4;
                    __m128i m;

                    v0 = _mm_min_epu8(D0, D1);
                    v1 = _mm_max_epu8(D0, D1);
                    v3 = _mm_min_epu8(D3, D4);
                    v4 = _mm_max_epu8(D3, D4);

                    m = _mm_max_epu8( _mm_min_epu8( _mm_max_epu8( v0, D2), v4), _mm_min_epu8( v0, D2));

                    __m128i med = _mm_max_epu8( _mm_min_epu8( _mm_max_epu8( v3, m), v1), _mm_min_epu8( v3, m));

                    __m128i P0 = _mm_load_si128((__m128i*)(img_t[2] + y*SPPW16 + x));
                    __m128i P1 = _mm_load_si128((__m128i*)(img_t[3] + y*SPPW16 + x));
                    __m128i diff = _mm_min_epu8(_mm_set1_epi8(127), _mm_or_si128(_mm_subs_epu8(P1, P0),_mm_subs_epu8(P0, P1))); //prevent diff > 127 that may cause error in next instruct

                    __m128i cmp = _mm_cmpgt_epi8(diff,_mm_set1_epi8(thd));

#if 1				//this is SSE2 instruct
                    __m128i out = _mm_or_si128(_mm_and_si128(cmp, D2), _mm_andnot_si128(cmp, med));
#else
                    //this is SSE4 instruct
                    __m128i out = _mm_blendv_epi8(med, D2, cmp);
#endif

                    _mm_store_si128((__m128i*)(dout + y*SPPW16 + x), out);

                }
            }
        }
        else //pPar->TEMP0_MODE == 1
        {
            for(int y=0; y<height; y++)
            {
                for(int x=0; x<width; x+=16)
                {
                    __m128i D0 = _mm_load_si128((__m128i*)(din + y*SPPW16 + x));
                    __m128i D1 = _mm_load_si128((__m128i*)(d_t[0] + y*SPPW16 + x));
                    __m128i D2 = _mm_load_si128((__m128i*)(d_t[1] + y*SPPW16 + x));

                    __m128i med = _mm_max_epu8( _mm_min_epu8( _mm_max_epu8( D0, D1), D2), _mm_min_epu8( D0, D1));

                    __m128i P0 = _mm_load_si128((__m128i*)(img_t[1] + y*SPPW16 + x));
                    __m128i P1 = _mm_load_si128((__m128i*)(img_t[2] + y*SPPW16 + x));
                    __m128i diff = _mm_min_epu8(_mm_set1_epi8(127), _mm_or_si128(_mm_subs_epu8(P1, P0),_mm_subs_epu8(P0, P1))); //prevent diff > 127 that may cause error in next instruct

                    __m128i cmp = _mm_cmpgt_epi8(diff,_mm_set1_epi8(thd));

#if 1			//this is SSE2 instruct
                    __m128i out = _mm_or_si128(_mm_and_si128(cmp, D1), _mm_andnot_si128(cmp, med));

#else
                    //this is SSE4 instruct
                    __m128i out = _mm_blendv_epi8(med, D1, cmp);
#endif

                    _mm_store_si128((__m128i*)(dout + y*SPPW16 + x), out);

                }
            }
        }

#else

        int Pc, Pt;

        for(int y=0; y<height; y++)
        {
            for(int x=0; x<width; x++)
            {

                Pc = img_t[2][y*SPPW16 + x];
                Pt = img_t[3][y*SPPW16 + x];

                int out;

                int D[5];
                D[0] = din[y*SPPW16 + x];
                D[1] = d_t[0][y*SPPW16 + x];
                D[2] = d_t[1][y*SPPW16 + x];
                D[3] = d_t[2][y*SPPW16 + x];
                D[4] = d_t[3][y*SPPW16 + x];
                out = M5p(D);

                if(abs(Pc - Pt) > 4)
                    dout[(y*SPPW16 + x)] = D[2];
                else
                    dout[(y*SPPW16 + x)] = out;
            }
        }

    #endif

        //current buffer assignment after TemporalMedian, before buffer re-assignment
        img_in = img_t[pPar->TEMP0_MODE];
        img_prev = img_t[pPar->TEMP0_MODE+1];

        //temporal buffer re-assignment
        unsigned char *pTmp;
        pTmp		= img_t[3];
        img_t[3]	= img_t[2];
        img_t[2]	= img_t[1];
        img_t[1]	= img_t[0];
        img_t[0]	= pTmp;

        pTmp		= d_t[3];
        d_t[3]		= d_t[2];
        d_t[2]		= d_t[1];
        d_t[1]		= d_t[0];
        d_t[0]		= din;

        // check which one should be replaced, the other will not be changed
        // and assign pTmpImg to be pTmp
        if(din == pPost->pTmpImg)
            din = pPost->pTmpImg = pTmp;
        else
            din = pPost->pDImg = pTmp;


        pPost->iPrevFromGPU = 0;

        swap(din, dout);
    }
}

void Convert_Color_Y(PPOST_STRUCT pPost, unsigned char *pIn, unsigned char *pLImg, int width, int height, int stride_out)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    int Fmt = pPost->iInputFmt;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {

        pCL->cl_y_in = pCL->cl_y_t[0];
        pCL->cl_y_prev = pCL->cl_y_t[1];

        if(Fmt == 0) //YUY2
        {
            int stride_in = pPost->iInputStride ? pPost->iInputStride : (width<<1);

            clEnqueueWriteBuffer(pCL->queue, pCL->cl_in, CL_TRUE, 0, stride_in*height, pIn, 0, 0, 0);

            cl_kernel clYUY2_Y = clCreateKernel(pCL->program, "YUY2_Y", 0);

            clSetKernelArg(clYUY2_Y, 0, sizeof(cl_mem), &pCL->cl_struct);
            clSetKernelArg(clYUY2_Y, 1, sizeof(cl_mem), &pCL->cl_in);
            clSetKernelArg(clYUY2_Y, 2, sizeof(cl_mem), pCL->cl_y_in);
            clSetKernelArg(clYUY2_Y, 3, sizeof(stride_in), &stride_in);
            clSetKernelArg(clYUY2_Y, 4, sizeof(stride_out), &stride_out);
            clSetKernelArg(clYUY2_Y, 5, sizeof(cl_mem), &pCL->cl_debug);

            size_t global_size[] = {(size_t)width, (size_t)height};
            clEnqueueNDRangeKernel(pCL->queue, clYUY2_Y, 2, 0, global_size, 0, 0, 0, 0);

            clEnqueueReadBuffer(pCL->queue, *pCL->cl_y_in, CL_TRUE, 0, sizeof(cl_uchar)*stride_out*height, pLImg, 0, 0, 0);

            clReleaseKernel(clYUY2_Y);

        }
        else if(Fmt == 1) // RGB
        {
            int stride_in = pPost->iInputStride;

            clEnqueueWriteBuffer(pCL->queue, pCL->cl_in, CL_TRUE, 0, stride_in*height, pIn, 0, 0, 0);

            cl_kernel clRGB_Y;

            if(pPost->iInputFlip)
                clRGB_Y= clCreateKernel(pCL->program, "RGB_Y_Flip", 0);
            else
                clRGB_Y= clCreateKernel(pCL->program, "RGB_Y", 0);

            clSetKernelArg(clRGB_Y, 0, sizeof(cl_mem), &pCL->cl_struct);
            clSetKernelArg(clRGB_Y, 1, sizeof(cl_mem), &pCL->cl_in);
            clSetKernelArg(clRGB_Y, 2, sizeof(cl_mem), pCL->cl_y_in);
            clSetKernelArg(clRGB_Y, 3, sizeof(stride_in), &stride_in);
            clSetKernelArg(clRGB_Y, 4, sizeof(stride_out), &stride_out);
            clSetKernelArg(clRGB_Y, 5, sizeof(cl_mem), &pCL->cl_debug);

            size_t global_size[] = {(size_t)width, (size_t)height};
            clEnqueueNDRangeKernel(pCL->queue, clRGB_Y, 2, 0, global_size, 0, 0, 0, 0);

            clEnqueueReadBuffer(pCL->queue, *pCL->cl_y_in, CL_TRUE, 0, sizeof(cl_uchar)*stride_out*height, pLImg, 0, 0, 0);

            clReleaseKernel(clRGB_Y);
        }

    }
    else //CPU
    {
        if(Fmt == 0) //YUY2
        {
            int Stride = pPost->iInputStride ? pPost->iInputStride : (width<<1);

#ifdef _SSE2
            for(int y=0; y<height; y++)
            {
                __m128i mask = _mm_set1_epi16(0xff);
                for(int x= 0; x<width; x+=16)
                {
                    __m128i in1 = _mm_and_si128(_mm_loadu_si128((__m128i*)(pIn + y*Stride + (x<<1))), mask);
                    __m128i in2 = _mm_and_si128(_mm_loadu_si128((__m128i*)(pIn + y*Stride + (x<<1) + 8)), mask);
                    __m128i out = _mm_packus_epi16 (in1, in2);

                    _mm_store_si128((__m128i*)(pLImg + y*SPPW16 + x), out);
                }
            }
#else
            for(int y=0; y<height; y++)
            {
                for(int x= 0; x<width; x++)
                {
                    pLImg[y*SPPW16 + x] = pIn[y*Stride + (x<<1)];
                }
            }
#endif

        }
        else if(Fmt == 1) // RGB
        {
            int Flip = pPost->iInputFlip;
            int Stride = Flip ? -pPost->iInputStride : pPost->iInputStride;
            int ypos = Flip* (height - 1);

            for(int y=0; y<height; y++)
            {
                for(int x= 0; x<width; x++)
                {
                    pLImg[y*SPPW16 + x] = (pIn[ypos + 3*x] + (pIn[ypos + 3*x + 1]<<1) + pIn[ypos + 3*x + 2])>>2;
                }
                ypos += Stride;
            }
        }
    }
}

void Convert_D_D(unsigned char *pDepthBuf, unsigned char *pDImg, int width, int height, int stride)
{

#ifdef _SSE2

    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x+=16)
        {
            _mm_store_si128((__m128i*)(pDImg + y*stride + x), _mm_loadu_si128((__m128i*)(pDepthBuf + y*width + x)));
        }
    }

#else

    for(int y=0; y<height; y++)
    {
        memcpy(pDImg + y*stride, pDepthBuf + y*width, width);
    }

#endif
}

void DepthOutput(PPOST_STRUCT pPost, unsigned char *pDImg, unsigned char *pOut, int width, int height, int stride)
{
    //PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPost->iPrevFromGPU) //GPU
    {
        clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pDImg, 0, 0, 0);
    }

#ifdef _SSE2

    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x+=16)
        {
            _mm_storeu_si128((__m128i*)(pOut + y*width + x), _mm_load_si128((__m128i*)(pDImg + y*stride + x)));
        }
    }

#else

    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x++)
        {
            pOut[y*width + x] = pDImg[ y*stride + x];
        }
    }

#endif

}

//for test
void DepthOutput(unsigned short *pDImg, unsigned char *pOut, int width, int height, int stride)
{
    for(int y=0; y<height; y++)
    {
        for(int x= 0; x<width; x++)
        {
            pOut[y*width + x] = (unsigned char) pDImg[ y*stride + x];
        }
    }
}

void MedianFilter(PPOST_STRUCT pPost, unsigned char * &pIn, unsigned char * &pOut, int width, int height, int stride)
{
    PPOST_PAR pPar = &(pPost->Par);
    PPOST_CL pCL = pPost->pCL;

    if(pPar->MF_MODE == 0)
        return;

    if(pPost->iOpenCL & pPar->GPU_EN) //GPU
    {
        if(!pPost->iPrevFromGPU) //last stage from CPU
        {
            clEnqueueWriteBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pIn, 0, 0, 0);
        }
    //	else
    //	{
    //		clFinish(pCL->queue);
    //	}

        cl_kernel clMedian = clCreateKernel(pCL->program, "Median", 0);

        clSetKernelArg(clMedian, 0, sizeof(cl_mem), &pCL->cl_struct);
        clSetKernelArg(clMedian, 1, sizeof(cl_mem), pCL->cl_d_in);
        clSetKernelArg(clMedian, 2, sizeof(cl_mem), pCL->cl_d_out);
        clSetKernelArg(clMedian, 3, sizeof(stride), &stride);
        clSetKernelArg(clMedian, 4, sizeof(cl_mem), &pCL->cl_debug);

        size_t global_size[] = {(size_t)width, (size_t)height};
        clEnqueueNDRangeKernel(pCL->queue, clMedian, 2, 0, global_size, 0, 0, 0, 0);

    //	clEnqueueReadBuffer(pCL->queue, pCL->cl_debug, CL_TRUE, 0, sizeof(cl_int)*100, debug, 0, 0, 0);

        clReleaseKernel(clMedian);

        pPost->iPrevFromGPU = 1;

        swap(pCL->cl_d_in, pCL->cl_d_out);
    }
    else //CPU
    {
        if(pPost->iPrevFromGPU)
        {
            clEnqueueReadBuffer(pCL->queue, *pCL->cl_d_in, CL_TRUE, 0, stride*height, pIn, 0, 0, 0);
        }

#ifdef _SSE2

        for(int y=0; y<height; y++)
        {
            for(int x=0; x<width; x+=16)
            {

                int x0, x1, y0, y1;
                x0 = x-1;
                x1 = x+1;
                y0 = y-1;
                y1 = y+1;

                __m128i v_max, v_min;

                __m128i P0 = _mm_loadu_si128((__m128i*)(pIn + y0*SPPW16 + x0));
                __m128i P1 = _mm_load_si128((__m128i*)(pIn + y0*SPPW16 + x));
                __m128i P2 = _mm_loadu_si128((__m128i*)(pIn + y0*SPPW16 + x1));

                v_max = _mm_max_epu8(P0, P1);
                v_min = _mm_min_epu8(P0, P1);

                __m128i m1 = _mm_max_epu8( _mm_min_epu8(v_max, P2), v_min);
                __m128i m0 = _mm_min_epu8( v_min, P2);
                __m128i m2 = _mm_max_epu8( v_max, P2);

                __m128i P3 = _mm_loadu_si128((__m128i*)(pIn + y*SPPW16 + x0));
                __m128i P4 = _mm_load_si128((__m128i*)(pIn + y*SPPW16 + x));
                __m128i P5 = _mm_loadu_si128((__m128i*)(pIn + y*SPPW16 + x1));

                v_max = _mm_max_epu8(P3, P4);
                v_min = _mm_min_epu8(P3, P4);

                __m128i m4 = _mm_max_epu8( _mm_min_epu8(v_max, P5), v_min);
                __m128i m3 = _mm_min_epu8( v_min, P5);
                __m128i m5 = _mm_max_epu8( v_max, P5);

                __m128i P6 = _mm_loadu_si128((__m128i*)(pIn + y1*SPPW16 + x0));
                __m128i P7 = _mm_load_si128((__m128i*)(pIn + y1*SPPW16 + x));
                __m128i P8 = _mm_loadu_si128((__m128i*)(pIn + y1*SPPW16 + x1));

                v_max = _mm_max_epu8(P6, P7);
                v_min = _mm_min_epu8(P6, P7);

                __m128i m7 = _mm_max_epu8( _mm_min_epu8(v_max, P8), v_min);
                __m128i m6 = _mm_min_epu8( v_min, P8);
                __m128i m8 = _mm_max_epu8( v_max, P8);

                __m128i v0 = _mm_max_epu8( _mm_max_epu8(m0, m3), m6);
                __m128i v1 = _mm_max_epu8( _mm_min_epu8( _mm_max_epu8( m1, m4), m7), _mm_min_epu8( m1, m4));
                __m128i v2 = _mm_min_epu8( _mm_min_epu8(m2, m5), m8);

                __m128i out = _mm_max_epu8( _mm_min_epu8( _mm_max_epu8( v0, v1), v2), _mm_min_epu8( v0, v1));

                _mm_store_si128((__m128i*)(pOut + y*SPPW16 + x), out);

            }
        }

#else

        int B[9];

        for(int y=0; y<height; y++)
        {
            for(int x=0; x<width; x++)
            {
                if(x==320 && y==240)
                    x=x;

                int x0, x1, y0, y1;
                x0 = x-1;
                x1 = x+1;
                y0 = y-1;
                y1 = y+1;

                B[0] = pIn[y0*SPPW16 + x0];
                B[1] = pIn[y0*SPPW16 + x ];
                B[2] = pIn[y0*SPPW16 + x1];
                B[3] = pIn[y *SPPW16 + x0];
                B[4] = pIn[y *SPPW16 + x ];
                B[5] = pIn[y *SPPW16 + x1];
                B[6] = pIn[y1*SPPW16 + x0];
                B[7] = pIn[y1*SPPW16 + x ];
                B[8] = pIn[y1*SPPW16 + x1];

                pOut[y*SPPW16 + x] = M9p(B);
            }
        }
#endif

        pPost->iPrevFromGPU = 0;

        swap(pIn, pOut);
    }
}
