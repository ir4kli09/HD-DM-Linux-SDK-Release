/*! \file EtronDI.h
  	\brief The definition of CEtronDI class member function and data
  	Copyright:
	This file copyright (C) 2017 by

	eYs3D an Etron company

	An unpublished work.  All rights reserved.

	This file is proprietary information, and may not be disclosed or
	copied without the prior permission of eYs3D an Etron company.
 */
 
#ifndef LIB_ETRONDI_H
#define LIB_ETRONDI_H

#include "SwPostProc.h"
#include "eSPDI_def.h"

// #ifdef X86_CONSOLE
#define FIND_DEVICE2 //EthanWu-20200609: remove define for arm
// #endif

#if defined(FIND_DEVICE2)
class CVideoDeviceGroup;
#endif

#ifdef TINY_VERSION
#define MAX_DEV_COUNT 6
#else
#ifdef X86_CONSOLE
#define MAX_DEV_COUNT 200
#else
#define MAX_DEV_COUNT 20//200 //EthanWu-20200609: modify to 20
#endif
#endif

typedef struct tagAllDevInfo 
{
  int   nTotalDevNum;
  char  *device_list[MAX_DEV_COUNT];
  char  *device_list2[MAX_DEV_COUNT];
  unsigned short nPID[MAX_DEV_COUNT];
  unsigned short nVID[MAX_DEV_COUNT];
  unsigned short nChipID[MAX_DEV_COUNT];
  DEVICE_TYPE deviceTypeArray[MAX_DEV_COUNT];
  //+[Thermal device]
  bool grape_device[MAX_DEV_COUNT];
  //-[Thermal device]
} ALLDEVINFO;

class CEtronDI {
	
public:
	CEtronDI(void);
	~CEtronDI();
	int Init(bool bIsLogEnabled);
	int FindDevice();
	
	int GetDeviceNumber();
	int GetDeviceInfo( PDEVSELINFO pDevSelInfo, PDEVINFORMATION pdevinfo);
private:
    bool CheckFile(char *filename);
public:
	ALLDEVINFO m_Adi;
	bool m_bIsLogEnabled;
#if defined(FIND_DEVICE2)
public:
        std::vector<CVideoDeviceGroup *> &GetVideoGroup();
private:
        std::vector<CVideoDeviceGroup *> m_deviceGroups;
#endif
};

#endif // LIB_ETRONDI_H
