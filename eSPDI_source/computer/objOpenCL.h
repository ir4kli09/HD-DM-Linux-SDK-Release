#pragma once

#include "CL/opencl.h"
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BOOL bool
#define MAX_PATH 260
#define BYTE unsigned char
#define TRUE true
#define FALSE false
class objOpenCL
{
    enum
    {
        MAX_WORK_ITEM_COUNT = 3,
    };

public:

    enum QUERY_INFO
    {
        Platform_Name,
        Plaeform_Vender,
        Platform_Version,

        Device_Name,
        Device_Vender,
        Device_Version,
        Device_ocl_Version,
    };
    objOpenCL();
    ~objOpenCL();

    BOOL   Initial();
    void   Uninitial();
    cl_int QueryOpenCL( char* pBuff, const size_t stBuffSize, const QUERY_INFO eQueryType );
    cl_int BuildProgramAndKernelBySource( const char* pSource, const char* pKernelName );
    cl_int BuildProgramAndKernelByBinaryFile( const char* pFilePath, const char* pKernelName );
    cl_int CreateProgramBinaryFile( const char* pFilePath );
    cl_int SetKernelBuffArg( const int    iArgIdx,
                             const size_t iBuffSize,
                             const void*  pBuff = NULL );
    cl_int GetKernelBuffArg( const int    iArgIdx,
                             const size_t iBuffSize,
                             void*        pBuff );
    cl_int SetKernelImageArg( const int    iArgIdx,
                              const size_t w,
                              const size_t h,
                              const void*  pRGB32 = NULL );
    cl_int GetKernelImageArg( const int    iArgIdx,
                              const size_t w,
                              const size_t h,
                              void*        pRGB32 );
    cl_int ExecuteKernel( const size_t global_x, const size_t global_y, double* pExtTime = NULL );

    inline BOOL IsSupportOpenCL() { return ( m_clPlatform != NULL ); }

private:

    cl_context       m_clContext;
    cl_platform_id   m_clPlatform;
    cl_device_id     m_clDevice;
    cl_command_queue m_clQueue;
    cl_program       m_clProgram;
    cl_kernel        m_clKernel;
    size_t           m_stPreferWorkItemsInGroup;
    size_t           m_maxWorkItemSize[ 3 ];
    size_t           m_maxWorkGroupSize;

    std::vector< cl_mem > m_vecKernelArg;
    
    cl_int BuildProgram( cl_program clProgram, const char* pKernelName );
};

