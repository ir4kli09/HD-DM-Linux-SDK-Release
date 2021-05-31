/*! \file Post_SW.h
    \brief The definition of SW Post Processs functions
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#ifndef POST_H
#define POST_H

#ifdef POST_DLL_EXPORTS
#define POST_DLL_API __declspec(dllexport)
#else
#define POST_DLL_API
#endif

POST_DLL_API void* Post_Init();
POST_DLL_API void Post_SetParam(void * pPost, int Idx, int Val);
POST_DLL_API int Post_GetParam(void * pPost, int Idx);
POST_DLL_API void Post_SetColorFmt(void * pPost, int Fmt, int Type, int Flip, int Stride);
POST_DLL_API int Post_GetInfo(void * pPost, int Command);
POST_DLL_API void Post_Reset(void * pPost);
POST_DLL_API void Post_Destroy(void * pPost);
POST_DLL_API int Post_Process(void * pPost, unsigned char *pIn, unsigned char *pDepthBuf, unsigned char *pOut, int width, int height);

#define INFO_IS_GPU_AVAILABLE 0

#endif
