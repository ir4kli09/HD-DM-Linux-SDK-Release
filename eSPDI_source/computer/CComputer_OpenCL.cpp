#include "CComputer_OpenCL.h"
#include "OpenCL_Kernel.h"

CComputer_OpenCL::CComputer_OpenCL():
CComputer_Default()
{
    SetType(OPENCL);
}

CComputer_OpenCL::~CComputer_OpenCL()
{

}

int CComputer_OpenCL::Reset() 
{
    m_clYUY2_to_RGB24.reset();
    m_clPointCloud_d14.reset();
    m_clPointCloud_d11.reset();
    m_clPointCloud_d11M.reset();
    m_clPointCloud_d8.reset();
    m_clResample.reset();
    m_clRotateYUY2.reset();
    m_clRotate.reset();
    m_clMirro.reset();
    m_clFusion.reset();
    m_clTableToData.reset();
    m_cl8029FDC_D8.reset();
    m_cl8029FDC_D11.reset();
}

int CComputer_OpenCL::FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height)
{
    cl_int ciErrNum = 0;
    double dbTime = 0;

    if (nullptr == m_cl8029FDC_D8)
    {
        m_cl8029FDC_D8.reset(new objOpenCL());
        m_cl8029FDC_D8->Initial();
        m_cl8029FDC_D8->BuildProgramAndKernelBySource(Kernel_FlyingDepthCancellation_D8, "FlyingDepthCancellation");
    }
    ciErrNum = m_cl8029FDC_D8->SetKernelBuffArg(0, width * height, pdepthD8);
    ciErrNum = m_cl8029FDC_D8->SetKernelBuffArg(1, width * height, pdepthD8);
    ciErrNum = m_cl8029FDC_D8->SetKernelBuffArg(2, width * height);
    ciErrNum = m_cl8029FDC_D8->ExecuteKernel(width, height, &dbTime);
    ciErrNum = m_cl8029FDC_D8->GetKernelBuffArg(2, width * height, pdepthD8);

    return ciErrNum ? ETronDI_VERIFY_DATA_FAIL : ETronDI_OK;
}

int CComputer_OpenCL::FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height)
{
    cl_int ciErrNum = 0;
    double dbTime = 0;

    if (nullptr == m_cl8029FDC_D11)
    {
        m_cl8029FDC_D11.reset(new objOpenCL());
        m_cl8029FDC_D11->Initial();
        m_cl8029FDC_D11->BuildProgramAndKernelBySource(Kernel_FlyingDepthCancellation_D11, "FlyingDepthCancellation");
    }
    ciErrNum = m_cl8029FDC_D11->SetKernelBuffArg(0, width * height * 2, pdepthD11);
    ciErrNum = m_cl8029FDC_D11->SetKernelBuffArg(1, width * height * 2, pdepthD11);
    ciErrNum = m_cl8029FDC_D11->SetKernelBuffArg(2, width * height * 2);
    ciErrNum = m_cl8029FDC_D11->ExecuteKernel(width, height, &dbTime);
    ciErrNum = m_cl8029FDC_D11->GetKernelBuffArg(2, width * height * 2, pdepthD11);

    return ciErrNum ? ETronDI_VERIFY_DATA_FAIL : ETronDI_OK;
}

int CComputer_OpenCL::ColorFormat_to_RGB24(unsigned char *ImgDst, unsigned char *ImgSrc,
                                                   int SrcSize, int width, int height,
                                                   EtronDIImageType::Value type)
{
    if (!ImgDst || !ImgSrc)
        return ETronDI_NullPtr;
    if (!width || !height)
        return ETronDI_ErrBufLen;
    if (EtronDIImageType::COLOR_YUY2 != type)
        return ETronDI_NotSupport;

    cl_int ciErrNum = 0;
    double dbTime = 0;

    if (nullptr == m_clYUY2_to_RGB24)
    {
        m_clYUY2_to_RGB24.reset(new objOpenCL());
        m_clYUY2_to_RGB24->Initial();
        m_clYUY2_to_RGB24->BuildProgramAndKernelBySource(Kernel_YUY2_to_RGB24, "YUY2_to_RGB24");
    }
    ciErrNum = m_clYUY2_to_RGB24->SetKernelBuffArg(0, width * height * 2, ImgSrc);
    ciErrNum = m_clYUY2_to_RGB24->SetKernelBuffArg(1, width * height * 3);
    ciErrNum = m_clYUY2_to_RGB24->SetKernelBuffArg(2, sizeof(int), &width);

    ciErrNum = m_clYUY2_to_RGB24->ExecuteKernel(width / 2, height, &dbTime); // yuy2 process 2-pixel in one work-item
    ciErrNum = m_clYUY2_to_RGB24->GetKernelBuffArg(1, width * height * 3, ImgDst);

    return ciErrNum ? ETronDI_VERIFY_DATA_FAIL : ETronDI_OK;
}

int CComputer_OpenCL::DepthMerge(unsigned char **pDepthBufList, float *pDepthMergeOut,
                                         unsigned char *pDepthMergeFlag,
                                         int nDWidth, int nDHeight,
                                         float fFocus, float *pBaseline,
                                         float *pWRNear, float *pWRFar,
                                         float *pWRFusion, int nMergeNum)
{
    cl_int ciErrNum = 0;
    double dbTime = 0;

    if (nullptr == m_clFusion)
    {
        m_clFusion.reset(new objOpenCL());
        m_clFusion->Initial();
        m_clFusion->BuildProgramAndKernelBySource(Kernel_DepthMerge, "Kernel_DepthMerge");
    }
    ciErrNum = m_clFusion->SetKernelBuffArg(0, nDWidth * nDHeight * 2, pDepthBufList[0]);
    ciErrNum = m_clFusion->SetKernelBuffArg(1, nDWidth * nDHeight * 2, pDepthBufList[1]);
    ciErrNum = m_clFusion->SetKernelBuffArg(2, nDWidth * nDHeight * 2, pDepthBufList[2]);

    ciErrNum = m_clFusion->SetKernelBuffArg(3, sizeof(float) * nDWidth * nDHeight, pDepthMergeOut);
    ciErrNum = m_clFusion->SetKernelBuffArg(4, nDWidth * nDHeight, pDepthMergeFlag);
    ciErrNum = m_clFusion->SetKernelBuffArg(5, sizeof(float), &fFocus);
    ciErrNum = m_clFusion->SetKernelBuffArg(6, sizeof(float) * nMergeNum, pBaseline);
    ciErrNum = m_clFusion->SetKernelBuffArg(7, sizeof(float) * nMergeNum, pWRNear);
    ciErrNum = m_clFusion->SetKernelBuffArg(8, sizeof(float) * nMergeNum, pWRFar);
    ciErrNum = m_clFusion->SetKernelBuffArg(9, sizeof(float) * nMergeNum, pWRFusion);

    ciErrNum = m_clFusion->ExecuteKernel(nDWidth, nDHeight, &dbTime);
    ciErrNum = m_clFusion->GetKernelBuffArg(3, sizeof(float) * nDWidth * nDHeight, pDepthMergeOut);
    ciErrNum = m_clFusion->GetKernelBuffArg(4, nDWidth * nDHeight, pDepthMergeFlag);

    return ETronDI_OK;
}

int CComputer_OpenCL::ImageRotate90(EtronDIImageType::Value imgType, int width, int height,
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

    cl_int ciErrNum = 0;
    double dbTime = 0;
    if (imgType == EtronDIImageType::COLOR_YUY2)
    {
        if (m_clRotateYUY2 == nullptr)
        {
            m_clRotateYUY2.reset(new objOpenCL());
            m_clRotateYUY2->Initial();
            ciErrNum = m_clRotateYUY2->BuildProgramAndKernelBySource(rotate_YUY2, "rotate_YUY2");
        }
        ciErrNum = m_clRotateYUY2->SetKernelBuffArg(0, imgSize, src);
        ciErrNum = m_clRotateYUY2->SetKernelBuffArg(1, imgSize);
        ciErrNum = m_clRotateYUY2->ExecuteKernel(width / 2, height / 2, &dbTime);
        ciErrNum = m_clRotateYUY2->GetKernelBuffArg(1, imgSize, dst);
    }
    else if (imgType == EtronDIImageType::DEPTH_11BITS || imgType == EtronDIImageType::DEPTH_14BITS)
    {
        static const int Depth_dim = 2;
        if (m_clRotate == nullptr)
        {
            m_clRotate.reset(new objOpenCL());
            m_clRotate->Initial();
            ciErrNum = m_clRotate->BuildProgramAndKernelBySource(rotate_dim, "rotate_dim");
        }
        ciErrNum = m_clRotate->SetKernelBuffArg(0, imgSize, src);
        ciErrNum = m_clRotate->SetKernelBuffArg(1, imgSize);
        ciErrNum = m_clRotate->SetKernelBuffArg(2, sizeof(int), &Depth_dim);
        ciErrNum = m_clRotate->ExecuteKernel(width, height, &dbTime);
        ciErrNum = m_clRotate->GetKernelBuffArg(1, imgSize, dst);
    }

    return ETronDI_OK;
}

int CComputer_OpenCL::ImageMirro(EtronDIImageType::Value imgType, 
                                 int width, int height, 
                                 unsigned char *src, unsigned char *dst)
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

    cl_int ciErrNum = 0;
    double dbTime = 0;
    if (imgType == EtronDIImageType::DEPTH_11BITS || imgType == EtronDIImageType::DEPTH_14BITS)
    {
        static const int Depth_dim = 2;
        if (m_clMirro == nullptr)
        {
            m_clMirro.reset(new objOpenCL());
            m_clMirro->Initial();
            ciErrNum = m_clMirro->BuildProgramAndKernelBySource(mirro_dim, "mirro_dim");
        }
        ciErrNum = m_clMirro->SetKernelBuffArg(0, imgSize, src);
        ciErrNum = m_clMirro->SetKernelBuffArg(1, imgSize);
        ciErrNum = m_clMirro->SetKernelBuffArg(2, sizeof(int), &Depth_dim);
        ciErrNum = m_clMirro->ExecuteKernel(width, height, &dbTime);
        ciErrNum = m_clMirro->GetKernelBuffArg(1, imgSize, dst);
    }

    return ETronDI_OK;
}

int CComputer_OpenCL::Resample(const BYTE *ImgSrc, const int SrcW, const int SrcH,
                               BYTE *ImgDst, const int DstW, const int DstH,
                               int BytePerPixel)
{
    cl_int ciErrNum = 0;
    double dbTime = 0;

    if (nullptr == m_clResample)
    {
        m_clResample.reset(new objOpenCL());
        m_clResample->Initial();
        m_clResample->BuildProgramAndKernelBySource(Kernel_Resample, "Resample");
    }
    ciErrNum = m_clResample->SetKernelBuffArg(0, SrcW * SrcH * BytePerPixel, ImgSrc);
    ciErrNum = m_clResample->SetKernelBuffArg(1, sizeof(int), &SrcW);
    ciErrNum = m_clResample->SetKernelBuffArg(2, sizeof(int), &SrcH);
    ciErrNum = m_clResample->SetKernelBuffArg(3, DstW * DstH * BytePerPixel);
    ciErrNum = m_clResample->SetKernelBuffArg(4, sizeof(int), &BytePerPixel);
    ciErrNum = m_clResample->ExecuteKernel(DstW, DstH, &dbTime);
    ciErrNum = m_clResample->GetKernelBuffArg(3, DstW * DstH * BytePerPixel, ImgDst);
}

int CComputer_OpenCL::GetPointCloud(unsigned char *ImgColor, int CW, int CH,
                                    unsigned char *ImgDepth, int DW, int DH,
                                    PointCloudInfo *pPointCloudInfo,
                                    unsigned char *pPointCloudRGB, float *pPointCloudXYZ, float Near, float Far, unsigned short pid)
{
    BYTE *pDepth = ImgDepth;
    BYTE *pColor = ImgColor;

    int nDstW = DW;
    int nDstH = DH;

    EtronDIImageType::Value imageType = EtronDIImageType::DepthDataTypeToDepthImageType(pPointCloudInfo->wDepthType);

    cl_int ciErrNum = 0;
    double dbTime = 0;

    switch (imageType)
    {
    case EtronDIImageType::DEPTH_11BITS:
    { //d11
        auto Run_PointCloudD11 = [&](std::shared_ptr<objOpenCL> &clPointCloud_d11, const char *Kernel, const char *Kernel_Name) {
            if (nullptr == clPointCloud_d11)
            {
                clPointCloud_d11.reset(new objOpenCL());
                clPointCloud_d11->Initial();

                clPointCloud_d11->BuildProgramAndKernelBySource(Kernel, Kernel_Name);
            }
            ciErrNum = clPointCloud_d11->SetKernelBuffArg(0, pColor ? nDstW * nDstH * 3 : 0, pColor);
            ciErrNum = clPointCloud_d11->SetKernelBuffArg(1, nDstW * nDstH * 2, pDepth);
            ciErrNum = clPointCloud_d11->SetKernelBuffArg(2, sizeof(float), &pPointCloudInfo->centerX);
            ciErrNum = clPointCloud_d11->SetKernelBuffArg(3, sizeof(float), &pPointCloudInfo->centerY);
            ciErrNum = clPointCloud_d11->SetKernelBuffArg(4, sizeof(float), &pPointCloudInfo->focalLength);

            if (pPointCloudInfo->focalLength_K)
            {
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(5, sizeof(float), &pPointCloudInfo->baseline_K);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(6, sizeof(float), &pPointCloudInfo->diff_K);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(7, sizeof(float), &pPointCloudInfo->focalLength_K);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(8, sizeof(float), &Near);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(9, sizeof(float), &Far);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(10, pPointCloudInfo->disparity_len * sizeof(float), pPointCloudInfo->disparityToW);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(11, nDstW * nDstH * 3);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(12, nDstW * nDstH * sizeof(float) * 3);
                ciErrNum = clPointCloud_d11->ExecuteKernel(nDstW, nDstH, &dbTime);
                ciErrNum = clPointCloud_d11->GetKernelBuffArg(11, nDstW * nDstH * 3, pPointCloudRGB);
                ciErrNum = clPointCloud_d11->GetKernelBuffArg(12, nDstW * nDstH * sizeof(float) * 3, pPointCloudXYZ);
            }
            else
            {
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(5, sizeof(float), &Near);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(6, sizeof(float), &Far);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(7, pPointCloudInfo->disparity_len * sizeof(float), pPointCloudInfo->disparityToW);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(8, nDstW * nDstH * 3);
                ciErrNum = clPointCloud_d11->SetKernelBuffArg(9, nDstW * nDstH * sizeof(float) * 3);
                ciErrNum = clPointCloud_d11->ExecuteKernel(nDstW, nDstH, &dbTime);
                ciErrNum = clPointCloud_d11->GetKernelBuffArg(8, nDstW * nDstH * 3, pPointCloudRGB);
                ciErrNum = clPointCloud_d11->GetKernelBuffArg(9, nDstW * nDstH * sizeof(float) * 3, pPointCloudXYZ);
            }
        };
        if (ETronDI_PID_8054 == pid && pPointCloudInfo->focalLength_K)
        {
            Run_PointCloudD11(m_clPointCloud_d11M, Kernel_PointCloud_MultipleD11, "PointCloud_MultipleD11");
        }
        else if (ETronDI_PID_8040S == pid)
        {
            if (pPointCloudInfo->focalLength_K)
                Run_PointCloudD11(m_clPointCloud_d11M, Kernel_PointCloud_MultipleCylinderD11, "PointCloud_MultipleCylinderD11");
            else
                Run_PointCloudD11(m_clPointCloud_d11, Kernel_PointCloud_CylinderD11, "PointCloud_CylinderD11");
        }
        else
        {
            Run_PointCloudD11(m_clPointCloud_d11, Kernel_PointCloud_D11, "PointCloud_D11");
        }
    }
    break;
    case EtronDIImageType::DEPTH_14BITS:
    {
        auto Run_PointCloudD14 = [&](std::shared_ptr<objOpenCL> &clPointCloud_d14, const char *Kernel, const char *Kernel_Name) {
            if (nullptr == clPointCloud_d14)
            {
                clPointCloud_d14.reset(new objOpenCL());
                clPointCloud_d14->Initial();

                clPointCloud_d14->BuildProgramAndKernelBySource(Kernel, Kernel_Name);
            }
            ciErrNum = clPointCloud_d14->SetKernelBuffArg(
                0, pColor ? nDstW * nDstH * 3 : 0, pColor);
            ciErrNum = clPointCloud_d14->SetKernelBuffArg(1, nDstW * nDstH * 2, pDepth);
            ciErrNum = clPointCloud_d14->SetKernelBuffArg(2, sizeof(float), &pPointCloudInfo->centerX);
            ciErrNum = clPointCloud_d14->SetKernelBuffArg(3, sizeof(float), &pPointCloudInfo->centerY);
            ciErrNum = clPointCloud_d14->SetKernelBuffArg(4, sizeof(float), &pPointCloudInfo->focalLength);

            if (pPointCloudInfo->focalLength_K)
            {
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(5, sizeof(float), &pPointCloudInfo->baseline_K);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(6, sizeof(float), &pPointCloudInfo->diff_K);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(7, sizeof(float), &pPointCloudInfo->focalLength_K);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(8, sizeof(float), &Near);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(9, sizeof(float), &Far);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(10, nDstW * nDstH * 3);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(11, nDstW * nDstH * sizeof(float) * 3);
                ciErrNum = clPointCloud_d14->ExecuteKernel(nDstW, nDstH, &dbTime);
                ciErrNum = clPointCloud_d14->GetKernelBuffArg(10, nDstW * nDstH * 3, pPointCloudRGB);
                ciErrNum = clPointCloud_d14->GetKernelBuffArg(11, nDstW * nDstH * sizeof(float) * 3, pPointCloudXYZ);
            }
            else
            {
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(5, sizeof(float), &Near);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(6, sizeof(float), &Far);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(7, nDstW * nDstH * 3);
                ciErrNum = clPointCloud_d14->SetKernelBuffArg(8, nDstW * nDstH * sizeof(float) * 3);
                ciErrNum = clPointCloud_d14->ExecuteKernel(nDstW, nDstH, &dbTime);
                ciErrNum = clPointCloud_d14->GetKernelBuffArg(7, nDstW * nDstH * 3, pPointCloudRGB);
                ciErrNum = clPointCloud_d14->GetKernelBuffArg(8, nDstW * nDstH * sizeof(float) * 3, pPointCloudXYZ);
            }
        };
        if (ETronDI_PID_8054 == pid && pPointCloudInfo->focalLength_K)
        {
            Run_PointCloudD14(m_clPointCloud_d14M, Kernel_PointCloud_MultipleD14, "PointCloud_MultipleD14");
        }
        else if (ETronDI_PID_8040S == pid)
        {
            if (pPointCloudInfo->focalLength_K)
                Run_PointCloudD14(m_clPointCloud_d14M, Kernel_PointCloud_MultipleCylinderD14, "PointCloud_MultipleCylinderD14");
            else
                Run_PointCloudD14(m_clPointCloud_d14, Kernel_PointCloud_CylinderD14, "PointCloud_CylinderD14");
        }
        else
            Run_PointCloudD14(m_clPointCloud_d14, Kernel_PointCloud_D14, "PointCloud_D14");
    }
    break;
    default:
    { //d8
        if (nullptr == m_clPointCloud_d8)
        {
            m_clPointCloud_d8.reset(new objOpenCL());
            m_clPointCloud_d8->Initial();
            m_clPointCloud_d8->BuildProgramAndKernelBySource(Kernel_PointCloud_D8, "PointCloud_D8");
        }
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(
            0, pColor ? nDstW * nDstH * 3 : 0, pColor);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(1, nDstW * nDstH, pDepth);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(2, sizeof(float), &pPointCloudInfo->centerX);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(3, sizeof(float), &pPointCloudInfo->centerY);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(4, sizeof(float), &pPointCloudInfo->focalLength);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(5, sizeof(float), &Near);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(6, sizeof(float), &Far);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(7, pPointCloudInfo->disparity_len * sizeof(float), pPointCloudInfo->disparityToW);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(8, nDstW * nDstH * 3);
        ciErrNum = m_clPointCloud_d8->SetKernelBuffArg(9, nDstW * nDstH * sizeof(float) * 3);
        ciErrNum = m_clPointCloud_d8->ExecuteKernel(nDstW, nDstH, &dbTime);
        ciErrNum = m_clPointCloud_d8->GetKernelBuffArg(8, nDstW * nDstH * 3, pPointCloudRGB);
        ciErrNum = m_clPointCloud_d8->GetKernelBuffArg(9, nDstW * nDstH * sizeof(float) * 3, pPointCloudXYZ);
    }
    break;
    }
    return ciErrNum ? ETronDI_VERIFY_DATA_FAIL : ETronDI_OK;
}

int CComputer_OpenCL::TableToData(int width, int height, int TableSize, unsigned short *Table,
                                  unsigned short *Src, unsigned short *Dst)
{
    cl_int ciErrNum = 0;
    double dbTime = 0;

    if (nullptr == m_clTableToData)
    {
        m_clTableToData.reset(new objOpenCL());
        m_clTableToData->Initial();
        m_clTableToData->BuildProgramAndKernelBySource(Kernel_TableToData, "Kernel_TableToData");
    }
    const int DepthSize = width * height * sizeof(unsigned short);

    ciErrNum = m_clTableToData->SetKernelBuffArg(0, TableSize, Table);
    ciErrNum = m_clTableToData->SetKernelBuffArg(1, DepthSize, Src);
    ciErrNum = m_clTableToData->SetKernelBuffArg(2, DepthSize, Dst);

    ciErrNum = m_clTableToData->ExecuteKernel(width, height, &dbTime);
    ciErrNum = m_clTableToData->GetKernelBuffArg(2, DepthSize, Dst);

    return ETronDI_OK;
}
