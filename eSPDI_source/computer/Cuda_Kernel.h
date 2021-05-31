__device__ float4 clamp(float4 x, float a, float b)
{
    return make_float4(max(a, min(b, x.x)),
                       max(a, min(b, x.y)),
                       max(a, min(b, x.z)),
                       max(a, min(b, x.w)));
}

__device__ int getX() { return blockIdx.x * blockDim.x + threadIdx.x; }
__device__ int getY() { return blockIdx.y * blockDim.y + threadIdx.y; }

__global__ void Kernel_YUY2_to_RGB24(unsigned char *YUY2, unsigned char *RGB24, int width, int height)
{
    int x = getX();
    int y = getY();

    if (x >= width || y >= height) return;
 
    int idx = (y * width + x) * 4;

    const int pt = idx / 2 * 3;

    const float4 YUYV = make_float4(YUY2[idx],
                                    YUY2[idx + 1] - 128.0f,
                                    YUY2[idx + 2],
                                    YUY2[idx + 3] - 128.0f);

    float4 pixel_1 = make_float4(YUYV.x + (1.4075f * YUYV.w),
                                 YUYV.x - (0.3455f * YUYV.y) - (0.7169f * YUYV.w),
                                 YUYV.x + (1.779f * YUYV.y), 0.0f);
    float4 pixel_2 = make_float4(YUYV.z + (1.4075f * YUYV.w),
                                 YUYV.z - (0.3455f * YUYV.y) - (0.7169f * YUYV.w),
                                 YUYV.z + (1.779f * YUYV.y), 0.0f);
    pixel_1 = clamp(pixel_1, 0.0f, 255.0f);
    pixel_2 = clamp(pixel_2, 0.0f, 255.0f);

    RGB24[pt] = pixel_1.x;
    RGB24[pt + 1] = pixel_1.y;
    RGB24[pt + 2] = pixel_1.z;
    RGB24[pt + 3] = pixel_2.x;
    RGB24[pt + 4] = pixel_2.y;
    RGB24[pt + 5] = pixel_2.z;
}

__global__ void Kernel_FlyingDepthCancellation_D8(unsigned char *pLImg, unsigned char *pDImg, unsigned char *pDImgOut, int width, int height)
{
    const int h_radius = 8;
    const int v_radius = 8;
    const int iREMOVE_D_MODE = 0;
    const int iREMOVE_D_THD = 2;
    const int iREMOVE_Y_THD = 5;
    const int iREMOVE_CURVE[17] = {150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 255, 255};
    const int iREMOVE_CNT_RATIO = 10;

    const int x = getX();
    const int y = getY();

    if (x >= width || y >= height)
        return;

    int center = x + y * width;
    int area_cnt = 0;
    int valid_cnt = 0;
    int all_cnt = 0;
    int d_8bit = pDImg[center];
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
                d_curr = (pDImg[x1 + y1 * width] << 3);
                y_curr = pLImg[x1 + y1 * width];
            }
            else
            {
                y_curr = d_curr = 0;
            }
            int cond = d_curr && (abs(d_8bit - (d_curr >> 3)) <= d_thd);
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

    pDImgOut[center] = (valid_cnt < ((area_cnt * iREMOVE_CNT_RATIO) >> 4) || (all_cnt < cnt_thd) ? 0 : pDImg[center]);
}

__global__ void Kernel_FlyingDepthCancellation_D11(unsigned char *pLImg, unsigned short *pD11Img, unsigned short *pDImgOut, int width, int height)
{
    const int h_radius = 8;
    const int v_radius = 8;
    const int iREMOVE_D_MODE = 0;
    const int iREMOVE_D_THD = 6;
    const int iREMOVE_Y_THD = 6;
    const int iREMOVE_CURVE[17] = {200, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180};
    const int iREMOVE_CNT_RATIO = 12;

    const int x = getX();
    const int y = getY();

    if (x >= width || y >= height) return;

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

__global__ void Kernel_DepthMerge(unsigned char *pDepth1, unsigned char *pDepth2, unsigned char *pDepth3, float *pDepthMerge, unsigned char *pDepthMergeFlag, float fFocus, float *pBaseline, float *pWRNear, float *pWRFar, float *pWRFusion, int nDWidth, int nDHeight)
{
    int nBestDepth = -1; // 0 1 2 3 -> D0 D1 D2 D3, -1 -> error pixel
    unsigned char *pDepthBufList = 0;

    const int j = getX();
    const int i = getY();

    if (j >= nDWidth || i >= nDHeight) return;

    float vD[3];
    float vD_scale[3];
    // Step 0 : Normalize to 8bit standard, that is 11bit -> 8bit for computation use, Do not decrease accuracy
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
            if ((fFocus) * pBaseline[k] / vD[k] > pWRNear[k] && (fFocus) * pBaseline[k] / vD[k] < pWRFar[k])
                nBestDepth = k;

    pDepthMergeFlag[i * nDWidth + j] = nBestDepth;
    // Step 4 : Use Alpha-Blending to smooth Z_error curve
    float fFusionD = 0.0;
    float diff = 0.0;
    if (nBestDepth != -1)
    {
        pDepthBufList = (nBestDepth == 0 ? pDepth1 : (nBestDepth == 1 ? pDepth2 : pDepth3));
        fFusionD = (float)pDepthBufList[i * nDWidth * 2 + j * 2 + 0] + pDepthBufList[i * nDWidth * 2 + j * 2 + 1] * 256;
    }
    if (pWRFusion[0] != 0.0 && nBestDepth > 0 && fabs(vD_scale[nBestDepth] - vD_scale[nBestDepth - 1]) < (pBaseline[nBestDepth] / pBaseline[nBestDepth - 1] + 0.5))
    {
        unsigned char *pPreDepthBufList = (nBestDepth == 1 ? pDepth1 : pDepth2);
        float fZ2 = (fFocus) * pBaseline[nBestDepth] / (pDepthBufList[i * nDWidth * 2 + j * 2 + 0] + pDepthBufList[i * nDWidth * 2 + j * 2 + 1] * 256) * 8.0;

        if (fZ2 >= pWRNear[nBestDepth] && fZ2 < (pWRNear[nBestDepth] + pWRFusion[nBestDepth]))
        {
            float fZ1 = (fFocus) * pBaseline[nBestDepth - 1] / (pPreDepthBufList[i * nDWidth * 2 + j * 2 + 0] + pPreDepthBufList[i * nDWidth * 2 + j * 2 + 1] * 256) * 8.0;
            float fAlpha = (fZ2 - pWRNear[nBestDepth]) / pWRFusion[nBestDepth];
            float fFusionZ = fZ2 * fAlpha + fZ1 * (1.0 - fAlpha);

            //printf(Z1= %.2f Z2= %.2f ZFusion = %.2f fAlpha = %.2f\n, fZ1, fZ2, fFusionZ, fAlpha);
            fFusionD = (fFocus) * pBaseline[nBestDepth] / fFusionZ * 8.0;
        }
    }

    // Step 5 : Normalize to maximum baseline standard
    if (nBestDepth != -1)
        fFusionD *= (pBaseline[2] / pBaseline[nBestDepth]);

    // Step 6 : Normalize to 0~255 for visual purpose.
    fFusionD /= (8 * pBaseline[2] / pBaseline[0]);

    // Step 7 : Assign disparity for pDepthMerge buffer
    if (nBestDepth == -1)
        pDepthMerge[i * nDWidth + j] = 0.0;
    else
        pDepthMerge[i * nDWidth + j] = fFusionD;
}

__global__ void rotate_YUY2(unsigned char *YUY2, unsigned char *dst, int width, int height)
{
    int x = getX();
    int y = getY();

    if (x >= width || y >= height) return;

    width *= 2;
    height *= 2;
    x *= 2;
    y *= 2;

    int jBase[2] = {y * width * 2, y * width * 2 + width * 2};
    int dstY[2][2] = {{0, 0}, {0, 0}};

    dstY[0][0] = (x * height + (height - 1 - y)) * 2;
    dst[dstY[0][0]] = YUY2[jBase[0] + x * 2];
    dst[dstY[0][0] + 1] = YUY2[jBase[0] + x * 2 + 3];

    dstY[0][1] = ((x + 1) * height + (height - 1 - y)) * 2;
    dst[dstY[0][1]] = YUY2[jBase[0] + x * 2 + 2];
    dst[dstY[0][1] + 1] = YUY2[jBase[0] + x * 2 + 3];

    dstY[1][0] = (x * height + (height - 1 - (y + 1))) * 2;
    dst[dstY[1][0]] = YUY2[jBase[1] + x * 2];
    dst[dstY[1][0] + 1] = YUY2[jBase[1] + x * 2 + 1];

    dstY[1][1] = ((x + 1) * height + (height - 1 - (y + 1))) * 2;
    dst[dstY[1][1]] = YUY2[jBase[1] + x * 2 + 2];
    dst[dstY[1][1] + 1] = YUY2[jBase[1] + x * 2 + 1];
}

__global__ void rotate_dim(unsigned char *src, unsigned char *dst, const int dim, int width, int height)
{
    const int L_dim = dim;
    const int x = getX();
    const int y = getY();

    if (x >= width || y >= height) return;

    for (int i = 0; i < L_dim; i++)
        dst[(x * height + y) * L_dim + i] = src[((height - y - 1) * width + x) * L_dim + i];
}

__global__ void mirro_dim(unsigned char *src, unsigned char *dst, const int dim, int width, int height)
{
    const int L_dim = dim;
    const int x = getX();
    const int y = getY();

    if (x >= width || y >= height)
        return;

    for (int i = 0; i < L_dim; i++)
        dst[(y * width + x) * L_dim + i] = src[(y * width + (width - x - 1)) * L_dim + i];
}

__global__ void Kernel_Resample(unsigned char *SrcData, const int SrcW, const int SrcH,
                                unsigned char *DstData, int *bytePerPixel,
                                const int DestW, const int DestH)
{
    if (getX() >= DestW || getY() >= DestH) return;

    unsigned char p00, p01, p10, p11;
    int idx = getY() * DestW + getX();
    float x_fac = (float)SrcW / DestW;
    float y_fac = (float)SrcH / DestH;
    float outx = x_fac * getX();
    float outy = y_fac * getY();
    int x0 = (int)outx;
    int y0 = (int)outy;
    int x1 = x0 < SrcW - 1 ? x0 + 1 : x0;
    int y1 = y0 < SrcH - 1 ? y0 + 1 : y0;
    float rx = outx - x0;
    float ry = outy - y0;
    int p00_index = (y0 * SrcW + x0) * (*bytePerPixel);
    int p01_index = (y0 * SrcW + x1) * (*bytePerPixel);
    int p10_index = (y1 * SrcW + x0) * (*bytePerPixel);
    int p11_index = (y1 * SrcW + x1) * (*bytePerPixel);
    for (int byte = 0; byte < *bytePerPixel; ++byte)
    {
        p00 = SrcData[p00_index + byte];
        p01 = SrcData[p01_index + byte];
        p10 = SrcData[p10_index + byte];
        p11 = SrcData[p11_index + byte];
        if (((p00 == 0) | (p01 == 0) | (p10 == 0) | (p11 == 0)))
        {
            DstData[idx * (*bytePerPixel) + byte] = 0;
        }
        else
        {
            float p0 = rx * p01 + (1 - rx) * p00;
            float p1 = rx * p11 + (1 - rx) * p10;
            DstData[idx * (*bytePerPixel) + byte] = (unsigned short)(ry * p1 + (1 - ry) * p0);
        }
    }
}

__global__ void Kernel_PointCloud_D14(unsigned char *Color, unsigned short *Depth,
                                      float centerX, float centerY,
                                      float focalLength, float Near, float Far,
                                      unsigned char *pointcloudRGB, float *pointcloudXYZ, int width, int height)
{
    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height) return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float Z = Depth[idx];
    const float W = focalLength / Z;
    if (W != 0.0f && Z < Far && Z > Near)
    {
        pointcloudRGB[c_idx] = Color[c_idx];
        pointcloudRGB[c_idx + 1] = Color[c_idx + 1];
        pointcloudRGB[c_idx + 2] = Color[c_idx + 2];
        pointcloudXYZ[c_idx] = (px - centerX) / W;
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_D11(unsigned char *Color, unsigned short *Depth,
                                      float centerX, float centerY,
                                      float focalLength, float Near, float Far,
                                      float *disparityToW, unsigned char *pointcloudRGB, float *pointcloudXYZ, int width, int height)
{

    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float W = disparityToW[Depth[idx]];
    const float Z = focalLength / W;
    if (W != 0.0f && Z < Far && Z > Near)
    {
        pointcloudRGB[c_idx] = Color[c_idx];
        pointcloudRGB[c_idx + 1] = Color[c_idx + 1];
        pointcloudRGB[c_idx + 2] = Color[c_idx + 2];
        pointcloudXYZ[c_idx] = (px - centerX) / W;
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_D8(unsigned char *Color, unsigned char *Depth,
                                     float centerX, float centerY,
                                     float focalLength, float Near, float Far,
                                     float *disparityToW, unsigned char *pointcloudRGB, float *pointcloudXYZ, int width, int height)
{
    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float W = disparityToW[Depth[idx]];
    const float Z = focalLength / W;
    if (W != 0.0f && Z < Far && Z > Near)
    {
        pointcloudRGB[c_idx] = Color[c_idx];
        pointcloudRGB[c_idx + 1] = Color[c_idx + 1];
        pointcloudRGB[c_idx + 2] = Color[c_idx + 2];
        pointcloudXYZ[c_idx] = (px - centerX) / W;
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_8029(unsigned char *Color, unsigned char *Depth,
                                       float centerX, float centerY,
                                       float focalLength, float Near, float Far,
                                       float *disparityToW, unsigned char *pointcloudRGB, float *pointcloudXYZ, int width, int height)
{

    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float W = disparityToW[Depth[idx]];
    const float Z = focalLength / W;
    if (W != 0.0f && Z < Far && Z > Near)
    {
        pointcloudRGB[c_idx] = Color[c_idx];
        pointcloudRGB[c_idx + 1] = Color[c_idx + 1];
        pointcloudRGB[c_idx + 2] = Color[c_idx + 2];
        pointcloudXYZ[c_idx] = (px - centerX) / W;
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_CylinderD11(unsigned char *Color, unsigned short *Depth,
                                              float centerX, float centerY,
                                              float focalLength, float Near, float Far,
                                              float *disparityToW, unsigned char *pointcloudRGB, float *pointcloudXYZ, 
                                              int width, int height)
{
    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float angle = (px - centerX) * (3.14159265359f / width);
    const float W = disparityToW[Depth[idx]];
    const float Z = cos(angle) * (focalLength / W);
    if (W != 0.0f && Z < Far && Z > Near)
    {
        pointcloudRGB[c_idx] = Color[c_idx];
        pointcloudRGB[c_idx + 1] = Color[c_idx + 1];
        pointcloudRGB[c_idx + 2] = Color[c_idx + 2];
        pointcloudXYZ[c_idx] = sin(angle) * (focalLength / W);
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_CylinderD14(unsigned char *Color, unsigned short *Depth,
                                              float centerX, float centerY,
                                              float focalLength, float Near, float Far,
                                              unsigned char *pointcloudRGB, float *pointcloudXYZ, 
                                              int width, int height)
{

    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float angle = (px - centerX) * (3.14159265359f / width);
    const float W = focalLength / Depth[idx];
    const float Z = cos(angle) * (focalLength / W);
    if (W != 0.0f && Z < Far && Z > Near)
    {
        pointcloudRGB[c_idx] = Color[c_idx];
        pointcloudRGB[c_idx + 1] = Color[c_idx + 1];
        pointcloudRGB[c_idx + 2] = Color[c_idx + 2];
        pointcloudXYZ[c_idx] = sin(angle) * (focalLength / W);
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_MultipleCylinderD11(unsigned char *Color, unsigned short *Depth,
                                                      float centerX, float centerY, float focalLength,
                                                      float baseline_K, float diff_K, float focalLength_K,
                                                      float Near, float Far,
                                                      float *disparityToW, unsigned char *pointcloudRGB, float *pointcloudXYZ, int width, int height)
{

    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float angle = (px - centerX) * (3.14159265359f / width);
    const float W = disparityToW[Depth[idx]];
    const float Z = cos(angle) * (focalLength / W);
    const int shift = ((focalLength_K / Z) - (diff_K)) * (baseline_K);
    if ((W != 0.0f && Z < Far && Z > Near) &&
        ((py - shift > 0) && (abs(py - shift) < height - 1)))
    {
        pointcloudRGB[c_idx] = Color[((py - shift) * width + px) * 3];
        pointcloudRGB[c_idx + 1] = Color[((py - shift) * width + px) * 3 + 1];
        pointcloudRGB[c_idx + 2] = Color[((py - shift) * width + px) * 3 + 2];
        pointcloudXYZ[c_idx] = sin(angle) * (focalLength / W);
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_MultipleCylinderD14(unsigned char *Color, unsigned short *Depth,
                                                      float centerX, float centerY, float focalLength,
                                                      float baseline_K, float diff_K, float focalLength_K,
                                                      float Near, float Far,
                                                      unsigned char *pointcloudRGB, float *pointcloudXYZ,
                                                      int width, int height)
{

    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float angle = (px - centerX) * (3.14159265359f / width);
    const float W = focalLength / Depth[idx];
    const float Z = cos(angle) * (focalLength / W);
    const int shift = ((focalLength_K / Z) - (diff_K)) * (baseline_K);
    if ((W != 0.0f && Z < Far && Z > Near) &&
        ((py - shift > 0) && (py - shift < height - 1)))
    {
        pointcloudRGB[c_idx] = Color[((py - shift) * width + px) * 3];
        pointcloudRGB[c_idx + 1] = Color[((py - shift) * width + px) * 3 + 1];
        pointcloudRGB[c_idx + 2] = Color[((py - shift) * width + px) * 3 + 2];
        pointcloudXYZ[c_idx] = sin(angle) * (focalLength / W);
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_MultipleD11(unsigned char *Color, unsigned short *Depth,
                                              float centerX, float centerY, float focalLength,
                                              float baseline_K, float diff_K, float focalLength_K,
                                              float Near, float Far,
                                              float *disparityToW, unsigned char *pointcloudRGB, float *pointcloudXYZ, 
                                              int width, int height)
{

    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float W = disparityToW[Depth[idx]];
    const float Z = focalLength / W;
    const int shift = ((focalLength_K / Z) - (diff_K)) * (baseline_K);
    if ((W != 0.0f && Z < Far && Z > Near) &&
        ((py - shift > 0) && (py - shift < height - 1)))
    {
        pointcloudRGB[c_idx] = Color[((py - shift) * width + px) * 3];
        pointcloudRGB[c_idx + 1] = Color[((py - shift) * width + px) * 3 + 1];
        pointcloudRGB[c_idx + 2] = Color[((py - shift) * width + px) * 3 + 2];
        pointcloudXYZ[c_idx] = (px - centerX) / W;
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }
}

__global__ void Kernel_PointCloud_MultipleD14(unsigned char *Color, unsigned short *Depth,
                                              float centerX, float centerY,
                                              float focalLength, float baseline_K,
                                              float diff_K, float focalLength_K,
                                              float Near, float Far,
                                              unsigned char *pointcloudRGB,
                                              float *pointcloudXYZ, 
                                              int width, int height)
{
    const int px = getX();
    const int py = getY();

    if (px >= width || py >= height)
        return;

    const int idx = py * width + px;
    const int c_idx = idx * 3;
    const float Z = Depth[idx];
    const float W = focalLength / Z;
    const int shift = ((focalLength_K / Z) - (diff_K)) * (baseline_K);
    if ((W != 0.0f && Z < Far && Z > Near) &&
        ((py - shift > 0) && (py - shift < height - 1)))
    {
        pointcloudRGB[c_idx] = Color[((py - shift) * width + px) * 3];
        pointcloudRGB[c_idx + 1] = Color[((py - shift) * width + px) * 3 + 1];
        pointcloudRGB[c_idx + 2] = Color[((py - shift) * width + px) * 3 + 2];
        pointcloudXYZ[c_idx] = (px - centerX) / W;
        pointcloudXYZ[c_idx + 1] = (py - centerY) / W;
        pointcloudXYZ[c_idx + 2] = Z;
    }
    else
    {
        pointcloudXYZ[c_idx] = 0;
        pointcloudXYZ[c_idx + 1] = 0;
        pointcloudXYZ[c_idx + 2] = 0;
    }

}

__global__ void Kernel_TableToData(unsigned short *pTable, unsigned short *pSrc, unsigned short *pDst, int width, int height)
{
    const int idx = getY() * width + getX();
    unsigned short value =  pTable[ pSrc[ idx ] ];
    pDst[ idx ] = ((value & 0xff) << 8) | ((value & 0xff00) >> 8);
}