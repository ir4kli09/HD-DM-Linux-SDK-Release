//#include "stdafx.h" // MFC only
//#include "eSPDI_Common.h"
#include "eSPDI_DM.h"
#include <opencv2\opencv.hpp>
#include "Post_Process_API.h"
#include "Public_Tool.h"
#include <time.h>

void *pHandleEtronDI = NULL;
DEVSELINFO g_DevSelInfo;

cv::Mat pRGBImg;
cv::Mat pDepthImg;


unsigned char D11ToGrayTable[16385];

void ImgCallback(EtronDIImageType::Value imgType, int imgId, unsigned char* imgBuf, int imgSize,
	int width, int height, int serialNumber, void* pParam)
{
	if (EtronDIImageType::IsImageColor(imgType)) // color image
	{
		// Convert color format from YUY2 to RGB24
		EtronDI_ColorFormat_to_RGB24(pHandleEtronDI, &g_DevSelInfo, (unsigned char*)(pRGBImg.data), imgBuf, imgSize, width, height, EtronDIImageType::COLOR_YUY2);
	}
	else if (EtronDIImageType::IsImageDepth(imgType)) // depth image
	{
		memcpy(pDepthImg.data, imgBuf, imgSize);
	}
}

int generate_D11ToGrayTable(unsigned char *table)
{
	int i;
	int znear = 100;
	int zfar = 3000;

	memset(table, 0, 16385);

	for (i = znear; i <= zfar; i++)
	{
		table[i] = 1 + ((255 - 1) * (i - znear) / (zfar - znear));
	}

	return 0;
}

int D11ToGray(cv::Mat d11_img, cv::Mat gray_img, int width, int height, unsigned char *zdtable)
{
	int i, j;
	int value;
	unsigned short *d11_ptr = (unsigned short *)d11_img.data;
	unsigned char *gray_ptr = (unsigned char *)gray_img.data;

	for (i = 0; i < height*width; i++) {
		value = D11ToGrayTable[((unsigned short*)zdtable)[d11_ptr[i]]];
		gray_ptr[i] = value;//(d11_ptr[i] * 0.125);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int width = 1280;
	int height = 720;
	int nFPS = 30;
	bool bMjpg = false;

	//
	// Create image buffers
	pRGBImg.create(height,width,CV_8UC3);
	memset(pRGBImg.data, 0, width*height * 3*sizeof(unsigned char));
	pDepthImg.create(height, width, CV_16UC1);
	cv::Mat pDepthImg_after(height, width, CV_16UC1);
	cv::Mat pDepthImg_out(height, width, CV_8UC1);
	cv::Mat m_gray(height, width, CV_8UC1);
	//
	// Initialize EtronDI 
	//
	int nDeviceCount = EtronDI_Init(&pHandleEtronDI, false);

	if (nDeviceCount <= ETronDI_OK)
		return -1;

	//
	// Set the format of depth image
	//
	EtronDI_SetDepthDataType(pHandleEtronDI, &g_DevSelInfo, ETronDI_DEPTH_DATA_11_BITS);

	//
	// Open the device
	//
	int colorOption = 0; // 0: 1280x720, 4: 640x480
	int depthOption = 0; // 0: 1280x720, 1: 640x480
	int depthSwitch = 2;
	int nRet = EtronDI_OpenDevice(pHandleEtronDI, &g_DevSelInfo, colorOption, depthOption, depthSwitch, nFPS, ImgCallback, NULL, EOF);

	if (nRet != ETronDI_SET_FPS_FAIL && nRet != ETronDI_OK)
	{
		return -2;
	}

	//
	// Turn on the IR light
	//
	int IR_value = 3;
	EtronDI_SetFWRegister(pHandleEtronDI, &g_DevSelInfo, 0xE3, 0x3F, FG_Address_1Byte | FG_Value_1Byte);
	EtronDI_SetFWRegister(pHandleEtronDI, &g_DevSelInfo, 0xE0, IR_value, FG_Address_1Byte | FG_Value_1Byte);
	
	//
	// Create output windows
	std::vector<unsigned char> bufDepth;
	std::vector<unsigned char> bufColor;
	unsigned char tmp;
	
	unsigned char* p_color = (unsigned char*)pRGBImg.data;
	unsigned char* p_depth = (unsigned char*)pDepthImg_out.data;


	clock_t start, stop;
	double ectime = 0;
	bool f_do_post_processing = true;
	string str = "";
	POST_PROCESS_API_PRESET_MODES Preset_Mode[5] = {
	POST_PROCESS_API_PRESET_MODE_DEFAULT,
	POST_PROCESS_API_PRESET_MODE_HAND,
	POST_PROCESS_API_PRESET_MODE_OBSTACLE_AVOIDANCE,
	POST_PROCESS_API_PRESET_MODE_BASIC,
	POST_PROCESS_API_PRESET_MODE_HIGH_ACCURACY
		};

	string Preset_Modes[5] = {
	"DEFAULT",
	"HAND",
	"OBSTACLE_AVOIDANCE",
	"BASIC",
	"HIGH_ACCURACY"
	};
	int i = 0;

	// 0. new handler
	int data_bits = 11;
	POST_PROCESS_API * p_pPApi = new POST_PROCESS_API(720, 1280);
	p_pPApi->focus_x = 556;
	p_pPApi->baseline = 60;
	p_pPApi->depth_data_type = POST_PROCESS_API_DATA_TYPE_DEPTH_D;
	p_pPApi->data_unit = 0.125;


	while (1)
	{
		// 1. set data pointer
		p_pPApi->p_src_data = pDepthImg.data;
		p_pPApi->p_dst_data = pDepthImg.data; // p_dst_data & p_src_data can be the same or not

		cv::cvtColor(pRGBImg, m_gray, cv::COLOR_BGR2GRAY);
		p_pPApi->p_intensity = m_gray.data; // must be single channel
		

		// 2. runtime
		start = clock();
		if (f_do_post_processing) {
			p_pPApi->SetPresetMode(Preset_Mode[i]); // optional
			p_pPApi->PostProcessing();
		}
		stop = clock();
		ectime = ectime * 0.9 + (double)(stop - start) / CLOCKS_PER_SEC*1000*0.1;

		str = Preset_Modes[i] +" : "+ std::to_string(ectime);
		cv::putText(pDepthImg, str, cvPoint(50, 50),
			0, 2, (255,255,255), 3, CV_AA);

		PublicTool::int16_im_show(pDepthImg, pDepthImg_after, pow(2, data_bits)," Depth", 1, 0);
		cv::namedWindow("m_gray", cv::WINDOW_NORMAL);
		cv::imshow("m_gray", m_gray);
		

		bufDepth.pop_back();
		int key = cvWaitKey(30);
		if (key == 's') {
			if (i < 4)
				i++;
			else
				i = 0;
		}
			

		if (key == 27 || key == 'q')
			break;
	}
	
	

	// Close the device and relese the EtronDI
	// 3. release
	delete p_pPApi;

	EtronDI_CloseDevice(pHandleEtronDI, &g_DevSelInfo);
	EtronDI_Release(&pHandleEtronDI);

	return 0;
}
