#include "CComputer_Default.h"
#include <string.h>
#include <cmath>
#include <stdio.h>

CComputer_Default::CComputer_Default() : CComputer()
{
}

int CComputer_Default::FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height)
{
    const int h_radius = 8;
    const int v_radius = 8;
    const int iREMOVE_D_MODE = 0;
    const int iREMOVE_D_THD = 2;
    const int iREMOVE_Y_THD = 5;
    const int iREMOVE_CURVE[17] = {150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 255, 255};
    const int iREMOVE_CNT_RATIO = 10;

    unsigned char pDImgOut[width * height] = {0};

    for (int x = 0; x < width; ++x)
    {
        for (int y = 0; y < height; ++y)
        {
            int center = x + y * width;
            int area_cnt = 0;
            int valid_cnt = 0;
            int all_cnt = 0;
            int d_8bit = pdepthD8[center];
            int idx = 0;
            int d_thd = iREMOVE_D_MODE ? (iREMOVE_D_THD * d_8bit * d_8bit) >> 14 : iREMOVE_D_THD;

            for (int j = -v_radius; j <= v_radius; ++j)
            {
                for (int i = -h_radius; i <= h_radius; ++i)
                {
                    int x1 = x + i;
                    int y1 = y + j;
                    int d_curr, y_curr;
                    if (x1 >= 0 && x1 <= width - 1 && y1 >= 0 && y1 <= height - 1)
                    {
                        d_curr = (pdepthD8[x1 + y1 * width] << 3);
                        y_curr = pdepthD8[x1 + y1 * width];
                    }
                    else
                    {
                        y_curr = d_curr = 0;
                    }
                    int cond = d_curr && (abs(d_8bit - (d_curr >> 3)) <= d_thd);
                    if (abs(pdepthD8[center] - y_curr) < iREMOVE_Y_THD && d_curr)
                    {
                        area_cnt++;
                        if (cond)
                        {
                            valid_cnt++;
                        }
                    }

                    if (cond)
                    {
                        all_cnt++;
                    }
                }
            }
            idx = d_8bit >> 4;
            int cnt_thd = iREMOVE_CURVE[idx] + ((iREMOVE_CURVE[idx + 1] - iREMOVE_CURVE[idx]) * (d_8bit & 0x0f) >> 4);

            pDImgOut[center] = (valid_cnt < ((area_cnt * iREMOVE_CNT_RATIO) >> 4) || (all_cnt < cnt_thd) ? 0 : pdepthD8[center]);
        }
    }

    memcpy(pdepthD8, pDImgOut, width * height);
    return ETronDI_OK;
}

int CComputer_Default::FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height)
{
    const int h_radius = 8;
    const int v_radius = 8;
    const int iREMOVE_D_MODE = 0;
    const int iREMOVE_D_THD = 6;
    const int iREMOVE_Y_THD = 6;
    const int iREMOVE_CURVE[17] = {200, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180};
    const int iREMOVE_CNT_RATIO = 12;

    unsigned char *pLImg = pdepthD11;
    unsigned short *pD11Img = (unsigned short *)pdepthD11;
    unsigned short pDImgOut[width * height] = {0};

    for (int x = 0; x < width; ++x)
    {
        for (int y = 0; y < height; ++y)
        {

            int center = x + y * width;
            int area_cnt = 0;
            int valid_cnt = 0;
            int all_cnt = 0;
            int d_8bit = pD11Img[center] >> 3;
            int idx = 0;
            int d_thd = iREMOVE_D_MODE ? (iREMOVE_D_THD * d_8bit * d_8bit) >> 14 : iREMOVE_D_THD;
            int x1 = 0;
            int y1 = 0;
            int cond = 0;
            int d_curr, y_curr;
            for (int j = -v_radius; j <= v_radius; ++j)
            {
                for (int i = -h_radius; i <= h_radius; ++i)
                {
                    x1 = x + i;
                    y1 = y + j;
                    if (x1 >= 0 && x1 <= width - 1 && y1 >= 0 && y1 <= height - 1)
                    {
                        d_curr = pD11Img[x1 + y1 * width];
                        y_curr = pLImg[x1 + y1 * width];
                    }
                    else
                    {
                        y_curr = d_curr = 0;
                    }
                    cond = d_curr && (abs(d_8bit - (d_curr >> 3)) <= d_thd);
                    if (abs(pLImg[center] - y_curr) < iREMOVE_Y_THD && d_curr)
                    {
                        area_cnt++;
                        if (cond)
                        {
                            valid_cnt++;
                        }
                    }

                    if (cond)
                    {
                        all_cnt++;
                    }
                }
            }
            idx = d_8bit >> 4;
            int cnt_thd = iREMOVE_CURVE[idx] + ((iREMOVE_CURVE[idx + 1] - iREMOVE_CURVE[idx]) * (d_8bit & 0x0f) >> 4);

            pDImgOut[center] = (valid_cnt < ((area_cnt * iREMOVE_CNT_RATIO) >> 4) || (all_cnt < cnt_thd) ? 0 : pD11Img[center]);
        }
    }

    memcpy(pdepthD11, pDImgOut, width * height * sizeof(unsigned short));
    return ETronDI_OK;
}

int CComputer_Default::ColorFormat_to_RGB24(unsigned char *ImgDst, unsigned char *ImgSrc,
                                            int SrcSize, int width, int height,
                                            EtronDIImageType::Value type)
{
    if (!ImgDst || !ImgSrc)
        return ETronDI_NullPtr;
    if (!width || !height)
        return ETronDI_ErrBufLen;
    if (EtronDIImageType::COLOR_YUY2 != type)
        return ETronDI_NotSupport;

    register float Y0 = 0, Y1 = 0, U0 = 0, V1 = 0;
    register float Rs = 0, Gs = 0, Bs = 0;
    //int k = 0;
    const unsigned char *p_src = ImgSrc;
    unsigned char *p_dst = ImgDst;
    // register unsigned char tmp = 0;
    // for (int i = 0; i < SrcSize; i+=2)
    // {
    //     tmp = p_src[i];
    //     p_dst[0] = tmp;
    //     p_dst[1] = tmp;
    //     p_dst[2] = tmp;
    //     p_dst+=3;
    // }

    // for (int i = 0; i < SrcSize; i+=4)
    // {
    //     Y0 = (float)(p_src[0]);
    //     U0 = (float)(p_src[1]) - 128.0f;
    //     Y1 = (float)(p_src[2]);
    //     V1 = (float)(p_src[3]) - 128.0f;
    //     p_src+=4;

    //     Rs = V1 * 1.4075f;
    //     Gs = -(U0 * 0.3455f) - (V1 * 0.7169f);
    //     Bs = U0 * 1.779f;

    //     (p_dst[0]) = Y0 + Rs;
    //     (p_dst[1]) = Y0 + Gs;
    //     (p_dst[2]) = Y0 + Bs;
    //     (p_dst[3]) = Y1 + Rs;
    //     (p_dst[4]) = Y1 + Gs;
    //     (p_dst[5]) = Y1 + Bs;
    //     p_dst+=6;
    // }

    for (int i = 0; i < SrcSize; i+=4)
    {
        Y0 = (float)(*p_src++);
        U0 = (float)(*p_src++) - 128.0f;
        Y1 = (float)(*p_src++);
        V1 = (float)(*p_src++) - 128.0f;

        Rs = V1 * 1.4075f;
        Gs = -(U0 * 0.3455f) - (V1 * 0.7169f);
        Bs = U0 * 1.779f;

        (*p_dst++) = Y0 + Rs;
        (*p_dst++) = Y0 + Gs;
        (*p_dst++) = Y0 + Bs;
        (*p_dst++) = Y1 + Rs;
        (*p_dst++) = Y1 + Gs;
        (*p_dst++) = Y1 + Bs;
    }

    return ETronDI_OK;
}

int CComputer_Default::DepthMerge(unsigned char **pDepthBufList, float *pDepthMergeOut,
                                  unsigned char *pDepthMergeFlag,
                                  int nDWidth, int nDHeight,
                                  float fFocus, float *pBaseline,
                                  float *pWRNear, float *pWRFar,
                                  float *pWRFusion, int nMergeNum)
{
    int nBestDepth = -1; // 0 1 2 3 -> D0 D1 D2 D3, -1 -> error pixel
    unsigned char *pDepthBufListTemp = nullptr;
    unsigned char *pDepth1 = pDepthBufList[0];
    unsigned char *pDepth2 = pDepthBufList[1];
    unsigned char *pDepth3 = pDepthBufList[2];

    for (int j = 0; j < nDWidth; ++j)
    {
        for (int i = 0; i < nDHeight; ++i)
        {

            float vD[3];
            float vD_scale[3];
            // Step 0 : Normalize to 8bit standard, that is 11bit -> 8bit for computation use, "Do not decrease accuracy
            vD[0] = (float)(pDepth1[i * nDWidth * 2 + j * 2 + 0] + pDepth1[i * nDWidth * 2 + j * 2 + 1] * 256) / 8.0f;
            vD[1] = (float)(pDepth2[i * nDWidth * 2 + j * 2 + 0] + pDepth2[i * nDWidth * 2 + j * 2 + 1] * 256) / 8.0f;
            vD[2] = (float)(pDepth3[i * nDWidth * 2 + j * 2 + 0] + pDepth3[i * nDWidth * 2 + j * 2 + 1] * 256) / 8.0f;

            // Step 1 : Use maximum baseline as standard, because its accuracy is best
            for (int k = 0; k < 3; k++)
                vD_scale[k] = (vD[k] * (pBaseline[2] / pBaseline[k]));

            // Step 2 : Baseline ratio constraint among depth maps
            for (int k = 1; k < 3; k++) // depth 1 compare
                if (vD_scale[0] != 0)
                    if (fabs(vD_scale[0] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[0] + 0.5))
                        nBestDepth = k;

            for (int k = 2; k < 3; k++) // depth 2 compare
                if (vD_scale[1] != 0 && nBestDepth == -1)
                    if (fabs(vD_scale[1] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[1] + 0.5))
                        nBestDepth = k;

            // Step 3 : All candidcate depth maps are not related, it can be classify to two case
            // 1. One, two or more depth map enter blind region(IC limitation, only search 256 pixel)
            // 2. Depth map error region or depth map computation error
            // -> use depth map 0 as default, because it has less depth map error region, if depth map 0 not work, use depth map 1.....
            for (int k = 0; k < 3; k++)
                if (nBestDepth == -1 && vD[k] != 0.0)
                    if (fFocus * pBaseline[k] / vD[k] > pWRNear[k] && fFocus * pBaseline[k] / vD[k] < pWRFar[k])
                        nBestDepth = k;

            pDepthMergeFlag[i * nDWidth + j] = nBestDepth;

            // Step 4 : Use Alpha-Blending to smooth Z_error curve
            float fFusionD = 0.0;
            float diff = 0.0;
            if (nBestDepth != -1)
            {
                pDepthBufListTemp = (nBestDepth == 0 ? pDepth1 : (nBestDepth == 1 ? pDepth2 : pDepth3));
                fFusionD = (float)pDepthBufListTemp[i * nDWidth * 2 + j * 2 + 0] + pDepthBufListTemp[i * nDWidth * 2 + j * 2 + 1] * 256;
            }
            if (pWRFusion[0] != 0.0 && nBestDepth > 0 && fabs(vD_scale[nBestDepth] - vD_scale[nBestDepth - 1]) < (pBaseline[nBestDepth] / pBaseline[nBestDepth - 1] + 0.5))
            {
                unsigned char *pPreDepthBufList = (nBestDepth == 1 ? pDepth1 : pDepth2);
                float fZ2 = fFocus * pBaseline[nBestDepth] / (pDepthBufListTemp[i * nDWidth * 2 + j * 2 + 0] + pDepthBufListTemp[i * nDWidth * 2 + j * 2 + 1] * 256) * 8.0;

                if (fZ2 >= pWRNear[nBestDepth] && fZ2 < (pWRNear[nBestDepth] + pWRFusion[nBestDepth]))
                {
                    float fZ1 = fFocus * pBaseline[nBestDepth - 1] / (pPreDepthBufList[i * nDWidth * 2 + j * 2 + 0] + pPreDepthBufList[i * nDWidth * 2 + j * 2 + 1] * 256) * 8.0;
                    float fAlpha = (fZ2 - pWRNear[nBestDepth]) / pWRFusion[nBestDepth];
                    float fFusionZ = fZ2 * fAlpha + fZ1 * (1.0 - fAlpha);
                    fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ * 8.0;
                }
            }

            // Step 5 : Normalize to maximum baseline standard
            if (nBestDepth != -1)
                fFusionD *= (pBaseline[2] / pBaseline[nBestDepth]);

            // Step 6 : Normalize to 0~255 for visual purpose.
            fFusionD /= (8 * pBaseline[2] / pBaseline[0]);

            // Step 7 : Assign disparity for pDepthMerge buffer
            if (nBestDepth == -1)
                pDepthMergeOut[i * nDWidth + j] = 0.0;
            else
                pDepthMergeOut[i * nDWidth + j] = fFusionD;
        }
    }

    return ETronDI_OK;
}

int CComputer_Default::ImageRotate90(EtronDIImageType::Value imgType, int width, int height,
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
        for (int x = 0; x < width; x += 2)
        {
            for (int y = 0; y < height; y += 2)
            {
                int jBase[2] = {y * width * 2, y * width * 2 + width * 2};
                int dstY[2][2] = {{0, 0}, {0, 0}};

                dstY[0][0] = (x * height + (height - 1 - y)) * 2;
                dst[dstY[0][0]] = src[jBase[0] + x * 2];
                dst[dstY[0][0] + 1] = src[jBase[0] + x * 2 + 3];

                dstY[0][1] = ((x + 1) * height + (height - 1 - y)) * 2;
                dst[dstY[0][1]] = src[jBase[0] + x * 2 + 2];
                dst[dstY[0][1] + 1] = src[jBase[0] + x * 2 + 3];

                dstY[1][0] = (x * height + (height - 1 - (y + 1))) * 2;
                dst[dstY[1][0]] = src[jBase[1] + x * 2];
                dst[dstY[1][0] + 1] = src[jBase[1] + x * 2 + 1];

                dstY[1][1] = ((x + 1) * height + (height - 1 - (y + 1))) * 2;
                dst[dstY[1][1]] = src[jBase[1] + x * 2 + 2];
                dst[dstY[1][1] + 1] = src[jBase[1] + x * 2 + 1];
            }
        }
    }
    else if (imgType == EtronDIImageType::DEPTH_11BITS || imgType == EtronDIImageType::DEPTH_14BITS)
    {
        int L_dim = 2;
        for (int x = 0; x < width; ++x)
        {
            ;
            for (int y = 0; y < height; ++y)
            {
                for (int i = 0; i < L_dim; i++)
                    dst[(x * height + y) * L_dim + i] = src[((height - y - 1) * width + x) * L_dim + i];
            }
        }
    }
    else
    {
        return ETronDI_NotSupport;
    }

    return ETronDI_OK;
}

int CComputer_Default::ImageMirro(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst)
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
    if (imgSize <= 0)
    {
        printf("%s, Image size incorrect = %d\n", __func__, imgSize);
        return ETronDI_ErrBufLen;
    }

    if (imgType == EtronDIImageType::DEPTH_11BITS || imgType == EtronDIImageType::DEPTH_14BITS)
    {
        int L_dim = 2;
        for (int x = 0; x < width; ++x)
        {
            for (int y = 0; y < height; ++y)
            {
                for (int i = 0; i < L_dim; i++)
                    dst[(y * width + x) * L_dim + i] = src[(y * width + (width - x - 1)) * L_dim + i];
            }
        }
    }
    else
    {
        return ETronDI_NotSupport;
    }

    return ETronDI_OK;
}

int CComputer_Default::Resample(const BYTE *ImgSrc, const int SrcW, const int SrcH,
                                BYTE *ImgDst, const int DstW, const int DstH,
                                int BytePerPixel)
{
    unsigned char p00, p01, p10, p11;

    for (int x = 0; x < DstW; ++x)
    {
        for (int y = 0; y < DstH; ++y)
        {
            int idx = y * DstW + x;
            float x_fac = (float)SrcW / DstW;
            float y_fac = (float)SrcH / DstH;
            float outx = x_fac * x;
            float outy = y_fac * y;
            int x0 = (int)outx;
            int y0 = (int)outy;
            int x1 = x0 < SrcW - 1 ? x0 + 1 : x0;
            int y1 = y0 < SrcH - 1 ? y0 + 1 : y0;
            float rx = outx - x0;
            float ry = outy - y0;
            int p00_index = (y0 * SrcW + x0) * BytePerPixel;
            int p01_index = (y0 * SrcW + x1) * BytePerPixel;
            int p10_index = (y1 * SrcW + x0) * BytePerPixel;
            int p11_index = (y1 * SrcW + x1) * BytePerPixel;
            for (int byte = 0; byte < BytePerPixel; ++byte)
            {
                p00 = ImgSrc[p00_index + byte];
                p01 = ImgSrc[p01_index + byte];
                p10 = ImgSrc[p10_index + byte];
                p11 = ImgSrc[p11_index + byte];
                if (((p00 == 0) | (p01 == 0) | (p10 == 0) | (p11 == 0)))
                {
                    ImgDst[idx * BytePerPixel + byte] = 0;
                }
                else
                {
                    float p0 = rx * p01 + (1 - rx) * p00;
                    float p1 = rx * p11 + (1 - rx) * p10;
                    ImgDst[idx * BytePerPixel + byte] = (unsigned short)(ry * p1 + (1 - ry) * p0);
                }
            }
        }
    }

    return ETronDI_OK;
}

#include <time.h>   // Red@20210426: datapipe,
#include <iostream> // Red@20210426: datapipe,
#include <stdlib.h> // Red@20210426: datapipe,
int CComputer_Default::GetPointCloud(unsigned char *ImgColor, int CW, int CH,
                                     unsigned char *ImgDepth, int DW, int DH,
                                     PointCloudInfo *pPointCloudInfo,
                                     unsigned char *pPointCloudRGB, float *pPointCloudXYZ, float Near, float Far, unsigned short pid)
{
    bool bDepthOnly = !ImgColor || !CW || !CH;

    const unsigned char constR = 0, constG = 255, constB = 0;

    BYTE *pDepth = ImgDepth;
    BYTE *pColor = ImgColor;
    const int nDstW = DW;
    const int nDstH = DH;

    EtronDIImageType::Value imageType = EtronDIImageType::DepthDataTypeToDepthImageType(pPointCloudInfo->wDepthType);

    enum POINT_CLOUD_TYPE
    {
        DEFAULT,
        MULTIPLE,
        CYLINDER,
        MULTIPLE_CYLINDER
    };

    unsigned short *Depth = (unsigned short *)pDepth;

#if !defined(NDEBUG)
    clock_t T[2];
    static double ec_time = 0;

    T[0] = clock();
#endif

    const float CX = pPointCloudInfo->centerX;
    const float CY = pPointCloudInfo->centerY;
    //int i = 0;
    int i3 = 0;
    const int END_H = nDstH - CY;
    const int END_W = nDstW - CX;
    const int Z_NEAR = Near;
    const int Z_FAR = Far;
    const float FOCAL = pPointCloudInfo->focalLength;
    const float FOCAL_INV = 1.0f / FOCAL;
    const unsigned short *p_depth = Depth;
    const float* p_disparityToW = pPointCloudInfo->disparityToW;

    const unsigned short MAX_D = 2048;
    float p_disparityToWInv[MAX_D];
    for (int i = 0; i < MAX_D ; ++i)
    {
        p_disparityToWInv[i] = ( 0 == p_disparityToW[i] ) ? 0.0f : 1.0f / p_disparityToW[i];
    }

    float *p_pointcloud = pPointCloudXYZ;
    //float* p_pointcloud = NULL;
    switch (imageType)
    {
    case EtronDIImageType::DEPTH_11BITS:
    { //d11
        auto Run_PointCloudD11 = [&](POINT_CLOUD_TYPE type = DEFAULT) {
            switch (type)
            {
            case DEFAULT:
            {
                if (!bDepthOnly)
                    memcpy(pPointCloudRGB, pColor, nDstH * nDstW * 3);

                for (int py = 0; py < nDstH; ++py)
                {
                    const float Y_SHIFT = (py - CY);
                    for (int px = 0; px < nDstW; ++px)
                    {
                        const float W_inv = p_disparityToWInv[*p_depth++];
                        if (W_inv != 0) {
                            const float Z = FOCAL * W_inv;
                            if (Z > Near && Z < Z_FAR)
                            {
                                p_pointcloud[0] = (px - CX) * W_inv;
                                p_pointcloud[1] = Y_SHIFT* W_inv;
                                p_pointcloud[2] = Z;
                            }
                            else
                            {
                                p_pointcloud[0] = NAN;
                                p_pointcloud[1] = NAN;
                                p_pointcloud[2] = NAN;
                            }
                        }
                        else
                        {
                            p_pointcloud[0] = NAN;
                            p_pointcloud[1] = NAN;
                            p_pointcloud[2] = NAN;
                        }

                        p_pointcloud += 3;
                    }
                }
                break;
            }
            case MULTIPLE:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float W = pPointCloudInfo->disparityToW[Depth[idx]];
                        const float Z = pPointCloudInfo->focalLength / W;
                        const int shift = ((pPointCloudInfo->focalLength_K / Z) - (pPointCloudInfo->diff_K)) * (pPointCloudInfo->baseline_K);
                        if ((W != 0.0f && Z < Far && Z > Near) &&
                            ((py - shift > 0) && (py - shift < nDstH - 1)))
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[((py - shift) * nDstW + px) * 3];
                                pPointCloudRGB[c_idx + 1] = pColor[((py - shift) * nDstW + px) * 3 + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[((py - shift) * nDstW + px) * 3 + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }

                            pPointCloudXYZ[c_idx] = (px - pPointCloudInfo->centerX) / W;
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }
                break;
            }
            case CYLINDER:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float angle = (px - pPointCloudInfo->centerX) * (3.14159265359f / nDstW);
                        const float W = pPointCloudInfo->disparityToW[Depth[idx]];
                        const float Z = cos(angle) * (pPointCloudInfo->focalLength / W);
                        if (W != 0.0f && Z < Far && Z > Near)
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[c_idx];
                                pPointCloudRGB[c_idx + 1] = pColor[c_idx + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[c_idx + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }

                            pPointCloudXYZ[c_idx] = sin(angle) * (pPointCloudInfo->focalLength / W);
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }
                break;
            }
            case MULTIPLE_CYLINDER:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float angle = (px - pPointCloudInfo->centerX) * (3.14159265359f / nDstW);
                        const float W = pPointCloudInfo->disparityToW[Depth[idx]];
                        const float Z = cos(angle) * (pPointCloudInfo->focalLength / W);
                        const int shift = ((pPointCloudInfo->focalLength_K / Z) - (pPointCloudInfo->diff_K)) * (pPointCloudInfo->baseline_K);
                        if ((W != 0.0f && Z < Far && Z > Near) &&
                            ((py - shift > 0) && (abs(py - shift) < nDstH - 1)))
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[((py - shift) * nDstW + px) * 3];
                                pPointCloudRGB[c_idx + 1] = pColor[((py - shift) * nDstW + px) * 3 + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[((py - shift) * nDstW + px) * 3 + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }

                            pPointCloudXYZ[c_idx] = sin(angle) * (pPointCloudInfo->focalLength / W);
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
        };
        if (ETronDI_PID_8054 == pid && pPointCloudInfo->focalLength_K)
        {
            Run_PointCloudD11(MULTIPLE);
        }
        else if (ETronDI_PID_8040S == pid)
        {
            if (pPointCloudInfo->focalLength_K)
                Run_PointCloudD11(MULTIPLE_CYLINDER);
            else
                Run_PointCloudD11(CYLINDER);
        }
        else
        {
            Run_PointCloudD11();
        }
    }
    break;
    case EtronDIImageType::DEPTH_14BITS:
    {
        auto Run_PointCloudD14 = [&](POINT_CLOUD_TYPE type = DEFAULT) {
            switch (type)
            {

            case DEFAULT:
            {
                if (!bDepthOnly)
                    memcpy(pPointCloudRGB, pColor, nDstH * nDstW * 3);
               // else
                    //memset(pPointCloudRGB, 0, nDstH * nDstW * 3);

                for (int py = 0; py < nDstH; ++py)
                {
                    const float Y_SHIFT = (py - CY);
                    for (int px = 0; px < nDstW; ++px)
                    {
                        const float Z = *p_depth++;

                        if (Z > Near && Z < Z_FAR) {
                            const float W_inv = Z *FOCAL_INV;
                            p_pointcloud[0] = (px - CX) * W_inv;
                            p_pointcloud[1]  = Y_SHIFT* W_inv;
                            p_pointcloud[2]  = Z;
                        }
                        else
                        {
                            p_pointcloud[0] = NAN;
                            p_pointcloud[1] = NAN;
                            p_pointcloud[2] = NAN;
                        }

                        p_pointcloud += 3;
                    }
                }
                break;
            }


            #if 0    
            case DEFAULT:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float Z = Depth[idx];
                        const float W = pPointCloudInfo->focalLength / Z;
                        if (W != 0.0f && Z < Far && Z > Near)
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[c_idx];
                                pPointCloudRGB[c_idx + 1] = pColor[c_idx + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[c_idx + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }

                            pPointCloudXYZ[c_idx] = (px - pPointCloudInfo->centerX) / W;
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }
                break;
            }
            #endif
            case MULTIPLE:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float Z = Depth[idx];
                        const float W = pPointCloudInfo->focalLength / Z;
                        const int shift = ((pPointCloudInfo->focalLength_K / Z) - (pPointCloudInfo->diff_K)) * (pPointCloudInfo->baseline_K);
                        if ((W != 0.0f && Z < Far && Z > Near) &&
                            ((py - shift > 0) && (py - shift < nDstH - 1)))
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[((py - shift) * nDstW + px) * 3];
                                pPointCloudRGB[c_idx + 1] = pColor[((py - shift) * nDstW + px) * 3 + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[((py - shift) * nDstW + px) * 3 + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }

                            pPointCloudXYZ[c_idx] = (px - pPointCloudInfo->centerX) / W;
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }

                break;
            }
            case CYLINDER:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float angle = (px - pPointCloudInfo->centerX) * (3.14159265359f / nDstW);
                        const float W = pPointCloudInfo->focalLength / Depth[idx];
                        const float Z = cos(angle) * (pPointCloudInfo->focalLength / W);
                        if (W != 0.0f && Z < Far && Z > Near)
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[c_idx];
                                pPointCloudRGB[c_idx + 1] = pColor[c_idx + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[c_idx + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }
                            pPointCloudXYZ[c_idx] = sin(angle) * (pPointCloudInfo->focalLength / W);
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }

                break;
            }
            case MULTIPLE_CYLINDER:
            {
                for (int px = 0; px < nDstW; ++px)
                {
                    for (int py = 0; py < nDstH; ++py)
                    {
                        const int idx = py * nDstW + px;
                        const int c_idx = idx * 3;
                        const float angle = (px - pPointCloudInfo->centerX) * (3.14159265359f / nDstW);
                        const float W = pPointCloudInfo->focalLength / Depth[idx];
                        const float Z = cos(angle) * (pPointCloudInfo->focalLength / W);
                        const int shift = ((pPointCloudInfo->focalLength_K / Z) - (pPointCloudInfo->diff_K)) * (pPointCloudInfo->baseline_K);
                        if ((W != 0.0f && Z < Far && Z > Near) &&
                            ((py - shift > 0) && (py - shift < nDstH - 1)))
                        {
                            if (!bDepthOnly)
                            {
                                pPointCloudRGB[c_idx] = pColor[((py - shift) * nDstW + px) * 3];
                                pPointCloudRGB[c_idx + 1] = pColor[((py - shift) * nDstW + px) * 3 + 1];
                                pPointCloudRGB[c_idx + 2] = pColor[((py - shift) * nDstW + px) * 3 + 2];
                            }
                            else
                            {
                                pPointCloudRGB[c_idx] = constR;
                                pPointCloudRGB[c_idx + 1] = constG;
                                pPointCloudRGB[c_idx + 2] = constB;
                            }

                            pPointCloudXYZ[c_idx] = sin(angle) * (pPointCloudInfo->focalLength / W);
                            pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                            pPointCloudXYZ[c_idx + 2] = Z;
                        }
                        else
                        {
                            pPointCloudXYZ[c_idx] = 0;
                            pPointCloudXYZ[c_idx + 1] = 0;
                            pPointCloudXYZ[c_idx + 2] = 0;
                        }
                    }
                }

                break;
            }
            default:
                break;
            }
        };
        if (ETronDI_PID_8054 == pid && pPointCloudInfo->focalLength_K)
        {
            Run_PointCloudD14(MULTIPLE);
        }
        else if (ETronDI_PID_8040S == pid)
        {
            if (pPointCloudInfo->focalLength_K)
                Run_PointCloudD14(MULTIPLE_CYLINDER);
            else
                Run_PointCloudD14(CYLINDER);
        }
        else
            Run_PointCloudD14();
    }
    break;
    default:
    { //d8
        for (int px = 0; px < nDstW; ++px)
        {
            for (int py = 0; py < nDstH; ++py)
            {
                const int idx = py * nDstW + px;
                const int c_idx = idx * 3;
                const float W = pPointCloudInfo->disparityToW[ImgDepth[idx]];
                const float Z = pPointCloudInfo->focalLength / W;
                if (W != 0.0f && Z < Far && Z > Near)
                {
                    if (!bDepthOnly)
                    {
                        pPointCloudRGB[c_idx] = pColor[c_idx];
                        pPointCloudRGB[c_idx + 1] = pColor[c_idx + 1];
                        pPointCloudRGB[c_idx + 2] = pColor[c_idx + 2];
                    }
                    else
                    {
                        pPointCloudRGB[c_idx] = constR;
                        pPointCloudRGB[c_idx + 1] = constG;
                        pPointCloudRGB[c_idx + 2] = constB;
                    }
                    pPointCloudXYZ[c_idx] = (px - pPointCloudInfo->centerX) / W;
                    pPointCloudXYZ[c_idx + 1] = (py - pPointCloudInfo->centerY) / W;
                    pPointCloudXYZ[c_idx + 2] = Z;
                }
                else
                {
                    pPointCloudXYZ[c_idx] = 0;
                    pPointCloudXYZ[c_idx + 1] = 0;
                    pPointCloudXYZ[c_idx + 2] = 0;
                }
            }
        }
    }
    break;
    }

#if !defined(NDEBUG)
    T[1] = clock();
    ec_time = ec_time * 0.9 + double(T[1] - T[0]) / CLOCKS_PER_SEC * 1000 * 0.1;
    // std::cout << "GetPointCloud : " << ec_time << std::endl;
#endif
    
    return ETronDI_OK;
}

int CComputer_Default::TableToData(int width, int height, int TableSize, unsigned short *Table,
                                   unsigned short *Src, unsigned short *Dst)
{
    for (int x = 0; x < width; ++x)
    {
        for (int y = 0; y < height; ++y)
        {
            const int idx = y * width + x;
            unsigned short value = Table[Src[idx]];
            Dst[idx] = ((value & 0xff) << 8) | ((value & 0xff00) >> 8);
        }
    }

    return ETronDI_OK;
}