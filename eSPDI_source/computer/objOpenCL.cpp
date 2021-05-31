#include "objOpenCL.h"
#include <math.h>
#define G_X    0
#define G_Y    1
#define DIM_XY 2

objOpenCL::objOpenCL() : m_clContext(NULL),
                         m_clPlatform(NULL),
                         m_clDevice(NULL),
                         m_clQueue(NULL),
                         m_clProgram(NULL),
                         m_clKernel(NULL),
                         m_stPreferWorkItemsInGroup(NULL)
{
}

objOpenCL::~objOpenCL()
{
    Uninitial();
}

cl_int objOpenCL::BuildProgram( cl_program clProgram, const char* pKernelName )
{
    cl_int    ciErrNum = clBuildProgram( clProgram, 1, &m_clDevice, NULL, NULL, NULL );
    cl_kernel clkernel = NULL;

    if( CL_SUCCESS != ciErrNum )
    {
        size_t stlog = NULL;

        clGetProgramBuildInfo( clProgram, m_clDevice, CL_PROGRAM_BUILD_LOG, NULL, NULL, &stlog );

        char* plog = new char[ stlog ];

        clGetProgramBuildInfo( clProgram, m_clDevice, CL_PROGRAM_BUILD_LOG, stlog, plog, NULL );

        printf("%s\n", plog);

        delete[] plog;

        return ciErrNum;
    }
    do
    {
        if ( !m_clQueue )
        {
            m_clQueue = clCreateCommandQueue( m_clContext, m_clDevice, CL_QUEUE_PROFILING_ENABLE, &ciErrNum ); if( CL_SUCCESS != ciErrNum ) break;
        }
        clkernel = clCreateKernel( clProgram, pKernelName, &ciErrNum );
    
        if( clkernel )
        {
            size_t stPreferWorkItemsInGroup = NULL;

            ciErrNum = clGetKernelWorkGroupInfo( clkernel, m_clDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof( size_t ), &stPreferWorkItemsInGroup, NULL );

            if( CL_SUCCESS == ciErrNum )
            {
                m_clProgram = clProgram;
                m_clKernel  = clkernel;
                m_stPreferWorkItemsInGroup = stPreferWorkItemsInGroup;

                return CL_SUCCESS;
            }
        }
    }
    while( FALSE );

    if ( clkernel ) clReleaseKernel( clkernel );

    if( m_clQueue )
    {
        clReleaseCommandQueue( m_clQueue );

        m_clQueue = NULL;
    }
    return ciErrNum;
}

BOOL objOpenCL::Initial()
{
    cl_uint ciPlatformCount = NULL;
    cl_uint ciDeviceCount   = NULL;
    cl_int  ciErrNum        = NULL;

    clGetPlatformIDs( NULL, NULL, &ciPlatformCount );

    std::vector< cl_platform_id > v_platform( ciPlatformCount );

    clGetPlatformIDs( ciPlatformCount, &v_platform[ NULL ], &ciPlatformCount );

    for (auto &clPlatformID : v_platform)
    {
        cl_context_properties prop[] = { CL_CONTEXT_PLATFORM, reinterpret_cast< cl_context_properties >( clPlatformID ), NULL };

        m_clContext = clCreateContextFromType( prop, CL_DEVICE_TYPE_GPU, NULL, NULL, &ciErrNum );

        if ( CL_SUCCESS != ciErrNum ) continue;

        clGetDeviceIDs( clPlatformID, CL_DEVICE_TYPE_GPU, NULL, NULL, &ciDeviceCount );

        std::vector< cl_device_id > v_device( ciDeviceCount );

        clGetDeviceIDs( clPlatformID, CL_DEVICE_TYPE_GPU, ciDeviceCount, &v_device[ NULL ], &ciDeviceCount );

        char cBuffer[ MAX_PATH ] = { NULL };

        for ( auto& clDeviceID : v_device )
        {
            clGetDeviceInfo( clDeviceID, CL_DEVICE_OPENCL_C_VERSION,    sizeof( cBuffer ),    cBuffer,             NULL );
            clGetDeviceInfo( clDeviceID, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof( size_t ),     &m_maxWorkGroupSize, NULL );
            clGetDeviceInfo( clDeviceID, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof( size_t ) * 3, m_maxWorkItemSize,   NULL );

            if ( strstr( cBuffer, "OpenCL C" ) )
            {
                m_clPlatform = clPlatformID;
                m_clDevice   = clDeviceID;

                return TRUE;
            }
        }
        clReleaseContext( m_clContext );
    }
    return FALSE;
}

void objOpenCL::Uninitial()
{
    for ( auto& KernelArg : m_vecKernelArg )
    {
        clReleaseMemObject( KernelArg );
    }  
    if ( m_clKernel  ) { clReleaseKernel( m_clKernel );      m_clKernel  = NULL; }
    if ( m_clProgram ) { clReleaseProgram( m_clProgram );    m_clProgram = NULL; }
    if ( m_clQueue   ) { clReleaseCommandQueue( m_clQueue ); m_clQueue   = NULL; }
    if ( m_clDevice  ) { clReleaseDevice( m_clDevice );      m_clDevice  = NULL; }
    if ( m_clContext ) { clReleaseContext( m_clContext );    m_clContext = NULL; }

    m_vecKernelArg.clear();
}

cl_int objOpenCL::QueryOpenCL( char* pBuff, const size_t stBuffSize, const QUERY_INFO eQueryType )
{
    if ( NULL == m_clPlatform ) return CL_INVALID_PLATFORM;
    if ( NULL == m_clDevice   ) return CL_INVALID_DEVICE;

    switch ( eQueryType )
    {
    case Platform_Name:      return clGetPlatformInfo ( m_clPlatform, CL_PLATFORM_NAME,           stBuffSize, pBuff, NULL );
    case Plaeform_Vender :   return clGetPlatformInfo ( m_clPlatform, CL_PLATFORM_VENDOR,         stBuffSize, pBuff, NULL );
    case Platform_Version:   return clGetPlatformInfo ( m_clPlatform, CL_PLATFORM_VERSION,        stBuffSize, pBuff, NULL );
    case Device_Name:        return clGetDeviceInfo   ( m_clDevice,   CL_DEVICE_NAME,             stBuffSize, pBuff, NULL );
    case Device_Vender:      return clGetDeviceInfo   ( m_clDevice,   CL_DEVICE_VENDOR,           stBuffSize, pBuff, NULL );
    case Device_Version:     return clGetDeviceInfo   ( m_clDevice,   CL_DRIVER_VERSION,          stBuffSize, pBuff, NULL );
    case Device_ocl_Version: return clGetDeviceInfo   ( m_clDevice,   CL_DEVICE_OPENCL_C_VERSION, stBuffSize, pBuff, NULL );
    }
    return CL_INVALID_VALUE;
}

cl_int objOpenCL::BuildProgramAndKernelBySource( const char* pSource, const char* pKernelName )
{
    if ( NULL == m_clContext ) return CL_INVALID_CONTEXT;

    cl_int     ciErrNum  = CL_SUCCESS;
    cl_program clProgram = clCreateProgramWithSource( m_clContext, 1, &pSource, NULL, &ciErrNum );

    return clProgram ? BuildProgram( clProgram, pKernelName ) : ciErrNum;
}

cl_int objOpenCL::BuildProgramAndKernelByBinaryFile( const char* pFilePath, const char* pKernelName )
{
    if ( NULL == m_clContext ) return CL_INVALID_CONTEXT;
    if ( NULL == m_clDevice  ) return CL_INVALID_DEVICE;

    FILE* fp = fopen( pFilePath, "rb");

    if ( !fp ) return CL_INVALID_VALUE;

    fseek( fp, NULL, SEEK_END );

    const size_t clProgSize = ftell( fp );

    fseek( fp, NULL, SEEK_SET );

    BYTE* pProgData = new BYTE[ clProgSize ];

    fread( pProgData, clProgSize, 1, fp );
    fclose( fp );

    cl_int     ciErrNum  = CL_SUCCESS;
    cl_program clProgram = clCreateProgramWithBinary( m_clContext, 1, &m_clDevice, &clProgSize, ( const BYTE** )&pProgData, NULL, &ciErrNum );

    delete[] pProgData;

    return clProgram ? BuildProgram( clProgram, pKernelName ) : ciErrNum;
}

cl_int objOpenCL::CreateProgramBinaryFile( const char* pFilePath )
{
    if ( NULL == m_clProgram ) return CL_INVALID_PROGRAM;

    FILE* fp = fopen( pFilePath, "wb");

    if ( !fp ) return CL_INVALID_VALUE;

    cl_ulong clProgSize = NULL;

    cl_int ciErrNum = clGetProgramInfo( m_clProgram, CL_PROGRAM_BINARY_SIZES, sizeof( cl_ulong ), &clProgSize, NULL );

    if ( CL_SUCCESS == ciErrNum )
    {
        BYTE* pProgData = new BYTE[ clProgSize ];

        ciErrNum = clGetProgramInfo( m_clProgram, CL_PROGRAM_BINARIES, clProgSize, &pProgData, NULL );

        if ( CL_SUCCESS == ciErrNum ) fwrite( pProgData, clProgSize, 1, fp );

        delete[] pProgData;
    }
    fclose( fp );

    return ciErrNum;
}

cl_int objOpenCL::SetKernelBuffArg( const int    iArgIdx,
                                    const size_t iBuffSize,
                                    const void*  pBuff )
{
    if ( NULL == m_clContext ) return CL_INVALID_CONTEXT;
    if ( NULL == m_clKernel  ) return CL_INVALID_KERNEL;
    if ( NULL == m_clQueue   ) return CL_INVALID_COMMAND_QUEUE;

    static const BYTE paterm = NULL;

    auto CreateKernelArg = [ = ]()
    {
        cl_int ciErrNum  = CL_SUCCESS; 
        cl_mem KernelArg = pBuff ? 
                                clCreateBuffer( m_clContext, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, iBuffSize, ( void* )pBuff, &ciErrNum ) :
                                iBuffSize ?
                                clCreateBuffer( m_clContext, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, iBuffSize, NULL, &ciErrNum ) : NULL;
        
        if (CL_SUCCESS != ciErrNum) return ciErrNum;

        //if ( !pBuff ) clEnqueueFillBuffer( m_clQueue, KernelArg, &paterm, 1, NULL, iBuffSize, NULL, NULL, NULL );

        ciErrNum = clSetKernelArg( m_clKernel, iArgIdx, sizeof( cl_mem ), &KernelArg );

        if( CL_SUCCESS == ciErrNum )
        {
            if ( iArgIdx < m_vecKernelArg.size() )
            {
                clReleaseMemObject( m_vecKernelArg[ iArgIdx ] );
                m_vecKernelArg[ iArgIdx ] = KernelArg;
            }
            else m_vecKernelArg.push_back( KernelArg );
        }
        else clReleaseMemObject( KernelArg );

        return ciErrNum;
        
    };
    if ( iArgIdx < m_vecKernelArg.size() )
    {
        size_t MemSize = NULL;

        clGetMemObjectInfo( m_vecKernelArg[ iArgIdx ], CL_MEM_SIZE, sizeof( size_t ), &MemSize, NULL );

        if ( MemSize < iBuffSize || !iBuffSize ) return CreateKernelArg();

        if ( pBuff ) return clEnqueueWriteBuffer( m_clQueue, m_vecKernelArg[ iArgIdx ], CL_TRUE, NULL, iBuffSize, pBuff, NULL, NULL, NULL );
        //else         return clEnqueueFillBuffer( m_clQueue, m_vecKernelArg[ iArgIdx ], &paterm, 1, NULL, iBuffSize, NULL, NULL, NULL );
    }
    else return CreateKernelArg();

    return CL_SUCCESS;
}

cl_int objOpenCL::GetKernelBuffArg( const int    iArgIdx,
                                    const size_t iBuffSize,
                                    void*        pBuff )
{
    if ( NULL == m_clQueue ) return CL_INVALID_COMMAND_QUEUE;

    if ( iArgIdx >= ( int )m_vecKernelArg.size() ) return CL_INVALID_ARG_INDEX;

    return clEnqueueReadBuffer( m_clQueue,
                                m_vecKernelArg[ iArgIdx ],
                                CL_TRUE,
                                NULL,
                                iBuffSize,
                                pBuff,
                                NULL,
                                NULL,
                                NULL );
}

cl_int objOpenCL::SetKernelImageArg( const int    iArgIdx,
                                     const size_t w,
                                     const size_t h,
                                     const void*  pRGB32 )
{
    if ( NULL == m_clContext ) return CL_INVALID_CONTEXT;
    if ( NULL == m_clKernel  ) return CL_INVALID_KERNEL;
    if ( NULL == m_clQueue   ) return CL_INVALID_COMMAND_QUEUE;
    if ( NULL == w           ) return CL_INVALID_ARG_SIZE;
    if ( NULL == h           ) return CL_INVALID_ARG_SIZE;

    auto CreateKernelArg = [ = ]()
    {
        const cl_image_format xformat = { CL_BGRA, CL_UNSIGNED_INT8 };

        cl_int ciErrNum  = CL_INVALID_ARG_SIZE; 
        cl_mem KernelArg = pRGB32 ? clCreateImage2D( m_clContext, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &xformat, w, h, NULL, ( void* )pRGB32, &ciErrNum ) :
                                    clCreateImage2D( m_clContext, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, &xformat, w, h, NULL, NULL, &ciErrNum );
        if( KernelArg )
        {
            ciErrNum = clSetKernelArg( m_clKernel, iArgIdx, sizeof( cl_mem ), &KernelArg );

            if( CL_SUCCESS == ciErrNum )
            {
                if ( iArgIdx < m_vecKernelArg.size() )
                {
                    clReleaseMemObject( m_vecKernelArg[ iArgIdx ] );
                    m_vecKernelArg[ iArgIdx ] = KernelArg;
                }
                else m_vecKernelArg.push_back( KernelArg );
            }
            else clReleaseMemObject( KernelArg );
        }
        return ciErrNum;
    };
    if ( iArgIdx < m_vecKernelArg.size() )
    {
        const size_t origin[] = { NULL, NULL, NULL };
        const size_t region[] = { w, h, 1 };

        size_t MemSize = NULL;
        clGetMemObjectInfo( m_vecKernelArg[ iArgIdx ], CL_MEM_SIZE, sizeof( size_t ), &MemSize, NULL );

        if ( MemSize < w * h * 4 ) return CreateKernelArg();

        if ( pRGB32 ) return clEnqueueWriteImage( m_clQueue, m_vecKernelArg[ iArgIdx ], CL_TRUE, origin, region, NULL, NULL, pRGB32, NULL, NULL, NULL );
    }
    else return CreateKernelArg();

    return CL_SUCCESS;
}

cl_int objOpenCL::GetKernelImageArg( const int    iArgIdx,
                                     const size_t w,
                                     const size_t h,
                                     void*        pRGB32 )
{
    if ( NULL == m_clQueue ) return CL_INVALID_COMMAND_QUEUE;

    if ( iArgIdx >= ( int )m_vecKernelArg.size() ) return CL_INVALID_ARG_INDEX;

    const size_t origin[] = { NULL, NULL, NULL };
    const size_t region[] = { w, h, 1 };

    return clEnqueueReadImage( m_clQueue,
                               m_vecKernelArg[ iArgIdx ],
                               CL_TRUE,
                               origin,
                               region,
                               NULL,
                               NULL,
                               pRGB32,
                               NULL,
                               NULL,
                               NULL );
}

cl_int objOpenCL::ExecuteKernel( const size_t global_x, const size_t global_y, double* pExtTime )
{
    if ( NULL == m_clKernel          ) return CL_INVALID_KERNEL;
    if ( NULL == m_clQueue           ) return CL_INVALID_COMMAND_QUEUE;
    if ( !m_stPreferWorkItemsInGroup ) return CL_INVALID_WORK_ITEM_SIZE;

    const size_t GLOBAL[ DIM_XY ] = { global_x, global_y };

    const size_t iWorkitem = ( int )sqrt( m_stPreferWorkItemsInGroup * 2.0 ); // 2-warp in work-item

    size_t WORK_ITEM[ DIM_XY ] = { iWorkitem, iWorkitem };

    if ( global_x % WORK_ITEM[ 0 ] ) WORK_ITEM[ 0 ] /= 2;
    if ( global_y % WORK_ITEM[ 1 ] ) WORK_ITEM[ 1 ] /= 2;

    while ( TRUE )
    {
        if ( GLOBAL[ 0 ] / WORK_ITEM[ 0 ] > m_maxWorkItemSize[ 0 ] ) WORK_ITEM[ 0 ] *= 2;
        
        else if ( GLOBAL[ 1 ] / WORK_ITEM[ 1 ] > m_maxWorkItemSize[ 1 ] ) WORK_ITEM[ 1 ] *= 2;

        else if ( WORK_ITEM[ 0 ] * WORK_ITEM[ 1 ] > m_maxWorkGroupSize ) return CL_INVALID_GLOBAL_WORK_SIZE;
        
        else break;
    }
    cl_event evt     = NULL;
    cl_int  ciErrNum = clEnqueueNDRangeKernel( m_clQueue, m_clKernel, DIM_XY, NULL, GLOBAL, WORK_ITEM, NULL, NULL, &evt );

    if ( CL_SUCCESS == ciErrNum )
    {
        ciErrNum = clWaitForEvents( 1, &evt );

        if ( pExtTime )
        {
            cl_ulong startTime = NULL;
            cl_ulong endTime   = NULL;

            ciErrNum = clGetEventProfilingInfo( evt, CL_PROFILING_COMMAND_START, sizeof( cl_ulong ), &startTime, NULL );
            ciErrNum = clGetEventProfilingInfo( evt, CL_PROFILING_COMMAND_END,   sizeof( cl_ulong ), &endTime,   NULL );

            *pExtTime = ( endTime - startTime ) / 1000000.0;
        }
    }
    if ( evt ) clReleaseEvent( evt );

    return ciErrNum;
}
