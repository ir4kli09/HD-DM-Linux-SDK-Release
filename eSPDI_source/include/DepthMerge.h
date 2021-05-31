/*! \file DepthMerge.h
    \brief Depth Merge functions, data structure and variable definition
    Copyright:
    This file copyright (C) 2018 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#ifndef DEPTH_MERGE_H
#define DEPTH_MERGE_H
#include <stdio.h>
#include <vector>
int depthMergeMBLbase(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit = 1);
int depthMergeMBRbaseV0(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit = 1);
int depthMergeMBRbaseV1(unsigned char ** pDepthBufList, double *pDepthMerge, unsigned char *pDepthMergeFlag, int nDWidth, int nDHeight, double fFocus, double * pBaseline, double * pWRNear, double * pWRFar, double * pWRFusion, int nMergeNum, bool bdepth2Byte11bit = 1);
#endif
