#include "CVideoDevicePool.h"
#include "utEtronVideoDeviceVertifyManager.h"
#include "dirent.h"
#include "EtronDI.h"
#include "debug.h"

CVideoDevicePool::~CVideoDevicePool()
{
    Clear();
}

CVideoDeviceGroup *CVideoDevicePool::GetGroup(unsigned int nIndex)
{
    if(nIndex >= m_Groups.size()) return nullptr;
    return m_Groups[nIndex];
}

CVideoDevice *CVideoDevicePool::GetDeivce(unsigned int nIndex)
{
    if(nIndex >= m_Devices.size()) return nullptr;
    return m_Devices[nIndex];
}

std::vector<CVideoDeviceGroup *> &CVideoDevicePool::GetGroups(int *nCount)
{
    if(nCount != nullptr) *nCount = m_Groups.size();
    return m_Groups;
}

std::vector<CVideoDevice*> &CVideoDevicePool::GetDevices(int *nCount)
{
    if(nCount != nullptr) *nCount = m_Devices.size();
    return m_Devices;
}

int CVideoDevicePool::Refresh(bool bDebugLog)
{
    m_bDebugLog = bDebugLog;
    CollectVideoDevice();
    GroupVideoDevice();
    return ETronDI_OK;
}

int CVideoDevicePool::Clear()
{
    for(CVideoDevice *pDevice : m_Devices){
        delete pDevice;
    }

    for(CVideoDeviceGroup *pGroup : m_Groups){
        delete pGroup;
    }

    m_Devices.clear();
    m_Groups.clear();
    return ETronDI_OK;
}

int CVideoDevicePool::CollectVideoDevice()
{
    m_Devices.clear();

#if 1
    DIR *dir = opendir("/sys/class/video4linux");
    if(!dir)
    {
        LOGE("Cannot access /sys/class/video4linux\n");
        return ETronDI_NoDevice;
    }

    while (dirent * entry = readdir(dir)){

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

        char buf[512] = {0};
        sprintf(buf, "/dev/%s", entry->d_name);

        CVideoDevice *pVideoDevice = new CVideoDevice(buf, m_bDebugLog);
        if (ETronDI_OK == VerifyDevice(pVideoDevice)){
            m_Devices.push_back(pVideoDevice);
        }else{
            delete pVideoDevice;
        }
    }

    closedir (dir);

    if (m_Devices.empty()) return ETronDI_NoDevice;
#else
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir("/dev/v4l/by-path/")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            char buf[512] = {0};
            sprintf(buf, "/dev/v4l/by-path/%s", ent->d_name);

            char *realPath = realpath(buf, nullptr);
            if(!realPath) continue;

            CVideoDevice *pVideoDevice = new CVideoDevice(realPath, m_bDebugLog);
            if (ETronDI_OK == VerifyDevice(pVideoDevice)){
                m_Devices.push_back(pVideoDevice);
            }else{
                delete pVideoDevice;
            }

            free(realPath);
        }
        closedir (dir);
    }else{
        int index = 0;
        char filePath[512] = {0};
        while(index < MAX_DEV_COUNT) {
            sprintf(filePath, "/dev/video%d", index++);
            CVideoDevice *pVideoDevice = new CVideoDevice(filePath, m_bDebugLog);
            if (ETronDI_OK == VerifyDevice(pVideoDevice)){
                m_Devices.push_back(pVideoDevice);
            }else{
                delete pVideoDevice;
            }
        }
    }
#endif
    return ETronDI_OK;
}

int CVideoDevicePool::VerifyDevice(CVideoDevice *pVideoDevice)
{
    if(pVideoDevice->m_fd == -1) return ETronDI_Init_Fail;

    unsigned int device_cap;
    pVideoDevice->GetDeviceCapability(&device_cap);
#if !defined(NDEBUG)
    unsigned short nChipID = 0;
    unsigned short nPid=0, nVid=0;
    pVideoDevice->GetHWRegister( CHIPID_ADDR, &nChipID, FG_Address_2Byte | FG_Value_1Byte);
    pVideoDevice->GetPidVid( &nPid, &nVid);
    LOGD("============================\n");
    LOGD("v4l device : %s \n", pVideoDevice->m_szDevName);
    LOGD("ChipID[0x%x] PID[0x%x] VID[0x%x] \n", nChipID, nPid, nVid);

    v4l2_capability cap;
    if(-1 == ioctl(pVideoDevice->m_fd, VIDIOC_QUERYCAP, &cap)) {
        LOGD("VIDIOC_QUERYCAP failed!! \n");
    }

    LOGD("driver : %s \n", cap.driver);
    LOGD("card : %s \n", cap.card);
    LOGD("bus_info : %s \n", cap.bus_info);
    LOGD ("Version: %u.%u.%u\n",
            (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF, cap.version & 0xFF);
    LOGD("capabilities : 0x%x \n", cap.capabilities);
    LOGD("device_caps : 0x%x \n", cap.device_caps);
    LOGD("============================\n");

    ETRONDI_STREAM_INFO steamInfo[64];
    memset(steamInfo, 0, sizeof(ETRONDI_STREAM_INFO)*64);
    pVideoDevice->GetDeviceResolutionList(64, steamInfo);
#endif
    if(!(device_cap & V4L2_CAP_VIDEO_CAPTURE)) return ETronDI_Init_Fail;

    return ETronDI_OK;
}

int CVideoDevicePool::GroupVideoDevice()
{
    std::vector<CVideoDevice *> anonymousDevices;
    utEtronVideoDeviceVertifyManager *pDeviceVertifyManager = utEtronVideoDeviceVertifyManager::GetInstance();

    for(CVideoDevice *pDevice : m_Devices){
        if(pDeviceVertifyManager->IsMasterDevice(pDevice)){
            m_Groups.push_back(CVideoDeviceGroupFactory::GenerateVideoGroupDevice(pDevice));
            continue;
        }
        anonymousDevices.push_back(pDevice);
    }

    for(CVideoDevice *pDevice : anonymousDevices){
        
        bool IsAnonymous = true;
        
        for(CVideoDeviceGroup *pGroup : m_Groups){
            if(pGroup->IsGroup(pDevice)){
                IsAnonymous = false;
                pGroup->AddDevice(pDevice);
                break;
            }
        }

        if (IsAnonymous) {
            m_Groups.push_back(CVideoDeviceGroupFactory::GenerateVideoGroupDevice(pDevice));
        }
    }

    PurgeGroup();
    return ETronDI_OK;
}

int CVideoDevicePool::PurgeGroup()
{
    std::vector<CVideoDeviceGroup *> purgeGroup;
    CVideoDeviceGroup_MultiBaseline *pMultiBaseDeviceGroup = nullptr;
    CVideoDeviceGroup_Grap *pGrapDeviceGroup = nullptr;
    for(CVideoDeviceGroup *pGroup : m_Groups){
        if(pGroup->IsValid()){
            purgeGroup.push_back(pGroup);
        }else{
            switch (pGroup->GetGroupType()){
                case VIDEO_DEVICE_GROUP_MULTIBASELINE:
                {
                    if (pMultiBaseDeviceGroup == nullptr)
                    {
                        pMultiBaseDeviceGroup = static_cast<CVideoDeviceGroup_MultiBaseline *>(pGroup);
                        continue;
                    }
                    else
                    {
                        if (pMultiBaseDeviceGroup->AddDevice(static_cast<CVideoDeviceGroup_MultiBaseline *>(pGroup)) == ETronDI_OK)
                        {
                            purgeGroup.push_back(pMultiBaseDeviceGroup);
                        }
                        else
                        {
                            delete pMultiBaseDeviceGroup;
                        }
                    }
                }
                break;
                case VIDEO_DEVICE_GROUP_GRAP:
                {
                    if (pGrapDeviceGroup == nullptr)
                    {
                        pGrapDeviceGroup = static_cast<CVideoDeviceGroup_Grap *>(pGroup);
                        continue;
                    }
                    else
                    {
                        if (pGrapDeviceGroup->AddDevice(static_cast<CVideoDeviceGroup_Grap *>(pGroup)) == ETronDI_OK)
                        {
                            purgeGroup.push_back(pGrapDeviceGroup);
                        }
                        else
                        {
                            delete pGrapDeviceGroup;
                        }
                    }
                }
                break;
                default: break;
            }

            delete pGroup;
        }
    }

    m_Groups.clear();
    m_Groups.assign(purgeGroup.begin(), purgeGroup.end());
    return ETronDI_OK;
}
