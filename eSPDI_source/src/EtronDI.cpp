/* EtronDI.cpp
  	\brief CEtronDI class member functions
  	Copyright:
	This file copyright (C) 2017 by

	eYs3D an Etron company

	An unpublished work.  All rights reserved.

	This file is proprietary information, and may not be disclosed or
	copied without the prior permission of eYs3D an Etron company.
 */
 
#include "EtronDI.h"
#include "videodevice.h"
#include "debug.h"
#if defined(FIND_DEVICE2)
#include "CVideoDevicePool.h"
#include "utEtronVideoDeviceVertifyManager.h"
#endif

using namespace std;
extern bool g_bIsLogEnabled;
extern int kernel_index;

CEtronDI::CEtronDI() {
	
	m_bIsLogEnabled = false;
	for(int i=0; i<MAX_DEV_COUNT; i++) m_Adi.device_list[i] = NULL;
	for(int i=0; i<MAX_DEV_COUNT; i++) m_Adi.device_list2[i] = NULL;
}

CEtronDI::~CEtronDI() {

	int i = 0;
	
	for(i = 0;i < MAX_DEV_COUNT; i++) {
		if(m_Adi.device_list[i] != NULL) {
			free(m_Adi.device_list[i]);
			m_Adi.device_list[i] = NULL;
		}
	}
	
	for(i = 0;i < MAX_DEV_COUNT; i++) {
		if(m_Adi.device_list2[i] != NULL) {
			free(m_Adi.device_list2[i]);
			m_Adi.device_list2[i] = NULL;
		}
	}
}

int CEtronDI::Init(bool bIsLogEnabled) {
	
	m_bIsLogEnabled = bIsLogEnabled;
	return ETronDI_OK;
}

bool CEtronDI::CheckFile(char *filename)
{
    FILE* fp = fopen(filename, "r");
    if (fp) {
        // file exists
        fclose(fp);
        return true;
    }

    return false;
}


bool get_video_cap(char *devName)
{
    struct v4l2_capability cap;

    memset(&cap, 0, sizeof(cap));
    int fd=open("/dev/video0", O_RDWR);

    ioctl(fd,VIDIOC_QUERYCAP,&cap);
    close(fd);
    printf("Driver Name:%s\n Card Name:%s\n Cap:%d\n Bus info:%s\n Driver Version:%u.%u.%u\n", cap.driver, cap.card, cap.device_caps, cap.bus_info, (cap.version >> 16) & 0XFF, (cap.version >> 8) & 0XFF, cap.version&0XFF);

    if (strcmp((char *)cap.driver, "uvcvideo") == 0) {
        return true;
    }

    return false;
}

int CEtronDI::FindDevice() {
#if defined(FIND_DEVICE2)
    CVideoDevicePool::GetInstance()->Refresh(m_bIsLogEnabled);

    m_deviceGroups = CVideoDevicePool::GetInstance()->GetGroups();

    CLEAR(m_Adi);
    utEtronVideoDeviceVertifyManager *pDeviceVertifyMagager = utEtronVideoDeviceVertifyManager::GetInstance();
    for(CVideoDeviceGroup *group : m_deviceGroups){
        CVideoDevice *pMaster = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_MASTER);
        utEtronVideoDeviceInfo deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pMaster);

        m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
        m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
        m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
        m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pMaster);
        m_Adi.device_list[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pMaster->m_szDevName) + 1);
        strcpy(m_Adi.device_list[m_Adi.nTotalDevNum] ,pMaster->m_szDevName);

        switch(group->GetGroupType()){
            case VIDEO_DEVICE_GROUP_STANDARD:
                {
                    CVideoDevice *pSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH);
                    m_Adi.device_list2[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pSlave->m_szDevName) + 1);
                    strcpy( m_Adi.device_list2[m_Adi.nTotalDevNum] ,pSlave->m_szDevName);
                }
                break;
            case VIDEO_DEVICE_GROUP_MULTIBASELINE:
                {
                    CVideoDevice *pSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH_60mm);
                    m_Adi.device_list2[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pSlave->m_szDevName) + 1);
                    strcpy( m_Adi.device_list2[m_Adi.nTotalDevNum] ,pSlave->m_szDevName);

                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDevice150mm = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_MASTER_150mm);
                    utEtronVideoDeviceInfo deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDevice150mm);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDevice150mm);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pDevice150mm->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum] ,pDevice150mm->m_szDevName);

                    CVideoDevice *pDevice150mmSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH_150mm);
                    m_Adi.device_list2[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pDevice150mmSlave->m_szDevName) + 1);
                    strcpy( m_Adi.device_list2[m_Adi.nTotalDevNum] ,pDevice150mmSlave->m_szDevName);
                }
                break;
            case VIDEO_DEVICE_GROUP_KOLOR:
                {
                    CVideoDevice *pSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH);
                    m_Adi.device_list2[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pSlave->m_szDevName) + 1);
                    strcpy( m_Adi.device_list2[m_Adi.nTotalDevNum] ,pSlave->m_szDevName);

                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDeviceKolor = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_KOLOR);
                    utEtronVideoDeviceInfo deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDeviceKolor);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDeviceKolor);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pDeviceKolor->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum] ,pDeviceKolor->m_szDevName);
                }
                break;
            case VIDEO_DEVICE_GROUP_KOLOR_TRACK:
                {
                    CVideoDevice *pSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_DEPTH);
                    m_Adi.device_list2[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pSlave->m_szDevName) + 1);
                    strcpy( m_Adi.device_list2[m_Adi.nTotalDevNum] ,pSlave->m_szDevName);

                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDeviceKolor = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_KOLOR);
                    utEtronVideoDeviceInfo deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDeviceKolor);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDeviceKolor);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pDeviceKolor->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum] ,pDeviceKolor->m_szDevName);

                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDeviceTrack = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TRACK);
                    deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDeviceTrack);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDeviceTrack);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char*)malloc(strlen(pDeviceTrack->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum] ,pDeviceTrack->m_szDevName);
                }
                break;
            case VIDEO_DEVICE_GROUP_GRAP:
                {
                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDeviceKolor = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_KOLOR);
                    utEtronVideoDeviceInfo deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDeviceKolor);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDeviceKolor);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char *)malloc(strlen(pDeviceKolor->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum], pDeviceKolor->m_szDevName);

                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDeviceColorSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_COLOR_SLAVE);
                    deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDeviceColorSlave);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDeviceColorSlave);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char *)malloc(strlen(pDeviceColorSlave->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum], pDeviceColorSlave->m_szDevName);

                    ++m_Adi.nTotalDevNum;

                    CVideoDevice *pDeviceKolorSlave = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_KOLOR_SLAVE);
                    deviceInfo = pDeviceVertifyMagager->GetDeviceInfo(pDeviceKolorSlave);

                    m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                    m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                    m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                    m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = pDeviceVertifyMagager->IdentifyDeviceType(pDeviceKolorSlave);
                    m_Adi.device_list[m_Adi.nTotalDevNum] = (char *)malloc(strlen(pDeviceKolorSlave->m_szDevName) + 1);
                    strcpy(m_Adi.device_list[m_Adi.nTotalDevNum], pDeviceKolorSlave->m_szDevName);

                    //+[Thermal device]
                    int device_count;
                    CVideoDevicePool::GetInstance()->GetDevices(&device_count);
                    if(device_count == 7){
                        ++m_Adi.nTotalDevNum;
                        CVideoDevice *pDeviceThermal = group->GetDevice(VIDEO_DEVICE_GROUP_DEVICE_THERMAL);
                        deviceInfo = pDeviceVertifyMagager->GetThermalDeviceInfo(pDeviceThermal);

                        m_Adi.nChipID[m_Adi.nTotalDevNum] = deviceInfo.nChipID;
                        m_Adi.nPID[m_Adi.nTotalDevNum] = deviceInfo.nPid;
                        m_Adi.nVID[m_Adi.nTotalDevNum] = deviceInfo.nVid;
                        m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = PUMA;
                        m_Adi.grape_device[m_Adi.nTotalDevNum] = true;
                        m_Adi.device_list[m_Adi.nTotalDevNum] = (char *)malloc(strlen(pDeviceThermal->m_szDevName) + 1);
                        strcpy(m_Adi.device_list[m_Adi.nTotalDevNum], pDeviceThermal->m_szDevName);
                    }
                    //-[Thermal device]
                }
                break;
            case VIDEO_DEVICE_GROUP_ANONYMOUS:
                break;
            default:
                break;
        }

        ++m_Adi.nTotalDevNum;
    }

    CVideoDevicePool::GetInstance()->Clear();

    if(m_Adi.nTotalDevNum == 0) return ETronDI_NoDevice;
    return ETronDI_OK;
#else	
	
	int index = 0, skip = 0, i;
    char szDevName_master[256];
    char szDevName_slave[256];
    bool bisEtronDevice = true;

	CLEAR(m_Adi);
	while(index < MAX_DEV_COUNT) {
		
        sprintf(szDevName_master, "/dev/video%d", index);
        //printf("First Device ===================>\n");
        CVideoDevice *p_vd = new CVideoDevice(szDevName_master, m_bIsLogEnabled);
				
		if( -1 != p_vd->m_fd ) {
			
            unsigned short nChipID = 0;
            unsigned short nPid=0, nVid=0;


			p_vd->GetHWRegister( CHIPID_ADDR, &nChipID, FG_Address_2Byte | FG_Value_1Byte);
            p_vd->GetPidVid( &nPid, &nVid);
            bisEtronDevice = true;
            // Check device type by ChipID
            printf("nChipID = %02x\n", nChipID);
            switch(nChipID){
                // AXES1
                case 0x18:
                case 0x19:
                case 0x1A:
                case 0x1B:  m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = AXES1;
                            break;
                //PUMA
                case 0x12:
                case 0x14:
                case 0x15:  m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = PUMA;
                            break;
                //KIWI
                case 0x1C:  m_Adi.deviceTypeArray[m_Adi.nTotalDevNum] = KIWI;
                            break;
                default:    if(nVid != 0x1E4E) bisEtronDevice = false;
                            break;
            }

            if( bisEtronDevice ) {
                //printf("==========================================================================================> 1\n");
                //printf("=========> %s, index = %d\n", szDevName_master, index);
                //get_video_cap(szDevName);
                bool ret;
                if (kernel_index == 2) {
                    if (skip == index)
                        skip++;

                    while (CheckFile(szDevName_slave) == false) {
                        skip++;
                        if (skip == index)
                            skip++;

                        if (skip > MAX_DEV_COUNT) return ETronDI_Init_Fail;

                        sprintf(szDevName_slave, "/dev/video%d", skip);

                    }

                    //} while (CheckFile(szDevName_slave) == false || get_video_cap(szDevName_slave) == false);

                } else {
                    if (skip == index)
                        skip++;
                    sprintf(szDevName_slave, "/dev/video%d", skip);
                    //while (CheckFile(szDevName_slave) == false || get_video_cap(szDevName_slave) == false) {
                    while (CheckFile(szDevName_slave) == false) {
                        skip++;

                        if (skip > MAX_DEV_COUNT) return ETronDI_Init_Fail;

                        sprintf(szDevName_slave, "/dev/video%d", skip);
                    }
                }

				m_Adi.nChipID[m_Adi.nTotalDevNum] = nChipID;
                m_Adi.nPID[m_Adi.nTotalDevNum] = nPid;
                m_Adi.nVID[m_Adi.nTotalDevNum] = nVid;
                m_Adi.device_list[m_Adi.nTotalDevNum] = (char*)malloc(strlen(szDevName_master));
                strcpy(m_Adi.device_list[m_Adi.nTotalDevNum] ,szDevName_master);
                if (g_bIsLogEnabled == true)
                    printf("m_Adi.device_list[%d] = %s\n", m_Adi.nTotalDevNum, m_Adi.device_list[m_Adi.nTotalDevNum]);
#ifndef __WEYE__
                char szBusInfo1[64];
                char szBusInfo2[64];

                p_vd->GetV4l2BusInfo(szBusInfo1);
                if (kernel_index == 2) {
                    do {
                        if (index > MAX_DEV_COUNT) return ETronDI_Init_Fail;

                        sprintf(szDevName_slave, "/dev/video%d", index + 1);
                        index++;
                    } while (CheckFile(szDevName_slave) == false);
                }

                int j = index + 1;

				while(j < MAX_DEV_COUNT) {
                    //printf("==========================================================================================> 2\n");
                    sprintf(szDevName_slave, "/dev/video%d", j);
                    if ( (CheckFile(szDevName_slave) == false)) {
                        j++;
                        continue;
                    }

                    CVideoDevice *p_vd_slave = new CVideoDevice(szDevName_slave, m_bIsLogEnabled);

                    if(m_bIsLogEnabled)
                        LOGI("GetV4l2BusInfo +\n");

                    ret = p_vd_slave->GetV4l2BusInfo(szBusInfo2);
                    if (ret != 0) {
                        j++;
                        continue;
                    } 

                    sprintf(szDevName_slave, "/dev/video%d", j);

                    if(m_bIsLogEnabled) LOGI("GetV4l2BusInfo -\n");
					delete p_vd_slave;
					p_vd_slave = NULL;
					
                    if( 0 == strcmp(szBusInfo1, szBusInfo2)) {
                        m_Adi.device_list2[m_Adi.nTotalDevNum] = (char*)malloc(strlen(szDevName_slave));
                        strcpy( m_Adi.device_list2[m_Adi.nTotalDevNum] ,szDevName_slave);
                        if (g_bIsLogEnabled == true)
                            printf("m_Adi.device_list2[%d] = %s\n", m_Adi.nTotalDevNum, m_Adi.device_list2[m_Adi.nTotalDevNum]);
						break;
					}
                    j++;
				}
#endif
				
				//todo: sean: check the total dev num machnism (vs windows)
				m_Adi.nTotalDevNum++;
            }
        }
		
		delete p_vd;
		p_vd = NULL;
#ifdef __WEYE__
		if( bisEtronDevice ) break;
#endif
		index++;
	}
	
	if(m_Adi.nTotalDevNum == 0) return ETronDI_NoDevice;
	
	return ETronDI_OK;
#endif
}

int CEtronDI::GetDeviceNumber() {
	return m_Adi.nTotalDevNum;
}

int CEtronDI::GetDeviceInfo( PDEVSELINFO pDevSelInfo, PDEVINFORMATION pdevinfo) {
	
	pdevinfo->wPID       = m_Adi.nPID[pDevSelInfo->index];
	pdevinfo->wVID       = m_Adi.nVID[pDevSelInfo->index];
	pdevinfo->nChipID    = m_Adi.nChipID[pDevSelInfo->index];
	pdevinfo->strDevName = (char*)malloc(strlen(m_Adi.device_list[pDevSelInfo->index]));
	strcpy(pdevinfo->strDevName,m_Adi.device_list[pDevSelInfo->index]);	
    pdevinfo->nDevType   = m_Adi.deviceTypeArray[pDevSelInfo->index];
	
	return ETronDI_OK;
}

#if defined(FIND_DEVICE2)
std::vector<CVideoDeviceGroup *> &CEtronDI::GetVideoGroup()
{
    return m_deviceGroups;
}
#endif
