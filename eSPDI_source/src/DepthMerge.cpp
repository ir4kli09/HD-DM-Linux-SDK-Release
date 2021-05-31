// DepthMerge_DLL.cpp : Defines the exported functions for the DLL application.
//
#include <cmath>
#include "DepthMerge.h"

int depthMergeMBLbase(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit)
{
	// Puma use
	std::vector<double> vD(nMergeNum, 0);		// disparity, maybe 8 bit or 11 bit
	std::vector<double> vD_scale(nMergeNum, 0);	// disparity * baseline to longest baseline coordinate
	//printf("Near = %.2f %.2f\n", pWRNear[0], pWRNear[1]);
	//printf("Range = %.2f %.2f\n", pWRFusion[0], pWRFusion[1]);
	for (int i = 0; i < nDHeight; i++)
		for (int j = 0; j < nDWidth; j++)
		{
			int nBestDepth = -1;	// 0 1 2 3 -> D0 D1 D2 D3, -1 -> error pixel

			// Step 0 : Normalize to 8bit standard, that is 11bit -> 8bit for computation use, "Do not decrease accuracy"
			for (int k = 0; k < nMergeNum; k++)
			{
				if (bdepth2Byte11bit)			// transfer to 8 bit to show
					vD[k] = (double)(pDepthBufList[k][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[k][i*nDWidth * 2 + j * 2 + 1] * 256) / 8.0;
				else
					vD[k] = (double)pDepthBufList[k][i*nDWidth + j];
			}

			// Step 1 : Use maximum baseline as standard, because its accuracy is best
			for (int k = 0; k < nMergeNum; k++)
				vD_scale[k] = (vD[k] * (pBaseline[nMergeNum - 1] / pBaseline[k]));

			// Step 2 : Baseline ratio constraint among depth maps	
			for (int k = 1; k < nMergeNum; k++)											// depth 1 compare
				if (vD_scale[0] != 0)
					if (fabs(vD_scale[0] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[0] + 0.5))
						nBestDepth = k;

			for (int k = 2; k < nMergeNum; k++)											// depth 2 compare
				if (vD_scale[1] != 0 && nBestDepth == -1)
					if (fabs(vD_scale[1] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[1] + 0.5))
						nBestDepth = k;

			for (int k = 3; k < nMergeNum; k++)											// depth 3 compare
				if (vD_scale[2] != 0 && nBestDepth == -1)
					if (fabs(vD_scale[2] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[2] + 0.5))
						nBestDepth = k;

			// Step 3 : All candidcate depth maps are not related, it can be classify to two case
			// 1. One, two or more depth map enter blind region(IC limitation, only search 256 pixel)
			// 2. Depth map error region or depth map computation error
			// -> use depth map 0 as default, because it has less depth map error region, if depth map 0 not work, use depth map 1.....
			for (int k = 0; k < nMergeNum; k++)
				if (nBestDepth == -1 && vD[k] != 0.0)
					if (fFocus*pBaseline[k] / vD[k] > pWRNear[k] && fFocus*pBaseline[k] / vD[k] < pWRFar[k])
						nBestDepth = k;

			pDepthMergeFlag[i*nDWidth + j] = nBestDepth;
			// Step 4 : Use Alpha-Blending to smooth Z_error curve 
			double fFusionD = 0.0;

			if (bdepth2Byte11bit)
			{
				if (nBestDepth == -1)
					fFusionD = 0.0;
				else
					fFusionD = (double)pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 1] * 256;
			}
			else
			{
				if (nBestDepth == -1)
					fFusionD = 0.0;
				else
					fFusionD = (double)pDepthBufList[nBestDepth][i*nDWidth + j];
			}

            if (pWRFusion[0] != 0.0 && nBestDepth > 0 && std::abs(vD_scale[nBestDepth] - vD_scale[nBestDepth - 1]) < (pBaseline[nBestDepth] / pBaseline[nBestDepth - 1] + 0.5))									//
			{
				double fZ2 = fFocus *pBaseline[nBestDepth] / (pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 1] * 256) * 8.0;			// Z2

				if (fZ2 >= pWRNear[nBestDepth] && fZ2 <(pWRNear[nBestDepth] + pWRFusion[nBestDepth]))
				{
					double fZ1 = fFocus *pBaseline[nBestDepth - 1] / (pDepthBufList[nBestDepth - 1][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth - 1][i*nDWidth * 2 + j * 2 + 1] * 256)*8.0;			// Z1
					double fAlpha = (fZ2 - pWRNear[nBestDepth]) / pWRFusion[nBestDepth];
					double fFusionZ = fZ2*fAlpha + fZ1*(1.0 - fAlpha);

					//printf("Z1= %.2f Z2= %.2f ZFusion = %.2f fAlpha = %.2f\n", fZ1, fZ2, fFusionZ, fAlpha);

					if (bdepth2Byte11bit)
						fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ *8.0;
					else
						fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ;
				}
			}

			// Step 5 : Normalize to maximum baseline standard
			fFusionD *= (pBaseline[nMergeNum - 1] / pBaseline[nBestDepth]);

			// Step 6 : Normalize to 0~255 for visual purpose.
			if (bdepth2Byte11bit)
				fFusionD /= (8 * pBaseline[nMergeNum - 1] / pBaseline[0]);
			else
				fFusionD /= pBaseline[nMergeNum - 1] / pBaseline[0];

			// Step 7 : Assign disparity for pDepthMerge buffer		
			if (bdepth2Byte11bit)
			{
				if (nBestDepth == -1)
					pDepthMerge[i*nDWidth + j] = 0.0;
				else
					pDepthMerge[i*nDWidth + j] = fFusionD;
			}
			else
			{
				if (nBestDepth == -1)
					pDepthMerge[i*nDWidth + j] = 0.0;
				else
					pDepthMerge[i*nDWidth + j] = fFusionD;
			}

			//if (i == 157 && j == 340)	// Debug use
			//{
			//	printf("bBest = %d fFusionD = %.2f\n", nBestDepth, fFusionD);
			//	printf("pDepthMerge = %.2f\n", pDepthMerge[i*nDWidth + j]);
			//}
		}

	return 0;
}
int depthMergeMBRbaseV0(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit)
{
	// Puma use
	std::vector<double> vD(nMergeNum, 0);		// disparity, maybe 8 bit or 11 bit
	std::vector<double> vD_scale(nMergeNum, 0);	// disparity * baseline to longest baseline coordinate
	//printf("Near = %.2f %.2f\n", pWRNear[0], pWRNear[1]);
	//printf("Range = %.2f %.2f\n", pWRFusion[0], pWRFusion[1]);
	for (int i = 0; i < nDHeight; i++)
		for (int j = 0; j < nDWidth; j++)
		{
			int nBestDepth = -1;	// 0 1 2 3 -> D0 D1 D2 D3, -1 -> error pixel

			// Step 0 : Normalize to 8bit standard, that is 11bit -> 8bit for computation use, "Do not decrease accuracy"
			for (int k = 0; k < nMergeNum; k++)
			{
				if (bdepth2Byte11bit)			// transfer to 8 bit to show
					vD[k] = (double)(pDepthBufList[k][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[k][i*nDWidth * 2 + j * 2 + 1] * 256) / 8.0;
				else
					vD[k] = (double)pDepthBufList[k][i*nDWidth + j];
			}

			// Step 1 : Use maximum baseline as standard, because its accuracy is best
			for (int k = 0; k < nMergeNum; k++)
				vD_scale[k] = (vD[k] * (pBaseline[nMergeNum - 1] / pBaseline[k]));

			// Step 2 : Baseline ratio constraint among depth maps	
			for (int k = 1; k < nMergeNum; k++)											// depth 1 compare
				if (vD_scale[0] != 0)
					if (fabs(vD_scale[0] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[0] + 0.5))
						nBestDepth = k;

			for (int k = 2; k < nMergeNum; k++)											// depth 2 compare
				if (vD_scale[1] != 0 && nBestDepth == -1)
					if (fabs(vD_scale[1] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[1] + 0.5))
						nBestDepth = k;

			for (int k = 3; k < nMergeNum; k++)											// depth 3 compare
				if (vD_scale[2] != 0 && nBestDepth == -1)
					if (fabs(vD_scale[2] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[2] + 0.5))
						nBestDepth = k;

			// Step 3 : All candidcate depth maps are not related, it can be classify to two case
			// 1. One, two or more depth map enter blind region(IC limitation, only search 256 pixel)
			// 2. Depth map error region or depth map computation error
			// -> use depth map 0 as default, because it has less depth map error region, if depth map 0 not work, use depth map 1.....
			for (int k = 0; k < nMergeNum; k++)
				if (nBestDepth == -1 && vD[k] != 0.0)
				{
					// For Occlusion, if depth0 depth1 baseline is the same, use depth0 as default
					if (k == 0)
					{
						if (pBaseline[0] < pBaseline[1] || fabs(pBaseline[0] - pBaseline[1]) < 2.0)
							nBestDepth = 0;
					}
					else
					{
						if (fFocus*pBaseline[k] / vD[k] > pWRNear[k] && fFocus*pBaseline[k] / vD[k] < pWRFar[k])
							nBestDepth = k;
					}
				}

			pDepthMergeFlag[i*nDWidth + j] = nBestDepth;

			double fFusionD = 0.0;
			if (bdepth2Byte11bit)
			{
				if (nBestDepth == -1)
					fFusionD = 0.0;
				else
					fFusionD = (double)pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 1] * 256;
			}
			else
			{
				if (nBestDepth == -1)
					fFusionD = 0.0;
				else
					fFusionD = (double)pDepthBufList[nBestDepth][i*nDWidth + j];
			}

			// Step 4 : Use Alpha-Blending to smooth Z_error curve 
            if (pWRFusion[0] != 0.0 && nBestDepth > 0 && std::abs(vD_scale[nBestDepth] - vD_scale[nBestDepth - 1]) < (pBaseline[nBestDepth] / pBaseline[nBestDepth - 1] + 0.5))									//
			{
				double fZ2 = fFocus *pBaseline[nBestDepth] / (pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 1] * 256) * 8.0;			// Z2

				if (fZ2 >= pWRNear[nBestDepth] && fZ2 <(pWRNear[nBestDepth] + pWRFusion[nBestDepth]))
				{
					double fZ1 = fFocus *pBaseline[nBestDepth - 1] / (pDepthBufList[nBestDepth - 1][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth - 1][i*nDWidth * 2 + j * 2 + 1] * 256)*8.0;			// Z1
					double fAlpha = (fZ2 - pWRNear[nBestDepth]) / pWRFusion[nBestDepth];
					double fFusionZ = fZ2*fAlpha + fZ1*(1.0 - fAlpha);

					//printf("Z1= %.2f Z2= %.2f ZFusion = %.2f fAlpha = %.2f\n", fZ1, fZ2, fFusionZ, fAlpha);

					if (bdepth2Byte11bit)
						fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ *8.0;
					else
						fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ;
				}
			}

			// Step 5 : Normalize to maximum baseline standard
			fFusionD *= (pBaseline[nMergeNum - 1] / pBaseline[nBestDepth]);

			// Step 6 : Normalize to 0~255 for visual purpose.
			if (bdepth2Byte11bit)
				fFusionD /= (8 * pBaseline[nMergeNum - 1] / pBaseline[0]);
			else
				fFusionD /= pBaseline[nMergeNum - 1] / pBaseline[0];

			// Step 7 : Assign disparity for pDepthMerge buffer		
			if (bdepth2Byte11bit)
			{
				if (nBestDepth == -1)
					pDepthMerge[i*nDWidth + j] = 0.0;
				else
					pDepthMerge[i*nDWidth + j] = fFusionD;
			}
			else
			{
				if (nBestDepth == -1)
					pDepthMerge[i*nDWidth + j] = 0.0;
				else
					pDepthMerge[i*nDWidth + j] = fFusionD;
			}

			//if (i == 157 && j == 340)	// Debug use
			//{
			//	printf("bBest = %d fFusionD = %.2f\n", nBestDepth, fFusionD);
			//	printf("pDepthMerge = %.2f\n", pDepthMerge[i*nDWidth + j]);
			//}
		}

	return 0;
}

int depthMergeMBRbaseV1(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit)
{

	// Puma use
	std::vector<double> vD(nMergeNum, 0);		// disparity, maybe 8 bit or 11 bit
	std::vector<double> vD_scale(nMergeNum, 0);	// disparity * baseline to longest baseline coordinate
	//printf("Near = %.2f %.2f\n", pWRNear[0], pWRNear[1]);
	//printf("Range = %.2f %.2f\n", pWRFusion[0], pWRFusion[1]);
	for (int i = 0; i < nDHeight; i++)
		for (int j = 0; j < nDWidth; j++)
		{
			int nBestDepth = -1;	// 0 1 2 3 -> D0 D1 D2 D3, -1 -> error pixel

			// Step 0 : Normalize to 8bit standard, that is 11bit -> 8bit for computation use, "Do not decrease accuracy"
			for (int k = 0; k < nMergeNum; k++)
			{
				if (bdepth2Byte11bit)			// transfer to 8 bit to show
					vD[k] = (double)(pDepthBufList[k][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[k][i*nDWidth * 2 + j * 2 + 1] * 256) / 8.0;
				else
					vD[k] = (double)pDepthBufList[k][i*nDWidth + j];
			}

			// Step 1 : Use maximum baseline as standard, because its accuracy is best
			for (int k = 0; k < nMergeNum; k++)
				vD_scale[k] = (vD[k] * (pBaseline[nMergeNum - 1] / pBaseline[k]));

			// Step 2 : Baseline ratio constraint among depth maps	
			int nBestMainDepth = -1;

			for (int k = 1; k < nMergeNum; k++)											// depth 1 compare
				if (vD_scale[0] != 0)
					if (fabs(vD_scale[0] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[0] + 0.5))
					{
						nBestDepth = k;
						nBestMainDepth = 0;
					}

			for (int k = 2; k < nMergeNum; k++)											// depth 2 compare
				if (vD_scale[1] != 0 && nBestDepth == -1)
					if (fabs(vD_scale[1] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[1] + 0.5))
					{
						nBestDepth = k;
						nBestMainDepth = 1;
					}

			for (int k = 3; k < nMergeNum; k++)											// depth 3 compare
				if (vD_scale[2] != 0 && nBestDepth == -1)
					if (fabs(vD_scale[2] - vD_scale[k]) < (4.0 * pBaseline[k] / pBaseline[2] + 0.5))
					{
						nBestDepth = k;
						nBestMainDepth = 2;
					}

			// Step 3 : All candidcate depth maps are not related, it can be classify to two case
			// 1. One, two or more depth map enter blind region(IC limitation, only search 256 pixel)
			// 2. Depth map error region or depth map computation error
			// -> use depth map 0 as default, because it has less depth map error region, if depth map 0 not work, use depth map 1.....
			for (int k = 0; k < nMergeNum; k++)
				if (nBestDepth == -1 && vD[k] != 0.0)
				{
					if (nBestMainDepth == -1)
						nBestMainDepth = k;
					else
						nBestDepth = k;
				}

			if (nBestDepth == -1)
				nBestDepth = nBestMainDepth;
			else
			{
				if (fabs(vD_scale[nBestDepth] - vD_scale[nBestMainDepth]) > (8.0*pBaseline[nBestDepth] / pBaseline[nBestMainDepth]))
					nBestDepth = nBestMainDepth;
			}

			pDepthMergeFlag[i*nDWidth + j] = nBestDepth;

			double fFusionD = 0.0;
			double fFusionD1 = 0.0;
			double fFusionD2 = 0.0;
			if (bdepth2Byte11bit)
			{
				if (nBestDepth == -1)
					fFusionD = 0.0;
				else
				{
					fFusionD1 = ((double)pDepthBufList[nBestMainDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestMainDepth][i*nDWidth * 2 + j * 2 + 1] * 256)*pBaseline[nBestDepth] / pBaseline[nBestMainDepth];
					fFusionD2 = (double)pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 1] * 256;
					fFusionD = fFusionD1*(pBaseline[nBestMainDepth] / (pBaseline[nBestDepth] + pBaseline[nBestMainDepth])) + fFusionD2*(pBaseline[nBestDepth] / (pBaseline[nBestDepth] + pBaseline[nBestMainDepth]));
				}
				if (fFusionD > 2047.0)
					fFusionD = 2047.0;
			}
			else
			{
				if (nBestDepth == -1)
					fFusionD = 0.0;
				else
				{
					fFusionD1 = (double)pDepthBufList[nBestMainDepth][i*nDWidth + j];
					fFusionD2 = (double)pDepthBufList[nBestDepth][i*nDWidth + j];
					fFusionD = fFusionD1*(pBaseline[nBestMainDepth] / (pBaseline[nBestDepth] + pBaseline[nBestMainDepth])) + fFusionD2*(pBaseline[nBestDepth] / (pBaseline[nBestDepth] + pBaseline[nBestMainDepth]));
				}
				if (fFusionD > 255.0)
					fFusionD = 255.0;
			}

			// Step 4 : Use Alpha-Blending to smooth Z_error curve 
            if (pWRFusion[0] != 0.0 && nBestDepth > 0 && std::abs(vD_scale[nBestDepth] - vD_scale[nBestDepth - 1]) < (pBaseline[nBestDepth] / pBaseline[nBestDepth - 1] + 0.5))									//
			{
				double fZ2 = fFocus *pBaseline[nBestDepth] / (pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth][i*nDWidth * 2 + j * 2 + 1] * 256) * 8.0;			// Z2

				if (fZ2 >= pWRNear[nBestDepth] && fZ2 < (pWRNear[nBestDepth] + pWRFusion[nBestDepth]))
				{
					double fZ1 = fFocus *pBaseline[nBestDepth - 1] / (pDepthBufList[nBestDepth - 1][i*nDWidth * 2 + j * 2 + 0] + pDepthBufList[nBestDepth - 1][i*nDWidth * 2 + j * 2 + 1] * 256)*8.0;			// Z1
					double fAlpha = (fZ2 - pWRNear[nBestDepth]) / pWRFusion[nBestDepth];
					double fFusionZ = fZ2*fAlpha + fZ1*(1.0 - fAlpha);

					//printf("Z1= %.2f Z2= %.2f ZFusion = %.2f fAlpha = %.2f\n", fZ1, fZ2, fFusionZ, fAlpha);

					if (bdepth2Byte11bit)
						fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ *8.0;
					else
						fFusionD = fFocus * pBaseline[nBestDepth] / fFusionZ;
				}
			}

			// Step 5 : Normalize to maximum baseline standard
			fFusionD *= (pBaseline[nMergeNum - 1] / pBaseline[nBestDepth]);

			// Step 6 : Normalize to 0~255 for visual purpose.
			if (bdepth2Byte11bit)
				fFusionD /= (8 * pBaseline[nMergeNum - 1] / pBaseline[0]);
			else
				fFusionD /= pBaseline[nMergeNum - 1] / pBaseline[0];

			// Step 7 : Assign disparity for pDepthMerge buffer		
			if (bdepth2Byte11bit)
			{
				if (nBestDepth == -1)
					pDepthMerge[i*nDWidth + j] = 0.0;
				else
					pDepthMerge[i*nDWidth + j] = fFusionD;
			}
			else
			{
				if (nBestDepth == -1)
					pDepthMerge[i*nDWidth + j] = 0.0;
				else
					pDepthMerge[i*nDWidth + j] = fFusionD;
			}

			//if (i == 157 && j == 340)	// Debug use
			//{
			//	printf("bBest = %d fFusionD = %.2f\n", nBestDepth, fFusionD);
			//	printf("pDepthMerge = %.2f\n", pDepthMerge[i*nDWidth + j]);
			//}
		}

	return 0;
}
