#pragma once

#include <vector>
#include "videodevice.h"
#include "CVideoDeviceGroup.h"
class CVideoDevicePool {

public:
    static CVideoDevicePool *GetInstance(){
        static CVideoDevicePool *pInstance = nullptr;
        if(pInstance == nullptr){
            pInstance = new CVideoDevicePool();
        }
        return pInstance;
    }

    CVideoDeviceGroup *GetGroup(unsigned int nIndex);
    CVideoDevice      *GetDeivce(unsigned int nIndex);

    std::vector<CVideoDeviceGroup *> &GetGroups(int *nCount = nullptr);
    std::vector<CVideoDevice*> &GetDevices(int *nCount = nullptr);

    int Refresh(bool bDebugLog = false);
    int Clear();

private:
    CVideoDevicePool() = default;
    ~CVideoDevicePool();

    int CollectVideoDevice();
    int VerifyDevice(CVideoDevice *pVideoDevice);
    int GroupVideoDevice();
    int PurgeGroup();

private:
    std::vector<CVideoDeviceGroup *> m_Groups;
    std::vector<CVideoDevice *> m_Devices;
    bool m_bDebugLog;
};
