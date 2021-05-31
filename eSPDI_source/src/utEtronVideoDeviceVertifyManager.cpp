#include "utEtronVideoDeviceVertifyManager.h"
#include "eSPDI_def.h"
#include "videodevice.h"
#include <vector>
//+[Thermal device]
#include <iostream> 
#include <fstream> 
//-[Thermal device]

void utEtronVideoDeviceVertifyManager::InitMap()
{
    auto AddListToMap = [](std::vector<unsigned short> PIDs, std::unordered_map<unsigned short ,bool> &map)
    {
        for (unsigned short nPID : PIDs){
            map[nPID] = true;
        }
    };

    AddListToMap({
                     ETronDI_PID_8040S_K,
                     ETronDI_PID_8054_K,
                     ETronDI_PID_8060_K,
                     ETronDI_PID_8060_T,
                     ETronDI_PID_GRAP_K,
                     ETronDI_PID_GRAP_SLAVE_K
                     //+[Thermal device]
                    ,ETronDI_PID_GRAP_THERMAL
                    ,ETronDI_PID_GRAP_THERMAL2
                    //-[Thermal device]
                    ,ETronDI_VID_2170
                 }, m_ExcludeMasterDeviceMap);

    AddListToMap({
                     ETronDI_PID_8040S,
                     ETronDI_PID_8038,
                     ETronDI_PID_8038_M0,
                     ETronDI_PID_8054
                 }, m_ColorWithDepthDeviceMap);

    AddListToMap({
                     ETronDI_PID_8038_M1,
                 }, m_Depth150mmDeviceMap);

    AddListToMap({
                     ETronDI_PID_8040S_K,
                     ETronDI_PID_8054_K,
                     ETronDI_PID_8060_K,
                     ETronDI_PID_GRAP_K,
                     ETronDI_PID_GRAP_SLAVE_K
                 }, m_KolorDeviceMap);

    AddListToMap({
                     ETronDI_PID_8060_T
                 }, m_TrackDeviceMap);

    AddListToMap({
                     ETronDI_PID_8040S,
                     ETronDI_PID_8040S_K,
                     ETronDI_PID_8054,
                     ETronDI_PID_8054_K
                 }, m_HubDeviceMap);

    auto AddSlaveDeviceToMasterDeviceMap = [&](std::vector<unsigned short> PIDs, unsigned short nMasterPID){
        for (unsigned short nPID : PIDs){
            m_SlaveDeviceToMasterDeviceMap[nPID] = nMasterPID;
        }
    };

    AddSlaveDeviceToMasterDeviceMap({
                                        ETronDI_PID_8038,
                                        ETronDI_PID_8038_M0,
                                        ETronDI_PID_8038_M1
                                    }, ETronDI_PID_8038);

    AddSlaveDeviceToMasterDeviceMap({
                                        ETronDI_PID_8040S,
                                        ETronDI_PID_8040S_K
                                    }, ETronDI_PID_8040S);

    AddSlaveDeviceToMasterDeviceMap({
                                        ETronDI_PID_8054,
                                        ETronDI_PID_8054_K
                                    }, ETronDI_PID_8054);

    AddSlaveDeviceToMasterDeviceMap({
                                        ETronDI_PID_8060,
                                        ETronDI_PID_8060_K,
                                        ETronDI_PID_8060_T
                                    }, ETronDI_PID_8060);

    AddSlaveDeviceToMasterDeviceMap({
                                        ETronDI_PID_GRAP,
                                        ETronDI_PID_GRAP_K,
                                        ETronDI_PID_GRAP_SLAVE,
                                        ETronDI_PID_GRAP_SLAVE_K
                                        //+[Thermal device]
                                        ,ETronDI_PID_GRAP_THERMAL
                                        ,ETronDI_PID_GRAP_THERMAL2
                                        //-[Thermal device]
                                     }, ETronDI_PID_GRAP);

    auto AddDeviceChipIdTypeMap = [&](std::vector<unsigned short> ChipIDs, DEVICE_TYPE type){
        for (unsigned short nChipID : ChipIDs){
            m_EtronDeviceChipIdTypeMap[nChipID] = type;
        }
    };

    AddDeviceChipIdTypeMap({
                                0x18,
                                0x19,
                                0x1A,
                                0x1B
                            }, AXES1);

    AddDeviceChipIdTypeMap({
                                0x12,
                                0x14,
                                0x15
                            }, PUMA);

    AddDeviceChipIdTypeMap({
                                0x1C
                            }, KIWI);

    auto AddDeviceGroupTypeMap = [&](std::vector<unsigned short> PIDs, VIDEO_DEVICE_GROUP_TYPE type){
        for (unsigned short nPID : PIDs){
            m_DeviceGroupTypeMap[nPID] = type;
        }
    };

    AddDeviceGroupTypeMap({
                              ETronDI_PID_8029,
                              ETronDI_PID_8036,
                              ETronDI_PID_8037,
                              ETronDI_PID_8052,
                              ETronDI_PID_8053,
                              ETronDI_PID_8059
                          }, VIDEO_DEVICE_GROUP_STANDARD);

    AddDeviceGroupTypeMap({
                              ETronDI_PID_8040S,
                              ETronDI_PID_8040S_K,
                              ETronDI_PID_8054,
                              ETronDI_PID_8054_K
                          }, VIDEO_DEVICE_GROUP_KOLOR);

    AddDeviceGroupTypeMap({
                              ETronDI_PID_8038,
                              ETronDI_PID_8038_M0,
                              ETronDI_PID_8038_M1
                          }, VIDEO_DEVICE_GROUP_MULTIBASELINE);

    AddDeviceGroupTypeMap({
                              ETronDI_PID_8060,
                              ETronDI_PID_8060_K,
                              ETronDI_PID_8060_T
                          }, VIDEO_DEVICE_GROUP_KOLOR_TRACK);

    AddDeviceGroupTypeMap({
                              ETronDI_PID_GRAP,
                              ETronDI_PID_GRAP_K,
                              ETronDI_PID_GRAP_SLAVE,
                              ETronDI_PID_GRAP_SLAVE_K
                              //+[Thermal device]
                              ,ETronDI_PID_GRAP_THERMAL
                              ,ETronDI_PID_GRAP_THERMAL2
                              //-[Thermal device]
                          }, VIDEO_DEVICE_GROUP_GRAP);

}

utEtronVideoDeviceInfo utEtronVideoDeviceVertifyManager::GetDeviceInfo(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info;
    memset(&info, 0, sizeof(utEtronVideoDeviceInfo));
    if(pDevice == nullptr) return info;

    pDevice->GetHWRegister( CHIPID_ADDR, &info.nChipID, FG_Address_2Byte | FG_Value_1Byte);
    pDevice->GetPidVid( &info.nPid, &info.nVid);

    return info;
}

bool utEtronVideoDeviceVertifyManager::IsEtronDevice(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info = GetDeviceInfo(pDevice);

    if(info.nVid == 0x1E4E) return true;

    if(m_EtronDeviceChipIdTypeMap.count(info.nChipID) > 0) return true;

    return false;
}

bool utEtronVideoDeviceVertifyManager::IsMasterDevice(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info = GetDeviceInfo(pDevice);
    if (!info.nPid || !info.nVid) return false;

    if (m_ExcludeMasterDeviceMap.count(info.nPid)) return false;

    if (0 != pDevice->GetDeviceIndex()) return false;

    return true;
}

//+[Thermal device]
utEtronVideoDeviceInfo utEtronVideoDeviceVertifyManager::GetThermalDeviceInfo(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info;
    memset(&info, 0, sizeof(utEtronVideoDeviceInfo));
    if(pDevice == nullptr) return info;
    info.nChipID =21 ;
    GetThermalPIDVID(pDevice, &info.nPid, &info.nVid);

    return info;
}

bool utEtronVideoDeviceVertifyManager::GetThermalPIDVID(CVideoDevice *pDevice,unsigned short *pPidBuf, unsigned short *pVidBuf)
{
        unsigned int vid_value = 0, pid_value = 0;

        std::string str = pDevice->m_szDevName;
        auto pos = str.find_last_of("/");
        if (pos == std::string::npos) {
            printf("Fail to get vid and pid\n");
        }
        std::string name = str.substr(pos+1);
        std:: string modalias;
        if (!(std::ifstream("/sys/class/video4linux/" + name + "/device/modalias") >> modalias))
            printf("Fail to read modalias\n");
        if (modalias.size() < 14 || modalias.substr(0,5) != "usb:v" || modalias[9] != 'p')
            printf("Not a usb format modalias\n");
        if (!(std::istringstream(modalias.substr(5,4)) >> std::hex >> vid_value))
           printf("Fail to read vid\n");
        if (!(std::istringstream(modalias.substr(10,4)) >> std::hex >> pid_value))
           printf("Fail to read pid\n");
        *pPidBuf = pid_value;
        *pVidBuf = vid_value;

        if(ETronDI_VID_GRAP_THERMAL == vid_value 
            &&(ETronDI_PID_GRAP_THERMAL ==pid_value ||ETronDI_PID_GRAP_THERMAL2 ==pid_value)){
                    return true;
        }
        return false;
}

bool utEtronVideoDeviceVertifyManager::IsThermalDevice(CVideoDevice *pDevice)
{
        unsigned short vid_value = 0, pid_value = 0;
        if (GetThermalPIDVID(pDevice, &pid_value, &vid_value)) {
            printf("Found Thermal sensor \n");
                    return true;
        }
        return false;
}
//-[Thermal device]

bool utEtronVideoDeviceVertifyManager::IsColorWithDepthDevice(CVideoDevice *pDevice)
{
    return m_ColorWithDepthDeviceMap.count(GetDeviceInfo(pDevice).nPid) > 0;
}

bool utEtronVideoDeviceVertifyManager::IsDepth150mmDevice(CVideoDevice *pDevice)
{
    return m_Depth150mmDeviceMap.count(GetDeviceInfo(pDevice).nPid) > 0;
}

bool utEtronVideoDeviceVertifyManager::HasUsbHubDevice(CVideoDevice *pDevice)
{
    return m_HubDeviceMap.count(GetDeviceInfo(pDevice).nPid) > 0;
}

bool utEtronVideoDeviceVertifyManager::IsKolorDevice(CVideoDevice *pDevice)
{
    return m_KolorDeviceMap.count(GetDeviceInfo(pDevice).nPid) > 0;
}

bool utEtronVideoDeviceVertifyManager::IsTrackDevice(CVideoDevice *pDevice)
{
    return m_TrackDeviceMap.count(GetDeviceInfo(pDevice).nPid) > 0;
}

unsigned short utEtronVideoDeviceVertifyManager::GetMasterPID(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info = GetDeviceInfo(pDevice);
    if(m_SlaveDeviceToMasterDeviceMap.count(info.nPid) == 0) return 0;
    return m_SlaveDeviceToMasterDeviceMap[info.nPid];
}

DEVICE_TYPE utEtronVideoDeviceVertifyManager::IdentifyDeviceType(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info = GetDeviceInfo(pDevice);
    if(m_EtronDeviceChipIdTypeMap.count(info.nChipID) == 0) return OTHERS;
    return m_EtronDeviceChipIdTypeMap[info.nChipID];
}

VIDEO_DEVICE_GROUP_TYPE utEtronVideoDeviceVertifyManager::GetDeviceGroupType(CVideoDevice *pDevice)
{
    utEtronVideoDeviceInfo info = GetDeviceInfo(pDevice);
    if (!info.nPid || !info.nVid) return  VIDEO_DEVICE_GROUP_ANONYMOUS;
    if (ETronDI_VID_2170 == info.nPid) return VIDEO_DEVICE_GROUP_ANONYMOUS;

    if(m_DeviceGroupTypeMap.count(info.nPid) == 0) return VIDEO_DEVICE_GROUP_STANDARD;
    return m_DeviceGroupTypeMap[info.nPid];
}

