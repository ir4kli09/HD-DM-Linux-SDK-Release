/*! \file InterruptHandle.h
    \brief The definition of Interrupt functions
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#ifndef INTERRUPT_HANDLE_H
#define INTERRUPT_HANDLE_H
int getIntEventPath(int vendor_id, char *eventPath);
int getInterrupt(int fd);
#endif

