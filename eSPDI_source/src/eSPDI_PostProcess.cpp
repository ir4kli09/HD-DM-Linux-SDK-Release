#include "eSPDI.h"
#include "Post_Process_API.h"

int EtronDI_InitPostProcess(void **ppPostProcessHandle, unsigned int nWidth,
                            unsigned int nHeight, EtronDIImageType::Value imageType) {
    if (!ppPostProcessHandle) return ETronDI_NullPtr;

    auto GetDataBit = [=](int nDepthDataType) -> int {
        switch (imageType) {
            case EtronDIImageType::DEPTH_8BITS: return 8;
            case EtronDIImageType::DEPTH_11BITS: return 11;
            case EtronDIImageType::DEPTH_14BITS: return 14; 
            default: return 0;
        }
    };

    int depth_bits = GetDataBit(imageType);

    if (!nWidth || !nHeight || !depth_bits)
        return ETronDI_NotSupport;
        
    POST_PROCESS_API *pPostProcessAPI = new POST_PROCESS_API(nHeight, nWidth);
    switch(depth_bits)
    {
        case 11:
            pPostProcessAPI->depth_data_type = POST_PROCESS_API_DATA_TYPE_DEPTH_D; 
            break;
        case 14:
            pPostProcessAPI->depth_data_type = POST_PROCESS_API_DATA_TYPE_DEPTH_Z; 
            break;
    }       

    *ppPostProcessHandle = pPostProcessAPI;

    return ETronDI_OK;
}

int EtronDI_PostProcess(void *pPostProcessHandle, unsigned char *pDepthData){
    
    POST_PROCESS_API *pPostProcessAPI = static_cast<POST_PROCESS_API*>(pPostProcessHandle);

    if (!pPostProcessAPI) return ETronDI_NullPtr;

    pPostProcessAPI->p_src_data = pDepthData;
    pPostProcessAPI->p_dst_data = pDepthData;

    pPostProcessAPI->PostProcessing();

    return ETronDI_OK;
}

int EtronDI_ReleasePostProcess(void *pPostProcessHandle){
    
    POST_PROCESS_API *pPostProcessAPI = static_cast<POST_PROCESS_API *>(pPostProcessHandle);

    if (!pPostProcessAPI) return ETronDI_NullPtr;

    delete pPostProcessAPI;

    return ETronDI_OK;
}
