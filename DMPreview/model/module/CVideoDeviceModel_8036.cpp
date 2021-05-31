#include "CVideoDeviceModel_8036.h"
#include "CEtronDeviceManager.h"
#include "CVideoDeviceController.h"

CVideoDeviceModel_8036::CVideoDeviceModel_8036(DEVSELINFO *pDeviceSelfInfo):
CVideoDeviceModel_8036_8052(pDeviceSelfInfo),
m_bIsInterleaveSupport(false)
{

}

int CVideoDeviceModel_8036::Init()
{
    int ret = CVideoDeviceModel::Init();

    if (ETronDI_OK != ret) return ret;

    unsigned short value;
    RETRY_ETRON_API(ret, EtronDI_GetFWRegister(CEtronDeviceManager::GetInstance()->GetEtronDI(),
                                               m_deviceSelInfo[0],
                                               0xe5, &value,
                                               FG_Address_1Byte | FG_Value_1Byte));

    m_bIsInterleaveSupport = (ETronDI_OK == ret) && (1 == value);

    return ret;
}

int CVideoDeviceModel_8036::GetRectifyLogData(int nDevIndex, int nRectifyLogIndex, eSPCtrl_RectLogData *pRectifyLogData, STREAM_TYPE depthType)
{
    std::vector<ETRONDI_STREAM_INFO> depthStreamInfo = GetStreamInfoList(STREAM_DEPTH);
    int nDepthIndex = m_pVideoDeviceController->GetPreviewOptions()->GetStreamIndex(STREAM_DEPTH);

    std::vector<ETRONDI_STREAM_INFO> colorStreamInfo = GetStreamInfoList(STREAM_COLOR);
    int nColorIndex = m_pVideoDeviceController->GetPreviewOptions()->GetStreamIndex(STREAM_COLOR);

    if (colorStreamInfo[nColorIndex].nHeight / depthStreamInfo[nDepthIndex].nHeight == 2){
        nRectifyLogIndex = 0;
    }

    return CVideoDeviceModel::GetRectifyLogData(nDevIndex, nRectifyLogIndex, pRectifyLogData, depthType);
}
