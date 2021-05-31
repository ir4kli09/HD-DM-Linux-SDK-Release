#include "Public_Tool.h"

using namespace std;
using namespace cv;

#define SHOW_RESULT (1)

void PublicTool::float_im_show(Mat src, Mat dst, float min, float max, string name, int t_wait, bool kill)
{
#if SHOW_RESULT
	int n_rows = src.rows;
	int n_cols = src.cols;
	//src /= max * 255;
	Mat dst2(src.rows, src.cols, CV_8UC1);
	for (int i = 0; i < n_rows*n_cols; i++) {
		float data = ((float*)src.data)[i];
		float tmp = (data - min) / (max- min) * 255;
		if (tmp > 255)
			tmp = 255;
		if (tmp < 0)
			tmp = 0;
		((unsigned char*)dst2.data)[i] = (unsigned char)tmp;

	}
	cv::namedWindow(name, WINDOW_NORMAL);
	//cv::equalizeHist(dst2, dst2);
	imshow(name, dst2);
	waitKey(t_wait);
	if (kill)
		destroyWindow(name);
#endif
};

void PublicTool::float_im_show(Mat src, Mat dst, float max, string name, int t_wait, bool kill)
{
#if SHOW_RESULT
	int n_rows = src.rows;
	int n_cols = src.cols;
	//src /= max * 255;
	Mat dst2(src.rows, src.cols, CV_8UC1);
	for (int i = 0; i < n_rows*n_cols; i++) {
		float tmp = (((float*)src.data)[i] / max * 255);
		if (tmp > 255)
			tmp = 255;
		((unsigned char*)dst2.data)[i] = (unsigned char)tmp;
		
	}
	cv::namedWindow(name, WINDOW_NORMAL);
	//cv::equalizeHist(dst2, dst2);
	imshow(name, dst2);
	waitKey(t_wait);
	if (kill)
		destroyWindow(name);
#endif
};

void PublicTool::int16_im_show(Mat src, Mat dst, float max, string name, int t_wait, bool kill)
{
#if (SHOW_RESULT)//SHOW_RESULT
	int n_rows = src.rows;
	int n_cols = src.cols;
	//src /= max * 255;
	Mat dst2(src.rows, src.cols, CV_8UC1);
	for (int i = 0; i < n_rows*n_cols; i++) {
		((unsigned char*)dst2.data)[i] = (unsigned char)(((unsigned short*)src.data)[i] / max * 255);
	}
	cv::namedWindow(name, WINDOW_NORMAL);
	//cv::equalizeHist(dst2, dst2);
	imshow(name, dst2);
	waitKey(t_wait);
	if (kill)
		destroyWindow(name);
#endif
};

void PublicTool::int32_im_show(Mat src, Mat dst, float max, string name, int t_wait, bool kill)
//void PublicTool::int32_im_show(Mat src, Mat dst, float max, string name, int t_wait, bool kill)
{
#if (SHOW_RESULT)//SHOW_RESULT
	int n_rows = src.rows;
	int n_cols = src.cols;
	//src /= max * 255;
	Mat dst2(src.rows, src.cols, CV_8UC1);
	for (int i = 0; i < n_rows*n_cols; i++) {
		((unsigned char*)dst2.data)[i] = (unsigned char)(((int*)src.data)[i] / max * 255);
	}
	cv::namedWindow(name, WINDOW_NORMAL);
	imshow(name, dst2);
	waitKey(t_wait);
	if (kill)
		destroyWindow(name);
#endif
};

void PublicTool::float32ch3_im_show(Mat src, Mat dst, float max, string name, int t_wait, bool kill)
{
#if SHOW_RESULT
	int n_rows = src.rows;
	int n_cols = src.cols;
	//src /= max * 255;
	Mat dst2(src.rows, src.cols, CV_8UC1);
	for (int i = 0; i < n_rows*n_cols; i++) {
		((unsigned char*)dst2.data)[i] = (unsigned char)(((float*)src.data)[i * 3 + 2] / max * 255);
	}
	cv::namedWindow(name, WINDOW_NORMAL);
	imshow(name, dst2);
	waitKey(t_wait);
	if (kill)
		destroyWindow(name);
#endif
};
