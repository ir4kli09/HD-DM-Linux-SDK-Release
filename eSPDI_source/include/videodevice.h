/*! \file videodevice.h
  	\brief The definition of CVideoDevice class member function and data
  	Copyright:
	This file copyright (C) 2017 by

	eYs3D an Etron company

	An unpublished work.  All rights reserved.

	This file is proprietary information, and may not be disclosed or
	copied without the prior permission of eYs3D an Etron company.
 */
#pragma once

#include "eSPDI_def.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <iostream>
#include <iomanip>
#include <errno.h>
#include <pthread.h>
#include <memory>
#include <vector>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

// extension command control +

#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <linux/usb/video.h>

#include "CComputer.h"

#define LENGTH_LIMIT 32  // IMU Module Extention Unit: 128, Flash R/W: 32
// File ID +
typedef struct tagFWFS_DATA {
	unsigned char      nMutex;
	unsigned char      nType;
	unsigned char      nFsID;
	unsigned long int  nLength;
	unsigned long int  nOffset;
    unsigned char      nData[LENGTH_LIMIT];
	unsigned char      nRetCode;
} FWFS_DATA;

#define FWFS_TYPE_GET_MUTEX			0x01
#define FWFS_TYPE_RELEASE_MUTEX		0x02
#define FWFS_TYPE_WRITE_FS			0x03
#define FWFS_TYPE_READ_FS			0x04
#define FWFS_TYPE_WRITE_SYNC		0x05
#define FWFS_TYPE_GET_FS_LENGTH		0x06
#define FWFS_TYPE_WRITE_FLASH		0x07
#define FWFS_TYPE_READ_FLASH		0x08
#define FWFS_TYPE_READ_GV			0x09
#define FWFS_TYPE_WRITE_GV			0x0A
#define FWFS_TYPE_READ_SRAM			0x0B


// File ID -

#define ExtenID    4

#define T_I2C      2
#define T_ASIC     3
#define T_OTP      4
#define T_FIRMWARE 7

// for serial number +
#define VARIABLE_DATA_START_ADDR 59
#define SERIAL_NUMBER_START_OFFSET 208
// for serial number -

// extension command control -

// for calibration log parse +
#define SN_ERROR_ID                  0
#define PRJ_ERROR_ID                 1
#define STAGE_TIME_RESULT_ERROR_ID   2
#define YOFFSET_ERROR_ID             3
#define AUTOADJUSTMENT_ERROR_ID      4
#define SMARTK_ERROR_ID              5
#define SMARTZD_ERROR_ID             6
#define DQ_ERROR_ID                  7
// for calibration log parse -

// for image capture +

// for Rectify Log +

#define TableHeaderSize		48 //bytes
#define TableInfoSize		16 //bytes
#define TAB_MAX_NUM 29

// for Rectify Log -

// for sensorif +

#define SENIF_EN_REG 0xf0b3

// for sensorif -

// for sensor slave address +
#define H22_SLAVE_ADDR     0x60
#define H65_SLAVE_ADDR     0x60
#define OV4770_SLAVE_ADDR  0x42
#define AR0134_SLAVE_ADDR  0x20
#define AR0135_SLAVE_ADDR  0x30
#define AR0144_SLAVE_ADDR  0x30
#define AR0330_SLAVE_ADDR  0x30
#define AR0522_SLAVE_ADDR  0x6C
#define AR1335_SLAVE_ADDR  0x6C
#define OV9714_SLAVE_ADDR  0x6C
#define OV9282_SLAVE_ADDR  0xC0
// for sensor slave address -

// for 3D Motor Control +

// Motor Reg Addr.
#define MOTOR_STEP_NUMBER_REG_LOWBYTE    0x81
#define MOTOR_STEP_NUMBER_REG_HIGHBYTE   0x9e

// Return Home
#define RETURN_HOME_REG     0x9f

#define STEPAMOUNT_PER_ROUND 512

// Gyro Slave Addr.
#define L3G_SLAVE_ADDR    0xd6
#define LSM_SLAVE_ADDR    0x3a
#define LPS_SLAVE_ADDR    0xba

// Sensor Addr.
#define L3G_CTRL_REG1     0x20
#define L3G_CTRL_REG4     0x23

#define LSM_CTRL_REG1     0x20
#define LSM_CTRL_REG2     0x21
#define LSM_CTRL_REG5     0x24
#define LSM_CTRL_REG6     0x25
#define LSM_CTRL_REG7     0x26

#define LPS_CTRL_REG1     0x20

// FW reg addr.
#define GYRO_X_DATA_LOW     0x84
#define GYRO_X_DATA_HIGH    0x85
#define GYRO_Y_DATA_LOW     0x86
#define GYRO_Y_DATA_HIGH    0x87
#define GYRO_Z_DATA_LOW     0x88
#define GYRO_Z_DATA_HIGH    0x89

#define ACCELE_X_DATA_LOW   0x8a
#define ACCELE_X_DATA_HIGH  0x8b
#define ACCELE_Y_DATA_LOW   0x8c
#define ACCELE_Y_DATA_HIGH  0x8d
#define ACCELE_Z_DATA_LOW   0x8e
#define ACCELE_Z_DATA_HIGH  0x8f

#define PRESS_DATA_LOW      0x90
#define PRESS_DATA_MIDD     0x91
#define PRESS_DATA_HIGH     0x92

#define TEMP_DATA_LOW       0x93
#define TEMP_DATA_HIGH      0x94

#define MAGNE_X_DATA_LOW    0x95
#define MAGNE_X_DATA_HIGH   0x96
#define MAGNE_Y_DATA_LOW    0x97
#define MAGNE_Y_DATA_HIGH   0x98
#define MAGNE_Z_DATA_LOW    0x99
#define MAGNE_Z_DATA_HIGH   0x9a

#define LASER_LED_POWER_CTRL 0x9b
#define LED_BRIGHTNESS_CTRL  0x9c

// for 3D Motor Control -

struct buffer
{
	void * start;
	size_t length;
};

class DepthmapFilter;
class CVideoDevice;
class CFWMutexLocker
{
public:
    CFWMutexLocker(pthread_mutex_t *pMutex, FWFS_DATA *pFWData, CVideoDevice *pVideoDevice);
    ~CFWMutexLocker();

    bool HasFWMutex(){ return m_bHasFWMutex; }
    void Release();
private:
    bool m_bHasFWMutex;
    bool m_bThreadLock;
    pthread_mutex_t *m_pMutex;
    FWFS_DATA *m_pFWData;
    unsigned char m_nFWMutex;
    CVideoDevice *m_pVideoDevice;
};

// for image capture -

class CVideoDevice {

public:
	CVideoDevice(char *szDevName, bool bIsLogEnabled);
	~CVideoDevice();	
	
	// for register +
	// sensor
	int GetSensorRegister( int nId, unsigned short address, unsigned short* pValue, int flag, int nSensorMode);
	int SetSensorRegister( int nId, unsigned short address, unsigned short nValue, int flag, int nSensorMode);
	// FW
	int GetFWRegister( unsigned short address, unsigned short* pValue, int flag);
	int SetFWRegister( unsigned short address, unsigned short nValue,  int flag);
	// HW
	int GetHWRegister( unsigned short address, unsigned short* pValue, int flag);
	int SetHWRegister( unsigned short address, unsigned short nValue,  int flag);
	// for register -	
	
	// File ID +
    int GetFwVersion        ( char *pszFwVersion, int nBufferSize, int *pActualLength);
    int GetPidVid           ( unsigned short *pPidBuf, unsigned short *pVidBuf);
    int SetPidVid           ( unsigned short *pPidBuf, unsigned short *pVidBuf);
    int GetSerialNumber     ( unsigned char* pData, int nbufferSize, int *pLen);
    int SetSerialNumber     ( unsigned char* pData, int nLen);
    int GetYOffset          ( BYTE *buffer, int BufferLength, int *pActualLength, int index);
    int SetYOffset          ( BYTE *buffer, int BufferLength, int *pActualLength, int index);
    int GetRectifyTable     ( BYTE *buffer, int BufferLength, int *pActualLength, int index);
    int SetRectifyTable     ( BYTE *buffer, int BufferLength, int *pActualLength, int index);
    int GetZDTable          ( BYTE *buffer, int BufferLength, int *pActualLength, PZDTABLEINFO pZDTableInfo);
    int SetZDTable          ( BYTE *buffer, int BufferLength, int *pActualLength, PZDTABLEINFO pZDTableInfo);
    int GetLogData          ( BYTE *buffer, int BufferLength, int *pActualLength, int index, CALIBRATION_LOG_TYPE type);
    int GetRecifyMatLogData ( eSPCtrl_RectLogData *pData, int index);
    int SetLogData          ( BYTE *buffer, int BufferLength, int *pActualLength, int index, CALIBRATION_LOG_TYPE type);
    int GetUserData         ( BYTE *buffer, int BufferLength, USERDATA_SECTION_INDEX usi);
    int SetUserData         ( BYTE *buffer, int BufferLength, USERDATA_SECTION_INDEX usi);
    int ReadFlashData       ( BYTE *pBuffer, unsigned long int BufferLength,
                                unsigned long int *pActualLength,  FLASH_DATA_TYPE fdt, bool bBootloader);
    int WriteFlashData_FullFunc ( BYTE *pBuffer, unsigned long int BufferLength, 
                                  FLASH_DATA_TYPE fdt, KEEP_DATA_CTRL kdc, bool bIsDataVerify,  bool bBootloader);
    int WriteFlashData ( BYTE *pBuffer, unsigned long int BufferLength, unsigned long int nStartAddr, 
					     unsigned long int nWriteLen, FLASH_DATA_TYPE fdt );							
    int GetDevicePortType(USB_PORT_TYPE &eUSB_Port_Type);
	// File ID -
		
	// for image +
	
	int GetV4l2BusInfo ( char *pszBusInfo);
	int GetUSBBusInfo ( char *pszBusInfo);
    int GetDeviceCapability ( unsigned int *pDeviceCapability);
	int GetDeviceResolutionList(int nMaxCount, ETRONDI_STREAM_INFO *pStreamInfo);
    int Set_v4l2_requestbuffers(int cnt);
	int OpenDevice( int nResWidth, int nResHeight, bool bIsMJPEG, int *pFPS, DEPTH_TRANSFER_CTRL dtc = DEPTH_IMG_NON_TRANSFER);
	int CloseDevice();
    bool Is2BitSerial();
    int GetImage(BYTE *pBuf, unsigned long int *pImageSize, int *pSerial = NULL, bool bIsDepthmap = false, unsigned short nDepthDataType = 0);
	int EnableFrameCount();
    int SetDepthDataType(unsigned short nValue);
    int GetDepthDataType(unsigned short *pValue);
	int SetCurrentIRValue(unsigned short nValue);
	int GetCurrentIRValue(unsigned short *pValue);
    int SetIRMaxValue(unsigned short nValue);
	int GetIRMinValue(unsigned short *pValue);
	int GetIRMaxValue(unsigned short *pValue);
	int SetIRMode(unsigned short nValue);
	int GetIRMode(unsigned short *pValue);
    int SetupNonBlock();
    int SetupBlock(int waitingTime_sec, int waitingTime_usec);
	// for image -
	
	// for AEAWB Control +
	
	int SetSensorTypeName( SENSOR_TYPE_NAME stn);
	int GetExposureTime( int nSensorMode, float *pfExpTimeMS);
	int SetExposureTime( int nSensorMode, float fExpTimeMS);
	int GetGlobalGain( int nSensorMode, float *pfGlobalGain);
	int SetGlobalGain( int nSensorMode, float fGlobalGain);
	int GetColorGain( int nSensorMode, float *pfGainR, float *pfGainG, float *pfGainB);
	int SetColorGain( int nSensorMode, float fGainR, float fGainG, float fGainB);
	int GetAccMeterValue( int *pX, int *pY, int *pZ);
	
	int GetAEStatus  (PAE_STATUS pAEStatus);
	int GetAWBStatus (PAWB_STATUS pAWBStatus);
    int EnableAE(bool enable);
    int EnableAWB(bool enable);

	int GetCTPropVal( int nId, long int *pValue);	
	int SetCTPropVal( int nId, long int nValue);
	int GetPUPropVal( int nId, long int *pValue);	
	int SetPUPropVal( int nId, long int nValue);
    int GetCTRangeAndStep( int nId, int *pMax, int *pMin, int *pStep, int *pDefault, int *pFlags);
    int GetPURangeAndStep( int nId, int *pMax, int *pMin, int *pStep, int *pDefault, int *pFlags);
	
	// for AEAWB Control -
	
	// for GPIO +
	int GetGPIOValue(int nGPIOIndex, BYTE *pValue);
	int SetGPIOValue(int nGPIOIndex, BYTE nValue);
	int SetGPIOCtrl(int nGPIOIndex, BYTE nValue);
	// for GPIO -
	
	// for calibration log +
	int GetSerialNumberFromCalibrationLog();
	int GetProjectFileFromCalibrationLog();
	int GetSTRLogFromCalibrationLog();
	int GetSensorOffsetFromCalibrationLog();
	int GetAutoAdjustLogFromCalibrationLog();
    int GetRectifyLogFromCalibrationLog(eSPCtrl_RectLogData *pData, int index);
	int GetZDLogFromCalibrationLog();
	int GetDepthQualityLogFromCalibrationLog();
	// for calibration log -
	
	// for sensorif +
	int EnableSensorIF(bool bIsEnable);
	// for sensorif -
	
	// for 3D Motor Control +
	int MotorInit( unsigned short nMinSpeedPerStep); 
	int MotorStart ( int motor_id, bool direction, double angle, long step, int timeperstep, float *steppersecond, bool bIsStep, bool bIsTime, bool bIsInfinity); 
	int MotorStart_IndexCheckOption( int motor_id, bool direction, double angle, long step, int timeperstep, float *steppersecond, bool bIsStep, bool bIsTime, bool bIsInfinity, bool bIsIgnoreHomeIndexTouched); 

	int MotorStop();
	int GetMotorCurrentState( bool* bIsRunning);
	int GetMotorCurrentState_IndexCheckOption( bool* bIsRunning, long* nRemainStepNum, int *nHomeIndexNum);
	int GetMotorAngle( double *angle);
	int GetMotorStep( long *step);    
	int GetMotorTimePerStep( unsigned short *nTimePerStep);
	int GetMotorStepPerSecond( float *fpStepPerSecond);
	int GetMotorDirection( bool *direction);
	int GetCurrentMotor( int *motor_id); 

	// for Gyro +	
	int GyroInit( SENSITIVITY_LEVEL_L3G sll3g, 
				  bool bPowerMode, 
				  bool bIsZEnable, 
				  bool bIsYEnable, 
				  bool bIsXEnable);
					   
	int ReadGyro( GYRO_ANGULAR_RATE_DATA *gard);
	int ReadGyroData(unsigned short* xValue, unsigned short* yValue, unsigned short* zValue, unsigned short* frameCount);
	int ReadGlobalVariable(unsigned short addr, BYTE* data, unsigned short dataLen);
	int ReadSRAM(BYTE* data, unsigned short dataLen);
	int ReadFlexibleGyroData(BYTE *data, int len);
	int GetMutex(void);
	int ReleaseMutex(void);
	// for Gyro -

	// for accelerometer and magnetometer +	
	int AccelInit( SENSITIVITY_LEVEL_LSM sllsm);
	int ReadAccel( ACCELERATION_DATA *ad);
	int ReadCompass( COMPASS_DATA *cd);
	// for accelerometer and magnetometer -

	// for pressure +	
	int PsInit( OUTPUT_DATA_RATE odr);
	int ReadPressure( int *nPressure);
	int ReadTemperature( short *nTemperature);
	// for pressure -

	// for LED and Laser +	
	int SetLaserPowerState( POWER_STATE ps);
	int SetDesktopLEDPowerState( POWER_STATE ps);
	int GetLaserPowerState( POWER_STATE *ps);
	int GetDesktopLEDPowerState( POWER_STATE *ps);
	int SetMobileLEDBrightnessLevel( BRIGHTNESS_LEVEL bl);
	int GetMobileLEDBrightnessLevel( BRIGHTNESS_LEVEL *pbl);
	// for LED and Laser -
	
	// Return home +
	int MotorStartReturnHome();
	int IsMotorReturnHomeRunning();
	// Return home -

    // Set color palette for depth
    void SetBaseColorPalette(int nDepthDataType);
    void SetBaseGrayPalette(int nDepthDataType);


	// for 3D Motor Control -
	
	int GenerateLutFile(const char* filename);
	int SaveLutData(const char* filename);
	int EncryptMp4(const char *filename);
	int DecryptMp4(const char *filename);
	int InjectExtraDataToMp4(const char* filename, const char* data, int dataLen);
    int RetrieveExtraDataFromMp4(const char* filename, char* data, int* dataLen);
	static int EncryptString(const char* src, char* dst);
	static int DecryptString(const char* src, char* dst);
	static int EncryptString(const char* src1, const char* src2, char* dst);
	static int DecryptString(const char* src, char* dst1, char* dst2);
	
    // for OpenCL Functions
    int FlyingDepthCancellation_D8(unsigned char *pdepthD8, int width, int height);
    int FlyingDepthCancellation_D11(unsigned char *pdepthD11, int width, int height);
    int ColorFormat_to_RGB24( unsigned char* ImgDst, unsigned char* ImgSrc, int SrcSize, int width, int height, EtronDIImageType::Value type );
    int DepthMerge( unsigned char** pDepthBufList, float *pDepthMergeOut, unsigned char *pDepthMergeFlag,
                    int nDWidth, int nDHeight, float fFocus, float * pBaseline, float * pWRNear, float * pWRFar, float * pWRFusion, int nMergeNum );
    int RotateImg90(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst, int len, bool clockwise);
    int ImgMirro(EtronDIImageType::Value imgType, int width, int height, unsigned char *src, unsigned char *dst);
    void Resample( const BYTE* ImgSrc, const int SrcW, const int SrcH, 
				   BYTE* ImgDst, const int DstW, const int DstH,
				   int BytePerPixel);
    int GetPointCloud( unsigned char* ImgColor, int CW, int CH,
                       unsigned char* ImgDepth, int DW, int DH,
                       PointCloudInfo* pPointCloudInfo,
                       unsigned char* pPointCloudRGB, float* pPointCloudXYZ, float Near, float Far, unsigned short pid);

    // for Depth Filter
    BYTE* SubSample(BYTE* depthBuf, int bytesPerPixel, int width, int height, int& new_width, int& new_height, int mode, int factor);
    void HoleFill(BYTE* depthBuf, int bytesPerPixel, int kernel_size, int width, int height, int level, bool horizontal);
	void TemporalFilter(BYTE* depthBuf, int bytesPerPixel, int width, int height, float alpha, int history);
	void EdgePreServingFilter(BYTE* depthBuf, int type, int width, int height, int level, float sigma, float lumda);
    void ApplyFilters(BYTE* depthBuf, BYTE* subDisparity, int bytesPerPixel, int width, int height, int sub_w, int sub_h, int threshold);
    void ResetFilters();
	void EnableGPUAcceleration(bool enable);
	int TableToData(int width, int height, int TableSize, unsigned short* Table, unsigned short* Src, unsigned short* Dst);

    int SetPlumAR0330(bool bEnable);

	int GetDeviceIndex();

	std::string GetDeviceStatString();

    friend class CFWMutexLocker;
	
private:
	int     WaitFwMuteReleased();
	bool IsPlugInVersionNew(bool *bIsNewVersionPlugIn);
	int  FWV3WriteData(unsigned char* Data, int nID, unsigned long int WriteLen);
	int  FWV3ReadData (unsigned char* Data, int nID, unsigned long int ReadLen );	
	int  GetPropertyValue( unsigned short address, unsigned short* pValue, int control, int flag);
	int  SetPropertyValue( unsigned short address, unsigned short nValue,  int control, int flag);
	int  GetPropertyValue_I2C( int senaddr, unsigned short address, unsigned short* pValue, int control, int flag, int nSensorMode);
	int  SetPropertyValue_I2C( int senaddr, unsigned short address, unsigned short nValue,  int control, int flag, int nSensorMode);
	int  FWFS_Command( FWFS_DATA *pFWFSData);
	int  enum_frame_formats(ETRONDI_STREAM_INFO *pStreamInfo);
	int  enum_frame_sizes(__u32 pixfmt, ETRONDI_STREAM_INFO *pStreamInfo);
	int  enum_frame_intervals(__u32 pixfmt, __u32 width, __u32 height);
	
	bool GetDataLengthFromFileID( int nFileId, BYTE *pBuffer, int *pLength, long nFileSysStartAddr );
	bool GetDataBufferFromFileID( int nFileId, BYTE *pBuffer, BYTE *pFileIDBuffer, int nLength ,long nFileSysStartAddr );
	bool SetDataBufferToFileID  ( int nFileId, BYTE *pBuffer, BYTE *pFileIDBuffer, int nLength ,long nFileSysStartAddr );
	
	int init_device();
	int set_framerate(int *pFPS);
	int init_mmap();
	int start_capturing();
	int stop_capturing();
	int uninit_device();
	int get_frame(void **, size_t*);
    int unget_frame();
    void HSV_to_RGB(double H, double S, double V, double &R, double &G, double &B);
    //int convert_depth_y_to_gray_rgb_buffer(unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height);

    bool GetLogBufferByTag( unsigned char nID, unsigned char *pTotalBuffer, unsigned char *pOutputBuffer, int *pLength);
    
    bool IsAngleLegal(double angle);
    double MotorStepToRotationAngle(long nTotalStep);
    void ByteExchange(unsigned char& byte1, unsigned char& byte2);
    int m_waitingTime_sec;
    int m_waitingTime_usec;
    bool m_b_blocking;
	
    void RelaseComputer();	

	std::string GetDeviceName();
	std::string GetDeviceRealPath();
	std::string GetDeviceUSBInfoPath();

public:
    int convert_depth_y_to_buffer(unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height, bool color, unsigned short nDepthDataType);
    int convert_depth_y_to_buffer_offset(unsigned char *depth_y, unsigned char *rgb, unsigned int width, unsigned int height, bool color, unsigned short nDepthDataType, int offset);
	char m_szDevName[64];
    int m_fd;
	buffer* buffers;
    unsigned int n_buffers;
	bool m_bHasStartCapture;
    
    // extension command control +
    FWFS_DATA m_FWFSData;
    // extension command control -
    
    struct v4l2_frmivalenum m_fival;
    
    int m_RessolutionIndex;
    
    int m_index;
    int m_devType;
    int m_nFrameCount;
    
    void *m_p;
    
    DEPTH_TRANSFER_CTRL m_Dtc;

    unsigned char* m_nColorPaletteR_D8;
    unsigned char* m_nColorPaletteG_D8;
    unsigned char* m_nColorPaletteB_D8;
    unsigned char* m_nColorPaletteR_D11;
    unsigned char* m_nColorPaletteG_D11;
    unsigned char* m_nColorPaletteB_D11;
    unsigned char* m_nColorPaletteR_Z14;
    unsigned char* m_nColorPaletteG_Z14;
    unsigned char* m_nColorPaletteB_Z14;

    unsigned char* m_nGrayPaletteR_D8;
    unsigned char* m_nGrayPaletteG_D8;
    unsigned char* m_nGrayPaletteB_D8;
    unsigned char* m_nGrayPaletteR_D11;
    unsigned char* m_nGrayPaletteG_D11;
    unsigned char* m_nGrayPaletteB_D11;
    unsigned char* m_nGrayPaletteR_Z14;
    unsigned char* m_nGrayPaletteG_Z14;
    unsigned char* m_nGrayPaletteB_Z14;

    bool m_bIsLogEnabled;
    
    unsigned short m_MinSecPerStep;
    
    // for debug
	char m_szDbgStr[256];

	SENSOR_TYPE_NAME m_stn;
	pthread_mutex_t cs_mutex;
    pthread_mutex_t img_mutex;

    // for OpenCL
	std::unique_ptr< CComputer > m_pComputer;
    // Depth filter

	struct DepthFilterParameters{
		int nEdgePreServingWidth = 0;
		int nEdgePreServingHeight = 0;
		int nTemporalFilterWidth = 0;
		int nTemporalFilterHeight = 0;
		int nTemporalFilterBytePerPixel = 0;
	};

	DepthFilterParameters m_depthFilterParameters;
    std::shared_ptr< DepthmapFilter > m_depthFilter;

	void *m_pTjHandle;

	std::vector<BYTE> m_vecDepthResample;
    std::vector<BYTE> m_vecColorResample;   
};
