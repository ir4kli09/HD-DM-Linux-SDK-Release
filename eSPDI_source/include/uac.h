#ifndef UAC
#define UAC
/*! \file uac.h
    \brief uac functions
    Copyright:
    This file copyright (C) 2017 by

    eYs3D an Etron company

    An unpublished work.  All rights reserved.

    This file is proprietary information, and may not be disclosed or
    copied without the prior permission of eYs3D an Etron company.
 */
#include <stdlib.h>

#define PERIOD_BUFFER_LENGTH    32
#define FRAME_BUFFER_LENGTH     PERIOD_BUFFER_LENGTH * 4
extern int uac_name(char *input, char *output);
extern int uac_init(char *deviceName);
extern int uac_get(unsigned char *buffer, int length);
extern int uac_release(void);
extern int begin_wave(int fd);
extern int end_wave(int fd, size_t length);
#endif // UAC

