#pragma once

#include "CComputer.h"

class CComputerFactory {

public:
    static CComputerFactory *GetInstance()
    {
        static CComputerFactory *pInstacne = nullptr;
        if (!pInstacne){
            pInstacne = new CComputerFactory();
        }
        return pInstacne;
    }

    CComputer *GenerateComputer();

private:
    CComputerFactory();
    ~CComputerFactory() = default;

private:

    CComputer::TYPE m_computerType;

};