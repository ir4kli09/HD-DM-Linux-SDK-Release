#pragma once

#include "WatchDog_Client.h"
#include <memory>

class watchdogHelper
{
public:

    static watchdogHelper* instance()
    {
        static std::unique_ptr<watchdogHelper> pInstance;
        if (!pInstance)
        {
            pInstance = std::unique_ptr<watchdogHelper>(new watchdogHelper());
        }

        return pInstance.get();
    }

    static eYs3D::WatchDog_Client &watchdog()
    {
        return instance()->watchdogInstance;
    }

    friend std::unique_ptr<watchdogHelper>::deleter_type;

private :
    watchdogHelper() = default;
    ~watchdogHelper() = default;

private:
    eYs3D::WatchDog_Client watchdogInstance;
};
