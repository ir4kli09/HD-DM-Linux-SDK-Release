#pragma once

#include "eSPDI.h"

class CComputer {

public:
    enum TYPE{
        DEFAULT = 0,
        OPENCL,
        CUDA,
        COUNT
    };

public:
    CComputer() : CComputer(DEFAULT) {}
    CComputer(TYPE type) : m_type(type) {}
    virtual ~CComputer() = default;

    TYPE GetType(){ return m_type; }

    virtual int Init(){}
    virtual int Reset(){}

    virtual int FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height) = 0;
    virtual int FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height) = 0;

    virtual int ColorFormat_to_RGB24(unsigned char *ImgDst, unsigned char *ImgSrc, 
                                     int SrcSize, int width, int height, 
                                     EtronDIImageType::Value type) = 0;

    virtual int DepthMerge(unsigned char **pDepthBufList, float *pDepthMergeOut, 
                           unsigned char *pDepthMergeFlag,
                           int nDWidth, int nDHeight, 
                           float fFocus, float *pBaseline, 
                           float *pWRNear, float *pWRFar, 
                           float *pWRFusion, int nMergeNum) = 0;

    virtual int ImageRotate90(EtronDIImageType::Value imgType, int width, int height,
                              unsigned char *src, unsigned char *dst, int len, bool clockwise) = 0;
    virtual int ImageMirro(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst) = 0;

    virtual int Resample(const BYTE *ImgSrc, const int SrcW, const int SrcH, 
                         BYTE *ImgDst, const int DstW, const int DstH, 
                         int BytePerPixel) = 0;

    virtual int GetPointCloud(unsigned char *ImgColor, int CW, int CH,
                              unsigned char *ImgDepth, int DW, int DH,
                              PointCloudInfo *pPointCloudInfo,
                              unsigned char *pPointCloudRGB, float *pPointCloudXYZ, float Near, float Far, unsigned short pid) = 0;

    virtual int TableToData(int width, int height, int TableSize, unsigned short *Table, 
                            unsigned short *Src, unsigned short *Dst) = 0;

protected:

    void SetType(TYPE type){ m_type = type; }

private:
    TYPE m_type;
};