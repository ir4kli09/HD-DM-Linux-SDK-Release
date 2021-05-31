#include "CComputer_Cuda.h"
#include <cuda_runtime.h>
#include <stdio.h>
#include "debug.h"
#include "Cuda_Kernel.h"
#include "objCuda.h"

inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true)
{
   if (code != cudaSuccess) 
   {
      LOGE("GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }


bool CComputer_Cuda::CudaSupport()
{
    return GetCudaDeviceIndex() != -1;
}

int CComputer_Cuda::GetCudaDeviceIndex()
{    
    int count;
    cudaError_t error_id = cudaGetDeviceCount(&count);
    if (cudaSuccess != error_id)
    {
        LOGE("cudaGetDeviceCount returned %s\n",
           cudaGetErrorString(error_id));
        return -1;
    }

    if (0 == count)
    {
        LOGI("There are no available device(s) that support CUDA\n");
        return -1;
    }

    for (int i = 0; i < count; i++)
    {
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, i) == cudaSuccess)
        {
            LOGD("CUDA Capability Major/Minor version number:    %d.%d\n",
           prop.major, prop.minor);
            return i;
        }
    }
    
    LOGI("There are no available device(s) that support CUDA\n");
    return -1;      
}


CComputer_Cuda::CComputer_Cuda():
CComputer_Default()
{
    SetType(CUDA);
    int index = GetCudaDeviceIndex();
    if (-1 != index) 
    {
        cudaSetDevice(index);
    }
}

CComputer_Cuda::~CComputer_Cuda()
{
    cudaDeviceReset();
}

int CComputer_Cuda::FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height)
{
    objCuda cudaObj;
    unsigned char *cuda_LImg = cudaObj.AllocateDeviceMemory(width * height, pdepthD8);
    unsigned char *cuda_DImg = cudaObj.AllocateDeviceMemory(width * height, pdepthD8);
    unsigned char *cuda_out = (unsigned char *)cudaObj.AllocateDeviceMemory(width * height);

    dim3 size(width, height);
    cudaObj.Lanuch(size, Kernel_FlyingDepthCancellation_D8, cuda_LImg, cuda_DImg, cuda_out, width, height);

    cudaObj.CopyCudaDeviceMemoryToHost(pdepthD8, cuda_out, width * height);

    return ETronDI_OK;
}

int CComputer_Cuda::FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height)
{    
    objCuda cudaObj;
    unsigned char *cuda_LImg = cudaObj.AllocateDeviceMemory(width * height * 2, pdepthD11);
    unsigned short *cuda_DImg = (unsigned short *)cudaObj.AllocateDeviceMemory(width * height * 2, pdepthD11);
    unsigned short *cuda_out = (unsigned short *)cudaObj.AllocateDeviceMemory(width * height * 2);

    dim3 size(width, height);
    cudaObj.Lanuch(size, Kernel_FlyingDepthCancellation_D11, cuda_LImg, cuda_DImg, cuda_out, width, height);

    cudaObj.CopyCudaDeviceMemoryToHost(pdepthD11, cuda_out, width * height * 2);

    return ETronDI_OK;
}

int CComputer_Cuda::ColorFormat_to_RGB24(unsigned char *ImgDst, unsigned char *ImgSrc,
                                         int SrcSize, int width, int height,
                                         EtronDIImageType::Value type)
{
    if (!ImgDst || !ImgSrc)
        return ETronDI_NullPtr;
    if (!width || !height)
        return ETronDI_ErrBufLen;
    if (EtronDIImageType::COLOR_YUY2 != type)
        return ETronDI_NotSupport;
    
    objCuda cudaObj;
    unsigned char *cuda_src = cudaObj.AllocateDeviceMemory(width * height * 2, ImgSrc);
    unsigned char *cuda_dst = (unsigned char *)cudaObj.AllocateDeviceMemory(width * height * 3);    

    dim3 size(width / 2, height);
    cudaObj.Lanuch(size, Kernel_YUY2_to_RGB24, cuda_src, cuda_dst, width / 2, height);

    cudaObj.CopyCudaDeviceMemoryToHost(ImgDst, cuda_dst, width * height * 3);

    return ETronDI_OK;
}

int CComputer_Cuda::DepthMerge(unsigned char **pDepthBufList, float *pDepthMergeOut,
                               unsigned char *pDepthMergeFlag,
                               int nDWidth, int nDHeight,
                               float fFocus, float *pBaseline,
                               float *pWRNear, float *pWRFar,
                               float *pWRFusion, int nMergeNum)
{
    objCuda cudaObj;
    
    unsigned char *cuda_depth1 = cudaObj.AllocateDeviceMemory(nDWidth * nDHeight * 2, pDepthBufList[0]);
    unsigned char *cuda_depth2 = cudaObj.AllocateDeviceMemory(nDWidth * nDHeight * 2, pDepthBufList[1]);    
    unsigned char *cuda_depth3 = cudaObj.AllocateDeviceMemory(nDWidth * nDHeight * 2, pDepthBufList[2]);
    float *cuda_depth_merge = cudaObj.AllocateDeviceMemory(sizeof(float) * nDWidth * nDHeight, pDepthMergeOut);
    unsigned char *cuda_flag = cudaObj.AllocateDeviceMemory(nDWidth * nDHeight, pDepthMergeFlag);
    float *cuda_baseline = cudaObj.AllocateDeviceMemory(sizeof(float) * nMergeNum, pBaseline);
    float *cuda_near = cudaObj.AllocateDeviceMemory(sizeof(float) * nMergeNum, pWRNear);
    float *cuda_far = cudaObj.AllocateDeviceMemory(sizeof(float) * nMergeNum, pWRFar);
    float *cuda_fusion = cudaObj.AllocateDeviceMemory(sizeof(float) * nMergeNum, pWRFusion);

    dim3 size(nDWidth, nDHeight);
    cudaObj.Lanuch(size, Kernel_DepthMerge, cuda_depth1, cuda_depth2, cuda_depth3, cuda_depth_merge, cuda_flag, fFocus, cuda_baseline, cuda_near, cuda_far, cuda_fusion,nDWidth, nDHeight);

    cudaObj.CopyCudaDeviceMemoryToHost(pDepthMergeOut, cuda_depth_merge, sizeof(float) * nDWidth * nDHeight);
    cudaObj.CopyCudaDeviceMemoryToHost(pDepthMergeFlag, cuda_flag, nDWidth * nDHeight);

    return ETronDI_OK;
}

int CComputer_Cuda::ImageRotate90(EtronDIImageType::Value imgType, int width, int height,
                                  unsigned char *src, unsigned char *dst, int len, bool clockwise)
{
    auto getBytePerPixel = [=](EtronDIImageType::Value imgType) -> int {
        switch (imgType)
        {
        case EtronDIImageType::DEPTH_8BITS:
            return 1;
        case EtronDIImageType::COLOR_YUY2:
        case EtronDIImageType::DEPTH_8BITS_0x80:
        case EtronDIImageType::DEPTH_11BITS:
        case EtronDIImageType::DEPTH_14BITS:
            return 2;
        case EtronDIImageType::COLOR_RGB24:
            return 3;
        default:
            break;
        }
        return -1;
    };

    unsigned int imgSize = width * height * getBytePerPixel(imgType);
    if (imgSize <= 0 || (int)imgSize > len)
    {
        printf("%s, Image size incorrect = %d, but len = %d\n", __func__, imgSize, len);
        return ETronDI_ErrBufLen;
    }

    if (imgType == EtronDIImageType::COLOR_YUY2)
    {
        objCuda cudaObj;
        unsigned char *cuda_src = cudaObj.AllocateDeviceMemory(imgSize, src);
        unsigned char *cuda_dst = (unsigned char *)cudaObj.AllocateDeviceMemory(imgSize);

        dim3 size(width / 2, height / 2);
        cudaObj.Lanuch(size, rotate_YUY2, cuda_src, cuda_dst, width / 2, height / 2);

        cudaObj.CopyCudaDeviceMemoryToHost(dst, cuda_dst, imgSize);
    }
    else if (imgType == EtronDIImageType::DEPTH_11BITS || imgType == EtronDIImageType::DEPTH_14BITS)
    {
        static const int Depth_dim = 2;

        objCuda cudaObj;
        unsigned char *cuda_src = cudaObj.AllocateDeviceMemory(imgSize, src);
        unsigned char *cuda_dst = (unsigned char *)cudaObj.AllocateDeviceMemory(imgSize);

        dim3 size(width, height);
        cudaObj.Lanuch(size, rotate_dim, cuda_src, cuda_dst, Depth_dim, width, height);

        cudaObj.CopyCudaDeviceMemoryToHost(dst, cuda_dst, imgSize);
    }

    return ETronDI_OK;
}

int CComputer_Cuda::ImageMirro(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst)
{
    auto getBytePerPixel = [=](EtronDIImageType::Value imgType) -> int {
        switch (imgType)
        {
        case EtronDIImageType::DEPTH_8BITS:
            return 1;
        case EtronDIImageType::COLOR_YUY2:
        case EtronDIImageType::DEPTH_8BITS_0x80:
        case EtronDIImageType::DEPTH_11BITS:
        case EtronDIImageType::DEPTH_14BITS:
            return 2;
        case EtronDIImageType::COLOR_RGB24:
            return 3;
        default:
            break;
        }
        printf("%s, imgType = %d\n", __func__, imgType);
        return -1;
    };

    unsigned int imgSize = width * height * getBytePerPixel(imgType);
    if (imgSize <= 0)
    {
        printf("%s, Image size incorrect = %d\n", __func__, imgSize);
        return ETronDI_ErrBufLen;
    }

    if (imgType == EtronDIImageType::DEPTH_11BITS || imgType == EtronDIImageType::DEPTH_14BITS)
    {
        static const int Depth_dim = 2;

        objCuda cudaObj;
        unsigned char *cuda_src = cudaObj.AllocateDeviceMemory(imgSize, src);
        unsigned char *cuda_dst = (unsigned char *)cudaObj.AllocateDeviceMemory(imgSize);

        dim3 size(width, height);
        cudaObj.Lanuch(size, mirro_dim, cuda_src, cuda_dst, Depth_dim, width, height);

        cudaObj.CopyCudaDeviceMemoryToHost(dst, cuda_dst, imgSize);
    }

    return ETronDI_OK;
}

int CComputer_Cuda::Resample(const BYTE *ImgSrc, const int SrcW, const int SrcH,
                             BYTE *ImgDst, const int DstW, const int DstH,
                             int BytePerPixel)
{
    objCuda cudaObj;
    unsigned char *cuda_src = (unsigned char *)cudaObj.AllocateDeviceMemory(SrcW * SrcH * BytePerPixel, ImgSrc);
    unsigned char *cuda_dst = (unsigned char *)cudaObj.AllocateDeviceMemory(DstW * DstH * BytePerPixel);

    dim3 size(DstW, DstH);
    cudaObj.Lanuch(size, Kernel_Resample, cuda_src, SrcW, SrcH, cuda_dst, &BytePerPixel, DstW, DstH);

    cudaObj.CopyCudaDeviceMemoryToHost(ImgDst, cuda_dst, DstW * DstH * BytePerPixel);
}

int CComputer_Cuda::GetPointCloud(unsigned char *ImgColor, int CW, int CH,
                                  unsigned char *ImgDepth, int DW, int DH,
                                  PointCloudInfo *pPointCloudInfo,
                                  unsigned char *pPointCloudRGB, float *pPointCloudXYZ, float Near, float Far, unsigned short pid)
{
    BYTE *pDepth = ImgDepth;
    BYTE *pColor = ImgColor;

    int nDstW = DW;
    int nDstH = DH;

    EtronDIImageType::Value imageType = EtronDIImageType::DepthDataTypeToDepthImageType(pPointCloudInfo->wDepthType);

    switch (imageType)
    {
    case EtronDIImageType::DEPTH_11BITS:
    { //d11
        auto Run_PointCloudD11 = [&](auto Kernel) {            
            objCuda cudaObj;
            unsigned char *cuda_color = cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3, pColor);
            unsigned short *cuda_depth = (unsigned short *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 2, pDepth);
            float *cuda_disparityToW = cudaObj.AllocateDeviceMemory(pPointCloudInfo->disparity_len * sizeof(float), pPointCloudInfo->disparityToW);
            unsigned char *cuda_rgb =  (unsigned char *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3);
            float *cuda_xyz =  (float *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * sizeof(float) * 3);

            dim3 size(nDstW, nDstH);
            cudaObj.Lanuch(size, Kernel, cuda_color, cuda_depth, pPointCloudInfo->centerX, pPointCloudInfo->centerY, pPointCloudInfo->focalLength, Near, Far, cuda_disparityToW, cuda_rgb, cuda_xyz, nDstW, nDstH);

            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudRGB, cuda_rgb, nDstW * nDstH * 3);
            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudXYZ, cuda_xyz, nDstW * nDstH * sizeof(float) * 3);
        };

        auto Run_PointCloudD11WithK = [&](auto Kernel) {            
            objCuda cudaObj;
            unsigned char *cuda_color = cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3, pColor);
            unsigned short *cuda_depth = (unsigned short *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 2, pDepth);
            float *cuda_disparityToW = cudaObj.AllocateDeviceMemory(pPointCloudInfo->disparity_len * sizeof(float), pPointCloudInfo->disparityToW);
            unsigned char *cuda_rgb =  (unsigned char *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3);
            float *cuda_xyz =  (float *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * sizeof(float) * 3);
            
            dim3 size(nDstW, nDstH);
            cudaObj.Lanuch(size, Kernel, cuda_color, cuda_depth, pPointCloudInfo->centerX, pPointCloudInfo->centerY, pPointCloudInfo->focalLength, pPointCloudInfo->baseline_K, pPointCloudInfo->diff_K, pPointCloudInfo->focalLength_K, Near, Far, cuda_disparityToW, cuda_rgb, cuda_xyz, nDstW, nDstH);

            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudRGB, cuda_rgb, nDstW * nDstH * 3);
            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudXYZ, cuda_xyz, nDstW * nDstH * sizeof(float) * 3);

        };
        if (ETronDI_PID_8054 == pid && pPointCloudInfo->focalLength_K)
        {
            Run_PointCloudD11WithK(Kernel_PointCloud_MultipleD11);
        }
        else if (ETronDI_PID_8040S == pid)
        {
            if (pPointCloudInfo->focalLength_K)
                Run_PointCloudD11WithK(Kernel_PointCloud_MultipleCylinderD11);
            else
                Run_PointCloudD11(Kernel_PointCloud_CylinderD11);
        }
        else
        {
            Run_PointCloudD11(Kernel_PointCloud_D11);
        }
    }
    break;
    case EtronDIImageType::DEPTH_14BITS:
    {
        auto Run_PointCloudD14 = [&](auto Kernel) {
            
            objCuda cudaObj;
            unsigned char *cuda_color = cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3, pColor);
            unsigned short *cuda_depth = (unsigned short *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 2, pDepth);

            unsigned char *cuda_rgb =  (unsigned char *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3);
            float *cuda_xyz =  (float *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * sizeof(float) * 3);

            dim3 size(nDstW, nDstH);
            cudaObj.Lanuch(size, Kernel, cuda_color, cuda_depth, pPointCloudInfo->centerX, pPointCloudInfo->centerY, pPointCloudInfo->focalLength, Near, Far, cuda_rgb, cuda_xyz, nDstW, nDstH);

            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudRGB, cuda_rgb, nDstW * nDstH * 3);
            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudXYZ, cuda_xyz, nDstW * nDstH * sizeof(float) * 3);

        };

        auto Run_PointCloudD14WidthK = [&](auto Kernel) {
            
            objCuda cudaObj;
            unsigned char *cuda_color = cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3, pColor);
            unsigned short *cuda_depth = (unsigned short *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 2, pDepth);
            unsigned char *cuda_rgb =  (unsigned char *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3);
            float *cuda_xyz =  (float *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * sizeof(float) * 3);
            
            dim3 size(nDstW, nDstH);
            cudaObj.Lanuch(size, Kernel, cuda_color, cuda_depth, pPointCloudInfo->centerX, pPointCloudInfo->centerY, pPointCloudInfo->focalLength, pPointCloudInfo->baseline_K, pPointCloudInfo->diff_K, pPointCloudInfo->focalLength_K, Near, Far, cuda_rgb, cuda_xyz, nDstW, nDstH);

            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudRGB, cuda_rgb, nDstW * nDstH * 3);
            cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudXYZ, cuda_xyz, nDstW * nDstH * sizeof(float) * 3);
        };
        if (ETronDI_PID_8054 == pid && pPointCloudInfo->focalLength_K)
        {
            Run_PointCloudD14WidthK(Kernel_PointCloud_MultipleD14);
        }
        else if (ETronDI_PID_8040S == pid)
        {
            if (pPointCloudInfo->focalLength_K)
                Run_PointCloudD14WidthK(Kernel_PointCloud_MultipleCylinderD14);
            else
                Run_PointCloudD14(Kernel_PointCloud_CylinderD14);
        }
        else
            Run_PointCloudD14(Kernel_PointCloud_D14);
    }
    break;
    default:
    { //d8
        objCuda cudaObj;
        unsigned char *cuda_color = cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3, pColor);
        unsigned char *cuda_depth = cudaObj.AllocateDeviceMemory(nDstW * nDstH, pDepth);
        float *cuda_disparityToW = cudaObj.AllocateDeviceMemory(pPointCloudInfo->disparity_len * sizeof(float), pPointCloudInfo->disparityToW);
        unsigned char *cuda_rgb =  (unsigned char *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * 3);
        float *cuda_xyz =  (float *)cudaObj.AllocateDeviceMemory(nDstW * nDstH * sizeof(float) * 3);

        dim3 size(nDstW, nDstH);
        cudaObj.Lanuch(size, Kernel_PointCloud_D8, cuda_color, cuda_depth, pPointCloudInfo->centerX, pPointCloudInfo->centerY, pPointCloudInfo->focalLength,Near, Far, cuda_disparityToW, cuda_rgb, cuda_xyz, nDstW, nDstH);

        cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudRGB, cuda_rgb, nDstW * nDstH * 3);
        cudaObj.CopyCudaDeviceMemoryToHost(pPointCloudXYZ, cuda_xyz, nDstW * nDstH * sizeof(float) * 3);
    }
    break;
    }
    return ETronDI_OK;
}

int CComputer_Cuda::TableToData(int width, int height, int TableSize, unsigned short *Table,
                                unsigned short *Src, unsigned short *Dst)
{
    const int DepthSize = width * height * sizeof(unsigned short);
    
    objCuda cudaObj;
    unsigned short *cuda_table = cudaObj.AllocateDeviceMemory(TableSize, Table);
    unsigned short *cuda_src = cudaObj.AllocateDeviceMemory(DepthSize, Src);
    unsigned short *cuda_dst = cudaObj.AllocateDeviceMemory(DepthSize, Dst);

    dim3 size(width, height);
    cudaObj.Lanuch(size, Kernel_TableToData, cuda_table, cuda_src, cuda_dst, width, height);

    cudaObj.CopyCudaDeviceMemoryToHost(Dst, cuda_dst, DepthSize);

    return ETronDI_OK;
}
