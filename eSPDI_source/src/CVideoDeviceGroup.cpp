#include "CVideoDeviceGroup.h"
#include "videodevice.h"
#include "utEtronVideoDeviceVertifyManager.h"

// CVideoDeviceGroup +
bool CVideoDeviceGroup::IsGroup(CVideoDevice *pDevice)
{
    char pBusInfo[64];
    pDevice->GetV4l2BusInfo(pBusInfo);
    return !strcmp(m_pGrouID, pBusInfo);
}

int CVideoDeviceGroup::AddDevice(CVideoDevice *pDevice)
{
    if(!utEtronVideoDeviceVertifyManager::GetInstance()->IsMasterDevice(pDevice))
        return ETronDI_NoDevice;

    char pBusInfo[64];
    pDevice->GetV4l2BusInfo(pBusInfo);
    strcpy(m_pGrouID, pBusInfo);

    return ETronDI_OK;
}
// CVideoDeviceGroup -

// CVideoDeviceGroup_Standard +
int CVideoDeviceGroup_Standard::AddDevice(CVideoDevice *pDevice)
{
    CVideoDeviceGroup::AddDevice(pDevice);

    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsMasterDevice(pDevice)){
        m_pColorDevice = pDevice;
    }else{
        m_pDepthDevice = pDevice;
    }

    return ETronDI_OK;
}

CVideoDevice *CVideoDeviceGroup_Standard::GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType)
{
    switch(devType){
        case VIDEO_DEVICE_GROUP_DEVICE_MASTER:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR:
            return m_pColorDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH:
            return m_pDepthDevice;
        default:
            break;
    }

    return nullptr;
}

bool CVideoDeviceGroup_Standard::IsValid()
{
    return m_pColorDevice != nullptr &&
           m_pDepthDevice != nullptr;
}
// CVideoDeviceGroup_Standard -

// CVideoDeviceGroup_MultiBaseline +
int CVideoDeviceGroup_MultiBaseline::AddDevice(CVideoDevice *pDevice)
{
    CVideoDeviceGroup::AddDevice(pDevice);

    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsMasterDevice(pDevice)){
        if(utEtronVideoDeviceVertifyManager::GetInstance()->IsDepth150mmDevice(pDevice)){
            m_pMasterDevice_150mm = pDevice;
        }else{
            m_pColorWithDepthDevice = pDevice;
        }
    }else{
        if(m_pColorWithDepthDevice){
            m_pDepthDevice_60mm = pDevice;
        }else{
            m_pDepthDevice_150mm = pDevice;
        }
    }
    return ETronDI_OK;
}

int CVideoDeviceGroup_MultiBaseline::AddDevice(CVideoDeviceGroup_MultiBaseline *pGroup)
{
    if(!m_pColorWithDepthDevice){
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_COLOR_WITH_DEPTH);
        if(pDevice){
            m_pColorWithDepthDevice = pDevice;
        }
    }

    if(!m_pDepthDevice_60mm){
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH_60mm);
        if(pDevice){
            m_pDepthDevice_60mm = pDevice;
        }
    }

    if(!m_pMasterDevice_150mm){
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_MASTER_150mm);
        if(pDevice){
            m_pMasterDevice_150mm = pDevice;
        }
    }

    if(!m_pDepthDevice_150mm){
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH_150mm);
        if(pDevice){
            m_pDepthDevice_150mm = pDevice;
        }
    }

    if(!IsValid()) return ETronDI_NullPtr;

    return ETronDI_OK;
}

CVideoDevice *CVideoDeviceGroup_MultiBaseline::GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType)
{
    switch(devType){
        case VIDEO_DEVICE_GROUP_DEVICE_MASTER:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR:
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH:
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH_30mm:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR_WITH_DEPTH:
            return m_pColorWithDepthDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH_60mm:
            return m_pDepthDevice_60mm;
        case VIDEO_DEVICE_GROUP_DEVICE_MASTER_150mm:
            return m_pMasterDevice_150mm;
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH_150mm:
            return m_pDepthDevice_150mm;
        default:
            break;
    }

    return nullptr;
}

bool CVideoDeviceGroup_MultiBaseline::IsValid()
{
    return m_pColorWithDepthDevice != nullptr &&
           m_pDepthDevice_60mm != nullptr &&
           m_pMasterDevice_150mm != nullptr &&
           m_pDepthDevice_150mm != nullptr;
}
// CVideoDeviceGroup_MultiBaseline -

// CVideoDeviceGroup_Kolor +
bool CVideoDeviceGroup_Kolor::IsGroup(CVideoDevice *pDevice)
{
    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsKolorDevice(pDevice)){
        char pBusInfo[64] = {0};
        pDevice->GetV4l2BusInfo(pBusInfo);

        return !strncmp(m_pGrouID, pBusInfo, strlen(pBusInfo) - 2);
    }

    return CVideoDeviceGroup::IsGroup(pDevice);
}

int CVideoDeviceGroup_Kolor::AddDevice(CVideoDevice *pDevice)
{
    CVideoDeviceGroup::AddDevice(pDevice);

    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsMasterDevice(pDevice)){
        m_pColorWithDepthDevice = pDevice;
    }else if(utEtronVideoDeviceVertifyManager::GetInstance()->IsKolorDevice(pDevice)){
        m_pKolorDevice = pDevice;
    }else{
        m_pDepthDevice = pDevice;
    }

    return ETronDI_OK;
}

CVideoDevice *CVideoDeviceGroup_Kolor::GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType)
{
    switch(devType){
        case VIDEO_DEVICE_GROUP_DEVICE_MASTER:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR_WITH_DEPTH:
            return m_pColorWithDepthDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH:
            return m_pDepthDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_KOLOR:
            return m_pKolorDevice;
        default:
            break;
    }
    return nullptr;
}

bool CVideoDeviceGroup_Kolor::IsValid()
{
    return m_pColorWithDepthDevice != nullptr &&
           m_pDepthDevice != nullptr &&
           m_pKolorDevice != nullptr;
}
// CVideoDeviceGroup_Kolor -

// CVideoDeviceGroup_Kolor_Track +
bool CVideoDeviceGroup_Kolor_Track::IsGroup(CVideoDevice *pDevice)
{
    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsKolorDevice(pDevice) ||
       utEtronVideoDeviceVertifyManager::GetInstance()->IsTrackDevice(pDevice)){
        char pBusInfo[64];
        pDevice->GetV4l2BusInfo(pBusInfo);

        return !strncmp(m_pGrouID, pBusInfo, strlen(pBusInfo) - 2);
    }

    return CVideoDeviceGroup::IsGroup(pDevice);
}

int CVideoDeviceGroup_Kolor_Track::AddDevice(CVideoDevice *pDevice)
{
    CVideoDeviceGroup::AddDevice(pDevice);

    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsMasterDevice(pDevice)){
        m_pColorDevice = pDevice;
    }else if(utEtronVideoDeviceVertifyManager::GetInstance()->IsKolorDevice(pDevice)){
        m_pKolorDevice = pDevice;
    }else if(utEtronVideoDeviceVertifyManager::GetInstance()->IsTrackDevice(pDevice)){
        m_pTrackDevice = pDevice;
    }else{
        m_pDepthDevice = pDevice;
    }
    return ETronDI_OK;
}

CVideoDevice *CVideoDeviceGroup_Kolor_Track::GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType)
{
    switch(devType){
        case VIDEO_DEVICE_GROUP_DEVICE_MASTER:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR:
            return m_pColorDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_DEPTH:
            return m_pDepthDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_KOLOR:
            return m_pKolorDevice;
        case VIDEO_DEVICE_GROUP_DEVICE_TRACK:
            return m_pTrackDevice;
        default:
            break;
    }
    return nullptr;
}

bool CVideoDeviceGroup_Kolor_Track::IsValid()
{
    return m_pColorDevice != nullptr &&
           m_pDepthDevice != nullptr &&
           m_pKolorDevice != nullptr &&
           m_pTrackDevice != nullptr;
}
// CVideoDeviceGroup_Kolor_Track -

// CVideoDeviceGroup_Grap +
bool CVideoDeviceGroup_Grap::IsGroup(CVideoDevice *pDevice)
{
    char pBusInfo[64];
    pDevice->GetV4l2BusInfo(pBusInfo);

    return !strncmp(m_pGrouID, pBusInfo, strlen(pBusInfo) - 2);
}

int CVideoDeviceGroup_Grap::AddDevice(CVideoDevice *pDevice)
{
    if (GetGroupType() != utEtronVideoDeviceVertifyManager::GetInstance()->GetDeviceGroupType(pDevice))
    {
        return ETronDI_NoDevice;
    }

    CVideoDeviceGroup::AddDevice(pDevice);
    
    //+[Thermal device]
    if(utEtronVideoDeviceVertifyManager::GetInstance()->IsThermalDevice(pDevice))
    {
            m_pThermalDevice = pDevice;
            return ETronDI_OK;
     }
     //-[Thermal device]

    if (utEtronVideoDeviceVertifyManager::GetInstance()->IsMasterDevice(pDevice)){
        utEtronVideoDeviceInfo info = utEtronVideoDeviceVertifyManager::GetInstance()->GetDeviceInfo(pDevice);
        if (info.nPid == utEtronVideoDeviceVertifyManager::GetInstance()->GetMasterPID(pDevice))
        {
            m_pColorDevice_Master = pDevice;
        }
        else
        {
            m_pColorDevice_Slave = pDevice;
        }
    }else{
        if (m_pColorDevice_Master)
        {
            m_pKolorDevice_Master = pDevice;
        }
        else
        {
            m_pKolorDevice_Slave = pDevice;
        }
    }
    return ETronDI_OK;
}

int CVideoDeviceGroup_Grap::AddDevice(CVideoDeviceGroup_Grap *pGroup)
{
    if (!m_pColorDevice_Master)
    {
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_COLOR);
        if (pDevice)
        {
            m_pColorDevice_Master = pDevice;
        }
    }

    if (!m_pKolorDevice_Master)
    {
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_KOLOR);
        if (pDevice)
        {
            m_pKolorDevice_Master = pDevice;
        }
    }

    if (!m_pColorDevice_Slave)
    {
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_COLOR_SLAVE);
        if (pDevice)
        {
            m_pColorDevice_Slave = pDevice;
        }
    }

    if (!m_pKolorDevice_Slave)
    {
        CVideoDevice *pDevice = pGroup->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_KOLOR_SLAVE);
        if (pDevice)
        {
            m_pKolorDevice_Slave = pDevice;
        }
    }

    if (!IsValid())
        return ETronDI_NullPtr;

    return ETronDI_OK;
}

CVideoDevice *CVideoDeviceGroup_Grap::GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType)
{
    switch (devType)
    {
        case VIDEO_DEVICE_GROUP_DEVICE_MASTER:
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR:
            return m_pColorDevice_Master;
        case VIDEO_DEVICE_GROUP_DEVICE_KOLOR:
            return m_pKolorDevice_Master;
        case VIDEO_DEVICE_GROUP_DEVICE_COLOR_SLAVE:
            return m_pColorDevice_Slave;
        case VIDEO_DEVICE_GROUP_DEVICE_KOLOR_SLAVE:
            return m_pKolorDevice_Slave;
         //+[Thermal device]
         case VIDEO_DEVICE_GROUP_DEVICE_THERMAL:
            return m_pThermalDevice;
          //-[Thermal device]
        default:
            break;
    }
    return nullptr;
}

bool CVideoDeviceGroup_Grap::IsValid()
{
    return m_pColorDevice_Master != nullptr &&
           m_pKolorDevice_Master != nullptr &&
           m_pColorDevice_Slave != nullptr &&
           m_pKolorDevice_Slave != nullptr;
}
// CVideoDeviceGroup_Grap -

// CVideoDeviceGroupFactory +
CVideoDeviceGroup *CVideoDeviceGroupFactory::GenerateVideoGroupDevice(CVideoDevice *pDevice)
{
    CVideoDeviceGroup *pGroup;
    switch(utEtronVideoDeviceVertifyManager::GetInstance()->GetDeviceGroupType(pDevice)){
        case VIDEO_DEVICE_GROUP_STANDARD:
            pGroup = new CVideoDeviceGroup_Standard();
            break;
        case VIDEO_DEVICE_GROUP_MULTIBASELINE:
            pGroup = new CVideoDeviceGroup_MultiBaseline();
            break;
        case VIDEO_DEVICE_GROUP_KOLOR:
            pGroup = new CVideoDeviceGroup_Kolor();
            break;
        case VIDEO_DEVICE_GROUP_KOLOR_TRACK:
            pGroup = new CVideoDeviceGroup_Kolor_Track();
            break;
        case VIDEO_DEVICE_GROUP_GRAP:
            pGroup = new CVideoDeviceGroup_Grap();
            break;
        case VIDEO_DEVICE_GROUP_ANONYMOUS:
            pGroup = new CVideoDeviceGroup_Anonymous();
        default:
            break;
    };

    pGroup->AddDevice(pDevice);
    return pGroup;
}
// CVideoDeviceGroupFactory -
