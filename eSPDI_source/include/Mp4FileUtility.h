/*! \file Mp4FileUtility.h
    \brief The definition of Mp4 File Utility
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

// 
// In order to use in Wcorder system, don't use c++11
// 

int MaxMp4ExtraDataSize();
bool AccessMp4FileExtraData(const char* filename, std::vector<char>& extraData, bool write);
