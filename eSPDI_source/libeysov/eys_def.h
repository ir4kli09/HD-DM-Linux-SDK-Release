/**
  * @file  eys_def.h
  * @title Definition of Enum
  */
#ifndef __EYS_DEF_H__
#define __EYS_DEF_H__

/**
  * Define operation system
  */
#ifdef WIN32
#include <cstddef>
#endif

#ifdef linux
#include <cstddef>
#endif

#define NULL 0

#if !defined(nullptr)
#define nullptr NULL
#endif // nullptr

const double EYS_PI = 3.141592653589793238462;

/**
  * @defgroup eys_enum Enum Definition
  * Functions for image process
  */

namespace eys
{

/**
  * @addtogroup eys_enum
  * @{
  */

/**
  * Image color flags
  */
enum ImgColorModes
{
	IMG_COLOR_GRAY     =  0, //!< The image is 1 byte gray level
	IMG_COLOR_RGB      =  1, //!< The image is color, the first byte is R, the second byte is G, and the third byte is B
	IMG_COLOR_BGR      =  2, //!< The image is color, the first byte is B, the second byte is G, and the third byte is R
	IMG_COLOR_JPG      =  3, //!< The image is jpeg format
	IMG_COLOR_YUY2     =  4, //!< The image is yuv422 (yuy2) format
	IMG_COLOR_YUV_NV12 =  5, //!< The image is yuv420-nv12 format
	IMG_COLOR_R        =  6, //!< The image is red format
	IMG_COLOR_G        =  7, //!< The image is green format
	IMG_COLOR_B        =  8, //!< The image is blue format
	IMG_COLOR_WHITE    =  9, //!< The image is white format
	IMG_COLOR_BLACK    = 10, //!< The image is black format
	IMG_COLOR_RGBA     = 11, //!< The image is color, the first byte is R, the second byte is G, and the third byte is B, fourth byte is A
	IMG_COLOR_BGRA     = 12  //!< The image is color, the first byte is B, the second byte is G, and the third byte is R, fourth byte is A
};

/**
  * Image filter flags
  */
enum ImgFilterModes
{
	IMG_FILTER_EDGE_PRESERVE  = 0, //!< Edge preserving filter, s=60, r=0.40
	IMG_FILTER_DETAIL_ENHANCE = 1, //!< Detail enhancing filter, s=10, r=0.15
	IMG_FILTER_STYLIZATION    = 2  //!< Stylization filter, s=60, r=0.45
};

/**
  * Image resolution flags
  */
enum ImgResolutionModes
{
	IMG_RESOLUTION_NORMAL    = 0,  //!< Image original resolution
	IMG_RESOLUTION_1080P     = 1,  //!< Image 1080p resolution with size of 1920 x 1080
	IMG_RESOLUTION_4096X1920 = 2,  //!< Image resolution with size of 4096 x 1920, not work now
	IMG_RESOLUTION_4320x2160 = 3,  //!< Image resolution with size of 4320 x 2160, not work now
	IMG_RESOLUTION_FAST_64   = 4,  //!< Only dewarp boundary 64 pixels, just for buffer output 
	IMG_RESOLUTION_2560x1440 = 5   //!< Image resolution with size of 2560 x 1440
};


/**
  * Video type flags
  */
enum VideoModes
{
	VIDEO_DIVX  = 0, //!< Divx video format
	VIDEO_MJPEG = 1, //!< MJPEG video format
	VIDEO_X264  = 2  //!< X264 video format
};

/**
  * Look up table flags
  */
enum LutModes
{
	LUT_LXLYRXRY_16_3      = 0, //!< Format of [LxLy][RxRy] with total bits of 16 and 3 bits for float, used in my program
	LUT_LXRXLYRY_16_3      = 1, //!< Format of [LxRxLyRy] with total bits of 16 and 3 bits for float, used in Etron Cap
	LUT_LXRXLYRY_16_3_BIT0 = 2, //!< Format of [LxRxLyRy] with total bits of 16 and 3 bits for float and lowest bit is set to 0, used in Etron Cap
	LUT_LXRXLYRY_16_5      = 3  //!< Format of [LxRxLyRy] with total bits of 16 and 5 bits for float
};


/**
  * Lens types flags
  */
enum LensModes
{
	LENS_SMALL_ANGLE = 0, //!< Lens is for small angle
	LENS_WIDE_ANGLE  = 1  //!< Lens is for wide angle
};

/**
  * Lens projection flags
  */
enum LensProjectionModes
{
	LENS_PROJ_EQUIDISTANT   = 0, //!< Equidistant fisheye : this is the ideal fisheye projection panotools uses internally
	LENS_PROJ_STEREOGRAPHIC = 1, //!< Stereographic fisheye
	LENS_PROJ_ORTHOGRAPHIC  = 2, //!< orthographic fisheye
	LENS_PROJ_EQUISOLID     = 3  //!< equisolid (equal-area) fisheye
};

/**
  * Lens distortion types flags
  */
enum LensDistortionPara
{
	LENS_SMALL_ANGLE_K1                      = 0, //!< Distortion parameters used in small angle lens
	LENS_SMALL_ANGLE_K1_K2                   = 1, //!< Distortion parameters used in small angle lens
	LENS_SMALL_ANGLE_K1_K2_P1_P2             = 2, //!< Distortion parameters used in small angle lens
	LENS_SMALL_ANGLE_K1_K2_P1_P2_K3          = 3, //!< Distortion parameters used in small angle lens
	LENS_SMALL_ANGLE_K1_K2_P1_P2_K3_K4_K5_K6 = 4, //!< Distortion parameters used in small angle lens
	LENS_WIDE_ANGLE_K1                       = 5, //!< Distortion parameters used in wide angle lens
	LENS_WIDE_ANGLE_K1_K2                    = 6, //!< Distortion parameters used in wide angle lens
	LENS_WIDE_ANGLE_K1_K2_K3                 = 7, //!< Distortion parameters used in wide angle lens
	LENS_WIDE_ANGLE_K1_K2_K3_K4              = 8  //!< Distortion parameters used in wide angle lens
};

/**
  * Direction flags
  */

enum DirModes
{
	DIR_LEFT   = 0, //!< Direction left
	DIR_RIGHT  = 1, //!< Direction right
	DIR_FRONT  = 2, //!< Direction front
	DIR_BACK   = 3, //!< Direction back
	DIR_TOP    = 4, //!< Direction top
	DIR_BOTTOM = 5  //!< Direction bottom
};

/**
  * @} eys_enum
  */

} // eys
#endif // __EYS_DEF_H__
