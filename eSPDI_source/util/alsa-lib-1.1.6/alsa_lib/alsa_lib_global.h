#ifndef ALSA_LIB_GLOBAL_H
#define ALSA_LIB_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(ALSA_LIB_LIBRARY)
#  define ALSA_LIBSHARED_EXPORT Q_DECL_EXPORT
#else
#  define ALSA_LIBSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // ALSA_LIB_GLOBAL_H
