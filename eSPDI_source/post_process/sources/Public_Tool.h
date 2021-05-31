#ifndef _PUBLIC_TOOL_H_
#define _PUBLIC_TOOL_H_

#include <iostream>
#include <stdlib.h>
#include <opencv2/opencv.hpp>
using namespace std;

class PublicTool {
public:

	static void float_im_show(cv::Mat src, cv::Mat dst, float max, string name, int t_wait, bool kill);
	static void int16_im_show(cv::Mat src, cv::Mat dst, float max, string name, int t_wait, bool kill);
	static void float32ch3_im_show(cv::Mat src, cv::Mat dst, float max, string name, int t_wait, bool kill);
	static void int32_im_show(cv::Mat src, cv::Mat dst, float max, string name, int t_wait, bool kill);
	static void float_im_show(cv::Mat src, cv::Mat dst, float min, float max, string name, int t_wait, bool kill);
};



#endif