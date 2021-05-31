//#define __debug__
/*! \file debug.h
    \brief Debug  API functions
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */

#include <stdio.h>
#ifdef UNITY_DEBUG_LOG
#include "DebugCPP.hpp" // unity native callback to managed code.
#endif
#include "watchdogHelper.h"

#define LOG_TAG "eSPDI_API"

#ifdef __WEYE__
//Use Android ALOGx 
#include <cutils/log.h>
#else

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */
#ifndef UNITY
#define LOGW(...) fprintf(stderr, "W/" LOG_TAG ": " __VA_ARGS__); \
                  watchdogHelper::watchdog().dump_message("W/" LOG_TAG ": " __VA_ARGS__);

#define LOGV(...) fprintf(stderr, "V/" LOG_TAG ": " __VA_ARGS__); \
                  watchdogHelper::watchdog().dump_message("V/" LOG_TAG ": " __VA_ARGS__);

#define LOGI(...) fprintf(stderr, "I/" LOG_TAG ": " __VA_ARGS__); \
                  watchdogHelper::watchdog().dump_message("I/" LOG_TAG ": " __VA_ARGS__);

#define LOGE(...)                                                          \
    fprintf(stderr, RED "E/" LOG_TAG ": " __VA_ARGS__);                    \
    watchdogHelper::watchdog().dump_message("E/" LOG_TAG ": " __VA_ARGS__); \
    fprintf(stderr, RESET);

#if !defined(NDEBUG)
#define LOGD(...)                                        \
    fprintf(stderr, BLUE "D/" LOG_TAG ": " __VA_ARGS__); \
    watchdogHelper::watchdog().dump_message("E/" LOG_TAG ": " __VA_ARGS__); \
    fprintf(stderr, RESET);
#else
#define LOGD(...)      \
    {                  \
    }
    #endif //__debug__
#endif // UNITY

#endif //endof none __WEYE__

 
