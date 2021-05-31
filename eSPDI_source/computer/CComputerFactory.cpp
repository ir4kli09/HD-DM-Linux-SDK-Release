#include "CComputerFactory.h"
#include "objOpenCL.h"
#include <memory>
#include "CComputer_Default.h"
#include "CComputer_OpenCL.h"
#if defined(CUDA_SUPPORT)
#include "CComputer_Cuda.h"
#endif
#include "debug.h"

CComputerFactory::CComputerFactory():
m_computerType(CComputer::DEFAULT)
{
#ifndef UNITY_DEBUG_LOG
    std::shared_ptr<objOpenCL> openCLObject(new objOpenCL());
    openCLObject->Initial();
    if (openCLObject->IsSupportOpenCL()){
        m_computerType = CComputer::OPENCL;
        LOGI("OpenCL Computing\n");
        return;
    }
#endif

#if defined(CUDA_SUPPORT)
    if (CComputer_Cuda::CudaSupport())
    {
        m_computerType = CComputer::CUDA;
        LOGI("CUDA Computing\n");
        return;;
    }
#endif

    LOGI("Default Computing\n");
}

CComputer *CComputerFactory::GenerateComputer()
{
    CComputer *pComputer = nullptr;

    switch (m_computerType) {
        case CComputer::OPENCL :
            pComputer = new CComputer_OpenCL();
            break;
#if defined(CUDA_SUPPORT)
        case CComputer::CUDA:
            pComputer = new CComputer_Cuda();
            break;
#endif
        case CComputer::DEFAULT:
        default:
            pComputer = new CComputer_Default();
            break;
    }

    return pComputer;
}