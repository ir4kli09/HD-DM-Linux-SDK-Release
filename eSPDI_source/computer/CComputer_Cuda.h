#pragma once
#include "CComputer_Default.h"

class CComputer_Cuda : public CComputer_Default
{
public:

    static bool CudaSupport();

    CComputer_Cuda();
    virtual ~CComputer_Cuda();

    virtual int FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height);
    virtual int FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height);

    virtual int ColorFormat_to_RGB24(unsigned char *ImgDst, unsigned char *ImgSrc,
                                     int SrcSize, int width, int height,
                                     EtronDIImageType::Value type);

    virtual int DepthMerge(unsigned char **pDepthBufList, float *pDepthMergeOut,
                           unsigned char *pDepthMergeFlag,
                           int nDWidth, int nDHeight,
                           float fFocus, float *pBaseline,
                           float *pWRNear, float *pWRFar,
                           float *pWRFusion, int nMergeNum);

    virtual int ImageRotate90(EtronDIImageType::Value imgType, int width, int height,
                              unsigned char *src, unsigned char *dst, int len, bool clockwise);
    virtual int ImageMirro(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst);

    virtual int Resample(const BYTE *ImgSrc, const int SrcW, const int SrcH,
                         BYTE *ImgDst, const int DstW, const int DstH,
                         int BytePerPixel);

    virtual int GetPointCloud(unsigned char *ImgColor, int CW, int CH,
                              unsigned char *ImgDepth, int DW, int DH,
                              PointCloudInfo *pPointCloudInfo,
                              unsigned char *pPointCloudRGB, float *pPointCloudXYZ, float Near, float Far, unsigned short pid);

    virtual int TableToData(int width, int height, int TableSize, unsigned short *Table,
                            unsigned short *Src, unsigned short *Dst);

private:
    static int GetCudaDeviceIndex();
};