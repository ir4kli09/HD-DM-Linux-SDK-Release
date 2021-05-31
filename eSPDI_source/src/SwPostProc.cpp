//#include "stdafx.h"
#include "SwPostProc.h"
#include "Post_SW.h"
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#define nullptr 0

map<string, int> CSwPostProc::keyToIndex = CSwPostProc::initKeyToIndexMap();

CSwPostProc::CSwPostProc()
    : m_procInstance(nullptr), m_depthBits(8)
{

}

CSwPostProc::~CSwPostProc()
{
    Uninit();
}

bool CSwPostProc::IsValid() const
{
    return (m_procInstance != nullptr);
}

bool CSwPostProc::Init(int depthBits)
{
    Uninit();

    m_procInstance = Post_Init();
    if (m_procInstance != nullptr)
    {
        Post_Reset(m_procInstance);
        m_depthBits = depthBits;
        char* configFileName = "../cfg/SWPP.cfg";
		loadParamFile(configFileName);
        return true;
    }
    
    return false;
}

void CSwPostProc::Uninit()
{
    if (m_procInstance != nullptr)
    {
        Post_Destroy(m_procInstance);
        m_procInstance = nullptr;
    }
}

bool CSwPostProc::GetParam(int index, int& value) const
{
    if (IsValid())
    {
        value = Post_GetParam(m_procInstance, index);
        return true;
    }

    return false;
}

bool CSwPostProc::SetParam(int index, int value)
{
    if (IsValid())
    {
        Post_SetParam(m_procInstance, index, value);
        return true;
    }

    return false;
}

bool CSwPostProc::Process(unsigned char* colorBuf, bool isColorRgb24, unsigned char* depthBuf, unsigned char* outputBuf, int width, int height)
{
    if (IsValid())
    {
        Post_SetColorFmt(m_procInstance, isColorRgb24 ? 1 : 0, 0, isColorRgb24 ? 1 : 0, isColorRgb24 ? width * 3 : width * 2);

        unsigned char* inDepthBuf = depthBuf;
        unsigned char* outDepthBuf = outputBuf;
        if (m_depthBits > 8)
        {
            // post process only support 8 bits now, so pass the higher 8 bits data as input to do post process
            // and keep the lower (n - 8) bits data
            if (m_postProcTempInBuf.size() != (unsigned int)width * height)
            {
                m_postProcTempInBuf.resize(width * height);
            }

            char andOperand = 0;
            for (int i = 0; i < 16 - m_depthBits; ++i)
            {
                andOperand |= (1 << i);
            }
            inDepthBuf = &m_postProcTempInBuf[0];
            for (int i = 0, size = width * height * 2; i < size; i += 2)
            {
                outputBuf[i] = (depthBuf[i] & andOperand);
                inDepthBuf[i / 2] = (unsigned char)((*((unsigned short*)&depthBuf[i])) >> (m_depthBits - 8));
            }

            if (m_postProcTempOutBuf.size() != (unsigned int)width * height)
            {
                m_postProcTempOutBuf.resize(width * height);
            }
            outDepthBuf = &m_postProcTempOutBuf[0];
        }

        bool ret = (Post_Process(m_procInstance, colorBuf, inDepthBuf, outDepthBuf, width, height) != 0);

        if (ret && m_depthBits > 8)
        {
            for (int i = 0, size = width * height; i < size; ++i)
            {
                outputBuf[i * 2] |= (outDepthBuf[i] << (m_depthBits - 8));
                outputBuf[i * 2 + 1] = (outDepthBuf[i] >> (16 - m_depthBits));
            }
        }

        return ret;
    }

    return false;
}

bool CSwPostProc::loadParamFile(char * configFileName)
{	
	std::ifstream file(configFileName, std::ios::in);
	if (file.is_open())
	{
		string line;
        printf("Load ... SWPP.cfg\n");
		while (getline(file, line))
		{
			std::stringstream ss(line);       // Insert the string into a stream			

			string key;
			string valueString;
			int value;
			ss >> key;
			ss >> valueString;
            value = stoi(valueString);
			if (keyToIndex.find(key) == keyToIndex.end()) {
				printf("Inavlid param: [%s] \n", key.c_str());
			}
			else {
				SetParam(keyToIndex[key], value);				
			}
			
		}
		file.close();
		return true;
	}
	else {
        printf("Without SWPP.cfg\n");
		return false;
	}
	
}
