#pragma once

#include <unordered_map>
#include "eSPDI_def.h"
#include "CVideoDeviceGroup.h"

typedef struct {
    unsigned short nChipID;
    unsigned short nPid;
    unsigned short nVid;
} utEtronVideoDeviceInfo;

class CVideoDevice;
class utEtronVideoDeviceVertifyManager {
public:
    static utEtronVideoDeviceVertifyManager *GetInstance(){
        utEtronVideoDeviceVertifyManager *pInstance = nullptr;
        if(pInstance == nullptr){
            pInstance = new utEtronVideoDeviceVertifyManager();
        }
        return pInstance;
    }

    utEtronVideoDeviceInfo GetDeviceInfo(CVideoDevice *pDevice);

    bool IsEtronDevice(CVideoDevice *pDevice);
    //+[Thermal device]
    bool GetThermalPIDVID(CVideoDevice *pDevice,unsigned short *pPidBuf, unsigned short *pVidBuf);
    bool IsThermalDevice(CVideoDevice *pDevice);
    utEtronVideoDeviceInfo GetThermalDeviceInfo(CVideoDevice *pDevice);
    //-[Thermal device]
    bool IsColorWithDepthDevice(CVideoDevice *pDevice);
    bool IsDepth150mmDevice(CVideoDevice *pDevice);
    bool IsMasterDevice(CVideoDevice *pDevice);
    bool HasUsbHubDevice(CVideoDevice *pDevice);
    bool IsKolorDevice(CVideoDevice *pDevice);
    bool IsTrackDevice(CVideoDevice *pDevice);
    unsigned short GetMasterPID(CVideoDevice *pDevice);
    DEVICE_TYPE IdentifyDeviceType(CVideoDevice *pDevice);
    VIDEO_DEVICE_GROUP_TYPE GetDeviceGroupType(CVideoDevice *pDevice);

private:
    utEtronVideoDeviceVertifyManager(){ InitMap(); }
    ~utEtronVideoDeviceVertifyManager(){}

    void InitMap();

private:
    std::unordered_map<unsigned short ,unsigned short> m_SlaveDeviceToMasterDeviceMap;
    std::unordered_map<unsigned short ,DEVICE_TYPE> m_EtronDeviceChipIdTypeMap;
    std::unordered_map<unsigned short ,VIDEO_DEVICE_GROUP_TYPE> m_DeviceGroupTypeMap;
    std::unordered_map<unsigned short ,bool> m_ExcludeMasterDeviceMap;
    std::unordered_map<unsigned short ,bool> m_ColorWithDepthDeviceMap;
    std::unordered_map<unsigned short ,bool> m_Depth150mmDeviceMap;
    std::unordered_map<unsigned short ,bool> m_KolorDeviceMap;
    std::unordered_map<unsigned short ,bool> m_TrackDeviceMap;
    std::unordered_map<unsigned short ,bool> m_HubDeviceMap;
};
