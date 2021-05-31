/*! \file DataEncrypt.h
    \brief The definition of CDataEncrypt class member function and data
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#pragma once
#include <vector>
#include <string>


#ifndef BYTE
#define BYTE unsigned char
#endif

class CDataEncrypt
{
public:
    CDataEncrypt(BYTE key);
    virtual ~CDataEncrypt();

    bool Encrypt(const std::vector<BYTE>& input, std::vector<BYTE>& output);
    bool Decrypt(const std::vector<BYTE>& input, std::vector<BYTE>& output);

    bool Encrypt(const std::string& input, std::string& output);
    bool Decrypt(const std::string& input, std::string& output);
    
    bool Encrypt(const std::string& input1, const std::string& input2, std::string& output);
    bool Decrypt(const std::string& input, std::string& output1, std::string& output2);

private:
    BYTE m_key;
};
