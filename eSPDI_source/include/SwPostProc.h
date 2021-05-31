/*! \file SwPostProc.h
    \brief SW Post Process functions
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#pragma once
#include <vector>
#include <map>
#include <iostream>
#include <string>
using namespace std;
#define nullptr 0
class CSwPostProc
{
public:
    CSwPostProc();
    ~CSwPostProc();

    bool IsValid() const;
    bool Init(int depthBits);// 8~16 bits
    void Uninit();
    bool GetParam(int index, int& value) const;
    bool SetParam(int index, int value);

    bool Process(unsigned char* colorBuf, bool isColorRgb24, unsigned char* depthBuf, unsigned char* outputBuf, int width, int height);
	bool loadParamFile(char* configFileName);


    static std::map<string, int> keyToIndex;
private:
    void* m_procInstance;
    int m_depthBits;
    std::vector<unsigned char> m_postProcTempInBuf;
    std::vector<unsigned char> m_postProcTempOutBuf;

    static std::map<string, int> initKeyToIndexMap() {
		if (keyToIndex.size() > 0) {
			return keyToIndex;
		}
        std::map<string, int> map;
		map["VERSION_ID"] =		0 ;
		map["PAR_NUM"] =		1 ;
		map["GPU_EN"] =			2 ;
		map["RSVD3"] =			3 ;
		map["RSVD4"] =			4 ;
		map["HR_MODE"] =		5 ;
		map["HR_CURVE_0"] =		6 ;
		map["HR_CURVE_1"] =		7 ;
		map["HR_CURVE_2"] =		8 ;
		map["HR_CURVE_3"] =		9 ;
		map["HR_CURVE_4"] =		10;
		map["HR_CURVE_5"] =		11;
		map["HR_CURVE_6"] =		12;
		map["HR_CURVE_7"] =		13;
		map["HR_CURVE_8"] =		14;
		map["HR_CTRL"] =		15;
		map["RSVD16"] =			16;
		map["HF_MODE"] =		17;
		map["RSVD5"] =			18;
		map["RSVD6"] =			19;
		map["DC_MODE"] =		20;
		map["DC_CNT_THD"] =		21;
		map["DC_GRAD_THD"] =	22;
		map["SEG_MODE"] =		23;
		map["SEG_THD_SUB"] =	24;
		map["SEG_THD_SLP"] =	25;
		map["SEG_THD_MAX"] =	26;
		map["SEG_THD_MIN"] =	27;
		map["SEG_FILL_MODE"] =	28;
		map["RSVD29"] =			29;
		map["RSVD30"] =			30;
		map["HF2_MODE"] =		31;
		map["RSVD32"] =			32;
		map["RSVD33"] =			33;
		map["GRAD_MODE"] =		34;
		map["RSVD35"] =			35;
		map["RSVD36"] =			36;
		map["TEMP0_MODE"] =		37;
		map["TEMP0_THD"] =		38;
		map["RSVD39"] =			39;
		map["RSVD40"] =			40;
		map["TEMP1_MODE"] =		41;
		map["TEMP1_LEVEL"] =	42;
		map["TEMP1_THD"] =		43;
		map["RSVD44"] =			44;
		map["RSVD45"] =			45;
		map["FC_MODE"] =		46;
		map["FC_EDGE_THD"] =	47;
		map["FC_AREA_THD"] =	48;
		map["RSVD49"] =			49;
		map["RSVD50"] =			50;
		map["MF_MODE"] =		51;
		map["ZM_MODE"] =		52;
		map["RF_MODE"] =		53;
		map["RF_LEVEL"] =		54;
		map["RF_BLK_SIZE"] =	55;
		return map;
	}
};
